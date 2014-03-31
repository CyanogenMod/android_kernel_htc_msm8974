/*
 * drivers/base/power/domain_governor.c - Governors for device PM domains.
 *
 * Copyright (C) 2011 Rafael J. Wysocki <rjw@sisk.pl>, Renesas Electronics Corp.
 *
 * This file is released under the GPLv2.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/hrtimer.h>

#ifdef CONFIG_PM_RUNTIME

bool default_stop_ok(struct device *dev)
{
	struct gpd_timing_data *td = &dev_gpd_data(dev)->td;

	dev_dbg(dev, "%s()\n", __func__);

	if (dev->power.max_time_suspended_ns < 0 || td->break_even_ns == 0)
		return true;

	return td->stop_latency_ns + td->start_latency_ns < td->break_even_ns
		&& td->break_even_ns < dev->power.max_time_suspended_ns;
}

static bool default_power_down_ok(struct dev_pm_domain *pd)
{
	struct generic_pm_domain *genpd = pd_to_genpd(pd);
	struct gpd_link *link;
	struct pm_domain_data *pdd;
	s64 min_dev_off_time_ns;
	s64 off_on_time_ns;
	ktime_t time_now = ktime_get();

	off_on_time_ns = genpd->power_off_latency_ns +
				genpd->power_on_latency_ns;
	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		if (pdd->dev->driver)
			off_on_time_ns +=
				to_gpd_data(pdd)->td.save_state_latency_ns;
	}

	list_for_each_entry(link, &genpd->master_links, master_node) {
		struct generic_pm_domain *sd = link->slave;
		s64 sd_max_off_ns = sd->max_off_time_ns;

		if (sd_max_off_ns < 0)
			continue;

		sd_max_off_ns -= ktime_to_ns(ktime_sub(time_now,
						       sd->power_off_time));
		if (sd_max_off_ns <= off_on_time_ns)
			return false;
	}

	min_dev_off_time_ns = -1;
	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		struct gpd_timing_data *td;
		struct device *dev = pdd->dev;
		s64 dev_off_time_ns;

		if (!dev->driver || dev->power.max_time_suspended_ns < 0)
			continue;

		td = &to_gpd_data(pdd)->td;
		dev_off_time_ns = dev->power.max_time_suspended_ns -
			(td->start_latency_ns + td->restore_state_latency_ns +
				ktime_to_ns(ktime_sub(time_now,
						dev->power.suspend_time)));
		if (dev_off_time_ns <= off_on_time_ns)
			return false;

		if (min_dev_off_time_ns > dev_off_time_ns
		    || min_dev_off_time_ns < 0)
			min_dev_off_time_ns = dev_off_time_ns;
	}

	if (min_dev_off_time_ns < 0) {
		genpd->max_off_time_ns = -1;
		return true;
	}

	min_dev_off_time_ns -= genpd->power_on_latency_ns;

	if (genpd->break_even_ns >
	    min_dev_off_time_ns - genpd->power_off_latency_ns)
		return false;

	genpd->max_off_time_ns = min_dev_off_time_ns;
	return true;
}

static bool always_on_power_down_ok(struct dev_pm_domain *domain)
{
	return false;
}

#else 

bool default_stop_ok(struct device *dev)
{
	return false;
}

#define default_power_down_ok	NULL
#define always_on_power_down_ok	NULL

#endif 

struct dev_power_governor simple_qos_governor = {
	.stop_ok = default_stop_ok,
	.power_down_ok = default_power_down_ok,
};

struct dev_power_governor pm_domain_always_on_gov = {
	.power_down_ok = always_on_power_down_ok,
	.stop_ok = default_stop_ok,
};
