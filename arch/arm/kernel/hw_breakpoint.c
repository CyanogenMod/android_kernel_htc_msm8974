/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
 * Copyright (C) 2009, 2010 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt) "hw-breakpoint: " fmt

#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/current.h>
#include <asm/hw_breakpoint.h>
#include <asm/kdebug.h>
#include <asm/traps.h>

static DEFINE_PER_CPU(struct perf_event *, bp_on_reg[ARM_MAX_BRP]);

static DEFINE_PER_CPU(struct perf_event *, wp_on_reg[ARM_MAX_WRP]);

static int core_num_brps;
static int core_num_wrps;

static u8 debug_arch;

static u8 max_watchpoint_len;

#define READ_WB_REG_CASE(OP2, M, VAL)		\
	case ((OP2 << 4) + M):			\
		ARM_DBG_READ(c ## M, OP2, VAL); \
		break

#define WRITE_WB_REG_CASE(OP2, M, VAL)		\
	case ((OP2 << 4) + M):			\
		ARM_DBG_WRITE(c ## M, OP2, VAL);\
		break

#define GEN_READ_WB_REG_CASES(OP2, VAL)		\
	READ_WB_REG_CASE(OP2, 0, VAL);		\
	READ_WB_REG_CASE(OP2, 1, VAL);		\
	READ_WB_REG_CASE(OP2, 2, VAL);		\
	READ_WB_REG_CASE(OP2, 3, VAL);		\
	READ_WB_REG_CASE(OP2, 4, VAL);		\
	READ_WB_REG_CASE(OP2, 5, VAL);		\
	READ_WB_REG_CASE(OP2, 6, VAL);		\
	READ_WB_REG_CASE(OP2, 7, VAL);		\
	READ_WB_REG_CASE(OP2, 8, VAL);		\
	READ_WB_REG_CASE(OP2, 9, VAL);		\
	READ_WB_REG_CASE(OP2, 10, VAL);		\
	READ_WB_REG_CASE(OP2, 11, VAL);		\
	READ_WB_REG_CASE(OP2, 12, VAL);		\
	READ_WB_REG_CASE(OP2, 13, VAL);		\
	READ_WB_REG_CASE(OP2, 14, VAL);		\
	READ_WB_REG_CASE(OP2, 15, VAL)

#define GEN_WRITE_WB_REG_CASES(OP2, VAL)	\
	WRITE_WB_REG_CASE(OP2, 0, VAL);		\
	WRITE_WB_REG_CASE(OP2, 1, VAL);		\
	WRITE_WB_REG_CASE(OP2, 2, VAL);		\
	WRITE_WB_REG_CASE(OP2, 3, VAL);		\
	WRITE_WB_REG_CASE(OP2, 4, VAL);		\
	WRITE_WB_REG_CASE(OP2, 5, VAL);		\
	WRITE_WB_REG_CASE(OP2, 6, VAL);		\
	WRITE_WB_REG_CASE(OP2, 7, VAL);		\
	WRITE_WB_REG_CASE(OP2, 8, VAL);		\
	WRITE_WB_REG_CASE(OP2, 9, VAL);		\
	WRITE_WB_REG_CASE(OP2, 10, VAL);	\
	WRITE_WB_REG_CASE(OP2, 11, VAL);	\
	WRITE_WB_REG_CASE(OP2, 12, VAL);	\
	WRITE_WB_REG_CASE(OP2, 13, VAL);	\
	WRITE_WB_REG_CASE(OP2, 14, VAL);	\
	WRITE_WB_REG_CASE(OP2, 15, VAL)

static u32 read_wb_reg(int n)
{
	u32 val = 0;

	switch (n) {
	GEN_READ_WB_REG_CASES(ARM_OP2_BVR, val);
	GEN_READ_WB_REG_CASES(ARM_OP2_BCR, val);
	GEN_READ_WB_REG_CASES(ARM_OP2_WVR, val);
	GEN_READ_WB_REG_CASES(ARM_OP2_WCR, val);
	default:
		pr_warning("attempt to read from unknown breakpoint "
				"register %d\n", n);
	}

	return val;
}

static void write_wb_reg(int n, u32 val)
{
	switch (n) {
	GEN_WRITE_WB_REG_CASES(ARM_OP2_BVR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_BCR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_WVR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_WCR, val);
	default:
		pr_warning("attempt to write to unknown breakpoint "
				"register %d\n", n);
	}
	isb();
}

static u8 get_debug_arch(void)
{
	u32 didr;

	
	if (((read_cpuid_id() >> 16) & 0xf) != 0xf) {
		pr_warning("CPUID feature registers not supported. "
			   "Assuming v6 debug is present.\n");
		return ARM_DEBUG_ARCH_V6;
	}

	ARM_DBG_READ(c0, 0, didr);
	return (didr >> 16) & 0xf;
}

u8 arch_get_debug_arch(void)
{
	return debug_arch;
}

static int debug_arch_supported(void)
{
	u8 arch = get_debug_arch();

	
	return (arch >= ARM_DEBUG_ARCH_V6 && arch <= ARM_DEBUG_ARCH_V7_ECP14) ||
		arch >= ARM_DEBUG_ARCH_V7_1;
}

static int get_num_wrp_resources(void)
{
	u32 didr;
	ARM_DBG_READ(c0, 0, didr);
	return ((didr >> 28) & 0xf) + 1;
}

static int get_num_brp_resources(void)
{
	u32 didr;
	ARM_DBG_READ(c0, 0, didr);
	return ((didr >> 24) & 0xf) + 1;
}

static int core_has_mismatch_brps(void)
{
	return (get_debug_arch() >= ARM_DEBUG_ARCH_V7_ECP14 &&
		get_num_brp_resources() > 1);
}

static int get_num_wrps(void)
{
	if (get_debug_arch() < ARM_DEBUG_ARCH_V7_1)
		return 1;

	return get_num_wrp_resources();
}

static int get_num_brps(void)
{
	int brps = get_num_brp_resources();
	return core_has_mismatch_brps() ? brps - 1 : brps;
}

static int halting_mode_enabled(void)
{
	u32 dscr;

	ARM_DBG_READ(c1, 0, dscr);

	if (WARN_ONCE(dscr & ARM_DSCR_HDBGEN,
		      "halting debug mode enabled. "
		      "Unable to access hardware resources.\n"))
		return -EPERM;
	return 0;
}

static int enable_monitor_mode(void)
{
	u32 dscr;
	int ret;

	ARM_DBG_READ(c1, 0, dscr);

	
	ret = halting_mode_enabled();
	if (ret)
		goto out;

	
	if (dscr & ARM_DSCR_MDBGEN)
		goto out;

	
	switch (get_debug_arch()) {
	case ARM_DEBUG_ARCH_V6:
	case ARM_DEBUG_ARCH_V6_1:
		ARM_DBG_WRITE(c1, 0, (dscr | ARM_DSCR_MDBGEN));
		break;
	case ARM_DEBUG_ARCH_V7_ECP14:
	case ARM_DEBUG_ARCH_V7_1:
		ARM_DBG_WRITE(c2, 2, (dscr | ARM_DSCR_MDBGEN));
		break;
	default:
		ret = -ENODEV;
		goto out;
	}

	
	ARM_DBG_READ(c1, 0, dscr);
	if (!(dscr & ARM_DSCR_MDBGEN))
		ret = -EPERM;

out:
	return ret;
}

int hw_breakpoint_slots(int type)
{
	if (!debug_arch_supported())
		return 0;

	switch (type) {
	case TYPE_INST:
		return get_num_brps();
	case TYPE_DATA:
		return get_num_wrps();
	default:
		pr_warning("unknown slot type: %d\n", type);
		return 0;
	}
}

static u8 get_max_wp_len(void)
{
	u32 ctrl_reg;
	struct arch_hw_breakpoint_ctrl ctrl;
	u8 size = 4;

	if (debug_arch < ARM_DEBUG_ARCH_V7_ECP14)
		goto out;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.len = ARM_BREAKPOINT_LEN_8;
	ctrl_reg = encode_ctrl_reg(ctrl);

	write_wb_reg(ARM_BASE_WVR, 0);
	write_wb_reg(ARM_BASE_WCR, ctrl_reg);
	if ((read_wb_reg(ARM_BASE_WCR) & ctrl_reg) == ctrl_reg)
		size = 8;

out:
	return size;
}

u8 arch_get_max_wp_len(void)
{
	return max_watchpoint_len;
}

int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	struct perf_event **slot, **slots;
	int i, max_slots, ctrl_base, val_base, ret = 0;
	u32 addr, ctrl;

	
	ret = enable_monitor_mode();
	if (ret)
		goto out;

	addr = info->address;
	ctrl = encode_ctrl_reg(info->ctrl) | 0x1;

	if (info->ctrl.type == ARM_BREAKPOINT_EXECUTE) {
		
		ctrl_base = ARM_BASE_BCR;
		val_base = ARM_BASE_BVR;
		slots = (struct perf_event **)__get_cpu_var(bp_on_reg);
		max_slots = core_num_brps;
	} else {
		
		ctrl_base = ARM_BASE_WCR;
		val_base = ARM_BASE_WVR;
		slots = (struct perf_event **)__get_cpu_var(wp_on_reg);
		max_slots = core_num_wrps;
	}

	for (i = 0; i < max_slots; ++i) {
		slot = &slots[i];

		if (!*slot) {
			*slot = bp;
			break;
		}
	}

	if (WARN_ONCE(i == max_slots, "Can't find any breakpoint slot\n")) {
		ret = -EBUSY;
		goto out;
	}

	
	if (info->step_ctrl.enabled) {
		addr = info->trigger & ~0x3;
		ctrl = encode_ctrl_reg(info->step_ctrl);
		if (info->ctrl.type != ARM_BREAKPOINT_EXECUTE) {
			i = 0;
			ctrl_base = ARM_BASE_BCR + core_num_brps;
			val_base = ARM_BASE_BVR + core_num_brps;
		}
	}

	
	write_wb_reg(val_base + i, addr);

	
	write_wb_reg(ctrl_base + i, ctrl);

out:
	return ret;
}

void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	struct perf_event **slot, **slots;
	int i, max_slots, base;

	if (info->ctrl.type == ARM_BREAKPOINT_EXECUTE) {
		
		base = ARM_BASE_BCR;
		slots = (struct perf_event **)__get_cpu_var(bp_on_reg);
		max_slots = core_num_brps;
	} else {
		
		base = ARM_BASE_WCR;
		slots = (struct perf_event **)__get_cpu_var(wp_on_reg);
		max_slots = core_num_wrps;
	}

	
	for (i = 0; i < max_slots; ++i) {
		slot = &slots[i];

		if (*slot == bp) {
			*slot = NULL;
			break;
		}
	}

	if (WARN_ONCE(i == max_slots, "Can't find any breakpoint slot\n"))
		return;

	
	if (info->ctrl.type != ARM_BREAKPOINT_EXECUTE &&
	    info->step_ctrl.enabled) {
		i = 0;
		base = ARM_BASE_BCR + core_num_brps;
	}

	
	write_wb_reg(base + i, 0);
}

