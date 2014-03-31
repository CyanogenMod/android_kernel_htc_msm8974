/*
 *	watchdog_core.c
 *
 *	(c) Copyright 2008-2011 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	(c) Copyright 2008-2011 Wim Van Sebroeck <wim@iguana.be>.
 *
 *	This source code is part of the generic code that can be used
 *	by all the watchdog timer drivers.
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
#include <linux/watchdog.h>	
#include <linux/init.h>		

#include "watchdog_dev.h"	

int watchdog_register_device(struct watchdog_device *wdd)
{
	int ret;

	if (wdd == NULL || wdd->info == NULL || wdd->ops == NULL)
		return -EINVAL;

	
	if (wdd->ops->start == NULL || wdd->ops->stop == NULL)
		return -EINVAL;

	if (wdd->min_timeout > wdd->max_timeout) {
		pr_info("Invalid min and max timeout values, resetting to 0!\n");
		wdd->min_timeout = 0;
		wdd->max_timeout = 0;
	}


	
	ret = watchdog_dev_register(wdd);
	if (ret) {
		pr_err("error registering /dev/watchdog (err=%d)\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(watchdog_register_device);

void watchdog_unregister_device(struct watchdog_device *wdd)
{
	int ret;

	if (wdd == NULL)
		return;

	ret = watchdog_dev_unregister(wdd);
	if (ret)
		pr_err("error unregistering /dev/watchdog (err=%d)\n", ret);
}
EXPORT_SYMBOL_GPL(watchdog_unregister_device);

MODULE_AUTHOR("Alan Cox <alan@lxorguk.ukuu.org.uk>");
MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("WatchDog Timer Driver Core");
MODULE_LICENSE("GPL");
