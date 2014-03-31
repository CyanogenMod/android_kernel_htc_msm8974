/*
 *	IB700 Single Board Computer WDT driver
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
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/uaccess.h>


static struct platform_device *ibwdt_platform_device;
static unsigned long ibwdt_is_open;
static DEFINE_SPINLOCK(ibwdt_lock);
static char expect_close;

#define DRV_NAME "ib700wdt"


#define WDT_STOP 0x441
#define WDT_START 0x443

#define WATCHDOG_TIMEOUT 30		
static int timeout = WATCHDOG_TIMEOUT;	
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. 0<= timeout <=30, default="
		__MODULE_STRING(WATCHDOG_TIMEOUT) ".");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");



static void ibwdt_ping(void)
{
	int wd_margin = 15 - ((timeout + 1) / 2);

	spin_lock(&ibwdt_lock);

	
	outb_p(wd_margin, WDT_START);

	spin_unlock(&ibwdt_lock);
}

static void ibwdt_disable(void)
{
	spin_lock(&ibwdt_lock);
	outb_p(0, WDT_STOP);
	spin_unlock(&ibwdt_lock);
}

static int ibwdt_set_heartbeat(int t)
{
	if (t < 0 || t > 30)
		return -EINVAL;

	timeout = t;
	return 0;
}


static ssize_t ibwdt_write(struct file *file, const char __user *buf,
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
		ibwdt_ping();
	}
	return count;
}

static long ibwdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int new_margin;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	static const struct watchdog_info ident = {
		.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT
							| WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity = "IB700 WDT",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &ident, sizeof(ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_SETOPTIONS:
	{
		int options, retval = -EINVAL;

		if (get_user(options, p))
			return -EFAULT;

		if (options & WDIOS_DISABLECARD) {
			ibwdt_disable();
			retval = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			ibwdt_ping();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		ibwdt_ping();
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_margin, p))
			return -EFAULT;
		if (ibwdt_set_heartbeat(new_margin))
			return -EINVAL;
		ibwdt_ping();
		

	case WDIOC_GETTIMEOUT:
		return put_user(timeout, p);

	default:
		return -ENOTTY;
	}
	return 0;
}

static int ibwdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &ibwdt_is_open))
		return -EBUSY;
	if (nowayout)
		__module_get(THIS_MODULE);

	
	ibwdt_ping();
	return nonseekable_open(inode, file);
}

static int ibwdt_close(struct inode *inode, struct file *file)
{
	if (expect_close == 42) {
		ibwdt_disable();
	} else {
		pr_crit("WDT device closed unexpectedly.  WDT will not stop!\n");
		ibwdt_ping();
	}
	clear_bit(0, &ibwdt_is_open);
	expect_close = 0;
	return 0;
}


static const struct file_operations ibwdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= ibwdt_write,
	.unlocked_ioctl	= ibwdt_ioctl,
	.open		= ibwdt_open,
	.release	= ibwdt_close,
};

static struct miscdevice ibwdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &ibwdt_fops,
};


static int __devinit ibwdt_probe(struct platform_device *dev)
{
	int res;

#if WDT_START != WDT_STOP
	if (!request_region(WDT_STOP, 1, "IB700 WDT")) {
		pr_err("STOP method I/O %X is not available\n", WDT_STOP);
		res = -EIO;
		goto out_nostopreg;
	}
#endif

	if (!request_region(WDT_START, 1, "IB700 WDT")) {
		pr_err("START method I/O %X is not available\n", WDT_START);
		res = -EIO;
		goto out_nostartreg;
	}

	if (ibwdt_set_heartbeat(timeout)) {
		ibwdt_set_heartbeat(WATCHDOG_TIMEOUT);
		pr_info("timeout value must be 0<=x<=30, using %d\n", timeout);
	}

	res = misc_register(&ibwdt_miscdev);
	if (res) {
		pr_err("failed to register misc device\n");
		goto out_nomisc;
	}
	return 0;

out_nomisc:
	release_region(WDT_START, 1);
out_nostartreg:
#if WDT_START != WDT_STOP
	release_region(WDT_STOP, 1);
#endif
out_nostopreg:
	return res;
}

static int __devexit ibwdt_remove(struct platform_device *dev)
{
	misc_deregister(&ibwdt_miscdev);
	release_region(WDT_START, 1);
#if WDT_START != WDT_STOP
	release_region(WDT_STOP, 1);
#endif
	return 0;
}

static void ibwdt_shutdown(struct platform_device *dev)
{
	
	ibwdt_disable();
}

static struct platform_driver ibwdt_driver = {
	.probe		= ibwdt_probe,
	.remove		= __devexit_p(ibwdt_remove),
	.shutdown	= ibwdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRV_NAME,
	},
};

static int __init ibwdt_init(void)
{
	int err;

	pr_info("WDT driver for IB700 single board computer initialising\n");

	err = platform_driver_register(&ibwdt_driver);
	if (err)
		return err;

	ibwdt_platform_device = platform_device_register_simple(DRV_NAME,
								-1, NULL, 0);
	if (IS_ERR(ibwdt_platform_device)) {
		err = PTR_ERR(ibwdt_platform_device);
		goto unreg_platform_driver;
	}

	return 0;

unreg_platform_driver:
	platform_driver_unregister(&ibwdt_driver);
	return err;
}

static void __exit ibwdt_exit(void)
{
	platform_device_unregister(ibwdt_platform_device);
	platform_driver_unregister(&ibwdt_driver);
	pr_info("Watchdog Module Unloaded\n");
}

module_init(ibwdt_init);
module_exit(ibwdt_exit);

MODULE_AUTHOR("Charles Howes <chowes@vsol.net>");
MODULE_DESCRIPTION("IB700 SBC watchdog driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

