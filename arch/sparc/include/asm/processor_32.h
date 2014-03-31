/* include/asm/processor.h
 *
 * Copyright (C) 1994 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __ASM_SPARC_PROCESSOR_H
#define __ASM_SPARC_PROCESSOR_H

#define current_text_addr() ({ void *pc; __asm__("sethi %%hi(1f), %0; or %0, %%lo(1f), %0;\n1:" : "=r" (pc)); pc; })

#include <asm/psr.h>
#include <asm/ptrace.h>
#include <asm/head.h>
#include <asm/signal.h>
#include <asm/btfixup.h>
#include <asm/page.h>

#define wp_works_ok 1
#define wp_works_ok__is_a_macro 

#define TASK_SIZE	PAGE_OFFSET
#ifdef __KERNEL__
#define STACK_TOP	(PAGE_OFFSET - PAGE_SIZE)
#define STACK_TOP_MAX	STACK_TOP
#endif 

struct task_struct;

#ifdef __KERNEL__
struct fpq {
	unsigned long *insn_addr;
	unsigned long insn;
};
#endif

typedef struct {
	int seg;
} mm_segment_t;

struct thread_struct {
	struct pt_regs *kregs;
	unsigned int _pad1;

	
	unsigned long fork_kpsr __attribute__ ((aligned (8)));
	unsigned long fork_kwim;

	
	unsigned long   float_regs[32] __attribute__ ((aligned (8)));
	unsigned long   fsr;
	unsigned long   fpqdepth;
	struct fpq	fpqueue[16];
	unsigned long flags;
	mm_segment_t current_ds;
};

#define SPARC_FLAG_KTHREAD      0x1    
#define SPARC_FLAG_UNALIGNED    0x2    

#define INIT_THREAD  { \
	.flags = SPARC_FLAG_KTHREAD, \
	.current_ds = KERNEL_DS, \
}

extern unsigned long thread_saved_pc(struct task_struct *t);

static inline void start_thread(struct pt_regs * regs, unsigned long pc,
				    unsigned long sp)
{
	register unsigned long zero asm("g1");

	regs->psr = (regs->psr & (PSR_CWP)) | PSR_S;
	regs->pc = ((pc & (~3)) - 4);
	regs->npc = regs->pc + 4;
	regs->y = 0;
	zero = 0;
	__asm__ __volatile__("std\t%%g0, [%0 + %3 + 0x00]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x08]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x10]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x18]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x20]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x28]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x30]\n\t"
			     "st\t%1, [%0 + %3 + 0x38]\n\t"
			     "st\t%%g0, [%0 + %3 + 0x3c]"
			     : 
			     : "r" (regs),
			       "r" (sp - sizeof(struct reg_window32)),
			       "r" (zero),
			       "i" ((const unsigned long)(&((struct pt_regs *)0)->u_regs[0]))
			     : "memory");
}

#define release_thread(tsk)		do { } while(0)
extern pid_t kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define prepare_to_copy(tsk)	do { } while (0)

extern unsigned long get_wchan(struct task_struct *);

#define task_pt_regs(tsk) ((tsk)->thread.kregs)
#define KSTK_EIP(tsk)  ((tsk)->thread.kregs->pc)
#define KSTK_ESP(tsk)  ((tsk)->thread.kregs->u_regs[UREG_FP])

#ifdef __KERNEL__

extern struct task_struct *last_task_used_math;

#define cpu_relax()	barrier()

#endif

#endif 
