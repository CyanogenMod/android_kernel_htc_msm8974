/* 
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright 2003 PathScale, Inc.
 * Derived from include/asm-i386/pgtable.h
 * Licensed under the GPL
 */

#ifndef __UM_PGTABLE_H
#define __UM_PGTABLE_H

#include <asm/fixmap.h>

#define _PAGE_PRESENT	0x001
#define _PAGE_NEWPAGE	0x002
#define _PAGE_NEWPROT	0x004
#define _PAGE_RW	0x020
#define _PAGE_USER	0x040
#define _PAGE_ACCESSED	0x080
#define _PAGE_DIRTY	0x100
#define _PAGE_FILE	0x008	
#define _PAGE_PROTNONE	0x010	

#ifdef CONFIG_3_LEVEL_PGTABLES
#include "asm/pgtable-3level.h"
#else
#include "asm/pgtable-2level.h"
#endif

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

extern unsigned long *empty_zero_page;

#define pgtable_cache_init() do ; while (0)


extern unsigned long end_iomem;

#define VMALLOC_OFFSET	(__va_space)
#define VMALLOC_START ((end_iomem + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define PKMAP_BASE ((FIXADDR_START - LAST_PKMAP * PAGE_SIZE) & PMD_MASK)
#ifdef CONFIG_HIGHMEM
# define VMALLOC_END	(PKMAP_BASE-2*PAGE_SIZE)
#else
# define VMALLOC_END	(FIXADDR_START-2*PAGE_SIZE)
#endif
#define MODULES_VADDR	VMALLOC_START
#define MODULES_END	VMALLOC_END
#define MODULES_LEN	(MODULES_VADDR - MODULES_END)

#define _PAGE_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _KERNPG_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
#define __PAGE_KERNEL_EXEC                                              \
	 (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED)
#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_COPY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED)
#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED)
#define PAGE_KERNEL_EXEC	__pgprot(__PAGE_KERNEL_EXEC)

#define io_remap_pfn_range	remap_pfn_range

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

#define ZERO_PAGE(vaddr) virt_to_page(empty_zero_page)

#define pte_clear(mm,addr,xp) pte_set_val(*(xp), (phys_t) 0, __pgprot(_PAGE_NEWPAGE))

#define pmd_none(x)	(!((unsigned long)pmd_val(x) & ~_PAGE_NEWPAGE))
#define	pmd_bad(x)	((pmd_val(x) & (~PAGE_MASK & ~_PAGE_USER)) != _KERNPG_TABLE)

#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = _PAGE_NEWPAGE; } while (0)

#define pmd_newpage(x)  (pmd_val(x) & _PAGE_NEWPAGE)
#define pmd_mkuptodate(x) (pmd_val(x) &= ~_PAGE_NEWPAGE)

#define pud_newpage(x)  (pud_val(x) & _PAGE_NEWPAGE)
#define pud_mkuptodate(x) (pud_val(x) &= ~_PAGE_NEWPAGE)

#define pmd_page(pmd) phys_to_page(pmd_val(pmd) & PAGE_MASK)

#define pte_page(x) pfn_to_page(pte_pfn(x))

#define pte_present(x)	pte_get_bits(x, (_PAGE_PRESENT | _PAGE_PROTNONE))


static inline int pte_none(pte_t pte)
{
	return pte_is_zero(pte);
}

static inline int pte_read(pte_t pte)
{ 
	return((pte_get_bits(pte, _PAGE_USER)) &&
	       !(pte_get_bits(pte, _PAGE_PROTNONE)));
}

static inline int pte_exec(pte_t pte){
	return((pte_get_bits(pte, _PAGE_USER)) &&
	       !(pte_get_bits(pte, _PAGE_PROTNONE)));
}

static inline int pte_write(pte_t pte)
{
	return((pte_get_bits(pte, _PAGE_RW)) &&
	       !(pte_get_bits(pte, _PAGE_PROTNONE)));
}

static inline int pte_file(pte_t pte)
{
	return pte_get_bits(pte, _PAGE_FILE);
}

static inline int pte_dirty(pte_t pte)
{
	return pte_get_bits(pte, _PAGE_DIRTY);
}

static inline int pte_young(pte_t pte)
{
	return pte_get_bits(pte, _PAGE_ACCESSED);
}

