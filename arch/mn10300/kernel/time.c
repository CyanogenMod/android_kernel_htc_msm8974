/* MN10300 Low level time management
 *
 * Copyright (C) 2007-2008 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from arch/i386/kernel/time.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/cnt32_to_63.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/processor.h>
#include <asm/intctl-regs.h>
#include <asm/rtc.h>
#include "internal.h"

static unsigned long mn10300_last_tsc;	

static unsigned long sched_clock_multiplier;

unsigned long long sched_clock(void)
{
	union {
		unsigned long long ll;
		unsigned l[2];
	} tsc64, result;
	unsigned long tmp;
	unsigned product[3]; 

	
	preempt_disable();

	tsc64.ll = cnt32_to_63(get_cycles()) & 0x7fffffffffffffffULL;

	preempt_enable();

	asm("mulu	%2,%0,%3,%0	\n"	
	    "mulu	%2,%1,%2,%1	\n"	
	    "add	%3,%1		\n"
	    "addc	0,%2		\n"	
	    : "=r"(product[0]), "=r"(product[1]), "=r"(product[2]), "=r"(tmp)
	    :  "0"(tsc64.l[0]),  "1"(tsc64.l[1]),  "2"(sched_clock_multiplier)
	    : "cc");

	result.l[0] = product[1] << 16 | product[0] >> 16;
	result.l[1] = product[2] << 16 | product[1] >> 16;

	return result.ll;
}

static void __init mn10300_sched_clock_init(void)
{
	sched_clock_multiplier =
		__muldiv64u(NSEC_PER_SEC, 1 << 16, MN10300_TSCCLK);
}

irqreturn_t local_timer_interrupt(void)
{
	profile_tick(CPU_PROFILING);
	update_process_times(user_mode(get_irq_regs()));
	return IRQ_HANDLED;
}

void __init time_init(void)
{
	TMPSCNT |= TMPSCNT_ENABLE;

	init_clocksource();

	printk(KERN_INFO
	       "timestamp counter I/O clock running at %lu.%02lu"
	       " (calibrated against RTC)\n",
	       MN10300_TSCCLK / 1000000, (MN10300_TSCCLK / 10000) % 100);

	mn10300_last_tsc = read_timestamp_counter();

	init_clockevents();

#ifdef CONFIG_MN10300_WD_TIMER
	
	watchdog_go();
#endif

	mn10300_sched_clock_init();
}
