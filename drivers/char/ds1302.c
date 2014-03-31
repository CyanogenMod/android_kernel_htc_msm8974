/*!***************************************************************************
*!
*! FILE NAME  : ds1302.c
*!
*! DESCRIPTION: Implements an interface for the DS1302 RTC
*!
*! Functions exported: ds1302_readreg, ds1302_writereg, ds1302_init, get_rtc_status
*!
*! ---------------------------------------------------------------------------
*!
*! (C) Copyright 1999, 2000, 2001  Axis Communications AB, LUND, SWEDEN
*!
*!***************************************************************************/


#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/bcd.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <asm/rtc.h>
#if defined(CONFIG_M32R)
#include <asm/m32r.h>
#endif

#define RTC_MAJOR_NR 121 

static DEFINE_MUTEX(rtc_mutex);
static const char ds1302_name[] = "ds1302";

static void
out_byte_rtc(unsigned int reg_addr, unsigned char x)
{
	
	outw(0x0001,(unsigned long)PLD_RTCRSTODT);
	
	outw(((x<<8)|(reg_addr&0xff)),(unsigned long)PLD_RTCWRDATA);
	
	outw(0x0002,(unsigned long)PLD_RTCCR);
	
	while(inw((unsigned long)PLD_RTCCR));

	
	outw(0x0000,(unsigned long)PLD_RTCRSTODT);

}

static unsigned char
in_byte_rtc(unsigned int reg_addr)
{
	unsigned char retval;

	
	outw(0x0001,(unsigned long)PLD_RTCRSTODT);
	
	outw((reg_addr&0xff),(unsigned long)PLD_RTCRDDATA);
	
	outw(0x0001,(unsigned long)PLD_RTCCR);
	
	while(inw((unsigned long)PLD_RTCCR));

	
	retval=(inw((unsigned long)PLD_RTCRDDATA) & 0xff00)>>8;

	
	outw(0x0000,(unsigned long)PLD_RTCRSTODT);

	return retval;
}


static void
ds1302_wenable(void)
{
	out_byte_rtc(0x8e,0x00);
}


static void
ds1302_wdisable(void)
{
	out_byte_rtc(0x8e,0x80);
}




unsigned char
ds1302_readreg(int reg)
{
	unsigned char x;

	x=in_byte_rtc((0x81 | (reg << 1))); 

	return x;
}


void
ds1302_writereg(int reg, unsigned char val)
{
	ds1302_wenable();
	out_byte_rtc((0x80 | (reg << 1)),val);
	ds1302_wdisable();
}

void
get_rtc_time(struct rtc_time *rtc_tm)
{
	unsigned long flags;

	local_irq_save(flags);

	rtc_tm->tm_sec = CMOS_READ(RTC_SECONDS);
	rtc_tm->tm_min = CMOS_READ(RTC_MINUTES);
	rtc_tm->tm_hour = CMOS_READ(RTC_HOURS);
	rtc_tm->tm_mday = CMOS_READ(RTC_DAY_OF_MONTH);
	rtc_tm->tm_mon = CMOS_READ(RTC_MONTH);
	rtc_tm->tm_year = CMOS_READ(RTC_YEAR);

	local_irq_restore(flags);

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon);
	rtc_tm->tm_year = bcd2bin(rtc_tm->tm_year);


	if (rtc_tm->tm_year <= 69)
		rtc_tm->tm_year += 100;

	rtc_tm->tm_mon--;
}

static unsigned char days_in_mo[] =
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


