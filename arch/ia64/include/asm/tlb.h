#ifndef _ASM_IA64_TLB_H
#define _ASM_IA64_TLB_H
/*
 * Based on <asm-generic/tlb.h>.
 *
 * Copyright (C) 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>

#include <asm/pgalloc.h>
#include <asm/processor.h>
#include <asm/tlbflush.h>
#include <asm/machvec.h>

#ifdef CONFIG_SMP
# define tlb_fast_mode(tlb)	((tlb)->nr == ~0U)
#else
# define tlb_fast_mode(tlb)	(1)
#endif

#define	IA64_GATHER_BUNDLE	8

struct mmu_gather {
	struct mm_struct	*mm;
	unsigned int		nr;		
	unsigned int		max;
	unsigned char		fullmm;		
	unsigned char		need_flush;	
	unsigned long		start_addr;
	unsigned long		end_addr;
	struct page		**pages;
	struct page		*local[IA64_GATHER_BUNDLE];
};

struct ia64_tr_entry {
	u64 ifa;
	u64 itir;
	u64 pte;
	u64 rr;
}; 

extern int ia64_itr_entry(u64 target_mask, u64 va, u64 pte, u64 log_size);
extern void ia64_ptr_entry(u64 target_mask, int slot);

extern struct ia64_tr_entry *ia64_idtrs[NR_CPUS];

#define RR_TO_VE(val)   (((val) >> 0) & 0x0000000000000001)
#define RR_VE(val)	(((val) & 0x0000000000000001) << 0)
#define RR_VE_MASK	0x0000000000000001L
#define RR_VE_SHIFT	0
#define RR_TO_PS(val)	(((val) >> 2) & 0x000000000000003f)
#define RR_PS(val)	(((val) & 0x000000000000003f) << 2)
#define RR_PS_MASK	0x00000000000000fcL
#define RR_PS_SHIFT	2
#define RR_RID_MASK	0x00000000ffffff00L
#define RR_TO_RID(val) 	((val >> 8) & 0xffffff)

static inline void
ia64_tlb_flush_mmu (struct mmu_gather *tlb, unsigned long start, unsigned long end)
{
	unsigned int nr;

	if (!tlb->need_flush)
		return;
	tlb->need_flush = 0;

	if (tlb->fullmm) {
		flush_tlb_mm(tlb->mm);
	} else if (unlikely (end - start >= 1024*1024*1024*1024UL
			     || REGION_NUMBER(start) != REGION_NUMBER(end - 1)))
	{
		flush_tlb_all();
	} else {
		struct vm_area_struct vma;

		vma.vm_mm = tlb->mm;
		
		flush_tlb_range(&vma, start, end);
		
		flush_tlb_range(&vma, ia64_thash(start), ia64_thash(end));
	}

	
	nr = tlb->nr;
	if (!tlb_fast_mode(tlb)) {
		unsigned long i;
		tlb->nr = 0;
		tlb->start_addr = ~0UL;
		for (i = 0; i < nr; ++i)
			free_page_and_swap_cache(tlb->pages[i]);
	}
}

static inline void __tlb_alloc_page(struct mmu_gather *tlb)
{
	unsigned long addr = __get_free_pages(GFP_NOWAIT | __GFP_NOWARN, 0);

	if (addr) {
		tlb->pages = (void *)addr;
		tlb->max = PAGE_SIZE / sizeof(void *);
	}
}


static inline void
tlb_gather_mmu(struct mmu_gather *tlb, struct mm_struct *mm, unsigned int full_mm_flush)
{
	tlb->mm = mm;
	tlb->max = ARRAY_SIZE(tlb->local);
	tlb->pages = tlb->local;
	tlb->nr = (num_online_cpus() == 1) ? ~0U : 0;
	tlb->fullmm = full_mm_flush;
	tlb->start_addr = ~0UL;
}

static inline void
tlb_finish_mmu(struct mmu_gather *tlb, unsigned long start, unsigned long end)
{
	ia64_tlb_flush_mmu(tlb, start, end);

	
	check_pgt_cache();

	if (tlb->pages != tlb->local)
		free_pages((unsigned long)tlb->pages, 0);
}

static inline int __tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	tlb->need_flush = 1;

	if (tlb_fast_mode(tlb)) {
		free_page_and_swap_cache(page);
		return 1; 
	}

	if (!tlb->nr && tlb->pages == tlb->local)
		__tlb_alloc_page(tlb);

	tlb->pages[tlb->nr++] = page;
	VM_BUG_ON(tlb->nr > tlb->max);

	return tlb->max - tlb->nr;
}

static inline void tlb_flush_mmu(struct mmu_gather *tlb)
{
	ia64_tlb_flush_mmu(tlb, tlb->start_addr, tlb->end_addr);
}

static inline void tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	if (!__tlb_remove_page(tlb, page))
		tlb_flush_mmu(tlb);
}

static inline void
__tlb_remove_tlb_entry (struct mmu_gather *tlb, pte_t *ptep, unsigned long address)
{
	if (tlb->start_addr == ~0UL)
		tlb->start_addr = address;
	tlb->end_addr = address + PAGE_SIZE;
}

#define tlb_migrate_finish(mm)	platform_tlb_migrate_finish(mm)

#define tlb_start_vma(tlb, vma)			do { } while (0)
#define tlb_end_vma(tlb, vma)			do { } while (0)

#define tlb_remove_tlb_entry(tlb, ptep, addr)		\
do {							\
	tlb->need_flush = 1;				\
	__tlb_remove_tlb_entry(tlb, ptep, addr);	\
} while (0)

#define pte_free_tlb(tlb, ptep, address)		\
do {							\
	tlb->need_flush = 1;				\
	__pte_free_tlb(tlb, ptep, address);		\
} while (0)

#define pmd_free_tlb(tlb, ptep, address)		\
do {							\
	tlb->need_flush = 1;				\
	__pmd_free_tlb(tlb, ptep, address);		\
} while (0)

#define pud_free_tlb(tlb, pudp, address)		\
do {							\
	tlb->need_flush = 1;				\
	__pud_free_tlb(tlb, pudp, address);		\
} while (0)

#endif 
