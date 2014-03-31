/*
 * loongson-specific suspend support
 *
 *  Copyright (C) 2009 Lemote Inc.
 *  Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <asm/i8259.h>
#include <asm/mipsregs.h>

#include <loongson.h>

static unsigned int __maybe_unused cached_master_mask;	
static unsigned int __maybe_unused cached_slave_mask;
static unsigned int __maybe_unused cached_bonito_irq_mask; 

void arch_suspend_disable_irqs(void)
{
	
	local_irq_disable();

#ifdef CONFIG_I8259
	
	cached_slave_mask = inb(PIC_SLAVE_IMR);
	cached_master_mask = inb(PIC_MASTER_IMR);

	outb(0xff, PIC_SLAVE_IMR);
	inb(PIC_SLAVE_IMR);
	outb(0xff, PIC_MASTER_IMR);
	inb(PIC_MASTER_IMR);
#endif
	
	cached_bonito_irq_mask = LOONGSON_INTEN;
	LOONGSON_INTENCLR = 0xffff;
	(void)LOONGSON_INTENCLR;
}

void arch_suspend_enable_irqs(void)
{
	
	local_irq_enable();
#ifdef CONFIG_I8259
	
	outb(cached_slave_mask, PIC_SLAVE_IMR);
	outb(cached_master_mask, PIC_MASTER_IMR);
#endif
	
	LOONGSON_INTENSET = cached_bonito_irq_mask;
	(void)LOONGSON_INTENSET;
}

void __weak setup_wakeup_events(void)
{
}

int __weak wakeup_loongson(void)
{
	return 1;
}

static void wait_for_wakeup_events(void)
{
	while (!wakeup_loongson())
		LOONGSON_CHIPCFG0 &= ~0x7;
}

static inline void stop_perf_counters(void)
{
	__write_64bit_c0_register($24, 0, 0);
}


static void loongson_suspend_enter(void)
{
	static unsigned int cached_cpu_freq;

	
	setup_wakeup_events();

	stop_perf_counters();

	cached_cpu_freq = LOONGSON_CHIPCFG0;

	
	LOONGSON_CHIPCFG0 &= ~0x7;

	
	wait_for_wakeup_events();

	LOONGSON_CHIPCFG0 = cached_cpu_freq;
	mmiowb();
}

void __weak mach_suspend(void)
{
}

void __weak mach_resume(void)
{
}

static int loongson_pm_enter(suspend_state_t state)
{
	mach_suspend();

	
	loongson_suspend_enter();

	mach_resume();

	return 0;
}

static int loongson_pm_valid_state(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_ON:
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		return 1;

	default:
		return 0;
	}
}

static const struct platform_suspend_ops loongson_pm_ops = {
	.valid	= loongson_pm_valid_state,
	.enter	= loongson_pm_enter,
};

static int __init loongson_pm_init(void)
{
	suspend_set_ops(&loongson_pm_ops);

	return 0;
}
arch_initcall(loongson_pm_init);
