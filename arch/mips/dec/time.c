/*
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Copyright (C) 2000, 2003  Maciej W. Rozycki
 *
 * This file contains the time handling details for PC-style clocks as
 * found in some MIPS systems.
 *
 */
#include <linux/bcd.h>
#include <linux/init.h>
#include <linux/mc146818rtc.h>
#include <linux/param.h>

#include <asm/cpu-features.h>
#include <asm/ds1287.h>
#include <asm/time.h>
#include <asm/dec/interrupts.h>
#include <asm/dec/ioasic.h>
#include <asm/dec/machtype.h>

void read_persistent_clock(struct timespec *ts)
{
	unsigned int year, mon, day, hour, min, sec, real_year;
	unsigned long flags;

	spin_lock_irqsave(&rtc_lock, flags);

	do {
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
		real_year = CMOS_READ(RTC_DEC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));

	spin_unlock_irqrestore(&rtc_lock, flags);

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		sec = bcd2bin(sec);
		min = bcd2bin(min);
		hour = bcd2bin(hour);
		day = bcd2bin(day);
		mon = bcd2bin(mon);
		year = bcd2bin(year);
	}

	year += real_year - 72 + 2000;

	ts->tv_sec = mktime(year, mon, day, hour, min, sec);
	ts->tv_nsec = 0;
}

/*
 * In order to set the CMOS clock precisely, rtc_mips_set_mmss has to
 * be called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later.  Check the Dallas
 * DS1287 data sheet for details.
 */
int rtc_mips_set_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	
	spin_lock(&rtc_lock);
	
	save_control = CMOS_READ(RTC_CONTROL);
	CMOS_WRITE((save_control | RTC_SET), RTC_CONTROL);

	
	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);
	CMOS_WRITE((save_freq_select | RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		cmos_minutes = bcd2bin(cmos_minutes);

	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15) / 30) & 1)
		real_minutes += 30;	
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			real_seconds = bin2bcd(real_seconds);
			real_minutes = bin2bcd(real_minutes);
		}
		CMOS_WRITE(real_seconds, RTC_SECONDS);
		CMOS_WRITE(real_minutes, RTC_MINUTES);
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

void __init plat_time_init(void)
{
	u32 start, end;
	int i = HZ / 10;

	
	ds1287_set_base_clock(HZ);

	if (cpu_has_counter) {
		while (!ds1287_timer_state())
			;

		start = read_c0_count();

		while (i--)
			while (!ds1287_timer_state())
				;

		end = read_c0_count();

		mips_hpt_frequency = (end - start) * 10;
		printk(KERN_INFO "MIPS counter frequency %dHz\n",
			mips_hpt_frequency);
	} else if (IOASIC)
		
		dec_ioasic_clocksource_init();

	ds1287_clockevent_init(dec_interrupt[DEC_IRQ_RTC]);
}
