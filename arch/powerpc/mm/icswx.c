/*
 *  ICSWX and ACOP Management
 *
 *  Copyright (C) 2011 Anton Blanchard, IBM Corp. <anton@samba.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "icswx.h"


void switch_cop(struct mm_struct *next)
{
#ifdef CONFIG_ICSWX_PID
	mtspr(SPRN_PID, next->context.cop_pid);
#endif
	mtspr(SPRN_ACOP, next->context.acop);
}

int use_cop(unsigned long acop, struct mm_struct *mm)
{
	int ret;

	if (!cpu_has_feature(CPU_FTR_ICSWX))
		return -ENODEV;

	if (!mm || !acop)
		return -EINVAL;

	
	spin_lock(&mm->page_table_lock);
	spin_lock(mm->context.cop_lockp);

	ret = get_cop_pid(mm);
	if (ret < 0)
		goto out;

	
	mm->context.acop |= acop;

	sync_cop(mm);

	if (atomic_read(&mm->mm_users) > 1)
		smp_call_function(sync_cop, mm, 1);

out:
	spin_unlock(mm->context.cop_lockp);
	spin_unlock(&mm->page_table_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(use_cop);

void drop_cop(unsigned long acop, struct mm_struct *mm)
{
	int free_pid;

	if (!cpu_has_feature(CPU_FTR_ICSWX))
		return;

	if (WARN_ON_ONCE(!mm))
		return;

	
	spin_lock(&mm->page_table_lock);
	spin_lock(mm->context.cop_lockp);

	mm->context.acop &= ~acop;

	free_pid = disable_cop_pid(mm);
	sync_cop(mm);

	if (atomic_read(&mm->mm_users) > 1)
		smp_call_function(sync_cop, mm, 1);

	if (free_pid != COP_PID_NONE)
		free_cop_pid(free_pid);

	spin_unlock(mm->context.cop_lockp);
	spin_unlock(&mm->page_table_lock);
}
EXPORT_SYMBOL_GPL(drop_cop);

static int acop_use_cop(int ct)
{
	
	return -1;
}

static u32 acop_get_inst(struct pt_regs *regs)
{
	u32 inst;
	u32 __user *p;

	p = (u32 __user *)regs->nip;
	if (!access_ok(VERIFY_READ, p, sizeof(*p)))
		return 0;

	if (__get_user(inst, p))
		return 0;

	return inst;
}

int acop_handle_fault(struct pt_regs *regs, unsigned long address,
		      unsigned long error_code)
{
	int ct;
	u32 inst = 0;

	if (!cpu_has_feature(CPU_FTR_ICSWX)) {
		pr_info("No coprocessors available");
		_exception(SIGILL, regs, ILL_ILLOPN, address);
	}

	if (!user_mode(regs)) {
		die("ICSWX from kernel failed", regs, SIGSEGV);
	}

	
	ct = ICSWX_GET_CT_HINT(error_code);
	if (ct < 0) {
		
		u32 ccw;
		u32 rs;

		inst = acop_get_inst(regs);
		if (inst == 0)
			return -1;

		rs = (inst >> (31 - 10)) & 0x1f;
		ccw = regs->gpr[rs];
		ct = (ccw >> 16) & 0x3f;
	}

	if ((acop_copro_type_bit(ct) & current->active_mm->context.acop) != 0) {
		sync_cop(current->active_mm);
		return 0;
	}

	
	if (!acop_use_cop(ct))
		return 0;

	
	pr_warn("%s[%d]: Coprocessor %d is unavailable\n",
		current->comm, current->pid, ct);

	
	if (inst == 0) {
		inst = acop_get_inst(regs);
		if (inst == 0)
			return -1;
	}

	
	if (inst & 1) {
		regs->ccr &= ~(0xful << 28);
		regs->ccr |= ICSWX_RC_NOT_FOUND << 28;

		
		regs->nip += 4;
	} else {
#ifdef CONFIG_PPC_ICSWX_USE_SIGILL
		_exception(SIGILL, regs, ILL_ILLOPN, address);
#else
		regs->nip += 4;
#endif
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acop_handle_fault);
