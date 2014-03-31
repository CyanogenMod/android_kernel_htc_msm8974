/*
 * TI DaVinci GPIO Support
 *
 * Copyright (c) 2006 David Brownell
 * Copyright (c) 2007, MontaVista Software, Inc. <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef	__DAVINCI_DAVINCI_GPIO_H
#define	__DAVINCI_DAVINCI_GPIO_H

#include <linux/io.h>
#include <linux/spinlock.h>

#include <asm-generic/gpio.h>

#include <mach/irqs.h>
#include <mach/common.h>

#define DAVINCI_GPIO_BASE 0x01C67000

enum davinci_gpio_type {
	GPIO_TYPE_DAVINCI = 0,
	GPIO_TYPE_TNETV107X,
};

#define	GPIO(X)		(X)		

#define GPIO_TO_PIN(bank, gpio)	(16 * (bank) + (gpio))

struct davinci_gpio_controller {
	struct gpio_chip	chip;
	int			irq_base;
	spinlock_t		lock;
	void __iomem		*regs;
	void __iomem		*set_data;
	void __iomem		*clr_data;
	void __iomem		*in_data;
};

static inline struct davinci_gpio_controller *
__gpio_to_controller(unsigned gpio)
{
	struct davinci_gpio_controller *ctlrs = davinci_soc_info.gpio_ctlrs;
	int index = gpio / 32;

	if (!ctlrs || index >= davinci_soc_info.gpio_ctlrs_num)
		return NULL;

	return ctlrs + index;
}

static inline u32 __gpio_mask(unsigned gpio)
{
	return 1 << (gpio % 32);
}

#endif	
