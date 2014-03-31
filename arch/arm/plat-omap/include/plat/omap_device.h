/*
 * omap_device headers
 *
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * Developed in collaboration with (alphabetical order): Benoit
 * Cousson, Kevin Hilman, Tony Lindgren, Rajendra Nayak, Vikram
 * Pandita, Sakari Poussa, Anand Sawant, Santosh Shilimkar, Richard
 * Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Eventually this type of functionality should either be
 * a) implemented via arch-specific pointers in platform_device
 * or
 * b) implemented as a proper omap_bus/omap_device in Linux, no more
 *    platform_device
 *
 * omap_device differs from omap_hwmod in that it includes external
 * (e.g., board- and system-level) integration details.  omap_hwmod
 * stores hardware data that is invariant for a given OMAP chip.
 *
 * To do:
 * - GPIO integration
 * - regulator integration
 *
 */
#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_DEVICE_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_DEVICE_H

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <plat/omap_hwmod.h>

extern struct dev_pm_domain omap_device_pm_domain;

#define OMAP_DEVICE_STATE_UNKNOWN	0
#define OMAP_DEVICE_STATE_ENABLED	1
#define OMAP_DEVICE_STATE_IDLE		2
#define OMAP_DEVICE_STATE_SHUTDOWN	3

#define OMAP_DEVICE_SUSPENDED BIT(0)
#define OMAP_DEVICE_NO_IDLE_ON_SUSPEND BIT(1)

struct omap_device {
	struct platform_device		*pdev;
	struct omap_hwmod		**hwmods;
	struct omap_device_pm_latency	*pm_lats;
	u32				dev_wakeup_lat;
	u32				_dev_wakeup_lat_limit;
	u8				pm_lats_cnt;
	s8				pm_lat_level;
	u8				hwmods_cnt;
	u8				_state;
	u8                              flags;
};


int omap_device_enable(struct platform_device *pdev);
int omap_device_idle(struct platform_device *pdev);
int omap_device_shutdown(struct platform_device *pdev);


struct platform_device *omap_device_build(const char *pdev_name, int pdev_id,
				      struct omap_hwmod *oh, void *pdata,
				      int pdata_len,
				      struct omap_device_pm_latency *pm_lats,
				      int pm_lats_cnt, int is_early_device);

struct platform_device *omap_device_build_ss(const char *pdev_name, int pdev_id,
					 struct omap_hwmod **oh, int oh_cnt,
					 void *pdata, int pdata_len,
					 struct omap_device_pm_latency *pm_lats,
					 int pm_lats_cnt, int is_early_device);

struct omap_device *omap_device_alloc(struct platform_device *pdev,
				      struct omap_hwmod **ohs, int oh_cnt,
				      struct omap_device_pm_latency *pm_lats,
				      int pm_lats_cnt);
void omap_device_delete(struct omap_device *od);
int omap_device_register(struct platform_device *pdev);

void __iomem *omap_device_get_rt_va(struct omap_device *od);
struct device *omap_device_get_by_hwmod_name(const char *oh_name);

int omap_device_align_pm_lat(struct platform_device *pdev,
			     u32 new_wakeup_lat_limit);
struct powerdomain *omap_device_get_pwrdm(struct omap_device *od);
int omap_device_get_context_loss_count(struct platform_device *pdev);


int omap_device_idle_hwmods(struct omap_device *od);
int omap_device_enable_hwmods(struct omap_device *od);

int omap_device_disable_clocks(struct omap_device *od);
int omap_device_enable_clocks(struct omap_device *od);

struct omap_device_pm_latency {
	u32 deactivate_lat;
	u32 deactivate_lat_worst;
	int (*deactivate_func)(struct omap_device *od);
	u32 activate_lat;
	u32 activate_lat_worst;
	int (*activate_func)(struct omap_device *od);
	u32 flags;
};

#define OMAP_DEVICE_LATENCY_AUTO_ADJUST BIT(1)

static inline struct omap_device *to_omap_device(struct platform_device *pdev)
{
	return pdev ? pdev->archdata.od : NULL;
}

static inline
void omap_device_disable_idle_on_suspend(struct platform_device *pdev)
{
	struct omap_device *od = to_omap_device(pdev);

	od->flags |= OMAP_DEVICE_NO_IDLE_ON_SUSPEND;
}

#endif
