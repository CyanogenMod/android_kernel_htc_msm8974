
#ifndef _CRIS_PGTABLE_H
#define _CRIS_PGTABLE_H

#include <asm/page.h>
#include <asm-generic/pgtable-nopmd.h>

#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <asm/mmu.h>
#endif
#include <arch/pgtable.h>

#ifndef __ASSEMBLY__
extern void paging_init(void);
#endif

#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)

#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)
#define set_pgu(pudptr, pudval) (*(pudptr) = pudval)


#define PGDIR_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-2))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define PTRS_PER_PTE	(1UL << (PAGE_SHIFT-2))
#define PTRS_PER_PGD	(1UL << (PAGE_SHIFT-2))


#define USER_PTRS_PER_PGD       (TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_ADDRESS      0

#ifndef __ASSEMBLY__
extern unsigned long empty_zero_page;
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))
#endif

#define BITS_PER_PTR			(8*sizeof(unsigned long))

#define PTR_MASK			(~(sizeof(void*)-1))

#define SIZEOF_PTR_LOG2			2

#define PAGE_PTR(address) \
((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)

#define SET_PAGE_DIR(tsk,pgdir)

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(mm,addr,xp)	do { pte_val(*(xp)) = 0; } while (0)

#define pmd_none(x)     (!pmd_val(x))
#define	pmd_bad(x)	((pmd_val(x) & (~PAGE_MASK & ~_PAGE_KERNEL)) != _PAGE_TABLE)
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)

#ifndef __ASSEMBLY__


static inline int pte_write(pte_t pte)          { return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_dirty(pte_t pte)          { return pte_val(pte) & _PAGE_MODIFIED; }
static inline int pte_young(pte_t pte)          { return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_file(pte_t pte)           { return pte_val(pte) & _PAGE_FILE; }
static inline int pte_special(pte_t pte)	{ return 0; }

static inline pte_t pte_wrprotect(pte_t pte)
{
        pte_val(pte) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
        return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_MODIFIED | _PAGE_SILENT_WRITE); 
	return pte; 
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED | _PAGE_SILENT_READ);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
        pte_val(pte) |= _PAGE_WRITE;
        if (pte_val(pte) & _PAGE_MODIFIED)
                pte_val(pte) |= _PAGE_SILENT_WRITE;
        return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
        pte_val(pte) |= _PAGE_MODIFIED;
        if (pte_val(pte) & _PAGE_WRITE)
                pte_val(pte) |= _PAGE_SILENT_WRITE;
        return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
        pte_val(pte) |= _PAGE_ACCESSED;
        if (pte_val(pte) & _PAGE_READ)
        {
                pte_val(pte) |= _PAGE_SILENT_READ;
                if ((pte_val(pte) & (_PAGE_WRITE | _PAGE_MODIFIED)) ==
		    (_PAGE_WRITE | _PAGE_MODIFIED))
                        pte_val(pte) |= _PAGE_SILENT_WRITE;
        }
        return pte;
}
static inline pte_t pte_mkspecial(pte_t pte)	{ return pte; }



static inline pte_t __mk_pte(void * page, pgprot_t pgprot)
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
{ pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot); return pte; }

#define pgprot_noncached(prot) __pgprot((pgprot_val(prot) | _PAGE_NO_CACHE))



static inline unsigned long __pte_page(pte_t pte)
{
	
	return (unsigned long)__va(pte_val(pte) & PAGE_MASK);
}

#define pte_pagenr(pte)         ((__pte_page(pte) - PAGE_OFFSET) >> PAGE_SHIFT)


#define __page_address(page)    (PAGE_OFFSET + (((page) - mem_map) << PAGE_SHIFT))
#define pte_page(pte)           (mem_map+pte_pagenr(pte))


static inline void pmd_set(pmd_t * pmdp, pte_t * ptep)
{ pmd_val(*pmdp) = _PAGE_TABLE | (unsigned long) ptep; }

#define pmd_page(pmd)		(pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT))
#define pmd_page_vaddr(pmd)	((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

static inline pgd_t * pgd_offset(const struct mm_struct *mm, unsigned long address)
{
	return mm->pgd + pgd_index(address);
}

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

#define __pte_offset(address) \
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset_kernel(dir, address) \
	((pte_t *) pmd_page_vaddr(*(dir)) +  __pte_offset(address))
#define pte_offset_map(dir, address) \
	((pte_t *)page_address(pmd_page(*(dir))) + __pte_offset(address))

#define pte_unmap(pte) do { } while (0)
#define pte_pfn(x)		((unsigned long)(__va((x).pte)) >> PAGE_SHIFT)
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))

#define pte_ERROR(e) \
        printk("%s:%d: bad pte %p(%08lx).\n", __FILE__, __LINE__, &(e), pte_val(e))
#define pgd_ERROR(e) \
        printk("%s:%d: bad pgd %p(%08lx).\n", __FILE__, __LINE__, &(e), pgd_val(e))

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)         \
		remap_pfn_range(vma, vaddr, pfn, size, prot)


extern pgd_t swapper_pg_dir[PTRS_PER_PGD]; 

static inline void update_mmu_cache(struct vm_area_struct * vma,
	unsigned long address, pte_t *ptep)
{
}


#define __swp_type(x)			(((x).val >> 5) & 0x7f)
#define __swp_offset(x)			((x).val >> 12)
#define __swp_entry(type, offset)	((swp_entry_t) { ((type) << 5) | ((offset) << 12) })
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })

#define kern_addr_valid(addr)   (1)

#include <asm-generic/pgtable.h>

#define pgtable_cache_init()   do { } while (0)

#define pte_to_pgoff(x)	(pte_val(x) >> 6)
#define pgoff_to_pte(x)	__pte(((x) << 6) | _PAGE_FILE)

typedef pte_t *pte_addr_t;

#endif 
#endif 
