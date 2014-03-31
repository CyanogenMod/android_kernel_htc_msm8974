
#ifndef __S390_MMU_CONTEXT_H
#define __S390_MMU_CONTEXT_H

#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/ctl_reg.h>
#include <asm-generic/mm_hooks.h>

static inline int init_new_context(struct task_struct *tsk,
				   struct mm_struct *mm)
{
	atomic_set(&mm->context.attach_count, 0);
	mm->context.flush_mm = 0;
	mm->context.asce_bits = _ASCE_TABLE_LENGTH | _ASCE_USER_BITS;
#ifdef CONFIG_64BIT
	mm->context.asce_bits |= _ASCE_TYPE_REGION3;
#endif
	if (current->mm && current->mm->context.alloc_pgste) {
		mm->context.has_pgste = 1;
		mm->context.alloc_pgste = 1;
	} else {
		mm->context.has_pgste = 0;
		mm->context.alloc_pgste = 0;
	}
	mm->context.asce_limit = STACK_TOP_MAX;
	crst_table_init((unsigned long *) mm->pgd, pgd_entry_type(mm));
	return 0;
}

#define destroy_context(mm)             do { } while (0)

#ifndef __s390x__
#define LCTL_OPCODE "lctl"
#else
#define LCTL_OPCODE "lctlg"
#endif

static inline void update_mm(struct mm_struct *mm, struct task_struct *tsk)
{
	pgd_t *pgd = mm->pgd;

	S390_lowcore.user_asce = mm->context.asce_bits | __pa(pgd);
	if (user_mode != HOME_SPACE_MODE) {
		
		asm volatile(LCTL_OPCODE" 1,1,%0\n"
			     : : "m" (S390_lowcore.user_asce) );
	} else
		
		asm volatile(LCTL_OPCODE" 13,13,%0"
			     : : "m" (S390_lowcore.user_asce) );
	set_fs(current->thread.mm_segment);
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	cpumask_set_cpu(smp_processor_id(), mm_cpumask(next));
	update_mm(next, tsk);
	atomic_dec(&prev->context.attach_count);
	WARN_ON(atomic_read(&prev->context.attach_count) < 0);
	atomic_inc(&next->context.attach_count);
	
	if (next->context.flush_mm)
		__tlb_flush_mm(next);
}

#define enter_lazy_tlb(mm,tsk)	do { } while (0)
#define deactivate_mm(tsk,mm)	do { } while (0)

static inline void activate_mm(struct mm_struct *prev,
                               struct mm_struct *next)
{
        switch_mm(prev, next, current);
}

#endif 
