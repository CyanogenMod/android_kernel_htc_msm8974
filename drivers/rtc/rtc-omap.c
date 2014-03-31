/*
 * TI OMAP1 Real Time Clock interface for Linux
 *
 * Copyright (C) 2003 MontaVista Software, Inc.
 * Author: George G. Davis <gdavis@mvista.com> or <source@mvista.com>
 *
 * Copyright (C) 2006 David Brownell (new RTC framework)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/platform_device.h>

#include <asm/io.h>



#define OMAP_RTC_BASE			0xfffb4800

#define OMAP_RTC_SECONDS_REG		0x00
#define OMAP_RTC_MINUTES_REG		0x04
#define OMAP_RTC_HOURS_REG		0x08
#define OMAP_RTC_DAYS_REG		0x0C
#define OMAP_RTC_MONTHS_REG		0x10
#define OMAP_RTC_YEARS_REG		0x14
#define OMAP_RTC_WEEKS_REG		0x18

#define OMAP_RTC_ALARM_SECONDS_REG	0x20
#define OMAP_RTC_ALARM_MINUTES_REG	0x24
#define OMAP_RTC_ALARM_HOURS_REG	0x28
#define OMAP_RTC_ALARM_DAYS_REG		0x2c
#define OMAP_RTC_ALARM_MONTHS_REG	0x30
#define OMAP_RTC_ALARM_YEARS_REG	0x34

#define OMAP_RTC_CTRL_REG		0x40
#define OMAP_RTC_STATUS_REG		0x44
#define OMAP_RTC_INTERRUPTS_REG		0x48

#define OMAP_RTC_COMP_LSB_REG		0x4c
#define OMAP_RTC_COMP_MSB_REG		0x50
#define OMAP_RTC_OSC_REG		0x54

#define OMAP_RTC_CTRL_SPLIT		(1<<7)
#define OMAP_RTC_CTRL_DISABLE		(1<<6)
#define OMAP_RTC_CTRL_SET_32_COUNTER	(1<<5)
#define OMAP_RTC_CTRL_TEST		(1<<4)
#define OMAP_RTC_CTRL_MODE_12_24	(1<<3)
#define OMAP_RTC_CTRL_AUTO_COMP		(1<<2)
#define OMAP_RTC_CTRL_ROUND_30S		(1<<1)
#define OMAP_RTC_CTRL_STOP		(1<<0)

#define OMAP_RTC_STATUS_POWER_UP        (1<<7)
#define OMAP_RTC_STATUS_ALARM           (1<<6)
#define OMAP_RTC_STATUS_1D_EVENT        (1<<5)
#define OMAP_RTC_STATUS_1H_EVENT        (1<<4)
#define OMAP_RTC_STATUS_1M_EVENT        (1<<3)
#define OMAP_RTC_STATUS_1S_EVENT        (1<<2)
#define OMAP_RTC_STATUS_RUN             (1<<1)
#define OMAP_RTC_STATUS_BUSY            (1<<0)

#define OMAP_RTC_INTERRUPTS_IT_ALARM    (1<<3)
#define OMAP_RTC_INTERRUPTS_IT_TIMER    (1<<2)

static void __iomem	*rtc_base;

#define rtc_read(addr)		__raw_readb(rtc_base + (addr))
#define rtc_write(val, addr)	__raw_writeb(val, rtc_base + (addr))


static void rtc_wait_not_busy(void)
{
	int	count = 0;
	u8	status;

	
	for (count = 0; count < 50; count++) {
		status = rtc_read(OMAP_RTC_STATUS_REG);
		if ((status & (u8)OMAP_RTC_STATUS_BUSY) == 0)
			break;
		udelay(1);
	}
	
}

static irqreturn_t rtc_irq(int irq, void *rtc)
{
	unsigned long		events = 0;
	u8			irq_data;

	irq_data = rtc_read(OMAP_RTC_STATUS_REG);

	
	if (irq_data & OMAP_RTC_STATUS_ALARM) {
		rtc_write(OMAP_RTC_STATUS_ALARM, OMAP_RTC_STATUS_REG);
		events |= RTC_IRQF | RTC_AF;
	}

	
	if (irq_data & OMAP_RTC_STATUS_1S_EVENT)
		events |= RTC_IRQF | RTC_UF;

	rtc_update_irq(rtc, 1, events);

	return IRQ_HANDLED;
}

static int omap_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	u8 reg;

	local_irq_disable();
	rtc_wait_not_busy();
	reg = rtc_read(OMAP_RTC_INTERRUPTS_REG);
	if (enabled)
		reg |= OMAP_RTC_INTERRUPTS_IT_ALARM;
	else
		reg &= ~OMAP_RTC_INTERRUPTS_IT_ALARM;
	rtc_wait_not_busy();
	rtc_write(reg, OMAP_RTC_INTERRUPTS_REG);
	local_irq_enable();

	return 0;
}

static int tm2bcd(struct rtc_time *tm)
{
	if (rtc_valid_tm(tm) != 0)
		return -EINVAL;

	tm->tm_sec = bin2bcd(tm->tm_sec);
	tm->tm_min = bin2bcd(tm->tm_min);
	tm->tm_hour = bin2bcd(tm->tm_hour);
	tm->tm_mday = bin2bcd(tm->tm_mday);

	tm->tm_mon = bin2bcd(tm->tm_mon + 1);

	
	if (tm->tm_year < 100 || tm->tm_year > 199)
		return -EINVAL;
	tm->tm_year = bin2bcd(tm->tm_year - 100);

	return 0;
}

static void bcd2tm(struct rtc_time *tm)
{
	tm->tm_sec = bcd2bin(tm->tm_sec);
	tm->tm_min = bcd2bin(tm->tm_min);
	tm->tm_hour = bcd2bin(tm->tm_hour);
	tm->tm_mday = bcd2bin(tm->tm_mday);
	tm->tm_mon = bcd2bin(tm->tm_mon) - 1;
	
	tm->tm_year = bcd2bin(tm->tm_year) + 100;
}


static int omap_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	
	local_irq_disable();
	rtc_wait_not_busy();

	tm->tm_sec = rtc_read(OMAP_RTC_SECONDS_REG);
	tm->tm_min = rtc_read(OMAP_RTC_MINUTES_REG);
	tm->tm_hour = rtc_read(OMAP_RTC_HOURS_REG);
	tm->tm_mday = rtc_read(OMAP_RTC_DAYS_REG);
	tm->tm_mon = rtc_read(OMAP_RTC_MONTHS_REG);
	tm->tm_year = rtc_read(OMAP_RTC_YEARS_REG);

	local_irq_enable();

	bcd2tm(tm);
	return 0;
}

static int omap_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	if (tm2bcd(tm) < 0)
		return -EINVAL;
	local_irq_disable();
	rtc_wait_not_busy();

	rtc_write(tm->tm_year, OMAP_RTC_YEARS_REG);
	rtc_write(tm->tm_mon, OMAP_RTC_MONTHS_REG);
	rtc_write(tm->tm_mday, OMAP_RTC_DAYS_REG);
	rtc_write(tm->tm_hour, OMAP_RTC_HOURS_REG);
	rtc_write(tm->tm_min, OMAP_RTC_MINUTES_REG);
	rtc_write(tm->tm_sec, OMAP_RTC_SECONDS_REG);

	local_irq_enable();

	return 0;
}

static int omap_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	local_irq_disable();
	rtc_wait_not_busy();

	alm->time.tm_sec = rtc_read(OMAP_RTC_ALARM_SECONDS_REG);
	alm->time.tm_min = rtc_read(OMAP_RTC_ALARM_MINUTES_REG);
	alm->time.tm_hour = rtc_read(OMAP_RTC_ALARM_HOURS_REG);
	alm->time.tm_mday = rtc_read(OMAP_RTC_ALARM_DAYS_REG);
	alm->time.tm_mon = rtc_read(OMAP_RTC_ALARM_MONTHS_REG);
	alm->time.tm_year = rtc_read(OMAP_RTC_ALARM_YEARS_REG);

	local_irq_enable();

	bcd2tm(&alm->time);
	alm->enabled = !!(rtc_read(OMAP_RTC_INTERRUPTS_REG)
			& OMAP_RTC_INTERRUPTS_IT_ALARM);

	return 0;
}

static int omap_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	u8 reg;

	if (tm2bcd(&alm->time) < 0)
		return -EINVAL;

	local_irq_disable();
	rtc_wait_not_busy();

	rtc_write(alm->time.tm_year, OMAP_RTC_ALARM_YEARS_REG);
	rtc_write(alm->time.tm_mon, OMAP_RTC_ALARM_MONTHS_REG);
	rtc_write(alm->time.tm_mday, OMAP_RTC_ALARM_DAYS_REG);
	rtc_write(alm->time.tm_hour, OMAP_RTC_ALARM_HOURS_REG);
	rtc_write(alm->time.tm_min, OMAP_RTC_ALARM_MINUTES_REG);
	rtc_write(alm->time.tm_sec, OMAP_RTC_ALARM_SECONDS_REG);

	reg = rtc_read(OMAP_RTC_INTERRUPTS_REG);
	if (alm->enabled)
		reg |= OMAP_RTC_INTERRUPTS_IT_ALARM;
	else
		reg &= ~OMAP_RTC_INTERRUPTS_IT_ALARM;
	rtc_write(reg, OMAP_RTC_INTERRUPTS_REG);

	local_irq_enable();

	return 0;
}

static struct rtc_class_ops omap_rtc_ops = {
	.read_time	= omap_rtc_read_time,
	.set_time	= omap_rtc_set_time,
	.read_alarm	= omap_rtc_read_alarm,
	.set_alarm	= omap_rtc_set_alarm,
	.alarm_irq_enable = omap_rtc_alarm_irq_enable,
};

static int omap_rtc_alarm;
static int omap_rtc_timer;

static int __init omap_rtc_probe(struct platform_device *pdev)
{
	struct resource		*res, *mem;
	struct rtc_device	*rtc;
	u8			reg, new_ctrl;

	omap_rtc_timer = platform_get_irq(pdev, 0);
	if (omap_rtc_timer <= 0) {
		pr_debug("%s: no update irq?\n", pdev->name);
		return -ENOENT;
	}

	omap_rtc_alarm = platform_get_irq(pdev, 1);
	if (omap_rtc_alarm <= 0) {
		pr_debug("%s: no alarm irq?\n", pdev->name);
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_debug("%s: RTC resource data missing\n", pdev->name);
		return -ENOENT;
	}

	mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!mem) {
		pr_debug("%s: RTC registers at %08x are not free\n",
			pdev->name, res->start);
		return -EBUSY;
	}

	rtc_base = ioremap(res->start, resource_size(res));
	if (!rtc_base) {
		pr_debug("%s: RTC registers can't be mapped\n", pdev->name);
		goto fail;
	}

	rtc = rtc_device_register(pdev->name, &pdev->dev,
			&omap_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		pr_debug("%s: can't register RTC device, err %ld\n",
			pdev->name, PTR_ERR(rtc));
		goto fail0;
	}
	platform_set_drvdata(pdev, rtc);
	dev_set_drvdata(&rtc->dev, mem);

	rtc_write(0, OMAP_RTC_INTERRUPTS_REG);

	
	reg = rtc_read(OMAP_RTC_STATUS_REG);
	if (reg & (u8) OMAP_RTC_STATUS_POWER_UP) {
		pr_info("%s: RTC power up reset detected\n",
			pdev->name);
		rtc_write(OMAP_RTC_STATUS_POWER_UP, OMAP_RTC_STATUS_REG);
	}
	if (reg & (u8) OMAP_RTC_STATUS_ALARM)
		rtc_write(OMAP_RTC_STATUS_ALARM, OMAP_RTC_STATUS_REG);

	
	if (request_irq(omap_rtc_timer, rtc_irq, 0,
			dev_name(&rtc->dev), rtc)) {
		pr_debug("%s: RTC timer interrupt IRQ%d already claimed\n",
			pdev->name, omap_rtc_timer);
		goto fail1;
	}
	if ((omap_rtc_timer != omap_rtc_alarm) &&
		(request_irq(omap_rtc_alarm, rtc_irq, 0,
			dev_name(&rtc->dev), rtc))) {
		pr_debug("%s: RTC alarm interrupt IRQ%d already claimed\n",
			pdev->name, omap_rtc_alarm);
		goto fail2;
	}

	
	reg = rtc_read(OMAP_RTC_CTRL_REG);
	if (reg & (u8) OMAP_RTC_CTRL_STOP)
		pr_info("%s: already running\n", pdev->name);

	
	new_ctrl = reg & (OMAP_RTC_CTRL_SPLIT|OMAP_RTC_CTRL_AUTO_COMP);
	new_ctrl |= OMAP_RTC_CTRL_STOP;


	if (new_ctrl & (u8) OMAP_RTC_CTRL_SPLIT)
		pr_info("%s: split power mode\n", pdev->name);

	if (reg != new_ctrl)
		rtc_write(new_ctrl, OMAP_RTC_CTRL_REG);

	return 0;

fail2:
	free_irq(omap_rtc_timer, rtc);
fail1:
	rtc_device_unregister(rtc);
fail0:
	iounmap(rtc_base);
fail:
	release_mem_region(mem->start, resource_size(mem));
	return -EIO;
}

static int __exit omap_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device	*rtc = platform_get_drvdata(pdev);
	struct resource		*mem = dev_get_drvdata(&rtc->dev);

	device_init_wakeup(&pdev->dev, 0);

	
	rtc_write(0, OMAP_RTC_INTERRUPTS_REG);

	free_irq(omap_rtc_timer, rtc);

	if (omap_rtc_timer != omap_rtc_alarm)
		free_irq(omap_rtc_alarm, rtc);

	rtc_device_unregister(rtc);
	iounmap(rtc_base);
	release_mem_region(mem->start, resource_size(mem));
	return 0;
}

#ifdef CONFIG_PM

static u8 irqstat;

static int omap_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	irqstat = rtc_read(OMAP_RTC_INTERRUPTS_REG);

	if (device_may_wakeup(&pdev->dev))
		enable_irq_wake(omap_rtc_alarm);
	else
		rtc_write(0, OMAP_RTC_INTERRUPTS_REG);

	return 0;
}

static int omap_rtc_resume(struct platform_device *pdev)
{
	if (device_may_wakeup(&pdev->dev))
		disable_irq_wake(omap_rtc_alarm);
	else
		rtc_write(irqstat, OMAP_RTC_INTERRUPTS_REG);
	return 0;
}

#else
#define omap_rtc_suspend NULL
#define omap_rtc_resume  NULL
#endif

static void omap_rtc_shutdown(struct platform_device *pdev)
{
	rtc_write(0, OMAP_RTC_INTERRUPTS_REG);
}

MODULE_ALIAS("platform:omap_rtc");
static struct platform_driver omap_rtc_driver = {
	.remove		= __exit_p(omap_rtc_remove),
	.suspend	= omap_rtc_suspend,
	.resume		= omap_rtc_resume,
	.shutdown	= omap_rtc_shutdown,
	.driver		= {
		.name	= "omap_rtc",
		.owner	= THIS_MODULE,
	},
};

static int __init rtc_init(void)
{
	return platform_driver_probe(&omap_rtc_driver, omap_rtc_probe);
}
module_init(rtc_init);

static void __exit rtc_exit(void)
{
	platform_driver_unregister(&omap_rtc_driver);
}
module_exit(rtc_exit);

MODULE_AUTHOR("George G. Davis (and others)");
MODULE_LICENSE("GPL");
