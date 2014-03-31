/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef __ASM_OPENRISC_PGTABLE_H
#define __ASM_OPENRISC_PGTABLE_H

#include <asm-generic/pgtable-nopmd.h>

#ifndef __ASSEMBLY__
#include <asm/mmu.h>
#include <asm/fixmap.h>


extern void paging_init(void);

#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
#define set_pte_at(mm, addr, ptep, pteval) set_pte(ptep, pteval)
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

#define PGDIR_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-2))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define PTRS_PER_PTE	(1UL << (PAGE_SHIFT-2))

#define PTRS_PER_PGD	(1UL << (PAGE_SHIFT-2))


#define USER_PTRS_PER_PGD       (TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_ADDRESS      0



#define VMALLOC_START	(PAGE_OFFSET-0x04000000)
#define VMALLOC_END	(PAGE_OFFSET)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))



#define _PAGE_CC       0x001 
#define _PAGE_CI       0x002 
#define _PAGE_WBC      0x004 
#define _PAGE_FILE     0x004 
#define _PAGE_WOM      0x008 

#define _PAGE_A        0x010 
#define _PAGE_D        0x020 
#define _PAGE_URE      0x040 
#define _PAGE_UWE      0x080 

#define _PAGE_SRE      0x100 
#define _PAGE_SWE      0x200 
#define _PAGE_EXEC     0x400 
#define _PAGE_U_SHARED 0x800 

#define _PAGE_PRESENT  _PAGE_CC
#define _PAGE_USER     _PAGE_URE
#define _PAGE_WRITE    (_PAGE_UWE | _PAGE_SWE)
#define _PAGE_DIRTY    _PAGE_D
#define _PAGE_ACCESSED _PAGE_A
#define _PAGE_NO_CACHE _PAGE_CI
#define _PAGE_SHARED   _PAGE_U_SHARED
#define _PAGE_READ     (_PAGE_URE | _PAGE_SRE)

#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_BASE     (_PAGE_PRESENT | _PAGE_ACCESSED)
#define _PAGE_ALL      (_PAGE_PRESENT | _PAGE_ACCESSED)
#define _KERNPG_TABLE \
	(_PAGE_BASE | _PAGE_SRE | _PAGE_SWE | _PAGE_ACCESSED | _PAGE_DIRTY)

#define PAGE_NONE       __pgprot(_PAGE_ALL)
#define PAGE_READONLY   __pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE)
#define PAGE_READONLY_X __pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE | _PAGE_EXEC)
#define PAGE_SHARED \
	__pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE | _PAGE_UWE | _PAGE_SWE \
		 | _PAGE_SHARED)
#define PAGE_SHARED_X \
	__pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE | _PAGE_UWE | _PAGE_SWE \
		 | _PAGE_SHARED | _PAGE_EXEC)
#define PAGE_COPY       __pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE)
#define PAGE_COPY_X     __pgprot(_PAGE_ALL | _PAGE_URE | _PAGE_SRE | _PAGE_EXEC)

#define PAGE_KERNEL \
	__pgprot(_PAGE_ALL | _PAGE_SRE | _PAGE_SWE \
		 | _PAGE_SHARED | _PAGE_DIRTY | _PAGE_EXEC)
#define PAGE_KERNEL_RO \
	__pgprot(_PAGE_ALL | _PAGE_SRE \
		 | _PAGE_SHARED | _PAGE_DIRTY | _PAGE_EXEC)
#define PAGE_KERNEL_NOCACHE \
	__pgprot(_PAGE_ALL | _PAGE_SRE | _PAGE_SWE \
		 | _PAGE_SHARED | _PAGE_DIRTY | _PAGE_EXEC | _PAGE_CI)

#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY_X
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY_X
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY_X
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY_X

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY_X
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED_X
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY_X
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED_X

extern unsigned long empty_zero_page[2048];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#define BITS_PER_PTR			(8*sizeof(unsigned long))

#define PTR_MASK			(~(sizeof(void *)-1))

#define SIZEOF_PTR_LOG2			2

#define PAGE_PTR(address) \
((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)

#define SET_PAGE_DIR(tsk, pgdir)

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(mm, addr, xp)	do { pte_val(*(xp)) = 0; } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define	pmd_bad(x)	((pmd_val(x) & (~PAGE_MASK)) != _KERNPG_TABLE)
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)