static int get_hbp_len(u8 hbp_len)
{
	unsigned int len_in_bytes = 0;

	switch (hbp_len) {
	case ARM_BREAKPOINT_LEN_1:
		len_in_bytes = 1;
		break;
	case ARM_BREAKPOINT_LEN_2:
		len_in_bytes = 2;
		break;
	case ARM_BREAKPOINT_LEN_4:
		len_in_bytes = 4;
		break;
	case ARM_BREAKPOINT_LEN_8:
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
	len = get_hbp_len(info->ctrl.len);

	return (va >= TASK_SIZE) && ((va + len - 1) >= TASK_SIZE);
}

int arch_bp_generic_fields(struct arch_hw_breakpoint_ctrl ctrl,
			   int *gen_len, int *gen_type)
{
	
	switch (ctrl.type) {
	case ARM_BREAKPOINT_EXECUTE:
		*gen_type = HW_BREAKPOINT_X;
		break;
	case ARM_BREAKPOINT_LOAD:
		*gen_type = HW_BREAKPOINT_R;
		break;
	case ARM_BREAKPOINT_STORE:
		*gen_type = HW_BREAKPOINT_W;
		break;
	case ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE:
		*gen_type = HW_BREAKPOINT_RW;
		break;
	default:
		return -EINVAL;
	}

	
	switch (ctrl.len) {
	case ARM_BREAKPOINT_LEN_1:
		*gen_len = HW_BREAKPOINT_LEN_1;
		break;
	case ARM_BREAKPOINT_LEN_2:
		*gen_len = HW_BREAKPOINT_LEN_2;
		break;
	case ARM_BREAKPOINT_LEN_4:
		*gen_len = HW_BREAKPOINT_LEN_4;
		break;
	case ARM_BREAKPOINT_LEN_8:
		*gen_len = HW_BREAKPOINT_LEN_8;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int arch_build_bp_info(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	
	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_X:
		info->ctrl.type = ARM_BREAKPOINT_EXECUTE;
		break;
	case HW_BREAKPOINT_R:
		info->ctrl.type = ARM_BREAKPOINT_LOAD;
		break;
	case HW_BREAKPOINT_W:
		info->ctrl.type = ARM_BREAKPOINT_STORE;
		break;
	case HW_BREAKPOINT_RW:
		info->ctrl.type = ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE;
		break;
	default:
		return -EINVAL;
	}

	
	switch (bp->attr.bp_len) {
	case HW_BREAKPOINT_LEN_1:
		info->ctrl.len = ARM_BREAKPOINT_LEN_1;
		break;
	case HW_BREAKPOINT_LEN_2:
		info->ctrl.len = ARM_BREAKPOINT_LEN_2;
		break;
	case HW_BREAKPOINT_LEN_4:
		info->ctrl.len = ARM_BREAKPOINT_LEN_4;
		break;
	case HW_BREAKPOINT_LEN_8:
		info->ctrl.len = ARM_BREAKPOINT_LEN_8;
		if ((info->ctrl.type != ARM_BREAKPOINT_EXECUTE)
			&& max_watchpoint_len >= 8)
			break;
	default:
		return -EINVAL;
	}

	if (info->ctrl.type == ARM_BREAKPOINT_EXECUTE &&
	    info->ctrl.len != ARM_BREAKPOINT_LEN_2 &&
	    info->ctrl.len != ARM_BREAKPOINT_LEN_4)
		return -EINVAL;

	
	info->address = bp->attr.bp_addr;

	
	info->ctrl.privilege = ARM_BREAKPOINT_USER;
	if (arch_check_bp_in_kernelspace(bp))
		info->ctrl.privilege |= ARM_BREAKPOINT_PRIV;

	
	info->ctrl.enabled = !bp->attr.disabled;

	
	info->ctrl.mismatch = 0;

	return 0;
}

int arch_validate_hwbkpt_settings(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	int ret = 0;
	u32 offset, alignment_mask = 0x3;

	
	ret = arch_build_bp_info(bp);
	if (ret)
		goto out;

	
	if (info->ctrl.len == ARM_BREAKPOINT_LEN_8)
		alignment_mask = 0x7;
	offset = info->address & alignment_mask;
	switch (offset) {
	case 0:
		
		break;
	case 1:
		
		if (info->ctrl.len == ARM_BREAKPOINT_LEN_1)
			break;
	case 2:
		
		if (info->ctrl.len == ARM_BREAKPOINT_LEN_2)
			break;
	default:
		ret = -EINVAL;
		goto out;
	}

	info->address &= ~alignment_mask;
	info->ctrl.len <<= offset;

	if (!bp->overflow_handler && (arch_check_bp_in_kernelspace(bp) ||
	    !core_has_mismatch_brps() || !bp->hw.bp_target)) {
		pr_warning("overflow handler required but none found\n");
		ret = -EINVAL;
	}
out:
	return ret;
}

static void enable_single_step(struct perf_event *bp, u32 addr)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	arch_uninstall_hw_breakpoint(bp);
	info->step_ctrl.mismatch  = 1;
	info->step_ctrl.len	  = ARM_BREAKPOINT_LEN_4;
	info->step_ctrl.type	  = ARM_BREAKPOINT_EXECUTE;
	info->step_ctrl.privilege = info->ctrl.privilege;
	info->step_ctrl.enabled	  = 1;
	info->trigger		  = addr;
	arch_install_hw_breakpoint(bp);
}

static void disable_single_step(struct perf_event *bp)
{
	arch_uninstall_hw_breakpoint(bp);
	counter_arch_bp(bp)->step_ctrl.enabled = 0;
	arch_install_hw_breakpoint(bp);
}

static void watchpoint_handler(unsigned long addr, unsigned int fsr,
			       struct pt_regs *regs)
{
	int i, access;
	u32 val, ctrl_reg, alignment_mask;
	struct perf_event *wp, **slots;
	struct arch_hw_breakpoint *info;
	struct arch_hw_breakpoint_ctrl ctrl;

	slots = (struct perf_event **)__get_cpu_var(wp_on_reg);

	for (i = 0; i < core_num_wrps; ++i) {
		rcu_read_lock();

		wp = slots[i];

		if (wp == NULL)
			goto unlock;

		info = counter_arch_bp(wp);
		if (debug_arch < ARM_DEBUG_ARCH_V7_1) {
			BUG_ON(i > 0);
			info->trigger = wp->attr.bp_addr;
		} else {
			if (info->ctrl.len == ARM_BREAKPOINT_LEN_8)
				alignment_mask = 0x7;
			else
				alignment_mask = 0x3;

			
			val = read_wb_reg(ARM_BASE_WVR + i);
			if (val != (addr & ~alignment_mask))
				goto unlock;

			
			ctrl_reg = read_wb_reg(ARM_BASE_WCR + i);
			decode_ctrl_reg(ctrl_reg, &ctrl);
			if (!((1 << (addr & alignment_mask)) & ctrl.len))
				goto unlock;

			
			access = (fsr & ARM_FSR_ACCESS_MASK) ? HW_BREAKPOINT_W :
				 HW_BREAKPOINT_R;
			if (!(access & hw_breakpoint_type(wp)))
				goto unlock;

			
			info->trigger = addr;
		}

		pr_debug("watchpoint fired: address = 0x%x\n", info->trigger);
		perf_bp_event(wp, regs);

		if (!wp->overflow_handler)
			enable_single_step(wp, instruction_pointer(regs));

unlock:
		rcu_read_unlock();
	}
}

static void watchpoint_single_step_handler(unsigned long pc)
{
	int i;
	struct perf_event *wp, **slots;
	struct arch_hw_breakpoint *info;

	slots = (struct perf_event **)__get_cpu_var(wp_on_reg);

	for (i = 0; i < core_num_wrps; ++i) {
		rcu_read_lock();

		wp = slots[i];

		if (wp == NULL)
			goto unlock;

		info = counter_arch_bp(wp);
		if (!info->step_ctrl.enabled)
			goto unlock;

		if (info->trigger != pc)
			disable_single_step(wp);

unlock:
		rcu_read_unlock();
	}
}

static void breakpoint_handler(unsigned long unknown, struct pt_regs *regs)
{
	int i;
	u32 ctrl_reg, val, addr;
	struct perf_event *bp, **slots;
	struct arch_hw_breakpoint *info;
	struct arch_hw_breakpoint_ctrl ctrl;

	slots = (struct perf_event **)__get_cpu_var(bp_on_reg);

	
	addr = regs->ARM_pc;

	
	for (i = 0; i < core_num_brps; ++i) {
		rcu_read_lock();

		bp = slots[i];

		if (bp == NULL)
			goto unlock;

		info = counter_arch_bp(bp);

		
		val = read_wb_reg(ARM_BASE_BVR + i);
		if (val != (addr & ~0x3))
			goto mismatch;

		
		ctrl_reg = read_wb_reg(ARM_BASE_BCR + i);
		decode_ctrl_reg(ctrl_reg, &ctrl);
		if ((1 << (addr & 0x3)) & ctrl.len) {
			info->trigger = addr;
			pr_debug("breakpoint fired: address = 0x%x\n", addr);
			perf_bp_event(bp, regs);
			if (!bp->overflow_handler)
				enable_single_step(bp, addr);
			goto unlock;
		}

mismatch:
		
		if (info->step_ctrl.enabled)
			disable_single_step(bp);
unlock:
		rcu_read_unlock();
	}

	
	watchpoint_single_step_handler(addr);
}

static int hw_breakpoint_pending(unsigned long addr, unsigned int fsr,
				 struct pt_regs *regs)
{
	int ret = 0;
	u32 dscr;

	preempt_disable();

	if (interrupts_enabled(regs))
		local_irq_enable();

	
	ARM_DBG_READ(c1, 0, dscr);

	
	switch (ARM_DSCR_MOE(dscr)) {
	case ARM_ENTRY_BREAKPOINT:
		breakpoint_handler(addr, regs);
		break;
	case ARM_ENTRY_ASYNC_WATCHPOINT:
		WARN(1, "Asynchronous watchpoint exception taken. Debugging results may be unreliable\n");
	case ARM_ENTRY_SYNC_WATCHPOINT:
		watchpoint_handler(addr, fsr, regs);
		break;
	default:
		ret = 1; 
	}

	preempt_enable();

	return ret;
}

static cpumask_t debug_err_mask;

static int debug_reg_trap(struct pt_regs *regs, unsigned int instr)
{
	int cpu = smp_processor_id();

	pr_warning("Debug register access (0x%x) caused undefined instruction on CPU %d\n",
		   instr, cpu);

	
	cpumask_set_cpu(cpu, &debug_err_mask);
	instruction_pointer(regs) += 4;
	return 0;
}

static struct undef_hook debug_reg_hook = {
	.instr_mask	= 0x0fe80f10,
	.instr_val	= 0x0e000e10,
	.fn		= debug_reg_trap,
};

static void reset_ctrl_regs(void *unused)
{
	int i, raw_num_brps, err = 0, cpu = smp_processor_id();
	u32 dbg_power;

	switch (debug_arch) {
	case ARM_DEBUG_ARCH_V6:
	case ARM_DEBUG_ARCH_V6_1:
		
		goto reset_regs;
	case ARM_DEBUG_ARCH_V7_ECP14:
		asm volatile("mrc p14, 0, %0, c1, c5, 4" : "=r" (dbg_power));
		if ((dbg_power & 0x1) == 0)
			err = -EPERM;
		break;
	case ARM_DEBUG_ARCH_V7_1:
		asm volatile("mrc p14, 0, %0, c1, c3, 4" : "=r" (dbg_power));
		if ((dbg_power & 0x1) == 1)
			err = -EPERM;
		break;
	}

	if (err) {
		pr_warning("CPU %d debug is powered down!\n", cpu);
		cpumask_or(&debug_err_mask, &debug_err_mask, cpumask_of(cpu));
		return;
	}

	asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (0));
	isb();

	asm volatile("mcr p14, 0, %0, c0, c7, 0" : : "r" (0));
	isb();

reset_regs:
	if (halting_mode_enabled())
		return;

	
	raw_num_brps = get_num_brp_resources();
	for (i = 0; i < raw_num_brps; ++i) {
		write_wb_reg(ARM_BASE_BCR + i, 0UL);
		write_wb_reg(ARM_BASE_BVR + i, 0UL);
	}

	for (i = 0; i < core_num_wrps; ++i) {
		write_wb_reg(ARM_BASE_WCR + i, 0UL);
		write_wb_reg(ARM_BASE_WVR + i, 0UL);
	}
	enable_monitor_mode();
}

