#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/lguest_launcher.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/interrupt.h>
#include <linux/virtio_ring.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/paravirt.h>
#include <asm/lguest_hcall.h>

static void *lguest_devices;

static inline void *lguest_map(unsigned long phys_addr, unsigned long pages)
{
	return (__force void *)ioremap_cache(phys_addr, PAGE_SIZE*pages);
}

static inline void lguest_unmap(void *addr)
{
	iounmap((__force void __iomem *)addr);
}

struct lguest_device {
	struct virtio_device vdev;

	
	struct lguest_device_desc *desc;
};

#define to_lgdev(vd) container_of(vd, struct lguest_device, vdev)

static struct lguest_vqconfig *lg_vq(const struct lguest_device_desc *desc)
{
	return (void *)(desc + 1);
}

static u8 *lg_features(const struct lguest_device_desc *desc)
{
	return (void *)(lg_vq(desc) + desc->num_vq);
}

static u8 *lg_config(const struct lguest_device_desc *desc)
{
	return lg_features(desc) + desc->feature_len * 2;
}

static unsigned desc_size(const struct lguest_device_desc *desc)
{
	return sizeof(*desc)
		+ desc->num_vq * sizeof(struct lguest_vqconfig)
		+ desc->feature_len * 2
		+ desc->config_len;
}

static u32 lg_get_features(struct virtio_device *vdev)
{
	unsigned int i;
	u32 features = 0;
	struct lguest_device_desc *desc = to_lgdev(vdev)->desc;
	u8 *in_features = lg_features(desc);

	
	for (i = 0; i < min(desc->feature_len * 8, 32); i++)
		if (in_features[i / 8] & (1 << (i % 8)))
			features |= (1 << i);

	return features;
}

static void status_notify(struct virtio_device *vdev)
{
	unsigned long offset = (void *)to_lgdev(vdev)->desc - lguest_devices;

	hcall(LHCALL_NOTIFY, (max_pfn << PAGE_SHIFT) + offset, 0, 0, 0);
}

static void lg_finalize_features(struct virtio_device *vdev)
{
	unsigned int i, bits;
	struct lguest_device_desc *desc = to_lgdev(vdev)->desc;
	
	u8 *out_features = lg_features(desc) + desc->feature_len;

	
	vring_transport_features(vdev);

	memset(out_features, 0, desc->feature_len);
	bits = min_t(unsigned, desc->feature_len, sizeof(vdev->features)) * 8;
	for (i = 0; i < bits; i++) {
		if (test_bit(i, vdev->features))
			out_features[i / 8] |= (1 << (i % 8));
	}

	
	status_notify(vdev);
}

static void lg_get(struct virtio_device *vdev, unsigned int offset,
		   void *buf, unsigned len)
{
	struct lguest_device_desc *desc = to_lgdev(vdev)->desc;

	
	BUG_ON(offset + len > desc->config_len);
	memcpy(buf, lg_config(desc) + offset, len);
}

static void lg_set(struct virtio_device *vdev, unsigned int offset,
		   const void *buf, unsigned len)
{
	struct lguest_device_desc *desc = to_lgdev(vdev)->desc;

	
	BUG_ON(offset + len > desc->config_len);
	memcpy(lg_config(desc) + offset, buf, len);
}

static u8 lg_get_status(struct virtio_device *vdev)
{
	return to_lgdev(vdev)->desc->status;
}

static void lg_set_status(struct virtio_device *vdev, u8 status)
{
	BUG_ON(!status);
	to_lgdev(vdev)->desc->status = status;

	
	if (status & VIRTIO_CONFIG_S_FAILED)
		status_notify(vdev);
}

static void lg_reset(struct virtio_device *vdev)
{
	
	to_lgdev(vdev)->desc->status = 0;
	status_notify(vdev);
}


struct lguest_vq_info {
	
	struct lguest_vqconfig config;

	
	void *pages;
};

static void lg_notify(struct virtqueue *vq)
{
	struct lguest_vq_info *lvq = vq->priv;

	hcall(LHCALL_NOTIFY, lvq->config.pfn << PAGE_SHIFT, 0, 0, 0);
}

extern int lguest_setup_irq(unsigned int irq);

