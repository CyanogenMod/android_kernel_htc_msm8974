/*
 * CS5536 General timer functions
 *
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Yanhua, yanh@lemote.com
 *
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu zhangjin, wuzhangjin@gmail.com
 *
 * Reference: AMD Geode(TM) CS5536 Companion Device Data Book
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/clockchips.h>

#include <asm/time.h>

#include <cs5536/cs5536_mfgpt.h>

DEFINE_SPINLOCK(mfgpt_lock);
EXPORT_SYMBOL(mfgpt_lock);

static u32 mfgpt_base;


void disable_mfgpt0_counter(void)
{
	outw(inw(MFGPT0_SETUP) & 0x7fff, MFGPT0_SETUP);
}
EXPORT_SYMBOL(disable_mfgpt0_counter);

void enable_mfgpt0_counter(void)
{
	outw(0xe310, MFGPT0_SETUP);
}
EXPORT_SYMBOL(enable_mfgpt0_counter);

static void init_mfgpt_timer(enum clock_event_mode mode,
			     struct clock_event_device *evt)
{
	spin_lock(&mfgpt_lock);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		outw(COMPARE, MFGPT0_CMP2);	
		outw(0, MFGPT0_CNT);	
		enable_mfgpt0_counter();
		break;

	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		if (evt->mode == CLOCK_EVT_MODE_PERIODIC ||
		    evt->mode == CLOCK_EVT_MODE_ONESHOT)
			disable_mfgpt0_counter();
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		
		break;

	case CLOCK_EVT_MODE_RESUME:
		
		break;
	}
	spin_unlock(&mfgpt_lock);
}

static struct clock_event_device mfgpt_clockevent = {
	.name = "mfgpt",
	.features = CLOCK_EVT_FEAT_PERIODIC,
	.set_mode = init_mfgpt_timer,
	.irq = CS5536_MFGPT_INTR,
};

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	u32 basehi;

	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_MFGPT), &basehi, &mfgpt_base);

	
	outw(inw(MFGPT0_SETUP) | 0x4000, MFGPT0_SETUP);

	mfgpt_clockevent.event_handler(&mfgpt_clockevent);

	return IRQ_HANDLED;
}

static struct irqaction irq5 = {
	.handler = timer_interrupt,
	.flags = IRQF_NOBALANCING | IRQF_TIMER,
	.name = "timer"
};

void __init setup_mfgpt0_timer(void)
{
	u32 basehi;
	struct clock_event_device *cd = &mfgpt_clockevent;
	unsigned int cpu = smp_processor_id();

	cd->cpumask = cpumask_of(cpu);
	clockevent_set_clock(cd, MFGPT_TICK_RATE);
	cd->max_delta_ns = clockevent_delta2ns(0xffff, cd);
	cd->min_delta_ns = clockevent_delta2ns(0xf, cd);

	
	_wrmsr(DIVIL_MSR_REG(MFGPT_IRQ), 0, 0x100);

	
	_wrmsr(DIVIL_MSR_REG(PIC_ZSEL_LOW), 0, 0x50000);

	
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_MFGPT), &basehi, &mfgpt_base);

	clockevents_register_device(cd);

	setup_irq(CS5536_MFGPT_INTR, &irq5);
}

static cycle_t mfgpt_read(struct clocksource *cs)
{
	unsigned long flags;
	int count;
	u32 jifs;
	static int old_count;
	static u32 old_jifs;

	spin_lock_irqsave(&mfgpt_lock, flags);
	jifs = jiffies;
	
	count = inw(MFGPT0_CNT);

	if (count < old_count && jifs == old_jifs)
		count = old_count;

	old_count = count;
	old_jifs = jifs;

	spin_unlock_irqrestore(&mfgpt_lock, flags);

	return (cycle_t) (jifs * COMPARE) + count;
}

static struct clocksource clocksource_mfgpt = {
	.name = "mfgpt",
	.rating = 120, 
	.read = mfgpt_read,
	.mask = CLOCKSOURCE_MASK(32),
};

int __init init_mfgpt_clocksource(void)
{
	if (num_possible_cpus() > 1)	
		return 0;

	return clocksource_register_hz(&clocksource_mfgpt, MFGPT_TICK_RATE);
}

arch_initcall(init_mfgpt_clocksource);
