/*
 *  linux/arch/alpha/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995, 1999, 2000  Linus Torvalds
 *
 * This file contains the PC-specific time handling details:
 * reading the RTC at bootup, etc..
 * 1994-07-02    Alan Modra
 *	fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 * 1995-03-26    Markus Kuhn
 *      fixed 500 ms bug at call to set_rtc_mmss, fixed DS12887
 *      precision CMOS clock update
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 * 1997-01-09    Adrian Sun
 *      use interval timer if CONFIG_RTC=y
 * 1997-10-29    John Bowman (bowman@math.ualberta.ca)
 *      fixed tick loss calculation in timer_interrupt
 *      (round system clock to nearest tick instead of truncating)
 *      fixed algorithm in time_init for getting time from CMOS clock
 * 1999-04-16	Thorsten Kranzkowski (dl8bcu@gmx.net)
 *	fixed algorithm in do_gettimeofday() for calculating the precise time
 *	from processor cycle counter (now taking lost_ticks into account)
 * 2000-08-13	Jan-Benedict Glaw <jbglaw@lug-owl.de>
 * 	Fixed time_init to be aware of epoches != 1900. This prevents
 * 	booting up in 2048 for me;) Code is stolen from rtc.c.
 * 2003-06-03	R. Scott Bailey <scott.bailey@eds.com>
 *	Tighten sanity in time_init from 1% (10,000 PPM) to 250 PPM
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/bcd.h>
#include <linux/profile.h>
#include <linux/irq_work.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hwrpb.h>
#include <asm/rtc.h>

#include <linux/mc146818rtc.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/clocksource.h>

#include "proto.h"
#include "irq_impl.h"

static int set_rtc_mmss(unsigned long);

DEFINE_SPINLOCK(rtc_lock);
EXPORT_SYMBOL(rtc_lock);

#define TICK_SIZE (tick_nsec / 1000)

#define FIX_SHIFT	48

static struct {
	
	__u32 last_time;
	
	unsigned long scaled_ticks_per_cycle;
	
	unsigned long partial_tick;
} state;

unsigned long est_cycle_freq;

#ifdef CONFIG_IRQ_WORK

DEFINE_PER_CPU(u8, irq_work_pending);

#define set_irq_work_pending_flag()  __get_cpu_var(irq_work_pending) = 1
#define test_irq_work_pending()      __get_cpu_var(irq_work_pending)
#define clear_irq_work_pending()     __get_cpu_var(irq_work_pending) = 0

void arch_irq_work_raise(void)
{
	set_irq_work_pending_flag();
}

#else  

#define test_irq_work_pending()      0
#define clear_irq_work_pending()

#endif 


static inline __u32 rpcc(void)
{
    __u32 result;
    asm volatile ("rpcc %0" : "=r"(result));
    return result;
}

int update_persistent_clock(struct timespec now)
{
	return set_rtc_mmss(now.tv_sec);
}

void read_persistent_clock(struct timespec *ts)
{
	unsigned int year, mon, day, hour, min, sec, epoch;

	sec = CMOS_READ(RTC_SECONDS);
	min = CMOS_READ(RTC_MINUTES);
	hour = CMOS_READ(RTC_HOURS);
	day = CMOS_READ(RTC_DAY_OF_MONTH);
	mon = CMOS_READ(RTC_MONTH);
	year = CMOS_READ(RTC_YEAR);

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		sec = bcd2bin(sec);
		min = bcd2bin(min);
		hour = bcd2bin(hour);
		day = bcd2bin(day);
		mon = bcd2bin(mon);
		year = bcd2bin(year);
	}

	
	epoch = 1900;
	if (year < 20)
		epoch = 2000;
	else if (year >= 20 && year < 48)
		
		epoch = 1980;
	else if (year >= 48 && year < 70)
		
		epoch = 1952;

	printk(KERN_INFO "Using epoch = %d\n", epoch);

	if ((year += epoch) < 1970)
		year += 100;

	ts->tv_sec = mktime(year, mon, day, hour, min, sec);
	ts->tv_nsec = 0;
}



irqreturn_t timer_interrupt(int irq, void *dev)
{
	unsigned long delta;
	__u32 now;
	long nticks;

#ifndef CONFIG_SMP
	
	profile_tick(CPU_PROFILING);
#endif

	now = rpcc();
	delta = now - state.last_time;
	state.last_time = now;
	delta = delta * state.scaled_ticks_per_cycle + state.partial_tick;
	state.partial_tick = delta & ((1UL << FIX_SHIFT) - 1); 
	nticks = delta >> FIX_SHIFT;

	if (nticks)
		xtime_update(nticks);

	if (test_irq_work_pending()) {
		clear_irq_work_pending();
		irq_work_run();
	}

#ifndef CONFIG_SMP
	while (nticks--)
		update_process_times(user_mode(get_irq_regs()));
#endif

	return IRQ_HANDLED;
}

void __init
common_init_rtc(void)
{
	unsigned char x;

	
	x = CMOS_READ(RTC_FREQ_SELECT) & 0x3f;
	if (x != 0x26 && x != 0x25 && x != 0x19 && x != 0x06) {
		printk("Setting RTC_FREQ to 1024 Hz (%x)\n", x);
		CMOS_WRITE(0x26, RTC_FREQ_SELECT);
	}

	
	x = CMOS_READ(RTC_CONTROL);
	if (!(x & RTC_PIE)) {
		printk("Turning on RTC interrupts.\n");
		x |= RTC_PIE;
		x &= ~(RTC_AIE | RTC_UIE);
		CMOS_WRITE(x, RTC_CONTROL);
	}
	(void) CMOS_READ(RTC_INTR_FLAGS);

	outb(0x36, 0x43);	
	outb(0x00, 0x40);
	outb(0x00, 0x40);

	outb(0xb6, 0x43);	
	outb(0x31, 0x42);
	outb(0x13, 0x42);

	init_rtc_irq();
}

unsigned int common_get_rtc_time(struct rtc_time *time)
{
	return __get_rtc_time(time);
}

int common_set_rtc_time(struct rtc_time *time)
{
	return __set_rtc_time(time);
}


static unsigned long __init
validate_cc_value(unsigned long cc)
{
	static struct bounds {
		unsigned int min, max;
	} cpu_hz[] __initdata = {
		[EV3_CPU]    = {   50000000,  200000000 },	
		[EV4_CPU]    = {  100000000,  300000000 },
		[LCA4_CPU]   = {  100000000,  300000000 },	
		[EV45_CPU]   = {  200000000,  300000000 },
		[EV5_CPU]    = {  250000000,  433000000 },
		[EV56_CPU]   = {  333000000,  667000000 },
		[PCA56_CPU]  = {  400000000,  600000000 },	
		[PCA57_CPU]  = {  500000000,  600000000 },	
		[EV6_CPU]    = {  466000000,  600000000 },
		[EV67_CPU]   = {  600000000,  750000000 },
		[EV68AL_CPU] = {  750000000,  940000000 },
		[EV68CB_CPU] = { 1000000000, 1333333333 },
		
		[EV68CX_CPU] = { 1000000000, 1700000000 },	
		[EV69_CPU]   = { 1000000000, 1700000000 },	
		[EV7_CPU]    = {  800000000, 1400000000 },	
		[EV79_CPU]   = { 1000000000, 2000000000 },	
	};

	
	const unsigned int deviation = 10000000;

	struct percpu_struct *cpu;
	unsigned int index;

	cpu = (struct percpu_struct *)((char*)hwrpb + hwrpb->processor_offset);
	index = cpu->type & 0xffffffff;

	
	if (index >= ARRAY_SIZE(cpu_hz))
		return cc;

	
	if (cpu_hz[index].max == 0)
		return cc;

	if (cc < cpu_hz[index].min - deviation
	    || cc > cpu_hz[index].max + deviation)
		return 0;

	return cc;
}



#define CALIBRATE_LATCH	0xffff
#define TIMEOUT_COUNT	0x100000

static unsigned long __init
calibrate_cc_with_pit(void)
{
	int cc, count = 0;

	
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	outb(0xb0, 0x43);		
	outb(CALIBRATE_LATCH & 0xff, 0x42);	
	outb(CALIBRATE_LATCH >> 8, 0x42);	

	cc = rpcc();
	do {
		count++;
	} while ((inb(0x61) & 0x20) == 0 && count < TIMEOUT_COUNT);
	cc = rpcc() - cc;

	
	if (count <= 1 || count == TIMEOUT_COUNT)
		return 0;

	return ((long)cc * PIT_TICK_RATE) / (CALIBRATE_LATCH + 1);
}


static unsigned long __init
rpcc_after_update_in_progress(void)
{
	do { } while (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP));
	do { } while (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP);

	return rpcc();
}

#ifndef CONFIG_SMP
static cycle_t read_rpcc(struct clocksource *cs)
{
	cycle_t ret = (cycle_t)rpcc();
	return ret;
}

static struct clocksource clocksource_rpcc = {
	.name                   = "rpcc",
	.rating                 = 300,
	.read                   = read_rpcc,
	.mask                   = CLOCKSOURCE_MASK(32),
	.flags                  = CLOCK_SOURCE_IS_CONTINUOUS
};

static inline void register_rpcc_clocksource(long cycle_freq)
{
	clocksource_register_hz(&clocksource_rpcc, cycle_freq);
}
#else 
static inline void register_rpcc_clocksource(long cycle_freq)
{
}
#endif 

void __init
time_init(void)
{
	unsigned int cc1, cc2;
	unsigned long cycle_freq, tolerance;
	long diff;

	
	if (!est_cycle_freq)
		est_cycle_freq = validate_cc_value(calibrate_cc_with_pit());

	cc1 = rpcc();

	
	if (!est_cycle_freq) {
		cc1 = rpcc_after_update_in_progress();
		cc2 = rpcc_after_update_in_progress();
		est_cycle_freq = validate_cc_value(cc2 - cc1);
		cc1 = cc2;
	}

	cycle_freq = hwrpb->cycle_freq;
	if (est_cycle_freq) {
		tolerance = cycle_freq / 4000;
		diff = cycle_freq - est_cycle_freq;
		if (diff < 0)
			diff = -diff;
		if ((unsigned long)diff > tolerance) {
			cycle_freq = est_cycle_freq;
			printk("HWRPB cycle frequency bogus.  "
			       "Estimated %lu Hz\n", cycle_freq);
		} else {
			est_cycle_freq = 0;
		}
	} else if (! validate_cc_value (cycle_freq)) {
		printk("HWRPB cycle frequency bogus, "
		       "and unable to estimate a proper value!\n");
	}

	__delay(1000000);


	if (HZ > (1<<16)) {
		extern void __you_loose (void);
		__you_loose();
	}

	register_rpcc_clocksource(cycle_freq);

	state.last_time = cc1;
	state.scaled_ticks_per_cycle
		= ((unsigned long) HZ << FIX_SHIFT) / cycle_freq;
	state.partial_tick = 0L;

	
	alpha_mv.init_rtc();
}

/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 *
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you won't notice until after reboot!
 */


static int
set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	
	spin_lock(&rtc_lock);
	
	save_control = CMOS_READ(RTC_CONTROL);
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	
	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		cmos_minutes = bcd2bin(cmos_minutes);

	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1) {
		
		real_minutes += 30;
	}
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			real_seconds = bin2bcd(real_seconds);
			real_minutes = bin2bcd(real_minutes);
		}
		CMOS_WRITE(real_seconds,RTC_SECONDS);
		CMOS_WRITE(real_minutes,RTC_MINUTES);
	} else {
		printk_once(KERN_NOTICE
		       "set_rtc_mmss: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
 		retval = -1;
	}

	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
	spin_unlock(&rtc_lock);

	return retval;
}
