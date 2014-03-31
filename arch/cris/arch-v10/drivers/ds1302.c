/*!***************************************************************************
*!
*! FILE NAME  : ds1302.c
*!
*! DESCRIPTION: Implements an interface for the DS1302 RTC through Etrax I/O
*!
*! Functions exported: ds1302_readreg, ds1302_writereg, ds1302_init
*!
*! ---------------------------------------------------------------------------
*!
*! (C) Copyright 1999-2007 Axis Communications AB, LUND, SWEDEN
*!
*!***************************************************************************/


#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/bcd.h>
#include <linux/capability.h>

#include <asm/uaccess.h>
#include <arch/svinto.h>
#include <asm/io.h>
#include <asm/rtc.h>
#include <arch/io_interface_mux.h>

#include "i2c.h"

#define RTC_MAJOR_NR 121 

static DEFINE_MUTEX(ds1302_mutex);
static const char ds1302_name[] = "ds1302";


#ifdef CONFIG_ETRAX_DS1302_RST_ON_GENERIC_PORT
#define TK_RST_OUT(x) REG_SHADOW_SET(R_PORT_G_DATA,  port_g_data_shadow,  CONFIG_ETRAX_DS1302_RSTBIT, x)
#define TK_RST_DIR(x)
#else
#define TK_RST_OUT(x) REG_SHADOW_SET(R_PORT_PB_DATA, port_pb_data_shadow, CONFIG_ETRAX_DS1302_RSTBIT, x)
#define TK_RST_DIR(x) REG_SHADOW_SET(R_PORT_PB_DIR,  port_pb_dir_shadow,  CONFIG_ETRAX_DS1302_RSTBIT, x)
#endif


#define TK_SDA_OUT(x) REG_SHADOW_SET(R_PORT_PB_DATA, port_pb_data_shadow, CONFIG_ETRAX_DS1302_SDABIT, x)
#define TK_SCL_OUT(x) REG_SHADOW_SET(R_PORT_PB_DATA, port_pb_data_shadow, CONFIG_ETRAX_DS1302_SCLBIT, x)

#define TK_SDA_IN()   ((*R_PORT_PB_READ >> CONFIG_ETRAX_DS1302_SDABIT) & 1)
#define TK_SDA_DIR(x) REG_SHADOW_SET(R_PORT_PB_DIR,  port_pb_dir_shadow,  CONFIG_ETRAX_DS1302_SDABIT, x)
#define TK_SCL_DIR(x) REG_SHADOW_SET(R_PORT_PB_DIR,  port_pb_dir_shadow,  CONFIG_ETRAX_DS1302_SCLBIT, x)



static void tempudelay(int usecs) 
{
	volatile int loops;

	for(loops = usecs * 12; loops > 0; loops--)
		;	
}


static void
out_byte(unsigned char x) 
{
	int i;
	TK_SDA_DIR(1);
	for (i = 8; i--;) {
		
		TK_SCL_OUT(0);
		TK_SDA_OUT(x & 1);
		tempudelay(1);
		TK_SCL_OUT(1);
		tempudelay(1);
		x >>= 1;
	}
	TK_SDA_DIR(0);
}

static unsigned char
in_byte(void) 
{
	unsigned char x = 0;
	int i;

	TK_SDA_DIR(0);

	for (i = 8; i--;) {
		TK_SCL_OUT(0);
		tempudelay(1);
		x >>= 1;
		x |= (TK_SDA_IN() << 7);
		TK_SCL_OUT(1);
		tempudelay(1);
	}

	return x;
}


static void
start(void) 
{
	TK_SCL_OUT(0);
	tempudelay(1);
	TK_RST_OUT(0);
	tempudelay(5);
	TK_RST_OUT(1);	
}


static void
stop(void) 
{
	tempudelay(2);
	TK_RST_OUT(0);
}


static void
ds1302_wenable(void) 
{
	start(); 	
	out_byte(0x8e); 
	out_byte(0x00); 
	stop();
}


static void
ds1302_wdisable(void) 
{
	start();
	out_byte(0x8e); 
	out_byte(0x80); 
	stop();
}




unsigned char
ds1302_readreg(int reg) 
{
	unsigned char x;

	start();
	out_byte(0x81 | (reg << 1)); 
	x = in_byte();
	stop();

	return x;
}


