/*
 * 	PCI searching functions.
 *
 *	Copyright (C) 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 *					David Mosberger-Tang
 *	Copyright (C) 1997 -- 2000 Martin Mares <mj@ucw.cz>
 *	Copyright (C) 2003 -- 2004 Greg Kroah-Hartman <greg@kroah.com>
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include "pci.h"

DECLARE_RWSEM(pci_bus_sem);
struct pci_dev *
pci_find_upstream_pcie_bridge(struct pci_dev *pdev)
{
	struct pci_dev *tmp = NULL;

	if (pci_is_pcie(pdev))
		return NULL;
	while (1) {
		if (pci_is_root_bus(pdev->bus))
			break;
		pdev = pdev->bus->self;
		
		if (!pci_is_pcie(pdev)) {
			tmp = pdev;
			continue;
		}
		
		if (pdev->pcie_type != PCI_EXP_TYPE_PCI_BRIDGE) {
			
			WARN_ON_ONCE(1);
			return NULL;
		}
		return pdev;
	}

	return tmp;
}

static struct pci_bus *pci_do_find_bus(struct pci_bus *bus, unsigned char busnr)
{
	struct pci_bus* child;
	struct list_head *tmp;

	if(bus->number == busnr)
		return bus;

	list_for_each(tmp, &bus->children) {
		child = pci_do_find_bus(pci_bus_b(tmp), busnr);
		if(child)
			return child;
	}
	return NULL;
}

struct pci_bus * pci_find_bus(int domain, int busnr)
{
	struct pci_bus *bus = NULL;
	struct pci_bus *tmp_bus;

	while ((bus = pci_find_next_bus(bus)) != NULL)  {
		if (pci_domain_nr(bus) != domain)
			continue;
		tmp_bus = pci_do_find_bus(bus, busnr);
		if (tmp_bus)
			return tmp_bus;
	}
	return NULL;
}

struct pci_bus * 
pci_find_next_bus(const struct pci_bus *from)
{
	struct list_head *n;
	struct pci_bus *b = NULL;

	WARN_ON(in_interrupt());
	down_read(&pci_bus_sem);
	n = from ? from->node.next : pci_root_buses.next;
	if (n != &pci_root_buses)
		b = pci_bus_b(n);
	up_read(&pci_bus_sem);
	return b;
}

struct pci_dev * pci_get_slot(struct pci_bus *bus, unsigned int devfn)
{
	struct list_head *tmp;
	struct pci_dev *dev;

	WARN_ON(in_interrupt());
	down_read(&pci_bus_sem);

	list_for_each(tmp, &bus->devices) {
		dev = pci_dev_b(tmp);
		if (dev->devfn == devfn)
			goto out;
	}

	dev = NULL;
 out:
	pci_dev_get(dev);
	up_read(&pci_bus_sem);
	return dev;
}

struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
					    unsigned int devfn)
{
	struct pci_dev *dev = NULL;

	for_each_pci_dev(dev) {
		if (pci_domain_nr(dev->bus) == domain &&
		    (dev->bus->number == bus && dev->devfn == devfn))
			return dev;
	}
	return NULL;
}
EXPORT_SYMBOL(pci_get_domain_bus_and_slot);

static int match_pci_dev_by_id(struct device *dev, void *data)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_device_id *id = data;

	if (pci_match_one_device(id, pdev))
		return 1;
	return 0;
}

static struct pci_dev *pci_get_dev_by_id(const struct pci_device_id *id,
					 struct pci_dev *from)
{
	struct device *dev;
	struct device *dev_start = NULL;
	struct pci_dev *pdev = NULL;

	WARN_ON(in_interrupt());
	if (from)
		dev_start = &from->dev;
	dev = bus_find_device(&pci_bus_type, dev_start, (void *)id,
			      match_pci_dev_by_id);
	if (dev)
		pdev = to_pci_dev(dev);
	if (from)
		pci_dev_put(from);
	return pdev;
}

struct pci_dev *pci_get_subsys(unsigned int vendor, unsigned int device,
			       unsigned int ss_vendor, unsigned int ss_device,
			       struct pci_dev *from)
{
	struct pci_dev *pdev;
	struct pci_device_id *id;

	if (unlikely(no_pci_devices()))
		return NULL;

	id = kzalloc(sizeof(*id), GFP_KERNEL);
	if (!id)
		return NULL;
	id->vendor = vendor;
	id->device = device;
	id->subvendor = ss_vendor;
	id->subdevice = ss_device;

	pdev = pci_get_dev_by_id(id, from);
	kfree(id);

	return pdev;
}

struct pci_dev *
pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from)
{
	return pci_get_subsys(vendor, device, PCI_ANY_ID, PCI_ANY_ID, from);
}

struct pci_dev *pci_get_class(unsigned int class, struct pci_dev *from)
{
	struct pci_dev *dev;
	struct pci_device_id *id;

	id = kzalloc(sizeof(*id), GFP_KERNEL);
	if (!id)
		return NULL;
	id->vendor = id->device = id->subvendor = id->subdevice = PCI_ANY_ID;
	id->class_mask = PCI_ANY_ID;
	id->class = class;

	dev = pci_get_dev_by_id(id, from);
	kfree(id);
	return dev;
}

int pci_dev_present(const struct pci_device_id *ids)
{
	struct pci_dev *found = NULL;

	WARN_ON(in_interrupt());
	while (ids->vendor || ids->subvendor || ids->class_mask) {
		found = pci_get_dev_by_id(ids, NULL);
		if (found)
			goto exit;
		ids++;
	}
exit:
	if (found)
		return 1;
	return 0;
}
EXPORT_SYMBOL(pci_dev_present);

EXPORT_SYMBOL(pci_find_bus);
EXPORT_SYMBOL(pci_find_next_bus);
EXPORT_SYMBOL(pci_get_device);
EXPORT_SYMBOL(pci_get_subsys);
EXPORT_SYMBOL(pci_get_slot);
EXPORT_SYMBOL(pci_get_class);
