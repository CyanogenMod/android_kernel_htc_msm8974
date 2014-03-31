/*
 *
 * TWL4030 MADC module driver-This driver monitors the real time
 * conversion of analog signals like battery temperature,
 * battery type, battery level etc.
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * J Keerthy <j-keerthy@ti.com>
 *
 * Based on twl4030-madc.c
 * Copyright (C) 2008 Nokia Corporation
 * Mikko Ylinen <mikko.k.ylinen@nokia.com>
 *
 * Amit Kucheria <amit.kucheria@canonical.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/twl4030-madc.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/err.h>

struct twl4030_madc_data {
	struct device *dev;
	struct mutex lock;	
	struct twl4030_madc_request requests[TWL4030_MADC_NUM_METHODS];
	int imr;
	int isr;
};

static struct twl4030_madc_data *twl4030_madc;

struct twl4030_prescale_divider_ratios {
	s16 numerator;
	s16 denominator;
};

static const struct twl4030_prescale_divider_ratios
twl4030_divider_ratios[16] = {
	{1, 1},		
	{1, 1},		
	{6, 10},	
	{6, 10},	
	{6, 10},	
	{6, 10},	
	{6, 10},	
	{6, 10},	
	{3, 14},	
	{1, 3},		
	{1, 1},		
	{15, 100},	
	{1, 4},		
	{1, 1},		
	{1, 1},		
	{5, 11},	
};


static int therm_tbl[] = {
30800,	29500,	28300,	27100,
26000,	24900,	23900,	22900,	22000,	21100,	20300,	19400,	18700,	17900,
17200,	16500,	15900,	15300,	14700,	14100,	13600,	13100,	12600,	12100,
11600,	11200,	10800,	10400,	10000,	9630,	9280,	8950,	8620,	8310,
8020,	7730,	7460,	7200,	6950,	6710,	6470,	6250,	6040,	5830,
5640,	5450,	5260,	5090,	4920,	4760,	4600,	4450,	4310,	4170,
4040,	3910,	3790,	3670,	3550
};

static
const struct twl4030_madc_conversion_method twl4030_conversion_methods[] = {
	[TWL4030_MADC_RT] = {
			     .sel = TWL4030_MADC_RTSELECT_LSB,
			     .avg = TWL4030_MADC_RTAVERAGE_LSB,
			     .rbase = TWL4030_MADC_RTCH0_LSB,
			     },
	[TWL4030_MADC_SW1] = {
			      .sel = TWL4030_MADC_SW1SELECT_LSB,
			      .avg = TWL4030_MADC_SW1AVERAGE_LSB,
			      .rbase = TWL4030_MADC_GPCH0_LSB,
			      .ctrl = TWL4030_MADC_CTRL_SW1,
			      },
	[TWL4030_MADC_SW2] = {
			      .sel = TWL4030_MADC_SW2SELECT_LSB,
			      .avg = TWL4030_MADC_SW2AVERAGE_LSB,
			      .rbase = TWL4030_MADC_GPCH0_LSB,
			      .ctrl = TWL4030_MADC_CTRL_SW2,
			      },
};

static int twl4030_madc_channel_raw_read(struct twl4030_madc_data *madc, u8 reg)
{
	u8 msb, lsb;
	int ret;
	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &msb, reg + 1);
	if (ret) {
		dev_err(madc->dev, "unable to read MSB register 0x%X\n",
			reg + 1);
		return ret;
	}
	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &lsb, reg);
	if (ret) {
		dev_err(madc->dev, "unable to read LSB register 0x%X\n", reg);
		return ret;
	}

	return (int)(((msb << 8) | lsb) >> 6);
}

static int twl4030battery_temperature(int raw_volt)
{
	u8 val;
	int temp, curr, volt, res, ret;

	volt = (raw_volt * TEMP_STEP_SIZE) / TEMP_PSR_R;
	
	ret = twl_i2c_read_u8(TWL4030_MODULE_MAIN_CHARGE, &val,
		REG_BCICTL2);
	if (ret < 0)
		return ret;
	curr = ((val & TWL4030_BCI_ITHEN) + 1) * 10;
	
	res = volt * 1000 / curr;
	
	for (temp = 58; temp >= 0; temp--) {
		int actual = therm_tbl[temp];

		if ((actual - res) >= 0)
			break;
	}

	return temp + 1;
}

static int twl4030battery_current(int raw_volt)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MAIN_CHARGE, &val,
		TWL4030_BCI_BCICTL1);
	if (ret)
		return ret;
	if (val & TWL4030_BCI_CGAIN) 
		return (raw_volt * CURR_STEP_SIZE) / CURR_PSR_R1;
	else 
		return (raw_volt * CURR_STEP_SIZE) / CURR_PSR_R2;
}
static int twl4030_madc_read_channels(struct twl4030_madc_data *madc,
				      u8 reg_base, unsigned
						long channels, int *buf)
{
	int count = 0, count_req = 0, i;
	u8 reg;

	for_each_set_bit(i, &channels, TWL4030_MADC_MAX_CHANNELS) {
		reg = reg_base + 2 * i;
		buf[i] = twl4030_madc_channel_raw_read(madc, reg);
		if (buf[i] < 0) {
			dev_err(madc->dev,
				"Unable to read register 0x%X\n", reg);
			count_req++;
			continue;
		}
		switch (i) {
		case 10:
			buf[i] = twl4030battery_current(buf[i]);
			if (buf[i] < 0) {
				dev_err(madc->dev, "err reading current\n");
				count_req++;
			} else {
				count++;
				buf[i] = buf[i] - 750;
			}
			break;
		case 1:
			buf[i] = twl4030battery_temperature(buf[i]);
			if (buf[i] < 0) {
				dev_err(madc->dev, "err reading temperature\n");
				count_req++;
			} else {
				buf[i] -= 3;
				count++;
			}
			break;
		default:
			count++;
			buf[i] = (buf[i] * 3 * 1000 *
				 twl4030_divider_ratios[i].denominator)
				/ (2 * 1023 *
				twl4030_divider_ratios[i].numerator);
		}
	}
	if (count_req)
		dev_err(madc->dev, "%d channel conversion failed\n", count_req);

	return count;
}

static int twl4030_madc_enable_irq(struct twl4030_madc_data *madc, u8 id)
{
	u8 val;
	int ret;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &val, madc->imr);
	if (ret) {
		dev_err(madc->dev, "unable to read imr register 0x%X\n",
			madc->imr);
		return ret;
	}
	val &= ~(1 << id);
	ret = twl_i2c_write_u8(TWL4030_MODULE_MADC, val, madc->imr);
	if (ret) {
		dev_err(madc->dev,
			"unable to write imr register 0x%X\n", madc->imr);
		return ret;

	}

	return 0;
}

static int twl4030_madc_disable_irq(struct twl4030_madc_data *madc, u8 id)
{
	u8 val;
	int ret;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &val, madc->imr);
	if (ret) {
		dev_err(madc->dev, "unable to read imr register 0x%X\n",
			madc->imr);
		return ret;
	}
	val |= (1 << id);
	ret = twl_i2c_write_u8(TWL4030_MODULE_MADC, val, madc->imr);
	if (ret) {
		dev_err(madc->dev,
			"unable to write imr register 0x%X\n", madc->imr);
		return ret;
	}

	return 0;
}

static irqreturn_t twl4030_madc_threaded_irq_handler(int irq, void *_madc)
{
	struct twl4030_madc_data *madc = _madc;
	const struct twl4030_madc_conversion_method *method;
	u8 isr_val, imr_val;
	int i, len, ret;
	struct twl4030_madc_request *r;

	mutex_lock(&madc->lock);
	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &isr_val, madc->isr);
	if (ret) {
		dev_err(madc->dev, "unable to read isr register 0x%X\n",
			madc->isr);
		goto err_i2c;
	}
	ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &imr_val, madc->imr);
	if (ret) {
		dev_err(madc->dev, "unable to read imr register 0x%X\n",
			madc->imr);
		goto err_i2c;
	}
	isr_val &= ~imr_val;
	for (i = 0; i < TWL4030_MADC_NUM_METHODS; i++) {
		if (!(isr_val & (1 << i)))
			continue;
		ret = twl4030_madc_disable_irq(madc, i);
		if (ret < 0)
			dev_dbg(madc->dev, "Disable interrupt failed%d\n", i);
		madc->requests[i].result_pending = 1;
	}
	for (i = 0; i < TWL4030_MADC_NUM_METHODS; i++) {
		r = &madc->requests[i];
		
		if (!r->result_pending)
			continue;
		method = &twl4030_conversion_methods[r->method];
		
		len = twl4030_madc_read_channels(madc, method->rbase,
						 r->channels, r->rbuf);
		
		if (r->func_cb != NULL) {
			r->func_cb(len, r->channels, r->rbuf);
			r->func_cb = NULL;
		}
		
		r->result_pending = 0;
		r->active = 0;
	}
	mutex_unlock(&madc->lock);

	return IRQ_HANDLED;

err_i2c:
	for (i = 0; i < TWL4030_MADC_NUM_METHODS; i++) {
		r = &madc->requests[i];
		if (r->active == 0)
			continue;
		method = &twl4030_conversion_methods[r->method];
		
		len = twl4030_madc_read_channels(madc, method->rbase,
						 r->channels, r->rbuf);
		
		if (r->func_cb != NULL) {
			r->func_cb(len, r->channels, r->rbuf);
			r->func_cb = NULL;
		}
		
		r->result_pending = 0;
		r->active = 0;
	}
	mutex_unlock(&madc->lock);

	return IRQ_HANDLED;
}

static int twl4030_madc_set_irq(struct twl4030_madc_data *madc,
				struct twl4030_madc_request *req)
{
	struct twl4030_madc_request *p;
	int ret;

	p = &madc->requests[req->method];
	memcpy(p, req, sizeof(*req));
	ret = twl4030_madc_enable_irq(madc, req->method);
	if (ret < 0) {
		dev_err(madc->dev, "enable irq failed!!\n");
		return ret;
	}

	return 0;
}

static int twl4030_madc_start_conversion(struct twl4030_madc_data *madc,
					 int conv_method)
{
	const struct twl4030_madc_conversion_method *method;
	int ret = 0;
	method = &twl4030_conversion_methods[conv_method];
	switch (conv_method) {
	case TWL4030_MADC_SW1:
	case TWL4030_MADC_SW2:
		ret = twl_i2c_write_u8(TWL4030_MODULE_MADC,
				       TWL4030_MADC_SW_START, method->ctrl);
		if (ret) {
			dev_err(madc->dev,
				"unable to write ctrl register 0x%X\n",
				method->ctrl);
			return ret;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int twl4030_madc_wait_conversion_ready(struct twl4030_madc_data *madc,
					      unsigned int timeout_ms,
					      u8 status_reg)
{
	unsigned long timeout;
	int ret;

	timeout = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		u8 reg;

		ret = twl_i2c_read_u8(TWL4030_MODULE_MADC, &reg, status_reg);
		if (ret) {
			dev_err(madc->dev,
				"unable to read status register 0x%X\n",
				status_reg);
			return ret;
		}
		if (!(reg & TWL4030_MADC_BUSY) && (reg & TWL4030_MADC_EOC_SW))
			return 0;
		usleep_range(500, 2000);
	} while (!time_after(jiffies, timeout));
	dev_err(madc->dev, "conversion timeout!\n");

	return -EAGAIN;
}

int twl4030_madc_conversion(struct twl4030_madc_request *req)
{
	const struct twl4030_madc_conversion_method *method;
	u8 ch_msb, ch_lsb;
	int ret;

	if (!req || !twl4030_madc)
		return -EINVAL;

	mutex_lock(&twl4030_madc->lock);
	if (req->method < TWL4030_MADC_RT || req->method > TWL4030_MADC_SW2) {
		ret = -EINVAL;
		goto out;
	}
	
	if (twl4030_madc->requests[req->method].active) {
		ret = -EBUSY;
		goto out;
	}
	ch_msb = (req->channels >> 8) & 0xff;
	ch_lsb = req->channels & 0xff;
	method = &twl4030_conversion_methods[req->method];
	
	ret = twl_i2c_write_u8(TWL4030_MODULE_MADC, ch_msb, method->sel + 1);
	if (ret) {
		dev_err(twl4030_madc->dev,
			"unable to write sel register 0x%X\n", method->sel + 1);
		goto out;
	}
	ret = twl_i2c_write_u8(TWL4030_MODULE_MADC, ch_lsb, method->sel);
	if (ret) {
		dev_err(twl4030_madc->dev,
			"unable to write sel register 0x%X\n", method->sel + 1);
		goto out;
	}
	
	if (req->do_avg) {
		ret = twl_i2c_write_u8(TWL4030_MODULE_MADC,
				       ch_msb, method->avg + 1);
		if (ret) {
			dev_err(twl4030_madc->dev,
				"unable to write avg register 0x%X\n",
				method->avg + 1);
			goto out;
		}
		ret = twl_i2c_write_u8(TWL4030_MODULE_MADC,
				       ch_lsb, method->avg);
		if (ret) {
			dev_err(twl4030_madc->dev,
				"unable to write sel reg 0x%X\n",
				method->sel + 1);
			goto out;
		}
	}
	if (req->type == TWL4030_MADC_IRQ_ONESHOT && req->func_cb != NULL) {
		ret = twl4030_madc_set_irq(twl4030_madc, req);
		if (ret < 0)
			goto out;
		ret = twl4030_madc_start_conversion(twl4030_madc, req->method);
		if (ret < 0)
			goto out;
		twl4030_madc->requests[req->method].active = 1;
		ret = 0;
		goto out;
	}
	
	if (req->method == TWL4030_MADC_RT) {
		ret = -EINVAL;
		goto out;
	}
	ret = twl4030_madc_start_conversion(twl4030_madc, req->method);
	if (ret < 0)
		goto out;
	twl4030_madc->requests[req->method].active = 1;
	
	ret = twl4030_madc_wait_conversion_ready(twl4030_madc, 5, method->ctrl);
	if (ret) {
		twl4030_madc->requests[req->method].active = 0;
		goto out;
	}
	ret = twl4030_madc_read_channels(twl4030_madc, method->rbase,
					 req->channels, req->rbuf);
	twl4030_madc->requests[req->method].active = 0;

out:
	mutex_unlock(&twl4030_madc->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(twl4030_madc_conversion);

int twl4030_get_madc_conversion(int channel_no)
{
	struct twl4030_madc_request req;
	int temp = 0;
	int ret;

	req.channels = (1 << channel_no);
	req.method = TWL4030_MADC_SW2;
	req.active = 0;
	req.func_cb = NULL;
	ret = twl4030_madc_conversion(&req);
	if (ret < 0)
		return ret;
	if (req.rbuf[channel_no] > 0)
		temp = req.rbuf[channel_no];

	return temp;
}
EXPORT_SYMBOL_GPL(twl4030_get_madc_conversion);

static int twl4030_madc_set_current_generator(struct twl4030_madc_data *madc,
					      int chan, int on)
{
	int ret;
	u8 regval;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MAIN_CHARGE,
			      &regval, TWL4030_BCI_BCICTL1);
	if (ret) {
		dev_err(madc->dev, "unable to read BCICTL1 reg 0x%X",
			TWL4030_BCI_BCICTL1);
		return ret;
	}
	if (on)
		regval |= chan ? TWL4030_BCI_ITHEN : TWL4030_BCI_TYPEN;
	else
		regval &= chan ? ~TWL4030_BCI_ITHEN : ~TWL4030_BCI_TYPEN;
	ret = twl_i2c_write_u8(TWL4030_MODULE_MAIN_CHARGE,
			       regval, TWL4030_BCI_BCICTL1);
	if (ret) {
		dev_err(madc->dev, "unable to write BCICTL1 reg 0x%X\n",
			TWL4030_BCI_BCICTL1);
		return ret;
	}

	return 0;
}

static int twl4030_madc_set_power(struct twl4030_madc_data *madc, int on)
{
	u8 regval;
	int ret;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MAIN_CHARGE,
			      &regval, TWL4030_MADC_CTRL1);
	if (ret) {
		dev_err(madc->dev, "unable to read madc ctrl1 reg 0x%X\n",
			TWL4030_MADC_CTRL1);
		return ret;
	}
	if (on)
		regval |= TWL4030_MADC_MADCON;
	else
		regval &= ~TWL4030_MADC_MADCON;
	ret = twl_i2c_write_u8(TWL4030_MODULE_MADC, regval, TWL4030_MADC_CTRL1);
	if (ret) {
		dev_err(madc->dev, "unable to write madc ctrl1 reg 0x%X\n",
			TWL4030_MADC_CTRL1);
		return ret;
	}

	return 0;
}

static int __devinit twl4030_madc_probe(struct platform_device *pdev)
{
	struct twl4030_madc_data *madc;
	struct twl4030_madc_platform_data *pdata = pdev->dev.platform_data;
	int ret;
	u8 regval;

	if (!pdata) {
		dev_err(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}
	madc = kzalloc(sizeof(*madc), GFP_KERNEL);
	if (!madc)
		return -ENOMEM;

	madc->dev = &pdev->dev;

	madc->imr = (pdata->irq_line == 1) ?
	    TWL4030_MADC_IMR1 : TWL4030_MADC_IMR2;
	madc->isr = (pdata->irq_line == 1) ?
	    TWL4030_MADC_ISR1 : TWL4030_MADC_ISR2;
	ret = twl4030_madc_set_power(madc, 1);
	if (ret < 0)
		goto err_power;
	ret = twl4030_madc_set_current_generator(madc, 0, 1);
	if (ret < 0)
		goto err_current_generator;

	ret = twl_i2c_read_u8(TWL4030_MODULE_MAIN_CHARGE,
			      &regval, TWL4030_BCI_BCICTL1);
	if (ret) {
		dev_err(&pdev->dev, "unable to read reg BCI CTL1 0x%X\n",
			TWL4030_BCI_BCICTL1);
		goto err_i2c;
	}
	regval |= TWL4030_BCI_MESBAT;
	ret = twl_i2c_write_u8(TWL4030_MODULE_MAIN_CHARGE,
			       regval, TWL4030_BCI_BCICTL1);
	if (ret) {
		dev_err(&pdev->dev, "unable to write reg BCI Ctl1 0x%X\n",
			TWL4030_BCI_BCICTL1);
		goto err_i2c;
	}

	
	ret = twl_i2c_read_u8(TWL4030_MODULE_INTBR, &regval, TWL4030_REG_GPBR1);
	if (ret) {
		dev_err(&pdev->dev, "unable to read reg GPBR1 0x%X\n",
				TWL4030_REG_GPBR1);
		goto err_i2c;
	}

	
	if (!(regval & TWL4030_GPBR1_MADC_HFCLK_EN)) {
		dev_info(&pdev->dev, "clk disabled, enabling\n");
		regval |= TWL4030_GPBR1_MADC_HFCLK_EN;
		ret = twl_i2c_write_u8(TWL4030_MODULE_INTBR, regval,
				       TWL4030_REG_GPBR1);
		if (ret) {
			dev_err(&pdev->dev, "unable to write reg GPBR1 0x%X\n",
					TWL4030_REG_GPBR1);
			goto err_i2c;
		}
	}

	platform_set_drvdata(pdev, madc);
	mutex_init(&madc->lock);
	ret = request_threaded_irq(platform_get_irq(pdev, 0), NULL,
				   twl4030_madc_threaded_irq_handler,
				   IRQF_TRIGGER_RISING, "twl4030_madc", madc);
	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq\n");
		goto err_irq;
	}
	twl4030_madc = madc;
	return 0;
err_irq:
	platform_set_drvdata(pdev, NULL);
err_i2c:
	twl4030_madc_set_current_generator(madc, 0, 0);
err_current_generator:
	twl4030_madc_set_power(madc, 0);
err_power:
	kfree(madc);

	return ret;
}

static int __devexit twl4030_madc_remove(struct platform_device *pdev)
{
	struct twl4030_madc_data *madc = platform_get_drvdata(pdev);

	free_irq(platform_get_irq(pdev, 0), madc);
	platform_set_drvdata(pdev, NULL);
	twl4030_madc_set_current_generator(madc, 0, 0);
	twl4030_madc_set_power(madc, 0);
	kfree(madc);

	return 0;
}

static struct platform_driver twl4030_madc_driver = {
	.probe = twl4030_madc_probe,
	.remove = __exit_p(twl4030_madc_remove),
	.driver = {
		   .name = "twl4030_madc",
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(twl4030_madc_driver);

MODULE_DESCRIPTION("TWL4030 ADC driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("J Keerthy");
MODULE_ALIAS("platform:twl4030_madc");
