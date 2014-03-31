/*
 * Copyright (C) 1994 Linus Torvalds
 *
 * Pentium III FXSR, SSE support
 * General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * x86-64 work by Andi Kleen 2002
 */

#ifndef _ASM_X86_I387_H
#define _ASM_X86_I387_H

#ifndef __ASSEMBLY__

#include <linux/sched.h>
#include <linux/hardirq.h>

struct pt_regs;
struct user_i387_struct;

extern int init_fpu(struct task_struct *child);
extern int dump_fpu(struct pt_regs *, struct user_i387_struct *);
extern void math_state_restore(void);

extern bool irq_fpu_usable(void);
extern void kernel_fpu_begin(void);
extern void kernel_fpu_end(void);

static inline int irq_ts_save(void)
{
	if (!in_atomic())
		return 0;

	if (read_cr0() & X86_CR0_TS) {
		clts();
		return 1;
	}

	return 0;
}

static inline void irq_ts_restore(int TS_state)
{
	if (TS_state)
		stts();
}

static inline int user_has_fpu(void)
{
	return current->thread.fpu.has_fpu;
}

extern void unlazy_fpu(struct task_struct *tsk);

#endif 

#endif 
