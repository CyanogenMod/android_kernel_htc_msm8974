/*
 * GE watchdog userspace interface
 *
 * Author:  Martyn Welch <martyn.welch@ge.com>
 *
 * Copyright 2008 GE Intelligent Platforms Embedded Systems, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Based on: mv64x60_wdt.c (MV64X60 watchdog userspace interface)
 *   Author: James Chapman <jchapman@katalix.com>
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <sysdev/fsl_soc.h>

#define GEF_WDC_ENABLE_SHIFT	24
#define GEF_WDC_SERVICE_SHIFT	26
#define GEF_WDC_ENABLED_SHIFT	31

#define GEF_WDC_ENABLED_TRUE	1
#define GEF_WDC_ENABLED_FALSE	0

#define GEF_WDOG_FLAG_OPENED	0

static unsigned long wdt_flags;
static int wdt_status;
static void __iomem *gef_wdt_regs;
static int gef_wdt_timeout;
static int gef_wdt_count;
static unsigned int bus_clk;
static char expect_close;
static DEFINE_SPINLOCK(gef_wdt_spinlock);

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static int gef_wdt_toggle_wdc(int enabled_predicate, int field_shift)
{
	u32 data;
	u32 enabled;
	int ret = 0;

	spin_lock(&gef_wdt_spinlock);
	data = ioread32be(gef_wdt_regs);
	enabled = (data >> GEF_WDC_ENABLED_SHIFT) & 1;

	
	if ((enabled ^ enabled_predicate) == 0) {
		
		data = (1 << field_shift) | gef_wdt_count;
		iowrite32be(data, gef_wdt_regs);

		data = (2 << field_shift) | gef_wdt_count;
		iowrite32be(data, gef_wdt_regs);
		ret = 1;
	}
	spin_unlock(&gef_wdt_spinlock);

	return ret;
}

static void gef_wdt_service(void)
{
	gef_wdt_toggle_wdc(GEF_WDC_ENABLED_TRUE,
		GEF_WDC_SERVICE_SHIFT);
}

static void gef_wdt_handler_enable(void)
{
	if (gef_wdt_toggle_wdc(GEF_WDC_ENABLED_FALSE,
				   GEF_WDC_ENABLE_SHIFT)) {
		gef_wdt_service();
		pr_notice("watchdog activated\n");
	}
}

static void gef_wdt_handler_disable(void)
{
	if (gef_wdt_toggle_wdc(GEF_WDC_ENABLED_TRUE,
				   GEF_WDC_ENABLE_SHIFT))
		pr_notice("watchdog deactivated\n");
}

static void gef_wdt_set_timeout(unsigned int timeout)
{
	
	if (timeout > 0xFFFFFFFF / bus_clk)
		timeout = 0xFFFFFFFF / bus_clk;

	
	gef_wdt_count = (timeout * bus_clk) >> 8;
	gef_wdt_timeout = timeout;
}


static ssize_t gef_wdt_write(struct file *file, const char __user *data,
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
		gef_wdt_service();
	}

	return len;
}

static long gef_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	int timeout;
	int options;
	void __user *argp = (void __user *)arg;
	static const struct watchdog_info info = {
		.options =	WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE |
				WDIOF_KEEPALIVEPING,
		.firmware_version = 0,
		.identity = "GE watchdog",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &info, sizeof(info)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		if (put_user(wdt_status, (int __user *)argp))
			return -EFAULT;
		wdt_status &= ~WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_SETOPTIONS:
		if (get_user(options, (int __user *)argp))
			return -EFAULT;

		if (options & WDIOS_DISABLECARD)
			gef_wdt_handler_disable();

		if (options & WDIOS_ENABLECARD)
			gef_wdt_handler_enable();
		break;

	case WDIOC_KEEPALIVE:
		gef_wdt_service();
		wdt_status |= WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(timeout, (int __user *)argp))
			return -EFAULT;
		gef_wdt_set_timeout(timeout);
		

	case WDIOC_GETTIMEOUT:
		if (put_user(gef_wdt_timeout, (int __user *)argp))
			return -EFAULT;
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static int gef_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(GEF_WDOG_FLAG_OPENED, &wdt_flags))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	gef_wdt_handler_enable();

	return nonseekable_open(inode, file);
}

static int gef_wdt_release(struct inode *inode, struct file *file)
{
	if (expect_close == 42)
		gef_wdt_handler_disable();
	else {
		pr_crit("unexpected close, not stopping timer!\n");
		gef_wdt_service();
	}
	expect_close = 0;

	clear_bit(GEF_WDOG_FLAG_OPENED, &wdt_flags);

	return 0;
}

static const struct file_operations gef_wdt_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = gef_wdt_write,
	.unlocked_ioctl = gef_wdt_ioctl,
	.open = gef_wdt_open,
	.release = gef_wdt_release,
};

static struct miscdevice gef_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &gef_wdt_fops,
};


static int __devinit gef_wdt_probe(struct platform_device *dev)
{
	int timeout = 10;
	u32 freq;

	bus_clk = 133; 

	freq = fsl_get_sys_freq();
	if (freq != -1)
		bus_clk = freq;

	
	gef_wdt_regs = of_iomap(dev->dev.of_node, 0);
	if (gef_wdt_regs == NULL)
		return -ENOMEM;

	gef_wdt_set_timeout(timeout);

	gef_wdt_handler_disable();	

	return misc_register(&gef_wdt_miscdev);
}

static int __devexit gef_wdt_remove(struct platform_device *dev)
{
	misc_deregister(&gef_wdt_miscdev);

	gef_wdt_handler_disable();

	iounmap(gef_wdt_regs);

	return 0;
}

static const struct of_device_id gef_wdt_ids[] = {
	{
		.compatible = "gef,fpga-wdt",
	},
	{},
};

static struct platform_driver gef_wdt_driver = {
	.driver = {
		.name = "gef_wdt",
		.owner = THIS_MODULE,
		.of_match_table = gef_wdt_ids,
	},
	.probe		= gef_wdt_probe,
};

static int __init gef_wdt_init(void)
{
	pr_info("GE watchdog driver\n");
	return platform_driver_register(&gef_wdt_driver);
}

static void __exit gef_wdt_exit(void)
{
	platform_driver_unregister(&gef_wdt_driver);
}

module_init(gef_wdt_init);
module_exit(gef_wdt_exit);

MODULE_AUTHOR("Martyn Welch <martyn.welch@ge.com>");
MODULE_DESCRIPTION("GE watchdog driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:gef_wdt");
