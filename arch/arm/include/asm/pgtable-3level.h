/*
 * arch/arm/include/asm/pgtable-3level.h
 *
 * Copyright (C) 2011 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _ASM_PGTABLE_3LEVEL_H
#define _ASM_PGTABLE_3LEVEL_H

#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		512
#define PTRS_PER_PGD		4

#define PTE_HWTABLE_PTRS	(PTRS_PER_PTE)
#define PTE_HWTABLE_OFF		(0)
#define PTE_HWTABLE_SIZE	(PTRS_PER_PTE * sizeof(u64))

#define PGDIR_SHIFT		30

#define PMD_SHIFT		21

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))

#define SECTION_SHIFT		21
#define SECTION_SIZE		(1UL << SECTION_SHIFT)
#define SECTION_MASK		(~(SECTION_SIZE-1))

#define USER_PTRS_PER_PGD	(PAGE_OFFSET / PGDIR_SIZE)

#define L_PTE_PRESENT		(_AT(pteval_t, 3) << 0)		
#define L_PTE_FILE		(_AT(pteval_t, 1) << 2)		
#define L_PTE_BUFFERABLE	(_AT(pteval_t, 1) << 2)		
#define L_PTE_CACHEABLE		(_AT(pteval_t, 1) << 3)		
#define L_PTE_USER		(_AT(pteval_t, 1) << 6)		
#define L_PTE_RDONLY		(_AT(pteval_t, 1) << 7)		
#define L_PTE_SHARED		(_AT(pteval_t, 3) << 8)		
#define L_PTE_YOUNG		(_AT(pteval_t, 1) << 10)	
#define L_PTE_XN		(_AT(pteval_t, 1) << 54)	
#define L_PTE_DIRTY		(_AT(pteval_t, 1) << 55)	
#define L_PTE_SPECIAL		(_AT(pteval_t, 1) << 56)	

#define L_PTE_XN_HIGH		(1 << (54 - 32))
#define L_PTE_DIRTY_HIGH	(1 << (55 - 32))

#define L_PTE_MT_UNCACHED	(_AT(pteval_t, 0) << 2)	
#define L_PTE_MT_BUFFERABLE	(_AT(pteval_t, 1) << 2)	
#define L_PTE_MT_WRITETHROUGH	(_AT(pteval_t, 2) << 2)	
#define L_PTE_MT_WRITEBACK	(_AT(pteval_t, 3) << 2)	
#define L_PTE_MT_WRITEALLOC	(_AT(pteval_t, 7) << 2)	
#define L_PTE_MT_DEV_SHARED	(_AT(pteval_t, 4) << 2)	
#define L_PTE_MT_DEV_NONSHARED	(_AT(pteval_t, 4) << 2)	
#define L_PTE_MT_DEV_WC		(_AT(pteval_t, 1) << 2)	
#define L_PTE_MT_DEV_CACHED	(_AT(pteval_t, 3) << 2)	
#define L_PTE_MT_MASK		(_AT(pteval_t, 7) << 2)

#define L_PGD_SWAPPER		(_AT(pgdval_t, 1) << 55)	

#ifndef __ASSEMBLY__

#define pud_none(pud)		(!pud_val(pud))
#define pud_bad(pud)		(!(pud_val(pud) & 2))
#define pud_present(pud)	(pud_val(pud))

#define pud_clear(pudp)			\
	do {				\
		*pudp = __pud(0);	\
		clean_pmd_entry(pudp);	\
	} while (0)

#define set_pud(pudp, pud)		\
	do {				\
		*pudp = pud;		\
		flush_pmd_entry(pudp);	\
	} while (0)

static inline pmd_t *pud_page_vaddr(pud_t pud)
{
	return __va(pud_val(pud) & PHYS_MASK & (s32)PAGE_MASK);
}

#define pmd_index(addr)		(((addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
static inline pmd_t *pmd_offset(pud_t *pud, unsigned long addr)
{
	return (pmd_t *)pud_page_vaddr(*pud) + pmd_index(addr);
}

#define pmd_bad(pmd)		(!(pmd_val(pmd) & 2))

#define copy_pmd(pmdpd,pmdps)		\
	do {				\
		*pmdpd = *pmdps;	\
		flush_pmd_entry(pmdpd);	\
	} while (0)

#define pmd_clear(pmdp)			\
	do {				\
		*pmdp = __pmd(0);	\
		clean_pmd_entry(pmdp);	\
	} while (0)

#define set_pte_ext(ptep,pte,ext) cpu_set_pte_ext(ptep,__pte(pte_val(pte)|(ext)))

#endif 

#endif 
