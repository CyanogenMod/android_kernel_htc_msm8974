/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <asm/pgtable.h>
#include <asm/addrspace.h>

void __iomem *__ioremap(unsigned long phys_addr, size_t size,
			unsigned long flags)
{
	unsigned long addr;
	struct vm_struct *area;
	unsigned long offset, last_addr;
	pgprot_t prot;

	if ((phys_addr >= P4SEG) && (flags == 0))
		return (void __iomem *)phys_addr;

	
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	if (PHYSADDR(P2SEGADDR(phys_addr)) == phys_addr)
		return (void __iomem *)P2SEGADDR(phys_addr);

	
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr + 1) - phys_addr;

	prot = __pgprot(_PAGE_PRESENT | _PAGE_GLOBAL | _PAGE_RW | _PAGE_DIRTY
			| _PAGE_ACCESSED | _PAGE_TYPE_SMALL | flags);

	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = (unsigned long )area->addr;
	if (ioremap_page_range(addr, addr + size, phys_addr, prot)) {
		vunmap((void *)addr);
		return NULL;
	}

	return (void __iomem *)(offset + (char *)addr);
}
EXPORT_SYMBOL(__ioremap);

void __iounmap(void __iomem *addr)
{
	struct vm_struct *p;

	if ((unsigned long)addr >= P4SEG)
		return;
	if (PXSEG(addr) == P2SEG)
		return;

	p = remove_vm_area((void *)(PAGE_MASK & (unsigned long __force)addr));
	if (unlikely(!p)) {
		printk (KERN_ERR "iounmap: bad address %p\n", addr);
		return;
	}

	kfree (p);
}
EXPORT_SYMBOL(__iounmap);
