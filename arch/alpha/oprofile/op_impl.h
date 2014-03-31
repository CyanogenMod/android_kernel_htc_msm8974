/**
 * @file arch/alpha/oprofile/op_impl.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Richard Henderson <rth@twiddle.net>
 */

#ifndef OP_IMPL_H
#define OP_IMPL_H 1

struct op_counter_config {
	unsigned long enabled;
	unsigned long event;
	unsigned long count;
	
	unsigned long kernel;
	unsigned long user;
	unsigned long unit_mask;
};

struct op_system_config {
	unsigned long enable_pal;
	unsigned long enable_kernel;
	unsigned long enable_user;
};

struct op_register_config {
	unsigned long enable;
	unsigned long mux_select;
	unsigned long proc_mode;
	unsigned long freq;
	unsigned long reset_values;
	unsigned long need_reset;
};

struct op_axp_model {
	void (*reg_setup) (struct op_register_config *,
			   struct op_counter_config *,
			   struct op_system_config *);
	void (*cpu_setup) (void *);
	void (*reset_ctr) (struct op_register_config *, unsigned long);
	void (*handle_interrupt) (unsigned long, struct pt_regs *,
				  struct op_counter_config *);
	char *cpu_type;
	unsigned char num_counters;
	unsigned char can_set_proc_mode;
};

#endif
