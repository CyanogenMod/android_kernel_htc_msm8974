#ifndef _CRIS_PAGE_H
#define _CRIS_PAGE_H

#include <arch/page.h>
#include <linux/const.h>

#define PAGE_SHIFT	13
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#define clear_page(page)        memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)      memcpy((void *)(to), (void *)(from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)    clear_page(page)
#define copy_user_page(to, from, vaddr, pg) copy_page(to, from)

#define __alloc_zeroed_user_highpage(movableflags, vma, vaddr) \
	alloc_page_vma(GFP_HIGHUSER | __GFP_ZERO | movableflags, vma, vaddr)
#define __HAVE_ARCH_ALLOC_ZEROED_USER_HIGHPAGE

#ifndef __ASSEMBLY__
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct page *pgtable_t;
#endif

#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#define ARCH_PFN_OFFSET		(PAGE_OFFSET >> PAGE_SHIFT)
#define pfn_valid(pfn)		(((pfn) - (PAGE_OFFSET >> PAGE_SHIFT)) < max_mapnr)

 

#define virt_to_page(kaddr)    (mem_map + (((unsigned long)(kaddr) - PAGE_OFFSET) >> PAGE_SHIFT))
#define VALID_PAGE(page)       (((page) - mem_map) < max_mapnr)
#define virt_addr_valid(kaddr)	pfn_valid((unsigned)(kaddr) >> PAGE_SHIFT)


#define page_to_phys(page)     __pa((((page) - mem_map) << PAGE_SHIFT) + PAGE_OFFSET)

#ifndef __ASSEMBLY__

#endif 

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif 

