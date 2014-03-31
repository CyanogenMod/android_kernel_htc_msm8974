/*
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/iommu.h>
#include <linux/scatterlist.h>
#include <linux/idr.h>
#include <linux/notifier.h>
#include <linux/err.h>

static struct kset *iommu_group_kset;
static struct idr iommu_group_idr;
static struct mutex iommu_group_mutex;

struct iommu_group {
	struct kobject kobj;
	struct kobject *devices_kobj;
	struct list_head devices;
	struct mutex mutex;
	struct blocking_notifier_head notifier;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
};

struct iommu_device {
	struct list_head list;
	struct device *dev;
	char *name;
};

struct iommu_group_attribute {
	struct attribute attr;
	ssize_t (*show)(struct iommu_group *group, char *buf);
	ssize_t (*store)(struct iommu_group *group,
			 const char *buf, size_t count);
};

#define IOMMU_GROUP_ATTR(_name, _mode, _show, _store)		\
struct iommu_group_attribute iommu_group_attr_##_name =		\
	__ATTR(_name, _mode, _show, _store)

#define to_iommu_group_attr(_attr)	\
	container_of(_attr, struct iommu_group_attribute, attr)
#define to_iommu_group(_kobj)		\
	container_of(_kobj, struct iommu_group, kobj)

static ssize_t iommu_group_attr_show(struct kobject *kobj,
				     struct attribute *__attr, char *buf)
{
	struct iommu_group_attribute *attr = to_iommu_group_attr(__attr);
	struct iommu_group *group = to_iommu_group(kobj);
	ssize_t ret = -EIO;

	if (attr->show)
		ret = attr->show(group, buf);
	return ret;
}

static ssize_t iommu_group_attr_store(struct kobject *kobj,
				      struct attribute *__attr,
				      const char *buf, size_t count)
{
	struct iommu_group_attribute *attr = to_iommu_group_attr(__attr);
	struct iommu_group *group = to_iommu_group(kobj);
	ssize_t ret = -EIO;

	if (attr->store)
		ret = attr->store(group, buf, count);
	return ret;
}

static const struct sysfs_ops iommu_group_sysfs_ops = {
	.show = iommu_group_attr_show,
	.store = iommu_group_attr_store,
};

static int iommu_group_create_file(struct iommu_group *group,
				   struct iommu_group_attribute *attr)
{
	return sysfs_create_file(&group->kobj, &attr->attr);
}

static void iommu_group_remove_file(struct iommu_group *group,
				    struct iommu_group_attribute *attr)
{
	sysfs_remove_file(&group->kobj, &attr->attr);
}

static ssize_t iommu_group_show_name(struct iommu_group *group, char *buf)
{
	return sprintf(buf, "%s\n", group->name);
}

static IOMMU_GROUP_ATTR(name, S_IRUGO, iommu_group_show_name, NULL);

static void iommu_group_release(struct kobject *kobj)
{
	struct iommu_group *group = to_iommu_group(kobj);

	if (group->iommu_data_release)
		group->iommu_data_release(group->iommu_data);

	mutex_lock(&iommu_group_mutex);
	idr_remove(&iommu_group_idr, group->id);
	mutex_unlock(&iommu_group_mutex);

	kfree(group->name);
	kfree(group);
}

static struct kobj_type iommu_group_ktype = {
	.sysfs_ops = &iommu_group_sysfs_ops,
	.release = iommu_group_release,
};

struct iommu_group *iommu_group_alloc(void)
{
	struct iommu_group *group;
	int ret;

	group = kzalloc(sizeof(*group), GFP_KERNEL);
	if (!group)
		return ERR_PTR(-ENOMEM);

	group->kobj.kset = iommu_group_kset;
	mutex_init(&group->mutex);
	INIT_LIST_HEAD(&group->devices);
	BLOCKING_INIT_NOTIFIER_HEAD(&group->notifier);

	mutex_lock(&iommu_group_mutex);

again:
	if (unlikely(0 == idr_pre_get(&iommu_group_idr, GFP_KERNEL))) {
		kfree(group);
		mutex_unlock(&iommu_group_mutex);
		return ERR_PTR(-ENOMEM);
	}

	ret = idr_get_new_above(&iommu_group_idr, group, 1, &group->id);
	if (ret == -EAGAIN)
		goto again;
	mutex_unlock(&iommu_group_mutex);

	if (ret == -ENOSPC) {
		kfree(group);
		return ERR_PTR(ret);
	}

	ret = kobject_init_and_add(&group->kobj, &iommu_group_ktype,
				   NULL, "%d", group->id);
	if (ret) {
		mutex_lock(&iommu_group_mutex);
		idr_remove(&iommu_group_idr, group->id);
		mutex_unlock(&iommu_group_mutex);
		kfree(group);
		return ERR_PTR(ret);
	}

	group->devices_kobj = kobject_create_and_add("devices", &group->kobj);
	if (!group->devices_kobj) {
		kobject_put(&group->kobj); 
		return ERR_PTR(-ENOMEM);
	}

	kobject_put(&group->kobj);

	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_alloc);

void *iommu_group_get_iommudata(struct iommu_group *group)
{
	return group->iommu_data;
}
EXPORT_SYMBOL_GPL(iommu_group_get_iommudata);

void iommu_group_set_iommudata(struct iommu_group *group, void *iommu_data,
			       void (*release)(void *iommu_data))
{
	group->iommu_data = iommu_data;
	group->iommu_data_release = release;
}
EXPORT_SYMBOL_GPL(iommu_group_set_iommudata);

int iommu_group_set_name(struct iommu_group *group, const char *name)
{
	int ret;

	if (group->name) {
		iommu_group_remove_file(group, &iommu_group_attr_name);
		kfree(group->name);
		group->name = NULL;
		if (!name)
			return 0;
	}

	group->name = kstrdup(name, GFP_KERNEL);
	if (!group->name)
		return -ENOMEM;

	ret = iommu_group_create_file(group, &iommu_group_attr_name);
	if (ret) {
		kfree(group->name);
		group->name = NULL;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(iommu_group_set_name);

int iommu_group_add_device(struct iommu_group *group, struct device *dev)
{
	int ret, i = 0;
	struct iommu_device *device;

	device = kzalloc(sizeof(*device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	device->dev = dev;

	ret = sysfs_create_link(&dev->kobj, &group->kobj, "iommu_group");
	if (ret) {
		kfree(device);
		return ret;
	}

	device->name = kasprintf(GFP_KERNEL, "%s", kobject_name(&dev->kobj));
rename:
	if (!device->name) {
		sysfs_remove_link(&dev->kobj, "iommu_group");
		kfree(device);
		return -ENOMEM;
	}

	ret = sysfs_create_link_nowarn(group->devices_kobj,
				       &dev->kobj, device->name);
	if (ret) {
		kfree(device->name);
		if (ret == -EEXIST && i >= 0) {
			device->name = kasprintf(GFP_KERNEL, "%s.%d",
						 kobject_name(&dev->kobj), i++);
			goto rename;
		}

		sysfs_remove_link(&dev->kobj, "iommu_group");
		kfree(device);
		return ret;
	}

	kobject_get(group->devices_kobj);

	dev->iommu_group = group;

	mutex_lock(&group->mutex);
	list_add_tail(&device->list, &group->devices);
	mutex_unlock(&group->mutex);

	
	blocking_notifier_call_chain(&group->notifier,
				     IOMMU_GROUP_NOTIFY_ADD_DEVICE, dev);
	return 0;
}
EXPORT_SYMBOL_GPL(iommu_group_add_device);

void iommu_group_remove_device(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;
	struct iommu_device *tmp_device, *device = NULL;

	
	blocking_notifier_call_chain(&group->notifier,
				     IOMMU_GROUP_NOTIFY_DEL_DEVICE, dev);

	mutex_lock(&group->mutex);
	list_for_each_entry(tmp_device, &group->devices, list) {
		if (tmp_device->dev == dev) {
			device = tmp_device;
			list_del(&device->list);
			break;
		}
	}
	mutex_unlock(&group->mutex);

	if (!device)
		return;

	sysfs_remove_link(group->devices_kobj, device->name);
	sysfs_remove_link(&dev->kobj, "iommu_group");

	kfree(device->name);
	kfree(device);
	dev->iommu_group = NULL;
	kobject_put(group->devices_kobj);
}
EXPORT_SYMBOL_GPL(iommu_group_remove_device);

int iommu_group_for_each_dev(struct iommu_group *group, void *data,
			     int (*fn)(struct device *, void *))
{
	struct iommu_device *device;
	int ret = 0;

	mutex_lock(&group->mutex);
	list_for_each_entry(device, &group->devices, list) {
		ret = fn(device->dev, data);
		if (ret)
			break;
	}
	mutex_unlock(&group->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_group_for_each_dev);

struct iommu_group *iommu_group_get(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;

	if (group)
		kobject_get(group->devices_kobj);

	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_get);

struct iommu_group *iommu_group_find(const char *name)
{
	struct iommu_group *group;
	int next = 0;

	mutex_lock(&iommu_group_mutex);
	while ((group = idr_get_next(&iommu_group_idr, &next))) {
		if (group->name) {
			if (strcmp(group->name, name) == 0)
				break;
		}
		++next;
	}
	mutex_unlock(&iommu_group_mutex);

	if (group)
		kobject_get(group->devices_kobj);

	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_find);

void iommu_group_put(struct iommu_group *group)
{
	if (group)
		kobject_put(group->devices_kobj);
}
EXPORT_SYMBOL_GPL(iommu_group_put);

int iommu_group_register_notifier(struct iommu_group *group,
				  struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&group->notifier, nb);
}
EXPORT_SYMBOL_GPL(iommu_group_register_notifier);

int iommu_group_unregister_notifier(struct iommu_group *group,
				    struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&group->notifier, nb);
}
EXPORT_SYMBOL_GPL(iommu_group_unregister_notifier);

int iommu_group_id(struct iommu_group *group)
{
	return group->id;
}
EXPORT_SYMBOL_GPL(iommu_group_id);

static int add_iommu_group(struct device *dev, void *data)
{
	struct iommu_ops *ops = data;

	if (!ops->add_device)
		return -ENODEV;

	WARN_ON(dev->iommu_group);

	ops->add_device(dev);

	return 0;
}

static int iommu_bus_notifier(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct device *dev = data;
	struct iommu_ops *ops = dev->bus->iommu_ops;
	struct iommu_group *group;
	unsigned long group_action = 0;

	if (action == BUS_NOTIFY_ADD_DEVICE) {
		if (ops->add_device)
			return ops->add_device(dev);
	} else if (action == BUS_NOTIFY_DEL_DEVICE) {
		if (ops->remove_device && dev->iommu_group) {
			ops->remove_device(dev);
			return 0;
		}
	}

	group = iommu_group_get(dev);
	if (!group)
		return 0;

	switch (action) {
	case BUS_NOTIFY_BIND_DRIVER:
		group_action = IOMMU_GROUP_NOTIFY_BIND_DRIVER;
		break;
	case BUS_NOTIFY_BOUND_DRIVER:
		group_action = IOMMU_GROUP_NOTIFY_BOUND_DRIVER;
		break;
	case BUS_NOTIFY_UNBIND_DRIVER:
		group_action = IOMMU_GROUP_NOTIFY_UNBIND_DRIVER;
		break;
	case BUS_NOTIFY_UNBOUND_DRIVER:
		group_action = IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER;
		break;
	}

	if (group_action)
		blocking_notifier_call_chain(&group->notifier,
					     group_action, dev);

	iommu_group_put(group);
	return 0;
}

static struct notifier_block iommu_bus_nb = {
	.notifier_call = iommu_bus_notifier,
};

static void iommu_bus_init(struct bus_type *bus, struct iommu_ops *ops)
{
	bus_register_notifier(bus, &iommu_bus_nb);
	bus_for_each_dev(bus, NULL, ops, add_iommu_group);
}

int bus_set_iommu(struct bus_type *bus, struct iommu_ops *ops)
{
	if (bus->iommu_ops != NULL)
		return -EBUSY;

	bus->iommu_ops = ops;

	
	iommu_bus_init(bus, ops);

	return 0;
}
EXPORT_SYMBOL_GPL(bus_set_iommu);

bool iommu_present(struct bus_type *bus)
{
	return bus->iommu_ops != NULL;
}
EXPORT_SYMBOL_GPL(iommu_present);

void iommu_set_fault_handler(struct iommu_domain *domain,
					iommu_fault_handler_t handler,
					void *token)
{
	BUG_ON(!domain);

	domain->handler = handler;
	domain->handler_token = token;
}
EXPORT_SYMBOL_GPL(iommu_set_fault_handler);

struct iommu_domain *iommu_domain_alloc(struct bus_type *bus, int flags)
{
	struct iommu_domain *domain;
	int ret;

	if (bus == NULL || bus->iommu_ops == NULL)
		return NULL;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	domain->ops = bus->iommu_ops;

	ret = domain->ops->domain_init(domain, flags);
	if (ret)
		goto out_free;

	return domain;

out_free:
	kfree(domain);

	return NULL;
}
EXPORT_SYMBOL_GPL(iommu_domain_alloc);

void iommu_domain_free(struct iommu_domain *domain)
{
	if (likely(domain->ops->domain_destroy != NULL))
		domain->ops->domain_destroy(domain);

	kfree(domain);
}
EXPORT_SYMBOL_GPL(iommu_domain_free);

int iommu_attach_device(struct iommu_domain *domain, struct device *dev)
{
	if (unlikely(domain->ops->attach_dev == NULL))
		return -ENODEV;

	return domain->ops->attach_dev(domain, dev);
}
EXPORT_SYMBOL_GPL(iommu_attach_device);

void iommu_detach_device(struct iommu_domain *domain, struct device *dev)
{
	if (unlikely(domain->ops->detach_dev == NULL))
		return;

	domain->ops->detach_dev(domain, dev);
}
EXPORT_SYMBOL_GPL(iommu_detach_device);

static int iommu_group_do_attach_device(struct device *dev, void *data)
{
	struct iommu_domain *domain = data;

	return iommu_attach_device(domain, dev);
}

int iommu_attach_group(struct iommu_domain *domain, struct iommu_group *group)
{
	return iommu_group_for_each_dev(group, domain,
					iommu_group_do_attach_device);
}
EXPORT_SYMBOL_GPL(iommu_attach_group);

static int iommu_group_do_detach_device(struct device *dev, void *data)
{
	struct iommu_domain *domain = data;

	iommu_detach_device(domain, dev);

	return 0;
}

void iommu_detach_group(struct iommu_domain *domain, struct iommu_group *group)
{
	iommu_group_for_each_dev(group, domain, iommu_group_do_detach_device);
}
EXPORT_SYMBOL_GPL(iommu_detach_group);

phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain,
			       unsigned long iova)
{
	if (unlikely(domain->ops->iova_to_phys == NULL))
		return 0;

	return domain->ops->iova_to_phys(domain, iova);
}
EXPORT_SYMBOL_GPL(iommu_iova_to_phys);

int iommu_domain_has_cap(struct iommu_domain *domain,
			 unsigned long cap)
{
	if (unlikely(domain->ops->domain_has_cap == NULL))
		return 0;

	return domain->ops->domain_has_cap(domain, cap);
}
EXPORT_SYMBOL_GPL(iommu_domain_has_cap);

int iommu_map(struct iommu_domain *domain, unsigned long iova,
	      phys_addr_t paddr, size_t size, int prot)
{
	unsigned long orig_iova = iova;
	unsigned int min_pagesz;
	size_t orig_size = size;
	int ret = 0;

	if (unlikely(domain->ops->map == NULL))
		return -ENODEV;

	
	min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

	if (!IS_ALIGNED(iova | paddr | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx pa 0x%lx size 0x%lx min_pagesz "
			"0x%x\n", iova, (unsigned long)paddr,
			(unsigned long)size, min_pagesz);
		return -EINVAL;
	}

	pr_debug("map: iova 0x%lx pa 0x%lx size 0x%lx\n", iova,
				(unsigned long)paddr, (unsigned long)size);

	while (size) {
		unsigned long pgsize, addr_merge = iova | paddr;
		unsigned int pgsize_idx;

		
		pgsize_idx = __fls(size);

		
		if (likely(addr_merge)) {
			
			unsigned int align_pgsize_idx = __ffs(addr_merge);

			pgsize_idx = min(pgsize_idx, align_pgsize_idx);
		}

		
		pgsize = (1UL << (pgsize_idx + 1)) - 1;

		
		pgsize &= domain->ops->pgsize_bitmap;

		
		BUG_ON(!pgsize);

		
		pgsize_idx = __fls(pgsize);
		pgsize = 1UL << pgsize_idx;

		pr_debug("mapping: iova 0x%lx pa 0x%lx pgsize %lu\n", iova,
					(unsigned long)paddr, pgsize);

		ret = domain->ops->map(domain, iova, paddr, pgsize, prot);
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}

	
	if (ret)
		iommu_unmap(domain, orig_iova, orig_size - size);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_map);

size_t iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size)
{
	size_t unmapped_page, unmapped = 0;
	unsigned int min_pagesz;

	if (unlikely(domain->ops->unmap == NULL))
		return -ENODEV;

	
	min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

	if (!IS_ALIGNED(iova | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx size 0x%lx min_pagesz 0x%x\n",
					iova, (unsigned long)size, min_pagesz);
		return -EINVAL;
	}

	pr_debug("unmap this: iova 0x%lx size 0x%lx\n", iova,
							(unsigned long)size);

	while (unmapped < size) {
		size_t left = size - unmapped;

		unmapped_page = domain->ops->unmap(domain, iova, left);
		if (!unmapped_page)
			break;

		pr_debug("unmapped: iova 0x%lx size %lx\n", iova,
					(unsigned long)unmapped_page);

		iova += unmapped_page;
		unmapped += unmapped_page;
	}

	return unmapped;
}
EXPORT_SYMBOL_GPL(iommu_unmap);

int iommu_map_range(struct iommu_domain *domain, unsigned int iova,
		    struct scatterlist *sg, unsigned int len, int prot)
{
	if (unlikely(domain->ops->map_range == NULL))
		return -ENODEV;

	BUG_ON(iova & (~PAGE_MASK));

	return domain->ops->map_range(domain, iova, sg, len, prot);
}
EXPORT_SYMBOL_GPL(iommu_map_range);

int iommu_unmap_range(struct iommu_domain *domain, unsigned int iova,
		      unsigned int len)
{
	if (unlikely(domain->ops->unmap_range == NULL))
		return -ENODEV;

	BUG_ON(iova & (~PAGE_MASK));

	return domain->ops->unmap_range(domain, iova, len);
}
EXPORT_SYMBOL_GPL(iommu_unmap_range);

phys_addr_t iommu_get_pt_base_addr(struct iommu_domain *domain)
{
	if (unlikely(domain->ops->get_pt_base_addr == NULL))
		return 0;

	return domain->ops->get_pt_base_addr(domain);
}
EXPORT_SYMBOL_GPL(iommu_get_pt_base_addr);

static int __init iommu_init(void)
{
	iommu_group_kset = kset_create_and_add("iommu_groups",
					       NULL, kernel_kobj);
	idr_init(&iommu_group_idr);
	mutex_init(&iommu_group_mutex);

	BUG_ON(!iommu_group_kset);

	return 0;
}
subsys_initcall(iommu_init);
