/*
 * Copyright (C) 2001, 2002, MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 * Copyright (c) 2003  Maciej W. Rozycki
 *
 * include/asm-mips/time.h
 *     header file for the new style time.c file and time services.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _ASM_TIME_H
#define _ASM_TIME_H

#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>

extern spinlock_t rtc_lock;

extern int rtc_mips_set_time(unsigned long);
extern int rtc_mips_set_mmss(unsigned long);

extern void plat_time_init(void);

extern unsigned int mips_hpt_frequency;

extern int (*perf_irq)(void);

#ifdef CONFIG_CEVT_R4K_LIB
extern unsigned int __weak get_c0_compare_int(void);
extern int r4k_clockevent_init(void);
#endif

static inline int mips_clockevent_init(void)
{
#ifdef CONFIG_MIPS_MT_SMTC
	extern int smtc_clockevent_init(void);

	return smtc_clockevent_init();
#elif defined(CONFIG_CEVT_R4K)
	return r4k_clockevent_init();
#else
	return -ENXIO;
#endif
}

#ifdef CONFIG_CSRC_R4K_LIB
extern int init_r4k_clocksource(void);
#endif

static inline int init_mips_clocksource(void)
{
#ifdef CONFIG_CSRC_R4K
	return init_r4k_clocksource();
#else
	return 0;
#endif
}

static inline void clockevent_set_clock(struct clock_event_device *cd,
					unsigned int clock)
{
	clockevents_calc_mult_shift(cd, clock, 4);
}

#endif 
