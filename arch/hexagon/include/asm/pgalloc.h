/*
 * Page table support for the Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H

#include <asm/mem-layout.h>
#include <asm/atomic.h>

#define check_pgt_cache() do {} while (0)

extern unsigned long long kmap_generation;

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;

	pgd = (pgd_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);


	memcpy(pgd, swapper_pg_dir, PTRS_PER_PGD*sizeof(pgd_t *));
	mm->context.generation = kmap_generation;

	
	mm->context.ptbase = __pa(pgd);

	return pgd;
}

static inline void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	free_page((unsigned long) pgd);
}

static inline struct page *pte_alloc_one(struct mm_struct *mm,
					 unsigned long address)
{
	struct page *pte;

	pte = alloc_page(GFP_KERNEL | __GFP_REPEAT | __GFP_ZERO);

	if (pte)
		pgtable_page_ctor(pte);

	return pte;
}

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm,
					  unsigned long address)
{
	gfp_t flags =  GFP_KERNEL | __GFP_REPEAT | __GFP_ZERO;
	return (pte_t *) __get_free_page(flags);
}

static inline void pte_free(struct mm_struct *mm, struct page *pte)
{
	pgtable_page_dtor(pte);
	__free_page(pte);
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	free_page((unsigned long)pte);
}

static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd,
				pgtable_t pte)
{
	set_pmd(pmd, __pmd(((unsigned long)page_to_pfn(pte) << PAGE_SHIFT) |
		HEXAGON_L1_PTE_SIZE));
}

static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd,
				       pte_t *pte)
{
	extern spinlock_t kmap_gen_lock;
	pmd_t *ppmd;
	int pmdindex;

	spin_lock(&kmap_gen_lock);
	kmap_generation++;
	mm->context.generation = kmap_generation;
	current->active_mm->context.generation = kmap_generation;
	spin_unlock(&kmap_gen_lock);

	set_pmd(pmd, __pmd(((unsigned long)__pa(pte)) | HEXAGON_L1_PTE_SIZE));

	pmdindex = (pgd_t *)pmd - mm->pgd;
	ppmd = (pmd_t *)current->active_mm->pgd + pmdindex;
	set_pmd(ppmd, __pmd(((unsigned long)__pa(pte)) | HEXAGON_L1_PTE_SIZE));
	if (pmdindex > max_kernel_seg)
		max_kernel_seg = pmdindex;
}

#define __pte_free_tlb(tlb, pte, addr)		\
do {						\
	pgtable_page_dtor((pte));		\
	tlb_remove_page((tlb), (pte));		\
} while (0)

#endif
