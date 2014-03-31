/*
 * arch/arm/mach-at91/at91x40.c
 *
 * (C) Copyright 2007, Greg Ungerer <gerg@snapgear.com>
 * Copyright (C) 2005 SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/proc-fns.h>
#include <asm/system_misc.h>
#include <asm/mach/arch.h>
#include <mach/at91x40.h>
#include <mach/at91_st.h>
#include <mach/timex.h>
#include "generic.h"

int clk_enable(struct clk *clk)
{
	return 0;
}

void clk_disable(struct clk *clk)
{
}

unsigned long clk_get_rate(struct clk *clk)
{
	return AT91X40_MASTER_CLOCK;
}

static void at91x40_idle(void)
{
	__raw_writel(AT91_PS_CR_CPU, AT91_PS_CR);
	cpu_do_idle();
}

void __init at91x40_initialize(unsigned long main_clock)
{
	arm_pm_idle = at91x40_idle;
	at91_extern_irq = (1 << AT91X40_ID_IRQ0) | (1 << AT91X40_ID_IRQ1)
			| (1 << AT91X40_ID_IRQ2);
}

static unsigned int at91x40_default_irq_priority[NR_AIC_IRQS] __initdata = {
	7,	
	0,	
	0,	
	0,	
	2,	
	2,	
	2,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
	0,	
};

void __init at91x40_init_interrupts(unsigned int priority[NR_AIC_IRQS])
{
	if (!priority)
		priority = at91x40_default_irq_priority;

	at91_aic_init(priority);
}

