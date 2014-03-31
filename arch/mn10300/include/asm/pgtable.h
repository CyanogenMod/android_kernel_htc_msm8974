/* MN10300 Page table manipulators and constants
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 *
 *
 * The Linux memory management assumes a three-level page table setup. On
 * the i386, we use that, but "fold" the mid level into the top-level page
 * table, so that we physically have the same two-level page table as the
 * i386 mmu expects.
 *
 * This file contains the functions and defines necessary to modify and use
 * the i386 page table tree for the purposes of the MN10300 TLB handler
 * functions.
 */
#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <asm/cpu-regs.h>

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/cache.h>
#include <linux/threads.h>

#include <asm/bitops.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))
extern unsigned long empty_zero_page[1024];
extern spinlock_t pgd_lock;
extern struct page *pgd_list;

extern void pmd_ctor(void *, struct kmem_cache *, unsigned long);
extern void pgtable_cache_init(void);
extern void paging_init(void);

#endif 

#define PGDIR_SHIFT	22
#define PTRS_PER_PGD	1024
#define PTRS_PER_PUD	1	
#define PTRS_PER_PMD	1	
#define PTRS_PER_PTE	1024

#define PGD_SIZE	PAGE_SIZE
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE - 1))

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0

#define USER_PGD_PTRS		(PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS		(PTRS_PER_PGD - USER_PGD_PTRS)

#define TWOLEVEL_PGDIR_SHIFT	22
#define BOOT_USER_PGD_PTRS	(__PAGE_OFFSET >> TWOLEVEL_PGDIR_SHIFT)
#define BOOT_KERNEL_PGD_PTRS	(1024 - BOOT_USER_PGD_PTRS)

#ifndef __ASSEMBLY__
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
#endif

#ifndef __ASSEMBLY__
#define VMALLOC_OFFSET	(8UL * 1024 * 1024)
#define VMALLOC_START	(0x70000000UL)
#define VMALLOC_END	(0x7C000000UL)
#else
#define VMALLOC_OFFSET	(8 * 1024 * 1024)
#define VMALLOC_START	(0x70000000)
#define VMALLOC_END	(0x7C000000)
#endif

#ifndef __ASSEMBLY__
extern pte_t kernel_vmalloc_ptes[(VMALLOC_END - VMALLOC_START) / PAGE_SIZE];
#endif

#define _PAGE_BIT_VALID		xPTEL2_V_BIT
#define _PAGE_BIT_CACHE		xPTEL2_C_BIT
#define _PAGE_BIT_PRESENT	xPTEL2_PV_BIT
#define _PAGE_BIT_DIRTY		xPTEL2_D_BIT
#define _PAGE_BIT_GLOBAL	xPTEL2_G_BIT
#define _PAGE_BIT_ACCESSED	xPTEL2_UNUSED1_BIT	

#define _PAGE_VALID		xPTEL2_V
#define _PAGE_CACHE		xPTEL2_C
#define _PAGE_PRESENT		xPTEL2_PV
#define _PAGE_DIRTY		xPTEL2_D
#define _PAGE_PROT		xPTEL2_PR
#define _PAGE_PROT_RKNU		xPTEL2_PR_ROK
#define _PAGE_PROT_WKNU		xPTEL2_PR_RWK
#define _PAGE_PROT_RKRU		xPTEL2_PR_ROK_ROU
#define _PAGE_PROT_WKRU		xPTEL2_PR_RWK_ROU
#define _PAGE_PROT_WKWU		xPTEL2_PR_RWK_RWU
#define _PAGE_GLOBAL		xPTEL2_G
#define _PAGE_PS_MASK		xPTEL2_PS
#define _PAGE_PS_4Kb		xPTEL2_PS_4Kb
#define _PAGE_PS_128Kb		xPTEL2_PS_128Kb
#define _PAGE_PS_1Kb		xPTEL2_PS_1Kb
#define _PAGE_PS_4Mb		xPTEL2_PS_4Mb
#define _PAGE_PSE		xPTEL2_PS_4Mb		
#define _PAGE_CACHE_WT		xPTEL2_CWT
#define _PAGE_ACCESSED		xPTEL2_UNUSED1
#define _PAGE_NX		0			

