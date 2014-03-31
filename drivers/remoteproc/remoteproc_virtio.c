/*
 * Remote processor messaging transport (OMAP platform-specific bits)
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

#include <linux/export.h>
#include <linux/remoteproc.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/slab.h>

#include "remoteproc_internal.h"

static void rproc_virtio_notify(struct virtqueue *vq)
{
	struct rproc_vring *rvring = vq->priv;
	struct rproc *rproc = rvring->rvdev->rproc;
	int notifyid = rvring->notifyid;

	dev_dbg(rproc->dev, "kicking vq index: %d\n", notifyid);

	rproc->ops->kick(rproc, notifyid);
}

irqreturn_t rproc_vq_interrupt(struct rproc *rproc, int notifyid)
{
	struct rproc_vring *rvring;

	dev_dbg(rproc->dev, "vq index %d is interrupted\n", notifyid);

	rvring = idr_find(&rproc->notifyids, notifyid);
	if (!rvring || !rvring->vq)
		return IRQ_NONE;

	return vring_interrupt(0, rvring->vq);
}
EXPORT_SYMBOL(rproc_vq_interrupt);

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned id,
				    void (*callback)(struct virtqueue *vq),
				    const char *name)
{
	struct rproc_vdev *rvdev = vdev_to_rvdev(vdev);
	struct rproc *rproc = vdev_to_rproc(vdev);
	struct rproc_vring *rvring;
	struct virtqueue *vq;
	void *addr;
	int len, size;

	
	if (id >= ARRAY_SIZE(rvdev->vring))
		return ERR_PTR(-EINVAL);

	rvring = &rvdev->vring[id];

	addr = rvring->va;
	len = rvring->len;

	
	size = vring_size(len, rvring->align);
	memset(addr, 0, size);

	dev_dbg(rproc->dev, "vring%d: va %p qsz %d notifyid %d\n",
					id, addr, len, rvring->notifyid);

	vq = vring_new_virtqueue(len, rvring->align, vdev, false, addr,
					rproc_virtio_notify, callback, name);
	if (!vq) {
		dev_err(rproc->dev, "vring_new_virtqueue %s failed\n", name);
		return ERR_PTR(-ENOMEM);
	}

	rvring->vq = vq;
	vq->priv = rvring;

	return vq;
}

static void rproc_virtio_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;
	struct rproc *rproc = vdev_to_rproc(vdev);
	struct rproc_vring *rvring;

	
	rproc_shutdown(rproc);

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		rvring = vq->priv;
		rvring->vq = NULL;
		vring_del_virtqueue(vq);
	}
}

static int rproc_virtio_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char *names[])
{
	struct rproc *rproc = vdev_to_rproc(vdev);
	int i, ret;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = rp_find_vq(vdev, i, callbacks[i], names[i]);
		if (IS_ERR(vqs[i])) {
			ret = PTR_ERR(vqs[i]);
			goto error;
		}
	}

	
	ret = rproc_boot(rproc);
	if (ret) {
		dev_err(rproc->dev, "rproc_boot() failed %d\n", ret);
		goto error;
	}

	return 0;

error:
	rproc_virtio_del_vqs(vdev);
	return ret;
}

static u8 rproc_virtio_get_status(struct virtio_device *vdev)
{
	return 0;
}

static void rproc_virtio_set_status(struct virtio_device *vdev, u8 status)
{
	dev_dbg(&vdev->dev, "status: %d\n", status);
}

static void rproc_virtio_reset(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "reset !\n");
}

static u32 rproc_virtio_get_features(struct virtio_device *vdev)
{
	struct rproc_vdev *rvdev = vdev_to_rvdev(vdev);

	return rvdev->dfeatures;
}

static void rproc_virtio_finalize_features(struct virtio_device *vdev)
{
	struct rproc_vdev *rvdev = vdev_to_rvdev(vdev);

	
	vring_transport_features(vdev);

	rvdev->gfeatures = vdev->features[0];
}

static struct virtio_config_ops rproc_virtio_config_ops = {
	.get_features	= rproc_virtio_get_features,
	.finalize_features = rproc_virtio_finalize_features,
	.find_vqs	= rproc_virtio_find_vqs,
	.del_vqs	= rproc_virtio_del_vqs,
	.reset		= rproc_virtio_reset,
	.set_status	= rproc_virtio_set_status,
	.get_status	= rproc_virtio_get_status,
};

static void rproc_vdev_release(struct device *dev)
{
	struct virtio_device *vdev = dev_to_virtio(dev);
	struct rproc *rproc = vdev_to_rproc(vdev);

	kref_put(&rproc->refcount, rproc_release);
}

int rproc_add_virtio_dev(struct rproc_vdev *rvdev, int id)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = rproc->dev;
	struct virtio_device *vdev = &rvdev->vdev;
	int ret;

	vdev->id.device	= id,
	vdev->config = &rproc_virtio_config_ops,
	vdev->dev.parent = dev;
	vdev->dev.release = rproc_vdev_release;

	kref_get(&rproc->refcount);

	ret = register_virtio_device(vdev);
	if (ret) {
		kref_put(&rproc->refcount, rproc_release);
		dev_err(dev, "failed to register vdev: %d\n", ret);
		goto out;
	}

	dev_info(dev, "registered %s (type %d)\n", dev_name(&vdev->dev), id);

out:
	return ret;
}

void rproc_remove_virtio_dev(struct rproc_vdev *rvdev)
{
	unregister_virtio_device(&rvdev->vdev);
}
