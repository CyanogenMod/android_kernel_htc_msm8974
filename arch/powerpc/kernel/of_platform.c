/*
 *    Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corp.
 *			 <benh@kernel.crashing.org>
 *    and		 Arnd Bergmann, IBM Corp.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#undef DEBUG

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/mod_devicetable.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/atomic.h>

#include <asm/errno.h>
#include <asm/topology.h>
#include <asm/pci-bridge.h>
#include <asm/ppc-pci.h>
#include <asm/eeh.h>

#ifdef CONFIG_PPC_OF_PLATFORM_PCI


static int __devinit of_pci_phb_probe(struct platform_device *dev)
{
	struct pci_controller *phb;

	
	if (ppc_md.pci_setup_phb == NULL)
		return -ENODEV;

	pr_info("Setting up PCI bus %s\n", dev->dev.of_node->full_name);

	
	phb = pcibios_alloc_controller(dev->dev.of_node);
	if (!phb)
		return -ENODEV;

	
	phb->parent = &dev->dev;

	
	if (ppc_md.pci_setup_phb(phb)) {
		pcibios_free_controller(phb);
		return -ENODEV;
	}

	
	pci_process_bridge_OF_ranges(phb, dev->dev.of_node, 0);

	
	pci_devs_phb_init_dynamic(phb);

	
	eeh_dev_phb_init_dynamic(phb);

	
#ifdef CONFIG_EEH
	if (dev->dev.of_node->child)
		eeh_add_device_tree_early(dev->dev.of_node);
#endif 

	
	pcibios_scan_phb(phb);
	if (phb->bus == NULL)
		return -ENXIO;

	pcibios_claim_one_bus(phb->bus);

	
#ifdef CONFIG_EEH
	eeh_add_device_tree_late(phb->bus);
#endif

	
	pci_bus_add_devices(phb->bus);

	return 0;
}

static struct of_device_id of_pci_phb_ids[] = {
	{ .type = "pci", },
	{ .type = "pcix", },
	{ .type = "pcie", },
	{ .type = "pciex", },
	{ .type = "ht", },
	{}
};

static struct platform_driver of_pci_phb_driver = {
	.probe = of_pci_phb_probe,
	.driver = {
		.name = "of-pci",
		.owner = THIS_MODULE,
		.of_match_table = of_pci_phb_ids,
	},
};

static __init int of_pci_phb_init(void)
{
	return platform_driver_register(&of_pci_phb_driver);
}

device_initcall(of_pci_phb_init);

#endif 
