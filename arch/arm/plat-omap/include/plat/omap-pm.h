/*
 * omap-pm.h - OMAP power management interface
 *
 * Copyright (C) 2008-2010 Texas Instruments, Inc.
 * Copyright (C) 2008-2010 Nokia Corporation
 * Paul Walmsley
 *
 * Interface developed by (in alphabetical order): Karthik Dasu, Jouni
 * Högander, Tony Lindgren, Rajendra Nayak, Sakari Poussa,
 * Veeramanikandan Raju, Anand Sawant, Igor Stoppa, Paul Walmsley,
 * Richard Woodruff
 */

#ifndef ASM_ARM_ARCH_OMAP_OMAP_PM_H
#define ASM_ARM_ARCH_OMAP_OMAP_PM_H

#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/opp.h>

#define OCP_TARGET_AGENT		1
#define OCP_INITIATOR_AGENT		2

int __init omap_pm_if_early_init(void);

int __init omap_pm_if_init(void);

void omap_pm_if_exit(void);



int omap_pm_set_max_mpu_wakeup_lat(struct device *dev, long t);


int omap_pm_set_min_bus_tput(struct device *dev, u8 agent_id, unsigned long r);


int omap_pm_set_max_dev_wakeup_lat(struct device *req_dev, struct device *dev,
				   long t);


int omap_pm_set_max_sdma_lat(struct device *dev, long t);


int omap_pm_set_min_clk_rate(struct device *dev, struct clk *c, long r);


const struct omap_opp *omap_pm_dsp_get_opp_table(void);

void omap_pm_dsp_set_min_opp(u8 opp_id);

u8 omap_pm_dsp_get_opp(void);



struct cpufreq_frequency_table **omap_pm_cpu_get_freq_table(void);

void omap_pm_cpu_set_freq(unsigned long f);

unsigned long omap_pm_cpu_get_freq(void);



int omap_pm_get_dev_context_loss_count(struct device *dev);

void omap_pm_enable_off_mode(void);
void omap_pm_disable_off_mode(void);

#endif
