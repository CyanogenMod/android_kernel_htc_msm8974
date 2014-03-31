/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 * Copyright (c) 2003, 2004  Maciej W. Rozycki
 *
 * Common time service routines for MIPS machines.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/bug.h>
#include <linux/clockchips.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/export.h>

#include <asm/cpu-features.h>
#include <asm/div64.h>
#include <asm/smtc_ipi.h>
#include <asm/time.h>

DEFINE_SPINLOCK(rtc_lock);
EXPORT_SYMBOL(rtc_lock);

int __weak rtc_mips_set_time(unsigned long sec)
{
	return 0;
}

int __weak rtc_mips_set_mmss(unsigned long nowtime)
{
	return rtc_mips_set_time(nowtime);
}

int update_persistent_clock(struct timespec now)
{
	return rtc_mips_set_mmss(now.tv_sec);
}

static int null_perf_irq(void)
{
	return 0;
}

int (*perf_irq)(void) = null_perf_irq;

EXPORT_SYMBOL(perf_irq);


unsigned int mips_hpt_frequency;

void __init plat_timer_setup(void)
{
	BUG();
}

static __init int cpu_has_mfc0_count_bug(void)
{
	switch (current_cpu_type()) {
	case CPU_R4000PC:
	case CPU_R4000SC:
	case CPU_R4000MC:
		return 1;

	case CPU_R4400PC:
	case CPU_R4400SC:
	case CPU_R4400MC:
		if ((current_cpu_data.processor_id & 0xff) <= 0x30)
			return 1;

		return 0;
	}

	return 0;
}

void __init time_init(void)
{
	plat_time_init();

	if (!mips_clockevent_init() || !cpu_has_mfc0_count_bug())
		init_mips_clocksource();
}
