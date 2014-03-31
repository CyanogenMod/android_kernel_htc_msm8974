/* pgtable.h: FR-V page table mangling
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Derived from:
 *	include/asm-m68knommu/pgtable.h
 *	include/asm-i386/pgtable.h
 */

#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <asm/mem-layout.h>
#include <asm/setup.h>
#include <asm/processor.h>

#ifndef __ASSEMBLY__
#include <linux/threads.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
struct vm_area_struct;
#endif

#ifndef __ASSEMBLY__
#if defined(CONFIG_HIGHPTE)
typedef unsigned long pte_addr_t;
#else
typedef pte_t *pte_addr_t;
#endif
#endif

#ifndef CONFIG_MMU

#define pgd_present(pgd)	(1)		
#define pgd_none(pgd)		(0)
#define pgd_bad(pgd)		(0)
#define pgd_clear(pgdp)
#define kern_addr_valid(addr)	(1)
#define	pmd_offset(a, b)	((void *) 0)

#define PAGE_NONE		__pgprot(0)	
#define PAGE_SHARED		__pgprot(0)	
#define PAGE_COPY		__pgprot(0)	
#define PAGE_READONLY		__pgprot(0)	
#define PAGE_KERNEL		__pgprot(0)	

#define __swp_type(x)		(0)
#define __swp_offset(x)		(0)
#define __swp_entry(typ,off)	((swp_entry_t) { ((typ) | ((off) << 7)) })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

#ifndef __ASSEMBLY__
static inline int pte_file(pte_t pte) { return 0; }
#endif

#define ZERO_PAGE(vaddr)	({ BUG(); NULL; })

#define swapper_pg_dir		((pgd_t *) NULL)

#define pgtable_cache_init()		do {} while (0)

#include <asm-generic/pgtable.h>

#else 

#ifndef __ASSEMBLY__
extern unsigned long empty_zero_page;
#define ZERO_PAGE(vaddr)	virt_to_page(empty_zero_page)
#endif

#define PGDIR_SHIFT		26
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE - 1))
#define PTRS_PER_PGD		64

#define PUD_SHIFT		26
#define PTRS_PER_PUD		1
#define PUD_SIZE		(1UL << PUD_SHIFT)
#define PUD_MASK		(~(PUD_SIZE - 1))
#define PUE_SIZE		256

#define PMD_SHIFT		26
#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE - 1))
#define PTRS_PER_PMD		1
#define PME_SIZE		256

#define __frv_PT_SIZE		256

#define PTRS_PER_PTE		4096

#define USER_PGDS_IN_LAST_PML4	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0

#define USER_PGD_PTRS		(PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS		(PTRS_PER_PGD - USER_PGD_PTRS)

#define TWOLEVEL_PGDIR_SHIFT	26
#define BOOT_USER_PGD_PTRS	(__PAGE_OFFSET >> TWOLEVEL_PGDIR_SHIFT)
#define BOOT_KERNEL_PGD_PTRS	(PTRS_PER_PGD - BOOT_USER_PGD_PTRS)

#ifndef __ASSEMBLY__

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, (e).pte)
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pud_ERROR(e) \
	printk("%s:%d: bad pud %08lx.\n", __FILE__, __LINE__, pmd_val(pud_val(e)))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pmd_val(pud_val(pgd_val(e))))

#define set_pte(pteptr, pteval)				\
do {							\
	*(pteptr) = (pteval);				\
	asm volatile("dcf %M0" :: "U"(*pteptr));	\
} while(0)
#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)

#define pgd_offset(mm, address) ((mm)->pgd + pgd_index(address))

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }
static inline void pgd_clear(pgd_t *pgd)	{ }

#define pgd_populate(mm, pgd, pud)		do { } while (0)
#define set_pgd(pgdptr, pgdval)				\
do {							\
	memcpy((pgdptr), &(pgdval), sizeof(pgd_t));	\
	asm volatile("dcf %M0" :: "U"(*(pgdptr)));	\
} while(0)

static inline pud_t *pud_offset(pgd_t *pgd, unsigned long address)
{
	return (pud_t *) pgd;
}

#define pgd_page(pgd)				(pud_page((pud_t){ pgd }))
#define pgd_page_vaddr(pgd)			(pud_page_vaddr((pud_t){ pgd }))

#define pud_alloc_one(mm, address)		NULL
#define pud_free(mm, x)				do { } while (0)
#define __pud_free_tlb(tlb, x, address)		do { } while (0)

