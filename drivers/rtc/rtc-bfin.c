/*
 * Blackfin On-Chip Real Time Clock Driver
 *  Supports BF51x/BF52x/BF53[123]/BF53[467]/BF54x
 *
 * Copyright 2004-2010 Analog Devices Inc.
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */



#include <linux/bcd.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <asm/blackfin.h>

#define dev_dbg_stamp(dev) dev_dbg(dev, "%s:%i: here i am\n", __func__, __LINE__)

struct bfin_rtc {
	struct rtc_device *rtc_dev;
	struct rtc_time rtc_alarm;
	u16 rtc_wrote_regs;
};

#define RTC_ISTAT_WRITE_COMPLETE  0x8000
#define RTC_ISTAT_WRITE_PENDING   0x4000
#define RTC_ISTAT_ALARM_DAY       0x0040
#define RTC_ISTAT_24HR            0x0020
#define RTC_ISTAT_HOUR            0x0010
#define RTC_ISTAT_MIN             0x0008
#define RTC_ISTAT_SEC             0x0004
#define RTC_ISTAT_ALARM           0x0002
#define RTC_ISTAT_STOPWATCH       0x0001

#define DAY_BITS_OFF    17
#define HOUR_BITS_OFF   12
#define MIN_BITS_OFF    6
#define SEC_BITS_OFF    0

static inline u32 rtc_time_to_bfin(unsigned long now)
{
	u32 sec  = (now % 60);
	u32 min  = (now % (60 * 60)) / 60;
	u32 hour = (now % (60 * 60 * 24)) / (60 * 60);
	u32 days = (now / (60 * 60 * 24));
	return (sec  << SEC_BITS_OFF) +
	       (min  << MIN_BITS_OFF) +
	       (hour << HOUR_BITS_OFF) +
	       (days << DAY_BITS_OFF);
}
static inline unsigned long rtc_bfin_to_time(u32 rtc_bfin)
{
	return (((rtc_bfin >> SEC_BITS_OFF)  & 0x003F)) +
	       (((rtc_bfin >> MIN_BITS_OFF)  & 0x003F) * 60) +
	       (((rtc_bfin >> HOUR_BITS_OFF) & 0x001F) * 60 * 60) +
	       (((rtc_bfin >> DAY_BITS_OFF)  & 0x7FFF) * 60 * 60 * 24);
}
static inline void rtc_bfin_to_tm(u32 rtc_bfin, struct rtc_time *tm)
{
	rtc_time_to_tm(rtc_bfin_to_time(rtc_bfin), tm);
}

static DECLARE_COMPLETION(bfin_write_complete);
static void bfin_rtc_sync_pending(struct device *dev)
{
	dev_dbg_stamp(dev);
	while (bfin_read_RTC_ISTAT() & RTC_ISTAT_WRITE_PENDING)
		wait_for_completion_timeout(&bfin_write_complete, HZ * 5);
	dev_dbg_stamp(dev);
}

static void bfin_rtc_reset(struct device *dev, u16 rtc_ictl)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);
	dev_dbg_stamp(dev);
	bfin_rtc_sync_pending(dev);
	bfin_write_RTC_PREN(0x1);
	bfin_write_RTC_ICTL(rtc_ictl);
	bfin_write_RTC_ALARM(0);
	bfin_write_RTC_ISTAT(0xFFFF);
	rtc->rtc_wrote_regs = 0;
}

static irqreturn_t bfin_rtc_interrupt(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct bfin_rtc *rtc = dev_get_drvdata(dev);
	unsigned long events = 0;
	bool write_complete = false;
	u16 rtc_istat, rtc_istat_clear, rtc_ictl, bits;

	dev_dbg_stamp(dev);

	rtc_istat = bfin_read_RTC_ISTAT();
	rtc_ictl = bfin_read_RTC_ICTL();
	rtc_istat_clear = 0;

	bits = RTC_ISTAT_WRITE_COMPLETE;
	if (rtc_istat & bits) {
		rtc_istat_clear |= bits;
		write_complete = true;
		complete(&bfin_write_complete);
	}

	bits = (RTC_ISTAT_ALARM | RTC_ISTAT_ALARM_DAY);
	if (rtc_ictl & bits) {
		if (rtc_istat & bits) {
			rtc_istat_clear |= bits;
			events |= RTC_AF | RTC_IRQF;
		}
	}

	bits = RTC_ISTAT_SEC;
	if (rtc_ictl & bits) {
		if (rtc_istat & bits) {
			rtc_istat_clear |= bits;
			events |= RTC_UF | RTC_IRQF;
		}
	}

	if (events)
		rtc_update_irq(rtc->rtc_dev, 1, events);

	if (write_complete || events) {
		bfin_write_RTC_ISTAT(rtc_istat_clear);
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

static void bfin_rtc_int_set(u16 rtc_int)
{
	bfin_write_RTC_ISTAT(rtc_int);
	bfin_write_RTC_ICTL(bfin_read_RTC_ICTL() | rtc_int);
}
static void bfin_rtc_int_clear(u16 rtc_int)
{
	bfin_write_RTC_ICTL(bfin_read_RTC_ICTL() & rtc_int);
}
static void bfin_rtc_int_set_alarm(struct bfin_rtc *rtc)
{
	bfin_rtc_int_set(rtc->rtc_alarm.tm_yday == -1 ? RTC_ISTAT_ALARM : RTC_ISTAT_ALARM_DAY);
}

static int bfin_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);

	dev_dbg_stamp(dev);
	if (enabled)
		bfin_rtc_int_set_alarm(rtc);
	else
		bfin_rtc_int_clear(~(RTC_ISTAT_ALARM | RTC_ISTAT_ALARM_DAY));

	return 0;
}

