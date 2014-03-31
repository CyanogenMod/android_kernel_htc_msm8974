/*
 * Access to GPIOs on TWL4030/TPS659x0 chips
 *
 * Copyright (C) 2006-2007 Texas Instruments, Inc.
 * Copyright (C) 2006 MontaVista Software, Inc.
 *
 * Code re-arranged and cleaned up by:
 *	Syed Mohammed Khasim <x0khasim@ti.com>
 *
 * Initial Code:
 *	Andy Lowe / Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/irqdomain.h>

#include <linux/i2c/twl.h>




static struct gpio_chip twl_gpiochip;
static int twl4030_gpio_irq_base;

#ifdef MODULE
#define is_module()	true
#else
#define is_module()	false
#endif

#define MASK_GPIO_CTRL_GPIO0CD1		BIT(0)
#define MASK_GPIO_CTRL_GPIO1CD2		BIT(1)
#define MASK_GPIO_CTRL_GPIO_ON		BIT(2)

#define GPIO_32_MASK			0x0003ffff

static DEFINE_MUTEX(gpio_lock);

static unsigned int gpio_usage_count;


static inline int gpio_twl4030_write(u8 address, u8 data)
{
	return twl_i2c_write_u8(TWL4030_MODULE_GPIO, data, address);
}



#define TWL4030_LED_LEDEN	0x0

#define LEDEN_LEDAON		BIT(0)
#define LEDEN_LEDBON		BIT(1)
#define LEDEN_LEDAEXT		BIT(2)
#define LEDEN_LEDBEXT		BIT(3)
#define LEDEN_LEDAPWM		BIT(4)
#define LEDEN_LEDBPWM		BIT(5)
#define LEDEN_PWM_LENGTHA	BIT(6)
#define LEDEN_PWM_LENGTHB	BIT(7)

#define TWL4030_PWMx_PWMxON	0x0
#define TWL4030_PWMx_PWMxOFF	0x1

#define PWMxON_LENGTH		BIT(7)


static inline int gpio_twl4030_read(u8 address)
{
	u8 data;
	int ret = 0;

	ret = twl_i2c_read_u8(TWL4030_MODULE_GPIO, &data, address);
	return (ret < 0) ? ret : data;
}


static u8 cached_leden;		

static void twl4030_led_set_value(int led, int value)
{
	u8 mask = LEDEN_LEDAON | LEDEN_LEDAPWM;
	int status;

	if (led)
		mask <<= 1;

	mutex_lock(&gpio_lock);
	if (value)
		cached_leden &= ~mask;
	else
		cached_leden |= mask;
	status = twl_i2c_write_u8(TWL4030_MODULE_LED, cached_leden,
			TWL4030_LED_LEDEN);
	mutex_unlock(&gpio_lock);
}

static int twl4030_set_gpio_direction(int gpio, int is_input)
{
	u8 d_bnk = gpio >> 3;
	u8 d_msk = BIT(gpio & 0x7);
	u8 reg = 0;
	u8 base = REG_GPIODATADIR1 + d_bnk;
	int ret = 0;

	mutex_lock(&gpio_lock);
	ret = gpio_twl4030_read(base);
	if (ret >= 0) {
		if (is_input)
			reg = ret & ~d_msk;
		else
			reg = ret | d_msk;

		ret = gpio_twl4030_write(base, reg);
	}
	mutex_unlock(&gpio_lock);
	return ret;
}

static int twl4030_set_gpio_dataout(int gpio, int enable)
{
	u8 d_bnk = gpio >> 3;
	u8 d_msk = BIT(gpio & 0x7);
	u8 base = 0;

	if (enable)
		base = REG_SETGPIODATAOUT1 + d_bnk;
	else
		base = REG_CLEARGPIODATAOUT1 + d_bnk;

	return gpio_twl4030_write(base, d_msk);
}

static int twl4030_get_gpio_datain(int gpio)
{
	u8 d_bnk = gpio >> 3;
	u8 d_off = gpio & 0x7;
	u8 base = 0;
	int ret = 0;

	if (unlikely((gpio >= TWL4030_GPIO_MAX)
		|| !(gpio_usage_count & BIT(gpio))))
		return -EPERM;

	base = REG_GPIODATAIN1 + d_bnk;
	ret = gpio_twl4030_read(base);
	if (ret > 0)
		ret = (ret >> d_off) & 0x1;

	return ret;
}


static int twl_request(struct gpio_chip *chip, unsigned offset)
{
	int status = 0;

	mutex_lock(&gpio_lock);

	
	if (offset >= TWL4030_GPIO_MAX) {
		u8	ledclr_mask = LEDEN_LEDAON | LEDEN_LEDAEXT
				| LEDEN_LEDAPWM | LEDEN_PWM_LENGTHA;
		u8	module = TWL4030_MODULE_PWMA;

		offset -= TWL4030_GPIO_MAX;
		if (offset) {
			ledclr_mask <<= 1;
			module = TWL4030_MODULE_PWMB;
		}

		
		status = twl_i2c_write_u8(module, 0x7f,
				TWL4030_PWMx_PWMxOFF);
		if (status < 0)
			goto done;
		status = twl_i2c_write_u8(module, 0x7f,
				TWL4030_PWMx_PWMxON);
		if (status < 0)
			goto done;

		
		module = TWL4030_MODULE_LED;
		status = twl_i2c_read_u8(module, &cached_leden,
				TWL4030_LED_LEDEN);
		if (status < 0)
			goto done;
		cached_leden &= ~ledclr_mask;
		status = twl_i2c_write_u8(module, cached_leden,
				TWL4030_LED_LEDEN);
		if (status < 0)
			goto done;

		status = 0;
		goto done;
	}

	
	if (!gpio_usage_count) {
		struct twl4030_gpio_platform_data *pdata;
		u8 value = MASK_GPIO_CTRL_GPIO_ON;

		pdata = chip->dev->platform_data;
		if (pdata)
			value |= pdata->mmc_cd & 0x03;

		status = gpio_twl4030_write(REG_GPIO_CTRL, value);
	}

	if (!status)
		gpio_usage_count |= (0x1 << offset);

done:
	mutex_unlock(&gpio_lock);
	return status;
}

static void twl_free(struct gpio_chip *chip, unsigned offset)
{
	if (offset >= TWL4030_GPIO_MAX) {
		twl4030_led_set_value(offset - TWL4030_GPIO_MAX, 1);
		return;
	}

	mutex_lock(&gpio_lock);

	gpio_usage_count &= ~BIT(offset);

	
	if (!gpio_usage_count)
		gpio_twl4030_write(REG_GPIO_CTRL, 0x0);

	mutex_unlock(&gpio_lock);
}

static int twl_direction_in(struct gpio_chip *chip, unsigned offset)
{
	return (offset < TWL4030_GPIO_MAX)
		? twl4030_set_gpio_direction(offset, 1)
		: -EINVAL;
}

static int twl_get(struct gpio_chip *chip, unsigned offset)
{
	int status = 0;

	if (offset < TWL4030_GPIO_MAX)
		status = twl4030_get_gpio_datain(offset);
	else if (offset == TWL4030_GPIO_MAX)
		status = cached_leden & LEDEN_LEDAON;
	else
		status = cached_leden & LEDEN_LEDBON;
	return (status < 0) ? 0 : status;
}

static int twl_direction_out(struct gpio_chip *chip, unsigned offset, int value)
{
	if (offset < TWL4030_GPIO_MAX) {
		twl4030_set_gpio_dataout(offset, value);
		return twl4030_set_gpio_direction(offset, 0);
	} else {
		twl4030_led_set_value(offset - TWL4030_GPIO_MAX, value);
		return 0;
	}
}

static void twl_set(struct gpio_chip *chip, unsigned offset, int value)
{
	if (offset < TWL4030_GPIO_MAX)
		twl4030_set_gpio_dataout(offset, value);
	else
		twl4030_led_set_value(offset - TWL4030_GPIO_MAX, value);
}

static int twl_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return (twl4030_gpio_irq_base && (offset < TWL4030_GPIO_MAX))
		? (twl4030_gpio_irq_base + offset)
		: -EINVAL;
}

static struct gpio_chip twl_gpiochip = {
	.label			= "twl4030",
	.owner			= THIS_MODULE,
	.request		= twl_request,
	.free			= twl_free,
	.direction_input	= twl_direction_in,
	.get			= twl_get,
	.direction_output	= twl_direction_out,
	.set			= twl_set,
	.to_irq			= twl_to_irq,
	.can_sleep		= 1,
};


static int __devinit gpio_twl4030_pulls(u32 ups, u32 downs)
{
	u8		message[6];
	unsigned	i, gpio_bit;

	for (gpio_bit = 1, i = 1; i < 6; i++) {
		u8		bit_mask;
		unsigned	j;

		for (bit_mask = 0, j = 0; j < 8; j += 2, gpio_bit <<= 1) {
			if (ups & gpio_bit)
				bit_mask |= 1 << (j + 1);
			else if (downs & gpio_bit)
				bit_mask |= 1 << (j + 0);
		}
		message[i] = bit_mask;
	}

	return twl_i2c_write(TWL4030_MODULE_GPIO, message,
				REG_GPIOPUPDCTR1, 5);
}

static int __devinit gpio_twl4030_debounce(u32 debounce, u8 mmc_cd)
{
	u8		message[4];

	message[1] = (debounce & 0xff) | (mmc_cd & 0x03);
	debounce >>= 8;
	message[2] = (debounce & 0xff);
	debounce >>= 8;
	message[3] = (debounce & 0x03);

	return twl_i2c_write(TWL4030_MODULE_GPIO, message,
				REG_GPIO_DEBEN1, 3);
}

static int gpio_twl4030_remove(struct platform_device *pdev);

static int __devinit gpio_twl4030_probe(struct platform_device *pdev)
{
	struct twl4030_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct device_node *node = pdev->dev.of_node;
	int ret, irq_base;

	
	if (is_module()) {
		dev_err(&pdev->dev, "can't dispatch IRQs from modules\n");
		goto no_irqs;
	}

	irq_base = irq_alloc_descs(-1, 0, TWL4030_GPIO_MAX, 0);
	if (irq_base < 0) {
		dev_err(&pdev->dev, "Failed to alloc irq_descs\n");
		return irq_base;
	}

	irq_domain_add_legacy(node, TWL4030_GPIO_MAX, irq_base, 0,
			      &irq_domain_simple_ops, NULL);

	ret = twl4030_sih_setup(&pdev->dev, TWL4030_MODULE_GPIO, irq_base);
	if (ret < 0)
		return ret;

	twl4030_gpio_irq_base = irq_base;

no_irqs:
	twl_gpiochip.base = -1;
	twl_gpiochip.ngpio = TWL4030_GPIO_MAX;
	twl_gpiochip.dev = &pdev->dev;

	if (pdata) {
		twl_gpiochip.base = pdata->gpio_base;

		ret = gpio_twl4030_pulls(pdata->pullups, pdata->pulldowns);
		if (ret)
			dev_dbg(&pdev->dev, "pullups %.05x %.05x --> %d\n",
					pdata->pullups, pdata->pulldowns,
					ret);

		ret = gpio_twl4030_debounce(pdata->debounce, pdata->mmc_cd);
		if (ret)
			dev_dbg(&pdev->dev, "debounce %.03x %.01x --> %d\n",
					pdata->debounce, pdata->mmc_cd,
					ret);

		if (pdata->use_leds)
			twl_gpiochip.ngpio += 2;
	}

	ret = gpiochip_add(&twl_gpiochip);
	if (ret < 0) {
		dev_err(&pdev->dev, "could not register gpiochip, %d\n", ret);
		twl_gpiochip.ngpio = 0;
		gpio_twl4030_remove(pdev);
	} else if (pdata && pdata->setup) {
		int status;

		status = pdata->setup(&pdev->dev,
				pdata->gpio_base, TWL4030_GPIO_MAX);
		if (status)
			dev_dbg(&pdev->dev, "setup --> %d\n", status);
	}

	return ret;
}

static int gpio_twl4030_remove(struct platform_device *pdev)
{
	struct twl4030_gpio_platform_data *pdata = pdev->dev.platform_data;
	int status;

	if (pdata && pdata->teardown) {
		status = pdata->teardown(&pdev->dev,
				pdata->gpio_base, TWL4030_GPIO_MAX);
		if (status) {
			dev_dbg(&pdev->dev, "teardown --> %d\n", status);
			return status;
		}
	}

	status = gpiochip_remove(&twl_gpiochip);
	if (status < 0)
		return status;

	if (is_module())
		return 0;

	
	WARN_ON(1);
	return -EIO;
}

static const struct of_device_id twl_gpio_match[] = {
	{ .compatible = "ti,twl4030-gpio", },
	{ },
};
MODULE_DEVICE_TABLE(of, twl_gpio_match);

MODULE_ALIAS("platform:twl4030_gpio");

static struct platform_driver gpio_twl4030_driver = {
	.driver = {
		.name	= "twl4030_gpio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(twl_gpio_match),
	},
	.probe		= gpio_twl4030_probe,
	.remove		= gpio_twl4030_remove,
};

static int __init gpio_twl4030_init(void)
{
	return platform_driver_register(&gpio_twl4030_driver);
}
subsys_initcall(gpio_twl4030_init);

static void __exit gpio_twl4030_exit(void)
{
	platform_driver_unregister(&gpio_twl4030_driver);
}
module_exit(gpio_twl4030_exit);

MODULE_AUTHOR("Texas Instruments, Inc.");
MODULE_DESCRIPTION("GPIO interface for TWL4030");
MODULE_LICENSE("GPL");
