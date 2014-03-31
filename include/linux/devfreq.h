/*
 * devfreq: Generic Dynamic Voltage and Frequency Scaling (DVFS) Framework
 *	    for Non-CPU Devices.
 *
 * Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_DEVFREQ_H__
#define __LINUX_DEVFREQ_H__

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/opp.h>

#define DEVFREQ_NAME_LEN 16

struct devfreq;

struct devfreq_dev_status {
	
	unsigned long total_time;
	unsigned long busy_time;
	unsigned long current_frequency;
	void *private_data;
};

#define DEVFREQ_FLAG_LEAST_UPPER_BOUND		0x1

#define DEVFREQ_FLAG_FAST_HINT	0x2
#define DEVFREQ_FLAG_SLOW_HINT	0x4

struct devfreq_governor_data {
	const char *name;
	void *data;
};

struct devfreq_dev_profile {
	unsigned long initial_freq;
	unsigned int polling_ms;

	int (*target)(struct device *dev, unsigned long *freq, u32 flags);
	int (*get_dev_status)(struct device *dev,
			      struct devfreq_dev_status *stat);
	int (*get_cur_freq)(struct device *dev, unsigned long *freq);
	void (*exit)(struct device *dev);

	unsigned int *freq_table;
	unsigned int max_state;
	const struct devfreq_governor_data *governor_data;
	unsigned int num_governor_data;
};

struct devfreq_governor {
	struct list_head node;

	const char name[DEVFREQ_NAME_LEN];
	int (*get_target_freq)(struct devfreq *this, unsigned long *freq,
				u32 *flag);
	int (*event_handler)(struct devfreq *devfreq,
				unsigned int event, void *data);
};

struct devfreq {
	struct list_head node;

	struct mutex lock;
	struct device dev;
	struct devfreq_dev_profile *profile;
	const struct devfreq_governor *governor;
	char governor_name[DEVFREQ_NAME_LEN];
	struct notifier_block nb;
	struct delayed_work work;

	unsigned long previous_freq;

	void *data; 

	unsigned long min_freq;
	unsigned long max_freq;
	bool stop_polling;

	
	unsigned int total_trans;
	unsigned int *trans_table;
	unsigned long *time_in_state;
	unsigned long last_stat_updated;
};

#if defined(CONFIG_PM_DEVFREQ)
extern struct devfreq *devfreq_add_device(struct device *dev,
				  struct devfreq_dev_profile *profile,
				  const char *governor_name,
				  void *data);
extern int devfreq_remove_device(struct devfreq *devfreq);
extern int devfreq_suspend_device(struct devfreq *devfreq);
extern int devfreq_resume_device(struct devfreq *devfreq);

extern struct opp *devfreq_recommended_opp(struct device *dev,
					   unsigned long *freq, u32 flags);
extern int devfreq_register_opp_notifier(struct device *dev,
					 struct devfreq *devfreq);
extern int devfreq_unregister_opp_notifier(struct device *dev,
					   struct devfreq *devfreq);

#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
struct devfreq_simple_ondemand_data {
	unsigned int upthreshold;
	unsigned int downdifferential;
};
#endif

#else 
static struct devfreq *devfreq_add_device(struct device *dev,
					  struct devfreq_dev_profile *profile,
					  const char *governor_name,
					  void *data)
{
	return NULL;
}

static int devfreq_remove_device(struct devfreq *devfreq)
{
	return 0;
}

static int devfreq_suspend_device(struct devfreq *devfreq)
{
	return 0;
}

static int devfreq_resume_device(struct devfreq *devfreq)
{
	return 0;
}

static struct opp *devfreq_recommended_opp(struct device *dev,
					   unsigned long *freq, u32 flags)
{
	return -EINVAL;
}

static int devfreq_register_opp_notifier(struct device *dev,
					 struct devfreq *devfreq)
{
	return -EINVAL;
}

static int devfreq_unregister_opp_notifier(struct device *dev,
					   struct devfreq *devfreq)
{
	return -EINVAL;
}

#endif 

#endif 
