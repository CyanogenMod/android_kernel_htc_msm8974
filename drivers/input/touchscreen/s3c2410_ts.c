/*
 * Samsung S3C24XX touchscreen driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the term of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 * Copyright 2008 Ben Dooks <ben-linux@fluff.org>
 * Copyright 2009 Simtec Electronics <linux@simtec.co.uk>
 *
 * Additional work by Herbert Pötzl <herbert@13thfloor.at> and
 * Harald Welte <laforge@openmoko.org>
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <plat/adc.h>
#include <plat/regs-adc.h>
#include <plat/ts.h>

#define TSC_SLEEP  (S3C2410_ADCTSC_PULL_UP_DISABLE | S3C2410_ADCTSC_XY_PST(0))

#define INT_DOWN	(0)
#define INT_UP		(1 << 8)

#define WAIT4INT	(S3C2410_ADCTSC_YM_SEN | \
			 S3C2410_ADCTSC_YP_SEN | \
			 S3C2410_ADCTSC_XP_SEN | \
			 S3C2410_ADCTSC_XY_PST(3))

#define AUTOPST		(S3C2410_ADCTSC_YM_SEN | \
			 S3C2410_ADCTSC_YP_SEN | \
			 S3C2410_ADCTSC_XP_SEN | \
			 S3C2410_ADCTSC_AUTO_PST | \
			 S3C2410_ADCTSC_XY_PST(0))

#define FEAT_PEN_IRQ	(1 << 0)	


struct s3c2410ts {
	struct s3c_adc_client *client;
	struct device *dev;
	struct input_dev *input;
	struct clk *clock;
	void __iomem *io;
	unsigned long xp;
	unsigned long yp;
	int irq_tc;
	int count;
	int shift;
	int features;
};

static struct s3c2410ts ts;

static inline bool get_down(unsigned long data0, unsigned long data1)
{
	
	return (!(data0 & S3C2410_ADCDAT0_UPDOWN) &&
		!(data1 & S3C2410_ADCDAT0_UPDOWN));
}

static void touch_timer_fire(unsigned long data)
{
	unsigned long data0;
	unsigned long data1;
	bool down;

	data0 = readl(ts.io + S3C2410_ADCDAT0);
	data1 = readl(ts.io + S3C2410_ADCDAT1);

	down = get_down(data0, data1);

	if (down) {
		if (ts.count == (1 << ts.shift)) {
			ts.xp >>= ts.shift;
			ts.yp >>= ts.shift;

			dev_dbg(ts.dev, "%s: X=%lu, Y=%lu, count=%d\n",
				__func__, ts.xp, ts.yp, ts.count);

			input_report_abs(ts.input, ABS_X, ts.xp);
			input_report_abs(ts.input, ABS_Y, ts.yp);

			input_report_key(ts.input, BTN_TOUCH, 1);
			input_sync(ts.input);

			ts.xp = 0;
			ts.yp = 0;
			ts.count = 0;
		}

		s3c_adc_start(ts.client, 0, 1 << ts.shift);
	} else {
		ts.xp = 0;
		ts.yp = 0;
		ts.count = 0;

		input_report_key(ts.input, BTN_TOUCH, 0);
		input_sync(ts.input);

		writel(WAIT4INT | INT_DOWN, ts.io + S3C2410_ADCTSC);
	}
}

static DEFINE_TIMER(touch_timer, touch_timer_fire, 0, 0);

static irqreturn_t stylus_irq(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;
	bool down;

	data0 = readl(ts.io + S3C2410_ADCDAT0);
	data1 = readl(ts.io + S3C2410_ADCDAT1);

	down = get_down(data0, data1);


	if (down)
		s3c_adc_start(ts.client, 0, 1 << ts.shift);
	else
		dev_dbg(ts.dev, "%s: count=%d\n", __func__, ts.count);

	if (ts.features & FEAT_PEN_IRQ) {
		
		writel(0x0, ts.io + S3C64XX_ADCCLRINTPNDNUP);
	}

	return IRQ_HANDLED;
}

static void s3c24xx_ts_conversion(struct s3c_adc_client *client,
				  unsigned data0, unsigned data1,
				  unsigned *left)
{
	dev_dbg(ts.dev, "%s: %d,%d\n", __func__, data0, data1);

	ts.xp += data0;
	ts.yp += data1;

	ts.count++;

}

static void s3c24xx_ts_select(struct s3c_adc_client *client, unsigned select)
{
	if (select) {
		writel(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST,
		       ts.io + S3C2410_ADCTSC);
	} else {
		mod_timer(&touch_timer, jiffies+1);
		writel(WAIT4INT | INT_UP, ts.io + S3C2410_ADCTSC);
	}
}

static int __devinit s3c2410ts_probe(struct platform_device *pdev)
{
	struct s3c2410_ts_mach_info *info;
	struct device *dev = &pdev->dev;
	struct input_dev *input_dev;
	struct resource *res;
	int ret = -EINVAL;

	
	memset(&ts, 0, sizeof(struct s3c2410ts));

	ts.dev = dev;

	info = pdev->dev.platform_data;
	if (!info) {
		dev_err(dev, "no platform data, cannot attach\n");
		return -EINVAL;
	}

	dev_dbg(dev, "initialising touchscreen\n");

	ts.clock = clk_get(dev, "adc");
	if (IS_ERR(ts.clock)) {
		dev_err(dev, "cannot get adc clock source\n");
		return -ENOENT;
	}

	clk_enable(ts.clock);
	dev_dbg(dev, "got and enabled clocks\n");

	ts.irq_tc = ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "no resource for interrupt\n");
		goto err_clk;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no resource for registers\n");
		ret = -ENOENT;
		goto err_clk;
	}

	ts.io = ioremap(res->start, resource_size(res));
	if (ts.io == NULL) {
		dev_err(dev, "cannot map registers\n");
		ret = -ENOMEM;
		goto err_clk;
	}

	
	if (info->cfg_gpio)
		info->cfg_gpio(to_platform_device(ts.dev));

	ts.client = s3c_adc_register(pdev, s3c24xx_ts_select,
				     s3c24xx_ts_conversion, 1);
	if (IS_ERR(ts.client)) {
		dev_err(dev, "failed to register adc client\n");
		ret = PTR_ERR(ts.client);
		goto err_iomap;
	}

	
	if ((info->delay & 0xffff) > 0)
		writel(info->delay & 0xffff, ts.io + S3C2410_ADCDLY);

	writel(WAIT4INT | INT_DOWN, ts.io + S3C2410_ADCTSC);

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Unable to allocate the input device !!\n");
		ret = -ENOMEM;
		goto err_iomap;
	}

	ts.input = input_dev;
	ts.input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts.input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(ts.input, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(ts.input, ABS_Y, 0, 0x3FF, 0, 0);

	ts.input->name = "S3C24XX TouchScreen";
	ts.input->id.bustype = BUS_HOST;
	ts.input->id.vendor = 0xDEAD;
	ts.input->id.product = 0xBEEF;
	ts.input->id.version = 0x0102;

	ts.shift = info->oversampling_shift;
	ts.features = platform_get_device_id(pdev)->driver_data;

	ret = request_irq(ts.irq_tc, stylus_irq, 0,
			  "s3c2410_ts_pen", ts.input);
	if (ret) {
		dev_err(dev, "cannot get TC interrupt\n");
		goto err_inputdev;
	}

	dev_info(dev, "driver attached, registering input device\n");

	
	ret = input_register_device(ts.input);
	if (ret < 0) {
		dev_err(dev, "failed to register input device\n");
		ret = -EIO;
		goto err_tcirq;
	}

	return 0;

 err_tcirq:
	free_irq(ts.irq_tc, ts.input);
 err_inputdev:
	input_free_device(ts.input);
 err_iomap:
	iounmap(ts.io);
 err_clk:
	del_timer_sync(&touch_timer);
	clk_put(ts.clock);
	return ret;
}

static int __devexit s3c2410ts_remove(struct platform_device *pdev)
{
	free_irq(ts.irq_tc, ts.input);
	del_timer_sync(&touch_timer);

	clk_disable(ts.clock);
	clk_put(ts.clock);

	input_unregister_device(ts.input);
	iounmap(ts.io);

	return 0;
}

#ifdef CONFIG_PM
static int s3c2410ts_suspend(struct device *dev)
{
	writel(TSC_SLEEP, ts.io + S3C2410_ADCTSC);
	disable_irq(ts.irq_tc);
	clk_disable(ts.clock);

	return 0;
}

static int s3c2410ts_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s3c2410_ts_mach_info *info = pdev->dev.platform_data;

	clk_enable(ts.clock);
	enable_irq(ts.irq_tc);

	
	if ((info->delay & 0xffff) > 0)
		writel(info->delay & 0xffff, ts.io + S3C2410_ADCDLY);

	writel(WAIT4INT | INT_DOWN, ts.io + S3C2410_ADCTSC);

	return 0;
}

static struct dev_pm_ops s3c_ts_pmops = {
	.suspend	= s3c2410ts_suspend,
	.resume		= s3c2410ts_resume,
};
#endif

static struct platform_device_id s3cts_driver_ids[] = {
	{ "s3c2410-ts", 0 },
	{ "s3c2440-ts", 0 },
	{ "s3c64xx-ts", FEAT_PEN_IRQ },
	{ }
};
MODULE_DEVICE_TABLE(platform, s3cts_driver_ids);

static struct platform_driver s3c_ts_driver = {
	.driver         = {
		.name   = "samsung-ts",
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &s3c_ts_pmops,
#endif
	},
	.id_table	= s3cts_driver_ids,
	.probe		= s3c2410ts_probe,
	.remove		= __devexit_p(s3c2410ts_remove),
};
module_platform_driver(s3c_ts_driver);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>, "
	      "Ben Dooks <ben@simtec.co.uk>, "
	      "Simtec Electronics <linux@simtec.co.uk>");
MODULE_DESCRIPTION("S3C24XX Touchscreen driver");
MODULE_LICENSE("GPL v2");
