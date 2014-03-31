/* ASB2305 PCI resource stuff
 *
 * Copyright (C) 2001 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from arch/i386/pci-i386.c
 *   - Copyright 1997--2000 Martin Mares <mj@suse.cz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include "pci-asb2305.h"

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t align)
{
	resource_size_t start = res->start;

#if 0
	struct pci_dev *dev = data;

	printk(KERN_DEBUG
	       "### PCIBIOS_ALIGN_RESOURCE(%s,,{%08lx-%08lx,%08lx},%lx)\n",
	       pci_name(dev),
	       res->start,
	       res->end,
	       res->flags,
	       size
	       );
#endif

	if ((res->flags & IORESOURCE_IO) && (start & 0x300))
		start = (start + 0x3ff) & ~0x3ff;

	return start;
}


static void __init pcibios_allocate_bus_resources(struct list_head *bus_list)
{
	struct pci_bus *bus;
	struct pci_dev *dev;
	int idx;
	struct resource *r;

	
	list_for_each_entry(bus, bus_list, node) {
		dev = bus->self;
		if (dev) {
			for (idx = PCI_BRIDGE_RESOURCES;
			     idx < PCI_NUM_RESOURCES;
			     idx++) {
				r = &dev->resource[idx];
				if (!r->flags)
					continue;
				if (!r->start ||
				    pci_claim_resource(dev, idx) < 0) {
					printk(KERN_ERR "PCI:"
					       " Cannot allocate resource"
					       " region %d of bridge %s\n",
					       idx, pci_name(dev));
					r->start = r->end = 0;
					r->flags = 0;
				}
			}
		}
		pcibios_allocate_bus_resources(&bus->children);
	}
}

static void __init pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev = NULL;
	int idx, disabled;
	u16 command;
	struct resource *r;

	for_each_pci_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for (idx = 0; idx < 6; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		
				continue;
			if (!r->start)		
				continue;
			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled) {
				DBG("PCI[%s]: Resource %08lx-%08lx"
				    " (f=%lx, d=%d, p=%d)\n",
				    pci_name(dev), r->start, r->end, r->flags,
				    disabled, pass);
				if (pci_claim_resource(dev, idx) < 0) {
					printk(KERN_ERR "PCI:"
					       " Cannot allocate resource"
					       " region %d of device %s\n",
					       idx, pci_name(dev));
					
					r->end -= r->start;
					r->start = 0;
				}
			}
		}
		if (!pass) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (r->flags & IORESOURCE_ROM_ENABLE) {
				u32 reg;
				DBG("PCI: Switching off ROM of %s\n",
				    pci_name(dev));
				r->flags &= ~IORESOURCE_ROM_ENABLE;
				pci_read_config_dword(
					dev, dev->rom_base_reg, &reg);
				pci_write_config_dword(
					dev, dev->rom_base_reg,
					reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static int __init pcibios_assign_resources(void)
{
	struct pci_dev *dev = NULL;
	struct resource *r;

	if (!(pci_probe & PCI_ASSIGN_ROMS)) {
		for_each_pci_dev(dev) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (!r->flags || !r->start)
				continue;
			if (pci_claim_resource(dev, PCI_ROM_RESOURCE) < 0) {
				r->end -= r->start;
				r->start = 0;
			}
		}
	}

	pci_assign_unassigned_resources();

	return 0;
}

fs_initcall(pcibios_assign_resources);

void __init pcibios_resource_survey(void)
{
	DBG("PCI: Allocating resources\n");
	pcibios_allocate_bus_resources(&pci_root_buses);
	pcibios_allocate_resources(0);
	pcibios_allocate_resources(1);
}

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	unsigned long prot;

	vma->vm_flags |= VM_LOCKED | VM_IO;

	prot = pgprot_val(vma->vm_page_prot);
	prot &= ~_PAGE_CACHE;
	vma->vm_page_prot = __pgprot(prot);

	
	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}