static inline int pud_none(pud_t pud)		{ return 0; }
static inline int pud_bad(pud_t pud)		{ return 0; }
static inline int pud_present(pud_t pud)	{ return 1; }
static inline void pud_clear(pud_t *pud)	{ }

#define pud_populate(mm, pmd, pte)		do { } while (0)

#define set_pud(pudptr, pudval)			set_pmd((pmd_t *)(pudptr), (pmd_t) { pudval })

#define pud_page(pud)				(pmd_page((pmd_t){ pud }))
#define pud_page_vaddr(pud)			(pmd_page_vaddr((pmd_t){ pud }))

extern void __set_pmd(pmd_t *pmdptr, unsigned long __pmd);

#define set_pmd(pmdptr, pmdval)			\
do {						\
	__set_pmd((pmdptr), (pmdval).ste[0]);	\
} while(0)

#define __pmd_index(address)			0

static inline pmd_t *pmd_offset(pud_t *dir, unsigned long address)
{
	return (pmd_t *) dir + __pmd_index(address);
}

#define pte_same(a, b)		((a).pte == (b).pte)
#define pte_page(x)		(mem_map + ((unsigned long)(((x).pte >> PAGE_SHIFT))))
#define pte_none(x)		(!(x).pte)
#define pte_pfn(x)		((unsigned long)(((x).pte >> PAGE_SHIFT)))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define pfn_pmd(pfn, prot)	__pmd(((pfn) << PAGE_SHIFT) | pgprot_val(prot))

#define VMALLOC_VMADDR(x)	((unsigned long) (x))

#endif 

#define _PAGE_BIT_PRESENT	xAMPRx_V_BIT
#define _PAGE_BIT_WP		DAMPRx_WP_BIT
#define _PAGE_BIT_NOCACHE	xAMPRx_C_BIT
#define _PAGE_BIT_SUPER		xAMPRx_S_BIT
#define _PAGE_BIT_ACCESSED	xAMPRx_RESERVED8_BIT
#define _PAGE_BIT_DIRTY		xAMPRx_M_BIT
#define _PAGE_BIT_NOTGLOBAL	xAMPRx_NG_BIT

#define _PAGE_PRESENT		xAMPRx_V
#define _PAGE_WP		DAMPRx_WP
#define _PAGE_NOCACHE		xAMPRx_C
#define _PAGE_SUPER		xAMPRx_S
#define _PAGE_ACCESSED		xAMPRx_RESERVED8	
#define _PAGE_DIRTY		xAMPRx_M
#define _PAGE_NOTGLOBAL		xAMPRx_NG

#define _PAGE_RESERVED_MASK	(xAMPRx_RESERVED8 | xAMPRx_RESERVED13)

#define _PAGE_FILE		0x002	
#define _PAGE_PROTNONE		0x000	

#define _PAGE_CHG_MASK		(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

#define __PGPROT_BASE \
	(_PAGE_PRESENT | xAMPRx_SS_16Kb | xAMPRx_D | _PAGE_NOTGLOBAL | _PAGE_ACCESSED)

#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(__PGPROT_BASE)
#define PAGE_COPY	__pgprot(__PGPROT_BASE | _PAGE_WP)
#define PAGE_READONLY	__pgprot(__PGPROT_BASE | _PAGE_WP)

#define __PAGE_KERNEL		(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY)
#define __PAGE_KERNEL_NOCACHE	(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY | _PAGE_NOCACHE)
#define __PAGE_KERNEL_RO	(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY | _PAGE_WP)

#define MAKE_GLOBAL(x) __pgprot((x) & ~_PAGE_NOTGLOBAL)

#define PAGE_KERNEL		MAKE_GLOBAL(__PAGE_KERNEL)
#define PAGE_KERNEL_RO		MAKE_GLOBAL(__PAGE_KERNEL_RO)
#define PAGE_KERNEL_NOCACHE	MAKE_GLOBAL(__PAGE_KERNEL_NOCACHE)

#define _PAGE_TABLE		(_PAGE_PRESENT | xAMPRx_SS_16Kb)

#ifndef __ASSEMBLY__

#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED

#undef TEST_ACCESS_OK

#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(mm,addr,xp)	do { set_pte_at(mm, addr, xp, __pte(0)); } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define	pmd_bad(x)	(pmd_val(x) & xAMPRx_SS)
#define pmd_clear(xp)	do { __set_pmd(xp, 0); } while(0)

