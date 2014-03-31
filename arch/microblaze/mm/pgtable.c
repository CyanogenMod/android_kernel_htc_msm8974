/*
 *  This file contains the routines setting up the linux page tables.
 *
 * Copyright (C) 2008 Michal Simek
 * Copyright (C) 2008 PetaLogix
 *
 *    Copyright (C) 2007 Xilinx, Inc.  All rights reserved.
 *
 *  Derived from arch/ppc/mm/pgtable.c:
 *    -- paulus
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License.  See the file COPYING in the main directory of this
 *  archive for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <linux/io.h>
#include <asm/mmu.h>
#include <asm/sections.h>
#include <asm/fixmap.h>

#define flush_HPTE(X, va, pg)	_tlbie(va)

unsigned long ioremap_base;
unsigned long ioremap_bot;
EXPORT_SYMBOL(ioremap_bot);

#ifndef CONFIG_SMP
struct pgtable_cache_struct quicklists;
#endif

static void __iomem *__ioremap(phys_addr_t addr, unsigned long size,
		unsigned long flags)
{
	unsigned long v, i;
	phys_addr_t p;
	int err;

	p = addr & PAGE_MASK;
	size = PAGE_ALIGN(addr + size) - p;

	if (mem_init_done &&
		p >= memory_start && p < virt_to_phys(high_memory) &&
		!(p >= virt_to_phys((unsigned long)&__bss_stop) &&
		p < virt_to_phys((unsigned long)__bss_stop))) {
		printk(KERN_WARNING "__ioremap(): phys addr "PTE_FMT
			" is RAM lr %pf\n", (unsigned long)p,
			__builtin_return_address(0));
		return NULL;
	}

	if (size == 0)
		return NULL;


	if (mem_init_done) {
		struct vm_struct *area;
		area = get_vm_area(size, VM_IOREMAP);
		if (area == NULL)
			return NULL;
		v = (unsigned long) area->addr;
	} else {
		v = (ioremap_bot -= size);
	}

	if ((flags & _PAGE_PRESENT) == 0)
		flags |= _PAGE_KERNEL;
	if (flags & _PAGE_NO_CACHE)
		flags |= _PAGE_GUARDED;

	err = 0;
	for (i = 0; i < size && err == 0; i += PAGE_SIZE)
		err = map_page(v + i, p + i, flags);
	if (err) {
		if (mem_init_done)
			vfree((void *)v);
		return NULL;
	}

	return (void __iomem *) (v + ((unsigned long)addr & ~PAGE_MASK));
}

void __iomem *ioremap(phys_addr_t addr, unsigned long size)
{
	return __ioremap(addr, size, _PAGE_NO_CACHE);
}
EXPORT_SYMBOL(ioremap);

void iounmap(void *addr)
{
	if (addr > high_memory && (unsigned long) addr < ioremap_bot)
		vfree((void *) (PAGE_MASK & (unsigned long) addr));
}
EXPORT_SYMBOL(iounmap);


int map_page(unsigned long va, phys_addr_t pa, int flags)
{
	pmd_t *pd;
	pte_t *pg;
	int err = -ENOMEM;
	
	pd = pmd_offset(pgd_offset_k(va), va);
	
	pg = pte_alloc_kernel(pd, va); 
	

	if (pg != NULL) {
		err = 0;
		set_pte_at(&init_mm, va, pg, pfn_pte(pa >> PAGE_SHIFT,
				__pgprot(flags)));
		if (unlikely(mem_init_done))
			flush_HPTE(0, va, pmd_val(*pd));
			
	}
	return err;
}

void __init mapin_ram(void)
{
	unsigned long v, p, s, f;

	v = CONFIG_KERNEL_START;
	p = memory_start;
	for (s = 0; s < lowmem_size; s += PAGE_SIZE) {
		f = _PAGE_PRESENT | _PAGE_ACCESSED |
				_PAGE_SHARED | _PAGE_HWEXEC;
		if ((char *) v < _stext || (char *) v >= _etext)
			f |= _PAGE_WRENABLE;
		else
			f |= _PAGE_USER;
		map_page(v, p, f);
		v += PAGE_SIZE;
		p += PAGE_SIZE;
	}
}

#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))

static int get_pteptr(struct mm_struct *mm, unsigned long addr, pte_t **ptep)
{
	pgd_t	*pgd;
	pmd_t	*pmd;
	pte_t	*pte;
	int     retval = 0;

	pgd = pgd_offset(mm, addr & PAGE_MASK);
	if (pgd) {
		pmd = pmd_offset(pgd, addr & PAGE_MASK);
		if (pmd_present(*pmd)) {
			pte = pte_offset_kernel(pmd, addr & PAGE_MASK);
			if (pte) {
				retval = 1;
				*ptep = pte;
			}
		}
	}
	return retval;
}

unsigned long iopa(unsigned long addr)
{
	unsigned long pa;

	pte_t *pte;
	struct mm_struct *mm;

	if (addr < TASK_SIZE)
		mm = current->mm;
	else
		mm = &init_mm;

	pa = 0;
	if (get_pteptr(mm, addr, &pte))
		pa = (pte_val(*pte) & PAGE_MASK) | (addr & ~PAGE_MASK);

	return pa;
}

__init_refok pte_t *pte_alloc_one_kernel(struct mm_struct *mm,
		unsigned long address)
{
	pte_t *pte;
	if (mem_init_done) {
		pte = (pte_t *)__get_free_page(GFP_KERNEL |
					__GFP_REPEAT | __GFP_ZERO);
	} else {
		pte = (pte_t *)early_get_page();
		if (pte)
			clear_page(pte);
	}
	return pte;
}

void __set_fixmap(enum fixed_addresses idx, phys_addr_t phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses)
		BUG();

	map_page(address, phys, pgprot_val(flags));
}
