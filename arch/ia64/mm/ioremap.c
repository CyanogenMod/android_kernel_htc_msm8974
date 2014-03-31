/*
 * (c) Copyright 2006, 2007 Hewlett-Packard Development Company, L.P.
 *	Bjorn Helgaas <bjorn.helgaas@hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/efi.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <asm/meminit.h>

static inline void __iomem *
__ioremap (unsigned long phys_addr)
{
	return (void __iomem *) (__IA64_UNCACHED_OFFSET | phys_addr);
}

void __iomem *
early_ioremap (unsigned long phys_addr, unsigned long size)
{
	return __ioremap(phys_addr);
}

void __iomem *
ioremap (unsigned long phys_addr, unsigned long size)
{
	void __iomem *addr;
	struct vm_struct *area;
	unsigned long offset;
	pgprot_t prot;
	u64 attr;
	unsigned long gran_base, gran_size;
	unsigned long page_base;

	attr = kern_mem_attribute(phys_addr, size);
	if (attr & EFI_MEMORY_WB)
		return (void __iomem *) phys_to_virt(phys_addr);
	else if (attr & EFI_MEMORY_UC)
		return __ioremap(phys_addr);

	gran_base = GRANULEROUNDDOWN(phys_addr);
	gran_size = GRANULEROUNDUP(phys_addr + size) - gran_base;
	if (efi_mem_attribute(gran_base, gran_size) & EFI_MEMORY_WB)
		return (void __iomem *) phys_to_virt(phys_addr);

	page_base = phys_addr & PAGE_MASK;
	size = PAGE_ALIGN(phys_addr + size) - page_base;
	if (efi_mem_attribute(page_base, size) & EFI_MEMORY_WB) {
		prot = PAGE_KERNEL;

		offset = phys_addr & ~PAGE_MASK;
		phys_addr &= PAGE_MASK;

		area = get_vm_area(size, VM_IOREMAP);
		if (!area)
			return NULL;

		area->phys_addr = phys_addr;
		addr = (void __iomem *) area->addr;
		if (ioremap_page_range((unsigned long) addr,
				(unsigned long) addr + size, phys_addr, prot)) {
			vunmap((void __force *) addr);
			return NULL;
		}

		return (void __iomem *) (offset + (char __iomem *)addr);
	}

	return __ioremap(phys_addr);
}
EXPORT_SYMBOL(ioremap);

void __iomem *
ioremap_nocache (unsigned long phys_addr, unsigned long size)
{
	if (kern_mem_attribute(phys_addr, size) & EFI_MEMORY_WB)
		return NULL;

	return __ioremap(phys_addr);
}
EXPORT_SYMBOL(ioremap_nocache);

void
early_iounmap (volatile void __iomem *addr, unsigned long size)
{
}

void
iounmap (volatile void __iomem *addr)
{
	if (REGION_NUMBER(addr) == RGN_GATE)
		vunmap((void *) ((unsigned long) addr & PAGE_MASK));
}
EXPORT_SYMBOL(iounmap);