#define pmd_page_vaddr(pmd) \
	((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#ifndef CONFIG_DISCONTIGMEM
#define pmd_page(pmd)	(pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT))
#endif

#define pages_to_mb(x) ((x) >> (20-PAGE_SHIFT))

static inline int pte_dirty(pte_t pte)		{ return (pte).pte & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)		{ return (pte).pte & _PAGE_ACCESSED; }
static inline int pte_write(pte_t pte)		{ return !((pte).pte & _PAGE_WP); }
static inline int pte_special(pte_t pte)	{ return 0; }

static inline pte_t pte_mkclean(pte_t pte)	{ (pte).pte &= ~_PAGE_DIRTY; return pte; }
static inline pte_t pte_mkold(pte_t pte)	{ (pte).pte &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_wrprotect(pte_t pte)	{ (pte).pte |= _PAGE_WP; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)	{ (pte).pte |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ (pte).pte |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkwrite(pte_t pte)	{ (pte).pte &= ~_PAGE_WP; return pte; }
static inline pte_t pte_mkspecial(pte_t pte)	{ return pte; }

static inline int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep)
{
	int i = test_and_clear_bit(_PAGE_BIT_ACCESSED, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
	return i;
}

static inline pte_t ptep_get_and_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	unsigned long x = xchg(&ptep->pte, 0);
	asm volatile("dcf %M0" :: "U"(*ptep));
	return __pte(x);
}

static inline void ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	set_bit(_PAGE_BIT_WP, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
}

#define pgprot_noncached(prot) (__pgprot(pgprot_val(prot) | _PAGE_NOCACHE))


#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))
#define mk_pte_huge(entry)	((entry).pte_low |= _PAGE_PRESENT | _PAGE_PSE)

#define mk_pte_phys(physpage, pgprot)	pfn_pte((physpage) >> PAGE_SHIFT, pgprot)

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte.pte &= _PAGE_CHG_MASK;
	pte.pte |= pgprot_val(newprot);
	return pte;
}

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pgd_index_k(addr) pgd_index(addr)

#define __pte_index(address) (((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define pte_index(address) \
		(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir, address) \
	((pte_t *) pmd_page_vaddr(*(dir)) +  pte_index(address))

#if defined(CONFIG_HIGHPTE)
#define pte_offset_map(dir, address) \
	((pte_t *)kmap_atomic(pmd_page(*(dir))) + pte_index(address))
#define pte_unmap(pte) kunmap_atomic(pte)
#else
#define pte_offset_map(dir, address) \
	((pte_t *)page_address(pmd_page(*(dir))) + pte_index(address))
#define pte_unmap(pte) do { } while (0)
#endif

#define __swp_type(x)			(((x).val >> 2) & 0x1f)
#define __swp_offset(x)			((x).val >> 8)
#define __swp_entry(type, offset)	((swp_entry_t) { ((type) << 2) | ((offset) << 8) })
#define __pte_to_swp_entry(_pte)	((swp_entry_t) { (_pte).pte })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })

static inline int pte_file(pte_t pte)
{
	return pte.pte & _PAGE_FILE;
}

#define PTE_FILE_MAX_BITS	29

#define pte_to_pgoff(PTE)	((PTE).pte >> 2)
#define pgoff_to_pte(off)	__pte((off) << 2 | _PAGE_FILE)

#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)		\
		remap_pfn_range(vma, vaddr, pfn, size, prot)

#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTE_SAME
#include <asm-generic/pgtable.h>

static inline void update_mmu_cache(struct vm_area_struct *vma, unsigned long address, pte_t *ptep)
{
	struct mm_struct *mm;
	unsigned long ampr;

	mm = current->mm;
	if (mm) {
		pgd_t *pge = pgd_offset(mm, address);
		pud_t *pue = pud_offset(pge, address);
		pmd_t *pme = pmd_offset(pue, address);

		ampr = pme->ste[0] & 0xffffff00;
		ampr |= xAMPRx_L | xAMPRx_SS_16Kb | xAMPRx_S | xAMPRx_C |
			xAMPRx_V;
	} else {
		address = ULONG_MAX;
		ampr = 0;
	}

	asm volatile("movgs %0,scr0\n"
		     "movgs %0,scr1\n"
		     "movgs %1,dampr4\n"
		     "movgs %1,dampr5\n"
		     :
		     : "r"(address), "r"(ampr)
		     );
}

#ifdef CONFIG_PROC_FS
extern char *proc_pid_status_frv_cxnr(struct mm_struct *mm, char *buffer);
#endif

extern void __init pgtable_cache_init(void);

#endif 
#endif 

#ifndef __ASSEMBLY__
extern void __init paging_init(void);
#endif 

#endif 
