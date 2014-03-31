/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_PGALLOC_H
#define _ASM_TILE_PGALLOC_H

#include <linux/threads.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <asm/fixmap.h>
#include <hv/hypervisor.h>

#define L2_KERNEL_PGTABLE_SHIFT \
  (HV_LOG2_PAGE_SIZE_LARGE - HV_LOG2_PAGE_SIZE_SMALL + HV_LOG2_PTE_SIZE)

#if L2_KERNEL_PGTABLE_SHIFT < HV_LOG2_PAGE_SIZE_SMALL
#define L2_USER_PGTABLE_SHIFT HV_LOG2_PAGE_SIZE_SMALL
#else
#define L2_USER_PGTABLE_SHIFT L2_KERNEL_PGTABLE_SHIFT
#endif

#define L2_USER_PGTABLE_ORDER (L2_USER_PGTABLE_SHIFT - HV_LOG2_PAGE_SIZE_SMALL)

#define L2_KERNEL_PGTABLE_SIZE (1 << L2_KERNEL_PGTABLE_SHIFT)

static inline void set_pmd(pmd_t *pmdp, pmd_t pmd)
{
#ifdef CONFIG_64BIT
	set_pte(pmdp, pmd);
#else
	set_pte(&pmdp->pud.pgd, pmd.pud.pgd);
#endif
}

static inline void pmd_populate_kernel(struct mm_struct *mm,
				       pmd_t *pmd, pte_t *ptep)
{
	set_pmd(pmd, ptfn_pmd(__pa(ptep) >> HV_LOG2_PAGE_TABLE_ALIGN,
			      __pgprot(_PAGE_PRESENT)));
}

static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd,
				pgtable_t page)
{
	set_pmd(pmd, ptfn_pmd(HV_PFN_TO_PTFN(page_to_pfn(page)),
			      __pgprot(_PAGE_PRESENT)));
}


extern pgd_t *pgd_alloc(struct mm_struct *mm);
extern void pgd_free(struct mm_struct *mm, pgd_t *pgd);

extern pgtable_t pte_alloc_one(struct mm_struct *mm, unsigned long address);
extern void pte_free(struct mm_struct *mm, struct page *pte);

#define pmd_pgtable(pmd) pmd_page(pmd)

static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	return pfn_to_kaddr(page_to_pfn(pte_alloc_one(mm, address)));
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	BUG_ON((unsigned long)pte & (PAGE_SIZE-1));
	pte_free(mm, virt_to_page(pte));
}

extern void __pte_free_tlb(struct mmu_gather *tlb, struct page *pte,
			   unsigned long address);

#define check_pgt_cache()	do { } while (0)

pte_t *get_prealloc_pte(unsigned long pfn);

void shatter_pmd(pmd_t *pmd);

void shatter_huge_page(unsigned long addr);

#ifdef __tilegx__
#if HV_L1_SIZE != HV_L2_SIZE
# error Rework assumption that L1 and L2 page tables are same size.
#endif
#define L1_USER_PGTABLE_ORDER L2_USER_PGTABLE_ORDER
#define pud_populate(mm, pud, pmd) \
  pmd_populate_kernel((mm), (pmd_t *)(pud), (pte_t *)(pmd))
#define pmd_alloc_one(mm, addr) \
  ((pmd_t *)page_to_virt(pte_alloc_one((mm), (addr))))
#define pmd_free(mm, pmdp) \
  pte_free((mm), virt_to_page(pmdp))
#define __pmd_free_tlb(tlb, pmdp, address) \
  __pte_free_tlb((tlb), virt_to_page(pmdp), (address))
#endif

#endif 
