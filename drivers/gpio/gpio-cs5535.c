/*
 * AMD CS5535/CS5536 GPIO driver
 * Copyright (C) 2006  Advanced Micro Devices, Inc.
 * Copyright (C) 2007-2009  Andres Salomon <dilinger@collabora.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/cs5535.h>
#include <asm/msr.h>

#define DRV_NAME "cs5535-gpio"

#define GPIO_DEFAULT_MASK 0x0F7FFFFF

static ulong mask = GPIO_DEFAULT_MASK;
module_param_named(mask, mask, ulong, 0444);
MODULE_PARM_DESC(mask, "GPIO channel mask.");

static struct cs5535_gpio_chip {
	struct gpio_chip chip;
	resource_size_t base;

	struct platform_device *pdev;
	spinlock_t lock;
} cs5535_gpio_chip;


static void errata_outl(struct cs5535_gpio_chip *chip, u32 val,
		unsigned int reg)
{
	unsigned long addr = chip->base + 0x80 + reg;

	if (reg != GPIO_POSITIVE_EDGE_STS && reg != GPIO_NEGATIVE_EDGE_STS) {
		if (val & 0xffff)
			val |= (inl(addr) & 0xffff); 
		else
			val |= (inl(addr) ^ (val >> 16));
	}
	outl(val, addr);
}

static void __cs5535_gpio_set(struct cs5535_gpio_chip *chip, unsigned offset,
		unsigned int reg)
{
	if (offset < 16)
		
		outl(1 << offset, chip->base + reg);
	else
		
		errata_outl(chip, 1 << (offset - 16), reg);
}

void cs5535_gpio_set(unsigned offset, unsigned int reg)
{
	struct cs5535_gpio_chip *chip = &cs5535_gpio_chip;
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__cs5535_gpio_set(chip, offset, reg);
	spin_unlock_irqrestore(&chip->lock, flags);
}
EXPORT_SYMBOL_GPL(cs5535_gpio_set);

static void __cs5535_gpio_clear(struct cs5535_gpio_chip *chip, unsigned offset,
		unsigned int reg)
{
	if (offset < 16)
		
		outl(1 << (offset + 16), chip->base + reg);
	else
		
		errata_outl(chip, 1 << offset, reg);
}

void cs5535_gpio_clear(unsigned offset, unsigned int reg)
{
	struct cs5535_gpio_chip *chip = &cs5535_gpio_chip;
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__cs5535_gpio_clear(chip, offset, reg);
	spin_unlock_irqrestore(&chip->lock, flags);
}
EXPORT_SYMBOL_GPL(cs5535_gpio_clear);

int cs5535_gpio_isset(unsigned offset, unsigned int reg)
{
	struct cs5535_gpio_chip *chip = &cs5535_gpio_chip;
	unsigned long flags;
	long val;

	spin_lock_irqsave(&chip->lock, flags);
	if (offset < 16)
		
		val = inl(chip->base + reg);
	else {
		
		val = inl(chip->base + 0x80 + reg);
		offset -= 16;
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	return (val & (1 << offset)) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(cs5535_gpio_isset);

int cs5535_gpio_set_irq(unsigned group, unsigned irq)
{
	uint32_t lo, hi;

	if (group > 7 || irq > 15)
		return -EINVAL;

	rdmsr(MSR_PIC_ZSEL_HIGH, lo, hi);

	lo &= ~(0xF << (group * 4));
	lo |= (irq & 0xF) << (group * 4);

	wrmsr(MSR_PIC_ZSEL_HIGH, lo, hi);
	return 0;
}
EXPORT_SYMBOL_GPL(cs5535_gpio_set_irq);

void cs5535_gpio_setup_event(unsigned offset, int pair, int pme)
{
	struct cs5535_gpio_chip *chip = &cs5535_gpio_chip;
	uint32_t shift = (offset % 8) * 4;
	unsigned long flags;
	uint32_t val;

	if (offset >= 24)
		offset = GPIO_MAP_W;
	else if (offset >= 16)
		offset = GPIO_MAP_Z;
	else if (offset >= 8)
		offset = GPIO_MAP_Y;
	else
		offset = GPIO_MAP_X;

	spin_lock_irqsave(&chip->lock, flags);
	val = inl(chip->base + offset);

	
	val &= ~(0xF << shift);

	
	val |= ((pair & 7) << shift);

	
	if (pme)
		val |= (1 << (shift + 3));

	outl(val, chip->base + offset);
	spin_unlock_irqrestore(&chip->lock, flags);
}
EXPORT_SYMBOL_GPL(cs5535_gpio_setup_event);


static int chip_gpio_request(struct gpio_chip *c, unsigned offset)
{
	struct cs5535_gpio_chip *chip = (struct cs5535_gpio_chip *) c;
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);

	
	if ((mask & (1 << offset)) == 0) {
		dev_info(&chip->pdev->dev,
			"pin %u is not available (check mask)\n", offset);
		spin_unlock_irqrestore(&chip->lock, flags);
		return -EINVAL;
	}

	
	__cs5535_gpio_clear(chip, offset, GPIO_OUTPUT_AUX1);
	__cs5535_gpio_clear(chip, offset, GPIO_OUTPUT_AUX2);

	
	__cs5535_gpio_clear(chip, offset, GPIO_INPUT_AUX1);

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int chip_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	return cs5535_gpio_isset(offset, GPIO_READ_BACK);
}

static void chip_gpio_set(struct gpio_chip *chip, unsigned offset, int val)
{
	if (val)
		cs5535_gpio_set(offset, GPIO_OUTPUT_VAL);
	else
		cs5535_gpio_clear(offset, GPIO_OUTPUT_VAL);
}

static int chip_direction_input(struct gpio_chip *c, unsigned offset)
{
	struct cs5535_gpio_chip *chip = (struct cs5535_gpio_chip *) c;
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__cs5535_gpio_set(chip, offset, GPIO_INPUT_ENABLE);
	__cs5535_gpio_clear(chip, offset, GPIO_OUTPUT_ENABLE);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int chip_direction_output(struct gpio_chip *c, unsigned offset, int val)
{
	struct cs5535_gpio_chip *chip = (struct cs5535_gpio_chip *) c;
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);

	__cs5535_gpio_set(chip, offset, GPIO_INPUT_ENABLE);
	__cs5535_gpio_set(chip, offset, GPIO_OUTPUT_ENABLE);
	if (val)
		__cs5535_gpio_set(chip, offset, GPIO_OUTPUT_VAL);
	else
		__cs5535_gpio_clear(chip, offset, GPIO_OUTPUT_VAL);

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static const char * const cs5535_gpio_names[] = {
	"GPIO0", "GPIO1", "GPIO2", "GPIO3",
	"GPIO4", "GPIO5", "GPIO6", "GPIO7",
	"GPIO8", "GPIO9", "GPIO10", "GPIO11",
	"GPIO12", "GPIO13", "GPIO14", "GPIO15",
	"GPIO16", "GPIO17", "GPIO18", "GPIO19",
	"GPIO20", "GPIO21", "GPIO22", NULL,
	"GPIO24", "GPIO25", "GPIO26", "GPIO27",
	"GPIO28", NULL, NULL, NULL,
};

static struct cs5535_gpio_chip cs5535_gpio_chip = {
	.chip = {
		.owner = THIS_MODULE,
		.label = DRV_NAME,

		.base = 0,
		.ngpio = 32,
		.names = cs5535_gpio_names,
		.request = chip_gpio_request,

		.get = chip_gpio_get,
		.set = chip_gpio_set,

		.direction_input = chip_direction_input,
		.direction_output = chip_direction_output,
	},
};

static int __devinit cs5535_gpio_probe(struct platform_device *pdev)
{
	struct resource *res;
	int err = -EIO;
	ulong mask_orig = mask;


	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't fetch device resource info\n");
		goto done;
	}

	if (!request_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "can't request region\n");
		goto done;
	}

	
	cs5535_gpio_chip.base = res->start;
	cs5535_gpio_chip.pdev = pdev;
	spin_lock_init(&cs5535_gpio_chip.lock);

	dev_info(&pdev->dev, "reserved resource region %pR\n", res);

	
	mask &= 0x1F7FFFFF;

	mask &= ~(1 << 28);

	if (mask_orig != mask)
		dev_info(&pdev->dev, "mask changed from 0x%08lX to 0x%08lX\n",
				mask_orig, mask);

	
	err = gpiochip_add(&cs5535_gpio_chip.chip);
	if (err)
		goto release_region;

	return 0;

release_region:
	release_region(res->start, resource_size(res));
done:
	return err;
}

static int __devexit cs5535_gpio_remove(struct platform_device *pdev)
{
	struct resource *r;
	int err;

	err = gpiochip_remove(&cs5535_gpio_chip.chip);
	if (err) {
		
		dev_err(&pdev->dev, "unable to remove gpio_chip?\n");
		return err;
	}

	r = platform_get_resource(pdev, IORESOURCE_IO, 0);
	release_region(r->start, resource_size(r));
	return 0;
}

static struct platform_driver cs5535_gpio_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = cs5535_gpio_probe,
	.remove = __devexit_p(cs5535_gpio_remove),
};

module_platform_driver(cs5535_gpio_driver);

MODULE_AUTHOR("Andres Salomon <dilinger@queued.net>");
MODULE_DESCRIPTION("AMD CS5535/CS5536 GPIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