#define _PAGE_FILE		xPTEL2_C	
#define _PAGE_PROTNONE		0x000		

#define __PAGE_PROT_UWAUX	0x010
#define __PAGE_PROT_USER	0x020
#define __PAGE_PROT_WRITE	0x040

#define _PAGE_PRESENTV		(_PAGE_PRESENT|_PAGE_VALID)

#ifndef __ASSEMBLY__

#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#define _PAGE_TABLE	(_PAGE_PRESENTV | _PAGE_PROT_WKNU | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_CHG_MASK	(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

#define __PAGE_NONE	(_PAGE_PRESENTV | _PAGE_PROT_RKNU | _PAGE_ACCESSED | _PAGE_CACHE)
#define __PAGE_SHARED	(_PAGE_PRESENTV | _PAGE_PROT_WKWU | _PAGE_ACCESSED | _PAGE_CACHE)
#define __PAGE_COPY	(_PAGE_PRESENTV | _PAGE_PROT_RKRU | _PAGE_ACCESSED | _PAGE_CACHE)
#define __PAGE_READONLY	(_PAGE_PRESENTV | _PAGE_PROT_RKRU | _PAGE_ACCESSED | _PAGE_CACHE)

#define PAGE_NONE		__pgprot(__PAGE_NONE     | _PAGE_NX)
#define PAGE_SHARED_NOEXEC	__pgprot(__PAGE_SHARED   | _PAGE_NX)
#define PAGE_COPY_NOEXEC	__pgprot(__PAGE_COPY     | _PAGE_NX)
#define PAGE_READONLY_NOEXEC	__pgprot(__PAGE_READONLY | _PAGE_NX)
#define PAGE_SHARED_EXEC	__pgprot(__PAGE_SHARED)
#define PAGE_COPY_EXEC		__pgprot(__PAGE_COPY)
#define PAGE_READONLY_EXEC	__pgprot(__PAGE_READONLY)
#define PAGE_COPY		PAGE_COPY_NOEXEC
#define PAGE_READONLY		PAGE_READONLY_NOEXEC
#define PAGE_SHARED		PAGE_SHARED_EXEC

#define __PAGE_KERNEL_BASE (_PAGE_PRESENTV | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_GLOBAL)

#define __PAGE_KERNEL		(__PAGE_KERNEL_BASE | _PAGE_PROT_WKNU | _PAGE_CACHE | _PAGE_NX)
#define __PAGE_KERNEL_NOCACHE	(__PAGE_KERNEL_BASE | _PAGE_PROT_WKNU | _PAGE_NX)
#define __PAGE_KERNEL_EXEC	(__PAGE_KERNEL & ~_PAGE_NX)
#define __PAGE_KERNEL_RO	(__PAGE_KERNEL_BASE | _PAGE_PROT_RKNU | _PAGE_CACHE | _PAGE_NX)
#define __PAGE_KERNEL_LARGE	(__PAGE_KERNEL | _PAGE_PSE)
#define __PAGE_KERNEL_LARGE_EXEC (__PAGE_KERNEL_EXEC | _PAGE_PSE)

#define PAGE_KERNEL		__pgprot(__PAGE_KERNEL)
#define PAGE_KERNEL_RO		__pgprot(__PAGE_KERNEL_RO)
#define PAGE_KERNEL_EXEC	__pgprot(__PAGE_KERNEL_EXEC)
#define PAGE_KERNEL_NOCACHE	__pgprot(__PAGE_KERNEL_NOCACHE)
#define PAGE_KERNEL_LARGE	__pgprot(__PAGE_KERNEL_LARGE)
#define PAGE_KERNEL_LARGE_EXEC	__pgprot(__PAGE_KERNEL_LARGE_EXEC)