static inline int pte_read(pte_t pte)  { return pte_val(pte) & _PAGE_READ; }
static inline int pte_write(pte_t pte) { return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_exec(pte_t pte)  { return pte_val(pte) & _PAGE_EXEC; }
static inline int pte_dirty(pte_t pte) { return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte) { return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_file(pte_t pte)  { return pte_val(pte) & _PAGE_FILE; }
static inline int pte_special(pte_t pte) { return 0; }
static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_WRITE);
	return pte;
}

static inline pte_t pte_rdprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_READ);
	return pte;
}

static inline pte_t pte_exprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_EXEC);
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_DIRTY);
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	return pte;
}

static inline pte_t pte_mkread(pte_t pte)
{
	pte_val(pte) |= _PAGE_READ;
	return pte;
}

static inline pte_t pte_mkexec(pte_t pte)
{
	pte_val(pte) |= _PAGE_EXEC;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_DIRTY;
	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	return pte;
}



static inline pte_t __mk_pte(void *page, pgprot_t pgprot)
{
	pte_t pte;
	
	pte_val(pte) = __pa(page) | pgprot_val(pgprot);
	return pte;
}

#define mk_pte(page, pgprot) __mk_pte(page_address(page), (pgprot))

#define mk_pte_phys(physpage, pgprot) \
({                                                                      \
	pte_t __pte;                                                    \
									\
	pte_val(__pte) = (physpage) + pgprot_val(pgprot);               \
	__pte;                                                          \
})

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot);
	return pte;
}



static inline unsigned long __pte_page(pte_t pte)
{
	
	return (unsigned long)__va(pte_val(pte) & PAGE_MASK);
}

#define pte_pagenr(pte)         ((__pte_page(pte) - PAGE_OFFSET) >> PAGE_SHIFT)


#define __page_address(page) (PAGE_OFFSET + (((page) - mem_map) << PAGE_SHIFT))
#define pte_page(pte)		(mem_map+pte_pagenr(pte))

static inline void pmd_set(pmd_t *pmdp, pte_t *ptep)
{
	pmd_val(*pmdp) = _KERNPG_TABLE | (unsigned long) ptep;
}

#define pmd_page(pmd)		(pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT))
#define pmd_page_kernel(pmd)    ((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#define pgd_index(address)      ((address >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

#define __pgd_offset(address)   pgd_index(address)

#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

#define __pmd_offset(address) \
	(((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

#define __pte_offset(address)                   \
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir, address)         \
	((pte_t *) pmd_page_kernel(*(dir)) +  __pte_offset(address))
#define pte_offset_map(dir, address)	        \
	((pte_t *)page_address(pmd_page(*(dir))) + __pte_offset(address))
#define pte_offset_map_nested(dir, address)     \
	pte_offset_map(dir, address)

#define pte_unmap(pte)          do { } while (0)
#define pte_unmap_nested(pte)   do { } while (0)
#define pte_pfn(x)		((unsigned long)(((x).pte)) >> PAGE_SHIFT)
#define pfn_pte(pfn, prot)  __pte((((pfn) << PAGE_SHIFT)) | pgprot_val(prot))

#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte %p(%08lx).\n", \
	       __FILE__, __LINE__, &(e), pte_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %p(%08lx).\n", \
	       __FILE__, __LINE__, &(e), pgd_val(e))

extern pgd_t swapper_pg_dir[PTRS_PER_PGD]; 

static inline void update_mmu_cache(struct vm_area_struct *vma,
	unsigned long address, pte_t *pte)
{
}



#define __swp_type(x)			(((x).val >> 5) & 0x7f)
#define __swp_offset(x)			((x).val >> 12)
#define __swp_entry(type, offset) \
	((swp_entry_t) { ((type) << 5) | ((offset) << 12) })
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })


#define PTE_FILE_MAX_BITS               26
#define pte_to_pgoff(x)	                (pte_val(x) >> 6)
#define pgoff_to_pte(x)	                __pte(((x) << 6) | _PAGE_FILE)

#define kern_addr_valid(addr)           (1)

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)         \
	remap_pfn_range(vma, vaddr, pfn, size, prot)

#include <asm-generic/pgtable.h>

#define pgtable_cache_init()		do { } while (0)

typedef pte_t *pte_addr_t;

#endif 
#endif 
