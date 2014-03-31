/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/unistd.h>
#include <linux/notifier.h>

#include <asm/traps.h>
#include <asm/uaccess.h>
#include <asm/ocd.h>
#include <asm/mmu_context.h>
#include <linux/kdebug.h>

static struct pt_regs *get_user_regs(struct task_struct *tsk)
{
	return (struct pt_regs *)((unsigned long)task_stack_page(tsk) +
				  THREAD_SIZE - sizeof(struct pt_regs));
}

void user_enable_single_step(struct task_struct *tsk)
{
	pr_debug("user_enable_single_step: pid=%u, PC=0x%08lx, SR=0x%08lx\n",
		 tsk->pid, task_pt_regs(tsk)->pc, task_pt_regs(tsk)->sr);

	set_tsk_thread_flag(tsk, TIF_BREAKPOINT);
	set_tsk_thread_flag(tsk, TIF_SINGLE_STEP);
}

void user_disable_single_step(struct task_struct *child)
{
	
}

void ptrace_disable(struct task_struct *child)
{
	clear_tsk_thread_flag(child, TIF_SINGLE_STEP);
	clear_tsk_thread_flag(child, TIF_BREAKPOINT);
	ocd_disable(child);
}

static int ptrace_read_user(struct task_struct *tsk, unsigned long offset,
			    unsigned long __user *data)
{
	unsigned long *regs;
	unsigned long value;

	if (offset & 3 || offset >= sizeof(struct user)) {
		printk("ptrace_read_user: invalid offset 0x%08lx\n", offset);
		return -EIO;
	}

	regs = (unsigned long *)get_user_regs(tsk);

	value = 0;
	if (offset < sizeof(struct pt_regs))
		value = regs[offset / sizeof(regs[0])];

	pr_debug("ptrace_read_user(%s[%u], %#lx, %p) -> %#lx\n",
		 tsk->comm, tsk->pid, offset, data, value);

	return put_user(value, data);
}

static int ptrace_write_user(struct task_struct *tsk, unsigned long offset,
			     unsigned long value)
{
	unsigned long *regs;

	pr_debug("ptrace_write_user(%s[%u], %#lx, %#lx)\n",
			tsk->comm, tsk->pid, offset, value);

	if (offset & 3 || offset >= sizeof(struct user)) {
		pr_debug("  invalid offset 0x%08lx\n", offset);
		return -EIO;
	}

	if (offset >= sizeof(struct pt_regs))
		return 0;

	regs = (unsigned long *)get_user_regs(tsk);
	regs[offset / sizeof(regs[0])] = value;

	return 0;
}

static int ptrace_getregs(struct task_struct *tsk, void __user *uregs)
{
	struct pt_regs *regs = get_user_regs(tsk);

	return copy_to_user(uregs, regs, sizeof(*regs)) ? -EFAULT : 0;
}

