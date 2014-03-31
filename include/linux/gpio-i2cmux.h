/*
 * gpio-i2cmux interface to platform code
 *
 * Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_GPIO_I2CMUX_H
#define _LINUX_GPIO_I2CMUX_H

#define GPIO_I2CMUX_NO_IDLE	((unsigned)-1)

struct gpio_i2cmux_platform_data {
	int parent;
	int base_nr;
	const unsigned *values;
	int n_values;
	const unsigned *gpios;
	int n_gpios;
	unsigned idle;
};

#endif 
