/*
 * Alarmtimer interface
 *
 * This interface provides a timer which is similarto hrtimers,
 * but triggers a RTC alarm if the box is suspend.
 *
 * This interface is influenced by the Android RTC Alarm timer
 * interface.
 *
 * Copyright (C) 2010 IBM Corperation
 *
 * Author: John Stultz <john.stultz@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/timerqueue.h>
#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/posix-timers.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>

static struct alarm_base {
	spinlock_t		lock;
	struct timerqueue_head	timerqueue;
	struct hrtimer		timer;
	ktime_t			(*gettime)(void);
	clockid_t		base_clockid;
} alarm_bases[ALARM_NUMTYPE];

static ktime_t freezer_delta;
static DEFINE_SPINLOCK(freezer_delta_lock);

#ifdef CONFIG_RTC_CLASS
static struct rtc_timer		rtctimer;
static struct rtc_device	*rtcdev;
static DEFINE_SPINLOCK(rtcdev_lock);

struct rtc_device *alarmtimer_get_rtcdev(void)
{
	unsigned long flags;
	struct rtc_device *ret;

	spin_lock_irqsave(&rtcdev_lock, flags);
	ret = rtcdev;
	spin_unlock_irqrestore(&rtcdev_lock, flags);

	return ret;
}


static int alarmtimer_rtc_add_device(struct device *dev,
				struct class_interface *class_intf)
{
	unsigned long flags;
	struct rtc_device *rtc = to_rtc_device(dev);

	if (rtcdev)
		return -EBUSY;

	if (!rtc->ops->set_alarm)
		return -1;
	if (!device_may_wakeup(rtc->dev.parent))
		return -1;

	spin_lock_irqsave(&rtcdev_lock, flags);
	if (!rtcdev) {
		rtcdev = rtc;
		
		get_device(dev);
	}
	spin_unlock_irqrestore(&rtcdev_lock, flags);
	return 0;
}

static inline void alarmtimer_rtc_timer_init(void)
{
	rtc_timer_init(&rtctimer, NULL, NULL);
}

static struct class_interface alarmtimer_rtc_interface = {
	.add_dev = &alarmtimer_rtc_add_device,
};

static int alarmtimer_rtc_interface_setup(void)
{
	alarmtimer_rtc_interface.class = rtc_class;
	return class_interface_register(&alarmtimer_rtc_interface);
}
static void alarmtimer_rtc_interface_remove(void)
{
	class_interface_unregister(&alarmtimer_rtc_interface);
}
#else
struct rtc_device *alarmtimer_get_rtcdev(void)
{
	return NULL;
}
#define rtcdev (NULL)
static inline int alarmtimer_rtc_interface_setup(void) { return 0; }
static inline void alarmtimer_rtc_interface_remove(void) { }
static inline void alarmtimer_rtc_timer_init(void) { }
#endif

static void alarmtimer_enqueue(struct alarm_base *base, struct alarm *alarm)
{
	timerqueue_add(&base->timerqueue, &alarm->node);
	alarm->state |= ALARMTIMER_STATE_ENQUEUED;

	if (&alarm->node == timerqueue_getnext(&base->timerqueue)) {
		hrtimer_try_to_cancel(&base->timer);
		hrtimer_start(&base->timer, alarm->node.expires,
				HRTIMER_MODE_ABS);
	}
}

static void alarmtimer_remove(struct alarm_base *base, struct alarm *alarm)
{
	struct timerqueue_node *next = timerqueue_getnext(&base->timerqueue);

	if (!(alarm->state & ALARMTIMER_STATE_ENQUEUED))
		return;

	timerqueue_del(&base->timerqueue, &alarm->node);
	alarm->state &= ~ALARMTIMER_STATE_ENQUEUED;

	if (next == &alarm->node) {
		hrtimer_try_to_cancel(&base->timer);
		next = timerqueue_getnext(&base->timerqueue);
		if (!next)
			return;
		hrtimer_start(&base->timer, next->expires, HRTIMER_MODE_ABS);
	}
}


static enum hrtimer_restart alarmtimer_fired(struct hrtimer *timer)
{
	struct alarm_base *base = container_of(timer, struct alarm_base, timer);
	struct timerqueue_node *next;
	unsigned long flags;
	ktime_t now;
	int ret = HRTIMER_NORESTART;
	int restart = ALARMTIMER_NORESTART;

	spin_lock_irqsave(&base->lock, flags);
	now = base->gettime();
	while ((next = timerqueue_getnext(&base->timerqueue))) {
		struct alarm *alarm;
		ktime_t expired = next->expires;

		if (expired.tv64 > now.tv64)
			break;

		alarm = container_of(next, struct alarm, node);

		timerqueue_del(&base->timerqueue, &alarm->node);
		alarm->state &= ~ALARMTIMER_STATE_ENQUEUED;

		alarm->state |= ALARMTIMER_STATE_CALLBACK;
		spin_unlock_irqrestore(&base->lock, flags);
		if (alarm->function)
			restart = alarm->function(alarm, now);
		spin_lock_irqsave(&base->lock, flags);
		alarm->state &= ~ALARMTIMER_STATE_CALLBACK;

		if (restart != ALARMTIMER_NORESTART) {
			timerqueue_add(&base->timerqueue, &alarm->node);
			alarm->state |= ALARMTIMER_STATE_ENQUEUED;
		}
	}

	if (next) {
		hrtimer_set_expires(&base->timer, next->expires);
		ret = HRTIMER_RESTART;
	}
	spin_unlock_irqrestore(&base->lock, flags);

	return ret;

}

#ifdef CONFIG_RTC_CLASS
static int alarmtimer_suspend(struct device *dev)
{
	struct rtc_time tm;
	ktime_t min, now;
	unsigned long flags;
	struct rtc_device *rtc;
	int i;

	spin_lock_irqsave(&freezer_delta_lock, flags);
	min = freezer_delta;
	freezer_delta = ktime_set(0, 0);
	spin_unlock_irqrestore(&freezer_delta_lock, flags);

	rtc = alarmtimer_get_rtcdev();
	
	if (!rtc)
		return 0;

	
	for (i = 0; i < ALARM_NUMTYPE; i++) {
		struct alarm_base *base = &alarm_bases[i];
		struct timerqueue_node *next;
		ktime_t delta;

		spin_lock_irqsave(&base->lock, flags);
		next = timerqueue_getnext(&base->timerqueue);
		spin_unlock_irqrestore(&base->lock, flags);
		if (!next)
			continue;
		delta = ktime_sub(next->expires, base->gettime());
		if (!min.tv64 || (delta.tv64 < min.tv64))
			min = delta;
	}
	if (min.tv64 == 0)
		return 0;

	
	WARN_ON(min.tv64 < NSEC_PER_SEC);

	
	rtc_timer_cancel(rtc, &rtctimer);
	rtc_read_time(rtc, &tm);
	now = rtc_tm_to_ktime(tm);
	now = ktime_add(now, min);

	rtc_timer_start(rtc, &rtctimer, now, ktime_set(0, 0));

	return 0;
}
#else
static int alarmtimer_suspend(struct device *dev)
{
	return 0;
}
#endif

static void alarmtimer_freezerset(ktime_t absexp, enum alarmtimer_type type)
{
	ktime_t delta;
	unsigned long flags;
	struct alarm_base *base = &alarm_bases[type];

	delta = ktime_sub(absexp, base->gettime());

	spin_lock_irqsave(&freezer_delta_lock, flags);
	if (!freezer_delta.tv64 || (delta.tv64 < freezer_delta.tv64))
		freezer_delta = delta;
	spin_unlock_irqrestore(&freezer_delta_lock, flags);
}


void alarm_init(struct alarm *alarm, enum alarmtimer_type type,
		enum alarmtimer_restart (*function)(struct alarm *, ktime_t))
{
	timerqueue_init(&alarm->node);
	alarm->function = function;
	alarm->type = type;
	alarm->state = ALARMTIMER_STATE_INACTIVE;
}

void alarm_start(struct alarm *alarm, ktime_t start)
{
	struct alarm_base *base = &alarm_bases[alarm->type];
	unsigned long flags;

	spin_lock_irqsave(&base->lock, flags);
	if (alarmtimer_active(alarm))
		alarmtimer_remove(base, alarm);
	alarm->node.expires = start;
	alarmtimer_enqueue(base, alarm);
	spin_unlock_irqrestore(&base->lock, flags);
}

int alarm_try_to_cancel(struct alarm *alarm)
{
	struct alarm_base *base = &alarm_bases[alarm->type];
	unsigned long flags;
	int ret = -1;
	spin_lock_irqsave(&base->lock, flags);

	if (alarmtimer_callback_running(alarm))
		goto out;

	if (alarmtimer_is_queued(alarm)) {
		alarmtimer_remove(base, alarm);
		ret = 1;
	} else
		ret = 0;
out:
	spin_unlock_irqrestore(&base->lock, flags);
	return ret;
}


int alarm_cancel(struct alarm *alarm)
{
	for (;;) {
		int ret = alarm_try_to_cancel(alarm);
		if (ret >= 0)
			return ret;
		cpu_relax();
	}
}


u64 alarm_forward(struct alarm *alarm, ktime_t now, ktime_t interval)
{
	u64 overrun = 1;
	ktime_t delta;

	delta = ktime_sub(now, alarm->node.expires);

	if (delta.tv64 < 0)
		return 0;

	if (unlikely(delta.tv64 >= interval.tv64)) {
		s64 incr = ktime_to_ns(interval);

		overrun = ktime_divns(delta, incr);

		alarm->node.expires = ktime_add_ns(alarm->node.expires,
							incr*overrun);

		if (alarm->node.expires.tv64 > now.tv64)
			return overrun;
		overrun++;
	}

	alarm->node.expires = ktime_add(alarm->node.expires, interval);
	return overrun;
}




static enum alarmtimer_type clock2alarm(clockid_t clockid)
{
	if (clockid == CLOCK_REALTIME_ALARM)
		return ALARM_REALTIME;
	if (clockid == CLOCK_BOOTTIME_ALARM)
		return ALARM_BOOTTIME;
	return -1;
}

static enum alarmtimer_restart alarm_handle_timer(struct alarm *alarm,
							ktime_t now)
{
	struct k_itimer *ptr = container_of(alarm, struct k_itimer,
						it.alarm.alarmtimer);
	if (posix_timer_event(ptr, 0) != 0)
		ptr->it_overrun++;

	
	if (ptr->it.alarm.interval.tv64) {
		ptr->it_overrun += alarm_forward(alarm, now,
						ptr->it.alarm.interval);
		return ALARMTIMER_RESTART;
	}
	return ALARMTIMER_NORESTART;
}

static int alarm_clock_getres(const clockid_t which_clock, struct timespec *tp)
{
	clockid_t baseid = alarm_bases[clock2alarm(which_clock)].base_clockid;

	if (!alarmtimer_get_rtcdev())
		return -ENOTSUPP;

	return hrtimer_get_res(baseid, tp);
}

static int alarm_clock_get(clockid_t which_clock, struct timespec *tp)
{
	struct alarm_base *base = &alarm_bases[clock2alarm(which_clock)];

	if (!alarmtimer_get_rtcdev())
		return -ENOTSUPP;

	*tp = ktime_to_timespec(base->gettime());
	return 0;
}

static int alarm_timer_create(struct k_itimer *new_timer)
{
	enum  alarmtimer_type type;
	struct alarm_base *base;

	if (!alarmtimer_get_rtcdev())
		return -ENOTSUPP;

	if (!capable(CAP_WAKE_ALARM))
		return -EPERM;

	type = clock2alarm(new_timer->it_clock);
	base = &alarm_bases[type];
	alarm_init(&new_timer->it.alarm.alarmtimer, type, alarm_handle_timer);
	return 0;
}

static void alarm_timer_get(struct k_itimer *timr,
				struct itimerspec *cur_setting)
{
	memset(cur_setting, 0, sizeof(struct itimerspec));

	cur_setting->it_interval =
			ktime_to_timespec(timr->it.alarm.interval);
	cur_setting->it_value =
		ktime_to_timespec(timr->it.alarm.alarmtimer.node.expires);
	return;
}

static int alarm_timer_del(struct k_itimer *timr)
{
	if (!rtcdev)
		return -ENOTSUPP;

	if (alarm_try_to_cancel(&timr->it.alarm.alarmtimer) < 0)
		return TIMER_RETRY;

	return 0;
}

static int alarm_timer_set(struct k_itimer *timr, int flags,
				struct itimerspec *new_setting,
				struct itimerspec *old_setting)
{
	if (!rtcdev)
		return -ENOTSUPP;

	if (old_setting)
		alarm_timer_get(timr, old_setting);

	
	if (alarm_try_to_cancel(&timr->it.alarm.alarmtimer) < 0)
		return TIMER_RETRY;

	
	timr->it.alarm.interval = timespec_to_ktime(new_setting->it_interval);
	alarm_start(&timr->it.alarm.alarmtimer,
			timespec_to_ktime(new_setting->it_value));
	return 0;
}

static enum alarmtimer_restart alarmtimer_nsleep_wakeup(struct alarm *alarm,
								ktime_t now)
{
	struct task_struct *task = (struct task_struct *)alarm->data;

	alarm->data = NULL;
	if (task)
		wake_up_process(task);
	return ALARMTIMER_NORESTART;
}

static int alarmtimer_do_nsleep(struct alarm *alarm, ktime_t absexp)
{
	alarm->data = (void *)current;
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		alarm_start(alarm, absexp);
		if (likely(alarm->data))
			schedule();

		alarm_cancel(alarm);
	} while (alarm->data && !signal_pending(current));

	__set_current_state(TASK_RUNNING);

	return (alarm->data == NULL);
}


static int update_rmtp(ktime_t exp, enum  alarmtimer_type type,
			struct timespec __user *rmtp)
{
	struct timespec rmt;
	ktime_t rem;

	rem = ktime_sub(exp, alarm_bases[type].gettime());

	if (rem.tv64 <= 0)
		return 0;
	rmt = ktime_to_timespec(rem);

	if (copy_to_user(rmtp, &rmt, sizeof(*rmtp)))
		return -EFAULT;

	return 1;

}

static long __sched alarm_timer_nsleep_restart(struct restart_block *restart)
{
	enum  alarmtimer_type type = restart->nanosleep.clockid;
	ktime_t exp;
	struct timespec __user  *rmtp;
	struct alarm alarm;
	int ret = 0;

	exp.tv64 = restart->nanosleep.expires;
	alarm_init(&alarm, type, alarmtimer_nsleep_wakeup);

	if (alarmtimer_do_nsleep(&alarm, exp))
		goto out;

	if (freezing(current))
		alarmtimer_freezerset(exp, type);

	rmtp = restart->nanosleep.rmtp;
	if (rmtp) {
		ret = update_rmtp(exp, type, rmtp);
		if (ret <= 0)
			goto out;
	}


	
	ret = -ERESTART_RESTARTBLOCK;
out:
	return ret;
}

static int alarm_timer_nsleep(const clockid_t which_clock, int flags,
		     struct timespec *tsreq, struct timespec __user *rmtp)
{
	enum  alarmtimer_type type = clock2alarm(which_clock);
	struct alarm alarm;
	ktime_t exp;
	int ret = 0;
	struct restart_block *restart;

	if (!alarmtimer_get_rtcdev())
		return -ENOTSUPP;

	if (!capable(CAP_WAKE_ALARM))
		return -EPERM;

	alarm_init(&alarm, type, alarmtimer_nsleep_wakeup);

	exp = timespec_to_ktime(*tsreq);
	
	if (flags != TIMER_ABSTIME) {
		ktime_t now = alarm_bases[type].gettime();
		exp = ktime_add(now, exp);
	}

	if (alarmtimer_do_nsleep(&alarm, exp))
		goto out;

	if (freezing(current))
		alarmtimer_freezerset(exp, type);

	
	if (flags == TIMER_ABSTIME) {
		ret = -ERESTARTNOHAND;
		goto out;
	}

	if (rmtp) {
		ret = update_rmtp(exp, type, rmtp);
		if (ret <= 0)
			goto out;
	}

	restart = &current_thread_info()->restart_block;
	restart->fn = alarm_timer_nsleep_restart;
	restart->nanosleep.clockid = type;
	restart->nanosleep.expires = exp.tv64;
	restart->nanosleep.rmtp = rmtp;
	ret = -ERESTART_RESTARTBLOCK;

out:
	return ret;
}


static const struct dev_pm_ops alarmtimer_pm_ops = {
	.suspend = alarmtimer_suspend,
};

static struct platform_driver alarmtimer_driver = {
	.driver = {
		.name = "alarmtimer",
		.pm = &alarmtimer_pm_ops,
	}
};

static int __init alarmtimer_init(void)
{
	struct platform_device *pdev;
	int error = 0;
	int i;
	struct k_clock alarm_clock = {
		.clock_getres	= alarm_clock_getres,
		.clock_get	= alarm_clock_get,
		.timer_create	= alarm_timer_create,
		.timer_set	= alarm_timer_set,
		.timer_del	= alarm_timer_del,
		.timer_get	= alarm_timer_get,
		.nsleep		= alarm_timer_nsleep,
	};

	alarmtimer_rtc_timer_init();

	posix_timers_register_clock(CLOCK_REALTIME_ALARM, &alarm_clock);
	posix_timers_register_clock(CLOCK_BOOTTIME_ALARM, &alarm_clock);

	
	alarm_bases[ALARM_REALTIME].base_clockid = CLOCK_REALTIME;
	alarm_bases[ALARM_REALTIME].gettime = &ktime_get_real;
	alarm_bases[ALARM_BOOTTIME].base_clockid = CLOCK_BOOTTIME;
	alarm_bases[ALARM_BOOTTIME].gettime = &ktime_get_boottime;
	for (i = 0; i < ALARM_NUMTYPE; i++) {
		timerqueue_init_head(&alarm_bases[i].timerqueue);
		spin_lock_init(&alarm_bases[i].lock);
		hrtimer_init(&alarm_bases[i].timer,
				alarm_bases[i].base_clockid,
				HRTIMER_MODE_ABS);
		alarm_bases[i].timer.function = alarmtimer_fired;
	}

	error = alarmtimer_rtc_interface_setup();
	if (error)
		return error;

	error = platform_driver_register(&alarmtimer_driver);
	if (error)
		goto out_if;

	pdev = platform_device_register_simple("alarmtimer", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		error = PTR_ERR(pdev);
		goto out_drv;
	}
	return 0;

out_drv:
	platform_driver_unregister(&alarmtimer_driver);
out_if:
	alarmtimer_rtc_interface_remove();
	return error;
}
device_initcall(alarmtimer_init);
