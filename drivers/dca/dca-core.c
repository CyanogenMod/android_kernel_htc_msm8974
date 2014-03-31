/*
 * Copyright(c) 2007 - 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called COPYING.
 */


#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/dca.h>
#include <linux/slab.h>
#include <linux/module.h>

#define DCA_VERSION "1.12.1"

MODULE_VERSION(DCA_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel Corporation");

static DEFINE_RAW_SPINLOCK(dca_lock);

static LIST_HEAD(dca_domains);

static BLOCKING_NOTIFIER_HEAD(dca_provider_chain);

static int dca_providers_blocked;

static struct pci_bus *dca_pci_rc_from_dev(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_bus *bus = pdev->bus;

	while (bus->parent)
		bus = bus->parent;

	return bus;
}

static struct dca_domain *dca_allocate_domain(struct pci_bus *rc)
{
	struct dca_domain *domain;

	domain = kzalloc(sizeof(*domain), GFP_NOWAIT);
	if (!domain)
		return NULL;

	INIT_LIST_HEAD(&domain->dca_providers);
	domain->pci_rc = rc;

	return domain;
}

static void dca_free_domain(struct dca_domain *domain)
{
	list_del(&domain->node);
	kfree(domain);
}

static int dca_provider_ioat_ver_3_0(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	return ((pdev->vendor == PCI_VENDOR_ID_INTEL) &&
		((pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG0) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG1) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG2) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG3) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG4) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG5) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG6) ||
		(pdev->device == PCI_DEVICE_ID_INTEL_IOAT_TBG7)));
}

static void unregister_dca_providers(void)
{
	struct dca_provider *dca, *_dca;
	struct list_head unregistered_providers;
	struct dca_domain *domain;
	unsigned long flags;

	blocking_notifier_call_chain(&dca_provider_chain,
				     DCA_PROVIDER_REMOVE, NULL);

	INIT_LIST_HEAD(&unregistered_providers);

	raw_spin_lock_irqsave(&dca_lock, flags);

	if (list_empty(&dca_domains)) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return;
	}

	
	domain = list_first_entry(&dca_domains, struct dca_domain, node);

	list_for_each_entry_safe(dca, _dca, &domain->dca_providers, node)
		list_move(&dca->node, &unregistered_providers);

	dca_free_domain(domain);

	raw_spin_unlock_irqrestore(&dca_lock, flags);

	list_for_each_entry_safe(dca, _dca, &unregistered_providers, node) {
		dca_sysfs_remove_provider(dca);
		list_del(&dca->node);
	}
}

static struct dca_domain *dca_find_domain(struct pci_bus *rc)
{
	struct dca_domain *domain;

	list_for_each_entry(domain, &dca_domains, node)
		if (domain->pci_rc == rc)
			return domain;

	return NULL;
}

static struct dca_domain *dca_get_domain(struct device *dev)
{
	struct pci_bus *rc;
	struct dca_domain *domain;

	rc = dca_pci_rc_from_dev(dev);
	domain = dca_find_domain(rc);

	if (!domain) {
		if (dca_provider_ioat_ver_3_0(dev) && !list_empty(&dca_domains))
			dca_providers_blocked = 1;
	}

	return domain;
}

static struct dca_provider *dca_find_provider_by_dev(struct device *dev)
{
	struct dca_provider *dca;
	struct pci_bus *rc;
	struct dca_domain *domain;

	if (dev) {
		rc = dca_pci_rc_from_dev(dev);
		domain = dca_find_domain(rc);
		if (!domain)
			return NULL;
	} else {
		if (!list_empty(&dca_domains))
			domain = list_first_entry(&dca_domains,
						  struct dca_domain,
						  node);
		else
			return NULL;
	}

	list_for_each_entry(dca, &domain->dca_providers, node)
		if ((!dev) || (dca->ops->dev_managed(dca, dev)))
			return dca;

	return NULL;
}

int dca_add_requester(struct device *dev)
{
	struct dca_provider *dca;
	int err, slot = -ENODEV;
	unsigned long flags;
	struct pci_bus *pci_rc;
	struct dca_domain *domain;

	if (!dev)
		return -EFAULT;

	raw_spin_lock_irqsave(&dca_lock, flags);

	
	dca = dca_find_provider_by_dev(dev);
	if (dca) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return -EEXIST;
	}

	pci_rc = dca_pci_rc_from_dev(dev);
	domain = dca_find_domain(pci_rc);
	if (!domain) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return -ENODEV;
	}

	list_for_each_entry(dca, &domain->dca_providers, node) {
		slot = dca->ops->add_requester(dca, dev);
		if (slot >= 0)
			break;
	}

	raw_spin_unlock_irqrestore(&dca_lock, flags);

	if (slot < 0)
		return slot;

	err = dca_sysfs_add_req(dca, dev, slot);
	if (err) {
		raw_spin_lock_irqsave(&dca_lock, flags);
		if (dca == dca_find_provider_by_dev(dev))
			dca->ops->remove_requester(dca, dev);
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dca_add_requester);

