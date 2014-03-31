/*
 *  linux/arch/parisc/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Modifications for ARM (C) 1994, 1995, 1996,1997 Russell King
 *  Copyright (C) 1999 SuSE GmbH, (Philipp Rumpf, prumpf@tux.org)
 *
 * 1994-07-02  Alan Modra
 *             fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 * 1998-12-20  Updated NTP code according to technical memorandum Jan '96
 *             "A Kernel Model for Precision Timekeeping" by Dave Mills
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/clocksource.h>
#include <linux/platform_device.h>
#include <linux/ftrace.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/param.h>
#include <asm/pdc.h>
#include <asm/led.h>

#include <linux/timex.h>

static unsigned long clocktick __read_mostly;	

irqreturn_t __irq_entry timer_interrupt(int irq, void *dev_id)
{
	unsigned long now, now2;
	unsigned long next_tick;
	unsigned long cycles_elapsed, ticks_elapsed = 1;
	unsigned long cycles_remainder;
	unsigned int cpu = smp_processor_id();
	struct cpuinfo_parisc *cpuinfo = &per_cpu(cpu_data, cpu);

	
	unsigned long cpt = clocktick;

	profile_tick(CPU_PROFILING);

	
	next_tick = cpuinfo->it_value;

	
	now = mfctl(16);

	cycles_elapsed = now - next_tick;

	if ((cycles_elapsed >> 6) < cpt) {
		cycles_remainder = cycles_elapsed;
		while (cycles_remainder > cpt) {
			cycles_remainder -= cpt;
			ticks_elapsed++;
		}
	} else {
		
		cycles_remainder = cycles_elapsed % cpt;
		ticks_elapsed += cycles_elapsed / cpt;
	}

	
	cycles_remainder = cpt - cycles_remainder;

	next_tick = now + cycles_remainder;

	cpuinfo->it_value = next_tick;

	mtctl(next_tick, 16);

	now2 = mfctl(16);
	if (next_tick - now2 > cpt)
		mtctl(next_tick+cpt, 16);

#if 1
	if (unlikely(now2 - now > 0x3000)) 	
		printk (KERN_CRIT "timer_interrupt(CPU %d): SLOW! 0x%lx cycles!"
			" cyc %lX rem %lX "
			" next/now %lX/%lX\n",
			cpu, now2 - now, cycles_elapsed, cycles_remainder,
			next_tick, now );
#endif

	if (unlikely(ticks_elapsed > HZ)) {
		
		printk (KERN_CRIT "timer_interrupt(CPU %d): delayed!"
			" cycles %lX rem %lX "
			" next/now %lX/%lX\n",
			cpu,
			cycles_elapsed, cycles_remainder,
			next_tick, now );
	}


	if (!--cpuinfo->prof_counter) {
		cpuinfo->prof_counter = cpuinfo->prof_multiplier;
		update_process_times(user_mode(get_irq_regs()));
	}

	if (cpu == 0)
		xtime_update(ticks_elapsed);

	return IRQ_HANDLED;
}


unsigned long profile_pc(struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);

	if (regs->gr[0] & PSW_N)
		pc -= 4;

#ifdef CONFIG_SMP
	if (in_lock_functions(pc))
		pc = regs->gr[2];
#endif

	return pc;
}
EXPORT_SYMBOL(profile_pc);



static cycle_t read_cr16(struct clocksource *cs)
{
	return get_cycles();
}

static struct clocksource clocksource_cr16 = {
	.name			= "cr16",
	.rating			= 300,
	.read			= read_cr16,
	.mask			= CLOCKSOURCE_MASK(BITS_PER_LONG),
	.flags			= CLOCK_SOURCE_IS_CONTINUOUS,
};

#ifdef CONFIG_SMP
int update_cr16_clocksource(void)
{
	if (clocksource_cr16.rating != 0 && num_online_cpus() > 1) {
		clocksource_change_rating(&clocksource_cr16, 0);
		return 1;
	}

	return 0;
}
#else
int update_cr16_clocksource(void)
{
	return 0; 
}
#endif 

void __init start_cpu_itimer(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long next_tick = mfctl(16) + clocktick;

	mtctl(next_tick, 16);		

	per_cpu(cpu_data, cpu).it_value = next_tick;
}

static struct platform_device rtc_generic_dev = {
	.name = "rtc-generic",
	.id = -1,
};

static int __init rtc_init(void)
{
	if (platform_device_register(&rtc_generic_dev) < 0)
		printk(KERN_ERR "unable to register rtc device...\n");

	
	return 0;
}
module_init(rtc_init);

void read_persistent_clock(struct timespec *ts)
{
	static struct pdc_tod tod_data;
	if (pdc_tod_read(&tod_data) == 0) {
		ts->tv_sec = tod_data.tod_sec;
		ts->tv_nsec = tod_data.tod_usec * 1000;
	} else {
		printk(KERN_ERR "Error reading tod clock\n");
	        ts->tv_sec = 0;
		ts->tv_nsec = 0;
	}
}

void __init time_init(void)
{
	unsigned long current_cr16_khz;

	clocktick = (100 * PAGE0->mem_10msec) / HZ;

	start_cpu_itimer();	

	
	current_cr16_khz = PAGE0->mem_10msec/10;  
	clocksource_register_khz(&clocksource_cr16, current_cr16_khz);
}
