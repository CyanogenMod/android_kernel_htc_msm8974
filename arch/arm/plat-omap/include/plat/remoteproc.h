/*
 * Remote Processor - omap-specific bits
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PLAT_REMOTEPROC_H
#define _PLAT_REMOTEPROC_H

struct rproc_ops;
struct platform_device;

struct omap_rproc_pdata {
	const char *name;
	const char *oh_name;
	const char *oh_name_opt;
	const char *firmware;
	const char *mbox_name;
	const struct rproc_ops *ops;
	int (*device_enable) (struct platform_device *pdev);
	int (*device_shutdown) (struct platform_device *pdev);
};

#if defined(CONFIG_OMAP_REMOTEPROC) || defined(CONFIG_OMAP_REMOTEPROC_MODULE)

void __init omap_rproc_reserve_cma(void);

#else

void __init omap_rproc_reserve_cma(void)
{
}

#endif

#endif 
