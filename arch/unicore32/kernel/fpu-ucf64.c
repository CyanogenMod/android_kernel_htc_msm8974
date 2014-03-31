/*
 * linux/arch/unicore32/kernel/fpu-ucf64.c
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/fpu-ucf64.h>

#define F64_NAN_FLAG	0x100

#define F64_EXCEPTION_ERROR	((u32)-1 & ~F64_NAN_FLAG)

#define f64reg(_f64_) #_f64_

#define cff(_f64_) ({			\
	u32 __v;			\
	asm("cff %0, " f64reg(_f64_) "@ fmrx	%0, " #_f64_	\
	    : "=r" (__v) : : "cc");	\
	__v;				\
	})

#define ctf(_f64_, _var_)		\
	asm("ctf %0, " f64reg(_f64_) "@ fmxr	" #_f64_ ", %0"	\
	   : : "r" (_var_) : "cc")

void ucf64_raise_sigfpe(unsigned int sicode, struct pt_regs *regs)
{
	siginfo_t info;

	memset(&info, 0, sizeof(info));

	info.si_signo = SIGFPE;
	info.si_code = sicode;
	info.si_addr = (void __user *)(instruction_pointer(regs) - 4);

	current->thread.error_code = 0;
	current->thread.trap_no = 6;

	send_sig_info(SIGFPE, &info, current);
}

void ucf64_exchandler(u32 inst, u32 fpexc, struct pt_regs *regs)
{
	u32 tmp = fpexc;
	u32 exc = F64_EXCEPTION_ERROR & fpexc;

	pr_debug("UniCore-F64: instruction %08x fpscr %08x\n",
			inst, fpexc);

	if (exc & FPSCR_CMPINSTR_BIT) {
		if (exc & FPSCR_CON)
			tmp |= FPSCR_CON;
		else
			tmp &= ~(FPSCR_CON);
		exc &= ~(FPSCR_CMPINSTR_BIT | FPSCR_CON);
	} else {
		pr_debug(KERN_ERR "UniCore-F64 Error: unhandled exceptions\n");
		pr_debug(KERN_ERR "UniCore-F64 FPSCR 0x%08x INST 0x%08x\n",
				cff(FPSCR), inst);

		ucf64_raise_sigfpe(0, regs);
		return;
	}

	tmp &= ~(FPSCR_TRAP | FPSCR_IOS | FPSCR_OFS | FPSCR_UFS |
			FPSCR_IXS | FPSCR_HIS | FPSCR_IOC | FPSCR_OFC |
			FPSCR_UFC | FPSCR_IXC | FPSCR_HIC);

	tmp |= exc;
	ctf(FPSCR, tmp);
}

static int __init ucf64_init(void)
{
	ctf(FPSCR, 0x0);     

	printk(KERN_INFO "Enable UniCore-F64 support.\n");

	return 0;
}

late_initcall(ucf64_init);
