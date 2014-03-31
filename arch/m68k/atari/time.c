/*
 * linux/arch/m68k/atari/time.c
 *
 * Atari time and real time clock stuff
 *
 * Assembled of parts of former atari/config.c 97-12-18 by Roman Hodek
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/mc146818rtc.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/export.h>

#include <asm/atariints.h>

DEFINE_SPINLOCK(rtc_lock);
EXPORT_SYMBOL_GPL(rtc_lock);

void __init
atari_sched_init(irq_handler_t timer_routine)
{
    
    st_mfp.tim_dt_c = INT_TICKS;
    
    st_mfp.tim_ct_cd = (st_mfp.tim_ct_cd & 15) | 0x60;
    
    if (request_irq(IRQ_MFP_TIMC, timer_routine, IRQ_TYPE_SLOW,
		    "timer", timer_routine))
	pr_err("Couldn't register timer interrupt\n");
}


#define TICK_SIZE 10000

unsigned long atari_gettimeoffset (void)
{
  unsigned long ticks, offset = 0;

  
  ticks = st_mfp.tim_dt_c;
  
  if (ticks > INT_TICKS - INT_TICKS / 50)
    
    if (st_mfp.int_pn_b & (1 << 5))
      offset = TICK_SIZE;

  ticks = INT_TICKS - ticks;
  ticks = ticks * 10000L / INT_TICKS;

  return ticks + offset;
}


static void mste_read(struct MSTE_RTC *val)
{
#define COPY(v) val->v=(mste_rtc.v & 0xf)
	do {
		COPY(sec_ones) ; COPY(sec_tens) ; COPY(min_ones) ;
		COPY(min_tens) ; COPY(hr_ones) ; COPY(hr_tens) ;
		COPY(weekday) ; COPY(day_ones) ; COPY(day_tens) ;
		COPY(mon_ones) ; COPY(mon_tens) ; COPY(year_ones) ;
		COPY(year_tens) ;
	
	} while (val->sec_ones != (mste_rtc.sec_ones & 0xf));
#undef COPY
}

static void mste_write(struct MSTE_RTC *val)
{
#define COPY(v) mste_rtc.v=val->v
	do {
		COPY(sec_ones) ; COPY(sec_tens) ; COPY(min_ones) ;
		COPY(min_tens) ; COPY(hr_ones) ; COPY(hr_tens) ;
		COPY(weekday) ; COPY(day_ones) ; COPY(day_tens) ;
		COPY(mon_ones) ; COPY(mon_tens) ; COPY(year_ones) ;
		COPY(year_tens) ;
	
	} while (val->sec_ones != (mste_rtc.sec_ones & 0xf));
#undef COPY
}

#define	RTC_READ(reg)				\
    ({	unsigned char	__val;			\
		(void) atari_writeb(reg,&tt_rtc.regsel);	\
		__val = tt_rtc.data;		\
		__val;				\
	})

#define	RTC_WRITE(reg,val)			\
    do {					\
		atari_writeb(reg,&tt_rtc.regsel);	\
		tt_rtc.data = (val);		\
	} while(0)


#define HWCLK_POLL_INTERVAL	5

int atari_mste_hwclk( int op, struct rtc_time *t )
{
    int hour, year;
    int hr24=0;
    struct MSTE_RTC val;

    mste_rtc.mode=(mste_rtc.mode | 1);
    hr24=mste_rtc.mon_tens & 1;
    mste_rtc.mode=(mste_rtc.mode & ~1);

    if (op) {
        

        val.sec_ones = t->tm_sec % 10;
        val.sec_tens = t->tm_sec / 10;
        val.min_ones = t->tm_min % 10;
        val.min_tens = t->tm_min / 10;
        hour = t->tm_hour;
        if (!hr24) {
	    if (hour > 11)
		hour += 20 - 12;
	    if (hour == 0 || hour == 20)
		hour += 12;
        }
        val.hr_ones = hour % 10;
        val.hr_tens = hour / 10;
        val.day_ones = t->tm_mday % 10;
        val.day_tens = t->tm_mday / 10;
        val.mon_ones = (t->tm_mon+1) % 10;
        val.mon_tens = (t->tm_mon+1) / 10;
        year = t->tm_year - 80;
        val.year_ones = year % 10;
        val.year_tens = year / 10;
        val.weekday = t->tm_wday;
        mste_write(&val);
        mste_rtc.mode=(mste_rtc.mode | 1);
        val.year_ones = (year % 4);	
        mste_rtc.mode=(mste_rtc.mode & ~1);
    }
    else {
        mste_read(&val);
        t->tm_sec = val.sec_ones + val.sec_tens * 10;
        t->tm_min = val.min_ones + val.min_tens * 10;
        hour = val.hr_ones + val.hr_tens * 10;
	if (!hr24) {
	    if (hour == 12 || hour == 12 + 20)
		hour -= 12;
	    if (hour >= 20)
                hour += 12 - 20;
        }
	t->tm_hour = hour;
	t->tm_mday = val.day_ones + val.day_tens * 10;
        t->tm_mon  = val.mon_ones + val.mon_tens * 10 - 1;
        t->tm_year = val.year_ones + val.year_tens * 10 + 80;
        t->tm_wday = val.weekday;
    }
    return 0;
}

int atari_tt_hwclk( int op, struct rtc_time *t )
{
    int sec=0, min=0, hour=0, day=0, mon=0, year=0, wday=0;
    unsigned long	flags;
    unsigned char	ctrl;
    int pm = 0;

    ctrl = RTC_READ(RTC_CONTROL); 

    if (op) {
        

        sec  = t->tm_sec;
        min  = t->tm_min;
        hour = t->tm_hour;
        day  = t->tm_mday;
        mon  = t->tm_mon + 1;
        year = t->tm_year - atari_rtc_year_offset;
        wday = t->tm_wday + (t->tm_wday >= 0);

        if (!(ctrl & RTC_24H)) {
	    if (hour > 11) {
		pm = 0x80;
		if (hour != 12)
		    hour -= 12;
	    }
	    else if (hour == 0)
		hour = 12;
        }

        if (!(ctrl & RTC_DM_BINARY)) {
	    sec = bin2bcd(sec);
	    min = bin2bcd(min);
	    hour = bin2bcd(hour);
	    day = bin2bcd(day);
	    mon = bin2bcd(mon);
	    year = bin2bcd(year);
	    if (wday >= 0)
		wday = bin2bcd(wday);
        }
    }


    while( RTC_READ(RTC_FREQ_SELECT) & RTC_UIP ) {
	if (in_atomic() || irqs_disabled())
	    mdelay(1);
	else
	    schedule_timeout_interruptible(HWCLK_POLL_INTERVAL);
    }

    local_irq_save(flags);
    RTC_WRITE( RTC_CONTROL, ctrl | RTC_SET );
    if (!op) {
        sec  = RTC_READ( RTC_SECONDS );
        min  = RTC_READ( RTC_MINUTES );
        hour = RTC_READ( RTC_HOURS );
        day  = RTC_READ( RTC_DAY_OF_MONTH );
        mon  = RTC_READ( RTC_MONTH );
        year = RTC_READ( RTC_YEAR );
        wday = RTC_READ( RTC_DAY_OF_WEEK );
    }
    else {
        RTC_WRITE( RTC_SECONDS, sec );
        RTC_WRITE( RTC_MINUTES, min );
        RTC_WRITE( RTC_HOURS, hour + pm);
        RTC_WRITE( RTC_DAY_OF_MONTH, day );
        RTC_WRITE( RTC_MONTH, mon );
        RTC_WRITE( RTC_YEAR, year );
        if (wday >= 0) RTC_WRITE( RTC_DAY_OF_WEEK, wday );
    }
    RTC_WRITE( RTC_CONTROL, ctrl & ~RTC_SET );
    local_irq_restore(flags);

    if (!op) {
        

        if (hour & 0x80) {
	    hour &= ~0x80;
	    pm = 1;
	}

	if (!(ctrl & RTC_DM_BINARY)) {
	    sec = bcd2bin(sec);
	    min = bcd2bin(min);
	    hour = bcd2bin(hour);
	    day = bcd2bin(day);
	    mon = bcd2bin(mon);
	    year = bcd2bin(year);
	    wday = bcd2bin(wday);
        }

        if (!(ctrl & RTC_24H)) {
	    if (!pm && hour == 12)
		hour = 0;
	    else if (pm && hour != 12)
		hour += 12;
        }

        t->tm_sec  = sec;
        t->tm_min  = min;
        t->tm_hour = hour;
        t->tm_mday = day;
        t->tm_mon  = mon - 1;
        t->tm_year = year + atari_rtc_year_offset;
        t->tm_wday = wday - 1;
    }

    return( 0 );
}


int atari_mste_set_clock_mmss (unsigned long nowtime)
{
    short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;
    struct MSTE_RTC val;
    unsigned char rtc_minutes;

    mste_read(&val);
    rtc_minutes= val.min_ones + val.min_tens * 10;
    if ((rtc_minutes < real_minutes
         ? real_minutes - rtc_minutes
         : rtc_minutes - real_minutes) < 30)
    {
        val.sec_ones = real_seconds % 10;
        val.sec_tens = real_seconds / 10;
        val.min_ones = real_minutes % 10;
        val.min_tens = real_minutes / 10;
        mste_write(&val);
    }
    else
        return -1;
    return 0;
}

int atari_tt_set_clock_mmss (unsigned long nowtime)
{
    int retval = 0;
    short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;
    unsigned char save_control, save_freq_select, rtc_minutes;

    save_control = RTC_READ (RTC_CONTROL); 
    RTC_WRITE (RTC_CONTROL, save_control | RTC_SET);

    save_freq_select = RTC_READ (RTC_FREQ_SELECT); 
    RTC_WRITE (RTC_FREQ_SELECT, save_freq_select | RTC_DIV_RESET2);

    rtc_minutes = RTC_READ (RTC_MINUTES);
    if (!(save_control & RTC_DM_BINARY))
	rtc_minutes = bcd2bin(rtc_minutes);

    if ((rtc_minutes < real_minutes
         ? real_minutes - rtc_minutes
         : rtc_minutes - real_minutes) < 30)
        {
            if (!(save_control & RTC_DM_BINARY))
                {
		    real_seconds = bin2bcd(real_seconds);
		    real_minutes = bin2bcd(real_minutes);
                }
            RTC_WRITE (RTC_SECONDS, real_seconds);
            RTC_WRITE (RTC_MINUTES, real_minutes);
        }
    else
        retval = -1;

    RTC_WRITE (RTC_FREQ_SELECT, save_freq_select);
    RTC_WRITE (RTC_CONTROL, save_control);
    return retval;
}

