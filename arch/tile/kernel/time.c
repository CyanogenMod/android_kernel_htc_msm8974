/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * Support the cycle counter clocksource and tile timer clock event device.
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <asm/irq_regs.h>
#include <asm/traps.h>
#include <hv/hypervisor.h>
#include <arch/interrupts.h>
#include <arch/spr_def.h>



static cycles_t cycles_per_sec __write_once;

cycles_t get_clock_rate(void)
{
	return cycles_per_sec;
}

#if CHIP_HAS_SPLIT_CYCLE()
cycles_t get_cycles(void)
{
	unsigned int high = __insn_mfspr(SPR_CYCLE_HIGH);
	unsigned int low = __insn_mfspr(SPR_CYCLE_LOW);
	unsigned int high2 = __insn_mfspr(SPR_CYCLE_HIGH);

	while (unlikely(high != high2)) {
		low = __insn_mfspr(SPR_CYCLE_LOW);
		high = high2;
		high2 = __insn_mfspr(SPR_CYCLE_HIGH);
	}

	return (((cycles_t)high) << 32) | low;
}
EXPORT_SYMBOL(get_cycles);
#endif

#define SCHED_CLOCK_SHIFT 10

static unsigned long sched_clock_mult __write_once;

static cycles_t clocksource_get_cycles(struct clocksource *cs)
{
	return get_cycles();
}

static struct clocksource cycle_counter_cs = {
	.name = "cycle counter",
	.rating = 300,
	.read = clocksource_get_cycles,
	.mask = CLOCKSOURCE_MASK(64),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

void __init setup_clock(void)
{
	cycles_per_sec = hv_sysconf(HV_SYSCONF_CPU_SPEED);
	sched_clock_mult =
		clocksource_hz2mult(cycles_per_sec, SCHED_CLOCK_SHIFT);
}

void __init calibrate_delay(void)
{
	loops_per_jiffy = get_clock_rate() / HZ;
	pr_info("Clock rate yields %lu.%02lu BogoMIPS (lpj=%lu)\n",
		loops_per_jiffy/(500000/HZ),
		(loops_per_jiffy/(5000/HZ)) % 100, loops_per_jiffy);
}

void __init time_init(void)
{
	
	clocksource_register_hz(&cycle_counter_cs, cycles_per_sec);

	
	setup_tile_timer();
}



#define MAX_TICK 0x7fffffff   
#define TILE_MINSEC 5         

static int tile_timer_set_next_event(unsigned long ticks,
				     struct clock_event_device *evt)
{
	BUG_ON(ticks > MAX_TICK);
	__insn_mtspr(SPR_TILE_TIMER_CONTROL, ticks);
	arch_local_irq_unmask_now(INT_TILE_TIMER);
	return 0;
}

static void tile_timer_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	arch_local_irq_mask_now(INT_TILE_TIMER);
}

static DEFINE_PER_CPU(struct clock_event_device, tile_timer) = {
	.name = "tile timer",
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.min_delta_ns = 1000,
	.rating = 100,
	.irq = -1,
	.set_next_event = tile_timer_set_next_event,
	.set_mode = tile_timer_set_mode,
};

void __cpuinit setup_tile_timer(void)
{
	struct clock_event_device *evt = &__get_cpu_var(tile_timer);

	
	clockevents_calc_mult_shift(evt, cycles_per_sec, TILE_MINSEC);
	evt->max_delta_ns = clockevent_delta2ns(MAX_TICK, evt);

	
	evt->cpumask = cpumask_of(smp_processor_id());

	
	arch_local_irq_mask_now(INT_TILE_TIMER);

	
	clockevents_register_device(evt);
}

void do_timer_interrupt(struct pt_regs *regs, int fault_num)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	struct clock_event_device *evt = &__get_cpu_var(tile_timer);

	arch_local_irq_mask(INT_TILE_TIMER);

	
	irq_enter();

	
	__get_cpu_var(irq_stat).irq_timer_count++;

	
	evt->event_handler(evt);

	irq_exit();

	set_irq_regs(old_regs);
}

unsigned long long sched_clock(void)
{
	return clocksource_cyc2ns(get_cycles(),
				  sched_clock_mult, SCHED_CLOCK_SHIFT);
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

cycles_t ns2cycles(unsigned long nsecs)
{
	struct clock_event_device *dev = &__get_cpu_var(tile_timer);
	return ((u64)nsecs * dev->mult) >> dev->shift;
}
