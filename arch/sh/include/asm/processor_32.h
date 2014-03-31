/*
 * include/asm-sh/processor.h
 *
 * Copyright (C) 1999, 2000  Niibe Yutaka
 * Copyright (C) 2002, 2003  Paul Mundt
 */

#ifndef __ASM_SH_PROCESSOR_32_H
#define __ASM_SH_PROCESSOR_32_H
#ifdef __KERNEL__

#include <linux/compiler.h>
#include <linux/linkage.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/hw_breakpoint.h>

#define current_text_addr() ({ void *pc; __asm__("mova	1f, %0\n.align 2\n1:":"=z" (pc)); pc; })

#define CCN_PVR		0xff000030
#define CCN_CVR		0xff000040
#define CCN_PRR		0xff000044

#define TASK_SIZE	0x7c000000UL

#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP

#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

#define SR_DSP		0x00001000
#define SR_IMASK	0x000000f0
#define SR_FD		0x00008000
#define SR_MD		0x40000000

struct sh_dsp_struct {
	unsigned long dsp_regs[14];
	long status;
};


struct sh_fpu_hard_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	long status; 
};

struct sh_fpu_soft_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	unsigned char lookahead;
	unsigned long entry_pc;
};

union thread_xstate {
	struct sh_fpu_hard_struct hardfpu;
	struct sh_fpu_soft_struct softfpu;
};

struct thread_struct {
	
	unsigned long sp;
	unsigned long pc;

	
	unsigned long flags;

	
	struct perf_event *ptrace_bps[HBP_NUM];

#ifdef CONFIG_SH_DSP
	
	struct sh_dsp_struct dsp_status;
#endif

	
	union thread_xstate *xstate;
};

#define INIT_THREAD  {						\
	.sp = sizeof(init_stack) + (long) &init_stack,		\
	.flags = 0,						\
}

struct task_struct;

extern void start_thread(struct pt_regs *regs, unsigned long new_pc, unsigned long new_sp);

extern void release_thread(struct task_struct *);

void prepare_to_copy(struct task_struct *tsk);

extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define copy_segments(p, mm)	do { } while(0)
#define release_segments(mm)	do { } while(0)


static __inline__ void disable_fpu(void)
{
	unsigned long __dummy;

	
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "or	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (SR_FD));
}

static __inline__ void enable_fpu(void)
{
	unsigned long __dummy;

	
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "and	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (~SR_FD));
}

#define FPSCR_INIT  0x00080000

#define	FPSCR_CAUSE_MASK	0x0001f000	
#define	FPSCR_FLAG_MASK		0x0000007c	

#define thread_saved_pc(tsk)	(tsk->thread.pc)

void show_trace(struct task_struct *tsk, unsigned long *sp,
		struct pt_regs *regs);

#ifdef CONFIG_DUMP_CODE
void show_code(struct pt_regs *regs);
#else
static inline void show_code(struct pt_regs *regs)
{
}
#endif

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  (task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)  (task_pt_regs(tsk)->regs[15])

#if defined(CONFIG_CPU_SH2A) || defined(CONFIG_CPU_SH4)

#define PREFETCH_STRIDE		L1_CACHE_BYTES
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW

static inline void prefetch(const void *x)
{
	__builtin_prefetch(x, 0, 3);
}

static inline void prefetchw(const void *x)
{
	__builtin_prefetch(x, 1, 3);
}
#endif

#endif 
#endif 
