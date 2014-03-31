#ifndef _ASM_POWERPC_PGALLOC_32_H
#define _ASM_POWERPC_PGALLOC_32_H

#include <linux/threads.h>

#define MAX_PGTABLE_INDEX_SIZE	0

extern void __bad_pte(pmd_t *pmd);

extern pgd_t *pgd_alloc(struct mm_struct *mm);
extern void pgd_free(struct mm_struct *mm, pgd_t *pgd);

#define pmd_free(mm, x) 		do { } while (0)
#define __pmd_free_tlb(tlb,x,a)		do { } while (0)

#ifndef CONFIG_BOOKE
#define pmd_populate_kernel(mm, pmd, pte)	\
		(pmd_val(*(pmd)) = __pa(pte) | _PMD_PRESENT)
#define pmd_populate(mm, pmd, pte)	\
		(pmd_val(*(pmd)) = (page_to_pfn(pte) << PAGE_SHIFT) | _PMD_PRESENT)
#define pmd_pgtable(pmd) pmd_page(pmd)
#else
#define pmd_populate_kernel(mm, pmd, pte)	\
		(pmd_val(*(pmd)) = (unsigned long)pte | _PMD_PRESENT)
#define pmd_populate(mm, pmd, pte)	\
		(pmd_val(*(pmd)) = (unsigned long)lowmem_page_address(pte) | _PMD_PRESENT)
#define pmd_pgtable(pmd) pmd_page(pmd)
#endif

extern pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long addr);
extern pgtable_t pte_alloc_one(struct mm_struct *mm, unsigned long addr);

static inline void pgtable_free(void *table, unsigned index_size)
{
	BUG_ON(index_size); 
	free_page((unsigned long)table);
}

#define check_pgt_cache()	do { } while (0)

#endif 
