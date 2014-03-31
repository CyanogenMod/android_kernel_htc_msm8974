/**
 * @file arch/alpha/oprofile/op_model_ev67.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Richard Henderson <rth@twiddle.net>
 * @author Falk Hueffner <falk@debian.org>
 */

#include <linux/oprofile.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <asm/ptrace.h>

#include "op_impl.h"



static void
ev67_reg_setup(struct op_register_config *reg,
	       struct op_counter_config *ctr,
	       struct op_system_config *sys)
{
	unsigned long ctl, reset, need_reset, i;

	
	ctl = 1UL << 4;		

	if (ctr[1].enabled) {
		ctl |= (ctr[1].event & 3) << 2;
	} else {
		if (ctr[0].event == 0) 
			ctl |= 1UL << 2;
	}
	reg->mux_select = ctl;

	
	reg->proc_mode = 0;

	reset = need_reset = 0;
	for (i = 0; i < 2; ++i) {
		unsigned long count = ctr[i].count;
		if (!ctr[i].enabled)
			continue;

		if (count > 0x100000)
			count = 0x100000;
		ctr[i].count = count;
		reset |= (0x100000 - count) << (i ? 6 : 28);
		if (count != 0x100000)
			need_reset |= 1 << i;
	}
	reg->reset_values = reset;
	reg->need_reset = need_reset;
}


static void
ev67_cpu_setup (void *x)
{
	struct op_register_config *reg = x;

	wrperfmon(2, reg->mux_select);
	wrperfmon(3, reg->proc_mode);
	wrperfmon(6, reg->reset_values | 3);
}


static void
ev67_reset_ctr(struct op_register_config *reg, unsigned long ctr)
{
	wrperfmon(6, reg->reset_values | (1 << ctr));
}


enum profileme_counters {
	PM_STALLED,		
	PM_TAKEN,		
	PM_MISPREDICT,		
	PM_ITB_MISS,		
	PM_DTB_MISS,		
	PM_REPLAY,		
	PM_LOAD_STORE,		
	PM_ICACHE_MISS,		
	PM_UNALIGNED,		
	PM_NUM_COUNTERS
};

static inline void
op_add_pm(unsigned long pc, int kern, unsigned long counter,
	  struct op_counter_config *ctr, unsigned long event)
{
	unsigned long fake_counter = 2 + event;
	if (counter == 1)
		fake_counter += PM_NUM_COUNTERS;
	if (ctr[fake_counter].enabled)
		oprofile_add_pc(pc, kern, fake_counter);
}

static void
ev67_handle_interrupt(unsigned long which, struct pt_regs *regs,
		      struct op_counter_config *ctr)
{
	unsigned long pmpc, pctr_ctl;
	int kern = !user_mode(regs);
	int mispredict = 0;
	union {
		unsigned long v;
		struct {
			unsigned reserved:	30; 
			unsigned overcount:	 3; 
			unsigned icache_miss:	 1; 
			unsigned trap_type:	 4; 
			unsigned load_store:	 1; 
			unsigned trap:		 1; 
			unsigned mispredict:	 1; 
		} fields;
	} i_stat;

	enum trap_types {
		TRAP_REPLAY,
		TRAP_INVALID0,
		TRAP_DTB_DOUBLE_MISS_3,
		TRAP_DTB_DOUBLE_MISS_4,
		TRAP_FP_DISABLED,
		TRAP_UNALIGNED,
		TRAP_DTB_SINGLE_MISS,
		TRAP_DSTREAM_FAULT,
		TRAP_OPCDEC,
		TRAP_INVALID1,
		TRAP_MACHINE_CHECK,
		TRAP_INVALID2,
		TRAP_ARITHMETIC,
		TRAP_INVALID3,
		TRAP_MT_FPCR,
		TRAP_RESET
	};

	pmpc = wrperfmon(9, 0);
	
	if (pmpc & 1)
		return;
	pmpc &= ~2;		

	i_stat.v = wrperfmon(8, 0);
	if (i_stat.fields.trap) {
		switch (i_stat.fields.trap_type) {
		case TRAP_INVALID1:
		case TRAP_INVALID2:
		case TRAP_INVALID3:
			oprofile_add_pc(regs->pc, kern, which);
			if ((pmpc & ((1 << 15) - 1)) ==  581)
				op_add_pm(regs->pc, kern, which,
					  ctr, PM_ITB_MISS);
			return;
		case TRAP_REPLAY:
			op_add_pm(pmpc, kern, which, ctr,
				  (i_stat.fields.load_store
				   ? PM_LOAD_STORE : PM_REPLAY));
			break;
		case TRAP_DTB_DOUBLE_MISS_3:
		case TRAP_DTB_DOUBLE_MISS_4:
		case TRAP_DTB_SINGLE_MISS:
			op_add_pm(pmpc, kern, which, ctr, PM_DTB_MISS);
			break;
		case TRAP_UNALIGNED:
			op_add_pm(pmpc, kern, which, ctr, PM_UNALIGNED);
			break;
		case TRAP_INVALID0:
		case TRAP_FP_DISABLED:
		case TRAP_DSTREAM_FAULT:
		case TRAP_OPCDEC:
		case TRAP_MACHINE_CHECK:
		case TRAP_ARITHMETIC:
		case TRAP_MT_FPCR:
		case TRAP_RESET:
			break;
		}

		if (i_stat.fields.mispredict) {
			mispredict = 1;
			op_add_pm(pmpc, kern, which, ctr, PM_MISPREDICT);
		}
	}

	oprofile_add_pc(pmpc, kern, which);

	pctr_ctl = wrperfmon(5, 0);
	if (pctr_ctl & (1UL << 27))
		op_add_pm(pmpc, kern, which, ctr, PM_STALLED);

	if (!mispredict && pctr_ctl & (1UL << 0))
		op_add_pm(pmpc, kern, which, ctr, PM_TAKEN);
}

struct op_axp_model op_model_ev67 = {
	.reg_setup		= ev67_reg_setup,
	.cpu_setup		= ev67_cpu_setup,
	.reset_ctr		= ev67_reset_ctr,
	.handle_interrupt	= ev67_handle_interrupt,
	.cpu_type		= "alpha/ev67",
	.num_counters		= 20,
	.can_set_proc_mode	= 0,
};
