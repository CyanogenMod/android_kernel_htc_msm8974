/*
 * arch/arm/plat-orion/time.c
 *
 * Marvell Orion SoC timer handling.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * Timer 0 is used as free-running clocksource, while timer 1 is
 * used as clock_event_device.
 */

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/sched_clock.h>

#define BRIDGE_CAUSE_OFF	0x0110
#define BRIDGE_MASK_OFF		0x0114
#define  BRIDGE_INT_TIMER0	 0x0002
#define  BRIDGE_INT_TIMER1	 0x0004


#define TIMER_CTRL_OFF		0x0000
#define  TIMER0_EN		 0x0001
#define  TIMER0_RELOAD_EN	 0x0002
#define  TIMER1_EN		 0x0004
#define  TIMER1_RELOAD_EN	 0x0008
#define TIMER0_RELOAD_OFF	0x0010
#define TIMER0_VAL_OFF		0x0014
#define TIMER1_RELOAD_OFF	0x0018
#define TIMER1_VAL_OFF		0x001c


static void __iomem *bridge_base;
static u32 bridge_timer1_clr_mask;
static void __iomem *timer_base;


static u32 ticks_per_jiffy;



static u32 notrace orion_read_sched_clock(void)
{
	return ~readl(timer_base + TIMER0_VAL_OFF);
}

static int
orion_clkevt_next_event(unsigned long delta, struct clock_event_device *dev)
{
	unsigned long flags;
	u32 u;

	if (delta == 0)
		return -ETIME;

	local_irq_save(flags);

	writel(bridge_timer1_clr_mask, bridge_base + BRIDGE_CAUSE_OFF);

	u = readl(bridge_base + BRIDGE_MASK_OFF);
	u |= BRIDGE_INT_TIMER1;
	writel(u, bridge_base + BRIDGE_MASK_OFF);

	writel(delta, timer_base + TIMER1_VAL_OFF);

	u = readl(timer_base + TIMER_CTRL_OFF);
	u = (u & ~TIMER1_RELOAD_EN) | TIMER1_EN;
	writel(u, timer_base + TIMER_CTRL_OFF);

	local_irq_restore(flags);

	return 0;
}

static void
orion_clkevt_mode(enum clock_event_mode mode, struct clock_event_device *dev)
{
	unsigned long flags;
	u32 u;

	local_irq_save(flags);
	if (mode == CLOCK_EVT_MODE_PERIODIC) {
		writel(ticks_per_jiffy - 1, timer_base + TIMER1_RELOAD_OFF);
		writel(ticks_per_jiffy - 1, timer_base + TIMER1_VAL_OFF);

		u = readl(bridge_base + BRIDGE_MASK_OFF);
		writel(u | BRIDGE_INT_TIMER1, bridge_base + BRIDGE_MASK_OFF);

		u = readl(timer_base + TIMER_CTRL_OFF);
		writel(u | TIMER1_EN | TIMER1_RELOAD_EN,
		       timer_base + TIMER_CTRL_OFF);
	} else {
		u = readl(timer_base + TIMER_CTRL_OFF);
		writel(u & ~TIMER1_EN, timer_base + TIMER_CTRL_OFF);

		u = readl(bridge_base + BRIDGE_MASK_OFF);
		writel(u & ~BRIDGE_INT_TIMER1, bridge_base + BRIDGE_MASK_OFF);

		writel(bridge_timer1_clr_mask, bridge_base + BRIDGE_CAUSE_OFF);

	}
	local_irq_restore(flags);
}

static struct clock_event_device orion_clkevt = {
	.name		= "orion_tick",
	.features	= CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC,
	.shift		= 32,
	.rating		= 300,
	.set_next_event	= orion_clkevt_next_event,
	.set_mode	= orion_clkevt_mode,
};

static irqreturn_t orion_timer_interrupt(int irq, void *dev_id)
{
	writel(bridge_timer1_clr_mask, bridge_base + BRIDGE_CAUSE_OFF);
	orion_clkevt.event_handler(&orion_clkevt);

	return IRQ_HANDLED;
}

static struct irqaction orion_timer_irq = {
	.name		= "orion_tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= orion_timer_interrupt
};

void __init
orion_time_set_base(u32 _timer_base)
{
	timer_base = (void __iomem *)_timer_base;
}

void __init
orion_time_init(u32 _bridge_base, u32 _bridge_timer1_clr_mask,
		unsigned int irq, unsigned int tclk)
{
	u32 u;

	bridge_base = (void __iomem *)_bridge_base;
	bridge_timer1_clr_mask = _bridge_timer1_clr_mask;

	ticks_per_jiffy = (tclk + HZ/2) / HZ;

	setup_sched_clock(orion_read_sched_clock, 32, tclk);

	writel(0xffffffff, timer_base + TIMER0_VAL_OFF);
	writel(0xffffffff, timer_base + TIMER0_RELOAD_OFF);
	u = readl(bridge_base + BRIDGE_MASK_OFF);
	writel(u & ~BRIDGE_INT_TIMER0, bridge_base + BRIDGE_MASK_OFF);
	u = readl(timer_base + TIMER_CTRL_OFF);
	writel(u | TIMER0_EN | TIMER0_RELOAD_EN, timer_base + TIMER_CTRL_OFF);
	clocksource_mmio_init(timer_base + TIMER0_VAL_OFF, "orion_clocksource",
		tclk, 300, 32, clocksource_mmio_readl_down);

	setup_irq(irq, &orion_timer_irq);
	orion_clkevt.mult = div_sc(tclk, NSEC_PER_SEC, orion_clkevt.shift);
	orion_clkevt.max_delta_ns = clockevent_delta2ns(0xfffffffe, &orion_clkevt);
	orion_clkevt.min_delta_ns = clockevent_delta2ns(1, &orion_clkevt);
	orion_clkevt.cpumask = cpumask_of(0);
	clockevents_register_device(&orion_clkevt);
}
