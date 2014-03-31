/*
 * include/asm-xtensa/pgtable.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (C) 2001 - 2007 Tensilica Inc.
 */

#ifndef _XTENSA_PGTABLE_H
#define _XTENSA_PGTABLE_H

#include <asm-generic/pgtable-nopmd.h>
#include <asm/page.h>


#define USER_RING		1	
#define KERNEL_RING		0	


#define PGDIR_SHIFT	22
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define PTRS_PER_PTE		1024
#define PTRS_PER_PTE_SHIFT	10
#define PTRS_PER_PGD		1024
#define PGD_ORDER		0
#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0
#define FIRST_USER_PGD_NR	(FIRST_USER_ADDRESS >> PGDIR_SHIFT)


#define VMALLOC_START		0xC0000000
#define VMALLOC_END		0xC7FEFFFF
#define TLBTEMP_BASE_1		0xC7FF0000
#define TLBTEMP_BASE_2		0xC7FF8000


#define _PAGE_HW_EXEC		(1<<0)	
#define _PAGE_HW_WRITE		(1<<1)	

#define _PAGE_FILE		(1<<1)	
#define _PAGE_PROTNONE		(3<<0)	

#define _PAGE_CA_BYPASS		(0<<2)	
#define _PAGE_CA_WB		(1<<2)	
#define _PAGE_CA_WT		(2<<2)	
#define _PAGE_CA_MASK		(3<<2)
#define _PAGE_INVALID		(3<<2)

#define _PAGE_USER		(1<<4)	

#define _PAGE_WRITABLE_BIT	6
#define _PAGE_WRITABLE		(1<<6)	
#define _PAGE_DIRTY		(1<<7)	
#define _PAGE_ACCESSED		(1<<8)	

#if XCHAL_HW_VERSION_MAJOR < 2000
# define _PAGE_VALID		(1<<0)
#else
# define _PAGE_VALID		0
#endif

#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_PRESENT	(_PAGE_VALID | _PAGE_CA_WB | _PAGE_ACCESSED)

#ifdef CONFIG_MMU

#define PAGE_NONE	   __pgprot(_PAGE_INVALID | _PAGE_USER | _PAGE_PROTNONE)
#define PAGE_COPY	   __pgprot(_PAGE_PRESENT | _PAGE_USER)
#define PAGE_COPY_EXEC	   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_HW_EXEC)
#define PAGE_READONLY	   __pgprot(_PAGE_PRESENT | _PAGE_USER)
#define PAGE_READONLY_EXEC __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_HW_EXEC)
#define PAGE_SHARED	   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITABLE)
#define PAGE_SHARED_EXEC \
	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITABLE | _PAGE_HW_EXEC)
#define PAGE_KERNEL	   __pgprot(_PAGE_PRESENT | _PAGE_HW_WRITE)
#define PAGE_KERNEL_EXEC   __pgprot(_PAGE_PRESENT|_PAGE_HW_WRITE|_PAGE_HW_EXEC)

#if (DCACHE_WAY_SIZE > PAGE_SIZE)
# define _PAGE_DIRECTORY (_PAGE_VALID | _PAGE_ACCESSED)
#else
# define _PAGE_DIRECTORY (_PAGE_VALID | _PAGE_ACCESSED | _PAGE_CA_WB)
#endif

#else 

# define PAGE_NONE       __pgprot(0)
# define PAGE_SHARED     __pgprot(0)
# define PAGE_COPY       __pgprot(0)
# define PAGE_READONLY   __pgprot(0)
# define PAGE_KERNEL     __pgprot(0)

#endif

#define __P000	PAGE_NONE		
#define __P001	PAGE_READONLY		
#define __P010	PAGE_COPY		
#define __P011	PAGE_COPY		
#define __P100	PAGE_READONLY_EXEC	
#define __P101	PAGE_READONLY_EXEC	
#define __P110	PAGE_COPY_EXEC		
#define __P111	PAGE_COPY_EXEC		

#define __S000	PAGE_NONE		
#define __S001	PAGE_READONLY		
#define __S010	PAGE_SHARED		
#define __S011	PAGE_SHARED		
#define __S100	PAGE_READONLY_EXEC	
#define __S101	PAGE_READONLY_EXEC	
#define __S110	PAGE_SHARED_EXEC	
#define __S111	PAGE_SHARED_EXEC	

#ifndef __ASSEMBLY__

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd entry %08lx.\n", __FILE__, __LINE__, pgd_val(e))

extern unsigned long empty_zero_page[1024];

#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

#ifdef CONFIG_MMU
extern pgd_t swapper_pg_dir[PAGE_SIZE/sizeof(pgd_t)];
extern void paging_init(void);
extern void pgtable_cache_init(void);
#else
# define swapper_pg_dir NULL
static inline void paging_init(void) { }
static inline void pgtable_cache_init(void) { }
#endif

#define pmd_page_vaddr(pmd) ((unsigned long)(pmd_val(pmd) & PAGE_MASK))
#define pmd_page(pmd) virt_to_page(pmd_val(pmd))

