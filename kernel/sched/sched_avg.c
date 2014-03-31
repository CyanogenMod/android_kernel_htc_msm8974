/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr);
static DEFINE_PER_CPU(unsigned long, iowait_prod_sum);
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static s64 last_get_time;

void sched_get_nr_running_avg(int *avg, int *iowait_avg)
{
	int cpu;
	u64 curr_time = sched_clock();
	u64 diff = curr_time - last_get_time;
	u64 tmp_avg = 0, tmp_iowait = 0;

	*avg = 0;
	*iowait_avg = 0;

	if (!diff)
		return;

	last_get_time = curr_time;
	
	for_each_possible_cpu(cpu) {
		unsigned long flags;

		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		tmp_avg += per_cpu(nr_prod_sum, cpu);
		tmp_avg += per_cpu(nr, cpu) *
			(curr_time - per_cpu(last_time, cpu));
		tmp_iowait = per_cpu(iowait_prod_sum, cpu);
		tmp_iowait +=  nr_iowait_cpu(cpu) *
			(curr_time - per_cpu(last_time, cpu));
		per_cpu(last_time, cpu) = curr_time;
		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(iowait_prod_sum, cpu) = 0;
		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}

	*avg = (int)div64_u64(tmp_avg * 100, diff);
	*iowait_avg = (int)div64_u64(tmp_iowait * 100, diff);

	BUG_ON(*avg < 0);
	pr_debug("%s - avg:%d\n", __func__, *avg);
	BUG_ON(*iowait_avg < 0);
	pr_debug("%s - avg:%d\n", __func__, *iowait_avg);
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

void sched_update_nr_prod(int cpu, unsigned long nr_running, bool inc)
{
	int diff;
	s64 curr_time;
	unsigned long flags;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	curr_time = sched_clock();
	diff = curr_time - per_cpu(last_time, cpu);
	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + (inc ? 1 : -1);

	BUG_ON(per_cpu(nr, cpu) < 0);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;
	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);
