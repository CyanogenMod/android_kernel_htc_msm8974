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

#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <linux/swap.h>
#include <asm/page.h>
#include <asm-generic/pgtable-nopmd.h>

extern unsigned long empty_zero_page;
extern unsigned long zero_page_mask;

#include <asm/vm_mmu.h>

#define _PAGE_READ	__HVM_PTE_R
#define _PAGE_WRITE	__HVM_PTE_W
#define _PAGE_EXECUTE	__HVM_PTE_X
#define _PAGE_USER	__HVM_PTE_U

#define _PAGE_PRESENT	(1<<0)
#define _PAGE_DIRTY	(1<<1)
#define _PAGE_ACCESSED	(1<<2)

#define _PAGE_FILE	_PAGE_DIRTY 

#define _PAGE_VALID	_PAGE_PRESENT


#define PGDIR_SHIFT 22
#define PTRS_PER_PGD 1024

#define PGDIR_SIZE (1UL << PGDIR_SHIFT)
#define PGDIR_MASK (~(PGDIR_SIZE-1))

#ifdef CONFIG_PAGE_SIZE_4KB
#define PTRS_PER_PTE 1024
#endif

#ifdef CONFIG_PAGE_SIZE_16KB
#define PTRS_PER_PTE 256
#endif

#ifdef CONFIG_PAGE_SIZE_64KB
#define PTRS_PER_PTE 64
#endif

#ifdef CONFIG_PAGE_SIZE_256KB
#define PTRS_PER_PTE 16
#endif

#ifdef CONFIG_PAGE_SIZE_1MB
#define PTRS_PER_PTE 4
#endif

#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__,\
		pgd_val(e))

extern unsigned long _dflt_cache_att;

#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_dflt_cache_att)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_PAGE_READ | _PAGE_EXECUTE | _dflt_cache_att)
#define PAGE_COPY	PAGE_READONLY
#define PAGE_EXEC	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_PAGE_READ | _PAGE_EXECUTE | _dflt_cache_att)
#define PAGE_COPY_EXEC	PAGE_EXEC
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | \
				_PAGE_EXECUTE | _PAGE_WRITE | _dflt_cache_att)
#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_READ | \
				_PAGE_WRITE | _PAGE_EXECUTE | _dflt_cache_att)


#define CACHEDEF	(CACHE_DEFAULT << 6)

#define __P000 __pgprot(_PAGE_PRESENT | _PAGE_USER | CACHEDEF)
#define __P001 __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | CACHEDEF)
#define __P010 __P000	
#define __P011 __P001	
#define __P100 __pgprot(_PAGE_PRESENT | _PAGE_USER | \
			_PAGE_EXECUTE | CACHEDEF)
#define __P101 __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_EXECUTE | \
			_PAGE_READ | CACHEDEF)
#define __P110 __P100	
#define __P111 __P101	

#define __S000 __P000
#define __S001 __P001
#define __S010 __pgprot(_PAGE_PRESENT | _PAGE_USER | \
			_PAGE_WRITE | CACHEDEF)
#define __S011 __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | \
			_PAGE_WRITE | CACHEDEF)
#define __S100 __pgprot(_PAGE_PRESENT | _PAGE_USER | \
			_PAGE_EXECUTE | CACHEDEF)
#define __S101 __P101
#define __S110 __pgprot(_PAGE_PRESENT | _PAGE_USER | \
			_PAGE_EXECUTE | _PAGE_WRITE | CACHEDEF)
#define __S111 __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | \
			_PAGE_EXECUTE | _PAGE_WRITE | CACHEDEF)

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];  

#define FIRST_USER_ADDRESS 0
#define pte_special(pte)	0
#define pte_mkspecial(pte)	(pte)

#ifdef CONFIG_HUGETLB_PAGE
#define pte_mkhuge(pte) __pte((pte_val(pte) & ~0x3) | HVM_HUGEPAGE_SIZE)
#endif

extern void sync_icache_dcache(pte_t pte);

#define pte_present_exec_user(pte) \
	((pte_val(pte) & (_PAGE_EXECUTE | _PAGE_USER)) == \
	(_PAGE_EXECUTE | _PAGE_USER))

static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	
	if (pte_present_exec_user(pteval))
		sync_icache_dcache(pteval);

	*ptep = pteval;
}

#define _NULL_PMD	0x7
#define _NULL_PTE	0x0

static inline void pmd_clear(pmd_t *pmd_entry_ptr)
{
	 pmd_val(*pmd_entry_ptr) = _NULL_PMD;
}

