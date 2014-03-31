/*
 * SA1100 Power Management Routines
 *
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2001-02-06:	Cliff Brake         Initial code
 *
 * 2001-02-25:	Sukjae Cho <sjcho@east.isi.edu> &
 * 		Chester Kuo <chester@linux.org.tw>
 * 			Save more value for the resume function! Support
 * 			Bitsy/Assabet/Freebird board
 *
 * 2001-08-29:	Nicolas Pitre <nico@fluxnic.net>
 * 			Cleaned up, pushed platform dependent stuff
 * 			in the platform specific files.
 *
 * 2002-05-27:	Nicolas Pitre	Killed sleep.h and the kmalloced save array.
 * 				Storage is local on the stack now.
 */
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>

#include <mach/hardware.h>
#include <asm/memory.h>
#include <asm/suspend.h>
#include <asm/mach/time.h>

extern int sa1100_finish_suspend(unsigned long);

#define SAVE(x)		sleep_save[SLEEP_SAVE_##x] = x
#define RESTORE(x)	x = sleep_save[SLEEP_SAVE_##x]

enum {	SLEEP_SAVE_GPDR, SLEEP_SAVE_GAFR,
	SLEEP_SAVE_PPDR, SLEEP_SAVE_PPSR, SLEEP_SAVE_PPAR, SLEEP_SAVE_PSDR,

	SLEEP_SAVE_Ser1SDCR0,

	SLEEP_SAVE_COUNT
};


static int sa11x0_pm_enter(suspend_state_t state)
{
	unsigned long gpio, sleep_save[SLEEP_SAVE_COUNT];

	gpio = GPLR;

	
	SAVE(GPDR);
	SAVE(GAFR);

	SAVE(PPDR);
	SAVE(PPSR);
	SAVE(PPAR);
	SAVE(PSDR);

	SAVE(Ser1SDCR0);

	
	RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;

	
	PSPR = virt_to_phys(cpu_resume);

	
	cpu_suspend(0, sa1100_finish_suspend);

	PSPR = 0;

	ICLR = 0;
	ICCR = 1;
	ICMR = 0;

	
	RESTORE(GPDR);
	RESTORE(GAFR);

	RESTORE(PPDR);
	RESTORE(PPSR);
	RESTORE(PPAR);
	RESTORE(PSDR);

	RESTORE(Ser1SDCR0);

	GPSR = gpio;
	GPCR = ~gpio;

	PSSR = PSSR_PH;

	return 0;
}

static const struct platform_suspend_ops sa11x0_pm_ops = {
	.enter		= sa11x0_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static int __init sa11x0_pm_init(void)
{
	suspend_set_ops(&sa11x0_pm_ops);
	return 0;
}

late_initcall(sa11x0_pm_init);
