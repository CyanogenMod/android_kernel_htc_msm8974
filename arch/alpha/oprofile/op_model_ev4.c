/**
 * @file arch/alpha/oprofile/op_model_ev4.c
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
ev4_reg_setup(struct op_register_config *reg,
	      struct op_counter_config *ctr,
	      struct op_system_config *sys)
{
	unsigned long ctl = 0, count, hilo;


	ctl |= (ctr[0].enabled ? ctr[0].event << 8 : 14 << 8);
	ctl |= (ctr[1].enabled ? (ctr[1].event - 16) << 32 : 7ul << 32);


	count = ctr[0].count;
	if (count <= 4096)
		count = 4096, hilo = 1;
	else
		count = 65536, hilo = 0;
	ctr[0].count = count;
	ctl |= (ctr[0].enabled && hilo) << 3;

	count = ctr[1].count;
	if (count <= 256)
		count = 256, hilo = 1;
	else
		count = 4096, hilo = 0;
	ctr[1].count = count;
	ctl |= (ctr[1].enabled && hilo);

	reg->mux_select = ctl;

	
	reg->proc_mode = 0;

	
	reg->freq = 0;

	
	reg->reset_values = 0;
	reg->need_reset = 0;

}


static void
ev4_cpu_setup(void *x)
{
	struct op_register_config *reg = x;

	wrperfmon(2, reg->mux_select);
	wrperfmon(3, reg->proc_mode);
}

static void
ev4_handle_interrupt(unsigned long which, struct pt_regs *regs,
		     struct op_counter_config *ctr)
{
	if (!ctr[which].enabled)
		return;

	
	oprofile_add_sample(regs, which);
}


struct op_axp_model op_model_ev4 = {
	.reg_setup		= ev4_reg_setup,
	.cpu_setup		= ev4_cpu_setup,
	.reset_ctr		= NULL,
	.handle_interrupt	= ev4_handle_interrupt,
	.cpu_type		= "alpha/ev4",
	.num_counters		= 2,
	.can_set_proc_mode	= 0,
};
