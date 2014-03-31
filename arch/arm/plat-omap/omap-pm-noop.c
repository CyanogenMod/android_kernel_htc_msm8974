/*
 * omap-pm-noop.c - OMAP power management interface - dummy version
 *
 * This code implements the OMAP power management interface to
 * drivers, CPUIdle, CPUFreq, and DSP Bridge.  It is strictly for
 * debug/demonstration use, as it does nothing but printk() whenever a
 * function is called (when DEBUG is defined, below)
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Copyright (C) 2008-2009 Nokia Corporation
 * Paul Walmsley
 *
 * Interface developed by (in alphabetical order):
 * Karthik Dasu, Tony Lindgren, Rajendra Nayak, Sakari Poussa, Veeramanikandan
 * Raju, Anand Sawant, Igor Stoppa, Paul Walmsley, Richard Woodruff
 */

#undef DEBUG

#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <plat/omap-pm.h>
#include <plat/omap_device.h>

static bool off_mode_enabled;
static int dummy_context_loss_counter;


int omap_pm_set_max_mpu_wakeup_lat(struct device *dev, long t)
{
	if (!dev || t < -1) {
		WARN(1, "OMAP PM: %s: invalid parameter(s)", __func__);
		return -EINVAL;
	};

	if (t == -1)
		pr_debug("OMAP PM: remove max MPU wakeup latency constraint: "
			 "dev %s\n", dev_name(dev));
	else
		pr_debug("OMAP PM: add max MPU wakeup latency constraint: "
			 "dev %s, t = %ld usec\n", dev_name(dev), t);


	return 0;
}

int omap_pm_set_min_bus_tput(struct device *dev, u8 agent_id, unsigned long r)
{
	if (!dev || (agent_id != OCP_INITIATOR_AGENT &&
	    agent_id != OCP_TARGET_AGENT)) {
		WARN(1, "OMAP PM: %s: invalid parameter(s)", __func__);
		return -EINVAL;
	};

	if (r == 0)
		pr_debug("OMAP PM: remove min bus tput constraint: "
			 "dev %s for agent_id %d\n", dev_name(dev), agent_id);
	else
		pr_debug("OMAP PM: add min bus tput constraint: "
			 "dev %s for agent_id %d: rate %ld KiB\n",
			 dev_name(dev), agent_id, r);


	return 0;
}

int omap_pm_set_max_dev_wakeup_lat(struct device *req_dev, struct device *dev,
				   long t)
{
	if (!req_dev || !dev || t < -1) {
		WARN(1, "OMAP PM: %s: invalid parameter(s)", __func__);
		return -EINVAL;
	};

	if (t == -1)
		pr_debug("OMAP PM: remove max device latency constraint: "
			 "dev %s\n", dev_name(dev));
	else
		pr_debug("OMAP PM: add max device latency constraint: "
			 "dev %s, t = %ld usec\n", dev_name(dev), t);


	return 0;
}

int omap_pm_set_max_sdma_lat(struct device *dev, long t)
{
	if (!dev || t < -1) {
		WARN(1, "OMAP PM: %s: invalid parameter(s)", __func__);
		return -EINVAL;
	};

	if (t == -1)
		pr_debug("OMAP PM: remove max DMA latency constraint: "
			 "dev %s\n", dev_name(dev));
	else
		pr_debug("OMAP PM: add max DMA latency constraint: "
			 "dev %s, t = %ld usec\n", dev_name(dev), t);


	return 0;
}

int omap_pm_set_min_clk_rate(struct device *dev, struct clk *c, long r)
{
	if (!dev || !c || r < 0) {
		WARN(1, "OMAP PM: %s: invalid parameter(s)", __func__);
		return -EINVAL;
	}

	if (r == 0)
		pr_debug("OMAP PM: remove min clk rate constraint: "
			 "dev %s\n", dev_name(dev));
	else
		pr_debug("OMAP PM: add min clk rate constraint: "
			 "dev %s, rate = %ld Hz\n", dev_name(dev), r);


	return 0;
}


const struct omap_opp *omap_pm_dsp_get_opp_table(void)
{
	pr_debug("OMAP PM: DSP request for OPP table\n");


	return NULL;
}

void omap_pm_dsp_set_min_opp(u8 opp_id)
{
	if (opp_id == 0) {
		WARN_ON(1);
		return;
	}

	pr_debug("OMAP PM: DSP requests minimum VDD1 OPP to be %d\n", opp_id);

}


u8 omap_pm_dsp_get_opp(void)
{
	pr_debug("OMAP PM: DSP requests current DSP OPP ID\n");


	return 0;
}


struct cpufreq_frequency_table **omap_pm_cpu_get_freq_table(void)
{
	pr_debug("OMAP PM: CPUFreq request for frequency table\n");


	return NULL;
}

void omap_pm_cpu_set_freq(unsigned long f)
{
	if (f == 0) {
		WARN_ON(1);
		return;
	}

	pr_debug("OMAP PM: CPUFreq requests CPU frequency to be set to %lu\n",
		 f);

}

unsigned long omap_pm_cpu_get_freq(void)
{
	pr_debug("OMAP PM: CPUFreq requests current CPU frequency\n");


	return 0;
}

void omap_pm_enable_off_mode(void)
{
	off_mode_enabled = true;
}

void omap_pm_disable_off_mode(void)
{
	off_mode_enabled = false;
}


#ifdef CONFIG_ARCH_OMAP2PLUS

int omap_pm_get_dev_context_loss_count(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int count;

	if (WARN_ON(!dev))
		return -ENODEV;

	if (dev->pm_domain == &omap_device_pm_domain) {
		count = omap_device_get_context_loss_count(pdev);
	} else {
		WARN_ONCE(off_mode_enabled, "omap_pm: using dummy context loss counter; device %s should be converted to omap_device",
			  dev_name(dev));

		count = dummy_context_loss_counter;

		if (off_mode_enabled) {
			count++;
			count &= INT_MAX;
			dummy_context_loss_counter = count;
		}
	}

	pr_debug("OMAP PM: context loss count for dev %s = %d\n",
		 dev_name(dev), count);

	return count;
}

#else

int omap_pm_get_dev_context_loss_count(struct device *dev)
{
	return dummy_context_loss_counter;
}

#endif

int __init omap_pm_if_early_init(void)
{
	return 0;
}

int __init omap_pm_if_init(void)
{
	return 0;
}

void omap_pm_if_exit(void)
{
	
}

