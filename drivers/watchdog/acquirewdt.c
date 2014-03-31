/*
 *	Acquire Single Board Computer Watchdog Timer driver
 *
 *	Based on wdt.c. Original copyright messages:
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
 *	    Added nowayout module option to override CONFIG_WATCHDOG_NOWAYOUT
 *	    Can't add timeout - driver doesn't allow changing value
 */



#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>		
#include <linux/moduleparam.h>		
#include <linux/types.h>		
#include <linux/errno.h>		
#include <linux/kernel.h>		
#include <linux/miscdevice.h>		
#include <linux/watchdog.h>		
#include <linux/fs.h>			
#include <linux/ioport.h>		
#include <linux/platform_device.h>	
#include <linux/init.h>			
#include <linux/uaccess.h>		
#include <linux/io.h>			

#define DRV_NAME "acquirewdt"
#define WATCHDOG_NAME "Acquire WDT"
#define WATCHDOG_HEARTBEAT 0

static struct platform_device *acq_platform_device;
static unsigned long acq_is_open;
static char expect_close;

static int wdt_stop = 0x43;
module_param(wdt_stop, int, 0);
MODULE_PARM_DESC(wdt_stop, "Acquire WDT 'stop' io port (default 0x43)");

static int wdt_start = 0x443;
module_param(wdt_start, int, 0);
MODULE_PARM_DESC(wdt_start, "Acquire WDT 'start' io port (default 0x443)");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static void acq_keepalive(void)
{
	
	inb_p(wdt_start);
}

static void acq_stop(void)
{
	
	inb_p(wdt_stop);
}


static ssize_t acq_write(struct file *file, const char __user *buf,
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
		acq_keepalive();
	}
	return count;
}

static long acq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int options, retval = -EINVAL;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options = WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity = WATCHDOG_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_SETOPTIONS:
	{
		if (get_user(options, p))
			return -EFAULT;
		if (options & WDIOS_DISABLECARD) {
			acq_stop();
			retval = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			acq_keepalive();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		acq_keepalive();
		return 0;

	case WDIOC_GETTIMEOUT:
		return put_user(WATCHDOG_HEARTBEAT, p);

	default:
		return -ENOTTY;
	}
}

static int acq_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &acq_is_open))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	
	acq_keepalive();
	return nonseekable_open(inode, file);
}

static int acq_close(struct inode *inode, struct file *file)
{
	if (expect_close == 42) {
		acq_stop();
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		acq_keepalive();
	}
	clear_bit(0, &acq_is_open);
	expect_close = 0;
	return 0;
}


static const struct file_operations acq_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= acq_write,
	.unlocked_ioctl	= acq_ioctl,
	.open		= acq_open,
	.release	= acq_close,
};

static struct miscdevice acq_miscdev = {
	.minor	= WATCHDOG_MINOR,
	.name	= "watchdog",
	.fops	= &acq_fops,
};


static int __devinit acq_probe(struct platform_device *dev)
{
	int ret;

	if (wdt_stop != wdt_start) {
		if (!request_region(wdt_stop, 1, WATCHDOG_NAME)) {
			pr_err("I/O address 0x%04x already in use\n", wdt_stop);
			ret = -EIO;
			goto out;
		}
	}

	if (!request_region(wdt_start, 1, WATCHDOG_NAME)) {
		pr_err("I/O address 0x%04x already in use\n", wdt_start);
		ret = -EIO;
		goto unreg_stop;
	}
	ret = misc_register(&acq_miscdev);
	if (ret != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto unreg_regions;
	}
	pr_info("initialized. (nowayout=%d)\n", nowayout);

	return 0;
unreg_regions:
	release_region(wdt_start, 1);
unreg_stop:
	if (wdt_stop != wdt_start)
		release_region(wdt_stop, 1);
out:
	return ret;
}

static int __devexit acq_remove(struct platform_device *dev)
{
	misc_deregister(&acq_miscdev);
	release_region(wdt_start, 1);
	if (wdt_stop != wdt_start)
		release_region(wdt_stop, 1);

	return 0;
}

static void acq_shutdown(struct platform_device *dev)
{
	
	acq_stop();
}

static struct platform_driver acquirewdt_driver = {
	.probe		= acq_probe,
	.remove		= __devexit_p(acq_remove),
	.shutdown	= acq_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRV_NAME,
	},
};

static int __init acq_init(void)
{
	int err;

	pr_info("WDT driver for Acquire single board computer initialising\n");

	err = platform_driver_register(&acquirewdt_driver);
	if (err)
		return err;

	acq_platform_device = platform_device_register_simple(DRV_NAME,
								-1, NULL, 0);
	if (IS_ERR(acq_platform_device)) {
		err = PTR_ERR(acq_platform_device);
		goto unreg_platform_driver;
	}
	return 0;

unreg_platform_driver:
	platform_driver_unregister(&acquirewdt_driver);
	return err;
}

static void __exit acq_exit(void)
{
	platform_device_unregister(acq_platform_device);
	platform_driver_unregister(&acquirewdt_driver);
	pr_info("Watchdog Module Unloaded\n");
}

module_init(acq_init);
module_exit(acq_exit);

MODULE_AUTHOR("David Woodhouse");
MODULE_DESCRIPTION("Acquire Inc. Single Board Computer Watchdog Timer driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
