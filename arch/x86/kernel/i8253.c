#include <linux/clockchips.h>
#include <linux/module.h>
#include <linux/timex.h>
#include <linux/i8253.h>

#include <asm/hpet.h>
#include <asm/time.h>
#include <asm/smp.h>

struct clock_event_device *global_clock_event;

void __init setup_pit_timer(void)
{
	clockevent_i8253_init(true);
	global_clock_event = &i8253_clockevent;
}

#ifndef CONFIG_X86_64
static int __init init_pit_clocksource(void)
{
	if (num_possible_cpus() > 1 || is_hpet_enabled() ||
	    i8253_clockevent.mode != CLOCK_EVT_MODE_PERIODIC)
		return 0;

	return clocksource_i8253_init();
}
arch_initcall(init_pit_clocksource);
#endif 