#define __PAGE_USERIO		(__PAGE_KERNEL_BASE | _PAGE_PROT_WKWU | _PAGE_NX)
#define PAGE_USERIO		__pgprot(__PAGE_USERIO)

#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY_NOEXEC
#define __P010	PAGE_COPY_NOEXEC
#define __P011	PAGE_COPY_NOEXEC
#define __P100	PAGE_READONLY_EXEC
#define __P101	PAGE_READONLY_EXEC
#define __P110	PAGE_COPY_EXEC
#define __P111	PAGE_COPY_EXEC

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY_NOEXEC
#define __S010	PAGE_SHARED_NOEXEC
#define __S011	PAGE_SHARED_NOEXEC
#define __S100	PAGE_READONLY_EXEC
#define __S101	PAGE_READONLY_EXEC
#define __S110	PAGE_SHARED_EXEC
#define __S111	PAGE_SHARED_EXEC

#undef TEST_VERIFY_AREA

#define pte_present(x)	(pte_val(x) & _PAGE_VALID)
#define pte_clear(mm, addr, xp)				\
do {							\
	set_pte_at((mm), (addr), (xp), __pte(0));	\
} while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(!pmd_none(x))
#define pmd_clear(xp)	do { set_pmd(xp, __pmd(0)); } while (0)
#define	pmd_bad(x)	0


#define pages_to_mb(x) ((x) >> (20 - PAGE_SHIFT))

#ifndef __ASSEMBLY__

static inline int pte_user(pte_t pte)	{ return pte_val(pte) & __PAGE_PROT_USER; }
static inline int pte_read(pte_t pte)	{ return pte_val(pte) & __PAGE_PROT_USER; }
static inline int pte_dirty(pte_t pte)	{ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)	{ return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_write(pte_t pte)	{ return pte_val(pte) & __PAGE_PROT_WRITE; }
static inline int pte_special(pte_t pte){ return 0; }

static inline int pte_file(pte_t pte)	{ return pte_val(pte) & _PAGE_FILE; }

static inline pte_t pte_rdprotect(pte_t pte)
{
	pte_val(pte) &= ~(__PAGE_PROT_USER|__PAGE_PROT_UWAUX); return pte;
}
static inline pte_t pte_exprotect(pte_t pte)
{
	pte_val(pte) |= _PAGE_NX; return pte;
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(__PAGE_PROT_WRITE|__PAGE_PROT_UWAUX); return pte;
}

static inline pte_t pte_mkclean(pte_t pte)	{ pte_val(pte) &= ~_PAGE_DIRTY; return pte; }
static inline pte_t pte_mkold(pte_t pte)	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkexec(pte_t pte)	{ pte_val(pte) &= ~_PAGE_NX; return pte; }

static inline pte_t pte_mkread(pte_t pte)
{
	pte_val(pte) |= __PAGE_PROT_USER;
	if (pte_write(pte))
		pte_val(pte) |= __PAGE_PROT_UWAUX;
	return pte;
}
static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= __PAGE_PROT_WRITE;
	if (pte_val(pte) & __PAGE_PROT_USER)
		pte_val(pte) |= __PAGE_PROT_UWAUX;
	return pte;
}

static inline pte_t pte_mkspecial(pte_t pte)	{ return pte; }

#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte %08lx.\n", \
	       __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", \
	       __FILE__, __LINE__, pgd_val(e))

#define pgd_clear(xp)				do { } while (0)

#define set_pte(pteptr, pteval)			(*(pteptr) = pteval)
#define set_pte_at(mm, addr, ptep, pteval)	set_pte((ptep), (pteval))
#define set_pte_atomic(pteptr, pteval)		set_pte((pteptr), (pteval))

#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

#define ptep_get_and_clear(mm, addr, ptep) \
	__pte(xchg(&(ptep)->pte, 0))