static long rtc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;

	switch(cmd) {
		case RTC_RD_TIME:	
		{
			struct rtc_time rtc_tm;

			memset(&rtc_tm, 0, sizeof (struct rtc_time));
			mutex_lock(&rtc_mutex);
			get_rtc_time(&rtc_tm);
			mutex_unlock(&rtc_mutex);
			if (copy_to_user((struct rtc_time*)arg, &rtc_tm, sizeof(struct rtc_time)))
				return -EFAULT;
			return 0;
		}

		case RTC_SET_TIME:	
		{
			struct rtc_time rtc_tm;
			unsigned char mon, day, hrs, min, sec, leap_yr;
			unsigned int yrs;

			if (!capable(CAP_SYS_TIME))
				return -EPERM;

			if (copy_from_user(&rtc_tm, (struct rtc_time*)arg, sizeof(struct rtc_time)))
				return -EFAULT;

			yrs = rtc_tm.tm_year + 1900;
			mon = rtc_tm.tm_mon + 1;   
			day = rtc_tm.tm_mday;
			hrs = rtc_tm.tm_hour;
			min = rtc_tm.tm_min;
			sec = rtc_tm.tm_sec;


			if ((yrs < 1970) || (yrs > 2069))
				return -EINVAL;

			leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));

			if ((mon > 12) || (day == 0))
				return -EINVAL;

			if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr)))
				return -EINVAL;

			if ((hrs >= 24) || (min >= 60) || (sec >= 60))
				return -EINVAL;

			if (yrs >= 2000)
				yrs -= 2000;	
			else
				yrs -= 1900;	

			sec = bin2bcd(sec);
			min = bin2bcd(min);
			hrs = bin2bcd(hrs);
			day = bin2bcd(day);
			mon = bin2bcd(mon);
			yrs = bin2bcd(yrs);

			mutex_lock(&rtc_mutex);
			local_irq_save(flags);
			CMOS_WRITE(yrs, RTC_YEAR);
			CMOS_WRITE(mon, RTC_MONTH);
			CMOS_WRITE(day, RTC_DAY_OF_MONTH);
			CMOS_WRITE(hrs, RTC_HOURS);
			CMOS_WRITE(min, RTC_MINUTES);
			CMOS_WRITE(sec, RTC_SECONDS);
			local_irq_restore(flags);
			mutex_unlock(&rtc_mutex);

			return 0;
		}

		case RTC_SET_CHARGE: 
		{
			int tcs_val;

			if (!capable(CAP_SYS_TIME))
				return -EPERM;

			if(copy_from_user(&tcs_val, (int*)arg, sizeof(int)))
				return -EFAULT;

			mutex_lock(&rtc_mutex);
			tcs_val = RTC_TCR_PATTERN | (tcs_val & 0x0F);
			ds1302_writereg(RTC_TRICKLECHARGER, tcs_val);
			mutex_unlock(&rtc_mutex);
			return 0;
		}
		default:
			return -EINVAL;
	}
}

int
get_rtc_status(char *buf)
{
	char *p;
	struct rtc_time tm;

	p = buf;

	get_rtc_time(&tm);


	p += sprintf(p,
		"rtc_time\t: %02d:%02d:%02d\n"
		"rtc_date\t: %04d-%02d-%02d\n",
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

	return  p - buf;
}



static const struct file_operations rtc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= rtc_ioctl,
	.llseek		= noop_llseek,
};


#define MAGIC_PATTERN 0x42

static int __init
ds1302_probe(void)
{
	int retval, res, baur;

	baur=(boot_cpu_data.bus_clock/(2*1000*1000));

	printk("%s: Set PLD_RTCBAUR = %d\n", ds1302_name,baur);

	outw(0x0000,(unsigned long)PLD_RTCCR);
	outw(0x0000,(unsigned long)PLD_RTCRSTODT);
	outw(baur,(unsigned long)PLD_RTCBAUR);

	

	ds1302_wenable();
	
	
	out_byte_rtc(0xc0,MAGIC_PATTERN);

	
	if((res = in_byte_rtc(0xc1)) == MAGIC_PATTERN) {
		char buf[100];
		ds1302_wdisable();
		printk("%s: RTC found.\n", ds1302_name);
		get_rtc_status(buf);
		printk(buf);
		retval = 1;
	} else {
		printk("%s: RTC not found.\n", ds1302_name);
		retval = 0;
	}

	return retval;
}



int __init
ds1302_init(void)
{
	if (!ds1302_probe()) {
		return -1;
  	}
	return 0;
}

static int __init ds1302_register(void)
{
	ds1302_init();
	if (register_chrdev(RTC_MAJOR_NR, ds1302_name, &rtc_fops)) {
		printk(KERN_INFO "%s: unable to get major %d for rtc\n",
		       ds1302_name, RTC_MAJOR_NR);
		return -1;
	}
	return 0;
}

module_init(ds1302_register);
