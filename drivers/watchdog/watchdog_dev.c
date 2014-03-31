/*
 *	watchdog_dev.c
 *
 *	(c) Copyright 2008-2011 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	(c) Copyright 2008-2011 Wim Van Sebroeck <wim@iguana.be>.
 *
 *
 *	This source code is part of the generic code that can be used
 *	by all the watchdog timer drivers.
 *
 *	This part of the generic code takes care of the following
 *	misc device: /dev/watchdog.
 *
 *	Based on source code of the following authors:
 *	  Matt Domsch <Matt_Domsch@dell.com>,
 *	  Rob Radez <rob@osinvestor.com>,
 *	  Rusty Lynch <rusty@linux.co.intel.com>
 *	  Satyam Sharma <satyam@infradead.org>
 *	  Randy Dunlap <randy.dunlap@oracle.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Alan Cox, CymruNet Ltd., Wim Van Sebroeck nor Iguana vzw.
 *	admit liability nor provide warranty for any of this software.
 *	This material is provided "AS-IS" and at no charge.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>	
#include <linux/types.h>	
#include <linux/errno.h>	
#include <linux/kernel.h>	
#include <linux/fs.h>		
#include <linux/watchdog.h>	
#include <linux/miscdevice.h>	
#include <linux/init.h>		
#include <linux/uaccess.h>	

static unsigned long watchdog_dev_busy;
static struct watchdog_device *wdd;


static int watchdog_ping(struct watchdog_device *wddev)
{
	if (test_bit(WDOG_ACTIVE, &wddev->status)) {
		if (wddev->ops->ping)
			return wddev->ops->ping(wddev);  
		else
			return wddev->ops->start(wddev); 
	}
	return 0;
}


static int watchdog_start(struct watchdog_device *wddev)
{
	int err;

	if (!test_bit(WDOG_ACTIVE, &wddev->status)) {
		err = wddev->ops->start(wddev);
		if (err < 0)
			return err;

		set_bit(WDOG_ACTIVE, &wddev->status);
	}
	return 0;
}


static int watchdog_stop(struct watchdog_device *wddev)
{
	int err = -EBUSY;

	if (test_bit(WDOG_NO_WAY_OUT, &wddev->status)) {
		pr_info("%s: nowayout prevents watchdog to be stopped!\n",
							wddev->info->identity);
		return err;
	}

	if (test_bit(WDOG_ACTIVE, &wddev->status)) {
		err = wddev->ops->stop(wddev);
		if (err < 0)
			return err;

		clear_bit(WDOG_ACTIVE, &wddev->status);
	}
	return 0;
}


static ssize_t watchdog_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	size_t i;
	char c;

	if (len == 0)
		return 0;

	clear_bit(WDOG_ALLOW_RELEASE, &wdd->status);

	
	for (i = 0; i != len; i++) {
		if (get_user(c, data + i))
			return -EFAULT;
		if (c == 'V')
			set_bit(WDOG_ALLOW_RELEASE, &wdd->status);
	}

	
	watchdog_ping(wdd);

	return len;
}


static long watchdog_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	unsigned int val;
	int err;

	if (wdd->ops->ioctl) {
		err = wdd->ops->ioctl(wdd, cmd, arg);
		if (err != -ENOIOCTLCMD)
			return err;
	}

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, wdd->info,
			sizeof(struct watchdog_info)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
		val = wdd->ops->status ? wdd->ops->status(wdd) : 0;
		return put_user(val, p);
	case WDIOC_GETBOOTSTATUS:
		return put_user(wdd->bootstatus, p);
	case WDIOC_SETOPTIONS:
		if (get_user(val, p))
			return -EFAULT;
		if (val & WDIOS_DISABLECARD) {
			err = watchdog_stop(wdd);
			if (err < 0)
				return err;
		}
		if (val & WDIOS_ENABLECARD) {
			err = watchdog_start(wdd);
			if (err < 0)
				return err;
		}
		return 0;
	case WDIOC_KEEPALIVE:
		if (!(wdd->info->options & WDIOF_KEEPALIVEPING))
			return -EOPNOTSUPP;
		watchdog_ping(wdd);
		return 0;
	case WDIOC_SETTIMEOUT:
		if ((wdd->ops->set_timeout == NULL) ||
		    !(wdd->info->options & WDIOF_SETTIMEOUT))
			return -EOPNOTSUPP;
		if (get_user(val, p))
			return -EFAULT;
		if ((wdd->max_timeout != 0) &&
		    (val < wdd->min_timeout || val > wdd->max_timeout))
				return -EINVAL;
		err = wdd->ops->set_timeout(wdd, val);
		if (err < 0)
			return err;
		watchdog_ping(wdd);
		
	case WDIOC_GETTIMEOUT:
		
		if (wdd->timeout == 0)
			return -EOPNOTSUPP;
		return put_user(wdd->timeout, p);
	case WDIOC_GETTIMELEFT:
		if (!wdd->ops->get_timeleft)
			return -EOPNOTSUPP;

		return put_user(wdd->ops->get_timeleft(wdd), p);
	default:
		return -ENOTTY;
	}
}


static int watchdog_open(struct inode *inode, struct file *file)
{
	int err = -EBUSY;

	
	if (test_and_set_bit(WDOG_DEV_OPEN, &wdd->status))
		return -EBUSY;

	if (!try_module_get(wdd->ops->owner))
		goto out;

	err = watchdog_start(wdd);
	if (err < 0)
		goto out_mod;

	
	return nonseekable_open(inode, file);

out_mod:
	module_put(wdd->ops->owner);
out:
	clear_bit(WDOG_DEV_OPEN, &wdd->status);
	return err;
}


static int watchdog_release(struct inode *inode, struct file *file)
{
	int err = -EBUSY;

	if (test_and_clear_bit(WDOG_ALLOW_RELEASE, &wdd->status) ||
	    !(wdd->info->options & WDIOF_MAGICCLOSE))
		err = watchdog_stop(wdd);

	
	if (err < 0) {
		pr_crit("%s: watchdog did not stop!\n", wdd->info->identity);
		watchdog_ping(wdd);
	}

	
	module_put(wdd->ops->owner);

	
	clear_bit(WDOG_DEV_OPEN, &wdd->status);

	return 0;
}

static const struct file_operations watchdog_fops = {
	.owner		= THIS_MODULE,
	.write		= watchdog_write,
	.unlocked_ioctl	= watchdog_ioctl,
	.open		= watchdog_open,
	.release	= watchdog_release,
};

static struct miscdevice watchdog_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &watchdog_fops,
};


int watchdog_dev_register(struct watchdog_device *watchdog)
{
	int err;

	
	if (test_and_set_bit(0, &watchdog_dev_busy)) {
		pr_err("only one watchdog can use /dev/watchdog\n");
		return -EBUSY;
	}

	wdd = watchdog;

	err = misc_register(&watchdog_miscdev);
	if (err != 0) {
		pr_err("%s: cannot register miscdev on minor=%d (err=%d)\n",
		       watchdog->info->identity, WATCHDOG_MINOR, err);
		goto out;
	}

	return 0;

out:
	wdd = NULL;
	clear_bit(0, &watchdog_dev_busy);
	return err;
}


int watchdog_dev_unregister(struct watchdog_device *watchdog)
{
	
	if (!test_bit(0, &watchdog_dev_busy) || !wdd)
		return -ENODEV;

	
	if (watchdog != wdd) {
		pr_err("%s: watchdog was not registered as /dev/watchdog\n",
		       watchdog->info->identity);
		return -ENODEV;
	}

	misc_deregister(&watchdog_miscdev);
	wdd = NULL;
	clear_bit(0, &watchdog_dev_busy);
	return 0;
}
