/*
 * HW_breakpoint: a unified kernel/user-space hardware breakpoint facility,
 * using the CPU's debug registers. Derived from
 * "arch/x86/kernel/hw_breakpoint.c"
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright 2010 IBM Corporation
 * Author: K.Prasad <prasad@linux.vnet.ibm.com>
 *
 */

#include <linux/hw_breakpoint.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/hw_breakpoint.h>
#include <asm/processor.h>
#include <asm/sstep.h>
#include <asm/uaccess.h>

static DEFINE_PER_CPU(struct perf_event *, bp_per_reg);

int hw_breakpoint_slots(int type)
{
	if (type == TYPE_DATA)
		return HBP_NUM;
	return 0;		
}

int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	struct perf_event **slot = &__get_cpu_var(bp_per_reg);

	*slot = bp;

	if (current->thread.last_hit_ubp != bp)
		set_dabr(info->address | info->type | DABR_TRANSLATION);

	return 0;
}

void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	struct perf_event **slot = &__get_cpu_var(bp_per_reg);

	if (*slot != bp) {
		WARN_ONCE(1, "Can't find the breakpoint");
		return;
	}

	*slot = NULL;
	set_dabr(0);
}

void arch_unregister_hw_breakpoint(struct perf_event *bp)
{
	if (bp->ctx->task)
		bp->ctx->task->thread.last_hit_ubp = NULL;
}

int arch_check_bp_in_kernelspace(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	return is_kernel_addr(info->address);
}

int arch_bp_generic_fields(int type, int *gen_bp_type)
{
	switch (type) {
	case DABR_DATA_READ:
		*gen_bp_type = HW_BREAKPOINT_R;
		break;
	case DABR_DATA_WRITE:
		*gen_bp_type = HW_BREAKPOINT_W;
		break;
	case (DABR_DATA_WRITE | DABR_DATA_READ):
		*gen_bp_type = (HW_BREAKPOINT_W | HW_BREAKPOINT_R);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int arch_validate_hwbkpt_settings(struct perf_event *bp)
{
	int ret = -EINVAL;
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	if (!bp)
		return ret;

	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_R:
		info->type = DABR_DATA_READ;
		break;
	case HW_BREAKPOINT_W:
		info->type = DABR_DATA_WRITE;
		break;
	case HW_BREAKPOINT_R | HW_BREAKPOINT_W:
		info->type = (DABR_DATA_READ | DABR_DATA_WRITE);
		break;
	default:
		return ret;
	}

	info->address = bp->attr.bp_addr;
	info->len = bp->attr.bp_len;

	if (info->len >
	    (HW_BREAKPOINT_LEN - (info->address & HW_BREAKPOINT_ALIGN)))
		return -EINVAL;
	return 0;
}

void thread_change_pc(struct task_struct *tsk, struct pt_regs *regs)
{
	struct arch_hw_breakpoint *info;

	if (likely(!tsk->thread.last_hit_ubp))
		return;

	info = counter_arch_bp(tsk->thread.last_hit_ubp);
	regs->msr &= ~MSR_SE;
	set_dabr(info->address | info->type | DABR_TRANSLATION);
	tsk->thread.last_hit_ubp = NULL;
}

int __kprobes hw_breakpoint_handler(struct die_args *args)
{
	int rc = NOTIFY_STOP;
	struct perf_event *bp;
	struct pt_regs *regs = args->regs;
	int stepped = 1;
	struct arch_hw_breakpoint *info;
	unsigned int instr;
	unsigned long dar = regs->dar;

	
	set_dabr(0);

	rcu_read_lock();

	bp = __get_cpu_var(bp_per_reg);
	if (!bp)
		goto out;
	info = counter_arch_bp(bp);

	if (bp->overflow_handler == ptrace_triggered) {
		perf_bp_event(bp, regs);
		rc = NOTIFY_DONE;
		goto out;
	}

	info->extraneous_interrupt = !((bp->attr.bp_addr <= dar) &&
			(dar - bp->attr.bp_addr < bp->attr.bp_len));

	
	if (user_mode(regs)) {
		bp->ctx->task->thread.last_hit_ubp = bp;
		regs->msr |= MSR_SE;
		goto out;
	}

	stepped = 0;
	instr = 0;
	if (!__get_user_inatomic(instr, (unsigned int *) regs->nip))
		stepped = emulate_step(regs, instr);

	if (!stepped) {
		WARN(1, "Unable to handle hardware breakpoint. Breakpoint at "
			"0x%lx will be disabled.", info->address);
		perf_event_disable(bp);
		goto out;
	}
	if (!info->extraneous_interrupt)
		perf_bp_event(bp, regs);

	set_dabr(info->address | info->type | DABR_TRANSLATION);
out:
	rcu_read_unlock();
	return rc;
}

int __kprobes single_step_dabr_instruction(struct die_args *args)
{
	struct pt_regs *regs = args->regs;
	struct perf_event *bp = NULL;
	struct arch_hw_breakpoint *bp_info;

	bp = current->thread.last_hit_ubp;
	if (!bp)
		return NOTIFY_DONE;

	bp_info = counter_arch_bp(bp);

	if (!bp_info->extraneous_interrupt)
		perf_bp_event(bp, regs);

	set_dabr(bp_info->address | bp_info->type | DABR_TRANSLATION);
	current->thread.last_hit_ubp = NULL;

	if (test_thread_flag(TIF_SINGLESTEP))
		return NOTIFY_DONE;

	return NOTIFY_STOP;
}

int __kprobes hw_breakpoint_exceptions_notify(
		struct notifier_block *unused, unsigned long val, void *data)
{
	int ret = NOTIFY_DONE;

	switch (val) {
	case DIE_DABR_MATCH:
		ret = hw_breakpoint_handler(data);
		break;
	case DIE_SSTEP:
		ret = single_step_dabr_instruction(data);
		break;
	}

	return ret;
}

void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	struct thread_struct *t = &tsk->thread;

	unregister_hw_breakpoint(t->ptrace_bps[0]);
	t->ptrace_bps[0] = NULL;
}

void hw_breakpoint_pmu_read(struct perf_event *bp)
{
	
}
