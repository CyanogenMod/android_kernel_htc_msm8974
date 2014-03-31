/*
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
 * Copyright (C) 2007 Alan Stern
 * Copyright (C) 2009 IBM Corporation
 * Copyright (C) 2009 Frederic Weisbecker <fweisbec@gmail.com>
 *
 * Authors: Alan Stern <stern@rowland.harvard.edu>
 *          K.Prasad <prasad@linux.vnet.ibm.com>
 *          Frederic Weisbecker <fweisbec@gmail.com>
 */


#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/irqflags.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/hw_breakpoint.h>
#include <asm/processor.h>
#include <asm/debugreg.h>

DEFINE_PER_CPU(unsigned long, cpu_dr7);
EXPORT_PER_CPU_SYMBOL(cpu_dr7);

static DEFINE_PER_CPU(unsigned long, cpu_debugreg[HBP_NUM]);

static DEFINE_PER_CPU(struct perf_event *, bp_per_reg[HBP_NUM]);


static inline unsigned long
__encode_dr7(int drnum, unsigned int len, unsigned int type)
{
	unsigned long bp_info;

	bp_info = (len | type) & 0xf;
	bp_info <<= (DR_CONTROL_SHIFT + drnum * DR_CONTROL_SIZE);
	bp_info |= (DR_GLOBAL_ENABLE << (drnum * DR_ENABLE_SIZE));

	return bp_info;
}

unsigned long encode_dr7(int drnum, unsigned int len, unsigned int type)
{
	return __encode_dr7(drnum, len, type) | DR_GLOBAL_SLOWDOWN;
}

int decode_dr7(unsigned long dr7, int bpnum, unsigned *len, unsigned *type)
{
	int bp_info = dr7 >> (DR_CONTROL_SHIFT + bpnum * DR_CONTROL_SIZE);

	*len = (bp_info & 0xc) | 0x40;
	*type = (bp_info & 0x3) | 0x80;

	return (dr7 >> (bpnum * DR_ENABLE_SIZE)) & 0x3;
}

int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	unsigned long *dr7;
	int i;

	for (i = 0; i < HBP_NUM; i++) {
		struct perf_event **slot = &__get_cpu_var(bp_per_reg[i]);

		if (!*slot) {
			*slot = bp;
			break;
		}
	}

	if (WARN_ONCE(i == HBP_NUM, "Can't find any breakpoint slot"))
		return -EBUSY;

	set_debugreg(info->address, i);
	__this_cpu_write(cpu_debugreg[i], info->address);

	dr7 = &__get_cpu_var(cpu_dr7);
	*dr7 |= encode_dr7(i, info->len, info->type);

	set_debugreg(*dr7, 7);

	return 0;
}

void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	unsigned long *dr7;
	int i;

	for (i = 0; i < HBP_NUM; i++) {
		struct perf_event **slot = &__get_cpu_var(bp_per_reg[i]);

		if (*slot == bp) {
			*slot = NULL;
			break;
		}
	}

	if (WARN_ONCE(i == HBP_NUM, "Can't find any breakpoint slot"))
		return;

	dr7 = &__get_cpu_var(cpu_dr7);
	*dr7 &= ~__encode_dr7(i, info->len, info->type);

	set_debugreg(*dr7, 7);
}

