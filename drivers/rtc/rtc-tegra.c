/*
 * An RTC driver for the NVIDIA Tegra 200 series internal RTC.
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

#define TEGRA_RTC_REG_BUSY			0x004
#define TEGRA_RTC_REG_SECONDS			0x008
#define TEGRA_RTC_REG_SHADOW_SECONDS		0x00c
#define TEGRA_RTC_REG_MILLI_SECONDS		0x010
#define TEGRA_RTC_REG_SECONDS_ALARM0		0x014
#define TEGRA_RTC_REG_SECONDS_ALARM1		0x018
#define TEGRA_RTC_REG_MILLI_SECONDS_ALARM0	0x01c
#define TEGRA_RTC_REG_INTR_MASK			0x028
#define TEGRA_RTC_REG_INTR_STATUS		0x02c

#define TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_MASK_MSEC_ALARM		(1<<2)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM1		(1<<1)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM0		(1<<0)

#define TEGRA_RTC_INTR_STATUS_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_STATUS_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_STATUS_MSEC_ALARM	(1<<2)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM1	(1<<1)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM0	(1<<0)

struct tegra_rtc_info {
	struct platform_device	*pdev;
	struct rtc_device	*rtc_dev;
	void __iomem		*rtc_base; 
	int			tegra_rtc_irq; 
	spinlock_t		tegra_rtc_lock;
};

static inline u32 tegra_rtc_check_busy(struct tegra_rtc_info *info)
{
	return readl(info->rtc_base + TEGRA_RTC_REG_BUSY) & 1;
}

static int tegra_rtc_wait_while_busy(struct device *dev)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);

	int retries = 500; 

	while (tegra_rtc_check_busy(info)) {
		if (!retries--)
			goto retry_failed;
		udelay(1);
	}

	
	return 0;

retry_failed:
	dev_err(dev, "write failed:retry count exceeded.\n");
	return -ETIMEDOUT;
}

static int tegra_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long sec, msec;
	unsigned long sl_irq_flags;

	spin_lock_irqsave(&info->tegra_rtc_lock, sl_irq_flags);

	msec = readl(info->rtc_base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(info->rtc_base + TEGRA_RTC_REG_SHADOW_SECONDS);

	spin_unlock_irqrestore(&info->tegra_rtc_lock, sl_irq_flags);

	rtc_time_to_tm(sec, tm);

	dev_vdbg(dev, "time read as %lu. %d/%d/%d %d:%02u:%02u\n",
		sec,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_year + 1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec
	);

	return 0;
}

static int tegra_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long sec;
	int ret;

	
	ret = rtc_valid_tm(tm);
	if (ret)
		return ret;

	rtc_tm_to_time(tm, &sec);

	dev_vdbg(dev, "time set to %lu. %d/%d/%d %d:%02u:%02u\n",
		sec,
		tm->tm_mon+1,
		tm->tm_mday,
		tm->tm_year+1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec
	);

	/* seconds only written if wait succeeded. */
	ret = tegra_rtc_wait_while_busy(dev);
	if (!ret)
		writel(sec, info->rtc_base + TEGRA_RTC_REG_SECONDS);

	dev_vdbg(dev, "time read back as %d\n",
		readl(info->rtc_base + TEGRA_RTC_REG_SECONDS));

	return ret;
}

static int tegra_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long sec;
	unsigned tmp;

	sec = readl(info->rtc_base + TEGRA_RTC_REG_SECONDS_ALARM0);

	if (sec == 0) {
		
		alarm->enabled = 0;
		alarm->time.tm_mon = -1;
		alarm->time.tm_mday = -1;
		alarm->time.tm_year = -1;
		alarm->time.tm_hour = -1;
		alarm->time.tm_min = -1;
		alarm->time.tm_sec = -1;
	} else {
		
		alarm->enabled = 1;
		rtc_time_to_tm(sec, &alarm->time);
	}

	tmp = readl(info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	alarm->pending = (tmp & TEGRA_RTC_INTR_STATUS_SEC_ALARM0) != 0;

	return 0;
}

