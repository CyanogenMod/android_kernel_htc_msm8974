/*
 *  linux/arch/cris/arch-v10/mm/tlb.c
 *
 *  Low level TLB handling
 *
 *
 *  Copyright (C) 2000-2007  Axis Communications AB
 *
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *
 */

#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <arch/svinto.h>

#define D(x)



void
flush_tlb_all(void)
{
	int i;
	unsigned long flags;


	local_irq_save(flags);
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		*R_TLB_SELECT = ( IO_FIELD(R_TLB_SELECT, index, i) );
		*R_TLB_HI = ( IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
			      IO_FIELD(R_TLB_HI, vpn,     i & 0xf ) );

		*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no       ) |
			      IO_STATE(R_TLB_LO, valid, no       ) |
			      IO_STATE(R_TLB_LO, kernel,no	 ) |
			      IO_STATE(R_TLB_LO, we,    no       ) |
			      IO_FIELD(R_TLB_LO, pfn,   0        ) );
	}
	local_irq_restore(flags);
	D(printk("tlb: flushed all\n"));
}


void
flush_tlb_mm(struct mm_struct *mm)
{
	int i;
	int page_id = mm->context.page_id;
	unsigned long flags;

	D(printk("tlb: flush mm context %d (%p)\n", page_id, mm));

	if(page_id == NO_CONTEXT)
		return;


	local_irq_save(flags);
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		*R_TLB_SELECT = IO_FIELD(R_TLB_SELECT, index, i);
		if (IO_EXTRACT(R_TLB_HI, page_id, *R_TLB_HI) == page_id) {
			*R_TLB_HI = ( IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
				      IO_FIELD(R_TLB_HI, vpn,     i & 0xf ) );

			*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no  ) |
				      IO_STATE(R_TLB_LO, valid, no  ) |
				      IO_STATE(R_TLB_LO, kernel,no  ) |
				      IO_STATE(R_TLB_LO, we,    no  ) |
				      IO_FIELD(R_TLB_LO, pfn,   0   ) );
		}
	}
	local_irq_restore(flags);
}


void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	struct mm_struct *mm = vma->vm_mm;
	int page_id = mm->context.page_id;
	int i;
	unsigned long flags;

	D(printk("tlb: flush page %p in context %d (%p)\n", addr, page_id, mm));

	if(page_id == NO_CONTEXT)
		return;

	addr &= PAGE_MASK; 


	local_irq_save(flags);
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		unsigned long tlb_hi;
		*R_TLB_SELECT = IO_FIELD(R_TLB_SELECT, index, i);
		tlb_hi = *R_TLB_HI;
		if (IO_EXTRACT(R_TLB_HI, page_id, tlb_hi) == page_id &&
		    (tlb_hi & PAGE_MASK) == addr) {
			*R_TLB_HI = IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
				addr; 

			*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no  ) |
				      IO_STATE(R_TLB_LO, valid, no  ) |
				      IO_STATE(R_TLB_LO, kernel,no  ) |
				      IO_STATE(R_TLB_LO, we,    no  ) |
				      IO_FIELD(R_TLB_LO, pfn,   0   ) );
		}
	}
	local_irq_restore(flags);
}


int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context.page_id = NO_CONTEXT;
	return 0;
}


void switch_mm(struct mm_struct *prev, struct mm_struct *next,
	struct task_struct *tsk)
{
	if (prev != next) {
		
		get_mmu_context(next);


		per_cpu(current_pgd, smp_processor_id()) = next->pgd;

		

		D(printk(KERN_DEBUG "switching mmu_context to %d (%p)\n",
			next->context, next));

		*R_MMU_CONTEXT = IO_FIELD(R_MMU_CONTEXT,
					  page_id, next->context.page_id);
	}
}

