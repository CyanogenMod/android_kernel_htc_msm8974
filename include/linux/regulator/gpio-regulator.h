/*
 * gpio-regulator.h
 *
 * Copyright 2011 Heiko Stuebner <heiko@sntech.de>
 *
 * based on fixed.h
 *
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * Copyright (c) 2009 Nokia Corporation
 * Roger Quadros <ext-roger.quadros@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef __REGULATOR_GPIO_H
#define __REGULATOR_GPIO_H

struct regulator_init_data;

enum regulator_type;

struct gpio_regulator_state {
	int value;
	int gpios;
};

struct gpio_regulator_config {
	const char *supply_name;

	int enable_gpio;
	unsigned enable_high:1;
	unsigned enabled_at_boot:1;
	unsigned startup_delay;

	struct gpio *gpios;
	int nr_gpios;

	struct gpio_regulator_state *states;
	int nr_states;

	enum regulator_type type;
	struct regulator_init_data *init_data;
};

#endif
