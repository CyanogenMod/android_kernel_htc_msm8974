/*
 * This file contains common routines for dealing with free of page tables
 * Along with common page table handling code
 *
 *  Derived from arch/powerpc/mm/tlb_64.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Dave Engebretsen <engebret@us.ibm.com>
 *      Rework for PPC64 port.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/hugetlb.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/tlb.h>

#include "mmu_decl.h"

static inline int is_exec_fault(void)
{
	return current->thread.regs && TRAP(current->thread.regs) == 0x400;
}

static inline int pte_looks_normal(pte_t pte)
{
	return (pte_val(pte) &
	    (_PAGE_PRESENT | _PAGE_SPECIAL | _PAGE_NO_CACHE | _PAGE_USER)) ==
	    (_PAGE_PRESENT | _PAGE_USER);
}

struct page * maybe_pte_to_page(pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);
	struct page *page;

	if (unlikely(!pfn_valid(pfn)))
		return NULL;
	page = pfn_to_page(pfn);
	if (PageReserved(page))
		return NULL;
	return page;
}

#if defined(CONFIG_PPC_STD_MMU) || _PAGE_EXEC == 0


static pte_t set_pte_filter(pte_t pte, unsigned long addr)
{
	pte = __pte(pte_val(pte) & ~_PAGE_HPTEFLAGS);
	if (pte_looks_normal(pte) && !(cpu_has_feature(CPU_FTR_COHERENT_ICACHE) ||
				       cpu_has_feature(CPU_FTR_NOEXECUTE))) {
		struct page *pg = maybe_pte_to_page(pte);
		if (!pg)
			return pte;
		if (!test_bit(PG_arch_1, &pg->flags)) {
#ifdef CONFIG_8xx
			
			_tlbil_va(addr, 0, 0, 0);
#endif 
			flush_dcache_icache_page(pg);
			set_bit(PG_arch_1, &pg->flags);
		}
	}
	return pte;
}

static pte_t set_access_flags_filter(pte_t pte, struct vm_area_struct *vma,
				     int dirty)
{
	return pte;
}

#else 

static pte_t set_pte_filter(pte_t pte, unsigned long addr)
{
	struct page *pg;

	
	if (!(pte_val(pte) & _PAGE_EXEC) || !pte_looks_normal(pte))
		return pte;

	
	pg = maybe_pte_to_page(pte);
	if (unlikely(!pg))
		return pte;

	
	if (test_bit(PG_arch_1, &pg->flags))
		return pte;

	
	if (is_exec_fault()) {
		flush_dcache_icache_page(pg);
		set_bit(PG_arch_1, &pg->flags);
		return pte;
	}

	
	return __pte(pte_val(pte) & ~_PAGE_EXEC);
}

static pte_t set_access_flags_filter(pte_t pte, struct vm_area_struct *vma,
				     int dirty)
{
	struct page *pg;

	if (dirty || (pte_val(pte) & _PAGE_EXEC) || !is_exec_fault())
		return pte;

#ifdef CONFIG_DEBUG_VM
	if (WARN_ON(!(vma->vm_flags & VM_EXEC)))
		return pte;
#endif 

	
	pg = maybe_pte_to_page(pte);
	if (unlikely(!pg))
		goto bail;

	
	if (test_bit(PG_arch_1, &pg->flags))
		goto bail;

	
	flush_dcache_icache_page(pg);
	set_bit(PG_arch_1, &pg->flags);

 bail:
	return __pte(pte_val(pte) | _PAGE_EXEC);
}

#endif 

void set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep,
		pte_t pte)
{
#ifdef CONFIG_DEBUG_VM
	WARN_ON(pte_present(*ptep));
#endif
	pte = set_pte_filter(pte, addr);

	
	__set_pte_at(mm, addr, ptep, pte, 0);
}

int ptep_set_access_flags(struct vm_area_struct *vma, unsigned long address,
			  pte_t *ptep, pte_t entry, int dirty)
{
	int changed;
	entry = set_access_flags_filter(entry, vma, dirty);
	changed = !pte_same(*(ptep), entry);
	if (changed) {
		if (!is_vm_hugetlb_page(vma))
			assert_pte_locked(vma->vm_mm, address);
		__ptep_set_access_flags(ptep, entry);
		flush_tlb_page_nohash(vma, address);
	}
	return changed;
}

#ifdef CONFIG_DEBUG_VM
void assert_pte_locked(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	if (mm == &init_mm)
		return;
	pgd = mm->pgd + pgd_index(addr);
	BUG_ON(pgd_none(*pgd));
	pud = pud_offset(pgd, addr);
	BUG_ON(pud_none(*pud));
	pmd = pmd_offset(pud, addr);
	BUG_ON(!pmd_present(*pmd));
	assert_spin_locked(pte_lockptr(mm, pmd));
}
#endif 

