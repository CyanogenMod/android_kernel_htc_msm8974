/*
 * Routines for tracking a legacy ISA bridge
 *
 * Copyrigh 2007 Benjamin Herrenschmidt <benh@kernel.crashing.org>, IBM Corp.
 *
 * Some bits and pieces moved over from pci_64.c
 *
 * Copyrigh 2003 Anton Blanchard <anton@au.ibm.com>, IBM Corp.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#define DEBUG

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/notifier.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/ppc-pci.h>

unsigned long isa_io_base;	
EXPORT_SYMBOL(isa_io_base);

static struct device_node *isa_bridge_devnode;
struct pci_dev *isa_bridge_pcidev;
EXPORT_SYMBOL_GPL(isa_bridge_pcidev);

#define ISA_SPACE_MASK 0x1
#define ISA_SPACE_IO 0x1

static void __devinit pci_process_ISA_OF_ranges(struct device_node *isa_node,
						unsigned long phb_io_base_phys)
{
	
	struct pci_address {
		u32 a_hi;
		u32 a_mid;
		u32 a_lo;
	};

	struct isa_address {
		u32 a_hi;
		u32 a_lo;
	};

	struct isa_range {
		struct isa_address isa_addr;
		struct pci_address pci_addr;
		unsigned int size;
	};

	const struct isa_range *range;
	unsigned long pci_addr;
	unsigned int isa_addr;
	unsigned int size;
	int rlen = 0;

	range = of_get_property(isa_node, "ranges", &rlen);
	if (range == NULL || (rlen < sizeof(struct isa_range)))
		goto inval_range;

	if ((range->isa_addr.a_hi & ISA_SPACE_MASK) != ISA_SPACE_IO) {
		range++;
		rlen -= sizeof(struct isa_range);
		if (rlen < sizeof(struct isa_range))
			goto inval_range;
	}
	if ((range->isa_addr.a_hi & ISA_SPACE_MASK) != ISA_SPACE_IO)
		goto inval_range;

	isa_addr = range->isa_addr.a_lo;
	pci_addr = (unsigned long) range->pci_addr.a_mid << 32 |
		range->pci_addr.a_lo;

	if ((pci_addr != 0) || (isa_addr != 0)) {
		printk(KERN_ERR "unexpected isa to pci mapping: %s\n",
		       __func__);
		return;
	}

	
	size = PAGE_ALIGN(range->size);
	if (size > 0x10000)
		size = 0x10000;

	__ioremap_at(phb_io_base_phys, (void *)ISA_IO_BASE,
		     size, _PAGE_NO_CACHE|_PAGE_GUARDED);
	return;

inval_range:
	printk(KERN_ERR "no ISA IO ranges or unexpected isa range, "
	       "mapping 64k\n");
	__ioremap_at(phb_io_base_phys, (void *)ISA_IO_BASE,
		     0x10000, _PAGE_NO_CACHE|_PAGE_GUARDED);
}


void __init isa_bridge_find_early(struct pci_controller *hose)
{
	struct device_node *np, *parent = NULL, *tmp;

	
	if (isa_bridge_devnode != NULL)
		return;

	for_each_node_by_type(np, "isa") {
		
		for (parent = of_get_parent(np); parent;) {
			if (parent == hose->dn) {
				of_node_put(parent);
				break;
			}
			tmp = parent;
			parent = of_get_parent(parent);
			of_node_put(tmp);
		}
		if (parent != NULL)
			break;
	}
	if (np == NULL)
		return;
	isa_bridge_devnode = np;

	
	pci_process_ISA_OF_ranges(np, hose->io_base_phys);

	
	isa_io_base = ISA_IO_BASE;

	pr_debug("ISA bridge (early) is %s\n", np->full_name);
}

static void __devinit isa_bridge_find_late(struct pci_dev *pdev,
					   struct device_node *devnode)
{
	struct pci_controller *hose = pci_bus_to_host(pdev->bus);

	
	isa_bridge_devnode = of_node_get(devnode);
	isa_bridge_pcidev = pdev;

	
	pci_process_ISA_OF_ranges(devnode, hose->io_base_phys);

	
	isa_io_base = ISA_IO_BASE;

	pr_debug("ISA bridge (late) is %s on %s\n",
		 devnode->full_name, pci_name(pdev));
}

static void isa_bridge_remove(void)
{
	pr_debug("ISA bridge removed !\n");

	isa_io_base = ISA_IO_BASE;

	
	of_node_put(isa_bridge_devnode);
	isa_bridge_devnode = NULL;
	isa_bridge_pcidev = NULL;

	
	__iounmap_at((void *)ISA_IO_BASE, 0x10000);
}

static int __devinit isa_bridge_notify(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	struct device *dev = data;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct device_node *devnode = pci_device_to_OF_node(pdev);

	switch(action) {
	case BUS_NOTIFY_ADD_DEVICE:
		
		if (isa_bridge_devnode && isa_bridge_devnode == devnode &&
		    !isa_bridge_pcidev) {
			pr_debug("ISA bridge PCI attached: %s\n",
				 pci_name(pdev));
			isa_bridge_pcidev = pdev;
		}

		if (!isa_bridge_devnode && devnode && devnode->type &&
		    !strcmp(devnode->type, "isa"))
			isa_bridge_find_late(pdev, devnode);

		return 0;
	case BUS_NOTIFY_DEL_DEVICE:
		
		if (pdev == isa_bridge_pcidev ||
		    (devnode && devnode == isa_bridge_devnode))
			isa_bridge_remove();
		return 0;
	}
	return 0;
}

static struct notifier_block isa_bridge_notifier = {
	.notifier_call = isa_bridge_notify
};

static int __init isa_bridge_init(void)
{
	bus_register_notifier(&pci_bus_type, &isa_bridge_notifier);
	return 0;
}
arch_initcall(isa_bridge_init);
