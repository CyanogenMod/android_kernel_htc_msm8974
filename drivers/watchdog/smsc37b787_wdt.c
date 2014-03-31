/*
 *	SMsC 37B787 Watchdog Timer driver for Linux 2.6.x.x
 *
 *	Based on acquirewdt.c by Alan Cox <alan@lxorguk.ukuu.org.uk>
 *	and some other existing drivers
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	The authors do NOT admit liability nor provide warranty for
 *	any of this software. This material is provided "AS-IS" in
 *	the hope that it may be useful for others.
 *
 *	(C) Copyright 2003-2006  Sven Anders <anders@anduras.de>
 *
 *  History:
 *	2003 - Created version 1.0 for Linux 2.4.x.
 *	2006 - Ported to Linux 2.6, added nowayout and MAGICCLOSE
 *	       features. Released version 1.1
 *
 *  Theory of operation:
 *
 *	A Watchdog Timer (WDT) is a hardware circuit that can
 *	reset the computer system in case of a software fault.
 *	You probably knew that already.
 *
 *	Usually a userspace daemon will notify the kernel WDT driver
 *	via the /dev/watchdog special device file that userspace is
 *	still alive, at regular intervals.  When such a notification
 *	occurs, the driver will usually tell the hardware watchdog
 *	that everything is in order, and that the watchdog should wait
 *	for yet another little while to reset the system.
 *	If userspace fails (RAM error, kernel bug, whatever), the
 *	notifications cease to occur, and the hardware watchdog will
 *	reset the system (causing a reboot) after the timeout occurs.
 *
 * Create device with:
 *  mknod /dev/watchdog c 10 130
 *
 * For an example userspace keep-alive daemon, see:
 *   Documentation/watchdog/wdt.txt
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/uaccess.h>


#define SMSC_SUPPORT_MINUTES
#undef SMSC_SUPPORT_MINUTES

#define MAX_TIMEOUT     255

#define UNIT_SECOND     0
#define UNIT_MINUTE     1

#define VERSION		"1.1"

#define IOPORT		0x3F0
#define IOPORT_SIZE     2
#define IODEV_NO	8

static int unit = UNIT_SECOND;	
static int timeout = 60;	
static unsigned long timer_enabled;   

static char expect_close;       

static DEFINE_SPINLOCK(io_lock);

static bool nowayout = WATCHDOG_NOWAYOUT;



static inline void open_io_config(void)
{
	outb(0x55, IOPORT);
	mdelay(1);
	outb(0x55, IOPORT);
}

static inline void close_io_config(void)
{
	outb(0xAA, IOPORT);
}

static inline void select_io_device(unsigned char devno)
{
	outb(0x07, IOPORT);
	outb(devno, IOPORT+1);
}

static inline void write_io_cr(unsigned char reg, unsigned char data)
{
	outb(reg, IOPORT);
	outb(data, IOPORT+1);
}

static inline char read_io_cr(unsigned char reg)
{
	outb(reg, IOPORT);
	return inb(IOPORT+1);
}


static inline void gpio_bit12(unsigned char reg)
{
	write_io_cr(0xE2, reg);
}

static inline void gpio_bit13(unsigned char reg)
{
	write_io_cr(0xE3, reg);
}

static inline void wdt_timer_units(unsigned char new_units)
{
	write_io_cr(0xF1, new_units);
}

static inline void wdt_timeout_value(unsigned char new_timeout)
{
	write_io_cr(0xF2, new_timeout);
}

static inline void wdt_timer_conf(unsigned char conf)
{
	write_io_cr(0xF3, conf);
}

static inline void wdt_timer_ctrl(unsigned char reg)
{
	write_io_cr(0xF4, reg);
}



static void wb_smsc_wdt_initialize(void)
{
	unsigned char old;

	spin_lock(&io_lock);
	open_io_config();
	select_io_device(IODEV_NO);

	
	gpio_bit13(0x08);  
	gpio_bit12(0x0A);  
	
	wdt_timeout_value(0);

	
	wdt_timer_ctrl(0x00);

	
	wdt_timer_conf(0x00);

	
	old = read_io_cr(0xF1) & 0x7F;
	if (unit == UNIT_SECOND)
		old |= 0x80;	

	
	wdt_timer_units(old);

	close_io_config();
	spin_unlock(&io_lock);
}


static void wb_smsc_wdt_shutdown(void)
{
	spin_lock(&io_lock);
	open_io_config();
	select_io_device(IODEV_NO);

	
	gpio_bit13(0x09);
	gpio_bit12(0x09);

	
	wdt_timer_conf(0x00);

	
	wdt_timer_ctrl(0x00);

	
	wdt_timeout_value(0x00);

	close_io_config();
	spin_unlock(&io_lock);
}


static void wb_smsc_wdt_set_timeout(unsigned char new_timeout)
{
	spin_lock(&io_lock);
	open_io_config();
	select_io_device(IODEV_NO);

	
	wdt_timer_ctrl((new_timeout == 0) ? 0x00 : 0x02);

	
	wdt_timeout_value(new_timeout);

	close_io_config();
	spin_unlock(&io_lock);
}


static unsigned char wb_smsc_wdt_get_timeout(void)
{
	unsigned char set_timeout;

	spin_lock(&io_lock);
	open_io_config();
	select_io_device(IODEV_NO);
	set_timeout = read_io_cr(0xF2);
	close_io_config();
	spin_unlock(&io_lock);

	return set_timeout;
}


static void wb_smsc_wdt_disable(void)
{
	
	wb_smsc_wdt_set_timeout(0);
}


static void wb_smsc_wdt_enable(void)
{
	
	wb_smsc_wdt_set_timeout(timeout);
}


static void wb_smsc_wdt_reset_timer(void)
{
	spin_lock(&io_lock);
	open_io_config();
	select_io_device(IODEV_NO);

	
	wdt_timeout_value(timeout);
	wdt_timer_conf(0x08);

	close_io_config();
	spin_unlock(&io_lock);
}


static int wb_smsc_wdt_status(void)
{
	return (wb_smsc_wdt_get_timeout() == 0) ? 0 : WDIOF_KEEPALIVEPING;
}




static int wb_smsc_wdt_open(struct inode *inode, struct file *file)
{
	

	if (test_and_set_bit(0, &timer_enabled))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	
	wb_smsc_wdt_enable();

	pr_info("Watchdog enabled. Timeout set to %d %s\n",
		timeout, (unit == UNIT_SECOND) ? "second(s)" : "minute(s)");

	return nonseekable_open(inode, file);
}


static int wb_smsc_wdt_release(struct inode *inode, struct file *file)
{
	

	if (expect_close == 42) {
		wb_smsc_wdt_disable();
		pr_info("Watchdog disabled, sleeping again...\n");
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		wb_smsc_wdt_reset_timer();
	}

	clear_bit(0, &timer_enabled);
	expect_close = 0;
	return 0;
}


static ssize_t wb_smsc_wdt_write(struct file *file, const char __user *data,
				 size_t len, loff_t *ppos)
{
	
	if (len) {
		if (!nowayout) {
			size_t i;

			
			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}

		
		wb_smsc_wdt_reset_timer();
	}
	return len;
}


static long wb_smsc_wdt_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	int new_timeout;

	union {
		struct watchdog_info __user *ident;
		int __user *i;
	} uarg;

	static const struct watchdog_info ident = {
		.options =		WDIOF_KEEPALIVEPING |
					WDIOF_SETTIMEOUT |
					WDIOF_MAGICCLOSE,
		.firmware_version =	0,
		.identity =		"SMsC 37B787 Watchdog",
	};

	uarg.i = (int __user *)arg;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(uarg.ident, &ident, sizeof(ident))
								? -EFAULT : 0;
	case WDIOC_GETSTATUS:
		return put_user(wb_smsc_wdt_status(), uarg.i);
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, uarg.i);
	case WDIOC_SETOPTIONS:
	{
		int options, retval = -EINVAL;

		if (get_user(options, uarg.i))
			return -EFAULT;

		if (options & WDIOS_DISABLECARD) {
			wb_smsc_wdt_disable();
			retval = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			wb_smsc_wdt_enable();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		wb_smsc_wdt_reset_timer();
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_timeout, uarg.i))
			return -EFAULT;
		
		if (unit == UNIT_MINUTE)
			new_timeout /= 60;
		if (new_timeout < 0 || new_timeout > MAX_TIMEOUT)
			return -EINVAL;
		timeout = new_timeout;
		wb_smsc_wdt_set_timeout(timeout);
		
	case WDIOC_GETTIMEOUT:
		new_timeout = timeout;
		if (unit == UNIT_MINUTE)
			new_timeout *= 60;
		return put_user(new_timeout, uarg.i);
	default:
		return -ENOTTY;
	}
}


static int wb_smsc_wdt_notify_sys(struct notifier_block *this,
					unsigned long code, void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT) {
		
		timeout = 0;
		wb_smsc_wdt_disable();
	}
	return NOTIFY_DONE;
}


static const struct file_operations wb_smsc_wdt_fops = {
	.owner	  = THIS_MODULE,
	.llseek		= no_llseek,
	.write		= wb_smsc_wdt_write,
	.unlocked_ioctl	= wb_smsc_wdt_ioctl,
	.open		= wb_smsc_wdt_open,
	.release	= wb_smsc_wdt_release,
};

static struct notifier_block wb_smsc_wdt_notifier = {
	.notifier_call  = wb_smsc_wdt_notify_sys,
};

static struct miscdevice wb_smsc_wdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &wb_smsc_wdt_fops,
};



static int __init wb_smsc_wdt_init(void)
{
	int ret;

	pr_info("SMsC 37B787 watchdog component driver "
		VERSION " initialising...\n");

	if (!request_region(IOPORT, IOPORT_SIZE, "SMsC 37B787 watchdog")) {
		pr_err("Unable to register IO port %#x\n", IOPORT);
		ret = -EBUSY;
		goto out_pnp;
	}

	
	if (timeout > MAX_TIMEOUT)
		timeout = MAX_TIMEOUT;

	
	wb_smsc_wdt_initialize();

	ret = register_reboot_notifier(&wb_smsc_wdt_notifier);
	if (ret) {
		pr_err("Unable to register reboot notifier err = %d\n", ret);
		goto out_io;
	}

	ret = misc_register(&wb_smsc_wdt_miscdev);
	if (ret) {
		pr_err("Unable to register miscdev on minor %d\n",
		       WATCHDOG_MINOR);
		goto out_rbt;
	}

	
	pr_info("Timeout set to %d %s\n",
		timeout, (unit == UNIT_SECOND) ? "second(s)" : "minute(s)");
	pr_info("Watchdog initialized and sleeping (nowayout=%d)...\n",
		nowayout);
out_clean:
	return ret;

out_rbt:
	unregister_reboot_notifier(&wb_smsc_wdt_notifier);

out_io:
	release_region(IOPORT, IOPORT_SIZE);

out_pnp:
	goto out_clean;
}


static void __exit wb_smsc_wdt_exit(void)
{
	
	if (!nowayout) {
		wb_smsc_wdt_shutdown();
		pr_info("Watchdog disabled\n");
	}

	misc_deregister(&wb_smsc_wdt_miscdev);
	unregister_reboot_notifier(&wb_smsc_wdt_notifier);
	release_region(IOPORT, IOPORT_SIZE);

	pr_info("SMsC 37B787 watchdog component driver removed\n");
}

module_init(wb_smsc_wdt_init);
module_exit(wb_smsc_wdt_exit);

MODULE_AUTHOR("Sven Anders <anders@anduras.de>");
MODULE_DESCRIPTION("Driver for SMsC 37B787 watchdog component (Version "
								VERSION ")");
MODULE_LICENSE("GPL");

MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

#ifdef SMSC_SUPPORT_MINUTES
module_param(unit, int, 0);
MODULE_PARM_DESC(unit,
		"set unit to use, 0=seconds or 1=minutes, default is 0");
#endif

module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "range is 1-255 units, default is 60");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");
