/*
 * Low level TLB handling.
 *
 * Copyright (C) 2000-2003, Axis Communications AB.
 *
 * Authors:   Bjorn Wesen <bjornw@axis.com>
 *            Tobias Anderberg <tobiasa@axis.com>, CRISv32 port.
 */

#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <arch/hwregs/asm/mmu_defs_asm.h>
#include <arch/hwregs/supp_reg.h>

#define UPDATE_TLB_SEL_IDX(val)					\
do {								\
	unsigned long tlb_sel;					\
								\
	tlb_sel = REG_FIELD(mmu, rw_mm_tlb_sel, idx, val);	\
	SUPP_REG_WR(RW_MM_TLB_SEL, tlb_sel);			\
} while(0)

#define UPDATE_TLB_HILO(tlb_hi, tlb_lo)		\
do {						\
	SUPP_REG_WR(RW_MM_TLB_HI, tlb_hi);	\
	SUPP_REG_WR(RW_MM_TLB_LO, tlb_lo);	\
} while(0)


void
__flush_tlb_all(void)
{
	int i;
	int mmu;
	unsigned long flags;
	unsigned long mmu_tlb_hi;
	unsigned long mmu_tlb_sel;

	/*
	 * Mask with 0xf so similar TLB entries aren't written in the same 4-way
	 * entry group.
	 */
	local_irq_save(flags);

	for (mmu = 1; mmu <= 2; mmu++) {
		SUPP_BANK_SEL(mmu); 
		for (i = 0; i < NUM_TLB_ENTRIES; i++) {
			
			mmu_tlb_sel = REG_FIELD(mmu, rw_mm_tlb_sel, idx, i);

			mmu_tlb_hi = (REG_FIELD(mmu, rw_mm_tlb_hi, pid, INVALID_PAGEID)
				    | REG_FIELD(mmu, rw_mm_tlb_hi, vpn, i & 0xf));

			SUPP_REG_WR(RW_MM_TLB_SEL, mmu_tlb_sel);
			SUPP_REG_WR(RW_MM_TLB_HI, mmu_tlb_hi);
			SUPP_REG_WR(RW_MM_TLB_LO, 0);
		}
	}

	local_irq_restore(flags);
}

void
__flush_tlb_mm(struct mm_struct *mm)
{
	int i;
	int mmu;
	unsigned long flags;
	unsigned long page_id;
	unsigned long tlb_hi;
	unsigned long mmu_tlb_hi;

	page_id = mm->context.page_id;

	if (page_id == NO_CONTEXT)
		return;

	
	local_irq_save(flags);

	for (mmu = 1; mmu <= 2; mmu++) {
		SUPP_BANK_SEL(mmu);
		for (i = 0; i < NUM_TLB_ENTRIES; i++) {
			UPDATE_TLB_SEL_IDX(i);

			
			SUPP_REG_RD(RW_MM_TLB_HI, tlb_hi);

			
			if ((tlb_hi & 0xff) == page_id) {
				mmu_tlb_hi = (REG_FIELD(mmu, rw_mm_tlb_hi, pid,
				                        INVALID_PAGEID)
				            | REG_FIELD(mmu, rw_mm_tlb_hi, vpn,
				                        i & 0xf));

				UPDATE_TLB_HILO(mmu_tlb_hi, 0);
			}
		}
	}

	local_irq_restore(flags);
}

void
__flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	int i;
	int mmu;
	unsigned long page_id;
	unsigned long flags;
	unsigned long tlb_hi;
	unsigned long mmu_tlb_hi;

	page_id = vma->vm_mm->context.page_id;

	if (page_id == NO_CONTEXT)
		return;

	addr &= PAGE_MASK;

	local_irq_save(flags);

	for (mmu = 1; mmu <= 2; mmu++) {
		SUPP_BANK_SEL(mmu);
		for (i = 0; i < NUM_TLB_ENTRIES; i++) {
			UPDATE_TLB_SEL_IDX(i);
			SUPP_REG_RD(RW_MM_TLB_HI, tlb_hi);

			
			if (((tlb_hi & 0xff) == page_id) &&
			    ((tlb_hi & PAGE_MASK) == addr)) {
				mmu_tlb_hi = REG_FIELD(mmu, rw_mm_tlb_hi, pid,
				                       INVALID_PAGEID) | addr;

				UPDATE_TLB_HILO(mmu_tlb_hi, 0);
			}
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

static DEFINE_SPINLOCK(mmu_context_lock);

void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk)
{
	if (prev != next) {
		int cpu = smp_processor_id();

		
		spin_lock(&mmu_context_lock);
		get_mmu_context(next);
		cpumask_set_cpu(cpu, mm_cpumask(next));
		spin_unlock(&mmu_context_lock);

		per_cpu(current_pgd, cpu) = next->pgd;

		
		if (tsk && task_thread_info(tsk)) {
			SPEC_REG_WR(SPEC_REG_PID, next->context.page_id |
				task_thread_info(tsk)->tls);
		} else {
			SPEC_REG_WR(SPEC_REG_PID, next->context.page_id);
		}
	}
}

