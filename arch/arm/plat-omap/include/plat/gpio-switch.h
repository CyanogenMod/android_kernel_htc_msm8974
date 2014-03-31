/*
 * GPIO switch definitions
 *
 * Copyright (C) 2006 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_OMAP_GPIO_SWITCH_H
#define __ASM_ARCH_OMAP_GPIO_SWITCH_H

#include <linux/types.h>

#define OMAP_GPIO_SWITCH_TYPE_COVER		0x0000
#define OMAP_GPIO_SWITCH_TYPE_CONNECTION	0x0001
#define OMAP_GPIO_SWITCH_TYPE_ACTIVITY		0x0002
#define OMAP_GPIO_SWITCH_FLAG_INVERTED		0x0001
#define OMAP_GPIO_SWITCH_FLAG_OUTPUT		0x0002

struct omap_gpio_switch {
	const char *name;
	s16 gpio;
	unsigned flags:4;
	unsigned type:4;

	u16 debounce_rising;
	
	u16 debounce_falling;

	
	void (* notify)(void *data, int state);
	void *notify_data;
};

extern void omap_register_gpio_switches(const struct omap_gpio_switch *tbl,
					int count);

#endif