void
ds1302_writereg(int reg, unsigned char val) 
{
#ifndef CONFIG_ETRAX_RTC_READONLY
	int do_writereg = 1;
#else
	int do_writereg = 0;

	if (reg == RTC_TRICKLECHARGER)
		do_writereg = 1;
#endif

	if (do_writereg) {
		ds1302_wenable();
		start();
		out_byte(0x80 | (reg << 1)); 
		out_byte(val);
		stop();
		ds1302_wdisable();
	}
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


static int rtc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;

	switch(cmd) {
		case RTC_RD_TIME:	
		{
			struct rtc_time rtc_tm;
						
			memset(&rtc_tm, 0, sizeof (struct rtc_time));
			get_rtc_time(&rtc_tm);						
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

			local_irq_save(flags);
			CMOS_WRITE(yrs, RTC_YEAR);
			CMOS_WRITE(mon, RTC_MONTH);
			CMOS_WRITE(day, RTC_DAY_OF_MONTH);
			CMOS_WRITE(hrs, RTC_HOURS);
			CMOS_WRITE(min, RTC_MINUTES);
			CMOS_WRITE(sec, RTC_SECONDS);
			local_irq_restore(flags);

			return 0;
		}

		case RTC_SET_CHARGE: 
		{
			int tcs_val;

			if (!capable(CAP_SYS_TIME))
				return -EPERM;
			
			if(copy_from_user(&tcs_val, (int*)arg, sizeof(int)))
				return -EFAULT;

			tcs_val = RTC_TCR_PATTERN | (tcs_val & 0x0F);
			ds1302_writereg(RTC_TRICKLECHARGER, tcs_val);
			return 0;
		}
		case RTC_VL_READ:
		{
			printk(KERN_WARNING "DS1302: RTC Voltage Low detection"
			       " is not supported\n");
			return 0;
		}
		case RTC_VL_CLR:
		{
			return 0;
		}
		default:
			return -ENOIOCTLCMD;
	}
}

static long rtc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&ds1302_mutex);
	ret = rtc_ioctl(file, cmd, arg);
	mutex_unlock(&ds1302_mutex);

	return ret;
}

static void
print_rtc_status(void)
{
	struct rtc_time tm;

	get_rtc_time(&tm);


	printk(KERN_INFO "rtc_time\t: %02d:%02d:%02d\n",
	       tm.tm_hour, tm.tm_min, tm.tm_sec);
	printk(KERN_INFO "rtc_date\t: %04d-%02d-%02d\n",
	       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}


static const struct file_operations rtc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = rtc_unlocked_ioctl,
	.llseek		= noop_llseek,
}; 


#define MAGIC_PATTERN 0x42

static int __init
ds1302_probe(void) 
{
	int retval, res; 

	TK_RST_DIR(1);
	TK_SCL_DIR(1);
	TK_SDA_DIR(0);
	
	

	ds1302_wenable();  
	start();
	out_byte(0xc0); 	
	out_byte(MAGIC_PATTERN); 
	start();
	out_byte(0xc1); 

	if((res = in_byte()) == MAGIC_PATTERN) {
		stop();
		ds1302_wdisable();
		printk(KERN_INFO "%s: RTC found.\n", ds1302_name);
		printk(KERN_INFO "%s: SDA, SCL, RST on PB%i, PB%i, %s%i\n",
		       ds1302_name,
		       CONFIG_ETRAX_DS1302_SDABIT,
		       CONFIG_ETRAX_DS1302_SCLBIT,
#ifdef CONFIG_ETRAX_DS1302_RST_ON_GENERIC_PORT
		       "GENIO",
#else
		       "PB",
#endif
		       CONFIG_ETRAX_DS1302_RSTBIT);
		       print_rtc_status();
		retval = 1;
	} else {
		stop();
		retval = 0;
	}

	return retval;
}



int __init
ds1302_init(void) 
{
#ifdef CONFIG_ETRAX_I2C
	i2c_init();
#endif

	if (!ds1302_probe()) {
#ifdef CONFIG_ETRAX_DS1302_RST_ON_GENERIC_PORT
#if CONFIG_ETRAX_DS1302_RSTBIT == 27
		if (cris_request_io_interface(if_ata, "ds1302/ATA")) {
			printk(KERN_WARNING "ds1302: Failed to get IO interface\n");
			return -1;
		}

#elif CONFIG_ETRAX_DS1302_RSTBIT == 0
		if (cris_io_interface_allocate_pins(if_gpio_grp_a,
						    'g',
						    CONFIG_ETRAX_DS1302_RSTBIT,
						    CONFIG_ETRAX_DS1302_RSTBIT)) {
			printk(KERN_WARNING "ds1302: Failed to get IO interface\n");
			return -1;
		}

		
		genconfig_shadow = ((genconfig_shadow &
 				     ~IO_MASK(R_GEN_CONFIG, g0dir)) |
 				   (IO_STATE(R_GEN_CONFIG, g0dir, out)));
		*R_GEN_CONFIG = genconfig_shadow;
#endif
		if (!ds1302_probe()) {
			printk(KERN_WARNING "%s: RTC not found.\n", ds1302_name);
			return -1;
		}
#else
		printk(KERN_WARNING "%s: RTC not found.\n", ds1302_name);
		return -1;
#endif
  	}
	
	ds1302_writereg(RTC_TRICKLECHARGER,
			RTC_TCR_PATTERN |(CONFIG_ETRAX_DS1302_TRICKLE_CHARGE & 0x0F));
        
	ds1302_writereg(RTC_SECONDS, (ds1302_readreg(RTC_SECONDS) & 0x7F));
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