static int ptrace_setregs(struct task_struct *tsk, const void __user *uregs)
{
	struct pt_regs newregs;
	int ret;

	ret = -EFAULT;
	if (copy_from_user(&newregs, uregs, sizeof(newregs)) == 0) {
		struct pt_regs *regs = get_user_regs(tsk);

		ret = -EINVAL;
		if (valid_user_regs(&newregs)) {
			*regs = newregs;
			ret = 0;
		}
	}

	return ret;
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret;
	void __user *datap = (void __user *) data;

	switch (request) {
	
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		ret = generic_ptrace_peekdata(child, addr, data);
		break;

	case PTRACE_PEEKUSR:
		ret = ptrace_read_user(child, addr, datap);
		break;

	
	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		ret = generic_ptrace_pokedata(child, addr, data);
		break;

	case PTRACE_POKEUSR:
		ret = ptrace_write_user(child, addr, data);
		break;

	case PTRACE_GETREGS:
		ret = ptrace_getregs(child, datap);
		break;

	case PTRACE_SETREGS:
		ret = ptrace_setregs(child, datap);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

asmlinkage void syscall_trace(void)
{
	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;
	if (!(current->ptrace & PT_PTRACED))
		return;

	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));

	if (current->exit_code) {
		pr_debug("syscall_trace: sending signal %d to PID %u\n",
			 current->exit_code, current->pid);
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}

extern void debug_trampoline(void);

asmlinkage struct pt_regs *do_debug(struct pt_regs *regs)
{
	struct thread_info	*ti;
	unsigned long		trampoline_addr;
	u32			status;
	u32			ctrl;
	int			code;

	status = ocd_read(DS);
	ti = current_thread_info();
	code = TRAP_BRKPT;

	pr_debug("do_debug: status=0x%08x PC=0x%08lx SR=0x%08lx tif=0x%08lx\n",
			status, regs->pc, regs->sr, ti->flags);

	if (!user_mode(regs)) {
		unsigned long	die_val = DIE_BREAKPOINT;

		if (status & (1 << OCD_DS_SSS_BIT))
			die_val = DIE_SSTEP;

		if (notify_die(die_val, "ptrace", regs, 0, 0, SIGTRAP)
				== NOTIFY_STOP)
			return regs;

		if ((status & (1 << OCD_DS_SWB_BIT))
				&& test_and_clear_ti_thread_flag(
					ti, TIF_BREAKPOINT)) {
			regs++;
			pr_debug("  -> TIF_BREAKPOINT done, adjusted regs:"
					"PC=0x%08lx SR=0x%08lx\n",
					regs->pc, regs->sr);
			BUG_ON(!user_mode(regs));

			if (test_thread_flag(TIF_SINGLE_STEP)) {
				pr_debug("Going to do single step...\n");
				return regs;
			}

			code = TRAP_TRACE;
		} else if ((status & (1 << OCD_DS_SSS_BIT))
				&& test_ti_thread_flag(ti, TIF_SINGLE_STEP)) {

			pr_debug("Stepped into something, "
					"setting TIF_BREAKPOINT...\n");
			set_ti_thread_flag(ti, TIF_BREAKPOINT);

			if ((regs->sr & MODE_MASK) == MODE_EXCEPTION) {
				trampoline_addr
					= (unsigned long)&debug_trampoline;

				pr_debug("Setting up trampoline...\n");
				ti->rar_saved = sysreg_read(RAR_EX);
				ti->rsr_saved = sysreg_read(RSR_EX);
				sysreg_write(RAR_EX, trampoline_addr);
				sysreg_write(RSR_EX, (MODE_EXCEPTION
							| SR_EM | SR_GM));
				BUG_ON(ti->rsr_saved & MODE_MASK);
			}

			if ((regs->sr & MODE_MASK) == MODE_SUPERVISOR) {
				pr_debug("Supervisor; no single step\n");
				clear_ti_thread_flag(ti, TIF_SINGLE_STEP);
			}

			ctrl = ocd_read(DC);
			ctrl &= ~(1 << OCD_DC_SS_BIT);
			ocd_write(DC, ctrl);

			return regs;
		} else {
			printk(KERN_ERR "Unexpected OCD_DS value: 0x%08x\n",
					status);
			printk(KERN_ERR "Thread flags: 0x%08lx\n", ti->flags);
			die("Unhandled debug trap in kernel mode",
					regs, SIGTRAP);
		}
	} else if (status & (1 << OCD_DS_SSS_BIT)) {
		
		code = TRAP_TRACE;

		ctrl = ocd_read(DC);
		ctrl &= ~(1 << OCD_DC_SS_BIT);
		ocd_write(DC, ctrl);
	}

	pr_debug("Sending SIGTRAP: code=%d PC=0x%08lx SR=0x%08lx\n",
			code, regs->pc, regs->sr);

	clear_thread_flag(TIF_SINGLE_STEP);
	_exception(SIGTRAP, regs, code, instruction_pointer(regs));

	return regs;
}