static int __cpuinit dbg_reset_notify(struct notifier_block *self,
				      unsigned long action, void *cpu)
{
	if (action == CPU_ONLINE)
		smp_call_function_single((int)cpu, reset_ctrl_regs, NULL, 1);

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata dbg_reset_nb = {
	.notifier_call = dbg_reset_notify,
};

static int __init arch_hw_breakpoint_init(void)
{
	u32 dscr;

	debug_arch = get_debug_arch();

	if (!debug_arch_supported()) {
		pr_info("debug architecture 0x%x unsupported.\n", debug_arch);
		return 0;
	}

	
	core_num_brps = get_num_brps();
	core_num_wrps = get_num_wrps();

	register_undef_hook(&debug_reg_hook);

	on_each_cpu(reset_ctrl_regs, NULL, 1);
	unregister_undef_hook(&debug_reg_hook);
	if (!cpumask_empty(&debug_err_mask)) {
		core_num_brps = 0;
		core_num_wrps = 0;
		return 0;
	}

	pr_info("found %d " "%s" "breakpoint and %d watchpoint registers.\n",
		core_num_brps, core_has_mismatch_brps() ? "(+1 reserved) " :
		"", core_num_wrps);

	ARM_DBG_READ(c1, 0, dscr);
	if (dscr & ARM_DSCR_HDBGEN) {
		max_watchpoint_len = 4;
		pr_warning("halting debug mode enabled. Assuming maximum watchpoint size of %u bytes.\n",
			   max_watchpoint_len);
	} else {
		
		max_watchpoint_len = get_max_wp_len();
		pr_info("maximum watchpoint size is %u bytes.\n",
				max_watchpoint_len);
	}

	
	hook_fault_code(FAULT_CODE_DEBUG, hw_breakpoint_pending, SIGTRAP,
			TRAP_HWBKPT, "watchpoint debug exception");
	hook_ifault_code(FAULT_CODE_DEBUG, hw_breakpoint_pending, SIGTRAP,
			TRAP_HWBKPT, "breakpoint debug exception");

	
	register_cpu_notifier(&dbg_reset_nb);
	return 0;
}
arch_initcall(arch_hw_breakpoint_init);

void hw_breakpoint_pmu_read(struct perf_event *bp)
{
}

int hw_breakpoint_exceptions_notify(struct notifier_block *unused,
					unsigned long val, void *data)
{
	return NOTIFY_DONE;
}