static int tegra_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned status;
	unsigned long sl_irq_flags;

	tegra_rtc_wait_while_busy(dev);
	spin_lock_irqsave(&info->tegra_rtc_lock, sl_irq_flags);

	
	status = readl(info->rtc_base + TEGRA_RTC_REG_INTR_MASK);
	if (enabled)
		status |= TEGRA_RTC_INTR_MASK_SEC_ALARM0; 
	else
		status &= ~TEGRA_RTC_INTR_MASK_SEC_ALARM0; 

	writel(status, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	spin_unlock_irqrestore(&info->tegra_rtc_lock, sl_irq_flags);

	return 0;
}

static int tegra_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long sec;

	if (alarm->enabled)
		rtc_tm_to_time(&alarm->time, &sec);
	else
		sec = 0;

	tegra_rtc_wait_while_busy(dev);
	writel(sec, info->rtc_base + TEGRA_RTC_REG_SECONDS_ALARM0);
	dev_vdbg(dev, "alarm read back as %d\n",
		readl(info->rtc_base + TEGRA_RTC_REG_SECONDS_ALARM0));

	/* if successfully written and alarm is enabled ... */
	if (sec) {
		tegra_rtc_alarm_irq_enable(dev, 1);

		dev_vdbg(dev, "alarm set as %lu. %d/%d/%d %d:%02u:%02u\n",
			sec,
			alarm->time.tm_mon+1,
			alarm->time.tm_mday,
			alarm->time.tm_year+1900,
			alarm->time.tm_hour,
			alarm->time.tm_min,
			alarm->time.tm_sec);
	} else {
		
		dev_vdbg(dev, "alarm disabled\n");
		tegra_rtc_alarm_irq_enable(dev, 0);
	}

	return 0;
}

static int tegra_rtc_proc(struct device *dev, struct seq_file *seq)
{
	if (!dev || !dev->driver)
		return 0;

	return seq_printf(seq, "name\t\t: %s\n", dev_name(dev));
}

