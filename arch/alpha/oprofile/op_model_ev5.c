/**
 * @file arch/alpha/oprofile/op_model_ev5.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Richard Henderson <rth@twiddle.net>
 */

#include <linux/oprofile.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <asm/ptrace.h>

#include "op_impl.h"



static void
common_reg_setup(struct op_register_config *reg,
		 struct op_counter_config *ctr,
		 struct op_system_config *sys,
		 int cbox1_ofs, int cbox2_ofs)
{
	int i, ctl, reset, need_reset;


	ctl = 0;
	for (i = 0; i < 3; ++i) {
		unsigned long event = ctr[i].event;
		if (!ctr[i].enabled)
			continue;

		
		if (i == 2) {
			if (event == 0)
				event = 12+48;
			else if (event == 2+41)
				event = 4+65;
		}

		
		if (event < 2)
			ctl |= event << 31;
		else if (event < 24)
			;
		else if (event < 40)
			ctl |= (event - 24) << 4;
		else if (event < 48)
			ctl |= (event - 40) << cbox1_ofs | 15 << 4;
		else if (event < 64)
			ctl |= event - 48;
		else if (event < 72)
			ctl |= (event - 64) << cbox2_ofs | 15;
	}
	reg->mux_select = ctl;

	
	ctl = 0;
	ctl |= !sys->enable_pal << 9;
	ctl |= !sys->enable_kernel << 8;
	ctl |= !sys->enable_user << 30;
	reg->proc_mode = ctl;


	ctl = reset = need_reset = 0;
	for (i = 0; i < 3; ++i) {
		unsigned long max, hilo, count = ctr[i].count;
		if (!ctr[i].enabled)
			continue;

		if (count <= 256)
			count = 256, hilo = 3, max = 256;
		else {
			max = (i == 2 ? 16384 : 65536);
			hilo = 2;
			if (count > max)
				count = max;
		}
		ctr[i].count = count;

		ctl |= hilo << (8 - i*2);
		reset |= (max - count) << (48 - 16*i);
		if (count != max)
			need_reset |= 1 << i;
	}
	reg->freq = ctl;
	reg->reset_values = reset;
	reg->need_reset = need_reset;
}

static void
ev5_reg_setup(struct op_register_config *reg,
	      struct op_counter_config *ctr,
	      struct op_system_config *sys)
{
	common_reg_setup(reg, ctr, sys, 19, 22);
}

static void
pca56_reg_setup(struct op_register_config *reg,
	        struct op_counter_config *ctr,
	        struct op_system_config *sys)
{
	common_reg_setup(reg, ctr, sys, 8, 11);
}


static void
ev5_cpu_setup (void *x)
{
	struct op_register_config *reg = x;

	wrperfmon(2, reg->mux_select);
	wrperfmon(3, reg->proc_mode);
	wrperfmon(4, reg->freq);
	wrperfmon(6, reg->reset_values);
}


static void
ev5_reset_ctr(struct op_register_config *reg, unsigned long ctr)
{
	unsigned long values, mask, not_pk, reset_values;

	mask = (ctr == 0 ? 0xfffful << 48
	        : ctr == 1 ? 0xfffful << 32
		: 0x3fff << 16);

	not_pk = 1 << 9 | 1 << 8;

	reset_values = reg->reset_values;

	if ((reg->proc_mode & not_pk) == not_pk) {
		values = wrperfmon(5, 0);
		values = (reset_values & mask) | (values & ~mask & -2);
		wrperfmon(6, values);
	} else {
		wrperfmon(0, -1);
		values = wrperfmon(5, 0);
		values = (reset_values & mask) | (values & ~mask & -2);
		wrperfmon(6, values);
		wrperfmon(1, reg->enable);
	}
}

static void
ev5_handle_interrupt(unsigned long which, struct pt_regs *regs,
		     struct op_counter_config *ctr)
{
	
	oprofile_add_sample(regs, which);
}


struct op_axp_model op_model_ev5 = {
	.reg_setup		= ev5_reg_setup,
	.cpu_setup		= ev5_cpu_setup,
	.reset_ctr		= ev5_reset_ctr,
	.handle_interrupt	= ev5_handle_interrupt,
	.cpu_type		= "alpha/ev5",
	.num_counters		= 3,
	.can_set_proc_mode	= 1,
};

struct op_axp_model op_model_pca56 = {
	.reg_setup		= pca56_reg_setup,
	.cpu_setup		= ev5_cpu_setup,
	.reset_ctr		= ev5_reset_ctr,
	.handle_interrupt	= ev5_handle_interrupt,
	.cpu_type		= "alpha/pca56",
	.num_counters		= 3,
	.can_set_proc_mode	= 1,
};
