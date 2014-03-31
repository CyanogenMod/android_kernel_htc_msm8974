/*
 * arch/sh/kernel/hw_breakpoint.c
 *
 * Unified kernel/user-space hardware breakpoint facility for the on-chip UBC.
 *
 * Copyright (C) 2009 - 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/percpu.h>
#include <linux/kallsyms.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>
#include <linux/kdebug.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <asm/hw_breakpoint.h>
#include <asm/mmu_context.h>
#include <asm/ptrace.h>
#include <asm/traps.h>

static DEFINE_PER_CPU(struct perf_event *, bp_per_reg[HBP_NUM]);

static struct sh_ubc ubc_dummy = { .num_events = 0 };

static struct sh_ubc *sh_ubc __read_mostly = &ubc_dummy;

int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	int i;

	for (i = 0; i < sh_ubc->num_events; i++) {
		struct perf_event **slot = &__get_cpu_var(bp_per_reg[i]);

		if (!*slot) {
			*slot = bp;
			break;
		}
	}

	if (WARN_ONCE(i == sh_ubc->num_events, "Can't find any breakpoint slot"))
		return -EBUSY;

	clk_enable(sh_ubc->clk);
	sh_ubc->enable(info, i);

	return 0;
}

void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	int i;

	for (i = 0; i < sh_ubc->num_events; i++) {
		struct perf_event **slot = &__get_cpu_var(bp_per_reg[i]);

		if (*slot == bp) {
			*slot = NULL;
			break;
		}
	}

	if (WARN_ONCE(i == sh_ubc->num_events, "Can't find any breakpoint slot"))
		return;

	sh_ubc->disable(info, i);
	clk_disable(sh_ubc->clk);
}

static int get_hbp_len(u16 hbp_len)
{
	unsigned int len_in_bytes = 0;

	switch (hbp_len) {
	case SH_BREAKPOINT_LEN_1:
		len_in_bytes = 1;
		break;
	case SH_BREAKPOINT_LEN_2:
		len_in_bytes = 2;
		break;
	case SH_BREAKPOINT_LEN_4:
		len_in_bytes = 4;
		break;
	case SH_BREAKPOINT_LEN_8:
		len_in_bytes = 8;
		break;
	}
	return len_in_bytes;
}

int arch_check_bp_in_kernelspace(struct perf_event *bp)
{
	unsigned int len;
	unsigned long va;
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	va = info->address;
	len = get_hbp_len(info->len);

	return (va >= TASK_SIZE) && ((va + len - 1) >= TASK_SIZE);
}

int arch_bp_generic_fields(int sh_len, int sh_type,
			   int *gen_len, int *gen_type)
{
	
	switch (sh_len) {
	case SH_BREAKPOINT_LEN_1:
		*gen_len = HW_BREAKPOINT_LEN_1;
		break;
	case SH_BREAKPOINT_LEN_2:
		*gen_len = HW_BREAKPOINT_LEN_2;
		break;
	case SH_BREAKPOINT_LEN_4:
		*gen_len = HW_BREAKPOINT_LEN_4;
		break;
	case SH_BREAKPOINT_LEN_8:
		*gen_len = HW_BREAKPOINT_LEN_8;
		break;
	default:
		return -EINVAL;
	}

	
	switch (sh_type) {
	case SH_BREAKPOINT_READ:
		*gen_type = HW_BREAKPOINT_R;
	case SH_BREAKPOINT_WRITE:
		*gen_type = HW_BREAKPOINT_W;
		break;
	case SH_BREAKPOINT_RW:
		*gen_type = HW_BREAKPOINT_W | HW_BREAKPOINT_R;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int arch_build_bp_info(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	info->address = bp->attr.bp_addr;

	
	switch (bp->attr.bp_len) {
	case HW_BREAKPOINT_LEN_1:
		info->len = SH_BREAKPOINT_LEN_1;
		break;
	case HW_BREAKPOINT_LEN_2:
		info->len = SH_BREAKPOINT_LEN_2;
		break;
	case HW_BREAKPOINT_LEN_4:
		info->len = SH_BREAKPOINT_LEN_4;
		break;
	case HW_BREAKPOINT_LEN_8:
		info->len = SH_BREAKPOINT_LEN_8;
		break;
	default:
		return -EINVAL;
	}

	
	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_R:
		info->type = SH_BREAKPOINT_READ;
		break;
	case HW_BREAKPOINT_W:
		info->type = SH_BREAKPOINT_WRITE;
		break;
	case HW_BREAKPOINT_W | HW_BREAKPOINT_R:
		info->type = SH_BREAKPOINT_RW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int arch_validate_hwbkpt_settings(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	unsigned int align;
	int ret;

	ret = arch_build_bp_info(bp);
	if (ret)
		return ret;

	ret = -EINVAL;

	switch (info->len) {
	case SH_BREAKPOINT_LEN_1:
		align = 0;
		break;
	case SH_BREAKPOINT_LEN_2:
		align = 1;
		break;
	case SH_BREAKPOINT_LEN_4:
		align = 3;
		break;
	case SH_BREAKPOINT_LEN_8:
		align = 7;
		break;
	default:
		return ret;
	}

	if (info->name)
		info->address = (unsigned long)kallsyms_lookup_name(info->name);

	if (info->address & align)
		return -EINVAL;

	return 0;
}

void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	int i;
	struct thread_struct *t = &tsk->thread;

	for (i = 0; i < sh_ubc->num_events; i++) {
		unregister_hw_breakpoint(t->ptrace_bps[i]);
		t->ptrace_bps[i] = NULL;
	}
}

static int __kprobes hw_breakpoint_handler(struct die_args *args)
{
	int cpu, i, rc = NOTIFY_STOP;
	struct perf_event *bp;
	unsigned int cmf, resume_mask;

	cmf = sh_ubc->triggered_mask();
	if (unlikely(!cmf))
		return NOTIFY_DONE;

	resume_mask = sh_ubc->active_mask();

	sh_ubc->disable_all();

	cpu = get_cpu();
	for (i = 0; i < sh_ubc->num_events; i++) {
		unsigned long event_mask = (1 << i);

		if (likely(!(cmf & event_mask)))
			continue;

		rcu_read_lock();

		bp = per_cpu(bp_per_reg[i], cpu);
		if (bp)
			rc = NOTIFY_DONE;

		sh_ubc->clear_triggered_mask(event_mask);

		if (!bp) {
			rcu_read_unlock();
			break;
		}

		if (bp->overflow_handler == ptrace_triggered)
			resume_mask &= ~(1 << i);

		perf_bp_event(bp, args->regs);

		
		if (!arch_check_bp_in_kernelspace(bp)) {
			siginfo_t info;

			info.si_signo = args->signr;
			info.si_errno = notifier_to_errno(rc);
			info.si_code = TRAP_HWBKPT;

			force_sig_info(args->signr, &info, current);
		}

		rcu_read_unlock();
	}

	if (cmf == 0)
		rc = NOTIFY_DONE;

	sh_ubc->enable_all(resume_mask);

	put_cpu();

	return rc;
}

BUILD_TRAP_HANDLER(breakpoint)
{
	unsigned long ex = lookup_exception_vector();
	TRAP_HANDLER_DECL;

	notify_die(DIE_BREAKPOINT, "breakpoint", regs, 0, ex, SIGTRAP);
}

int __kprobes hw_breakpoint_exceptions_notify(struct notifier_block *unused,
				    unsigned long val, void *data)
{
	struct die_args *args = data;

	if (val != DIE_BREAKPOINT)
		return NOTIFY_DONE;

	if (args->trapnr != sh_ubc->trap_nr)
		return NOTIFY_DONE;

	return hw_breakpoint_handler(data);
}

void hw_breakpoint_pmu_read(struct perf_event *bp)
{
	
}

int register_sh_ubc(struct sh_ubc *ubc)
{
	
	if (sh_ubc != &ubc_dummy)
		return -EBUSY;
	sh_ubc = ubc;

	pr_info("HW Breakpoints: %s UBC support registered\n", ubc->name);

	WARN_ON(ubc->num_events > HBP_NUM);

	return 0;
}
