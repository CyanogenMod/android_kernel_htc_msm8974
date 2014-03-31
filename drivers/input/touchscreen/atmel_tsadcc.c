/*
 *  Atmel Touch Screen Driver
 *
 *  Copyright (c) 2008 ATMEL
 *  Copyright (c) 2008 Dan Liang
 *  Copyright (c) 2008 TimeSys Corporation
 *  Copyright (c) 2008 Justin Waters
 *
 *  Based on touchscreen code from Atmel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/cpu.h>


#define ATMEL_TSADCC_CR		0x00	
#define   ATMEL_TSADCC_SWRST	(1 << 0)	
#define	  ATMEL_TSADCC_START	(1 << 1)	

#define ATMEL_TSADCC_MR		0x04	
#define	  ATMEL_TSADCC_TSAMOD	(3    <<  0)	
#define	    ATMEL_TSADCC_TSAMOD_ADC_ONLY_MODE	(0x0)	
#define	    ATMEL_TSADCC_TSAMOD_TS_ONLY_MODE	(0x1)	
#define	  ATMEL_TSADCC_LOWRES	(1    <<  4)	
#define	  ATMEL_TSADCC_SLEEP	(1    <<  5)	
#define	  ATMEL_TSADCC_PENDET	(1    <<  6)	
#define	  ATMEL_TSADCC_PRES	(1    <<  7)	
#define	  ATMEL_TSADCC_PRESCAL	(0x3f <<  8)	
#define	  ATMEL_TSADCC_EPRESCAL	(0xff <<  8)	
#define	  ATMEL_TSADCC_STARTUP	(0x7f << 16)	
#define	  ATMEL_TSADCC_SHTIM	(0xf  << 24)	
#define	  ATMEL_TSADCC_PENDBC	(0xf  << 28)	

#define ATMEL_TSADCC_TRGR	0x08	
#define	  ATMEL_TSADCC_TRGMOD	(7      <<  0)	
#define	    ATMEL_TSADCC_TRGMOD_NONE		(0 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_RISING	(1 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_FALLING	(2 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_ANY		(3 << 0)
#define     ATMEL_TSADCC_TRGMOD_PENDET		(4 << 0)
#define     ATMEL_TSADCC_TRGMOD_PERIOD		(5 << 0)
#define     ATMEL_TSADCC_TRGMOD_CONTINUOUS	(6 << 0)
#define   ATMEL_TSADCC_TRGPER	(0xffff << 16)	

#define ATMEL_TSADCC_TSR	0x0C	
#define	  ATMEL_TSADCC_TSFREQ	(0xf <<  0)	
#define	  ATMEL_TSADCC_TSSHTIM	(0xf << 24)	

#define ATMEL_TSADCC_CHER	0x10	
#define ATMEL_TSADCC_CHDR	0x14	
#define ATMEL_TSADCC_CHSR	0x18	
#define	  ATMEL_TSADCC_CH(n)	(1 << (n))	

#define ATMEL_TSADCC_SR		0x1C	
#define	  ATMEL_TSADCC_EOC(n)	(1 << ((n)+0))	
#define	  ATMEL_TSADCC_OVRE(n)	(1 << ((n)+8))	
#define	  ATMEL_TSADCC_DRDY	(1 << 16)	
#define	  ATMEL_TSADCC_GOVRE	(1 << 17)	
#define	  ATMEL_TSADCC_ENDRX	(1 << 18)	
#define	  ATMEL_TSADCC_RXBUFF	(1 << 19)	
#define	  ATMEL_TSADCC_PENCNT	(1 << 20)	
#define	  ATMEL_TSADCC_NOCNT	(1 << 21)	

#define ATMEL_TSADCC_LCDR	0x20	
#define	  ATMEL_TSADCC_DATA	(0x3ff << 0)	

#define ATMEL_TSADCC_IER	0x24	
#define ATMEL_TSADCC_IDR	0x28	
#define ATMEL_TSADCC_IMR	0x2C	
#define ATMEL_TSADCC_CDR0	0x30	
#define ATMEL_TSADCC_CDR1	0x34	
#define ATMEL_TSADCC_CDR2	0x38	
#define ATMEL_TSADCC_CDR3	0x3C	
#define ATMEL_TSADCC_CDR4	0x40	
#define ATMEL_TSADCC_CDR5	0x44	

#define ATMEL_TSADCC_XPOS	0x50
#define ATMEL_TSADCC_Z1DAT	0x54
#define ATMEL_TSADCC_Z2DAT	0x58

#define PRESCALER_VAL(x)	((x) >> 8)

#define ADC_DEFAULT_CLOCK	100000

struct atmel_tsadcc {
	struct input_dev	*input;
	char			phys[32];
	struct clk		*clk;
	int			irq;
	unsigned int		prev_absx;
	unsigned int		prev_absy;
	unsigned char		bufferedmeasure;
};

static void __iomem		*tsc_base;

#define atmel_tsadcc_read(reg)		__raw_readl(tsc_base + (reg))
#define atmel_tsadcc_write(reg, val)	__raw_writel((val), tsc_base + (reg))

static irqreturn_t atmel_tsadcc_interrupt(int irq, void *dev)
{
	struct atmel_tsadcc	*ts_dev = (struct atmel_tsadcc *)dev;
	struct input_dev	*input_dev = ts_dev->input;

	unsigned int status;
	unsigned int reg;

	status = atmel_tsadcc_read(ATMEL_TSADCC_SR);
	status &= atmel_tsadcc_read(ATMEL_TSADCC_IMR);

	if (status & ATMEL_TSADCC_NOCNT) {
		
		reg = atmel_tsadcc_read(ATMEL_TSADCC_MR) | ATMEL_TSADCC_PENDBC;

		atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
		atmel_tsadcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_NONE);
		atmel_tsadcc_write(ATMEL_TSADCC_IDR,
				   ATMEL_TSADCC_EOC(3) | ATMEL_TSADCC_NOCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_IER, ATMEL_TSADCC_PENCNT);

		input_report_key(input_dev, BTN_TOUCH, 0);
		ts_dev->bufferedmeasure = 0;
		input_sync(input_dev);

	} else if (status & ATMEL_TSADCC_PENCNT) {
		
		reg = atmel_tsadcc_read(ATMEL_TSADCC_MR);
		reg &= ~ATMEL_TSADCC_PENDBC;

		atmel_tsadcc_write(ATMEL_TSADCC_IDR, ATMEL_TSADCC_PENCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
		atmel_tsadcc_write(ATMEL_TSADCC_IER,
				   ATMEL_TSADCC_EOC(3) | ATMEL_TSADCC_NOCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_TRGR,
				   ATMEL_TSADCC_TRGMOD_PERIOD | (0x0FFF << 16));

	} else if (status & ATMEL_TSADCC_EOC(3)) {
		

		if (ts_dev->bufferedmeasure) {
			input_report_abs(input_dev, ABS_X, ts_dev->prev_absx);
			input_report_abs(input_dev, ABS_Y, ts_dev->prev_absy);
			input_report_key(input_dev, BTN_TOUCH, 1);
			input_sync(input_dev);
		} else
			ts_dev->bufferedmeasure = 1;

		
		ts_dev->prev_absx = atmel_tsadcc_read(ATMEL_TSADCC_CDR3) << 10;
		ts_dev->prev_absx /= atmel_tsadcc_read(ATMEL_TSADCC_CDR2);

		ts_dev->prev_absy = atmel_tsadcc_read(ATMEL_TSADCC_CDR1) << 10;
		ts_dev->prev_absy /= atmel_tsadcc_read(ATMEL_TSADCC_CDR0);
	}

	return IRQ_HANDLED;
}


static int __devinit atmel_tsadcc_probe(struct platform_device *pdev)
{
	struct atmel_tsadcc	*ts_dev;
	struct input_dev	*input_dev;
	struct resource		*res;
	struct at91_tsadcc_data *pdata = pdev->dev.platform_data;
	int		err = 0;
	unsigned int	prsc;
	unsigned int	reg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mmio resource defined.\n");
		return -ENXIO;
	}

	
	ts_dev = kzalloc(sizeof(struct atmel_tsadcc), GFP_KERNEL);
	if (!ts_dev) {
		dev_err(&pdev->dev, "failed to allocate memory.\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ts_dev);

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "failed to allocate input device.\n");
		err = -EBUSY;
		goto err_free_mem;
	}

	ts_dev->irq = platform_get_irq(pdev, 0);
	if (ts_dev->irq < 0) {
		dev_err(&pdev->dev, "no irq ID is designated.\n");
		err = -ENODEV;
		goto err_free_dev;
	}

	if (!request_mem_region(res->start, resource_size(res),
				"atmel tsadcc regs")) {
		dev_err(&pdev->dev, "resources is unavailable.\n");
		err = -EBUSY;
		goto err_free_dev;
	}

	tsc_base = ioremap(res->start, resource_size(res));
	if (!tsc_base) {
		dev_err(&pdev->dev, "failed to map registers.\n");
		err = -ENOMEM;
		goto err_release_mem;
	}

	err = request_irq(ts_dev->irq, atmel_tsadcc_interrupt, 0,
			pdev->dev.driver->name, ts_dev);
	if (err) {
		dev_err(&pdev->dev, "failed to allocate irq.\n");
		goto err_unmap_regs;
	}

	ts_dev->clk = clk_get(&pdev->dev, "tsc_clk");
	if (IS_ERR(ts_dev->clk)) {
		dev_err(&pdev->dev, "failed to get ts_clk\n");
		err = PTR_ERR(ts_dev->clk);
		goto err_free_irq;
	}

	ts_dev->input = input_dev;
	ts_dev->bufferedmeasure = 0;

	snprintf(ts_dev->phys, sizeof(ts_dev->phys),
		 "%s/input0", dev_name(&pdev->dev));

	input_dev->name = "atmel touch screen controller";
	input_dev->phys = ts_dev->phys;
	input_dev->dev.parent = &pdev->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 0x3FF, 0, 0);

	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	
	clk_enable(ts_dev->clk);

	prsc = clk_get_rate(ts_dev->clk);
	dev_info(&pdev->dev, "Master clock is set at: %d Hz\n", prsc);

	if (!pdata)
		goto err_fail;

	if (!pdata->adc_clock)
		pdata->adc_clock = ADC_DEFAULT_CLOCK;

	prsc = (prsc / (2 * pdata->adc_clock)) - 1;

	
	if (cpu_is_at91sam9rl()) {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_PRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_PRESCAL);
	} else {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL);
	}

	dev_info(&pdev->dev, "Prescaler is set at: %d\n", prsc);

	reg = ATMEL_TSADCC_TSAMOD_TS_ONLY_MODE		|
		((0x00 << 5) & ATMEL_TSADCC_SLEEP)	|	
		((0x01 << 6) & ATMEL_TSADCC_PENDET)	|	
		(prsc << 8)				|
		((0x26 << 16) & ATMEL_TSADCC_STARTUP)	|
		((pdata->pendet_debounce << 28) & ATMEL_TSADCC_PENDBC);

	atmel_tsadcc_write(ATMEL_TSADCC_CR, ATMEL_TSADCC_SWRST);
	atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
	atmel_tsadcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_NONE);
	atmel_tsadcc_write(ATMEL_TSADCC_TSR,
		(pdata->ts_sample_hold_time << 24) & ATMEL_TSADCC_TSSHTIM);

	atmel_tsadcc_read(ATMEL_TSADCC_SR);
	atmel_tsadcc_write(ATMEL_TSADCC_IER, ATMEL_TSADCC_PENCNT);

	
	err = input_register_device(input_dev);
	if (err)
		goto err_fail;

	return 0;

err_fail:
	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);
err_free_irq:
	free_irq(ts_dev->irq, ts_dev);
err_unmap_regs:
	iounmap(tsc_base);
err_release_mem:
	release_mem_region(res->start, resource_size(res));
err_free_dev:
	input_free_device(input_dev);
err_free_mem:
	kfree(ts_dev);
	return err;
}

static int __devexit atmel_tsadcc_remove(struct platform_device *pdev)
{
	struct atmel_tsadcc *ts_dev = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	free_irq(ts_dev->irq, ts_dev);

	input_unregister_device(ts_dev->input);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(tsc_base);
	release_mem_region(res->start, resource_size(res));

	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);

	kfree(ts_dev);

	return 0;
}

static struct platform_driver atmel_tsadcc_driver = {
	.probe		= atmel_tsadcc_probe,
	.remove		= __devexit_p(atmel_tsadcc_remove),
	.driver		= {
		.name	= "atmel_tsadcc",
	},
};
module_platform_driver(atmel_tsadcc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atmel TouchScreen Driver");
MODULE_AUTHOR("Dan Liang <dan.liang@atmel.com>");

