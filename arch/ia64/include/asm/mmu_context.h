#ifndef _ASM_IA64_MMU_CONTEXT_H
#define _ASM_IA64_MMU_CONTEXT_H

/*
 * Copyright (C) 1998-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */


#define IA64_REGION_ID_KERNEL	0 

#define ia64_rid(ctx,addr)	(((ctx) << 3) | (addr >> 61))

# include <asm/page.h>
# ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <asm/processor.h>
#include <asm-generic/mm_hooks.h>

struct ia64_ctx {
	spinlock_t lock;
	unsigned int next;	
	unsigned int limit;     
	unsigned int max_ctx;   
				
	unsigned long *bitmap;  
	unsigned long *flushmap;
};

extern struct ia64_ctx ia64_ctx;
DECLARE_PER_CPU(u8, ia64_need_tlb_flush);

extern void mmu_context_init (void);
extern void wrap_mmu_context (struct mm_struct *mm);

static inline void
enter_lazy_tlb (struct mm_struct *mm, struct task_struct *tsk)
{
}

static inline void
delayed_tlb_flush (void)
{
	extern void local_flush_tlb_all (void);
	unsigned long flags;

	if (unlikely(__ia64_per_cpu_var(ia64_need_tlb_flush))) {
		spin_lock_irqsave(&ia64_ctx.lock, flags);
		if (__ia64_per_cpu_var(ia64_need_tlb_flush)) {
			local_flush_tlb_all();
			__ia64_per_cpu_var(ia64_need_tlb_flush) = 0;
		}
		spin_unlock_irqrestore(&ia64_ctx.lock, flags);
	}
}

static inline nv_mm_context_t
get_mmu_context (struct mm_struct *mm)
{
	unsigned long flags;
	nv_mm_context_t context = mm->context;

	if (likely(context))
		goto out;

	spin_lock_irqsave(&ia64_ctx.lock, flags);
	
	context = mm->context;
	if (context == 0) {
		cpumask_clear(mm_cpumask(mm));
		if (ia64_ctx.next >= ia64_ctx.limit) {
			ia64_ctx.next = find_next_zero_bit(ia64_ctx.bitmap,
					ia64_ctx.max_ctx, ia64_ctx.next);
			ia64_ctx.limit = find_next_bit(ia64_ctx.bitmap,
					ia64_ctx.max_ctx, ia64_ctx.next);
			if (ia64_ctx.next >= ia64_ctx.max_ctx)
				wrap_mmu_context(mm);
		}
		mm->context = context = ia64_ctx.next++;
		__set_bit(context, ia64_ctx.bitmap);
	}
	spin_unlock_irqrestore(&ia64_ctx.lock, flags);
out:
	delayed_tlb_flush();

	return context;
}

static inline int
init_new_context (struct task_struct *p, struct mm_struct *mm)
{
	mm->context = 0;
	return 0;
}

static inline void
destroy_context (struct mm_struct *mm)
{
	
}

static inline void
reload_context (nv_mm_context_t context)
{
	unsigned long rid;
	unsigned long rid_incr = 0;
	unsigned long rr0, rr1, rr2, rr3, rr4, old_rr4;

	old_rr4 = ia64_get_rr(RGN_BASE(RGN_HPAGE));
	rid = context << 3;	
	rid_incr = 1 << 8;

	
	rr0 = (rid << 8) | (PAGE_SHIFT << 2) | 1;
	rr1 = rr0 + 1*rid_incr;
	rr2 = rr0 + 2*rid_incr;
	rr3 = rr0 + 3*rid_incr;
	rr4 = rr0 + 4*rid_incr;
#ifdef  CONFIG_HUGETLB_PAGE
	rr4 = (rr4 & (~(0xfcUL))) | (old_rr4 & 0xfc);

#  if RGN_HPAGE != 4
#    error "reload_context assumes RGN_HPAGE is 4"
#  endif
#endif

	ia64_set_rr0_to_rr4(rr0, rr1, rr2, rr3, rr4);
	ia64_srlz_i();			
}

static inline void
activate_context (struct mm_struct *mm)
{
	nv_mm_context_t context;

	do {
		context = get_mmu_context(mm);
		if (!cpumask_test_cpu(smp_processor_id(), mm_cpumask(mm)))
			cpumask_set_cpu(smp_processor_id(), mm_cpumask(mm));
		reload_context(context);
	} while (unlikely(context != mm->context));
}

#define deactivate_mm(tsk,mm)	do { } while (0)

static inline void
activate_mm (struct mm_struct *prev, struct mm_struct *next)
{
	ia64_set_kr(IA64_KR_PT_BASE, __pa(next->pgd));
	activate_context(next);
}

#define switch_mm(prev_mm,next_mm,next_task)	activate_mm(prev_mm, next_mm)

# endif 
#endif 
