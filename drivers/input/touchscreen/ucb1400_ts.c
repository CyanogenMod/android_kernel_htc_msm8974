/*
 *  Philips UCB1400 touchscreen driver
 *
 *  Author:	Nicolas Pitre
 *  Created:	September 25, 2006
 *  Copyright:	MontaVista Software, Inc.
 *
 * Spliting done by: Marek Vasut <marek.vasut@gmail.com>
 * If something doesn't work and it worked before spliting, e-mail me,
 * dont bother Nicolas please ;-)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This code is heavily based on ucb1x00-*.c copyrighted by Russell King
 * covering the UCB1100, UCB1200 and UCB1300..  Support for the UCB1400 has
 * been made separate from ucb1x00-core/ucb1x00-ts on Russell's request.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/ucb1400.h>

#define UCB1400_TS_POLL_PERIOD	10 

static bool adcsync;
static int ts_delay = 55; 
static int ts_delay_pressure;	

static void ucb1400_ts_mode_int(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_INT);
}

static unsigned int ucb1400_ts_read_pressure(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);

	udelay(ts_delay_pressure);

	return ucb1400_adc_read(ucb->ac97, UCB_ADC_INP_TSPY, adcsync);
}

static unsigned int ucb1400_ts_read_xpos(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(ts_delay);

	return ucb1400_adc_read(ucb->ac97, UCB_ADC_INP_TSPY, adcsync);
}

static int ucb1400_ts_read_ypos(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(ts_delay);

	return ucb1400_adc_read(ucb->ac97, UCB_ADC_INP_TSPX, adcsync);
}

static unsigned int ucb1400_ts_read_xres(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1400_adc_read(ucb->ac97, 0, adcsync);
}

static unsigned int ucb1400_ts_read_yres(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1400_adc_read(ucb->ac97, 0, adcsync);
}

static int ucb1400_ts_pen_up(struct ucb1400_ts *ucb)
{
	unsigned short val = ucb1400_reg_read(ucb->ac97, UCB_TS_CR);

	return val & (UCB_TS_CR_TSPX_LOW | UCB_TS_CR_TSMX_LOW);
}

static void ucb1400_ts_irq_enable(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, UCB_IE_TSPX);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0);
	ucb1400_reg_write(ucb->ac97, UCB_IE_FAL, UCB_IE_TSPX);
}

static void ucb1400_ts_irq_disable(struct ucb1400_ts *ucb)
{
	ucb1400_reg_write(ucb->ac97, UCB_IE_FAL, 0);
}

static void ucb1400_ts_report_event(struct input_dev *idev, u16 pressure, u16 x, u16 y)
{
	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_report_abs(idev, ABS_PRESSURE, pressure);
	input_report_key(idev, BTN_TOUCH, 1);
	input_sync(idev);
}

static void ucb1400_ts_event_release(struct input_dev *idev)
{
	input_report_abs(idev, ABS_PRESSURE, 0);
	input_report_key(idev, BTN_TOUCH, 0);
	input_sync(idev);
}

static void ucb1400_clear_pending_irq(struct ucb1400_ts *ucb)
{
	unsigned int isr;

	isr = ucb1400_reg_read(ucb->ac97, UCB_IE_STATUS);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, isr);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0);

	if (isr & UCB_IE_TSPX)
		ucb1400_ts_irq_disable(ucb);
	else
		dev_dbg(&ucb->ts_idev->dev,
			"ucb1400: unexpected IE_STATUS = %#x\n", isr);
}

static irqreturn_t ucb1400_irq(int irqnr, void *devid)
{
	struct ucb1400_ts *ucb = devid;
	unsigned int x, y, p;
	bool penup;

	if (unlikely(irqnr != ucb->irq))
		return IRQ_NONE;

	ucb1400_clear_pending_irq(ucb);

	
	msleep(UCB1400_TS_POLL_PERIOD);

	while (!ucb->stopped && !(penup = ucb1400_ts_pen_up(ucb))) {

		ucb1400_adc_enable(ucb->ac97);
		x = ucb1400_ts_read_xpos(ucb);
		y = ucb1400_ts_read_ypos(ucb);
		p = ucb1400_ts_read_pressure(ucb);
		ucb1400_adc_disable(ucb->ac97);

		ucb1400_ts_report_event(ucb->ts_idev, p, x, y);

		wait_event_timeout(ucb->ts_wait, ucb->stopped,
				   msecs_to_jiffies(UCB1400_TS_POLL_PERIOD));
	}

	ucb1400_ts_event_release(ucb->ts_idev);

	if (!ucb->stopped) {
		
		ucb1400_ts_mode_int(ucb);
		ucb1400_ts_irq_enable(ucb);
	}

	return IRQ_HANDLED;
}

static void ucb1400_ts_stop(struct ucb1400_ts *ucb)
{
	
	ucb->stopped = true;
	mb();
	wake_up(&ucb->ts_wait);
	disable_irq(ucb->irq);

	ucb1400_ts_irq_disable(ucb);
	ucb1400_reg_write(ucb->ac97, UCB_TS_CR, 0);
}

static void ucb1400_ts_start(struct ucb1400_ts *ucb)
{
	
	ucb->stopped = false;
	mb();

	ucb1400_ts_mode_int(ucb);
	ucb1400_ts_irq_enable(ucb);

	enable_irq(ucb->irq);
}

static int ucb1400_ts_open(struct input_dev *idev)
{
	struct ucb1400_ts *ucb = input_get_drvdata(idev);

	ucb1400_ts_start(ucb);

	return 0;
}

static void ucb1400_ts_close(struct input_dev *idev)
{
	struct ucb1400_ts *ucb = input_get_drvdata(idev);

	ucb1400_ts_stop(ucb);
}

#ifndef NO_IRQ
#define NO_IRQ	0
#endif

static int __devinit ucb1400_ts_detect_irq(struct ucb1400_ts *ucb,
					   struct platform_device *pdev)
{
	unsigned long mask, timeout;

	mask = probe_irq_on();

	
	ucb1400_reg_write(ucb->ac97, UCB_IE_RIS, UCB_IE_ADC);
	ucb1400_reg_write(ucb->ac97, UCB_IE_FAL, UCB_IE_ADC);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0xffff);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0);

	
	ucb1400_reg_write(ucb->ac97, UCB_ADC_CR, UCB_ADC_ENA);
	ucb1400_reg_write(ucb->ac97, UCB_ADC_CR, UCB_ADC_ENA | UCB_ADC_START);

	
	timeout = jiffies + HZ/2;
	while (!(ucb1400_reg_read(ucb->ac97, UCB_ADC_DATA) &
						UCB_ADC_DAT_VALID)) {
		cpu_relax();
		if (time_after(jiffies, timeout)) {
			dev_err(&pdev->dev, "timed out in IRQ probe\n");
			probe_irq_off(mask);
			return -ENODEV;
		}
	}
	ucb1400_reg_write(ucb->ac97, UCB_ADC_CR, 0);

	
	ucb1400_reg_write(ucb->ac97, UCB_IE_RIS, 0);
	ucb1400_reg_write(ucb->ac97, UCB_IE_FAL, 0);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0xffff);
	ucb1400_reg_write(ucb->ac97, UCB_IE_CLEAR, 0);

	
	ucb->irq = probe_irq_off(mask);
	if (ucb->irq < 0 || ucb->irq == NO_IRQ)
		return -ENODEV;

	return 0;
}

static int __devinit ucb1400_ts_probe(struct platform_device *pdev)
{
	struct ucb1400_ts *ucb = pdev->dev.platform_data;
	int error, x_res, y_res;
	u16 fcsr;

	ucb->ts_idev = input_allocate_device();
	if (!ucb->ts_idev) {
		error = -ENOMEM;
		goto err;
	}

	
	if (ucb->irq < 0) {
		error = ucb1400_ts_detect_irq(ucb, pdev);
		if (error) {
			dev_err(&pdev->dev, "IRQ probe failed\n");
			goto err_free_devs;
		}
	}
	dev_dbg(&pdev->dev, "found IRQ %d\n", ucb->irq);

	init_waitqueue_head(&ucb->ts_wait);

	input_set_drvdata(ucb->ts_idev, ucb);

	ucb->ts_idev->dev.parent	= &pdev->dev;
	ucb->ts_idev->name		= "UCB1400 touchscreen interface";
	ucb->ts_idev->id.vendor		= ucb1400_reg_read(ucb->ac97,
						AC97_VENDOR_ID1);
	ucb->ts_idev->id.product	= ucb->id;
	ucb->ts_idev->open		= ucb1400_ts_open;
	ucb->ts_idev->close		= ucb1400_ts_close;
	ucb->ts_idev->evbit[0]		= BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	ucb->ts_idev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	fcsr = ucb1400_reg_read(ucb->ac97, UCB_FCSR);
	ucb1400_reg_write(ucb->ac97, UCB_FCSR, fcsr | UCB_FCSR_AVE);

	ucb1400_adc_enable(ucb->ac97);
	x_res = ucb1400_ts_read_xres(ucb);
	y_res = ucb1400_ts_read_yres(ucb);
	ucb1400_adc_disable(ucb->ac97);
	dev_dbg(&pdev->dev, "x/y = %d/%d\n", x_res, y_res);

	input_set_abs_params(ucb->ts_idev, ABS_X, 0, x_res, 0, 0);
	input_set_abs_params(ucb->ts_idev, ABS_Y, 0, y_res, 0, 0);
	input_set_abs_params(ucb->ts_idev, ABS_PRESSURE, 0, 0, 0, 0);

	ucb1400_ts_stop(ucb);

	error = request_threaded_irq(ucb->irq, NULL, ucb1400_irq,
				     IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				     "UCB1400", ucb);
	if (error) {
		dev_err(&pdev->dev,
			"unable to grab irq%d: %d\n", ucb->irq, error);
		goto err_free_devs;
	}

	error = input_register_device(ucb->ts_idev);
	if (error)
		goto err_free_irq;

	return 0;

err_free_irq:
	free_irq(ucb->irq, ucb);
err_free_devs:
	input_free_device(ucb->ts_idev);
err:
	return error;
}

static int __devexit ucb1400_ts_remove(struct platform_device *pdev)
{
	struct ucb1400_ts *ucb = pdev->dev.platform_data;

	free_irq(ucb->irq, ucb);
	input_unregister_device(ucb->ts_idev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ucb1400_ts_suspend(struct device *dev)
{
	struct ucb1400_ts *ucb = dev->platform_data;
	struct input_dev *idev = ucb->ts_idev;

	mutex_lock(&idev->mutex);

	if (idev->users)
		ucb1400_ts_start(ucb);

	mutex_unlock(&idev->mutex);
	return 0;
}

static int ucb1400_ts_resume(struct device *dev)
{
	struct ucb1400_ts *ucb = dev->platform_data;
	struct input_dev *idev = ucb->ts_idev;

	mutex_lock(&idev->mutex);

	if (idev->users)
		ucb1400_ts_stop(ucb);

	mutex_unlock(&idev->mutex);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ucb1400_ts_pm_ops,
			 ucb1400_ts_suspend, ucb1400_ts_resume);

static struct platform_driver ucb1400_ts_driver = {
	.probe	= ucb1400_ts_probe,
	.remove	= __devexit_p(ucb1400_ts_remove),
	.driver	= {
		.name	= "ucb1400_ts",
		.owner	= THIS_MODULE,
		.pm	= &ucb1400_ts_pm_ops,
	},
};
module_platform_driver(ucb1400_ts_driver);

module_param(adcsync, bool, 0444);
MODULE_PARM_DESC(adcsync, "Synchronize touch readings with ADCSYNC pin.");

module_param(ts_delay, int, 0444);
MODULE_PARM_DESC(ts_delay, "Delay between panel setup and"
			    " position read. Default = 55us.");

module_param(ts_delay_pressure, int, 0444);
MODULE_PARM_DESC(ts_delay_pressure,
		"delay between panel setup and pressure read."
		"  Default = 0us.");

MODULE_DESCRIPTION("Philips UCB1400 touchscreen driver");
MODULE_LICENSE("GPL");
