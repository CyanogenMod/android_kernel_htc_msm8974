
/*
 * omap_device implementation
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 * Paul Walmsley, Kevin Hilman
 *
 * Developed in collaboration with (alphabetical order): Benoit
 * Cousson, Thara Gopinath, Tony Lindgren, Rajendra Nayak, Vikram
 * Pandita, Sakari Poussa, Anand Sawant, Santosh Shilimkar, Richard
 * Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This code provides a consistent interface for OMAP device drivers
 * to control power management and interconnect properties of their
 * devices.
 *
 * In the medium- to long-term, this code should either be
 * a) implemented via arch-specific pointers in platform_data
 * or
 * b) implemented as a proper omap_bus/omap_device in Linux, no more
 *    platform_data func pointers
 *
 *
 * Guidelines for usage by driver authors:
 *
 * 1. These functions are intended to be used by device drivers via
 * function pointers in struct platform_data.  As an example,
 * omap_device_enable() should be passed to the driver as
 *
 * struct foo_driver_platform_data {
 * ...
 *      int (*device_enable)(struct platform_device *pdev);
 * ...
 * }
 *
 * Note that the generic "device_enable" name is used, rather than
 * "omap_device_enable".  This is so other architectures can pass in their
 * own enable/disable functions here.
 *
 * This should be populated during device setup:
 *
 * ...
 * pdata->device_enable = omap_device_enable;
 * ...
 *
 * 2. Drivers should first check to ensure the function pointer is not null
 * before calling it, as in:
 *
 * if (pdata->device_enable)
 *     pdata->device_enable(pdev);
 *
 * This allows other architectures that don't use similar device_enable()/
 * device_shutdown() functions to execute normally.
 *
 * ...
 *
 * Suggested usage by device drivers:
 *
 * During device initialization:
 * device_enable()
 *
 * During device idle:
 * (save remaining device context if necessary)
 * device_idle();
 *
 * During device resume:
 * device_enable();
 * (restore context if necessary)
 *
 * During device shutdown:
 * device_shutdown()
 * (device must be reinitialized at this point to use it again)
 *
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/notifier.h>

#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/clock.h>

#define USE_WAKEUP_LAT			0
#define IGNORE_WAKEUP_LAT		1

static int omap_early_device_register(struct platform_device *pdev);

static struct omap_device_pm_latency omap_default_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	}
};


static int _omap_device_activate(struct omap_device *od, u8 ignore_lat)
{
	struct timespec a, b, c;

	dev_dbg(&od->pdev->dev, "omap_device: activating\n");

	while (od->pm_lat_level > 0) {
		struct omap_device_pm_latency *odpl;
		unsigned long long act_lat = 0;

		od->pm_lat_level--;

		odpl = od->pm_lats + od->pm_lat_level;

		if (!ignore_lat &&
		    (od->dev_wakeup_lat <= od->_dev_wakeup_lat_limit))
			break;

		read_persistent_clock(&a);

		
		odpl->activate_func(od);

		read_persistent_clock(&b);

		c = timespec_sub(b, a);
		act_lat = timespec_to_ns(&c);

		dev_dbg(&od->pdev->dev,
			"omap_device: pm_lat %d: activate: elapsed time "
			"%llu nsec\n", od->pm_lat_level, act_lat);

		if (act_lat > odpl->activate_lat) {
			odpl->activate_lat_worst = act_lat;
			if (odpl->flags & OMAP_DEVICE_LATENCY_AUTO_ADJUST) {
				odpl->activate_lat = act_lat;
				dev_dbg(&od->pdev->dev,
					"new worst case activate latency "
					"%d: %llu\n",
					od->pm_lat_level, act_lat);
			} else
				dev_warn(&od->pdev->dev,
					 "activate latency %d "
					 "higher than exptected. (%llu > %d)\n",
					 od->pm_lat_level, act_lat,
					 odpl->activate_lat);
		}

		od->dev_wakeup_lat -= odpl->activate_lat;
	}

	return 0;
}

static int _omap_device_deactivate(struct omap_device *od, u8 ignore_lat)
{
	struct timespec a, b, c;

	dev_dbg(&od->pdev->dev, "omap_device: deactivating\n");

	while (od->pm_lat_level < od->pm_lats_cnt) {
		struct omap_device_pm_latency *odpl;
		unsigned long long deact_lat = 0;

		odpl = od->pm_lats + od->pm_lat_level;

		if (!ignore_lat &&
		    ((od->dev_wakeup_lat + odpl->activate_lat) >
		     od->_dev_wakeup_lat_limit))
			break;

		read_persistent_clock(&a);

		
		odpl->deactivate_func(od);

		read_persistent_clock(&b);

		c = timespec_sub(b, a);
		deact_lat = timespec_to_ns(&c);

		dev_dbg(&od->pdev->dev,
			"omap_device: pm_lat %d: deactivate: elapsed time "
			"%llu nsec\n", od->pm_lat_level, deact_lat);

		if (deact_lat > odpl->deactivate_lat) {
			odpl->deactivate_lat_worst = deact_lat;
			if (odpl->flags & OMAP_DEVICE_LATENCY_AUTO_ADJUST) {
				odpl->deactivate_lat = deact_lat;
				dev_dbg(&od->pdev->dev,
					"new worst case deactivate latency "
					"%d: %llu\n",
					od->pm_lat_level, deact_lat);
			} else
				dev_warn(&od->pdev->dev,
					 "deactivate latency %d "
					 "higher than exptected. (%llu > %d)\n",
					 od->pm_lat_level, deact_lat,
					 odpl->deactivate_lat);
		}

		od->dev_wakeup_lat += odpl->activate_lat;

		od->pm_lat_level++;
	}

	return 0;
}

static void _add_clkdev(struct omap_device *od, const char *clk_alias,
		       const char *clk_name)
{
	struct clk *r;
	struct clk_lookup *l;

	if (!clk_alias || !clk_name)
		return;

	dev_dbg(&od->pdev->dev, "Creating %s -> %s\n", clk_alias, clk_name);

	r = clk_get_sys(dev_name(&od->pdev->dev), clk_alias);
	if (!IS_ERR(r)) {
		dev_warn(&od->pdev->dev,
			 "alias %s already exists\n", clk_alias);
		clk_put(r);
		return;
	}

	r = omap_clk_get_by_name(clk_name);
	if (IS_ERR(r)) {
		dev_err(&od->pdev->dev,
			"omap_clk_get_by_name for %s failed\n", clk_name);
		return;
	}

	l = clkdev_alloc(r, clk_alias, dev_name(&od->pdev->dev));
	if (!l) {
		dev_err(&od->pdev->dev,
			"clkdev_alloc for %s failed\n", clk_alias);
		return;
	}

	clkdev_add(l);
}

static void _add_hwmod_clocks_clkdev(struct omap_device *od,
				     struct omap_hwmod *oh)
{
	int i;

	_add_clkdev(od, "fck", oh->main_clk);

	for (i = 0; i < oh->opt_clks_cnt; i++)
		_add_clkdev(od, oh->opt_clks[i].role, oh->opt_clks[i].clk);
}


static int omap_device_build_from_dt(struct platform_device *pdev)
{
	struct omap_hwmod **hwmods;
	struct omap_device *od;
	struct omap_hwmod *oh;
	struct device_node *node = pdev->dev.of_node;
	const char *oh_name;
	int oh_cnt, i, ret = 0;

	oh_cnt = of_property_count_strings(node, "ti,hwmods");
	if (!oh_cnt || IS_ERR_VALUE(oh_cnt)) {
		dev_dbg(&pdev->dev, "No 'hwmods' to build omap_device\n");
		return -ENODEV;
	}

	hwmods = kzalloc(sizeof(struct omap_hwmod *) * oh_cnt, GFP_KERNEL);
	if (!hwmods) {
		ret = -ENOMEM;
		goto odbfd_exit;
	}

	for (i = 0; i < oh_cnt; i++) {
		of_property_read_string_index(node, "ti,hwmods", i, &oh_name);
		oh = omap_hwmod_lookup(oh_name);
		if (!oh) {
			dev_err(&pdev->dev, "Cannot lookup hwmod '%s'\n",
				oh_name);
			ret = -EINVAL;
			goto odbfd_exit1;
		}
		hwmods[i] = oh;
	}

	od = omap_device_alloc(pdev, hwmods, oh_cnt, NULL, 0);
	if (!od) {
		dev_err(&pdev->dev, "Cannot allocate omap_device for :%s\n",
			oh_name);
		ret = PTR_ERR(od);
		goto odbfd_exit1;
	}

	if (of_get_property(node, "ti,no_idle_on_suspend", NULL))
		omap_device_disable_idle_on_suspend(pdev);

	pdev->dev.pm_domain = &omap_device_pm_domain;

odbfd_exit1:
	kfree(hwmods);
odbfd_exit:
	return ret;
}

static int _omap_device_notifier_call(struct notifier_block *nb,
				      unsigned long event, void *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	switch (event) {
	case BUS_NOTIFY_ADD_DEVICE:
		if (pdev->dev.of_node)
			omap_device_build_from_dt(pdev);
		break;

	case BUS_NOTIFY_DEL_DEVICE:
		if (pdev->archdata.od)
			omap_device_delete(pdev->archdata.od);
		break;
	}

	return NOTIFY_DONE;
}



int omap_device_get_context_loss_count(struct platform_device *pdev)
{
	struct omap_device *od;
	u32 ret = 0;

	od = to_omap_device(pdev);

	if (od->hwmods_cnt)
		ret = omap_hwmod_get_context_loss_count(od->hwmods[0]);

	return ret;
}

static int omap_device_count_resources(struct omap_device *od)
{
	int c = 0;
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		c += omap_hwmod_count_resources(od->hwmods[i]);

	pr_debug("omap_device: %s: counted %d total resources across %d "
		 "hwmods\n", od->pdev->name, c, od->hwmods_cnt);

	return c;
}

static int omap_device_fill_resources(struct omap_device *od,
				      struct resource *res)
{
	int c = 0;
	int i, r;

	for (i = 0; i < od->hwmods_cnt; i++) {
		r = omap_hwmod_fill_resources(od->hwmods[i], res);
		res += r;
		c += r;
	}

	return 0;
}

struct omap_device *omap_device_alloc(struct platform_device *pdev,
					struct omap_hwmod **ohs, int oh_cnt,
					struct omap_device_pm_latency *pm_lats,
					int pm_lats_cnt)
{
	int ret = -ENOMEM;
	struct omap_device *od;
	struct resource *res = NULL;
	int i, res_count;
	struct omap_hwmod **hwmods;

	od = kzalloc(sizeof(struct omap_device), GFP_KERNEL);
	if (!od) {
		ret = -ENOMEM;
		goto oda_exit1;
	}
	od->hwmods_cnt = oh_cnt;

	hwmods = kmemdup(ohs, sizeof(struct omap_hwmod *) * oh_cnt, GFP_KERNEL);
	if (!hwmods)
		goto oda_exit2;

	od->hwmods = hwmods;
	od->pdev = pdev;

	if (pdev->num_resources && pdev->resource)
		dev_warn(&pdev->dev, "%s(): resources already allocated %d\n",
			__func__, pdev->num_resources);

	res_count = omap_device_count_resources(od);
	if (res_count > 0) {
		dev_dbg(&pdev->dev, "%s(): resources allocated from hwmod %d\n",
			__func__, res_count);
		res = kzalloc(sizeof(struct resource) * res_count, GFP_KERNEL);
		if (!res)
			goto oda_exit3;

		omap_device_fill_resources(od, res);

		ret = platform_device_add_resources(pdev, res, res_count);
		kfree(res);

		if (ret)
			goto oda_exit3;
	}

	if (!pm_lats) {
		pm_lats = omap_default_latency;
		pm_lats_cnt = ARRAY_SIZE(omap_default_latency);
	}

	od->pm_lats_cnt = pm_lats_cnt;
	od->pm_lats = kmemdup(pm_lats,
			sizeof(struct omap_device_pm_latency) * pm_lats_cnt,
			GFP_KERNEL);
	if (!od->pm_lats)
		goto oda_exit3;

	pdev->archdata.od = od;

	for (i = 0; i < oh_cnt; i++) {
		hwmods[i]->od = od;
		_add_hwmod_clocks_clkdev(od, hwmods[i]);
	}

	return od;

oda_exit3:
	kfree(hwmods);
oda_exit2:
	kfree(od);
oda_exit1:
	dev_err(&pdev->dev, "omap_device: build failed (%d)\n", ret);

	return ERR_PTR(ret);
}

void omap_device_delete(struct omap_device *od)
{
	if (!od)
		return;

	od->pdev->archdata.od = NULL;
	kfree(od->pm_lats);
	kfree(od->hwmods);
	kfree(od);
}

struct platform_device __init *omap_device_build(const char *pdev_name, int pdev_id,
				      struct omap_hwmod *oh, void *pdata,
				      int pdata_len,
				      struct omap_device_pm_latency *pm_lats,
				      int pm_lats_cnt, int is_early_device)
{
	struct omap_hwmod *ohs[] = { oh };

	if (!oh)
		return ERR_PTR(-EINVAL);

	return omap_device_build_ss(pdev_name, pdev_id, ohs, 1, pdata,
				    pdata_len, pm_lats, pm_lats_cnt,
				    is_early_device);
}

struct platform_device __init *omap_device_build_ss(const char *pdev_name, int pdev_id,
					 struct omap_hwmod **ohs, int oh_cnt,
					 void *pdata, int pdata_len,
					 struct omap_device_pm_latency *pm_lats,
					 int pm_lats_cnt, int is_early_device)
{
	int ret = -ENOMEM;
	struct platform_device *pdev;
	struct omap_device *od;

	if (!ohs || oh_cnt == 0 || !pdev_name)
		return ERR_PTR(-EINVAL);

	if (!pdata && pdata_len > 0)
		return ERR_PTR(-EINVAL);

	pdev = platform_device_alloc(pdev_name, pdev_id);
	if (!pdev) {
		ret = -ENOMEM;
		goto odbs_exit;
	}

	
	if (pdev->id != -1)
		dev_set_name(&pdev->dev, "%s.%d", pdev->name,  pdev->id);
	else
		dev_set_name(&pdev->dev, "%s", pdev->name);

	od = omap_device_alloc(pdev, ohs, oh_cnt, pm_lats, pm_lats_cnt);
	if (!od)
		goto odbs_exit1;

	ret = platform_device_add_data(pdev, pdata, pdata_len);
	if (ret)
		goto odbs_exit2;

	if (is_early_device)
		ret = omap_early_device_register(pdev);
	else
		ret = omap_device_register(pdev);
	if (ret)
		goto odbs_exit2;

	return pdev;

odbs_exit2:
	omap_device_delete(od);
odbs_exit1:
	platform_device_put(pdev);
odbs_exit:

	pr_err("omap_device: %s: build failed (%d)\n", pdev_name, ret);

	return ERR_PTR(ret);
}

static int __init omap_early_device_register(struct platform_device *pdev)
{
	struct platform_device *devices[1];

	devices[0] = pdev;
	early_platform_add_devices(devices, 1);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int _od_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	ret = pm_generic_runtime_suspend(dev);

	if (!ret)
		omap_device_idle(pdev);

	return ret;
}

static int _od_runtime_idle(struct device *dev)
{
	return pm_generic_runtime_idle(dev);
}

static int _od_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	omap_device_enable(pdev);

	return pm_generic_runtime_resume(dev);
}
#endif

#ifdef CONFIG_SUSPEND
static int _od_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *od = to_omap_device(pdev);
	int ret;

	ret = pm_generic_suspend_noirq(dev);

	if (!ret && !pm_runtime_status_suspended(dev)) {
		if (pm_generic_runtime_suspend(dev) == 0) {
			if (!(od->flags & OMAP_DEVICE_NO_IDLE_ON_SUSPEND))
				omap_device_idle(pdev);
			od->flags |= OMAP_DEVICE_SUSPENDED;
		}
	}

	return ret;
}

static int _od_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *od = to_omap_device(pdev);

	if ((od->flags & OMAP_DEVICE_SUSPENDED) &&
	    !pm_runtime_status_suspended(dev)) {
		od->flags &= ~OMAP_DEVICE_SUSPENDED;
		if (!(od->flags & OMAP_DEVICE_NO_IDLE_ON_SUSPEND))
			omap_device_enable(pdev);
		pm_generic_runtime_resume(dev);
	}

	return pm_generic_resume_noirq(dev);
}
#else
#define _od_suspend_noirq NULL
#define _od_resume_noirq NULL
#endif

struct dev_pm_domain omap_device_pm_domain = {
	.ops = {
		SET_RUNTIME_PM_OPS(_od_runtime_suspend, _od_runtime_resume,
				   _od_runtime_idle)
		USE_PLATFORM_PM_SLEEP_OPS
		.suspend_noirq = _od_suspend_noirq,
		.resume_noirq = _od_resume_noirq,
	}
};

int omap_device_register(struct platform_device *pdev)
{
	pr_debug("omap_device: %s: registering\n", pdev->name);

	pdev->dev.pm_domain = &omap_device_pm_domain;
	return platform_device_add(pdev);
}



int omap_device_enable(struct platform_device *pdev)
{
	int ret;
	struct omap_device *od;

	od = to_omap_device(pdev);

	if (od->_state == OMAP_DEVICE_STATE_ENABLED) {
		dev_warn(&pdev->dev,
			 "omap_device: %s() called from invalid state %d\n",
			 __func__, od->_state);
		return -EINVAL;
	}

	
	if (od->_state == OMAP_DEVICE_STATE_UNKNOWN)
		od->pm_lat_level = od->pm_lats_cnt;

	ret = _omap_device_activate(od, IGNORE_WAKEUP_LAT);

	od->dev_wakeup_lat = 0;
	od->_dev_wakeup_lat_limit = UINT_MAX;
	od->_state = OMAP_DEVICE_STATE_ENABLED;

	return ret;
}

int omap_device_idle(struct platform_device *pdev)
{
	int ret;
	struct omap_device *od;

	od = to_omap_device(pdev);

	if (od->_state != OMAP_DEVICE_STATE_ENABLED) {
		dev_warn(&pdev->dev,
			 "omap_device: %s() called from invalid state %d\n",
			 __func__, od->_state);
		return -EINVAL;
	}

	ret = _omap_device_deactivate(od, USE_WAKEUP_LAT);

	od->_state = OMAP_DEVICE_STATE_IDLE;

	return ret;
}

int omap_device_shutdown(struct platform_device *pdev)
{
	int ret, i;
	struct omap_device *od;

	od = to_omap_device(pdev);

	if (od->_state != OMAP_DEVICE_STATE_ENABLED &&
	    od->_state != OMAP_DEVICE_STATE_IDLE) {
		dev_warn(&pdev->dev,
			 "omap_device: %s() called from invalid state %d\n",
			 __func__, od->_state);
		return -EINVAL;
	}

	ret = _omap_device_deactivate(od, IGNORE_WAKEUP_LAT);

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_shutdown(od->hwmods[i]);

	od->_state = OMAP_DEVICE_STATE_SHUTDOWN;

	return ret;
}

int omap_device_align_pm_lat(struct platform_device *pdev,
			     u32 new_wakeup_lat_limit)
{
	int ret = -EINVAL;
	struct omap_device *od;

	od = to_omap_device(pdev);

	if (new_wakeup_lat_limit == od->dev_wakeup_lat)
		return 0;

	od->_dev_wakeup_lat_limit = new_wakeup_lat_limit;

	if (od->_state != OMAP_DEVICE_STATE_IDLE)
		return 0;
	else if (new_wakeup_lat_limit > od->dev_wakeup_lat)
		ret = _omap_device_deactivate(od, USE_WAKEUP_LAT);
	else if (new_wakeup_lat_limit < od->dev_wakeup_lat)
		ret = _omap_device_activate(od, USE_WAKEUP_LAT);

	return ret;
}

struct powerdomain *omap_device_get_pwrdm(struct omap_device *od)
{
	if (!od->hwmods_cnt)
		return NULL;

	return omap_hwmod_get_pwrdm(od->hwmods[0]);
}

void __iomem *omap_device_get_rt_va(struct omap_device *od)
{
	if (od->hwmods_cnt != 1)
		return NULL;

	return omap_hwmod_get_mpu_rt_va(od->hwmods[0]);
}

struct device *omap_device_get_by_hwmod_name(const char *oh_name)
{
	struct omap_hwmod *oh;

	if (!oh_name) {
		WARN(1, "%s: no hwmod name!\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	oh = omap_hwmod_lookup(oh_name);
	if (IS_ERR_OR_NULL(oh)) {
		WARN(1, "%s: no hwmod for %s\n", __func__,
			oh_name);
		return ERR_PTR(oh ? PTR_ERR(oh) : -ENODEV);
	}
	if (IS_ERR_OR_NULL(oh->od)) {
		WARN(1, "%s: no omap_device for %s\n", __func__,
			oh_name);
		return ERR_PTR(oh->od ? PTR_ERR(oh->od) : -ENODEV);
	}

	if (IS_ERR_OR_NULL(oh->od->pdev))
		return ERR_PTR(oh->od->pdev ? PTR_ERR(oh->od->pdev) : -ENODEV);

	return &oh->od->pdev->dev;
}
EXPORT_SYMBOL(omap_device_get_by_hwmod_name);


int omap_device_enable_hwmods(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_enable(od->hwmods[i]);

	
	return 0;
}

int omap_device_idle_hwmods(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_idle(od->hwmods[i]);

	
	return 0;
}

int omap_device_disable_clocks(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_disable_clocks(od->hwmods[i]);

	
	return 0;
}

int omap_device_enable_clocks(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_enable_clocks(od->hwmods[i]);

	
	return 0;
}

static struct notifier_block platform_nb = {
	.notifier_call = _omap_device_notifier_call,
};

static int __init omap_device_init(void)
{
	bus_register_notifier(&platform_bus_type, &platform_nb);
	return 0;
}
core_initcall(omap_device_init);
