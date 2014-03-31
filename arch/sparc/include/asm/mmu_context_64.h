#ifndef __SPARC64_MMU_CONTEXT_H
#define __SPARC64_MMU_CONTEXT_H


#ifndef __ASSEMBLY__

#include <linux/spinlock.h>
#include <asm/spitfire.h>
#include <asm-generic/mm_hooks.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

extern spinlock_t ctx_alloc_lock;
extern unsigned long tlb_context_cache;
extern unsigned long mmu_context_bmap[];

extern void get_new_mmu_context(struct mm_struct *mm);
#ifdef CONFIG_SMP
extern void smp_new_mmu_context_version(void);
#else
#define smp_new_mmu_context_version() do { } while (0)
#endif

extern int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
extern void destroy_context(struct mm_struct *mm);

extern void __tsb_context_switch(unsigned long pgd_pa,
				 struct tsb_config *tsb_base,
				 struct tsb_config *tsb_huge,
				 unsigned long tsb_descr_pa);

static inline void tsb_context_switch(struct mm_struct *mm)
{
	__tsb_context_switch(__pa(mm->pgd),
			     &mm->context.tsb_block[0],
#ifdef CONFIG_HUGETLB_PAGE
			     (mm->context.tsb_block[1].tsb ?
			      &mm->context.tsb_block[1] :
			      NULL)
#else
			     NULL
#endif
			     , __pa(&mm->context.tsb_descr[0]));
}

extern void tsb_grow(struct mm_struct *mm, unsigned long tsb_index, unsigned long mm_rss);
#ifdef CONFIG_SMP
extern void smp_tsb_sync(struct mm_struct *mm);
#else
#define smp_tsb_sync(__mm) do { } while (0)
#endif

#define load_secondary_context(__mm) \
	__asm__ __volatile__( \
	"\n661:	stxa		%0, [%1] %2\n" \
	"	.section	.sun4v_1insn_patch, \"ax\"\n" \
	"	.word		661b\n" \
	"	stxa		%0, [%1] %3\n" \
	"	.previous\n" \
	"	flush		%%g6\n" \
	:  \
	: "r" (CTX_HWBITS((__mm)->context)), \
	  "r" (SECONDARY_CONTEXT), "i" (ASI_DMMU), "i" (ASI_MMU))

extern void __flush_tlb_mm(unsigned long, unsigned long);

static inline void switch_mm(struct mm_struct *old_mm, struct mm_struct *mm, struct task_struct *tsk)
{
	unsigned long ctx_valid, flags;
	int cpu;

	if (unlikely(mm == &init_mm))
		return;

	spin_lock_irqsave(&mm->context.lock, flags);
	ctx_valid = CTX_VALID(mm->context);
	if (!ctx_valid)
		get_new_mmu_context(mm);

	load_secondary_context(mm);
	tsb_context_switch(mm);

	cpu = smp_processor_id();
	if (!ctx_valid || !cpumask_test_cpu(cpu, mm_cpumask(mm))) {
		cpumask_set_cpu(cpu, mm_cpumask(mm));
		__flush_tlb_mm(CTX_HWBITS(mm->context),
			       SECONDARY_CONTEXT);
	}
	spin_unlock_irqrestore(&mm->context.lock, flags);
}

#define deactivate_mm(tsk,mm)	do { } while (0)

static inline void activate_mm(struct mm_struct *active_mm, struct mm_struct *mm)
{
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&mm->context.lock, flags);
	if (!CTX_VALID(mm->context))
		get_new_mmu_context(mm);
	cpu = smp_processor_id();
	if (!cpumask_test_cpu(cpu, mm_cpumask(mm)))
		cpumask_set_cpu(cpu, mm_cpumask(mm));

	load_secondary_context(mm);
	__flush_tlb_mm(CTX_HWBITS(mm->context), SECONDARY_CONTEXT);
	tsb_context_switch(mm);
	spin_unlock_irqrestore(&mm->context.lock, flags);
}

#endif 

#endif 