static inline void pte_clear(struct mm_struct *mm, unsigned long addr,
				pte_t *ptep)
{
	pte_val(*ptep) = _NULL_PTE;
}

#ifdef NEED_PMD_INDEX_DESPITE_BEING_2_LEVEL
#define pmd_index(address) (((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

#endif

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

#define pgd_offset(mm, addr) ((mm)->pgd + pgd_index(addr))

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

static inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) == _NULL_PMD;
}

static inline int pmd_present(pmd_t pmd)
{
	return pmd_val(pmd) != (unsigned long)_NULL_PMD;
}

static inline int pmd_bad(pmd_t pmd)
{
	return 0;
}

#define pmd_page(pmd)  (pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT))
#define pmd_pgtable(pmd) pmd_page(pmd)

static inline int pte_none(pte_t pte)
{
	return pte_val(pte) == _NULL_PTE;
};

static inline int pte_present(pte_t pte)
{
	return pte_val(pte) & _PAGE_PRESENT;
}

#define mk_pte(page, pgprot) pfn_pte(page_to_pfn(page), (pgprot))

#define pte_page(x) pfn_to_page(pte_pfn(x))

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_ACCESSED;
	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_DIRTY;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_DIRTY;
	return pte;
}

static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}

static inline int pte_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_DIRTY;
}

static inline pte_t pte_modify(pte_t pte, pgprot_t prot)
{
	pte_val(pte) &= PAGE_MASK;
	pte_val(pte) |= pgprot_val(prot);
	return pte;
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_WRITE;
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	return pte;
}

static inline pte_t pte_mkexec(pte_t pte)
{
	pte_val(pte) |= _PAGE_EXECUTE;
	return pte;
}

static inline int pte_read(pte_t pte)
{
	return pte_val(pte) & _PAGE_READ;
}

static inline int pte_write(pte_t pte)
{
	return pte_val(pte) & _PAGE_WRITE;
}


static inline int pte_exec(pte_t pte)
{
	return pte_val(pte) & _PAGE_EXECUTE;
}

#define __pte_to_swp_entry(pte) ((swp_entry_t) { pte_val(pte) })

#define __swp_entry_to_pte(x) ((pte_t) { (x).val })

#define pfn_pte(pfn, pgprot) __pte((pfn << PAGE_SHIFT) | pgprot_val(pgprot))

#define pte_pfn(pte) (pte_val(pte) >> PAGE_SHIFT)
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = (pmdval))

#define set_pte_at(mm, addr, ptep, pte) set_pte(ptep, pte)

#define pte_unmap(pte)		do { } while (0)
#define pte_unmap_nested(pte)	do { } while (0)

#define pte_offset_map(dir, address)                                    \
	((pte_t *)page_address(pmd_page(*(dir))) + __pte_offset(address))

#define pte_offset_map_nested(pmd, addr) pte_offset_map(pmd, addr)

#define pte_offset_kernel(dir, address) \
	((pte_t *) (unsigned long) __va(pmd_val(*dir) & PAGE_MASK) \
				+  __pte_offset(address))

#define ZERO_PAGE(vaddr) (virt_to_page(&empty_zero_page))

#define __pte_offset(address) (((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot) \
	remap_pfn_range(vma, vaddr, pfn, size, prot)

#define pgtable_cache_init()    do { } while (0)

#define PTE_FILE_MAX_BITS     27

#define __swp_type(swp_pte)		(((swp_pte).val >> 2) & 0x1f)

#define __swp_offset(swp_pte) \
	((((swp_pte).val >> 7) & 0x7) | (((swp_pte).val >> 10) & 0x003ffff8))

#define __swp_entry(type, offset) \
	((swp_entry_t)	{ \
		((type << 2) | \
		 ((offset & 0x3ffff8) << 10) | ((offset & 0x7) << 7)) })

#define pte_file(pte) \
	((pte_val(pte) & (_PAGE_FILE | _PAGE_PRESENT)) == _PAGE_FILE)

#define pte_to_pgoff(pte) \
	(((pte_val(pte) >> 2) & 0xff) | ((pte_val(pte) >> 5) & 0x07ffff00))

#define pgoff_to_pte(off) \
	((pte_t) { ((((off) & 0x7ffff00) << 5) | (((off) & 0xff) << 2)\
	| _PAGE_FILE) })

#include <asm-generic/pgtable.h>

#endif
