#ifndef __ASM_POWERPC_MMU_CONTEXT_H
#define __ASM_POWERPC_MMU_CONTEXT_H
#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/mmu.h>	
#include <asm/cputable.h>
#include <asm-generic/mm_hooks.h>
#include <asm/cputhreads.h>

extern int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
extern void destroy_context(struct mm_struct *mm);

extern void switch_mmu_context(struct mm_struct *prev, struct mm_struct *next);
extern void switch_stab(struct task_struct *tsk, struct mm_struct *mm);
extern void switch_slb(struct task_struct *tsk, struct mm_struct *mm);
extern void set_context(unsigned long id, pgd_t *pgd);

#ifdef CONFIG_PPC_BOOK3S_64
extern int __init_new_context(void);
extern void __destroy_context(int context_id);
static inline void mmu_context_init(void) { }
#else
extern unsigned long __init_new_context(void);
extern void __destroy_context(unsigned long context_id);
extern void mmu_context_init(void);
#endif

extern void switch_cop(struct mm_struct *next);
extern int use_cop(unsigned long acop, struct mm_struct *mm);
extern void drop_cop(unsigned long acop, struct mm_struct *mm);

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	
	cpumask_set_cpu(smp_processor_id(), mm_cpumask(next));

	
#ifdef CONFIG_PPC32
	tsk->thread.pgdir = next->pgd;
#endif 

	
#ifdef CONFIG_PPC_BOOK3E_64
	get_paca()->pgd = next->pgd;
#endif
	
	if (prev == next)
		return;

#ifdef CONFIG_PPC_ICSWX
	
	if (prev->context.acop || next->context.acop)
		switch_cop(next);
#endif 

#ifdef CONFIG_ALTIVEC
	if (cpu_has_feature(CPU_FTR_ALTIVEC))
		asm volatile ("dssall");
#endif 

#ifdef CONFIG_PPC_STD_MMU_64
	if (mmu_has_feature(MMU_FTR_SLB))
		switch_slb(tsk, next);
	else
		switch_stab(tsk, next);
#else
	
	switch_mmu_context(prev, next);
#endif

}

#define deactivate_mm(tsk,mm)	do { } while (0)

static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned long flags;

	local_irq_save(flags);
	switch_mm(prev, next, current);
	local_irq_restore(flags);
}

static inline void enter_lazy_tlb(struct mm_struct *mm,
				  struct task_struct *tsk)
{
	
#ifdef CONFIG_PPC_BOOK3E_64
	get_paca()->pgd = NULL;
#endif
}

#endif 
#endif 
