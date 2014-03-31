/*
 * Virtio-based remote processor messaging bus
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/rpmsg.h>
#include <linux/mutex.h>

struct virtproc_info {
	struct virtio_device *vdev;
	struct virtqueue *rvq, *svq;
	void *rbufs, *sbufs;
	int last_sbuf;
	dma_addr_t bufs_dma;
	struct mutex tx_lock;
	struct idr endpoints;
	struct mutex endpoints_lock;
	wait_queue_head_t sendq;
	atomic_t sleepers;
	struct rpmsg_endpoint *ns_ept;
};

struct rpmsg_channel_info {
	char name[RPMSG_NAME_SIZE];
	u32 src;
	u32 dst;
};

#define to_rpmsg_channel(d) container_of(d, struct rpmsg_channel, dev)
#define to_rpmsg_driver(d) container_of(d, struct rpmsg_driver, drv)

#define RPMSG_NUM_BUFS		(512)
#define RPMSG_BUF_SIZE		(512)
#define RPMSG_TOTAL_BUF_SPACE	(RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)

#define RPMSG_RESERVED_ADDRESSES	(1024)

#define RPMSG_NS_ADDR			(53)

#define rpmsg_show_attr(field, path, format_string)			\
static ssize_t								\
field##_show(struct device *dev,					\
			struct device_attribute *attr, char *buf)	\
{									\
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);		\
									\
	return sprintf(buf, format_string, rpdev->path);		\
}

rpmsg_show_attr(name, id.name, "%s\n");
rpmsg_show_attr(src, src, "0x%x\n");
rpmsg_show_attr(dst, dst, "0x%x\n");
rpmsg_show_attr(announce, announce ? "true" : "false", "%s\n");

static unsigned int rpmsg_dev_index;

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);

	return sprintf(buf, RPMSG_DEVICE_MODALIAS_FMT "\n", rpdev->id.name);
}

static struct device_attribute rpmsg_dev_attrs[] = {
	__ATTR_RO(name),
	__ATTR_RO(modalias),
	__ATTR_RO(dst),
	__ATTR_RO(src),
	__ATTR_RO(announce),
	__ATTR_NULL
};

static inline int rpmsg_id_match(const struct rpmsg_channel *rpdev,
				  const struct rpmsg_device_id *id)
{
	return strncmp(id->name, rpdev->id.name, RPMSG_NAME_SIZE) == 0;
}

static int rpmsg_dev_match(struct device *dev, struct device_driver *drv)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);
	struct rpmsg_driver *rpdrv = to_rpmsg_driver(drv);
	const struct rpmsg_device_id *ids = rpdrv->id_table;
	unsigned int i;

	for (i = 0; ids[i].name[0]; i++)
		if (rpmsg_id_match(rpdev, &ids[i]))
			return 1;

	return 0;
}

static int rpmsg_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);

	return add_uevent_var(env, "MODALIAS=" RPMSG_DEVICE_MODALIAS_FMT,
					rpdev->id.name);
}

static struct rpmsg_endpoint *__rpmsg_create_ept(struct virtproc_info *vrp,
		struct rpmsg_channel *rpdev, rpmsg_rx_cb_t cb,
		void *priv, u32 addr)
{
	int err, tmpaddr, request;
	struct rpmsg_endpoint *ept;
	struct device *dev = rpdev ? &rpdev->dev : &vrp->vdev->dev;

	if (!idr_pre_get(&vrp->endpoints, GFP_KERNEL))
		return NULL;

	ept = kzalloc(sizeof(*ept), GFP_KERNEL);
	if (!ept) {
		dev_err(dev, "failed to kzalloc a new ept\n");
		return NULL;
	}

	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;

	
	request = addr == RPMSG_ADDR_ANY ? RPMSG_RESERVED_ADDRESSES : addr;

	mutex_lock(&vrp->endpoints_lock);

	
	err = idr_get_new_above(&vrp->endpoints, ept, request, &tmpaddr);
	if (err) {
		dev_err(dev, "idr_get_new_above failed: %d\n", err);
		goto free_ept;
	}

	
	if (addr != RPMSG_ADDR_ANY && tmpaddr != addr) {
		dev_err(dev, "address 0x%x already in use\n", addr);
		goto rem_idr;
	}

	ept->addr = tmpaddr;

	mutex_unlock(&vrp->endpoints_lock);

	return ept;

rem_idr:
	idr_remove(&vrp->endpoints, request);
free_ept:
	mutex_unlock(&vrp->endpoints_lock);
	kfree(ept);
	return NULL;
}

struct rpmsg_endpoint *rpmsg_create_ept(struct rpmsg_channel *rpdev,
				rpmsg_rx_cb_t cb, void *priv, u32 addr)
{
	return __rpmsg_create_ept(rpdev->vrp, rpdev, cb, priv, addr);
}
EXPORT_SYMBOL(rpmsg_create_ept);

static void
__rpmsg_destroy_ept(struct virtproc_info *vrp, struct rpmsg_endpoint *ept)
{
	mutex_lock(&vrp->endpoints_lock);
	idr_remove(&vrp->endpoints, ept->addr);
	mutex_unlock(&vrp->endpoints_lock);

	kfree(ept);
}

void rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	__rpmsg_destroy_ept(ept->rpdev->vrp, ept);
}
EXPORT_SYMBOL(rpmsg_destroy_ept);

static int rpmsg_dev_probe(struct device *dev)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);
	struct rpmsg_driver *rpdrv = to_rpmsg_driver(rpdev->dev.driver);
	struct virtproc_info *vrp = rpdev->vrp;
	struct rpmsg_endpoint *ept;
	int err;

	ept = rpmsg_create_ept(rpdev, rpdrv->callback, NULL, rpdev->src);
	if (!ept) {
		dev_err(dev, "failed to create endpoint\n");
		err = -ENOMEM;
		goto out;
	}

	rpdev->ept = ept;
	rpdev->src = ept->addr;

	err = rpdrv->probe(rpdev);
	if (err) {
		dev_err(dev, "%s: failed: %d\n", __func__, err);
		rpmsg_destroy_ept(ept);
		goto out;
	}

	
	if (rpdev->announce &&
			virtio_has_feature(vrp->vdev, VIRTIO_RPMSG_F_NS)) {
		struct rpmsg_ns_msg nsm;

		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->src;
		nsm.flags = RPMSG_NS_CREATE;

		err = rpmsg_sendto(rpdev, &nsm, sizeof(nsm), RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

out:
	return err;
}

static int rpmsg_dev_remove(struct device *dev)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);
	struct rpmsg_driver *rpdrv = to_rpmsg_driver(rpdev->dev.driver);
	struct virtproc_info *vrp = rpdev->vrp;
	int err = 0;

	
	if (rpdev->announce &&
			virtio_has_feature(vrp->vdev, VIRTIO_RPMSG_F_NS)) {
		struct rpmsg_ns_msg nsm;

		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->src;
		nsm.flags = RPMSG_NS_DESTROY;

		err = rpmsg_sendto(rpdev, &nsm, sizeof(nsm), RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

	rpdrv->remove(rpdev);

	rpmsg_destroy_ept(rpdev->ept);

	return err;
}

static struct bus_type rpmsg_bus = {
	.name		= "rpmsg",
	.match		= rpmsg_dev_match,
	.dev_attrs	= rpmsg_dev_attrs,
	.uevent		= rpmsg_uevent,
	.probe		= rpmsg_dev_probe,
	.remove		= rpmsg_dev_remove,
};

int register_rpmsg_driver(struct rpmsg_driver *rpdrv)
{
	rpdrv->drv.bus = &rpmsg_bus;
	return driver_register(&rpdrv->drv);
}
EXPORT_SYMBOL(register_rpmsg_driver);

void unregister_rpmsg_driver(struct rpmsg_driver *rpdrv)
{
	driver_unregister(&rpdrv->drv);
}
EXPORT_SYMBOL(unregister_rpmsg_driver);

static void rpmsg_release_device(struct device *dev)
{
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);

	kfree(rpdev);
}

static int rpmsg_channel_match(struct device *dev, void *data)
{
	struct rpmsg_channel_info *chinfo = data;
	struct rpmsg_channel *rpdev = to_rpmsg_channel(dev);

	if (chinfo->src != RPMSG_ADDR_ANY && chinfo->src != rpdev->src)
		return 0;

	if (chinfo->dst != RPMSG_ADDR_ANY && chinfo->dst != rpdev->dst)
		return 0;

	if (strncmp(chinfo->name, rpdev->id.name, RPMSG_NAME_SIZE))
		return 0;

	
	return 1;
}

static struct rpmsg_channel *rpmsg_create_channel(struct virtproc_info *vrp,
				struct rpmsg_channel_info *chinfo)
{
	struct rpmsg_channel *rpdev;
	struct device *tmp, *dev = &vrp->vdev->dev;
	int ret;

	
	tmp = device_find_child(dev, chinfo, rpmsg_channel_match);
	if (tmp) {
		
		put_device(tmp);
		dev_err(dev, "channel %s:%x:%x already exist\n",
				chinfo->name, chinfo->src, chinfo->dst);
		return NULL;
	}

	rpdev = kzalloc(sizeof(struct rpmsg_channel), GFP_KERNEL);
	if (!rpdev) {
		pr_err("kzalloc failed\n");
		return NULL;
	}

	rpdev->vrp = vrp;
	rpdev->src = chinfo->src;
	rpdev->dst = chinfo->dst;

	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY ? true : false;

	strncpy(rpdev->id.name, chinfo->name, RPMSG_NAME_SIZE);

	
	dev_set_name(&rpdev->dev, "rpmsg%d", rpmsg_dev_index++);

	rpdev->dev.parent = &vrp->vdev->dev;
	rpdev->dev.bus = &rpmsg_bus;
	rpdev->dev.release = rpmsg_release_device;

	ret = device_register(&rpdev->dev);
	if (ret) {
		dev_err(dev, "device_register failed: %d\n", ret);
		put_device(&rpdev->dev);
		return NULL;
	}

	return rpdev;
}

static int rpmsg_destroy_channel(struct virtproc_info *vrp,
					struct rpmsg_channel_info *chinfo)
{
	struct virtio_device *vdev = vrp->vdev;
	struct device *dev;

	dev = device_find_child(&vdev->dev, chinfo, rpmsg_channel_match);
	if (!dev)
		return -EINVAL;

	device_unregister(dev);

	put_device(dev);

	return 0;
}

static void *get_a_tx_buf(struct virtproc_info *vrp)
{
	unsigned int len;
	void *ret;

	
	mutex_lock(&vrp->tx_lock);

	if (vrp->last_sbuf < RPMSG_NUM_BUFS / 2)
		ret = vrp->sbufs + RPMSG_BUF_SIZE * vrp->last_sbuf++;
	
	else
		ret = virtqueue_get_buf(vrp->svq, &len);

	mutex_unlock(&vrp->tx_lock);

	return ret;
}

static void rpmsg_upref_sleepers(struct virtproc_info *vrp)
{
	
	mutex_lock(&vrp->tx_lock);

	
	if (atomic_inc_return(&vrp->sleepers) == 1)
		
		virtqueue_enable_cb(vrp->svq);

	mutex_unlock(&vrp->tx_lock);
}

static void rpmsg_downref_sleepers(struct virtproc_info *vrp)
{
	
	mutex_lock(&vrp->tx_lock);

	
	if (atomic_dec_and_test(&vrp->sleepers))
		
		virtqueue_disable_cb(vrp->svq);

	mutex_unlock(&vrp->tx_lock);
}

int rpmsg_send_offchannel_raw(struct rpmsg_channel *rpdev, u32 src, u32 dst,
					void *data, int len, bool wait)
{
	struct virtproc_info *vrp = rpdev->vrp;
	struct device *dev = &rpdev->dev;
	struct scatterlist sg;
	struct rpmsg_hdr *msg;
	int err;

	
	if (src == RPMSG_ADDR_ANY || dst == RPMSG_ADDR_ANY) {
		dev_err(dev, "invalid addr (src 0x%x, dst 0x%x)\n", src, dst);
		return -EINVAL;
	}

	if (len > RPMSG_BUF_SIZE - sizeof(struct rpmsg_hdr)) {
		dev_err(dev, "message is too big (%d)\n", len);
		return -EMSGSIZE;
	}

	
	msg = get_a_tx_buf(vrp);
	if (!msg && !wait)
		return -ENOMEM;

	
	while (!msg) {
		
		rpmsg_upref_sleepers(vrp);

		err = wait_event_interruptible_timeout(vrp->sendq,
					(msg = get_a_tx_buf(vrp)),
					msecs_to_jiffies(15000));

		
		rpmsg_downref_sleepers(vrp);

		
		if (!err) {
			dev_err(dev, "timeout waiting for a tx buffer\n");
			return -ERESTARTSYS;
		}
	}

	msg->len = len;
	msg->flags = 0;
	msg->src = src;
	msg->dst = dst;
	msg->reserved = 0;
	memcpy(msg->data, data, len);

	dev_dbg(dev, "TX From 0x%x, To 0x%x, Len %d, Flags %d, Reserved %d\n",
					msg->src, msg->dst, msg->len,
					msg->flags, msg->reserved);
	print_hex_dump(KERN_DEBUG, "rpmsg_virtio TX: ", DUMP_PREFIX_NONE, 16, 1,
					msg, sizeof(*msg) + msg->len, true);

	sg_init_one(&sg, msg, sizeof(*msg) + len);

	mutex_lock(&vrp->tx_lock);

	
	err = virtqueue_add_buf(vrp->svq, &sg, 1, 0, msg, GFP_KERNEL);
	if (err < 0) {
		dev_err(dev, "virtqueue_add_buf failed: %d\n", err);
		goto out;
	}

	
	virtqueue_kick(vrp->svq);

	err = 0;
out:
	mutex_unlock(&vrp->tx_lock);
	return err;
}
EXPORT_SYMBOL(rpmsg_send_offchannel_raw);

static void rpmsg_recv_done(struct virtqueue *rvq)
{
	struct rpmsg_hdr *msg;
	unsigned int len;
	struct rpmsg_endpoint *ept;
	struct scatterlist sg;
	struct virtproc_info *vrp = rvq->vdev->priv;
	struct device *dev = &rvq->vdev->dev;
	int err;

	msg = virtqueue_get_buf(rvq, &len);
	if (!msg) {
		dev_err(dev, "uhm, incoming signal, but no used buffer ?\n");
		return;
	}

	dev_dbg(dev, "From: 0x%x, To: 0x%x, Len: %d, Flags: %d, Reserved: %d\n",
					msg->src, msg->dst, msg->len,
					msg->flags, msg->reserved);
	print_hex_dump(KERN_DEBUG, "rpmsg_virtio RX: ", DUMP_PREFIX_NONE, 16, 1,
					msg, sizeof(*msg) + msg->len, true);

	if (len > RPMSG_BUF_SIZE ||
		msg->len > (len - sizeof(struct rpmsg_hdr))) {
		dev_warn(dev, "inbound msg too big: (%d, %d)\n", len, msg->len);
		return;
	}

	
	mutex_lock(&vrp->endpoints_lock);
	ept = idr_find(&vrp->endpoints, msg->dst);
	mutex_unlock(&vrp->endpoints_lock);

	if (ept && ept->cb)
		ept->cb(ept->rpdev, msg->data, msg->len, ept->priv, msg->src);
	else
		dev_warn(dev, "msg received with no recepient\n");

	
	sg_init_one(&sg, msg, RPMSG_BUF_SIZE);

	
	err = virtqueue_add_buf(vrp->rvq, &sg, 0, 1, msg, GFP_KERNEL);
	if (err < 0) {
		dev_err(dev, "failed to add a virtqueue buffer: %d\n", err);
		return;
	}

	
	virtqueue_kick(vrp->rvq);
}

static void rpmsg_xmit_done(struct virtqueue *svq)
{
	struct virtproc_info *vrp = svq->vdev->priv;

	dev_dbg(&svq->vdev->dev, "%s\n", __func__);

	
	wake_up_interruptible(&vrp->sendq);
}

static void rpmsg_ns_cb(struct rpmsg_channel *rpdev, void *data, int len,
							void *priv, u32 src)
{
	struct rpmsg_ns_msg *msg = data;
	struct rpmsg_channel *newch;
	struct rpmsg_channel_info chinfo;
	struct virtproc_info *vrp = priv;
	struct device *dev = &vrp->vdev->dev;
	int ret;

	print_hex_dump(KERN_DEBUG, "NS announcement: ",
			DUMP_PREFIX_NONE, 16, 1,
			data, len, true);

	if (len != sizeof(*msg)) {
		dev_err(dev, "malformed ns msg (%d)\n", len);
		return;
	}

	if (rpdev) {
		dev_err(dev, "anomaly: ns ept has an rpdev handle\n");
		return;
	}

	
	msg->name[RPMSG_NAME_SIZE - 1] = '\0';

	dev_info(dev, "%sing channel %s addr 0x%x\n",
			msg->flags & RPMSG_NS_DESTROY ? "destroy" : "creat",
			msg->name, msg->addr);

	strncpy(chinfo.name, msg->name, sizeof(chinfo.name));
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = msg->addr;

	if (msg->flags & RPMSG_NS_DESTROY) {
		ret = rpmsg_destroy_channel(vrp, &chinfo);
		if (ret)
			dev_err(dev, "rpmsg_destroy_channel failed: %d\n", ret);
	} else {
		newch = rpmsg_create_channel(vrp, &chinfo);
		if (!newch)
			dev_err(dev, "rpmsg_create_channel failed\n");
	}
}

static int rpmsg_probe(struct virtio_device *vdev)
{
	vq_callback_t *vq_cbs[] = { rpmsg_recv_done, rpmsg_xmit_done };
	const char *names[] = { "input", "output" };
	struct virtqueue *vqs[2];
	struct virtproc_info *vrp;
	void *bufs_va;
	int err = 0, i;

	vrp = kzalloc(sizeof(*vrp), GFP_KERNEL);
	if (!vrp)
		return -ENOMEM;

	vrp->vdev = vdev;

	idr_init(&vrp->endpoints);
	mutex_init(&vrp->endpoints_lock);
	mutex_init(&vrp->tx_lock);
	init_waitqueue_head(&vrp->sendq);

	
	err = vdev->config->find_vqs(vdev, 2, vqs, vq_cbs, names);
	if (err)
		goto free_vrp;

	vrp->rvq = vqs[0];
	vrp->svq = vqs[1];

	
	bufs_va = dma_alloc_coherent(vdev->dev.parent, RPMSG_TOTAL_BUF_SPACE,
				&vrp->bufs_dma, GFP_KERNEL);
	if (!bufs_va)
		goto vqs_del;

	dev_dbg(&vdev->dev, "buffers: va %p, dma 0x%llx\n", bufs_va,
					(unsigned long long)vrp->bufs_dma);

	
	vrp->rbufs = bufs_va;

	
	vrp->sbufs = bufs_va + RPMSG_TOTAL_BUF_SPACE / 2;

	
	for (i = 0; i < RPMSG_NUM_BUFS / 2; i++) {
		struct scatterlist sg;
		void *cpu_addr = vrp->rbufs + i * RPMSG_BUF_SIZE;

		sg_init_one(&sg, cpu_addr, RPMSG_BUF_SIZE);

		err = virtqueue_add_buf(vrp->rvq, &sg, 0, 1, cpu_addr,
								GFP_KERNEL);
		WARN_ON(err < 0); 
	}

	
	virtqueue_disable_cb(vrp->svq);

	vdev->priv = vrp;

	
	if (virtio_has_feature(vdev, VIRTIO_RPMSG_F_NS)) {
		
		vrp->ns_ept = __rpmsg_create_ept(vrp, NULL, rpmsg_ns_cb,
						vrp, RPMSG_NS_ADDR);
		if (!vrp->ns_ept) {
			dev_err(&vdev->dev, "failed to create the ns ept\n");
			err = -ENOMEM;
			goto free_coherent;
		}
	}

	
	virtqueue_kick(vrp->rvq);

	dev_info(&vdev->dev, "rpmsg host is online\n");

	return 0;

free_coherent:
	dma_free_coherent(vdev->dev.parent, RPMSG_TOTAL_BUF_SPACE, bufs_va,
					vrp->bufs_dma);
vqs_del:
	vdev->config->del_vqs(vrp->vdev);
free_vrp:
	kfree(vrp);
	return err;
}

static int rpmsg_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

static void __devexit rpmsg_remove(struct virtio_device *vdev)
{
	struct virtproc_info *vrp = vdev->priv;
	int ret;

	vdev->config->reset(vdev);

	ret = device_for_each_child(&vdev->dev, NULL, rpmsg_remove_device);
	if (ret)
		dev_warn(&vdev->dev, "can't remove rpmsg device: %d\n", ret);

	if (vrp->ns_ept)
		__rpmsg_destroy_ept(vrp, vrp->ns_ept);

	idr_remove_all(&vrp->endpoints);
	idr_destroy(&vrp->endpoints);

	vdev->config->del_vqs(vrp->vdev);

	dma_free_coherent(vdev->dev.parent, RPMSG_TOTAL_BUF_SPACE,
					vrp->rbufs, vrp->bufs_dma);

	kfree(vrp);
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_RPMSG, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {
	VIRTIO_RPMSG_F_NS,
};

static struct virtio_driver virtio_ipc_driver = {
	.feature_table	= features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name	= KBUILD_MODNAME,
	.driver.owner	= THIS_MODULE,
	.id_table	= id_table,
	.probe		= rpmsg_probe,
	.remove		= __devexit_p(rpmsg_remove),
};

static int __init rpmsg_init(void)
{
	int ret;

	ret = bus_register(&rpmsg_bus);
	if (ret) {
		pr_err("failed to register rpmsg bus: %d\n", ret);
		return ret;
	}

	ret = register_virtio_driver(&virtio_ipc_driver);
	if (ret) {
		pr_err("failed to register virtio driver: %d\n", ret);
		bus_unregister(&rpmsg_bus);
	}

	return ret;
}
module_init(rpmsg_init);

static void __exit rpmsg_fini(void)
{
	unregister_virtio_driver(&virtio_ipc_driver);
	bus_unregister(&rpmsg_bus);
}
module_exit(rpmsg_fini);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio-based remote processor messaging bus");
MODULE_LICENSE("GPL v2");
