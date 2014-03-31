/*
 * Time related functions for Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/module.h>

#include <asm/timer-regs.h>
#include <asm/hexagon_vm.h>


cycles_t	pcycle_freq_mhz;
cycles_t	thread_freq_mhz;
cycles_t	sleep_clk_freq;

static struct resource rtos_timer_resources[] = {
	{
		.start	= RTOS_TIMER_REGS_ADDR,
		.end	= RTOS_TIMER_REGS_ADDR+PAGE_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device rtos_timer_device = {
	.name		= "rtos_timer",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtos_timer_resources),
	.resource	= rtos_timer_resources,
};

struct adsp_hw_timer_struct {
	u32 match;   
	u32 count;
	u32 enable;  
	u32 clear;   
};

static __iomem struct adsp_hw_timer_struct *rtos_timer;

static cycle_t timer_get_cycles(struct clocksource *cs)
{
	return (cycle_t) __vmgettime();
}

static struct clocksource hexagon_clocksource = {
	.name		= "pcycles",
	.rating		= 250,
	.read		= timer_get_cycles,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int set_next_event(unsigned long delta, struct clock_event_device *evt)
{
	

	iowrite32(1, &rtos_timer->clear);
	iowrite32(0, &rtos_timer->clear);

	iowrite32(delta, &rtos_timer->match);
	iowrite32(1 << TIMER_ENABLE, &rtos_timer->enable);
	return 0;
}

static void set_mode(enum clock_event_mode mode,
	struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_SHUTDOWN:
		
	default:
		break;
	}
}

#ifdef CONFIG_SMP
static void broadcast(const struct cpumask *mask)
{
	send_ipi(mask, IPI_TIMER);
}
#endif

static struct clock_event_device hexagon_clockevent_dev = {
	.name		= "clockevent",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.rating		= 400,
	.irq		= RTOS_TIMER_INT,
	.set_next_event = set_next_event,
	.set_mode	= set_mode,
#ifdef CONFIG_SMP
	.broadcast	= broadcast,
#endif
};

#ifdef CONFIG_SMP
static DEFINE_PER_CPU(struct clock_event_device, clock_events);

void setup_percpu_clockdev(void)
{
	int cpu = smp_processor_id();
	struct clock_event_device *ce_dev = &hexagon_clockevent_dev;
	struct clock_event_device *dummy_clock_dev =
		&per_cpu(clock_events, cpu);

	memcpy(dummy_clock_dev, ce_dev, sizeof(*dummy_clock_dev));
	INIT_LIST_HEAD(&dummy_clock_dev->list);

	dummy_clock_dev->features = CLOCK_EVT_FEAT_DUMMY;
	dummy_clock_dev->cpumask = cpumask_of(cpu);
	dummy_clock_dev->mode = CLOCK_EVT_MODE_UNUSED;

	clockevents_register_device(dummy_clock_dev);
}

void ipi_timer(void)
{
	int cpu = smp_processor_id();
	struct clock_event_device *ce_dev = &per_cpu(clock_events, cpu);

	ce_dev->event_handler(ce_dev);
}
#endif 

static irqreturn_t timer_interrupt(int irq, void *devid)
{
	struct clock_event_device *ce_dev = &hexagon_clockevent_dev;

	iowrite32(0, &rtos_timer->enable);
	ce_dev->event_handler(ce_dev);

	return IRQ_HANDLED;
}

static struct irqaction rtos_timer_intdesc = {
	.handler = timer_interrupt,
	.flags = IRQF_TIMER | IRQF_TRIGGER_RISING,
	.name = "rtos_timer"
};

void __init time_init_deferred(void)
{
	struct resource *resource = NULL;
	struct clock_event_device *ce_dev = &hexagon_clockevent_dev;
	struct device_node *dn;
	struct resource r;
	int err;

	ce_dev->cpumask = cpu_all_mask;

	if (!resource)
		resource = rtos_timer_device.resource;

	
	rtos_timer = ioremap(resource->start, resource->end
		- resource->start + 1);

	if (!rtos_timer) {
		release_mem_region(resource->start, resource->end
			- resource->start + 1);
	}
	clocksource_register_khz(&hexagon_clocksource, pcycle_freq_mhz * 1000);

	

	clockevents_calc_mult_shift(ce_dev, sleep_clk_freq, 4);

	ce_dev->max_delta_ns = clockevent_delta2ns(0x7fffffff, ce_dev);
	ce_dev->min_delta_ns = clockevent_delta2ns(0xf, ce_dev);

#ifdef CONFIG_SMP
	setup_percpu_clockdev();
#endif

	clockevents_register_device(ce_dev);
	setup_irq(ce_dev->irq, &rtos_timer_intdesc);
}

void __init time_init(void)
{
	late_time_init = time_init_deferred;
}

static long long fudgefactor = 350;  

void __udelay(unsigned long usecs)
{
	unsigned long long start = __vmgettime();
	unsigned long long finish = (pcycle_freq_mhz * usecs) - fudgefactor;

	while ((__vmgettime() - start) < finish)
		cpu_relax(); 
}
EXPORT_SYMBOL(__udelay);
