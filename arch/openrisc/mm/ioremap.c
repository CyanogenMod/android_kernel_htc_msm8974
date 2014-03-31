/*
 * OpenRISC ioremap.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/vmalloc.h>
#include <linux/io.h>
#include <asm/pgalloc.h>
#include <asm/kmap_types.h>
#include <asm/fixmap.h>
#include <asm/bug.h>
#include <asm/pgtable.h>
#include <linux/sched.h>
#include <asm/tlbflush.h>

extern int mem_init_done;

static unsigned int fixmaps_used __initdata;

void __iomem *__init_refok
__ioremap(phys_addr_t addr, unsigned long size, pgprot_t prot)
{
	phys_addr_t p;
	unsigned long v;
	unsigned long offset, last_addr;
	struct vm_struct *area = NULL;

	
	last_addr = addr + size - 1;
	if (!size || last_addr < addr)
		return NULL;

	offset = addr & ~PAGE_MASK;
	p = addr & PAGE_MASK;
	size = PAGE_ALIGN(last_addr + 1) - p;

	if (likely(mem_init_done)) {
		area = get_vm_area(size, VM_IOREMAP);
		if (!area)
			return NULL;
		v = (unsigned long)area->addr;
	} else {
		if ((fixmaps_used + (size >> PAGE_SHIFT)) > FIX_N_IOREMAPS)
			return NULL;
		v = fix_to_virt(FIX_IOREMAP_BEGIN + fixmaps_used);
		fixmaps_used += (size >> PAGE_SHIFT);
	}

	if (ioremap_page_range(v, v + size, p, prot)) {
		if (likely(mem_init_done))
			vfree(area->addr);
		else
			fixmaps_used -= (size >> PAGE_SHIFT);
		return NULL;
	}

	return (void __iomem *)(offset + (char *)v);
}

void iounmap(void *addr)
{
	if (unlikely((unsigned long)addr > FIXADDR_START)) {
		flush_tlb_all();
		return;
	}

	return vfree((void *)(PAGE_MASK & (unsigned long)addr));
}


pte_t __init_refok *pte_alloc_one_kernel(struct mm_struct *mm,
					 unsigned long address)
{
	pte_t *pte;

	if (likely(mem_init_done)) {
		pte = (pte_t *) __get_free_page(GFP_KERNEL | __GFP_REPEAT);
	} else {
		pte = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
#if 0
		
		pte = (pte_t *) __va(memblock_alloc(PAGE_SIZE, PAGE_SIZE));
#endif
	}

	if (pte)
		clear_page(pte);
	return pte;
}
