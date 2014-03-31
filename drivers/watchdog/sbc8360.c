/*
 *	SBC8360 Watchdog driver
 *
 *	(c) Copyright 2005 Webcon, Inc.
 *
 *	Based on ib700wdt.c, which is based on advantechwdt.c which is based
 *	on acquirewdt.c which is based on wdt.c.
 *
 *	(c) Copyright 2001 Charles Howes <chowes@vsol.net>
 *
 *	Based on advantechwdt.c which is based on acquirewdt.c which
 *	is based on wdt.c.
 *
 *	(c) Copyright 2000-2001 Marek Michalkiewicz <marekm@linux.org.pl>
 *
 *	Based on acquirewdt.c which is based on wdt.c.
 *	Original copyright messages:
 *
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 *	14-Dec-2001 Matt Domsch <Matt_Domsch@dell.com>
 *	     Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 *	     Added timeout module option to override default
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/uaccess.h>


static unsigned long sbc8360_is_open;
static char expect_close;


static int wd_times[64][2] = {
	{0, 1},			
	{1, 1},			
	{2, 1},			
	{3, 1},			
	{4, 1},			
	{5, 1},			
	{6, 1},			
	{7, 1},			
	{8, 1},			
	{9, 1},			
	{0xA, 1},		
	{0xB, 1},		
	{0xC, 1},		
	{0xD, 1},		
	{0xE, 1},		
	{0xF, 1},		
	{0, 2},			
	{1, 2},			
	{2, 2},			
	{3, 2},			
	{4, 2},			
	{5, 2},			
	{6, 2},			
	{7, 2},			
	{8, 2},			
	{9, 2},			
	{0xA, 2},		
	{0xB, 2},		
	{0xC, 2},		
	{0xD, 2},		
	{0xE, 2},		
	{0xF, 2},		
	{0, 3},			
	{1, 3},			
	{2, 3},			
	{3, 3},			
	{4, 3},			
	{5, 3},			
	{6, 3},			
	{7, 3},			
	{8, 3},			
	{9, 3},			
	{0xA, 3},		
	{0xB, 3},		
	{0xC, 3},		
	{0xD, 3},		
	{0xE, 3},		
	{0xF, 3},		
	{0, 4},			
	{1, 4},			
	{2, 4},			
	{3, 4},			
	{4, 4},			
	{5, 4},			
	{6, 4},			
	{7, 4},			
	{8, 4},			
	{9, 4},			
	{0xA, 4},		
	{0xB, 4},		
	{0xC, 4},		
	{0xD, 4},		
	{0xE, 4},		
	{0xF, 4}		
};

#define SBC8360_ENABLE 0x120
#define SBC8360_BASETIME 0x121

static int timeout = 27;
static int wd_margin = 0xB;
static int wd_multiplier = 2;
static bool nowayout = WATCHDOG_NOWAYOUT;

module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "Index into timeout table (0-63) (default=27 (60s))");
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		 "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static void sbc8360_activate(void)
{
	
	outb(0x0A, SBC8360_ENABLE);
	msleep_interruptible(100);
	outb(0x0B, SBC8360_ENABLE);
	msleep_interruptible(100);
	
	outb(wd_multiplier, SBC8360_ENABLE);
	msleep_interruptible(100);
	
}

static void sbc8360_ping(void)
{
	
	outb(wd_margin, SBC8360_BASETIME);
}

static void sbc8360_stop(void)
{
	
	outb(0, SBC8360_ENABLE);
}

static ssize_t sbc8360_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	if (count) {
		if (!nowayout) {
			size_t i;

			
			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		sbc8360_ping();
	}
	return count;
}

static int sbc8360_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &sbc8360_is_open))
		return -EBUSY;
	if (nowayout)
		__module_get(THIS_MODULE);

	
	sbc8360_activate();
	sbc8360_ping();
	return nonseekable_open(inode, file);
}

static int sbc8360_close(struct inode *inode, struct file *file)
{
	if (expect_close == 42)
		sbc8360_stop();
	else
		pr_crit("SBC8360 device closed unexpectedly.  SBC8360 will not stop!\n");

	clear_bit(0, &sbc8360_is_open);
	expect_close = 0;
	return 0;
}


static int sbc8360_notify_sys(struct notifier_block *this, unsigned long code,
			      void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		sbc8360_stop();	

	return NOTIFY_DONE;
}


static const struct file_operations sbc8360_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = sbc8360_write,
	.open = sbc8360_open,
	.release = sbc8360_close,
};

static struct miscdevice sbc8360_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &sbc8360_fops,
};


static struct notifier_block sbc8360_notifier = {
	.notifier_call = sbc8360_notify_sys,
};

static int __init sbc8360_init(void)
{
	int res;
	unsigned long int mseconds = 60000;

	if (timeout < 0 || timeout > 63) {
		pr_err("Invalid timeout index (must be 0-63)\n");
		res = -EINVAL;
		goto out;
	}

	if (!request_region(SBC8360_ENABLE, 1, "SBC8360")) {
		pr_err("ENABLE method I/O %X is not available\n",
		       SBC8360_ENABLE);
		res = -EIO;
		goto out;
	}
	if (!request_region(SBC8360_BASETIME, 1, "SBC8360")) {
		pr_err("BASETIME method I/O %X is not available\n",
		       SBC8360_BASETIME);
		res = -EIO;
		goto out_nobasetimereg;
	}

	res = register_reboot_notifier(&sbc8360_notifier);
	if (res) {
		pr_err("Failed to register reboot notifier\n");
		goto out_noreboot;
	}

	res = misc_register(&sbc8360_miscdev);
	if (res) {
		pr_err("failed to register misc device\n");
		goto out_nomisc;
	}

	wd_margin = wd_times[timeout][0];
	wd_multiplier = wd_times[timeout][1];

	if (wd_multiplier == 1)
		mseconds = (wd_margin + 1) * 500;
	else if (wd_multiplier == 2)
		mseconds = (wd_margin + 1) * 5000;
	else if (wd_multiplier == 3)
		mseconds = (wd_margin + 1) * 50000;
	else if (wd_multiplier == 4)
		mseconds = (wd_margin + 1) * 100000;

	
	pr_info("Timeout set at %ld ms\n", mseconds);

	return 0;

out_nomisc:
	unregister_reboot_notifier(&sbc8360_notifier);
out_noreboot:
	release_region(SBC8360_BASETIME, 1);
out_nobasetimereg:
	release_region(SBC8360_ENABLE, 1);
out:
	return res;
}

static void __exit sbc8360_exit(void)
{
	misc_deregister(&sbc8360_miscdev);
	unregister_reboot_notifier(&sbc8360_notifier);
	release_region(SBC8360_ENABLE, 1);
	release_region(SBC8360_BASETIME, 1);
}

module_init(sbc8360_init);
module_exit(sbc8360_exit);

MODULE_AUTHOR("Ian E. Morgan <imorgan@webcon.ca>");
MODULE_DESCRIPTION("SBC8360 watchdog driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.01");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

