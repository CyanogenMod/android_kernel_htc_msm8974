/*
 * The generic setup file for PMC-Sierra MSP processors
 *
 * Copyright 2005-2007 PMC-Sierra, Inc,
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/bootinfo.h>
#include <asm/cacheflush.h>
#include <asm/r4kcache.h>
#include <asm/reboot.h>
#include <asm/smp-ops.h>
#include <asm/time.h>

#include <msp_prom.h>
#include <msp_regs.h>

#if defined(CONFIG_PMC_MSP7120_GW)
#include <msp_regops.h>
#define MSP_BOARD_RESET_GPIO	9
#endif

extern void msp_serial_setup(void);
extern void pmctwiled_setup(void);

#if defined(CONFIG_PMC_MSP7120_EVAL) || \
    defined(CONFIG_PMC_MSP7120_GW) || \
    defined(CONFIG_PMC_MSP7120_FPGA)
void msp7120_reset(void)
{
	void *start, *end, *iptr;
	register int i;

	
	local_irq_disable();
#ifdef CONFIG_SYS_SUPPORTS_MULTITHREADING
	dvpe();
#endif

	
	__asm__ __volatile__ (
		"	.set	push				\n"
		"	.set	mips3				\n"
		"	la	%0,startpoint			\n"
		"	la	%1,endpoint			\n"
		"	.set	pop				\n"
		: "=r" (start), "=r" (end)
		:
	);

	for (iptr = (void *)((unsigned int)start & ~(L1_CACHE_BYTES - 1));
	     iptr < end; iptr += L1_CACHE_BYTES)
		cache_op(Fill, iptr);

	__asm__ __volatile__ (
		"startpoint:					\n"
	);

	
	DDRC_INDIRECT_WRITE(DDRC_CTL(10), 0xb, 1 << 16);


	
	for (i = 0; i < 100000000; i++);

#if defined(CONFIG_PMC_MSP7120_GW)
	set_value_reg32(GPIO_CFG3_REG, 0xf000, 0x8000);
	set_reg32(GPIO_DATA3_REG, 8);

#endif
	
	*RST_SET_REG = 0x00000001;

	__asm__ __volatile__ (
		"endpoint:					\n"
	);
}
#endif

void msp_restart(char *command)
{
	printk(KERN_WARNING "Now rebooting .......\n");

#if defined(CONFIG_PMC_MSP7120_EVAL) || \
    defined(CONFIG_PMC_MSP7120_GW) || \
    defined(CONFIG_PMC_MSP7120_FPGA)
	msp7120_reset();
#else
	
	set_c0_status(ST0_BEV | ST0_ERL);
	change_c0_config(CONF_CM_CMASK, CONF_CM_UNCACHED);
	flush_cache_all();
	write_c0_wired(0);

	__asm__ __volatile__("jr\t%0"::"r"(0xbfc00000));
#endif
}

void msp_halt(void)
{
	printk(KERN_WARNING "\n** You can safely turn off the power\n");
	while (1)
		
		if (cpu_wait)
			(*cpu_wait)();
		else
			__asm__(".set\tmips3\n\t" "wait\n\t" ".set\tmips0");
}

void msp_power_off(void)
{
	msp_halt();
}

void __init plat_mem_setup(void)
{
	_machine_restart = msp_restart;
	_machine_halt = msp_halt;
	pm_power_off = msp_power_off;
}

extern struct plat_smp_ops msp_smtc_smp_ops;

void __init prom_init(void)
{
	unsigned long family;
	unsigned long revision;

	prom_argc = fw_arg0;
	prom_argv = (char **)fw_arg1;
	prom_envp = (char **)fw_arg2;

	family = identify_family();
	revision = identify_revision();

	switch (family)	{
	case FAMILY_FPGA:
		if (FPGA_IS_MSP4200(revision)) {
			
			mips_machtype = MACH_MSP4200_FPGA;
		} else {
			mips_machtype = MACH_MSP_OTHER;
		}
		break;

	case FAMILY_MSP4200:
#if defined(CONFIG_PMC_MSP4200_EVAL)
		mips_machtype  = MACH_MSP4200_EVAL;
#elif defined(CONFIG_PMC_MSP4200_GW)
		mips_machtype  = MACH_MSP4200_GW;
#else
		mips_machtype = MACH_MSP_OTHER;
#endif
		break;

	case FAMILY_MSP4200_FPGA:
		mips_machtype  = MACH_MSP4200_FPGA;
		break;

	case FAMILY_MSP7100:
#if defined(CONFIG_PMC_MSP7120_EVAL)
		mips_machtype = MACH_MSP7120_EVAL;
#elif defined(CONFIG_PMC_MSP7120_GW)
		mips_machtype = MACH_MSP7120_GW;
#else
		mips_machtype = MACH_MSP_OTHER;
#endif
		break;

	case FAMILY_MSP7100_FPGA:
		mips_machtype  = MACH_MSP7120_FPGA;
		break;

	default:
		
		mips_machtype  = MACH_UNKNOWN;
		panic("***Bogosity factor five***, exiting");
		break;
	}

	prom_init_cmdline();

	prom_meminit();

	msp_serial_setup();

	if (register_vsmp_smp_ops()) {
#ifdef CONFIG_MIPS_MT_SMTC
		register_smp_ops(&msp_smtc_smp_ops);
#endif
	}

#ifdef CONFIG_PMCTWILED
	pmctwiled_setup();
#endif
}