static struct virtqueue *lg_find_vq(struct virtio_device *vdev,
				    unsigned index,
				    void (*callback)(struct virtqueue *vq),
				    const char *name)
{
	struct lguest_device *ldev = to_lgdev(vdev);
	struct lguest_vq_info *lvq;
	struct virtqueue *vq;
	int err;

	
	if (index >= ldev->desc->num_vq)
		return ERR_PTR(-ENOENT);

	lvq = kmalloc(sizeof(*lvq), GFP_KERNEL);
	if (!lvq)
		return ERR_PTR(-ENOMEM);

	memcpy(&lvq->config, lg_vq(ldev->desc)+index, sizeof(lvq->config));

	printk("Mapping virtqueue %i addr %lx\n", index,
	       (unsigned long)lvq->config.pfn << PAGE_SHIFT);
	
	lvq->pages = lguest_map((unsigned long)lvq->config.pfn << PAGE_SHIFT,
				DIV_ROUND_UP(vring_size(lvq->config.num,
							LGUEST_VRING_ALIGN),
					     PAGE_SIZE));
	if (!lvq->pages) {
		err = -ENOMEM;
		goto free_lvq;
	}

	vq = vring_new_virtqueue(lvq->config.num, LGUEST_VRING_ALIGN, vdev,
				 true, lvq->pages, lg_notify, callback, name);
	if (!vq) {
		err = -ENOMEM;
		goto unmap;
	}

	
	err = lguest_setup_irq(lvq->config.irq);
	if (err)
		goto destroy_vring;

	err = request_irq(lvq->config.irq, vring_interrupt, IRQF_SHARED,
			  dev_name(&vdev->dev), vq);
	if (err)
		goto free_desc;

	vq->priv = lvq;
	return vq;

free_desc:
	irq_free_desc(lvq->config.irq);
destroy_vring:
	vring_del_virtqueue(vq);
unmap:
	lguest_unmap(lvq->pages);
free_lvq:
	kfree(lvq);
	return ERR_PTR(err);
}

static void lg_del_vq(struct virtqueue *vq)
{
	struct lguest_vq_info *lvq = vq->priv;

	
	free_irq(lvq->config.irq, vq);
	
	vring_del_virtqueue(vq);
	
	lguest_unmap(lvq->pages);
	
	kfree(lvq);
}

static void lg_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list)
		lg_del_vq(vq);
}

static int lg_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char *names[])
{
	struct lguest_device *ldev = to_lgdev(vdev);
	int i;

	
	if (nvqs > ldev->desc->num_vq)
		return -ENOENT;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = lg_find_vq(vdev, i, callbacks[i], names[i]);
		if (IS_ERR(vqs[i]))
			goto error;
	}
	return 0;

error:
	lg_del_vqs(vdev);
	return PTR_ERR(vqs[i]);
}

static const char *lg_bus_name(struct virtio_device *vdev)
{
	return "";
}

static struct virtio_config_ops lguest_config_ops = {
	.get_features = lg_get_features,
	.finalize_features = lg_finalize_features,
	.get = lg_get,
	.set = lg_set,
	.get_status = lg_get_status,
	.set_status = lg_set_status,
	.reset = lg_reset,
	.find_vqs = lg_find_vqs,
	.del_vqs = lg_del_vqs,
	.bus_name = lg_bus_name,
};

static struct device *lguest_root;

static void add_lguest_device(struct lguest_device_desc *d,
			      unsigned int offset)
{
	struct lguest_device *ldev;

	
	ldev = kzalloc(sizeof(*ldev), GFP_KERNEL);
	if (!ldev) {
		printk(KERN_EMERG "Cannot allocate lguest dev %u type %u\n",
		       offset, d->type);
		return;
	}

	
	ldev->vdev.dev.parent = lguest_root;
	ldev->vdev.id.device = d->type;
	ldev->vdev.config = &lguest_config_ops;
	
	ldev->desc = d;

	if (register_virtio_device(&ldev->vdev) != 0) {
		printk(KERN_ERR "Failed to register lguest dev %u type %u\n",
		       offset, d->type);
		kfree(ldev);
	}
}

static void scan_devices(void)
{
	unsigned int i;
	struct lguest_device_desc *d;

	
	for (i = 0; i < PAGE_SIZE; i += desc_size(d)) {
		d = lguest_devices + i;

		
		if (d->type == 0)
			break;

		printk("Device at %i has size %u\n", i, desc_size(d));
		add_lguest_device(d, i);
	}
}

static int __init lguest_devices_init(void)
{
	if (strcmp(pv_info.name, "lguest") != 0)
		return 0;

	lguest_root = root_device_register("lguest");
	if (IS_ERR(lguest_root))
		panic("Could not register lguest root");

	
	lguest_devices = lguest_map(max_pfn<<PAGE_SHIFT, 1);

	scan_devices();
	return 0;
}
postcore_initcall(lguest_devices_init);