#define pte_same(a, b)		(pte_val(a) == pte_val(b))
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pte_none(x)		(!pte_val(x))
#define pte_pfn(x)		((unsigned long) (pte_val(x) >> PAGE_SHIFT))
#define __pfn_addr(pfn)		((pfn) << PAGE_SHIFT)
#define pfn_pte(pfn, prot)	__pte(__pfn_addr(pfn) | pgprot_val(prot))
#define pfn_pmd(pfn, prot)	__pmd(__pfn_addr(pfn) | pgprot_val(prot))

static inline int pte_exec(pte_t pte)
{
	return pte_user(pte);
}

static inline int pte_exec_kernel(pte_t pte)
{
	return 1;
}

#define PTE_FILE_MAX_BITS	30

#define pte_to_pgoff(pte)	(pte_val(pte) >> 2)
#define pgoff_to_pte(off)	__pte((off) << 2 | _PAGE_FILE)

#define __swp_type(x)			(((x).val >> 2) & 0x3f)
#define __swp_offset(x)			((x).val >> 8)
#define __swp_entry(type, offset) \
	((swp_entry_t) { ((type) << 2) | ((offset) << 8) })
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		__pte((x).val)

static inline
int ptep_test_and_clear_dirty(struct vm_area_struct *vma, unsigned long addr,
			      pte_t *ptep)
{
	if (!pte_dirty(*ptep))
		return 0;
	return test_and_clear_bit(_PAGE_BIT_DIRTY, &ptep->pte);
}

static inline
int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr,
			      pte_t *ptep)
{
	if (!pte_young(*ptep))
		return 0;
	return test_and_clear_bit(_PAGE_BIT_ACCESSED, &ptep->pte);
}

static inline
void ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_val(*ptep) &= ~(__PAGE_PROT_WRITE|__PAGE_PROT_UWAUX);
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	set_bit(_PAGE_BIT_DIRTY, &ptep->pte);
}

#define pgprot_noncached(prot)	__pgprot(pgprot_val(prot) & ~_PAGE_CACHE)

#define pgprot_through(prot)	__pgprot(pgprot_val(prot) | _PAGE_CACHE_WT)


#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))
#define mk_pte_huge(entry) \
	((entry).pte |= _PAGE_PRESENT | _PAGE_PSE | _PAGE_VALID)

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) &= _PAGE_CHG_MASK;
	pte_val(pte) |= pgprot_val(newprot);
	return pte;
}

#define page_pte(page)	page_pte_prot((page), __pgprot(0))

#define pmd_page_kernel(pmd) \
	((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#define pmd_page(pmd)	pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT)

#define pmd_large(pmd) \
	((pmd_val(pmd) & (_PAGE_PSE | _PAGE_PRESENT)) == \
	 (_PAGE_PSE | _PAGE_PRESENT))

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))

#define pgd_offset(mm, address)	((mm)->pgd + pgd_index(address))

#define pgd_offset_k(address)	pgd_offset(&init_mm, address)

#define pmd_index(address) \
	(((address) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))

#define pte_index(address) \
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define pte_offset_kernel(dir, address) \
	((pte_t *) pmd_page_kernel(*(dir)) +  pte_index(address))

static inline int set_kernel_exec(unsigned long vaddr, int enable)
{
	return 0;
}

#define pte_offset_map(dir, address) \
	((pte_t *) page_address(pmd_page(*(dir))) + pte_index(address))
#define pte_unmap(pte)		do {} while (0)

extern void update_mmu_cache(struct vm_area_struct *vma,
			     unsigned long address, pte_t *ptep);

#endif 

#define kern_addr_valid(addr)	(1)

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot) \
	remap_pfn_range((vma), (vaddr), (pfn), (size), (prot))

#define MK_IOSPACE_PFN(space, pfn)	(pfn)
#define GET_IOSPACE(pfn)		0
#define GET_PFN(pfn)			(pfn)

#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_DIRTY
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTEP_MKDIRTY
#define __HAVE_ARCH_PTE_SAME
#include <asm-generic/pgtable.h>

#endif 

#endif 