static int get_hbp_len(u8 hbp_len)
{
	unsigned int len_in_bytes = 0;

	switch (hbp_len) {
	case X86_BREAKPOINT_LEN_1:
		len_in_bytes = 1;
		break;
	case X86_BREAKPOINT_LEN_2:
		len_in_bytes = 2;
		break;
	case X86_BREAKPOINT_LEN_4:
		len_in_bytes = 4;
		break;
#ifdef CONFIG_X86_64
	case X86_BREAKPOINT_LEN_8:
		len_in_bytes = 8;
		break;
#endif
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

int arch_bp_generic_fields(int x86_len, int x86_type,
			   int *gen_len, int *gen_type)
{
	
	switch (x86_type) {
	case X86_BREAKPOINT_EXECUTE:
		if (x86_len != X86_BREAKPOINT_LEN_X)
			return -EINVAL;

		*gen_type = HW_BREAKPOINT_X;
		*gen_len = sizeof(long);
		return 0;
	case X86_BREAKPOINT_WRITE:
		*gen_type = HW_BREAKPOINT_W;
		break;
	case X86_BREAKPOINT_RW:
		*gen_type = HW_BREAKPOINT_W | HW_BREAKPOINT_R;
		break;
	default:
		return -EINVAL;
	}

	
	switch (x86_len) {
	case X86_BREAKPOINT_LEN_1:
		*gen_len = HW_BREAKPOINT_LEN_1;
		break;
	case X86_BREAKPOINT_LEN_2:
		*gen_len = HW_BREAKPOINT_LEN_2;
		break;
	case X86_BREAKPOINT_LEN_4:
		*gen_len = HW_BREAKPOINT_LEN_4;
		break;
#ifdef CONFIG_X86_64
	case X86_BREAKPOINT_LEN_8:
		*gen_len = HW_BREAKPOINT_LEN_8;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}


static int arch_build_bp_info(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	info->address = bp->attr.bp_addr;

	
	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_W:
		info->type = X86_BREAKPOINT_WRITE;
		break;
	case HW_BREAKPOINT_W | HW_BREAKPOINT_R:
		info->type = X86_BREAKPOINT_RW;
		break;
	case HW_BREAKPOINT_X:
		info->type = X86_BREAKPOINT_EXECUTE;
		if (bp->attr.bp_len == sizeof(long)) {
			info->len = X86_BREAKPOINT_LEN_X;
			return 0;
		}
	default:
		return -EINVAL;
	}

	
	switch (bp->attr.bp_len) {
	case HW_BREAKPOINT_LEN_1:
		info->len = X86_BREAKPOINT_LEN_1;
		break;
	case HW_BREAKPOINT_LEN_2:
		info->len = X86_BREAKPOINT_LEN_2;
		break;
	case HW_BREAKPOINT_LEN_4:
		info->len = X86_BREAKPOINT_LEN_4;
		break;
#ifdef CONFIG_X86_64
	case HW_BREAKPOINT_LEN_8:
		info->len = X86_BREAKPOINT_LEN_8;
		break;
#endif
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
	case X86_BREAKPOINT_LEN_1:
		align = 0;
		break;
	case X86_BREAKPOINT_LEN_2:
		align = 1;
		break;
	case X86_BREAKPOINT_LEN_4:
		align = 3;
		break;
#ifdef CONFIG_X86_64
	case X86_BREAKPOINT_LEN_8:
		align = 7;
		break;
#endif
	default:
		return ret;
	}

	if (info->address & align)
		return -EINVAL;

	return 0;
}

void aout_dump_debugregs(struct user *dump)
{
	int i;
	int dr7 = 0;
	struct perf_event *bp;
	struct arch_hw_breakpoint *info;
	struct thread_struct *thread = &current->thread;

	for (i = 0; i < HBP_NUM; i++) {
		bp = thread->ptrace_bps[i];

		if (bp && !bp->attr.disabled) {
			dump->u_debugreg[i] = bp->attr.bp_addr;
			info = counter_arch_bp(bp);
			dr7 |= encode_dr7(i, info->len, info->type);
		} else {
			dump->u_debugreg[i] = 0;
		}
	}

	dump->u_debugreg[4] = 0;
	dump->u_debugreg[5] = 0;
	dump->u_debugreg[6] = current->thread.debugreg6;

	dump->u_debugreg[7] = dr7;
}
EXPORT_SYMBOL_GPL(aout_dump_debugregs);

void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	int i;
	struct thread_struct *t = &tsk->thread;

	for (i = 0; i < HBP_NUM; i++) {
		unregister_hw_breakpoint(t->ptrace_bps[i]);
		t->ptrace_bps[i] = NULL;
	}
}

void hw_breakpoint_restore(void)
{
	set_debugreg(__this_cpu_read(cpu_debugreg[0]), 0);
	set_debugreg(__this_cpu_read(cpu_debugreg[1]), 1);
	set_debugreg(__this_cpu_read(cpu_debugreg[2]), 2);
	set_debugreg(__this_cpu_read(cpu_debugreg[3]), 3);
	set_debugreg(current->thread.debugreg6, 6);
	set_debugreg(__this_cpu_read(cpu_dr7), 7);
}
EXPORT_SYMBOL_GPL(hw_breakpoint_restore);

static int __kprobes hw_breakpoint_handler(struct die_args *args)
{
	int i, cpu, rc = NOTIFY_STOP;
	struct perf_event *bp;
	unsigned long dr7, dr6;
	unsigned long *dr6_p;

	
	dr6_p = (unsigned long *)ERR_PTR(args->err);
	dr6 = *dr6_p;

	
	if (dr6 & DR_STEP)
		return NOTIFY_DONE;

	
	if ((dr6 & DR_TRAP_BITS) == 0)
		return NOTIFY_DONE;

	get_debugreg(dr7, 7);
	
	set_debugreg(0UL, 7);
	current->thread.debugreg6 &= ~DR_TRAP_BITS;
	cpu = get_cpu();

	
	for (i = 0; i < HBP_NUM; ++i) {
		if (likely(!(dr6 & (DR_TRAP0 << i))))
			continue;

		rcu_read_lock();

		bp = per_cpu(bp_per_reg[i], cpu);
		(*dr6_p) &= ~(DR_TRAP0 << i);
		if (!bp) {
			rcu_read_unlock();
			break;
		}

		perf_bp_event(bp, args->regs);

		if (bp->hw.info.type == X86_BREAKPOINT_EXECUTE)
			args->regs->flags |= X86_EFLAGS_RF;

		rcu_read_unlock();
	}
	if ((current->thread.debugreg6 & DR_TRAP_BITS) ||
	    (dr6 & (~DR_TRAP_BITS)))
		rc = NOTIFY_DONE;

	set_debugreg(dr7, 7);
	put_cpu();

	return rc;
}

int __kprobes hw_breakpoint_exceptions_notify(
		struct notifier_block *unused, unsigned long val, void *data)
{
	if (val != DIE_DEBUG)
		return NOTIFY_DONE;

	return hw_breakpoint_handler(data);
}

void hw_breakpoint_pmu_read(struct perf_event *bp)
{
	
}
