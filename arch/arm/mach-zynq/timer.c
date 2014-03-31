/*
 * This file contains driver for the Xilinx PS Timer Counter IP.
 *
 *  Copyright (C) 2011 Xilinx
 *
 * based on arch/mips/kernel/time.c timer driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>

#include <asm/mach/time.h>
#include <mach/zynq_soc.h>
#include "common.h"

#define IRQ_TIMERCOUNTER0	42

#define XTTCPSS_CLOCKSOURCE	0	
#define XTTCPSS_CLOCKEVENT	1	

#define XTTCPSS_TIMER_BASE		TTC0_BASE
#define XTTCPCC_EVENT_TIMER_IRQ		(IRQ_TIMERCOUNTER0 + 1)
#define XTTCPSS_CLK_CNTRL_OFFSET	0x00 
#define XTTCPSS_CNT_CNTRL_OFFSET	0x0C 
#define XTTCPSS_COUNT_VAL_OFFSET	0x18 
#define XTTCPSS_INTR_VAL_OFFSET		0x24 
#define XTTCPSS_MATCH_1_OFFSET		0x30 
#define XTTCPSS_MATCH_2_OFFSET		0x3C 
#define XTTCPSS_MATCH_3_OFFSET		0x48 
#define XTTCPSS_ISR_OFFSET		0x54 
#define XTTCPSS_IER_OFFSET		0x60 

#define XTTCPSS_CNT_CNTRL_DISABLE_MASK	0x1


#define TIMER_RATE (PERIPHERAL_CLOCK_RATE / 32)

struct xttcpss_timer {
	void __iomem *base_addr;
};

static struct xttcpss_timer timers[2];
static struct clock_event_device xttcpss_clockevent;

static void xttcpss_set_interval(struct xttcpss_timer *timer,
					unsigned long cycles)
{
	u32 ctrl_reg;

	
	ctrl_reg = __raw_readl(timer->base_addr + XTTCPSS_CNT_CNTRL_OFFSET);
	ctrl_reg |= XTTCPSS_CNT_CNTRL_DISABLE_MASK;
	__raw_writel(ctrl_reg, timer->base_addr + XTTCPSS_CNT_CNTRL_OFFSET);

	__raw_writel(cycles, timer->base_addr + XTTCPSS_INTR_VAL_OFFSET);

	ctrl_reg |= 0x10;
	ctrl_reg &= ~XTTCPSS_CNT_CNTRL_DISABLE_MASK;
	__raw_writel(ctrl_reg, timer->base_addr + XTTCPSS_CNT_CNTRL_OFFSET);
}

static irqreturn_t xttcpss_clock_event_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &xttcpss_clockevent;
	struct xttcpss_timer *timer = dev_id;

	
	__raw_writel(__raw_readl(timer->base_addr + XTTCPSS_ISR_OFFSET),
			timer->base_addr + XTTCPSS_ISR_OFFSET);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction event_timer_irq = {
	.name	= "xttcpss clockevent",
	.flags	= IRQF_DISABLED | IRQF_TIMER,
	.handler = xttcpss_clock_event_interrupt,
};

static void __init xttcpss_timer_hardware_init(void)
{
	timers[XTTCPSS_CLOCKSOURCE].base_addr = XTTCPSS_TIMER_BASE;

	__raw_writel(0x0, timers[XTTCPSS_CLOCKSOURCE].base_addr +
				XTTCPSS_IER_OFFSET);
	__raw_writel(0x9, timers[XTTCPSS_CLOCKSOURCE].base_addr +
				XTTCPSS_CLK_CNTRL_OFFSET);
	__raw_writel(0x10, timers[XTTCPSS_CLOCKSOURCE].base_addr +
				XTTCPSS_CNT_CNTRL_OFFSET);


	timers[XTTCPSS_CLOCKEVENT].base_addr = XTTCPSS_TIMER_BASE + 4;

	__raw_writel(0x23, timers[XTTCPSS_CLOCKEVENT].base_addr +
			XTTCPSS_CNT_CNTRL_OFFSET);
	__raw_writel(0x9, timers[XTTCPSS_CLOCKEVENT].base_addr +
			XTTCPSS_CLK_CNTRL_OFFSET);
	__raw_writel(0x1, timers[XTTCPSS_CLOCKEVENT].base_addr +
			XTTCPSS_IER_OFFSET);

	
	event_timer_irq.dev_id = &timers[XTTCPSS_CLOCKEVENT];
	setup_irq(XTTCPCC_EVENT_TIMER_IRQ, &event_timer_irq);
}

static cycle_t __raw_readl_cycles(struct clocksource *cs)
{
	struct xttcpss_timer *timer = &timers[XTTCPSS_CLOCKSOURCE];

	return (cycle_t)__raw_readl(timer->base_addr +
				XTTCPSS_COUNT_VAL_OFFSET);
}


static struct clocksource clocksource_xttcpss = {
	.name		= "xttcpss_timer1",
	.rating		= 200,			
	.read		= __raw_readl_cycles,
	.mask		= CLOCKSOURCE_MASK(16),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};


static int xttcpss_set_next_event(unsigned long cycles,
					struct clock_event_device *evt)
{
	struct xttcpss_timer *timer = &timers[XTTCPSS_CLOCKEVENT];

	xttcpss_set_interval(timer, cycles);
	return 0;
}

static void xttcpss_set_mode(enum clock_event_mode mode,
					struct clock_event_device *evt)
{
	struct xttcpss_timer *timer = &timers[XTTCPSS_CLOCKEVENT];
	u32 ctrl_reg;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		xttcpss_set_interval(timer, TIMER_RATE / HZ);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ctrl_reg = __raw_readl(timer->base_addr +
					XTTCPSS_CNT_CNTRL_OFFSET);
		ctrl_reg |= XTTCPSS_CNT_CNTRL_DISABLE_MASK;
		__raw_writel(ctrl_reg,
				timer->base_addr + XTTCPSS_CNT_CNTRL_OFFSET);
		break;
	case CLOCK_EVT_MODE_RESUME:
		ctrl_reg = __raw_readl(timer->base_addr +
					XTTCPSS_CNT_CNTRL_OFFSET);
		ctrl_reg &= ~XTTCPSS_CNT_CNTRL_DISABLE_MASK;
		__raw_writel(ctrl_reg,
				timer->base_addr + XTTCPSS_CNT_CNTRL_OFFSET);
		break;
	}
}

static struct clock_event_device xttcpss_clockevent = {
	.name		= "xttcpss_timer2",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event	= xttcpss_set_next_event,
	.set_mode	= xttcpss_set_mode,
	.rating		= 200,
};

static void __init xttcpss_timer_init(void)
{
	xttcpss_timer_hardware_init();
	clocksource_register_hz(&clocksource_xttcpss, TIMER_RATE);

	clockevents_calc_mult_shift(&xttcpss_clockevent, TIMER_RATE, 4);

	xttcpss_clockevent.max_delta_ns =
		clockevent_delta2ns(0xfffe, &xttcpss_clockevent);
	xttcpss_clockevent.min_delta_ns =
		clockevent_delta2ns(1, &xttcpss_clockevent);

	

	xttcpss_clockevent.cpumask = cpumask_of(0);
	clockevents_register_device(&xttcpss_clockevent);
}

struct sys_timer xttcpss_sys_timer = {
	.init		= xttcpss_timer_init,
};
