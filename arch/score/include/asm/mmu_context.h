#ifndef _ASM_SCORE_MMU_CONTEXT_H
#define _ASM_SCORE_MMU_CONTEXT_H

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm-generic/mm_hooks.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/scoreregs.h>

extern unsigned long asid_cache;
extern unsigned long pgd_current;

#define TLBMISS_HANDLER_SETUP_PGD(pgd) (pgd_current = (unsigned long)(pgd))

#define TLBMISS_HANDLER_SETUP()				\
do {							\
	write_c0_context(0);				\
	TLBMISS_HANDLER_SETUP_PGD(swapper_pg_dir)	\
} while (0)

#define ASID_VERSION_MASK	0xfffff000
#define ASID_FIRST_VERSION	0x1000

#define ASID_INC	0x10
#define ASID_MASK	0xff0

static inline void enter_lazy_tlb(struct mm_struct *mm,
				struct task_struct *tsk)
{}

static inline void
get_new_mmu_context(struct mm_struct *mm)
{
	unsigned long asid = asid_cache + ASID_INC;

	if (!(asid & ASID_MASK)) {
		local_flush_tlb_all();		
		if (!asid)			
			asid = ASID_FIRST_VERSION;
	}

	mm->context = asid;
	asid_cache = asid;
}

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context = 0;
	return 0;
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			struct task_struct *tsk)
{
	unsigned long flags;

	local_irq_save(flags);
	if ((next->context ^ asid_cache) & ASID_VERSION_MASK)
		get_new_mmu_context(next);

	pevn_set(next->context);
	TLBMISS_HANDLER_SETUP_PGD(next->pgd);
	local_irq_restore(flags);
}

static inline void destroy_context(struct mm_struct *mm)
{}

static inline void
deactivate_mm(struct task_struct *task, struct mm_struct *mm)
{}

static inline void
activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned long flags;

	local_irq_save(flags);
	get_new_mmu_context(next);
	pevn_set(next->context);
	TLBMISS_HANDLER_SETUP_PGD(next->pgd);
	local_irq_restore(flags);
}

#endif 
