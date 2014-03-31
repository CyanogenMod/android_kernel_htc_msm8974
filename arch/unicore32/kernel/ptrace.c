/*
 * linux/arch/unicore32/kernel/ptrace.c
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * By Ross Biro 1/23/92
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/uaccess.h>

static inline long get_user_reg(struct task_struct *task, int offset)
{
	return task_pt_regs(task)->uregs[offset];
}

static inline int
put_user_reg(struct task_struct *task, int offset, long data)
{
	struct pt_regs newregs, *regs = task_pt_regs(task);
	int ret = -EINVAL;

	newregs = *regs;
	newregs.uregs[offset] = data;

	if (valid_user_regs(&newregs)) {
		regs->uregs[offset] = data;
		ret = 0;
	}

	return ret;
}

void ptrace_disable(struct task_struct *child)
{
}

static int ptrace_read_user(struct task_struct *tsk, unsigned long off,
			    unsigned long __user *ret)
{
	unsigned long tmp;

	tmp = 0;
	if (off < sizeof(struct pt_regs))
		tmp = get_user_reg(tsk, off >> 2);

	return put_user(tmp, ret);
}

static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
			     unsigned long val)
{
	if (off >= sizeof(struct pt_regs))
		return 0;

	return put_user_reg(tsk, off >> 2, val);
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret;
	unsigned long __user *datap = (unsigned long __user *) data;

	switch (request) {
	case PTRACE_PEEKUSR:
		ret = ptrace_read_user(child, addr, datap);
		break;

	case PTRACE_POKEUSR:
		ret = ptrace_write_user(child, addr, data);
		break;

	case PTRACE_GET_THREAD_AREA:
		ret = put_user(task_pt_regs(child)->UCreg_16,
			       datap);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

asmlinkage int syscall_trace(int why, struct pt_regs *regs, int scno)
{
	unsigned long ip;

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return scno;
	if (!(current->ptrace & PT_PTRACED))
		return scno;

	ip = regs->UCreg_ip;
	regs->UCreg_ip = why;

	current_thread_info()->syscall = scno;

	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
	regs->UCreg_ip = ip;

	return current_thread_info()->syscall;
}
