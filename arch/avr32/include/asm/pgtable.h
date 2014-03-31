/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_PGTABLE_H
#define __ASM_AVR32_PGTABLE_H

#include <asm/addrspace.h>

#ifndef __ASSEMBLY__
#include <linux/sched.h>

#endif 

#include <asm/pgtable-2level.h>

#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0

#ifndef __ASSEMBLY__
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void paging_init(void);

extern struct page *empty_zero_page;
#define ZERO_PAGE(vaddr) (empty_zero_page)

#define VMALLOC_OFFSET	(8 * 1024 * 1024)
#define VMALLOC_START	(P3SEG + VMALLOC_OFFSET)
#define VMALLOC_END	(P4SEG - VMALLOC_OFFSET)
#endif 

#define _TLBEHI_BIT_VALID	9
#define _TLBEHI_VALID		(1 << _TLBEHI_BIT_VALID)

#define _PAGE_BIT_WT		0  
#define _PAGE_BIT_DIRTY		1  
#define _PAGE_BIT_SZ0		2  
#define _PAGE_BIT_SZ1		3  
#define _PAGE_BIT_EXECUTE	4  
#define _PAGE_BIT_RW		5  
#define _PAGE_BIT_USER		6  
#define _PAGE_BIT_BUFFER	7  
#define _PAGE_BIT_GLOBAL	8  
#define _PAGE_BIT_CACHABLE	9  

#define _PAGE_BIT_PRESENT	10
#define _PAGE_BIT_ACCESSED	11 

#define _PAGE_BIT_FILE		0 

#define _PAGE_WT		(1 << _PAGE_BIT_WT)
#define _PAGE_DIRTY		(1 << _PAGE_BIT_DIRTY)
#define _PAGE_EXECUTE		(1 << _PAGE_BIT_EXECUTE)
#define _PAGE_RW		(1 << _PAGE_BIT_RW)
#define _PAGE_USER		(1 << _PAGE_BIT_USER)
#define _PAGE_BUFFER		(1 << _PAGE_BIT_BUFFER)
#define _PAGE_GLOBAL		(1 << _PAGE_BIT_GLOBAL)
#define _PAGE_CACHABLE		(1 << _PAGE_BIT_CACHABLE)

#define _PAGE_ACCESSED		(1 << _PAGE_BIT_ACCESSED)
#define _PAGE_PRESENT		(1 << _PAGE_BIT_PRESENT)
#define _PAGE_FILE		(1 << _PAGE_BIT_FILE)

#define _PAGE_TYPE_MASK		((1 << _PAGE_BIT_SZ0) | (1 << _PAGE_BIT_SZ1))
#define _PAGE_TYPE_NONE		(0 << _PAGE_BIT_SZ0)
#define _PAGE_TYPE_SMALL	(1 << _PAGE_BIT_SZ0)
#define _PAGE_TYPE_MEDIUM	(2 << _PAGE_BIT_SZ0)
#define _PAGE_TYPE_LARGE	(3 << _PAGE_BIT_SZ0)

#define _PAGE_FLAGS_HARDWARE_MASK	0xfffff3ff

#define _PAGE_FLAGS_CACHE_MASK	(_PAGE_CACHABLE | _PAGE_BUFFER | _PAGE_WT)

#define _PAGE_CHG_MASK		(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY \
				 | _PAGE_FLAGS_CACHE_MASK)

#define _PAGE_FLAGS_READ	(_PAGE_CACHABLE	| _PAGE_BUFFER)
#define _PAGE_FLAGS_WRITE	(_PAGE_FLAGS_READ | _PAGE_RW | _PAGE_DIRTY)

#define _PAGE_NORMAL(x)	__pgprot((x) | _PAGE_PRESENT | _PAGE_TYPE_SMALL	\
				 | _PAGE_ACCESSED)

#define PAGE_NONE	(_PAGE_ACCESSED | _PAGE_TYPE_NONE)
#define PAGE_READ	(_PAGE_FLAGS_READ | _PAGE_USER)
#define PAGE_EXEC	(_PAGE_FLAGS_READ | _PAGE_EXECUTE | _PAGE_USER)
#define PAGE_WRITE	(_PAGE_FLAGS_WRITE | _PAGE_USER)
#define PAGE_KERNEL	_PAGE_NORMAL(_PAGE_FLAGS_WRITE | _PAGE_EXECUTE | _PAGE_GLOBAL)
#define PAGE_KERNEL_RO	_PAGE_NORMAL(_PAGE_FLAGS_READ | _PAGE_EXECUTE | _PAGE_GLOBAL)

#define _PAGE_P(x)	_PAGE_NORMAL((x) & ~(_PAGE_RW | _PAGE_DIRTY))
#define _PAGE_S(x)	_PAGE_NORMAL(x)

#define PAGE_COPY	_PAGE_P(PAGE_WRITE | PAGE_READ)
#define PAGE_SHARED	_PAGE_S(PAGE_WRITE | PAGE_READ)

#ifndef __ASSEMBLY__

