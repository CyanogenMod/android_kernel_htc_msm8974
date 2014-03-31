/*
 * Touchscreen driver for WM831x PMICs
 *
 * Copyright 2011 Wolfson Microelectronics plc.
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/pm.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/irq.h>
#include <linux/mfd/wm831x/pdata.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#define WM831X_TCH_ENA                          0x8000  
#define WM831X_TCH_CVT_ENA                      0x4000  
#define WM831X_TCH_SLPENA                       0x1000  
#define WM831X_TCH_Z_ENA                        0x0400  
#define WM831X_TCH_Y_ENA                        0x0200  
#define WM831X_TCH_X_ENA                        0x0100  
#define WM831X_TCH_DELAY_MASK                   0x00E0  
#define WM831X_TCH_DELAY_SHIFT                       5  
#define WM831X_TCH_DELAY_WIDTH                       3  
#define WM831X_TCH_RATE_MASK                    0x001F  
#define WM831X_TCH_RATE_SHIFT                        0  
#define WM831X_TCH_RATE_WIDTH                        5  

#define WM831X_TCH_PD_WK                        0x2000  
#define WM831X_TCH_5WIRE                        0x1000  
#define WM831X_TCH_PDONLY                       0x0800  
#define WM831X_TCH_ISEL                         0x0100  
#define WM831X_TCH_RPU_MASK                     0x000F  
#define WM831X_TCH_RPU_SHIFT                         0  
#define WM831X_TCH_RPU_WIDTH                         4  

#define WM831X_TCH_PD                           0x8000  
#define WM831X_TCH_DATA_MASK                    0x0FFF  
#define WM831X_TCH_DATA_SHIFT                        0  
#define WM831X_TCH_DATA_WIDTH                       12  

struct wm831x_ts {
	struct input_dev *input_dev;
	struct wm831x *wm831x;
	unsigned int data_irq;
	unsigned int pd_irq;
	bool pressure;
	bool pen_down;
	struct work_struct pd_data_work;
};

static void wm831x_pd_data_work(struct work_struct *work)
{
	struct wm831x_ts *wm831x_ts =
		container_of(work, struct wm831x_ts, pd_data_work);

	if (wm831x_ts->pen_down) {
		enable_irq(wm831x_ts->data_irq);
		dev_dbg(wm831x_ts->wm831x->dev, "IRQ PD->DATA done\n");
	} else {
		enable_irq(wm831x_ts->pd_irq);
		dev_dbg(wm831x_ts->wm831x->dev, "IRQ DATA->PD done\n");
	}
}

static irqreturn_t wm831x_ts_data_irq(int irq, void *irq_data)
{
	struct wm831x_ts *wm831x_ts = irq_data;
	struct wm831x *wm831x = wm831x_ts->wm831x;
	static int data_types[] = { ABS_X, ABS_Y, ABS_PRESSURE };
	u16 data[3];
	int count;
	int i, ret;

	if (wm831x_ts->pressure)
		count = 3;
	else
		count = 2;

	wm831x_set_bits(wm831x, WM831X_INTERRUPT_STATUS_1,
			WM831X_TCHDATA_EINT, WM831X_TCHDATA_EINT);

	ret = wm831x_bulk_read(wm831x, WM831X_TOUCH_DATA_X, count,
			       data);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to read touch data: %d\n",
			ret);
		return IRQ_NONE;
	}

	wm831x_ts->pen_down = true;
	for (i = 0; i < count; i++) {
		if (!(data[i] & WM831X_TCH_PD)) {
			wm831x_ts->pen_down = false;
			continue;
		}
		input_report_abs(wm831x_ts->input_dev, data_types[i],
				 data[i] & WM831X_TCH_DATA_MASK);
	}

	if (!wm831x_ts->pen_down) {
		
		dev_dbg(wm831x->dev, "IRQ DATA->PD\n");

		disable_irq_nosync(wm831x_ts->data_irq);

		
		wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
				WM831X_TCH_X_ENA | WM831X_TCH_Y_ENA |
				WM831X_TCH_Z_ENA, 0);

		
		wm831x_set_bits(wm831x, WM831X_INTERRUPT_STATUS_1,
				WM831X_TCHDATA_EINT, WM831X_TCHDATA_EINT);

		wm831x_bulk_read(wm831x, WM831X_TOUCH_DATA_X, count, data);

		if (wm831x_ts->pressure)
			input_report_abs(wm831x_ts->input_dev,
					 ABS_PRESSURE, 0);

		input_report_key(wm831x_ts->input_dev, BTN_TOUCH, 0);

		schedule_work(&wm831x_ts->pd_data_work);
	} else {
		input_report_key(wm831x_ts->input_dev, BTN_TOUCH, 1);
	}

	input_sync(wm831x_ts->input_dev);

	return IRQ_HANDLED;
}

static irqreturn_t wm831x_ts_pen_down_irq(int irq, void *irq_data)
{
	struct wm831x_ts *wm831x_ts = irq_data;
	struct wm831x *wm831x = wm831x_ts->wm831x;
	int ena = 0;

	if (wm831x_ts->pen_down)
		return IRQ_HANDLED;

	disable_irq_nosync(wm831x_ts->pd_irq);

	
	if (wm831x_ts->pressure)
		ena |= WM831X_TCH_Z_ENA;

	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
			WM831X_TCH_X_ENA | WM831X_TCH_Y_ENA | WM831X_TCH_Z_ENA,
			WM831X_TCH_X_ENA | WM831X_TCH_Y_ENA | ena);

	wm831x_set_bits(wm831x, WM831X_INTERRUPT_STATUS_1,
			WM831X_TCHPD_EINT, WM831X_TCHPD_EINT);

	wm831x_ts->pen_down = true;

	
	dev_dbg(wm831x->dev, "IRQ PD->DATA\n");
	schedule_work(&wm831x_ts->pd_data_work);

	return IRQ_HANDLED;
}

static int wm831x_ts_input_open(struct input_dev *idev)
{
	struct wm831x_ts *wm831x_ts = input_get_drvdata(idev);
	struct wm831x *wm831x = wm831x_ts->wm831x;

	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
			WM831X_TCH_ENA | WM831X_TCH_CVT_ENA |
			WM831X_TCH_X_ENA | WM831X_TCH_Y_ENA |
			WM831X_TCH_Z_ENA, WM831X_TCH_ENA);

	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
			WM831X_TCH_CVT_ENA, WM831X_TCH_CVT_ENA);

	return 0;
}

static void wm831x_ts_input_close(struct input_dev *idev)
{
	struct wm831x_ts *wm831x_ts = input_get_drvdata(idev);
	struct wm831x *wm831x = wm831x_ts->wm831x;

	
	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
			WM831X_TCH_ENA | WM831X_TCH_X_ENA |
			WM831X_TCH_Y_ENA | WM831X_TCH_Z_ENA, 0);

	synchronize_irq(wm831x_ts->data_irq);
	synchronize_irq(wm831x_ts->pd_irq);

	
	flush_work_sync(&wm831x_ts->pd_data_work);

	if (wm831x_ts->pen_down) {
		disable_irq(wm831x_ts->data_irq);
		enable_irq(wm831x_ts->pd_irq);
		wm831x_ts->pen_down = false;
	}
}

static __devinit int wm831x_ts_probe(struct platform_device *pdev)
{
	struct wm831x_ts *wm831x_ts;
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *core_pdata = dev_get_platdata(pdev->dev.parent);
	struct wm831x_touch_pdata *pdata = NULL;
	struct input_dev *input_dev;
	int error, irqf;

	if (core_pdata)
		pdata = core_pdata->touch;

	wm831x_ts = kzalloc(sizeof(struct wm831x_ts), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!wm831x_ts || !input_dev) {
		error = -ENOMEM;
		goto err_alloc;
	}

	wm831x_ts->wm831x = wm831x;
	wm831x_ts->input_dev = input_dev;
	INIT_WORK(&wm831x_ts->pd_data_work, wm831x_pd_data_work);

	if (pdata && pdata->data_irq)
		wm831x_ts->data_irq = pdata->data_irq;
	else
		wm831x_ts->data_irq = platform_get_irq_byname(pdev, "TCHDATA");

	if (pdata && pdata->pd_irq)
		wm831x_ts->pd_irq = pdata->pd_irq;
	else
		wm831x_ts->pd_irq = platform_get_irq_byname(pdev, "TCHPD");

	if (pdata)
		wm831x_ts->pressure = pdata->pressure;
	else
		wm831x_ts->pressure = true;

	
	if (pdata && pdata->fivewire) {
		wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_2,
				WM831X_TCH_5WIRE, WM831X_TCH_5WIRE);

		
		WARN_ON(pdata->pressure && pdata->fivewire);
		wm831x_ts->pressure = false;
	} else {
		wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_2,
				WM831X_TCH_5WIRE, 0);
	}

	if (pdata) {
		switch (pdata->isel) {
		default:
			dev_err(&pdev->dev, "Unsupported ISEL setting: %d\n",
				pdata->isel);
			
		case 200:
		case 0:
			wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_2,
					WM831X_TCH_ISEL, 0);
			break;
		case 400:
			wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_2,
					WM831X_TCH_ISEL, WM831X_TCH_ISEL);
			break;
		}
	}

	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_2,
			WM831X_TCH_PDONLY, 0);

	
	wm831x_set_bits(wm831x, WM831X_TOUCH_CONTROL_1,
			WM831X_TCH_RATE_MASK, 6);

	if (pdata && pdata->data_irqf)
		irqf = pdata->data_irqf;
	else
		irqf = IRQF_TRIGGER_HIGH;

	error = request_threaded_irq(wm831x_ts->data_irq,
				     NULL, wm831x_ts_data_irq,
				     irqf | IRQF_ONESHOT,
				     "Touchscreen data", wm831x_ts);
	if (error) {
		dev_err(&pdev->dev, "Failed to request data IRQ %d: %d\n",
			wm831x_ts->data_irq, error);
		goto err_alloc;
	}
	disable_irq(wm831x_ts->data_irq);

	if (pdata && pdata->pd_irqf)
		irqf = pdata->pd_irqf;
	else
		irqf = IRQF_TRIGGER_HIGH;

	error = request_threaded_irq(wm831x_ts->pd_irq,
				     NULL, wm831x_ts_pen_down_irq,
				     irqf | IRQF_ONESHOT,
				     "Touchscreen pen down", wm831x_ts);
	if (error) {
		dev_err(&pdev->dev, "Failed to request pen down IRQ %d: %d\n",
			wm831x_ts->pd_irq, error);
		goto err_data_irq;
	}

	
	input_dev->name = "WM831x touchscreen";
	input_dev->phys = "wm831x";
	input_dev->open = wm831x_ts_input_open;
	input_dev->close = wm831x_ts_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, 4095, 5, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 4095, 5, 0);
	if (wm831x_ts->pressure)
		input_set_abs_params(input_dev, ABS_PRESSURE, 0, 4095, 5, 0);

	input_set_drvdata(input_dev, wm831x_ts);
	input_dev->dev.parent = &pdev->dev;

	error = input_register_device(input_dev);
	if (error)
		goto err_pd_irq;

	platform_set_drvdata(pdev, wm831x_ts);
	return 0;

err_pd_irq:
	free_irq(wm831x_ts->pd_irq, wm831x_ts);
err_data_irq:
	free_irq(wm831x_ts->data_irq, wm831x_ts);
err_alloc:
	input_free_device(input_dev);
	kfree(wm831x_ts);

	return error;
}

static __devexit int wm831x_ts_remove(struct platform_device *pdev)
{
	struct wm831x_ts *wm831x_ts = platform_get_drvdata(pdev);

	free_irq(wm831x_ts->pd_irq, wm831x_ts);
	free_irq(wm831x_ts->data_irq, wm831x_ts);
	input_unregister_device(wm831x_ts->input_dev);
	kfree(wm831x_ts);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver wm831x_ts_driver = {
	.driver = {
		.name = "wm831x-touch",
		.owner = THIS_MODULE,
	},
	.probe = wm831x_ts_probe,
	.remove = __devexit_p(wm831x_ts_remove),
};
module_platform_driver(wm831x_ts_driver);

MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_DESCRIPTION("WM831x PMIC touchscreen driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm831x-touch");
