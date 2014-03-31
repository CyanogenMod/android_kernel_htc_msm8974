/*
 * arch/xtensa/kernel/pci.c
 *
 * PCI bios-type initialisation for PCI machines
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2001-2005 Tensilica Inc.
 *
 * Based largely on work from Cort (ppc/kernel/pci.c)
 * IO functions copied from sparc.
 *
 * Chris Zankel <chris@zankel.net>
 *
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/bootmem.h>

#include <asm/pci-bridge.h>
#include <asm/platform.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif




struct pci_controller* pci_ctrl_head;
struct pci_controller** pci_ctrl_tail = &pci_ctrl_head;

static int pci_bus_count;

resource_size_t
pcibios_align_resource(void *data, const struct resource *res,
		       resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;
	resource_size_t start = res->start;

	if (res->flags & IORESOURCE_IO) {
		if (size > 0x100) {
			printk(KERN_ERR "PCI: I/O Region %s/%d too large"
			       " (%ld bytes)\n", pci_name(dev),
			       dev->resource - res, size);
		}

		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;
	}

	return start;
}

int
pcibios_enable_resources(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for(idx=0; idx<6; idx++) {
		r = &dev->resource[idx];
		if (!r->start && r->end) {
			printk (KERN_ERR "PCI: Device %s not available because "
				"of resource collisions\n", pci_name(dev));
			return -EINVAL;
		}
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	if (dev->resource[PCI_ROM_RESOURCE].start)
		cmd |= PCI_COMMAND_MEMORY;
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n",
			pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

struct pci_controller * __init pcibios_alloc_controller(void)
{
	struct pci_controller *pci_ctrl;

	pci_ctrl = (struct pci_controller *)alloc_bootmem(sizeof(*pci_ctrl));
	memset(pci_ctrl, 0, sizeof(struct pci_controller));

	*pci_ctrl_tail = pci_ctrl;
	pci_ctrl_tail = &pci_ctrl->next;

	return pci_ctrl;
}

static void __init pci_controller_apertures(struct pci_controller *pci_ctrl,
					    struct list_head *resources)
{
	struct resource *res;
	unsigned long io_offset;
	int i;

	io_offset = (unsigned long)pci_ctrl->io_space.base;
	res = &pci_ctrl->io_resource;
	if (!res->flags) {
		if (io_offset)
			printk (KERN_ERR "I/O resource not set for host"
				" bridge %d\n", pci_ctrl->index);
		res->start = 0;
		res->end = IO_SPACE_LIMIT;
		res->flags = IORESOURCE_IO;
	}
	res->start += io_offset;
	res->end += io_offset;
	pci_add_resource_offset(resources, res, io_offset);

	for (i = 0; i < 3; i++) {
		res = &pci_ctrl->mem_resources[i];
		if (!res->flags) {
			if (i > 0)
				continue;
			printk(KERN_ERR "Memory resource not set for "
			       "host bridge %d\n", pci_ctrl->index);
			res->start = 0;
			res->end = ~0U;
			res->flags = IORESOURCE_MEM;
		}
		pci_add_resource(resources, res);
	}
}

static int __init pcibios_init(void)
{
	struct pci_controller *pci_ctrl;
	struct list_head resources;
	struct pci_bus *bus;
	int next_busno = 0, i;

	printk("PCI: Probing PCI hardware\n");

	
	for (pci_ctrl = pci_ctrl_head; pci_ctrl; pci_ctrl = pci_ctrl->next) {
		pci_ctrl->last_busno = 0xff;
		INIT_LIST_HEAD(&resources);
		pci_controller_apertures(pci_ctrl, &resources);
		bus = pci_scan_root_bus(NULL, pci_ctrl->first_busno,
					pci_ctrl->ops, pci_ctrl, &resources);
		pci_ctrl->bus = bus;
		pci_ctrl->last_busno = bus->subordinate;
		if (next_busno <= pci_ctrl->last_busno)
			next_busno = pci_ctrl->last_busno+1;
	}
	pci_bus_count = next_busno;

	return platform_pcibios_fixup();
}

subsys_initcall(pcibios_init);

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	if (bus->parent) {
		
		pci_read_bridge_bases(bus);
	}
}

char __init *pcibios_setup(char *str)
{
	return str;
}

void pcibios_set_master(struct pci_dev *dev)
{
	
}


void __init
pcibios_update_irq(struct pci_dev *dev, int irq)
{
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx=0; idx<6; idx++) {
		r = &dev->resource[idx];
		if (!r->start && r->end) {
			printk(KERN_ERR "PCI: Device %s not available because "
			       "of resource collisions\n", pci_name(dev));
			return -EINVAL;
		}
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n",
		       pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}

	return 0;
}

#ifdef CONFIG_PROC_FS


int
pci_controller_num(struct pci_dev *dev)
{
	struct pci_controller *pci_ctrl = (struct pci_controller*) dev->sysdata;
	return pci_ctrl->index;
}

#endif 


static __inline__ int
__pci_mmap_make_offset(struct pci_dev *dev, struct vm_area_struct *vma,
		       enum pci_mmap_state mmap_state)
{
	struct pci_controller *pci_ctrl = (struct pci_controller*) dev->sysdata;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long io_offset = 0;
	int i, res_bit;

	if (pci_ctrl == 0)
		return -EINVAL;		

	
	if (mmap_state == pci_mmap_mem) {
		res_bit = IORESOURCE_MEM;
	} else {
		io_offset = (unsigned long)pci_ctrl->io_space.base;
		offset += io_offset;
		res_bit = IORESOURCE_IO;
	}

	for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
		struct resource *rp = &dev->resource[i];
		int flags = rp->flags;

		
		if (i == PCI_ROM_RESOURCE)
			flags |= IORESOURCE_MEM;

		
		if ((flags & res_bit) == 0)
			continue;

		
		if (offset < (rp->start & PAGE_MASK) || offset > rp->end)
			continue;

		
		if (mmap_state == pci_mmap_io)
			offset += pci_ctrl->io_space.start - io_offset;
		vma->vm_pgoff = offset >> PAGE_SHIFT;
		return 0;
	}

	return -EINVAL;
}

static __inline__ void
__pci_mmap_set_pgprot(struct pci_dev *dev, struct vm_area_struct *vma,
		      enum pci_mmap_state mmap_state, int write_combine)
{
	int prot = pgprot_val(vma->vm_page_prot);

	
	prot &= ~_PAGE_NO_CACHE;
#if 0
	if (!write_combine)
		prot |= _PAGE_WRITETHRU;
#endif
	vma->vm_page_prot = __pgprot(prot);
}

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state,
			int write_combine)
{
	int ret;

	ret = __pci_mmap_make_offset(dev, vma, mmap_state);
	if (ret < 0)
		return ret;

	__pci_mmap_set_pgprot(dev, vma, mmap_state, write_combine);

	ret = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			         vma->vm_end - vma->vm_start,vma->vm_page_prot);

	return ret;
}