int dca_remove_requester(struct device *dev)
{
	struct dca_provider *dca;
	int slot;
	unsigned long flags;

	if (!dev)
		return -EFAULT;

	raw_spin_lock_irqsave(&dca_lock, flags);
	dca = dca_find_provider_by_dev(dev);
	if (!dca) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return -ENODEV;
	}
	slot = dca->ops->remove_requester(dca, dev);
	raw_spin_unlock_irqrestore(&dca_lock, flags);

	if (slot < 0)
		return slot;

	dca_sysfs_remove_req(dca, slot);

	return 0;
}
EXPORT_SYMBOL_GPL(dca_remove_requester);

u8 dca_common_get_tag(struct device *dev, int cpu)
{
	struct dca_provider *dca;
	u8 tag;
	unsigned long flags;

	raw_spin_lock_irqsave(&dca_lock, flags);

	dca = dca_find_provider_by_dev(dev);
	if (!dca) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return -ENODEV;
	}
	tag = dca->ops->get_tag(dca, dev, cpu);

	raw_spin_unlock_irqrestore(&dca_lock, flags);
	return tag;
}

u8 dca3_get_tag(struct device *dev, int cpu)
{
	if (!dev)
		return -EFAULT;

	return dca_common_get_tag(dev, cpu);
}
EXPORT_SYMBOL_GPL(dca3_get_tag);

u8 dca_get_tag(int cpu)
{
	struct device *dev = NULL;

	return dca_common_get_tag(dev, cpu);
}
EXPORT_SYMBOL_GPL(dca_get_tag);

struct dca_provider *alloc_dca_provider(struct dca_ops *ops, int priv_size)
{
	struct dca_provider *dca;
	int alloc_size;

	alloc_size = (sizeof(*dca) + priv_size);
	dca = kzalloc(alloc_size, GFP_KERNEL);
	if (!dca)
		return NULL;
	dca->ops = ops;

	return dca;
}
EXPORT_SYMBOL_GPL(alloc_dca_provider);

void free_dca_provider(struct dca_provider *dca)
{
	kfree(dca);
}
EXPORT_SYMBOL_GPL(free_dca_provider);

int register_dca_provider(struct dca_provider *dca, struct device *dev)
{
	int err;
	unsigned long flags;
	struct dca_domain *domain, *newdomain = NULL;

	raw_spin_lock_irqsave(&dca_lock, flags);
	if (dca_providers_blocked) {
		raw_spin_unlock_irqrestore(&dca_lock, flags);
		return -ENODEV;
	}
	raw_spin_unlock_irqrestore(&dca_lock, flags);

	err = dca_sysfs_add_provider(dca, dev);
	if (err)
		return err;

	raw_spin_lock_irqsave(&dca_lock, flags);
	domain = dca_get_domain(dev);
	if (!domain) {
		struct pci_bus *rc;

		if (dca_providers_blocked) {
			raw_spin_unlock_irqrestore(&dca_lock, flags);
			dca_sysfs_remove_provider(dca);
			unregister_dca_providers();
			return -ENODEV;
		}

		raw_spin_unlock_irqrestore(&dca_lock, flags);
		rc = dca_pci_rc_from_dev(dev);
		newdomain = dca_allocate_domain(rc);
		if (!newdomain)
			return -ENODEV;
		raw_spin_lock_irqsave(&dca_lock, flags);
		
		domain = dca_get_domain(dev);
		if (!domain) {
			domain = newdomain;
			newdomain = NULL;
			list_add(&domain->node, &dca_domains);
		}
	}
	list_add(&dca->node, &domain->dca_providers);
	raw_spin_unlock_irqrestore(&dca_lock, flags);

	blocking_notifier_call_chain(&dca_provider_chain,
				     DCA_PROVIDER_ADD, NULL);
	kfree(newdomain);
	return 0;
}
EXPORT_SYMBOL_GPL(register_dca_provider);

void unregister_dca_provider(struct dca_provider *dca, struct device *dev)
{
	unsigned long flags;
	struct pci_bus *pci_rc;
	struct dca_domain *domain;

	blocking_notifier_call_chain(&dca_provider_chain,
				     DCA_PROVIDER_REMOVE, NULL);

	raw_spin_lock_irqsave(&dca_lock, flags);

	list_del(&dca->node);

	pci_rc = dca_pci_rc_from_dev(dev);
	domain = dca_find_domain(pci_rc);
	if (list_empty(&domain->dca_providers))
		dca_free_domain(domain);

	raw_spin_unlock_irqrestore(&dca_lock, flags);

	dca_sysfs_remove_provider(dca);
}
EXPORT_SYMBOL_GPL(unregister_dca_provider);

void dca_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&dca_provider_chain, nb);
}
EXPORT_SYMBOL_GPL(dca_register_notify);

void dca_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&dca_provider_chain, nb);
}
EXPORT_SYMBOL_GPL(dca_unregister_notify);

static int __init dca_init(void)
{
	pr_info("dca service started, version %s\n", DCA_VERSION);
	return dca_sysfs_init();
}

static void __exit dca_exit(void)
{
	dca_sysfs_exit();
}

arch_initcall(dca_init);
module_exit(dca_exit);