#define pte_none(pte)	 (pte_val(pte) == _PAGE_INVALID)
#define pte_present(pte)						\
	(((pte_val(pte) & _PAGE_CA_MASK) != _PAGE_INVALID)		\
	 || ((pte_val(pte) & _PAGE_PROTNONE) == _PAGE_PROTNONE))
#define pte_clear(mm,addr,ptep)						\
	do { update_pte(ptep, __pte(_PAGE_INVALID)); } while(0)

#define pmd_none(pmd)	 (!pmd_val(pmd))
#define pmd_present(pmd) (pmd_val(pmd) & PAGE_MASK)
#define pmd_bad(pmd)	 (pmd_val(pmd) & ~PAGE_MASK)
#define pmd_clear(pmdp)	 do { set_pmd(pmdp, __pmd(0)); } while (0)

static inline int pte_write(pte_t pte) { return pte_val(pte) & _PAGE_WRITABLE; }
static inline int pte_dirty(pte_t pte) { return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte) { return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_file(pte_t pte)  { return pte_val(pte) & _PAGE_FILE; }
static inline int pte_special(pte_t pte) { return 0; }

static inline pte_t pte_wrprotect(pte_t pte)	
	{ pte_val(pte) &= ~(_PAGE_WRITABLE | _PAGE_HW_WRITE); return pte; }
static inline pte_t pte_mkclean(pte_t pte)
	{ pte_val(pte) &= ~(_PAGE_DIRTY | _PAGE_HW_WRITE); return pte; }
static inline pte_t pte_mkold(pte_t pte)
	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)
	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)
	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkwrite(pte_t pte)
	{ pte_val(pte) |= _PAGE_WRITABLE; return pte; }
static inline pte_t pte_mkspecial(pte_t pte)
	{ return pte; }


#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)
#define pte_same(a,b)		(pte_val(a) == pte_val(b))
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define mk_pte(page, prot)	pfn_pte(page_to_pfn(page), prot)

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

static inline void update_pte(pte_t *ptep, pte_t pteval)
{
	*ptep = pteval;
#if (DCACHE_WAY_SIZE > PAGE_SIZE) && XCHAL_DCACHE_IS_WRITEBACK
	__asm__ __volatile__ ("dhwb %0, 0" :: "a" (ptep));
#endif

}

struct mm_struct;

static inline void
set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pteval)
{
	update_pte(ptep, pteval);
}


static inline void
set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	*pmdp = pmdval;
}

struct vm_area_struct;

static inline int
ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr,
    			  pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	update_pte(ptep, pte_mkold(pte));
	return 1;
}

static inline pte_t
ptep_get_and_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(mm, addr, ptep);
	return pte;
}

static inline void
ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
  	pte_t pte = *ptep;
  	update_pte(ptep, pte_wrprotect(pte));
}

#define pgd_offset_k(address)	pgd_offset(&init_mm, address)

#define pgd_offset(mm,address)	((mm)->pgd + pgd_index(address))

#define pgd_index(address)	((address) >> PGDIR_SHIFT)

#define pmd_offset(dir,address) ((pmd_t*)(dir))

#define pte_index(address)	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir,addr) 					\
	((pte_t*) pmd_page_vaddr(*(dir)) + pte_index(addr))
#define pte_offset_map(dir,addr)	pte_offset_kernel((dir),(addr))
#define pte_unmap(pte)		do { } while (0)



#define __swp_type(entry)	(((entry).val >> 6) & 0x1f)
#define __swp_offset(entry)	((entry).val >> 11)
#define __swp_entry(type,offs)	\
	((swp_entry_t) {((type) << 6) | ((offs) << 11) | _PAGE_INVALID})
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

#define PTE_FILE_MAX_BITS	28
#define pte_to_pgoff(pte)	(pte_val(pte) >> 4)
#define pgoff_to_pte(off)	\
	((pte_t) { ((off) << 4) | _PAGE_INVALID | _PAGE_FILE })

#endif 


#ifdef __ASSEMBLY__

#define _PGD_INDEX(rt,rs)	extui	rt, rs, PGDIR_SHIFT, 32-PGDIR_SHIFT
#define _PTE_INDEX(rt,rs)	extui	rt, rs, PAGE_SHIFT, PTRS_PER_PTE_SHIFT

#define _PGD_OFFSET(mm,adr,tmp)		l32i	mm, mm, MM_PGD;		\
					_PGD_INDEX(tmp, adr);		\
					addx4	mm, tmp, mm

#define _PTE_OFFSET(pmd,adr,tmp)	_PTE_INDEX(tmp, adr);		\
					srli	pmd, pmd, PAGE_SHIFT;	\
					slli	pmd, pmd, PAGE_SHIFT;	\
					addx4	pmd, tmp, pmd

#else

#define kern_addr_valid(addr)	(1)

extern  void update_mmu_cache(struct vm_area_struct * vma,
			      unsigned long address, pte_t *ptep);


#define io_remap_pfn_range(vma,from,pfn,size,prot) \
                remap_pfn_range(vma, from, pfn, size, prot)

typedef pte_t *pte_addr_t;

#endif 

#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTEP_MKDIRTY
#define __HAVE_ARCH_PTE_SAME

#include <asm-generic/pgtable.h>

#endif 
