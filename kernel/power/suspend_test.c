/*
 * kernel/power/suspend_test.c - Suspend to RAM and standby test facility.
 *
 * Copyright (c) 2009 Pavel Machek <pavel@ucw.cz>
 *
 * This file is released under the GPLv2.
 */

#include <linux/init.h>
#include <linux/rtc.h>

#include "power.h"

#define TEST_SUSPEND_SECONDS	10

static unsigned long suspend_test_start_time;

void suspend_test_start(void)
{
	suspend_test_start_time = jiffies;
}

void suspend_test_finish(const char *label)
{
	long nj = jiffies - suspend_test_start_time;
	unsigned msec;

	msec = jiffies_to_msecs(abs(nj));
	pr_info("PM: %s took %d.%03d seconds\n", label,
			msec / 1000, msec % 1000);

	WARN(msec > (TEST_SUSPEND_SECONDS * 1000),
	     "Component: %s, time: %u\n", label, msec);
}


static void __init test_wakealarm(struct rtc_device *rtc, suspend_state_t state)
{
	static char err_readtime[] __initdata =
		KERN_ERR "PM: can't read %s time, err %d\n";
	static char err_wakealarm [] __initdata =
		KERN_ERR "PM: can't set %s wakealarm, err %d\n";
	static char err_suspend[] __initdata =
		KERN_ERR "PM: suspend test failed, error %d\n";
	static char info_test[] __initdata =
		KERN_INFO "PM: test RTC wakeup from '%s' suspend\n";

	unsigned long		now;
	struct rtc_wkalrm	alm;
	int			status;

	
	status = rtc_read_time(rtc, &alm.time);
	if (status < 0) {
		printk(err_readtime, dev_name(&rtc->dev), status);
		return;
	}
	rtc_tm_to_time(&alm.time, &now);

	memset(&alm, 0, sizeof alm);
	rtc_time_to_tm(now + TEST_SUSPEND_SECONDS, &alm.time);
	alm.enabled = true;

	status = rtc_set_alarm(rtc, &alm);
	if (status < 0) {
		printk(err_wakealarm, dev_name(&rtc->dev), status);
		return;
	}

	if (state == PM_SUSPEND_MEM) {
		printk(info_test, pm_states[state]);
		status = pm_suspend(state);
		if (status == -ENODEV)
			state = PM_SUSPEND_STANDBY;
	}
	if (state == PM_SUSPEND_STANDBY) {
		printk(info_test, pm_states[state]);
		status = pm_suspend(state);
	}
	if (status < 0)
		printk(err_suspend, status);

	alm.enabled = false;
	rtc_set_alarm(rtc, &alm);
}

static int __init has_wakealarm(struct device *dev, void *name_ptr)
{
	struct rtc_device *candidate = to_rtc_device(dev);

	if (!candidate->ops->set_alarm)
		return 0;
	if (!device_may_wakeup(candidate->dev.parent))
		return 0;

	*(const char **)name_ptr = dev_name(dev);
	return 1;
}

static suspend_state_t test_state __initdata = PM_SUSPEND_ON;

static char warn_bad_state[] __initdata =
	KERN_WARNING "PM: can't test '%s' suspend state\n";

static int __init setup_test_suspend(char *value)
{
	unsigned i;

	
	value++;
	for (i = 0; i < PM_SUSPEND_MAX; i++) {
		if (!pm_states[i])
			continue;
		if (strcmp(pm_states[i], value) != 0)
			continue;
		test_state = (__force suspend_state_t) i;
		return 0;
	}
	printk(warn_bad_state, value);
	return 0;
}
__setup("test_suspend", setup_test_suspend);

static int __init test_suspend(void)
{
	static char		warn_no_rtc[] __initdata =
		KERN_WARNING "PM: no wakealarm-capable RTC driver is ready\n";

	char			*pony = NULL;
	struct rtc_device	*rtc = NULL;

	
	if (test_state == PM_SUSPEND_ON)
		goto done;
	if (!valid_state(test_state)) {
		printk(warn_bad_state, pm_states[test_state]);
		goto done;
	}

	
	class_find_device(rtc_class, NULL, &pony, has_wakealarm);
	if (pony)
		rtc = rtc_class_open(pony);
	if (!rtc) {
		printk(warn_no_rtc);
		goto done;
	}

	
	test_wakealarm(rtc, test_state);
	rtc_class_close(rtc);
done:
	return 0;
}
late_initcall(test_suspend);
