#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/fixmap.h>

#include <asm/cache.h>

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = (pgd_t *)__get_free_pages(GFP_KERNEL,
					       PGD_ALLOC_ORDER);
	pgd_t *actual_pgd = pgd;

	if (likely(pgd != NULL)) {
		memset(pgd, 0, PAGE_SIZE<<PGD_ALLOC_ORDER);
#ifdef CONFIG_64BIT
		actual_pgd += PTRS_PER_PGD;
		__pgd_val_set(*actual_pgd, (PxD_FLAG_PRESENT | 
				        PxD_FLAG_VALID | 
					PxD_FLAG_ATTACHED) 
			+ (__u32)(__pa((unsigned long)pgd) >> PxD_VALUE_SHIFT));
		__pgd_val_set(*pgd, PxD_FLAG_ATTACHED);
#endif
	}
	return actual_pgd;
}

static inline void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
#ifdef CONFIG_64BIT
	pgd -= PTRS_PER_PGD;
#endif
	free_pages((unsigned long)pgd, PGD_ALLOC_ORDER);
}

#if PT_NLEVELS == 3


static inline void pgd_populate(struct mm_struct *mm, pgd_t *pgd, pmd_t *pmd)
{
	__pgd_val_set(*pgd, (PxD_FLAG_PRESENT | PxD_FLAG_VALID) +
		        (__u32)(__pa((unsigned long)pmd) >> PxD_VALUE_SHIFT));
}

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pmd_t *pmd = (pmd_t *)__get_free_pages(GFP_KERNEL|__GFP_REPEAT,
					       PMD_ORDER);
	if (pmd)
		memset(pmd, 0, PAGE_SIZE<<PMD_ORDER);
	return pmd;
}

static inline void pmd_free(struct mm_struct *mm, pmd_t *pmd)
{
#ifdef CONFIG_64BIT
	if(pmd_flag(*pmd) & PxD_FLAG_ATTACHED)
		return;
#endif
	free_pages((unsigned long)pmd, PMD_ORDER);
}

#else



#define pmd_alloc_one(mm, addr)		({ BUG(); ((pmd_t *)2); })
#define pmd_free(mm, x)			do { } while (0)
#define pgd_populate(mm, pmd, pte)	BUG()

#endif

static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
#ifdef CONFIG_64BIT
	if(pmd_flag(*pmd) & PxD_FLAG_ATTACHED)
		__pmd_val_set(*pmd, (PxD_FLAG_PRESENT |
				 PxD_FLAG_VALID |
				 PxD_FLAG_ATTACHED) 
			+ (__u32)(__pa((unsigned long)pte) >> PxD_VALUE_SHIFT));
	else
#endif
		__pmd_val_set(*pmd, (PxD_FLAG_PRESENT | PxD_FLAG_VALID) 
			+ (__u32)(__pa((unsigned long)pte) >> PxD_VALUE_SHIFT));
}

#define pmd_populate(mm, pmd, pte_page) \
	pmd_populate_kernel(mm, pmd, page_address(pte_page))
#define pmd_pgtable(pmd) pmd_page(pmd)

static inline pgtable_t
pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *page = alloc_page(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO);
	if (page)
		pgtable_page_ctor(page);
	return page;
}

static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long addr)
{
	pte_t *pte = (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO);
	return pte;
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	free_page((unsigned long)pte);
}

static inline void pte_free(struct mm_struct *mm, struct page *pte)
{
	pgtable_page_dtor(pte);
	pte_free_kernel(mm, page_address(pte));
}

#define check_pgt_cache()	do { } while (0)

#endif
