/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/irqreturn.h>

#ifndef MSM_IOMMU_PERFMON_H
#define MSM_IOMMU_PERFMON_H

struct iommu_pmon_counter {
	unsigned int counter_no;
	unsigned int absolute_counter_no;
	unsigned long value;
	unsigned long overflow_count;
	unsigned int enabled;
	int current_event_class;
	struct dentry *counter_dir;
	struct iommu_pmon_cnt_group *cnt_group;
};

struct iommu_pmon_cnt_group {
	unsigned int grp_no;
	unsigned int num_counters;
	struct iommu_pmon_counter *counters;
	struct dentry *group_dir;
	struct iommu_pmon *pmon;
};

struct iommu_info {
	const char *iommu_name;
	void *base;
	int evt_irq;
	struct device *iommu_dev;
	struct iommu_access_ops *ops;
	struct iommu_pm_hw_ops *hw_ops;
	unsigned int always_on;
};

struct iommu_pmon {
	struct dentry *iommu_dir;
	struct iommu_info iommu;
	struct list_head iommu_list;
	struct iommu_pmon_cnt_group *cnt_grp;
	u32 num_groups;
	u32 num_counters;
	u32 *event_cls_supported;
	u32 nevent_cls_supported;
	unsigned int enabled;
	unsigned int iommu_attach_count;
	struct mutex lock;
};

struct iommu_pm_hw_ops {
	void (*initialize_hw)(const struct iommu_pmon *);
	unsigned int (*is_hw_access_OK)(const struct iommu_pmon *);
	void (*grp_enable)(struct iommu_info *, unsigned int);
	void (*grp_disable)(struct iommu_info *, unsigned int);
	void (*enable_pm)(struct iommu_info *);
	void (*disable_pm)(struct iommu_info *);
	void (*reset_counters)(const struct iommu_info *);
	void (*check_for_overflow)(struct iommu_pmon *);
	irqreturn_t (*evt_ovfl_int_handler)(int, void *);
	void (*counter_enable)(struct iommu_info *,
			       struct iommu_pmon_counter *);
	void (*counter_disable)(struct iommu_info *,
			       struct iommu_pmon_counter *);
	void (*ovfl_int_enable)(struct iommu_info *,
				const struct iommu_pmon_counter *);
	void (*ovfl_int_disable)(struct iommu_info *,
				const struct iommu_pmon_counter *);
	void (*set_event_class)(struct iommu_pmon *pmon, unsigned int,
				unsigned int);
	unsigned int (*read_counter)(struct iommu_pmon_counter *);
};

#define MSM_IOMMU_PMU_NO_EVENT_CLASS -1

#ifdef CONFIG_MSM_IOMMU_PMON

struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v0(void);

struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v1(void);

struct iommu_pmon *msm_iommu_pm_alloc(struct device *iommu_dev);

void msm_iommu_pm_free(struct device *iommu_dev);

int msm_iommu_pm_iommu_register(struct iommu_pmon *info);

void msm_iommu_pm_iommu_unregister(struct device *dev);

void msm_iommu_attached(struct device *dev);

void msm_iommu_detached(struct device *dev);
#else
static inline struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v0(void)
{
	return NULL;
}

static inline struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v1(void)
{
	return NULL;
}

static inline struct iommu_pmon *msm_iommu_pm_alloc(struct device *iommu_dev)
{
	return NULL;
}

static inline void msm_iommu_pm_free(struct device *iommu_dev)
{
	return;
}

static inline int msm_iommu_pm_iommu_register(struct iommu_pmon *info)
{
	return -EIO;
}

static inline void msm_iommu_pm_iommu_unregister(struct device *dev)
{
}

static inline void msm_iommu_attached(struct device *dev)
{
}

static inline void msm_iommu_detached(struct device *dev)
{
}
#endif
#endif