static irqreturn_t tegra_rtc_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long events = 0;
	unsigned status;
	unsigned long sl_irq_flags;

	status = readl(info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	if (status) {
		
		tegra_rtc_wait_while_busy(dev);
		spin_lock_irqsave(&info->tegra_rtc_lock, sl_irq_flags);
		writel(0, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);
		writel(status, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
		spin_unlock_irqrestore(&info->tegra_rtc_lock, sl_irq_flags);
	}

	
	if ((status & TEGRA_RTC_INTR_STATUS_SEC_ALARM0))
		events |= RTC_IRQF | RTC_AF;

	
	if ((status & TEGRA_RTC_INTR_STATUS_SEC_CDN_ALARM))
		events |= RTC_IRQF | RTC_PF;

	rtc_update_irq(info->rtc_dev, 1, events);

	return IRQ_HANDLED;
}

static struct rtc_class_ops tegra_rtc_ops = {
	.read_time	= tegra_rtc_read_time,
	.set_time	= tegra_rtc_set_time,
	.read_alarm	= tegra_rtc_read_alarm,
	.set_alarm	= tegra_rtc_set_alarm,
	.proc		= tegra_rtc_proc,
	.alarm_irq_enable = tegra_rtc_alarm_irq_enable,
};

static int __devinit tegra_rtc_probe(struct platform_device *pdev)
{
	struct tegra_rtc_info *info;
	struct resource *res;
	int ret;

	info = kzalloc(sizeof(struct tegra_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to allocate resources for device.\n");
		ret = -EBUSY;
		goto err_free_info;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device.\n");
		ret = -EBUSY;
		goto err_free_info;
	}

	info->tegra_rtc_irq = platform_get_irq(pdev, 0);
	if (info->tegra_rtc_irq <= 0) {
		ret = -EBUSY;
		goto err_release_mem_region;
	}

	info->rtc_base = ioremap_nocache(res->start, resource_size(res));
	if (!info->rtc_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		ret = -EBUSY;
		goto err_release_mem_region;
	}

	
	info->pdev = pdev;
	spin_lock_init(&info->tegra_rtc_lock);

	platform_set_drvdata(pdev, info);

	
	writel(0, info->rtc_base + TEGRA_RTC_REG_SECONDS_ALARM0);
	writel(0xffffffff, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	writel(0, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	device_init_wakeup(&pdev->dev, 1);

	info->rtc_dev = rtc_device_register(
		pdev->name, &pdev->dev, &tegra_rtc_ops, THIS_MODULE);
	if (IS_ERR(info->rtc_dev)) {
		ret = PTR_ERR(info->rtc_dev);
		info->rtc_dev = NULL;
		dev_err(&pdev->dev,
			"Unable to register device (err=%d).\n",
			ret);
		goto err_iounmap;
	}

	ret = request_irq(info->tegra_rtc_irq, tegra_rtc_irq_handler,
		IRQF_TRIGGER_HIGH, "rtc alarm", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n",
			ret);
		goto err_dev_unreg;
	}

	dev_notice(&pdev->dev, "Tegra internal Real Time Clock\n");

	return 0;

err_dev_unreg:
	rtc_device_unregister(info->rtc_dev);
err_iounmap:
	iounmap(info->rtc_base);
err_release_mem_region:
	release_mem_region(res->start, resource_size(res));
err_free_info:
	kfree(info);

	return ret;
}

static int __devexit tegra_rtc_remove(struct platform_device *pdev)
{
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EBUSY;

	free_irq(info->tegra_rtc_irq, &pdev->dev);
	rtc_device_unregister(info->rtc_dev);
	iounmap(info->rtc_base);
	release_mem_region(res->start, resource_size(res));
	kfree(info);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);

	tegra_rtc_wait_while_busy(dev);

	
	writel(0xffffffff, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	writel(TEGRA_RTC_INTR_STATUS_SEC_ALARM0,
		info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	dev_vdbg(dev, "alarm sec = %d\n",
		readl(info->rtc_base + TEGRA_RTC_REG_SECONDS_ALARM0));

	dev_vdbg(dev, "Suspend (device_may_wakeup=%d) irq:%d\n",
		device_may_wakeup(dev), info->tegra_rtc_irq);

	
	if (device_may_wakeup(dev))
		enable_irq_wake(info->tegra_rtc_irq);

	return 0;
}

static int tegra_rtc_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);

	dev_vdbg(dev, "Resume (device_may_wakeup=%d)\n",
		device_may_wakeup(dev));
	
	if (device_may_wakeup(dev))
		disable_irq_wake(info->tegra_rtc_irq);

	return 0;
}
#endif

static void tegra_rtc_shutdown(struct platform_device *pdev)
{
	dev_vdbg(&pdev->dev, "disabling interrupts.\n");
	tegra_rtc_alarm_irq_enable(&pdev->dev, 0);
}

MODULE_ALIAS("platform:tegra_rtc");
static struct platform_driver tegra_rtc_driver = {
	.remove		= __devexit_p(tegra_rtc_remove),
	.shutdown	= tegra_rtc_shutdown,
	.driver		= {
		.name	= "tegra_rtc",
		.owner	= THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend	= tegra_rtc_suspend,
	.resume		= tegra_rtc_resume,
#endif
};

static int __init tegra_rtc_init(void)
{
	return platform_driver_probe(&tegra_rtc_driver, tegra_rtc_probe);
}
module_init(tegra_rtc_init);

static void __exit tegra_rtc_exit(void)
{
	platform_driver_unregister(&tegra_rtc_driver);
}
module_exit(tegra_rtc_exit);

MODULE_AUTHOR("Jon Mayo <jmayo@nvidia.com>");
MODULE_DESCRIPTION("driver for Tegra internal RTC");
MODULE_LICENSE("GPL");
