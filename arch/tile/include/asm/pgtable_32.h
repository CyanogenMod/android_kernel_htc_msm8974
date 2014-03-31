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
 *
 */

#ifndef _ASM_TILE_PGTABLE_32_H
#define _ASM_TILE_PGTABLE_32_H

#define PGDIR_SHIFT	HV_LOG2_PAGE_SIZE_LARGE
#define PGDIR_SIZE	HV_PAGE_SIZE_LARGE
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PTRS_PER_PGD	(1 << (32 - PGDIR_SHIFT))
#define SIZEOF_PGD	(PTRS_PER_PGD * sizeof(pgd_t))

#define PTRS_PER_PTE (1 << (HV_LOG2_PAGE_SIZE_LARGE - HV_LOG2_PAGE_SIZE_SMALL))
#define SIZEOF_PTE	(PTRS_PER_PTE * sizeof(pte_t))

#ifndef __ASSEMBLY__

#define LAST_PKMAP PTRS_PER_PTE

#define PKMAP_BASE   ((FIXADDR_BOOT_START - PAGE_SIZE*LAST_PKMAP) & PGDIR_MASK)

#ifdef CONFIG_HIGHMEM
# define __VMAPPING_END	(PKMAP_BASE & ~(HPAGE_SIZE-1))
#else
# define __VMAPPING_END	(FIXADDR_START & ~(HPAGE_SIZE-1))
#endif

#ifdef CONFIG_HUGEVMAP
#define HUGE_VMAP_END	__VMAPPING_END
#define HUGE_VMAP_BASE	(HUGE_VMAP_END - CONFIG_NR_HUGE_VMAPS * HPAGE_SIZE)
#define _VMALLOC_END	HUGE_VMAP_BASE
#else
#define _VMALLOC_END	__VMAPPING_END
#endif

#define VMALLOC_END	(_VMALLOC_END - PAGE_SIZE)
extern unsigned long VMALLOC_RESERVE ;
#define _VMALLOC_START	(_VMALLOC_END - VMALLOC_RESERVE)
#define VMALLOC_START	(_VMALLOC_START + PAGE_SIZE)

#define MAXMEM		(_VMALLOC_START - PAGE_OFFSET)

#include <asm-generic/pgtable-nopmd.h>

static inline int pgd_addr_invalid(unsigned long addr)
{
	return addr >= MEM_HV_INTRPT;
}

#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_SET_WRPROTECT

extern int ptep_test_and_clear_young(struct vm_area_struct *,
				     unsigned long addr, pte_t *);
extern void ptep_set_wrprotect(struct mm_struct *,
			       unsigned long addr, pte_t *);

#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(struct mm_struct *mm,
				       unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(_mm, addr, ptep);
	return pte;
}

static inline void __set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	set_pte(&pmdp->pud.pgd, pmdval.pud.pgd);
}

static inline pmd_t ptfn_pmd(unsigned long ptfn, pgprot_t prot)
{
	return (pmd_t){ { hv_pte_set_ptfn(prot, ptfn) } };
}

#define pmd_ptfn(pmd) hv_pte_get_ptfn((pmd).pud.pgd)

static inline void pmd_clear(pmd_t *pmdp)
{
	__pte_clear(&pmdp->pud.pgd);
}

#endif 

#endif 
