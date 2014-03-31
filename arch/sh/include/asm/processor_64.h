#ifndef __ASM_SH_PROCESSOR_64_H
#define __ASM_SH_PROCESSOR_64_H

/*
 * include/asm-sh/processor_64.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 * Copyright (C) 2004  Richard Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <asm/page.h>
#include <asm/types.h>
#include <cpu/registers.h>

#define current_text_addr() ({ \
void *pc; \
unsigned long long __dummy = 0; \
__asm__("gettr	tr0, %1\n\t" \
	"pta	4, tr0\n\t" \
	"gettr	tr0, %0\n\t" \
	"ptabs	%1, tr0\n\t"	\
	:"=r" (pc), "=r" (__dummy) \
	: "1" (__dummy)); \
pc; })

#endif

#define TASK_SIZE	0x7ffff000UL

#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP

#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

#if defined(CONFIG_SH64_SR_WATCH)
#define SR_MMU   0x84000000
#else
#define SR_MMU   0x80000000
#endif

#define SR_IMASK 0x000000f0
#define SR_FD    0x00008000
#define SR_SSTEP 0x08000000

#ifndef __ASSEMBLY__


struct sh_fpu_hard_struct {
	unsigned long fp_regs[64];
	unsigned int fpscr;
	
};

struct sh_fpu_soft_struct {
	unsigned long fp_regs[64];
	unsigned int fpscr;
	unsigned char lookahead;
	unsigned long entry_pc;
};

union thread_xstate {
	struct sh_fpu_hard_struct hardfpu;
	struct sh_fpu_soft_struct softfpu;
	unsigned long long alignment_dummy;
};

struct thread_struct {
	unsigned long sp;
	unsigned long pc;

	
	unsigned long flags;

        struct pt_regs *kregs;
	struct pt_regs *uregs;

	unsigned long trap_no, error_code;
	unsigned long address;
	

	
	union thread_xstate *xstate;
};

#define INIT_MMAP \
{ &init_mm, 0, 0, NULL, PAGE_SHARED, VM_READ | VM_WRITE | VM_EXEC, 1, NULL, NULL }

#define INIT_THREAD  {				\
	.sp		= sizeof(init_stack) +	\
			  (long) &init_stack,	\
	.pc		= 0,			\
        .kregs		= &fake_swapper_regs,	\
	.uregs	        = NULL,			\
	.trap_no	= 0,			\
	.error_code	= 0,			\
	.address	= 0,			\
	.flags		= 0,			\
}

#define SR_USER (SR_MMU | SR_FD)

#define start_thread(_regs, new_pc, new_sp)			\
	_regs->sr = SR_USER;			\
	_regs->pc = new_pc - 4;		\
	_regs->pc |= 1;				\
	_regs->regs[18] = 0;					\
	_regs->regs[15] = new_sp

struct task_struct;
struct mm_struct;

extern void release_thread(struct task_struct *);
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);


#define copy_segments(p, mm)	do { } while (0)
#define release_segments(mm)	do { } while (0)
#define forget_segments()	do { } while (0)
#define prepare_to_copy(tsk)	do { } while (0)

static inline void disable_fpu(void)
{
	unsigned long long __dummy;

	
	__asm__ __volatile__("getcon	" __SR ", %0\n\t"
			     "or	%0, %1, %0\n\t"
			     "putcon	%0, " __SR "\n\t"
			     : "=&r" (__dummy)
			     : "r" (SR_FD));
}

static inline void enable_fpu(void)
{
	unsigned long long __dummy;

	
	__asm__ __volatile__("getcon	" __SR ", %0\n\t"
			     "and	%0, %1, %0\n\t"
			     "putcon	%0, " __SR "\n\t"
			     : "=&r" (__dummy)
			     : "r" (~SR_FD));
}

#if defined(CONFIG_SH64_FPU_DENORM_FLUSH)
#define FPSCR_INIT  0x00040000
#else
#define FPSCR_INIT  0x00000000
#endif

#ifdef CONFIG_SH_FPU
void fpinit(struct sh_fpu_hard_struct *fpregs);
#else
#define fpinit(fpregs)	do { } while (0)
#endif

extern struct task_struct *last_task_used_math;

#define thread_saved_pc(tsk)	(tsk->thread.pc)

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  ((tsk)->thread.pc)
#define KSTK_ESP(tsk)  ((tsk)->thread.sp)

#endif	
#endif 