static inline int pte_newpage(pte_t pte)
{
	return pte_get_bits(pte, _PAGE_NEWPAGE);
}

static inline int pte_newprot(pte_t pte)
{ 
	return(pte_present(pte) && (pte_get_bits(pte, _PAGE_NEWPROT)));
}

static inline int pte_special(pte_t pte)
{
	return 0;
}


static inline pte_t pte_mknewprot(pte_t pte)
{
	pte_set_bits(pte, _PAGE_NEWPROT);
	return(pte);
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_clear_bits(pte, _PAGE_DIRTY);
	return(pte);
}

static inline pte_t pte_mkold(pte_t pte)	
{ 
	pte_clear_bits(pte, _PAGE_ACCESSED);
	return(pte);
}

static inline pte_t pte_wrprotect(pte_t pte)
{ 
	pte_clear_bits(pte, _PAGE_RW);
	return(pte_mknewprot(pte)); 
}

static inline pte_t pte_mkread(pte_t pte)
{ 
	pte_set_bits(pte, _PAGE_USER);
	return(pte_mknewprot(pte)); 
}

static inline pte_t pte_mkdirty(pte_t pte)
{ 
	pte_set_bits(pte, _PAGE_DIRTY);
	return(pte);
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_set_bits(pte, _PAGE_ACCESSED);
	return(pte);
}

static inline pte_t pte_mkwrite(pte_t pte)	
{
	pte_set_bits(pte, _PAGE_RW);
	return(pte_mknewprot(pte)); 
}

static inline pte_t pte_mkuptodate(pte_t pte)	
{
	pte_clear_bits(pte, _PAGE_NEWPAGE);
	if(pte_present(pte))
		pte_clear_bits(pte, _PAGE_NEWPROT);
	return(pte); 
}

static inline pte_t pte_mknewpage(pte_t pte)
{
	pte_set_bits(pte, _PAGE_NEWPAGE);
	return(pte);
}

static inline pte_t pte_mkspecial(pte_t pte)
{
	return(pte);
}

static inline void set_pte(pte_t *pteptr, pte_t pteval)
{
	pte_copy(*pteptr, pteval);


	*pteptr = pte_mknewpage(*pteptr);
	if(pte_present(*pteptr)) *pteptr = pte_mknewprot(*pteptr);
}
#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)


#define phys_to_page(phys) pfn_to_page(phys_to_pfn(phys))
#define __virt_to_page(virt) phys_to_page(__pa(virt))
#define page_to_phys(page) pfn_to_phys((pfn_t) page_to_pfn(page))
#define virt_to_page(addr) __virt_to_page((const unsigned long) addr)

#define mk_pte(page, pgprot) \
	({ pte_t pte;					\
							\
	pte_set_val(pte, page_to_phys(page), (pgprot));	\
	if (pte_present(pte))				\
		pte_mknewprot(pte_mknewpage(pte));	\
	pte;})

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte_set_val(pte, (pte_val(pte) & _PAGE_CHG_MASK), newprot);
	return pte; 
}

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

#define pmd_page_vaddr(pmd) ((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))
#define pmd_index(address) (((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

#define pmd_page_vaddr(pmd) \
	((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#define pte_index(address) (((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir, address) \
	((pte_t *) pmd_page_vaddr(*(dir)) +  pte_index(address))
#define pte_offset_map(dir, address) \
	((pte_t *)page_address(pmd_page(*(dir))) + pte_index(address))
#define pte_unmap(pte) do { } while (0)

struct mm_struct;
extern pte_t *virt_to_pte(struct mm_struct *mm, unsigned long addr);

#define update_mmu_cache(vma,address,ptep) do ; while (0)

#define __swp_type(x)			(((x).val >> 4) & 0x3f)
#define __swp_offset(x)			((x).val >> 11)

#define __swp_entry(type, offset) \
	((swp_entry_t) { ((type) << 4) | ((offset) << 11) })
#define __pte_to_swp_entry(pte) \
	((swp_entry_t) { pte_val(pte_mkuptodate(pte)) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })

#define kern_addr_valid(addr) (1)

#include <asm-generic/pgtable.h>

#define kpte_clear_flush(ptep, vaddr)		\
do {						\
	pte_clear(&init_mm, (vaddr), (ptep));	\
	__flush_tlb_one((vaddr));		\
} while (0)

#endif
