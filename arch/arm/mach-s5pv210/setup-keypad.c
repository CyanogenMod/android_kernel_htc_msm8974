/*
 * linux/arch/arm/mach-s5pv210/setup-keypad.c
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

void samsung_keypad_cfg_gpio(unsigned int rows, unsigned int cols)
{
	
	s3c_gpio_cfgrange_nopull(S5PV210_GPH3(0), rows, S3C_GPIO_SFN(3));

	
	s3c_gpio_cfgrange_nopull(S5PV210_GPH2(0), cols, S3C_GPIO_SFN(3));
}
