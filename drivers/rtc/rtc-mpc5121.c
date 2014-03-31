/*
 * Real-time clock driver for MPC5121
 *
 * Copyright 2007, Domen Puncer <domen.puncer@telargo.com>
 * Copyright 2008, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2011, Dmitry Eremin-Solenikov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/slab.h>

struct mpc5121_rtc_regs {
	u8 set_time;		
	u8 hour_set;		
	u8 minute_set;		
	u8 second_set;		

	u8 set_date;		
	u8 month_set;		
	u8 weekday_set;		
	u8 date_set;		

	u8 write_sw;		
	u8 sw_set;		
	u16 year_set;		

	u8 alm_enable;		
	u8 alm_hour_set;	
	u8 alm_min_set;		
	u8 int_enable;		

	u8 reserved1;
	u8 hour;		
	u8 minute;		
	u8 second;		

	u8 month;		
	u8 wday_mday;		
	u16 year;		

	u8 int_alm;		
	u8 int_sw;		
	u8 alm_status;		
	u8 sw_minute;		

	u8 bus_error_1;		
	u8 int_day;		
	u8 int_min;		
	u8 int_sec;		

	u32 target_time;	
	u32 actual_time;	
	u32 keep_alive;		
};

struct mpc5121_rtc_data {
	unsigned irq;
	unsigned irq_periodic;
	struct mpc5121_rtc_regs __iomem *regs;
	struct rtc_device *rtc;
	struct rtc_wkalrm wkalarm;
};

static void mpc5121_rtc_update_smh(struct mpc5121_rtc_regs __iomem *regs,
				   struct rtc_time *tm)
{
	out_8(&regs->second_set, tm->tm_sec);
	out_8(&regs->minute_set, tm->tm_min);
	out_8(&regs->hour_set, tm->tm_hour);

	
	out_8(&regs->set_time, 0x1);
	out_8(&regs->set_time, 0x3);
	out_8(&regs->set_time, 0x1);
	out_8(&regs->set_time, 0x0);
}

static int mpc5121_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;
	unsigned long now;

	now = in_be32(&regs->actual_time) + in_be32(&regs->target_time);

	rtc_time_to_tm(now, tm);

	mpc5121_rtc_update_smh(regs, tm);

	return rtc_valid_tm(tm);
}

static int mpc5121_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;
	int ret;
	unsigned long now;

	ret = rtc_tm_to_time(tm, &now);
	if (ret == 0)
		out_be32(&regs->target_time, now - in_be32(&regs->actual_time));

	mpc5121_rtc_update_smh(regs, tm);

	return 0;
}

static int mpc5200_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;
	int tmp;

	tm->tm_sec = in_8(&regs->second);
	tm->tm_min = in_8(&regs->minute);

	
	if (in_8(&regs->hour) & 0x20)
		tm->tm_hour = (in_8(&regs->hour) >> 1) +
			(in_8(&regs->hour) & 1 ? 12 : 0);
	else
		tm->tm_hour = in_8(&regs->hour);

	tmp = in_8(&regs->wday_mday);
	tm->tm_mday = tmp & 0x1f;
	tm->tm_mon = in_8(&regs->month) - 1;
	tm->tm_year = in_be16(&regs->year) - 1900;
	tm->tm_wday = (tmp >> 5) % 7;
	tm->tm_yday = rtc_year_days(tm->tm_mday, tm->tm_mon, tm->tm_year);
	tm->tm_isdst = 0;

	return 0;
}

static int mpc5200_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	mpc5121_rtc_update_smh(regs, tm);

	
	out_8(&regs->month_set, tm->tm_mon + 1);
	out_8(&regs->weekday_set, tm->tm_wday ? tm->tm_wday : 7);
	out_8(&regs->date_set, tm->tm_mday);
	out_be16(&regs->year_set, tm->tm_year + 1900);

	
	out_8(&regs->set_date, 0x1);
	out_8(&regs->set_date, 0x3);
	out_8(&regs->set_date, 0x1);
	out_8(&regs->set_date, 0x0);

	return 0;
}

static int mpc5121_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	*alarm = rtc->wkalarm;

	alarm->pending = in_8(&regs->alm_status);

	return 0;
}

static int mpc5121_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	if (alarm->time.tm_sec) {
		alarm->time.tm_sec = 0;
		alarm->time.tm_min++;
		if (alarm->time.tm_min >= 60) {
			alarm->time.tm_min = 0;
			alarm->time.tm_hour++;
			if (alarm->time.tm_hour >= 24)
				alarm->time.tm_hour = 0;
		}
	}

	alarm->time.tm_mday = -1;
	alarm->time.tm_mon = -1;
	alarm->time.tm_year = -1;

	out_8(&regs->alm_min_set, alarm->time.tm_min);
	out_8(&regs->alm_hour_set, alarm->time.tm_hour);

	out_8(&regs->alm_enable, alarm->enabled);

	rtc->wkalarm = *alarm;
	return 0;
}

static irqreturn_t mpc5121_rtc_handler(int irq, void *dev)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata((struct device *)dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	if (in_8(&regs->int_alm)) {
		
		out_8(&regs->int_alm, 1);
		out_8(&regs->alm_status, 1);

		rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static irqreturn_t mpc5121_rtc_handler_upd(int irq, void *dev)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata((struct device *)dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	if (in_8(&regs->int_sec) && (in_8(&regs->int_enable) & 0x1)) {
		
		out_8(&regs->int_sec, 1);

		rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_UF);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int mpc5121_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;
	int val;

	if (enabled)
		val = 1;
	else
		val = 0;

	out_8(&regs->alm_enable, val);
	rtc->wkalarm.enabled = val;

	return 0;
}

static const struct rtc_class_ops mpc5121_rtc_ops = {
	.read_time = mpc5121_rtc_read_time,
	.set_time = mpc5121_rtc_set_time,
	.read_alarm = mpc5121_rtc_read_alarm,
	.set_alarm = mpc5121_rtc_set_alarm,
	.alarm_irq_enable = mpc5121_rtc_alarm_irq_enable,
};

static const struct rtc_class_ops mpc5200_rtc_ops = {
	.read_time = mpc5200_rtc_read_time,
	.set_time = mpc5200_rtc_set_time,
	.read_alarm = mpc5121_rtc_read_alarm,
	.set_alarm = mpc5121_rtc_set_alarm,
	.alarm_irq_enable = mpc5121_rtc_alarm_irq_enable,
};

static int __devinit mpc5121_rtc_probe(struct platform_device *op)
{
	struct mpc5121_rtc_data *rtc;
	int err = 0;

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->regs = of_iomap(op->dev.of_node, 0);
	if (!rtc->regs) {
		dev_err(&op->dev, "%s: couldn't map io space\n", __func__);
		err = -ENOSYS;
		goto out_free;
	}

	device_init_wakeup(&op->dev, 1);

	dev_set_drvdata(&op->dev, rtc);

	rtc->irq = irq_of_parse_and_map(op->dev.of_node, 1);
	err = request_irq(rtc->irq, mpc5121_rtc_handler, 0,
						"mpc5121-rtc", &op->dev);
	if (err) {
		dev_err(&op->dev, "%s: could not request irq: %i\n",
							__func__, rtc->irq);
		goto out_dispose;
	}

	rtc->irq_periodic = irq_of_parse_and_map(op->dev.of_node, 0);
	err = request_irq(rtc->irq_periodic, mpc5121_rtc_handler_upd,
				0, "mpc5121-rtc_upd", &op->dev);
	if (err) {
		dev_err(&op->dev, "%s: could not request irq: %i\n",
						__func__, rtc->irq_periodic);
		goto out_dispose2;
	}

	if (of_device_is_compatible(op->dev.of_node, "fsl,mpc5121-rtc")) {
		u32 ka;
		ka = in_be32(&rtc->regs->keep_alive);
		if (ka & 0x02) {
			dev_warn(&op->dev,
				"mpc5121-rtc: Battery or oscillator failure!\n");
			out_be32(&rtc->regs->keep_alive, ka);
		}

		rtc->rtc = rtc_device_register("mpc5121-rtc", &op->dev,
						&mpc5121_rtc_ops, THIS_MODULE);
	} else {
		rtc->rtc = rtc_device_register("mpc5200-rtc", &op->dev,
						&mpc5200_rtc_ops, THIS_MODULE);
	}

	if (IS_ERR(rtc->rtc)) {
		err = PTR_ERR(rtc->rtc);
		goto out_free_irq;
	}
	rtc->rtc->uie_unsupported = 1;

	return 0;

out_free_irq:
	free_irq(rtc->irq_periodic, &op->dev);
out_dispose2:
	irq_dispose_mapping(rtc->irq_periodic);
	free_irq(rtc->irq, &op->dev);
out_dispose:
	irq_dispose_mapping(rtc->irq);
	iounmap(rtc->regs);
out_free:
	kfree(rtc);

	return err;
}

static int __devexit mpc5121_rtc_remove(struct platform_device *op)
{
	struct mpc5121_rtc_data *rtc = dev_get_drvdata(&op->dev);
	struct mpc5121_rtc_regs __iomem *regs = rtc->regs;

	
	out_8(&regs->alm_enable, 0);
	out_8(&regs->int_enable, in_8(&regs->int_enable) & ~0x1);

	rtc_device_unregister(rtc->rtc);
	iounmap(rtc->regs);
	free_irq(rtc->irq, &op->dev);
	free_irq(rtc->irq_periodic, &op->dev);
	irq_dispose_mapping(rtc->irq);
	irq_dispose_mapping(rtc->irq_periodic);
	dev_set_drvdata(&op->dev, NULL);
	kfree(rtc);

	return 0;
}

static struct of_device_id mpc5121_rtc_match[] __devinitdata = {
	{ .compatible = "fsl,mpc5121-rtc", },
	{ .compatible = "fsl,mpc5200-rtc", },
	{},
};

static struct platform_driver mpc5121_rtc_driver = {
	.driver = {
		.name = "mpc5121-rtc",
		.owner = THIS_MODULE,
		.of_match_table = mpc5121_rtc_match,
	},
	.probe = mpc5121_rtc_probe,
	.remove = __devexit_p(mpc5121_rtc_remove),
};

module_platform_driver(mpc5121_rtc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Rigby <jcrigby@gmail.com>");