#define __P000	__pgprot(PAGE_NONE)
#define __P001	_PAGE_P(PAGE_READ)
#define __P010	_PAGE_P(PAGE_WRITE)
#define __P011	_PAGE_P(PAGE_WRITE | PAGE_READ)
#define __P100	_PAGE_P(PAGE_EXEC)
#define __P101	_PAGE_P(PAGE_EXEC | PAGE_READ)
#define __P110	_PAGE_P(PAGE_EXEC | PAGE_WRITE)
#define __P111	_PAGE_P(PAGE_EXEC | PAGE_WRITE | PAGE_READ)

#define __S000	__pgprot(PAGE_NONE)
#define __S001	_PAGE_S(PAGE_READ)
#define __S010	_PAGE_S(PAGE_WRITE)
#define __S011	_PAGE_S(PAGE_WRITE | PAGE_READ)
#define __S100	_PAGE_S(PAGE_EXEC)
#define __S101	_PAGE_S(PAGE_EXEC | PAGE_READ)
#define __S110	_PAGE_S(PAGE_EXEC | PAGE_WRITE)
#define __S111	_PAGE_S(PAGE_EXEC | PAGE_WRITE | PAGE_READ)

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)

#define pte_clear(mm,addr,xp)					\
	do {							\
		set_pte_at(mm, addr, xp, __pte(0));		\
	} while (0)

static inline int pte_write(pte_t pte)
{
	return pte_val(pte) & _PAGE_RW;
}
static inline int pte_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_DIRTY;
}
static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}
static inline int pte_special(pte_t pte)
{
	return 0;
}

static inline int pte_file(pte_t pte)
{
	return pte_val(pte) & _PAGE_FILE;
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_RW));
	return pte;
}
static inline pte_t pte_mkclean(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_DIRTY));
	return pte;
}
static inline pte_t pte_mkold(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_ACCESSED));
	return pte;
}
static inline pte_t pte_mkwrite(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) | _PAGE_RW));
	return pte;
}
static inline pte_t pte_mkdirty(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) | _PAGE_DIRTY));
	return pte;
}
static inline pte_t pte_mkyoung(pte_t pte)
{
	set_pte(&pte, __pte(pte_val(pte) | _PAGE_ACCESSED));
	return pte;
}
static inline pte_t pte_mkspecial(pte_t pte)
{
	return pte;
}

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x))

static inline void pmd_clear(pmd_t *pmdp)
{
	set_pmd(pmdp, __pmd(0));
}

#define	pmd_bad(x)	(pmd_val(x) & ~PAGE_MASK)

#define pages_to_mb(x)	((x) >> (20-PAGE_SHIFT))
#define pte_page(x)	(pfn_to_page(pte_pfn(x)))

#define pgprot_noncached(prot)						\
	__pgprot(pgprot_val(prot) & ~(_PAGE_BUFFER | _PAGE_CACHABLE))

#define pgprot_writecombine(prot)					\
	__pgprot((pgprot_val(prot) & ~_PAGE_CACHABLE) | _PAGE_BUFFER)

#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	set_pte(&pte, __pte((pte_val(pte) & _PAGE_CHG_MASK)
			    | pgprot_val(newprot)));
	return pte;
}

#define page_pte(page)	page_pte_prot(page, __pgprot(0))

#define pmd_page_vaddr(pmd)	pmd_val(pmd)
#define pmd_page(pmd)		(virt_to_page(pmd_val(pmd)))

#define pgd_index(address)	(((address) >> PGDIR_SHIFT)	\
				 & (PTRS_PER_PGD - 1))
#define pgd_offset(mm, address)	((mm)->pgd + pgd_index(address))

#define pgd_offset_k(address)	pgd_offset(&init_mm, address)

#define pte_index(address)				\
	((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, address)					\
	((pte_t *) pmd_page_vaddr(*(dir)) + pte_index(address))
#define pte_offset_kernel(dir, address)					\
	((pte_t *) pmd_page_vaddr(*(dir)) + pte_index(address))
#define pte_offset_map(dir, address) pte_offset_kernel(dir, address)
#define pte_unmap(pte)		do { } while (0)

struct vm_area_struct;
extern void update_mmu_cache(struct vm_area_struct * vma,
			     unsigned long address, pte_t *ptep);

#define __swp_type(x)		(((x).val >> 4) & 0x3f)
#define __swp_offset(x)		((x).val >> 11)
#define __swp_entry(type, offset) ((swp_entry_t) { ((type) << 4) | ((offset) << 11) })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

#define PTE_FILE_MAX_BITS	30
#define pte_to_pgoff(pte)	(((pte_val(pte) >> 1) & 0x1ff)		\
				 | ((pte_val(pte) >> 11) << 9))
#define pgoff_to_pte(off)	((pte_t) { ((((off) & 0x1ff) << 1)	\
					    | (((off) >> 9) << 11)	\
					    | _PAGE_FILE) })

typedef pte_t *pte_addr_t;

#define kern_addr_valid(addr)	(1)

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)	\
	remap_pfn_range(vma, vaddr, pfn, size, prot)

#define pgtable_cache_init()	do { } while(0)

#include <asm-generic/pgtable.h>

#endif 

#endif 
