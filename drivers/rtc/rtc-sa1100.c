/*
 * Real Time Clock interface for StrongARM SA1x00 and XScale PXA2xx
 *
 * Copyright (c) 2000 Nils Faerber
 *
 * Based on rtc.c by Paul Gortmaker
 *
 * Original Driver by Nils Faerber <nils@kernelconcepts.de>
 *
 * Modifications from:
 *   CIH <cih@coventive.com>
 *   Nicolas Pitre <nico@fluxnic.net>
 *   Andrew Christian <andrew.christian@hp.com>
 *
 * Converted to the RTC subsystem and Driver Model
 *   by Richard Purdie <rpurdie@rpsys.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

#if defined(CONFIG_ARCH_PXA) || defined(CONFIG_ARCH_MMP)
#include <mach/regs-rtc.h>
#endif

#define RTC_DEF_DIVIDER		(32768 - 1)
#define RTC_DEF_TRIM		0
#define RTC_FREQ		1024

struct sa1100_rtc {
	spinlock_t		lock;
	int			irq_1hz;
	int			irq_alarm;
	struct rtc_device	*rtc;
	struct clk		*clk;
};

static irqreturn_t sa1100_rtc_interrupt(int irq, void *dev_id)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev_id);
	struct rtc_device *rtc = info->rtc;
	unsigned int rtsr;
	unsigned long events = 0;

	spin_lock(&info->lock);

	rtsr = RTSR;
	
	RTSR = 0;
	if (rtsr & (RTSR_ALE | RTSR_HZE)) {
		RTSR = (RTSR_AL | RTSR_HZ) & (rtsr >> 2);
	} else {
		RTSR = RTSR_AL | RTSR_HZ;
	}

	
	if (rtsr & RTSR_AL)
		rtsr &= ~RTSR_ALE;
	RTSR = rtsr & (RTSR_ALE | RTSR_HZE);

	
	if (rtsr & RTSR_AL)
		events |= RTC_AF | RTC_IRQF;
	if (rtsr & RTSR_HZ)
		events |= RTC_UF | RTC_IRQF;

	rtc_update_irq(rtc, 1, events);

	spin_unlock(&info->lock);

	return IRQ_HANDLED;
}

static int sa1100_rtc_open(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	struct rtc_device *rtc = info->rtc;
	int ret;

	ret = clk_prepare_enable(info->clk);
	if (ret)
		goto fail_clk;
	ret = request_irq(info->irq_1hz, sa1100_rtc_interrupt, 0, "rtc 1Hz", dev);
	if (ret) {
		dev_err(dev, "IRQ %d already in use.\n", info->irq_1hz);
		goto fail_ui;
	}
	ret = request_irq(info->irq_alarm, sa1100_rtc_interrupt, 0, "rtc Alrm", dev);
	if (ret) {
		dev_err(dev, "IRQ %d already in use.\n", info->irq_alarm);
		goto fail_ai;
	}
	rtc->max_user_freq = RTC_FREQ;
	rtc_irq_set_freq(rtc, NULL, RTC_FREQ);

	return 0;

 fail_ai:
	free_irq(info->irq_1hz, dev);
 fail_ui:
	clk_disable_unprepare(info->clk);
 fail_clk:
	return ret;
}

static void sa1100_rtc_release(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);

	spin_lock_irq(&info->lock);
	RTSR = 0;
	spin_unlock_irq(&info->lock);

	free_irq(info->irq_alarm, dev);
	free_irq(info->irq_1hz, dev);
	clk_disable_unprepare(info->clk);
}

static int sa1100_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);

	spin_lock_irq(&info->lock);
	if (enabled)
		RTSR |= RTSR_ALE;
	else
		RTSR &= ~RTSR_ALE;
	spin_unlock_irq(&info->lock);
	return 0;
}

static int sa1100_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	rtc_time_to_tm(RCNR, tm);
	return 0;
}

static int sa1100_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	int ret;

	ret = rtc_tm_to_time(tm, &time);
	if (ret == 0)
		RCNR = time;
	return ret;
}

static int sa1100_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u32	rtsr;

	rtsr = RTSR;
	alrm->enabled = (rtsr & RTSR_ALE) ? 1 : 0;
	alrm->pending = (rtsr & RTSR_AL) ? 1 : 0;
	return 0;
}

static int sa1100_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	unsigned long time;
	int ret;

	spin_lock_irq(&info->lock);
	ret = rtc_tm_to_time(&alrm->time, &time);
	if (ret != 0)
		goto out;
	RTSR = RTSR & (RTSR_HZE|RTSR_ALE|RTSR_AL);
	RTAR = time;
	if (alrm->enabled)
		RTSR |= RTSR_ALE;
	else
		RTSR &= ~RTSR_ALE;
out:
	spin_unlock_irq(&info->lock);

	return ret;
}

static int sa1100_rtc_proc(struct device *dev, struct seq_file *seq)
{
	seq_printf(seq, "trim/divider\t\t: 0x%08x\n", (u32) RTTR);
	seq_printf(seq, "RTSR\t\t\t: 0x%08x\n", (u32)RTSR);

	return 0;
}

static const struct rtc_class_ops sa1100_rtc_ops = {
	.open = sa1100_rtc_open,
	.release = sa1100_rtc_release,
	.read_time = sa1100_rtc_read_time,
	.set_time = sa1100_rtc_set_time,
	.read_alarm = sa1100_rtc_read_alarm,
	.set_alarm = sa1100_rtc_set_alarm,
	.proc = sa1100_rtc_proc,
	.alarm_irq_enable = sa1100_rtc_alarm_irq_enable,
};

static int sa1100_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct sa1100_rtc *info;
	int irq_1hz, irq_alarm, ret = 0;

	irq_1hz = platform_get_irq_byname(pdev, "rtc 1Hz");
	irq_alarm = platform_get_irq_byname(pdev, "rtc alarm");
	if (irq_1hz < 0 || irq_alarm < 0)
		return -ENODEV;

	info = kzalloc(sizeof(struct sa1100_rtc), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to find rtc clock source\n");
		ret = PTR_ERR(info->clk);
		goto err_clk;
	}
	info->irq_1hz = irq_1hz;
	info->irq_alarm = irq_alarm;
	spin_lock_init(&info->lock);
	platform_set_drvdata(pdev, info);

	if (RTTR == 0) {
		RTTR = RTC_DEF_DIVIDER + (RTC_DEF_TRIM << 16);
		dev_warn(&pdev->dev, "warning: "
			"initializing default clock divider/trim value\n");
		
		RCNR = 0;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc = rtc_device_register(pdev->name, &pdev->dev, &sa1100_rtc_ops,
		THIS_MODULE);

	if (IS_ERR(rtc)) {
		ret = PTR_ERR(rtc);
		goto err_dev;
	}
	info->rtc = rtc;

	RTSR = RTSR_AL | RTSR_HZ;

	return 0;
err_dev:
	platform_set_drvdata(pdev, NULL);
	clk_put(info->clk);
err_clk:
	kfree(info);
	return ret;
}

static int sa1100_rtc_remove(struct platform_device *pdev)
{
	struct sa1100_rtc *info = platform_get_drvdata(pdev);

	if (info) {
		rtc_device_unregister(info->rtc);
		clk_put(info->clk);
		platform_set_drvdata(pdev, NULL);
		kfree(info);
	}

	return 0;
}

#ifdef CONFIG_PM
static int sa1100_rtc_suspend(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	if (device_may_wakeup(dev))
		enable_irq_wake(info->irq_alarm);
	return 0;
}

static int sa1100_rtc_resume(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	if (device_may_wakeup(dev))
		disable_irq_wake(info->irq_alarm);
	return 0;
}

static const struct dev_pm_ops sa1100_rtc_pm_ops = {
	.suspend	= sa1100_rtc_suspend,
	.resume		= sa1100_rtc_resume,
};
#endif

static struct of_device_id sa1100_rtc_dt_ids[] = {
	{ .compatible = "mrvl,sa1100-rtc", },
	{ .compatible = "mrvl,mmp-rtc", },
	{}
};
MODULE_DEVICE_TABLE(of, sa1100_rtc_dt_ids);

static struct platform_driver sa1100_rtc_driver = {
	.probe		= sa1100_rtc_probe,
	.remove		= sa1100_rtc_remove,
	.driver		= {
		.name	= "sa1100-rtc",
#ifdef CONFIG_PM
		.pm	= &sa1100_rtc_pm_ops,
#endif
		.of_match_table = sa1100_rtc_dt_ids,
	},
};

module_platform_driver(sa1100_rtc_driver);

MODULE_AUTHOR("Richard Purdie <rpurdie@rpsys.net>");
MODULE_DESCRIPTION("SA11x0/PXA2xx Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sa1100-rtc");
