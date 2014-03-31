/*
 * Copyright (C) 2004 Anton Blanchard <anton@au.ibm.com>, IBM
 * Added mmcra[slot] support:
 * Copyright (C) 2006-2007 Will Schmidt <willschm@us.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/oprofile.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <asm/firmware.h>
#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/cputable.h>
#include <asm/rtas.h>
#include <asm/oprofile_impl.h>
#include <asm/reg.h>

#define dbg(args...)

static unsigned long reset_value[OP_MAX_COUNTER];

static int oprofile_running;
static int use_slot_nums;

static u32 mmcr0_val;
static u64 mmcr1_val;
static u64 mmcra_val;

static int power4_reg_setup(struct op_counter_config *ctr,
			     struct op_system_config *sys,
			     int num_ctrs)
{
	int i;

	mmcr0_val = sys->mmcr0;
	mmcr1_val = sys->mmcr1;
	mmcra_val = sys->mmcra;

	for (i = 0; i < cur_cpu_spec->num_pmcs; ++i)
		reset_value[i] = 0x80000000UL - ctr[i].count;

	
	if (sys->enable_kernel)
		mmcr0_val &= ~MMCR0_KERNEL_DISABLE;
	else
		mmcr0_val |= MMCR0_KERNEL_DISABLE;

	if (sys->enable_user)
		mmcr0_val &= ~MMCR0_PROBLEM_DISABLE;
	else
		mmcr0_val |= MMCR0_PROBLEM_DISABLE;

	if (__is_processor(PV_POWER4) || __is_processor(PV_POWER4p) ||
	    __is_processor(PV_970) || __is_processor(PV_970FX) ||
	    __is_processor(PV_970MP) || __is_processor(PV_970GX) ||
	    __is_processor(PV_POWER5) || __is_processor(PV_POWER5p))
		use_slot_nums = 1;

	return 0;
}

extern void ppc_enable_pmcs(void);

static inline int mmcra_must_set_sample(void)
{
	if (__is_processor(PV_POWER4) || __is_processor(PV_POWER4p) ||
	    __is_processor(PV_970) || __is_processor(PV_970FX) ||
	    __is_processor(PV_970MP) || __is_processor(PV_970GX))
		return 1;

	return 0;
}

static int power4_cpu_setup(struct op_counter_config *ctr)
{
	unsigned int mmcr0 = mmcr0_val;
	unsigned long mmcra = mmcra_val;

	ppc_enable_pmcs();

	
	mmcr0 |= MMCR0_FC;
	mtspr(SPRN_MMCR0, mmcr0);

	mmcr0 |= MMCR0_FCM1|MMCR0_PMXE|MMCR0_FCECE;
	mmcr0 |= MMCR0_PMC1CE|MMCR0_PMCjCE;
	mtspr(SPRN_MMCR0, mmcr0);

	mtspr(SPRN_MMCR1, mmcr1_val);

	if (mmcra_must_set_sample())
		mmcra |= MMCRA_SAMPLE_ENABLE;
	mtspr(SPRN_MMCRA, mmcra);

	dbg("setup on cpu %d, mmcr0 %lx\n", smp_processor_id(),
	    mfspr(SPRN_MMCR0));
	dbg("setup on cpu %d, mmcr1 %lx\n", smp_processor_id(),
	    mfspr(SPRN_MMCR1));
	dbg("setup on cpu %d, mmcra %lx\n", smp_processor_id(),
	    mfspr(SPRN_MMCRA));

	return 0;
}

static int power4_start(struct op_counter_config *ctr)
{
	int i;
	unsigned int mmcr0;

	
	mtmsrd(mfmsr() | MSR_PMM);

	for (i = 0; i < cur_cpu_spec->num_pmcs; ++i) {
		if (ctr[i].enabled) {
			classic_ctr_write(i, reset_value[i]);
		} else {
			classic_ctr_write(i, 0);
		}
	}

	mmcr0 = mfspr(SPRN_MMCR0);

	mmcr0 &= ~MMCR0_PMAO;

	mmcr0 &= ~MMCR0_FC;
	mtspr(SPRN_MMCR0, mmcr0);

	oprofile_running = 1;

	dbg("start on cpu %d, mmcr0 %x\n", smp_processor_id(), mmcr0);
	return 0;
}

static void power4_stop(void)
{
	unsigned int mmcr0;

	
	mmcr0 = mfspr(SPRN_MMCR0);
	mmcr0 |= MMCR0_FC;
	mtspr(SPRN_MMCR0, mmcr0);

	oprofile_running = 0;

	dbg("stop on cpu %d, mmcr0 %x\n", smp_processor_id(), mmcr0);

	mb();
}

static void __used hypervisor_bucket(void)
{
}

static void __used rtas_bucket(void)
{
}

static void __used kernel_unknown_bucket(void)
{
}

static unsigned long get_pc(struct pt_regs *regs)
{
	unsigned long pc = mfspr(SPRN_SIAR);
	unsigned long mmcra;
	unsigned long slot;

	
	if (!cur_cpu_spec->oprofile_mmcra_sihv)
		return pc;

	mmcra = mfspr(SPRN_MMCRA);

	if (use_slot_nums && (mmcra & MMCRA_SAMPLE_ENABLE)) {
		slot = ((mmcra & MMCRA_SLOT) >> MMCRA_SLOT_SHIFT);
		if (slot > 1)
			pc += 4 * (slot - 1);
	}

	
	if (firmware_has_feature(FW_FEATURE_LPAR) &&
	    (mmcra & cur_cpu_spec->oprofile_mmcra_sihv))
		
		return *((unsigned long *)hypervisor_bucket);

	
	if (mmcra & cur_cpu_spec->oprofile_mmcra_sipr)
		return pc;

#ifdef CONFIG_PPC_RTAS
	
	if (pc >= rtas.base && pc < (rtas.base + rtas.size))
		
		return *((unsigned long *)rtas_bucket);
#endif

	
	if (pc < 0x1000000UL)
		return (unsigned long)__va(pc);

	
	if (!is_kernel_addr(pc))
		
		return *((unsigned long *)kernel_unknown_bucket);

	return pc;
}

static int get_kernel(unsigned long pc, unsigned long mmcra)
{
	int is_kernel;

	if (!cur_cpu_spec->oprofile_mmcra_sihv) {
		is_kernel = is_kernel_addr(pc);
	} else {
		is_kernel = ((mmcra & cur_cpu_spec->oprofile_mmcra_sipr) == 0);
	}

	return is_kernel;
}

static bool pmc_overflow(unsigned long val)
{
	if ((int)val < 0)
		return true;

	if (__is_processor(PV_POWER7) && ((0x80000000 - val) <= 256))
		return true;

	return false;
}

static void power4_handle_interrupt(struct pt_regs *regs,
				    struct op_counter_config *ctr)
{
	unsigned long pc;
	int is_kernel;
	int val;
	int i;
	unsigned int mmcr0;
	unsigned long mmcra;

	mmcra = mfspr(SPRN_MMCRA);

	pc = get_pc(regs);
	is_kernel = get_kernel(pc, mmcra);

	
	mtmsrd(mfmsr() | MSR_PMM);

	for (i = 0; i < cur_cpu_spec->num_pmcs; ++i) {
		val = classic_ctr_read(i);
		if (pmc_overflow(val)) {
			if (oprofile_running && ctr[i].enabled) {
				oprofile_add_ext_sample(pc, regs, i, is_kernel);
				classic_ctr_write(i, reset_value[i]);
			} else {
				classic_ctr_write(i, 0);
			}
		}
	}

	mmcr0 = mfspr(SPRN_MMCR0);

	
	mmcr0 |= MMCR0_PMXE;

	mmcr0 &= ~MMCR0_PMAO;

	
	mmcra &= ~cur_cpu_spec->oprofile_mmcra_clear;
	mtspr(SPRN_MMCRA, mmcra);

	mmcr0 &= ~MMCR0_FC;
	mtspr(SPRN_MMCR0, mmcr0);
}

struct op_powerpc_model op_model_power4 = {
	.reg_setup		= power4_reg_setup,
	.cpu_setup		= power4_cpu_setup,
	.start			= power4_start,
	.stop			= power4_stop,
	.handle_interrupt	= power4_handle_interrupt,
};
