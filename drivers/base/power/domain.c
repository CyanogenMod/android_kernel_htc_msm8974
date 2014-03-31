/*
 * drivers/base/power/domain.c - Common code related to device power domains.
 *
 * Copyright (C) 2011 Rafael J. Wysocki <rjw@sisk.pl>, Renesas Electronics Corp.
 *
 * This file is released under the GPLv2.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/export.h>

#define GENPD_DEV_CALLBACK(genpd, type, callback, dev)		\
({								\
	type (*__routine)(struct device *__d); 			\
	type __ret = (type)0;					\
								\
	__routine = genpd->dev_ops.callback; 			\
	if (__routine) {					\
		__ret = __routine(dev); 			\
	} else {						\
		__routine = dev_gpd_data(dev)->ops.callback;	\
		if (__routine) 					\
			__ret = __routine(dev);			\
	}							\
	__ret;							\
})

#define GENPD_DEV_TIMED_CALLBACK(genpd, type, callback, dev, field, name)	\
({										\
	ktime_t __start = ktime_get();						\
	type __retval = GENPD_DEV_CALLBACK(genpd, type, callback, dev);		\
	s64 __elapsed = ktime_to_ns(ktime_sub(ktime_get(), __start));		\
	struct generic_pm_domain_data *__gpd_data = dev_gpd_data(dev);		\
	if (__elapsed > __gpd_data->td.field) {					\
		__gpd_data->td.field = __elapsed;				\
		dev_warn(dev, name " latency exceeded, new value %lld ns\n",	\
			__elapsed);						\
	}									\
	__retval;								\
})

static LIST_HEAD(gpd_list);
static DEFINE_MUTEX(gpd_list_lock);

#ifdef CONFIG_PM

struct generic_pm_domain *dev_to_genpd(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->pm_domain))
		return ERR_PTR(-EINVAL);

	return pd_to_genpd(dev->pm_domain);
}

static int genpd_stop_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_TIMED_CALLBACK(genpd, int, stop, dev,
					stop_latency_ns, "stop");
}

static int genpd_start_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_TIMED_CALLBACK(genpd, int, start, dev,
					start_latency_ns, "start");
}

static int genpd_save_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_TIMED_CALLBACK(genpd, int, save_state, dev,
					save_state_latency_ns, "state save");
}

static int genpd_restore_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_TIMED_CALLBACK(genpd, int, restore_state, dev,
					restore_state_latency_ns,
					"state restore");
}

static bool genpd_sd_counter_dec(struct generic_pm_domain *genpd)
{
	bool ret = false;

	if (!WARN_ON(atomic_read(&genpd->sd_count) == 0))
		ret = !!atomic_dec_and_test(&genpd->sd_count);

	return ret;
}

static void genpd_sd_counter_inc(struct generic_pm_domain *genpd)
{
	atomic_inc(&genpd->sd_count);
	smp_mb__after_atomic_inc();
}

static void genpd_acquire_lock(struct generic_pm_domain *genpd)
{
	DEFINE_WAIT(wait);

	mutex_lock(&genpd->lock);
	for (;;) {
		prepare_to_wait(&genpd->status_wait_queue, &wait,
				TASK_UNINTERRUPTIBLE);
		if (genpd->status == GPD_STATE_ACTIVE
		    || genpd->status == GPD_STATE_POWER_OFF)
			break;
		mutex_unlock(&genpd->lock);

		schedule();

		mutex_lock(&genpd->lock);
	}
	finish_wait(&genpd->status_wait_queue, &wait);
}

static void genpd_release_lock(struct generic_pm_domain *genpd)
{
	mutex_unlock(&genpd->lock);
}

static void genpd_set_active(struct generic_pm_domain *genpd)
{
	if (genpd->resume_count == 0)
		genpd->status = GPD_STATE_ACTIVE;
}

int __pm_genpd_poweron(struct generic_pm_domain *genpd)
	__releases(&genpd->lock) __acquires(&genpd->lock)
{
	struct gpd_link *link;
	DEFINE_WAIT(wait);
	int ret = 0;

	
	for (;;) {
		prepare_to_wait(&genpd->status_wait_queue, &wait,
				TASK_UNINTERRUPTIBLE);
		if (genpd->status != GPD_STATE_WAIT_MASTER)
			break;
		mutex_unlock(&genpd->lock);

		schedule();

		mutex_lock(&genpd->lock);
	}
	finish_wait(&genpd->status_wait_queue, &wait);

	if (genpd->status == GPD_STATE_ACTIVE
	    || (genpd->prepared_count > 0 && genpd->suspend_power_off))
		return 0;

	if (genpd->status != GPD_STATE_POWER_OFF) {
		genpd_set_active(genpd);
		return 0;
	}

	list_for_each_entry(link, &genpd->slave_links, slave_node) {
		genpd_sd_counter_inc(link->master);
		genpd->status = GPD_STATE_WAIT_MASTER;

		mutex_unlock(&genpd->lock);

		ret = pm_genpd_poweron(link->master);

		mutex_lock(&genpd->lock);

		genpd->status = GPD_STATE_POWER_OFF;
		wake_up_all(&genpd->status_wait_queue);
		if (ret) {
			genpd_sd_counter_dec(link->master);
			goto err;
		}
	}

	if (genpd->power_on) {
		ktime_t time_start = ktime_get();
		s64 elapsed_ns;

		ret = genpd->power_on(genpd);
		if (ret)
			goto err;

		elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
		if (elapsed_ns > genpd->power_on_latency_ns) {
			genpd->power_on_latency_ns = elapsed_ns;
			if (genpd->name)
				pr_warning("%s: Power-on latency exceeded, "
					"new value %lld ns\n", genpd->name,
					elapsed_ns);
		}
	}

	genpd_set_active(genpd);

	return 0;

 err:
	list_for_each_entry_continue_reverse(link, &genpd->slave_links, slave_node)
		genpd_sd_counter_dec(link->master);

	return ret;
}

int pm_genpd_poweron(struct generic_pm_domain *genpd)
{
	int ret;

	mutex_lock(&genpd->lock);
	ret = __pm_genpd_poweron(genpd);
	mutex_unlock(&genpd->lock);
	return ret;
}

#endif 

#ifdef CONFIG_PM_RUNTIME

static int __pm_genpd_save_device(struct pm_domain_data *pdd,
				  struct generic_pm_domain *genpd)
	__releases(&genpd->lock) __acquires(&genpd->lock)
{
	struct generic_pm_domain_data *gpd_data = to_gpd_data(pdd);
	struct device *dev = pdd->dev;
	int ret = 0;

	if (gpd_data->need_restore)
		return 0;

	mutex_unlock(&genpd->lock);

	genpd_start_dev(genpd, dev);
	ret = genpd_save_dev(genpd, dev);
	genpd_stop_dev(genpd, dev);

	mutex_lock(&genpd->lock);

	if (!ret)
		gpd_data->need_restore = true;

	return ret;
}

static void __pm_genpd_restore_device(struct pm_domain_data *pdd,
				      struct generic_pm_domain *genpd)
	__releases(&genpd->lock) __acquires(&genpd->lock)
{
	struct generic_pm_domain_data *gpd_data = to_gpd_data(pdd);
	struct device *dev = pdd->dev;

	if (!gpd_data->need_restore)
		return;

	mutex_unlock(&genpd->lock);

	genpd_start_dev(genpd, dev);
	genpd_restore_dev(genpd, dev);
	genpd_stop_dev(genpd, dev);

	mutex_lock(&genpd->lock);

	gpd_data->need_restore = false;
}

static bool genpd_abort_poweroff(struct generic_pm_domain *genpd)
{
	return genpd->status == GPD_STATE_WAIT_MASTER
		|| genpd->status == GPD_STATE_ACTIVE || genpd->resume_count > 0;
}

void genpd_queue_power_off_work(struct generic_pm_domain *genpd)
{
	if (!work_pending(&genpd->power_off_work))
		queue_work(pm_wq, &genpd->power_off_work);
}

static int pm_genpd_poweroff(struct generic_pm_domain *genpd)
	__releases(&genpd->lock) __acquires(&genpd->lock)
{
	struct pm_domain_data *pdd;
	struct gpd_link *link;
	unsigned int not_suspended;
	int ret = 0;

 start:
	if (genpd->status == GPD_STATE_POWER_OFF
	    || genpd->status == GPD_STATE_WAIT_MASTER
	    || genpd->resume_count > 0 || genpd->prepared_count > 0)
		return 0;

	if (atomic_read(&genpd->sd_count) > 0)
		return -EBUSY;

	not_suspended = 0;
	list_for_each_entry(pdd, &genpd->dev_list, list_node)
		if (pdd->dev->driver && (!pm_runtime_suspended(pdd->dev)
		    || pdd->dev->power.irq_safe || to_gpd_data(pdd)->always_on))
			not_suspended++;

	if (not_suspended > genpd->in_progress)
		return -EBUSY;

	if (genpd->poweroff_task) {
		genpd->status = GPD_STATE_REPEAT;
		return 0;
	}

	if (genpd->gov && genpd->gov->power_down_ok) {
		if (!genpd->gov->power_down_ok(&genpd->domain))
			return -EAGAIN;
	}

	genpd->status = GPD_STATE_BUSY;
	genpd->poweroff_task = current;

	list_for_each_entry_reverse(pdd, &genpd->dev_list, list_node) {
		ret = atomic_read(&genpd->sd_count) == 0 ?
			__pm_genpd_save_device(pdd, genpd) : -EBUSY;

		if (genpd_abort_poweroff(genpd))
			goto out;

		if (ret) {
			genpd_set_active(genpd);
			goto out;
		}

		if (genpd->status == GPD_STATE_REPEAT) {
			genpd->poweroff_task = NULL;
			goto start;
		}
	}

	if (genpd->power_off) {
		ktime_t time_start;
		s64 elapsed_ns;

		if (atomic_read(&genpd->sd_count) > 0) {
			ret = -EBUSY;
			goto out;
		}

		time_start = ktime_get();

		ret = genpd->power_off(genpd);
		if (ret == -EBUSY) {
			genpd_set_active(genpd);
			goto out;
		}

		elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
		if (elapsed_ns > genpd->power_off_latency_ns) {
			genpd->power_off_latency_ns = elapsed_ns;
			if (genpd->name)
				pr_warning("%s: Power-off latency exceeded, "
					"new value %lld ns\n", genpd->name,
					elapsed_ns);
		}
	}

	genpd->status = GPD_STATE_POWER_OFF;
	genpd->power_off_time = ktime_get();

	
	list_for_each_entry_reverse(pdd, &genpd->dev_list, list_node) {
		struct gpd_timing_data *td = &to_gpd_data(pdd)->td;

		pm_runtime_update_max_time_suspended(pdd->dev,
					td->start_latency_ns +
					td->restore_state_latency_ns +
					genpd->power_on_latency_ns);
	}

	list_for_each_entry(link, &genpd->slave_links, slave_node) {
		genpd_sd_counter_dec(link->master);
		genpd_queue_power_off_work(link->master);
	}

 out:
	genpd->poweroff_task = NULL;
	wake_up_all(&genpd->status_wait_queue);
	return ret;
}

static void genpd_power_off_work_fn(struct work_struct *work)
{
	struct generic_pm_domain *genpd;

	genpd = container_of(work, struct generic_pm_domain, power_off_work);

	genpd_acquire_lock(genpd);
	pm_genpd_poweroff(genpd);
	genpd_release_lock(genpd);
}

static int pm_genpd_runtime_suspend(struct device *dev)
{
	struct generic_pm_domain *genpd;
	bool (*stop_ok)(struct device *__dev);
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	might_sleep_if(!genpd->dev_irq_safe);

	if (dev_gpd_data(dev)->always_on)
		return -EBUSY;

	stop_ok = genpd->gov ? genpd->gov->stop_ok : NULL;
	if (stop_ok && !stop_ok(dev))
		return -EBUSY;

	ret = genpd_stop_dev(genpd, dev);
	if (ret)
		return ret;

	pm_runtime_update_max_time_suspended(dev,
				dev_gpd_data(dev)->td.start_latency_ns);

	if (dev->power.irq_safe)
		return 0;

	mutex_lock(&genpd->lock);
	genpd->in_progress++;
	pm_genpd_poweroff(genpd);
	genpd->in_progress--;
	mutex_unlock(&genpd->lock);

	return 0;
}

static int pm_genpd_runtime_resume(struct device *dev)
{
	struct generic_pm_domain *genpd;
	DEFINE_WAIT(wait);
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	might_sleep_if(!genpd->dev_irq_safe);

	
	if (dev->power.irq_safe)
		goto out;

	mutex_lock(&genpd->lock);
	ret = __pm_genpd_poweron(genpd);
	if (ret) {
		mutex_unlock(&genpd->lock);
		return ret;
	}
	genpd->status = GPD_STATE_BUSY;
	genpd->resume_count++;
	for (;;) {
		prepare_to_wait(&genpd->status_wait_queue, &wait,
				TASK_UNINTERRUPTIBLE);
		if (!genpd->poweroff_task || genpd->poweroff_task == current)
			break;
		mutex_unlock(&genpd->lock);

		schedule();

		mutex_lock(&genpd->lock);
	}
	finish_wait(&genpd->status_wait_queue, &wait);
	__pm_genpd_restore_device(dev->power.subsys_data->domain_data, genpd);
	genpd->resume_count--;
	genpd_set_active(genpd);
	wake_up_all(&genpd->status_wait_queue);
	mutex_unlock(&genpd->lock);

 out:
	genpd_start_dev(genpd, dev);

	return 0;
}

void pm_genpd_poweroff_unused(void)
{
	struct generic_pm_domain *genpd;

	mutex_lock(&gpd_list_lock);

	list_for_each_entry(genpd, &gpd_list, gpd_list_node)
		genpd_queue_power_off_work(genpd);

	mutex_unlock(&gpd_list_lock);
}

#else

static inline void genpd_power_off_work_fn(struct work_struct *work) {}

#define pm_genpd_runtime_suspend	NULL
#define pm_genpd_runtime_resume		NULL

#endif 

#ifdef CONFIG_PM_SLEEP

static bool genpd_dev_active_wakeup(struct generic_pm_domain *genpd,
				    struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, bool, active_wakeup, dev);
}

static int genpd_suspend_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, suspend, dev);
}

static int genpd_suspend_late(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, suspend_late, dev);
}

static int genpd_resume_early(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, resume_early, dev);
}

static int genpd_resume_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, resume, dev);
}

static int genpd_freeze_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, freeze, dev);
}

static int genpd_freeze_late(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, freeze_late, dev);
}

static int genpd_thaw_early(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, thaw_early, dev);
}

static int genpd_thaw_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, thaw, dev);
}

static void pm_genpd_sync_poweroff(struct generic_pm_domain *genpd)
{
	struct gpd_link *link;

	if (genpd->status == GPD_STATE_POWER_OFF)
		return;

	if (genpd->suspended_count != genpd->device_count
	    || atomic_read(&genpd->sd_count) > 0)
		return;

	if (genpd->power_off)
		genpd->power_off(genpd);

	genpd->status = GPD_STATE_POWER_OFF;

	list_for_each_entry(link, &genpd->slave_links, slave_node) {
		genpd_sd_counter_dec(link->master);
		pm_genpd_sync_poweroff(link->master);
	}
}

static bool resume_needed(struct device *dev, struct generic_pm_domain *genpd)
{
	bool active_wakeup;

	if (!device_can_wakeup(dev))
		return false;

	active_wakeup = genpd_dev_active_wakeup(genpd, dev);
	return device_may_wakeup(dev) ? active_wakeup : !active_wakeup;
}

static int pm_genpd_prepare(struct device *dev)
{
	struct generic_pm_domain *genpd;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	pm_runtime_get_noresume(dev);
	if (pm_runtime_barrier(dev) && device_may_wakeup(dev))
		pm_wakeup_event(dev, 0);

	if (pm_wakeup_pending()) {
		pm_runtime_put_sync(dev);
		return -EBUSY;
	}

	if (resume_needed(dev, genpd))
		pm_runtime_resume(dev);

	genpd_acquire_lock(genpd);

	if (genpd->prepared_count++ == 0) {
		genpd->suspended_count = 0;
		genpd->suspend_power_off = genpd->status == GPD_STATE_POWER_OFF;
	}

	genpd_release_lock(genpd);

	if (genpd->suspend_power_off) {
		pm_runtime_put_noidle(dev);
		return 0;
	}

	pm_runtime_resume(dev);
	__pm_runtime_disable(dev, false);

	ret = pm_generic_prepare(dev);
	if (ret) {
		mutex_lock(&genpd->lock);

		if (--genpd->prepared_count == 0)
			genpd->suspend_power_off = false;

		mutex_unlock(&genpd->lock);
		pm_runtime_enable(dev);
	}

	pm_runtime_put_sync(dev);
	return ret;
}

static int pm_genpd_suspend(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_suspend_dev(genpd, dev);
}

static int pm_genpd_suspend_late(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_suspend_late(genpd, dev);
}

static int pm_genpd_suspend_noirq(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	if (genpd->suspend_power_off || dev_gpd_data(dev)->always_on
	    || (dev->power.wakeup_path && genpd_dev_active_wakeup(genpd, dev)))
		return 0;

	genpd_stop_dev(genpd, dev);

	genpd->suspended_count++;
	pm_genpd_sync_poweroff(genpd);

	return 0;
}

static int pm_genpd_resume_noirq(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	if (genpd->suspend_power_off || dev_gpd_data(dev)->always_on
	    || (dev->power.wakeup_path && genpd_dev_active_wakeup(genpd, dev)))
		return 0;

	pm_genpd_poweron(genpd);
	genpd->suspended_count--;

	return genpd_start_dev(genpd, dev);
}

static int pm_genpd_resume_early(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_resume_early(genpd, dev);
}

static int pm_genpd_resume(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_resume_dev(genpd, dev);
}

static int pm_genpd_freeze(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_freeze_dev(genpd, dev);
}

static int pm_genpd_freeze_late(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_freeze_late(genpd, dev);
}

static int pm_genpd_freeze_noirq(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off || dev_gpd_data(dev)->always_on ?
		0 : genpd_stop_dev(genpd, dev);
}

static int pm_genpd_thaw_noirq(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off || dev_gpd_data(dev)->always_on ?
		0 : genpd_start_dev(genpd, dev);
}

static int pm_genpd_thaw_early(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_thaw_early(genpd, dev);
}

static int pm_genpd_thaw(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	return genpd->suspend_power_off ? 0 : genpd_thaw_dev(genpd, dev);
}

static int pm_genpd_restore_noirq(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	if (genpd->suspended_count++ == 0) {
		genpd->status = GPD_STATE_POWER_OFF;
		if (genpd->suspend_power_off) {
			if (genpd->power_off)
				genpd->power_off(genpd);

			return 0;
		}
	}

	if (genpd->suspend_power_off)
		return 0;

	pm_genpd_poweron(genpd);

	return dev_gpd_data(dev)->always_on ? 0 : genpd_start_dev(genpd, dev);
}

static void pm_genpd_complete(struct device *dev)
{
	struct generic_pm_domain *genpd;
	bool run_complete;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return;

	mutex_lock(&genpd->lock);

	run_complete = !genpd->suspend_power_off;
	if (--genpd->prepared_count == 0)
		genpd->suspend_power_off = false;

	mutex_unlock(&genpd->lock);

	if (run_complete) {
		pm_generic_complete(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
		pm_runtime_idle(dev);
	}
}

#else

#define pm_genpd_prepare		NULL
#define pm_genpd_suspend		NULL
#define pm_genpd_suspend_late		NULL
#define pm_genpd_suspend_noirq		NULL
#define pm_genpd_resume_early		NULL
#define pm_genpd_resume_noirq		NULL
#define pm_genpd_resume			NULL
#define pm_genpd_freeze			NULL
#define pm_genpd_freeze_late		NULL
#define pm_genpd_freeze_noirq		NULL
#define pm_genpd_thaw_early		NULL
#define pm_genpd_thaw_noirq		NULL
#define pm_genpd_thaw			NULL
#define pm_genpd_restore_noirq		NULL
#define pm_genpd_complete		NULL

#endif 

int __pm_genpd_add_device(struct generic_pm_domain *genpd, struct device *dev,
			  struct gpd_timing_data *td)
{
	struct generic_pm_domain_data *gpd_data;
	struct pm_domain_data *pdd;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(dev))
		return -EINVAL;

	genpd_acquire_lock(genpd);

	if (genpd->status == GPD_STATE_POWER_OFF) {
		ret = -EINVAL;
		goto out;
	}

	if (genpd->prepared_count > 0) {
		ret = -EAGAIN;
		goto out;
	}

	list_for_each_entry(pdd, &genpd->dev_list, list_node)
		if (pdd->dev == dev) {
			ret = -EINVAL;
			goto out;
		}

	gpd_data = kzalloc(sizeof(*gpd_data), GFP_KERNEL);
	if (!gpd_data) {
		ret = -ENOMEM;
		goto out;
	}

	genpd->device_count++;

	dev->pm_domain = &genpd->domain;
	dev_pm_get_subsys_data(dev);
	dev->power.subsys_data->domain_data = &gpd_data->base;
	gpd_data->base.dev = dev;
	gpd_data->need_restore = false;
	list_add_tail(&gpd_data->base.list_node, &genpd->dev_list);
	if (td)
		gpd_data->td = *td;

 out:
	genpd_release_lock(genpd);

	return ret;
}

int __pm_genpd_of_add_device(struct device_node *genpd_node, struct device *dev,
			     struct gpd_timing_data *td)
{
	struct generic_pm_domain *genpd = NULL, *gpd;

	dev_dbg(dev, "%s()\n", __func__);

	if (IS_ERR_OR_NULL(genpd_node) || IS_ERR_OR_NULL(dev))
		return -EINVAL;

	mutex_lock(&gpd_list_lock);
	list_for_each_entry(gpd, &gpd_list, gpd_list_node) {
		if (gpd->of_node == genpd_node) {
			genpd = gpd;
			break;
		}
	}
	mutex_unlock(&gpd_list_lock);

	if (!genpd)
		return -EINVAL;

	return __pm_genpd_add_device(genpd, dev, td);
}

int pm_genpd_remove_device(struct generic_pm_domain *genpd,
			   struct device *dev)
{
	struct pm_domain_data *pdd;
	int ret = -EINVAL;

	dev_dbg(dev, "%s()\n", __func__);

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(dev))
		return -EINVAL;

	genpd_acquire_lock(genpd);

	if (genpd->prepared_count > 0) {
		ret = -EAGAIN;
		goto out;
	}

	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		if (pdd->dev != dev)
			continue;

		list_del_init(&pdd->list_node);
		pdd->dev = NULL;
		dev_pm_put_subsys_data(dev);
		dev->pm_domain = NULL;
		kfree(to_gpd_data(pdd));

		genpd->device_count--;

		ret = 0;
		break;
	}

 out:
	genpd_release_lock(genpd);

	return ret;
}

void pm_genpd_dev_always_on(struct device *dev, bool val)
{
	struct pm_subsys_data *psd;
	unsigned long flags;

	spin_lock_irqsave(&dev->power.lock, flags);

	psd = dev_to_psd(dev);
	if (psd && psd->domain_data)
		to_gpd_data(psd->domain_data)->always_on = val;

	spin_unlock_irqrestore(&dev->power.lock, flags);
}
EXPORT_SYMBOL_GPL(pm_genpd_dev_always_on);

int pm_genpd_add_subdomain(struct generic_pm_domain *genpd,
			   struct generic_pm_domain *subdomain)
{
	struct gpd_link *link;
	int ret = 0;

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(subdomain))
		return -EINVAL;

 start:
	genpd_acquire_lock(genpd);
	mutex_lock_nested(&subdomain->lock, SINGLE_DEPTH_NESTING);

	if (subdomain->status != GPD_STATE_POWER_OFF
	    && subdomain->status != GPD_STATE_ACTIVE) {
		mutex_unlock(&subdomain->lock);
		genpd_release_lock(genpd);
		goto start;
	}

	if (genpd->status == GPD_STATE_POWER_OFF
	    &&  subdomain->status != GPD_STATE_POWER_OFF) {
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(link, &genpd->slave_links, slave_node) {
		if (link->slave == subdomain && link->master == genpd) {
			ret = -EINVAL;
			goto out;
		}
	}

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto out;
	}
	link->master = genpd;
	list_add_tail(&link->master_node, &genpd->master_links);
	link->slave = subdomain;
	list_add_tail(&link->slave_node, &subdomain->slave_links);
	if (subdomain->status != GPD_STATE_POWER_OFF)
		genpd_sd_counter_inc(genpd);

 out:
	mutex_unlock(&subdomain->lock);
	genpd_release_lock(genpd);

	return ret;
}

int pm_genpd_remove_subdomain(struct generic_pm_domain *genpd,
			      struct generic_pm_domain *subdomain)
{
	struct gpd_link *link;
	int ret = -EINVAL;

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(subdomain))
		return -EINVAL;

 start:
	genpd_acquire_lock(genpd);

	list_for_each_entry(link, &genpd->master_links, master_node) {
		if (link->slave != subdomain)
			continue;

		mutex_lock_nested(&subdomain->lock, SINGLE_DEPTH_NESTING);

		if (subdomain->status != GPD_STATE_POWER_OFF
		    && subdomain->status != GPD_STATE_ACTIVE) {
			mutex_unlock(&subdomain->lock);
			genpd_release_lock(genpd);
			goto start;
		}

		list_del(&link->master_node);
		list_del(&link->slave_node);
		kfree(link);
		if (subdomain->status != GPD_STATE_POWER_OFF)
			genpd_sd_counter_dec(genpd);

		mutex_unlock(&subdomain->lock);

		ret = 0;
		break;
	}

	genpd_release_lock(genpd);

	return ret;
}

int pm_genpd_add_callbacks(struct device *dev, struct gpd_dev_ops *ops,
			   struct gpd_timing_data *td)
{
	struct pm_domain_data *pdd;
	int ret = 0;

	if (!(dev && dev->power.subsys_data && ops))
		return -EINVAL;

	pm_runtime_disable(dev);
	device_pm_lock();

	pdd = dev->power.subsys_data->domain_data;
	if (pdd) {
		struct generic_pm_domain_data *gpd_data = to_gpd_data(pdd);

		gpd_data->ops = *ops;
		if (td)
			gpd_data->td = *td;
	} else {
		ret = -EINVAL;
	}

	device_pm_unlock();
	pm_runtime_enable(dev);

	return ret;
}
EXPORT_SYMBOL_GPL(pm_genpd_add_callbacks);

int __pm_genpd_remove_callbacks(struct device *dev, bool clear_td)
{
	struct pm_domain_data *pdd;
	int ret = 0;

	if (!(dev && dev->power.subsys_data))
		return -EINVAL;

	pm_runtime_disable(dev);
	device_pm_lock();

	pdd = dev->power.subsys_data->domain_data;
	if (pdd) {
		struct generic_pm_domain_data *gpd_data = to_gpd_data(pdd);

		gpd_data->ops = (struct gpd_dev_ops){ 0 };
		if (clear_td)
			gpd_data->td = (struct gpd_timing_data){ 0 };
	} else {
		ret = -EINVAL;
	}

	device_pm_unlock();
	pm_runtime_enable(dev);

	return ret;
}
EXPORT_SYMBOL_GPL(__pm_genpd_remove_callbacks);


static int pm_genpd_default_save_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct device_driver *drv = dev->driver;

	cb = dev_gpd_data(dev)->ops.save_state;
	if (cb)
		return cb(dev);

	if (drv && drv->pm && drv->pm->runtime_suspend)
		return drv->pm->runtime_suspend(dev);

	return 0;
}

static int pm_genpd_default_restore_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct device_driver *drv = dev->driver;

	cb = dev_gpd_data(dev)->ops.restore_state;
	if (cb)
		return cb(dev);

	if (drv && drv->pm && drv->pm->runtime_resume)
		return drv->pm->runtime_resume(dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int pm_genpd_default_suspend(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.suspend;

	return cb ? cb(dev) : pm_generic_suspend(dev);
}

static int pm_genpd_default_suspend_late(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.suspend_late;

	return cb ? cb(dev) : pm_generic_suspend_late(dev);
}

static int pm_genpd_default_resume_early(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.resume_early;

	return cb ? cb(dev) : pm_generic_resume_early(dev);
}

static int pm_genpd_default_resume(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.resume;

	return cb ? cb(dev) : pm_generic_resume(dev);
}

static int pm_genpd_default_freeze(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.freeze;

	return cb ? cb(dev) : pm_generic_freeze(dev);
}

static int pm_genpd_default_freeze_late(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.freeze_late;

	return cb ? cb(dev) : pm_generic_freeze_late(dev);
}

static int pm_genpd_default_thaw_early(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.thaw_early;

	return cb ? cb(dev) : pm_generic_thaw_early(dev);
}

static int pm_genpd_default_thaw(struct device *dev)
{
	int (*cb)(struct device *__dev) = dev_gpd_data(dev)->ops.thaw;

	return cb ? cb(dev) : pm_generic_thaw(dev);
}

#else 

#define pm_genpd_default_suspend	NULL
#define pm_genpd_default_suspend_late	NULL
#define pm_genpd_default_resume_early	NULL
#define pm_genpd_default_resume		NULL
#define pm_genpd_default_freeze		NULL
#define pm_genpd_default_freeze_late	NULL
#define pm_genpd_default_thaw_early	NULL
#define pm_genpd_default_thaw		NULL

#endif 

void pm_genpd_init(struct generic_pm_domain *genpd,
		   struct dev_power_governor *gov, bool is_off)
{
	if (IS_ERR_OR_NULL(genpd))
		return;

	INIT_LIST_HEAD(&genpd->master_links);
	INIT_LIST_HEAD(&genpd->slave_links);
	INIT_LIST_HEAD(&genpd->dev_list);
	mutex_init(&genpd->lock);
	genpd->gov = gov;
	INIT_WORK(&genpd->power_off_work, genpd_power_off_work_fn);
	genpd->in_progress = 0;
	atomic_set(&genpd->sd_count, 0);
	genpd->status = is_off ? GPD_STATE_POWER_OFF : GPD_STATE_ACTIVE;
	init_waitqueue_head(&genpd->status_wait_queue);
	genpd->poweroff_task = NULL;
	genpd->resume_count = 0;
	genpd->device_count = 0;
	genpd->max_off_time_ns = -1;
	genpd->domain.ops.runtime_suspend = pm_genpd_runtime_suspend;
	genpd->domain.ops.runtime_resume = pm_genpd_runtime_resume;
	genpd->domain.ops.runtime_idle = pm_generic_runtime_idle;
	genpd->domain.ops.prepare = pm_genpd_prepare;
	genpd->domain.ops.suspend = pm_genpd_suspend;
	genpd->domain.ops.suspend_late = pm_genpd_suspend_late;
	genpd->domain.ops.suspend_noirq = pm_genpd_suspend_noirq;
	genpd->domain.ops.resume_noirq = pm_genpd_resume_noirq;
	genpd->domain.ops.resume_early = pm_genpd_resume_early;
	genpd->domain.ops.resume = pm_genpd_resume;
	genpd->domain.ops.freeze = pm_genpd_freeze;
	genpd->domain.ops.freeze_late = pm_genpd_freeze_late;
	genpd->domain.ops.freeze_noirq = pm_genpd_freeze_noirq;
	genpd->domain.ops.thaw_noirq = pm_genpd_thaw_noirq;
	genpd->domain.ops.thaw_early = pm_genpd_thaw_early;
	genpd->domain.ops.thaw = pm_genpd_thaw;
	genpd->domain.ops.poweroff = pm_genpd_suspend;
	genpd->domain.ops.poweroff_late = pm_genpd_suspend_late;
	genpd->domain.ops.poweroff_noirq = pm_genpd_suspend_noirq;
	genpd->domain.ops.restore_noirq = pm_genpd_restore_noirq;
	genpd->domain.ops.restore_early = pm_genpd_resume_early;
	genpd->domain.ops.restore = pm_genpd_resume;
	genpd->domain.ops.complete = pm_genpd_complete;
	genpd->dev_ops.save_state = pm_genpd_default_save_state;
	genpd->dev_ops.restore_state = pm_genpd_default_restore_state;
	genpd->dev_ops.suspend = pm_genpd_default_suspend;
	genpd->dev_ops.suspend_late = pm_genpd_default_suspend_late;
	genpd->dev_ops.resume_early = pm_genpd_default_resume_early;
	genpd->dev_ops.resume = pm_genpd_default_resume;
	genpd->dev_ops.freeze = pm_genpd_default_freeze;
	genpd->dev_ops.freeze_late = pm_genpd_default_freeze_late;
	genpd->dev_ops.thaw_early = pm_genpd_default_thaw_early;
	genpd->dev_ops.thaw = pm_genpd_default_thaw;
	mutex_lock(&gpd_list_lock);
	list_add(&genpd->gpd_list_node, &gpd_list);
	mutex_unlock(&gpd_list_lock);
}
