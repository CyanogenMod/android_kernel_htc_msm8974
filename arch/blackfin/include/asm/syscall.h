/*
 * Magic syscall break down functions
 *
 * Copyright 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __ASM_BLACKFIN_SYSCALL_H__
#define __ASM_BLACKFIN_SYSCALL_H__


#include <linux/err.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

static inline long
syscall_get_nr(struct task_struct *task, struct pt_regs *regs)
{
	return regs->p0;
}

static inline void
syscall_rollback(struct task_struct *task, struct pt_regs *regs)
{
	regs->p0 = regs->orig_p0;
}

static inline long
syscall_get_error(struct task_struct *task, struct pt_regs *regs)
{
	return IS_ERR_VALUE(regs->r0) ? regs->r0 : 0;
}

static inline long
syscall_get_return_value(struct task_struct *task, struct pt_regs *regs)
{
	return regs->r0;
}

static inline void
syscall_set_return_value(struct task_struct *task, struct pt_regs *regs,
                         int error, long val)
{
	regs->r0 = error ? -error : val;
}

static inline void
syscall_get_arguments(struct task_struct *task, struct pt_regs *regs,
                      unsigned int i, unsigned int n, unsigned long *args)
{
	long *aregs = &regs->r0 - i;

	BUG_ON(i > 5 || i + n > 6);

	while (n--)
		*args++ = *aregs--;
}

static inline void
syscall_set_arguments(struct task_struct *task, struct pt_regs *regs,
                      unsigned int i, unsigned int n, const unsigned long *args)
{
	long *aregs = &regs->r0 - i;

	BUG_ON(i > 5 || i + n > 6);

	while (n--)
		*aregs-- = *args++;
}

#endif