static int bfin_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);

	dev_dbg_stamp(dev);

	if (rtc->rtc_wrote_regs & 0x1)
		bfin_rtc_sync_pending(dev);

	rtc_bfin_to_tm(bfin_read_RTC_STAT(), tm);

	return 0;
}

static int bfin_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	unsigned long now;

	dev_dbg_stamp(dev);

	ret = rtc_tm_to_time(tm, &now);
	if (ret == 0) {
		if (rtc->rtc_wrote_regs & 0x1)
			bfin_rtc_sync_pending(dev);
		bfin_write_RTC_STAT(rtc_time_to_bfin(now));
		rtc->rtc_wrote_regs = 0x1;
	}

	return ret;
}

static int bfin_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);
	dev_dbg_stamp(dev);
	alrm->time = rtc->rtc_alarm;
	bfin_rtc_sync_pending(dev);
	alrm->enabled = !!(bfin_read_RTC_ICTL() & (RTC_ISTAT_ALARM | RTC_ISTAT_ALARM_DAY));
	return 0;
}

static int bfin_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct bfin_rtc *rtc = dev_get_drvdata(dev);
	unsigned long rtc_alarm;

	dev_dbg_stamp(dev);

	if (rtc_tm_to_time(&alrm->time, &rtc_alarm))
		return -EINVAL;

	rtc->rtc_alarm = alrm->time;

	bfin_rtc_sync_pending(dev);
	bfin_write_RTC_ALARM(rtc_time_to_bfin(rtc_alarm));
	if (alrm->enabled)
		bfin_rtc_int_set_alarm(rtc);

	return 0;
}

static int bfin_rtc_proc(struct device *dev, struct seq_file *seq)
{
#define yesno(x) ((x) ? "yes" : "no")
	u16 ictl = bfin_read_RTC_ICTL();
	dev_dbg_stamp(dev);
	seq_printf(seq,
		"alarm_IRQ\t: %s\n"
		"wkalarm_IRQ\t: %s\n"
		"seconds_IRQ\t: %s\n",
		yesno(ictl & RTC_ISTAT_ALARM),
		yesno(ictl & RTC_ISTAT_ALARM_DAY),
		yesno(ictl & RTC_ISTAT_SEC));
	return 0;
#undef yesno
}

static struct rtc_class_ops bfin_rtc_ops = {
	.read_time     = bfin_rtc_read_time,
	.set_time      = bfin_rtc_set_time,
	.read_alarm    = bfin_rtc_read_alarm,
	.set_alarm     = bfin_rtc_set_alarm,
	.proc          = bfin_rtc_proc,
	.alarm_irq_enable = bfin_rtc_alarm_irq_enable,
};

static int __devinit bfin_rtc_probe(struct platform_device *pdev)
{
	struct bfin_rtc *rtc;
	struct device *dev = &pdev->dev;
	int ret = 0;
	unsigned long timeout = jiffies + HZ;

	dev_dbg_stamp(dev);

	
	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (unlikely(!rtc))
		return -ENOMEM;
	platform_set_drvdata(pdev, rtc);
	device_init_wakeup(dev, 1);

	
	rtc->rtc_dev = rtc_device_register(pdev->name, dev, &bfin_rtc_ops,
						THIS_MODULE);
	if (unlikely(IS_ERR(rtc->rtc_dev))) {
		ret = PTR_ERR(rtc->rtc_dev);
		goto err;
	}

	
	ret = request_irq(IRQ_RTC, bfin_rtc_interrupt, 0, pdev->name, dev);
	if (unlikely(ret))
		goto err_reg;
	while (bfin_read_RTC_ISTAT() & RTC_ISTAT_WRITE_PENDING)
		if (time_after(jiffies, timeout))
			break;
	bfin_rtc_reset(dev, RTC_ISTAT_WRITE_COMPLETE);
	bfin_write_RTC_SWCNT(0);

	return 0;

err_reg:
	rtc_device_unregister(rtc->rtc_dev);
err:
	kfree(rtc);
	return ret;
}

static int __devexit bfin_rtc_remove(struct platform_device *pdev)
{
	struct bfin_rtc *rtc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	bfin_rtc_reset(dev, 0);
	free_irq(IRQ_RTC, dev);
	rtc_device_unregister(rtc->rtc_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(rtc);

	return 0;
}

#ifdef CONFIG_PM
static int bfin_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;

	dev_dbg_stamp(dev);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(IRQ_RTC);
		bfin_rtc_sync_pending(dev);
	} else
		bfin_rtc_int_clear(0);

	return 0;
}

static int bfin_rtc_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg_stamp(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(IRQ_RTC);

	while (!(bfin_read_RTC_ISTAT() & RTC_ISTAT_SEC))
		continue;
	bfin_rtc_int_set(RTC_ISTAT_WRITE_COMPLETE);

	return 0;
}
#else
# define bfin_rtc_suspend NULL
# define bfin_rtc_resume  NULL
#endif

static struct platform_driver bfin_rtc_driver = {
	.driver		= {
		.name	= "rtc-bfin",
		.owner	= THIS_MODULE,
	},
	.probe		= bfin_rtc_probe,
	.remove		= __devexit_p(bfin_rtc_remove),
	.suspend	= bfin_rtc_suspend,
	.resume		= bfin_rtc_resume,
};

module_platform_driver(bfin_rtc_driver);

MODULE_DESCRIPTION("Blackfin On-Chip Real Time Clock Driver");
MODULE_AUTHOR("Mike Frysinger <vapier@gentoo.org>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc-bfin");
