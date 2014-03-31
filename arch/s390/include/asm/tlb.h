#ifndef _S390_TLB_H
#define _S390_TLB_H


#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <asm/processor.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

struct mmu_gather {
	struct mm_struct *mm;
	struct mmu_table_batch *batch;
	unsigned int fullmm;
};

struct mmu_table_batch {
	struct rcu_head		rcu;
	unsigned int		nr;
	void			*tables[0];
};

#define MAX_TABLE_BATCH		\
	((PAGE_SIZE - sizeof(struct mmu_table_batch)) / sizeof(void *))

extern void tlb_table_flush(struct mmu_gather *tlb);
extern void tlb_remove_table(struct mmu_gather *tlb, void *table);

static inline void tlb_gather_mmu(struct mmu_gather *tlb,
				  struct mm_struct *mm,
				  unsigned int full_mm_flush)
{
	tlb->mm = mm;
	tlb->fullmm = full_mm_flush;
	tlb->batch = NULL;
	if (tlb->fullmm)
		__tlb_flush_mm(mm);
}

static inline void tlb_flush_mmu(struct mmu_gather *tlb)
{
	tlb_table_flush(tlb);
}

static inline void tlb_finish_mmu(struct mmu_gather *tlb,
				  unsigned long start, unsigned long end)
{
	tlb_table_flush(tlb);
}

static inline int __tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	free_page_and_swap_cache(page);
	return 1; 
}

static inline void tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	free_page_and_swap_cache(page);
}

static inline void pte_free_tlb(struct mmu_gather *tlb, pgtable_t pte,
				unsigned long address)
{
	if (!tlb->fullmm)
		return page_table_free_rcu(tlb, (unsigned long *) pte);
	page_table_free(tlb->mm, (unsigned long *) pte);
}

static inline void pmd_free_tlb(struct mmu_gather *tlb, pmd_t *pmd,
				unsigned long address)
{
#ifdef __s390x__
	if (tlb->mm->context.asce_limit <= (1UL << 31))
		return;
	if (!tlb->fullmm)
		return tlb_remove_table(tlb, pmd);
	crst_table_free(tlb->mm, (unsigned long *) pmd);
#endif
}

static inline void pud_free_tlb(struct mmu_gather *tlb, pud_t *pud,
				unsigned long address)
{
#ifdef __s390x__
	if (tlb->mm->context.asce_limit <= (1UL << 42))
		return;
	if (!tlb->fullmm)
		return tlb_remove_table(tlb, pud);
	crst_table_free(tlb->mm, (unsigned long *) pud);
#endif
}

#define tlb_start_vma(tlb, vma)			do { } while (0)
#define tlb_end_vma(tlb, vma)			do { } while (0)
#define tlb_remove_tlb_entry(tlb, ptep, addr)	do { } while (0)
#define tlb_migrate_finish(mm)			do { } while (0)

#endif 
