/*
 * This file is part of wl12xx
 *
 * Copyright (C) 2009 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
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

#ifndef _LINUX_WL12XX_H
#define _LINUX_WL12XX_H

enum {
	WL12XX_REFCLOCK_19	= 0, 
	WL12XX_REFCLOCK_26	= 1, 
	WL12XX_REFCLOCK_38	= 2, 
	WL12XX_REFCLOCK_52	= 3, 
	WL12XX_REFCLOCK_38_XTAL = 4, 
	WL12XX_REFCLOCK_26_XTAL = 5, 
};

enum {
	WL12XX_TCXOCLOCK_19_2	= 0, 
	WL12XX_TCXOCLOCK_26	= 1, 
	WL12XX_TCXOCLOCK_38_4	= 2, 
	WL12XX_TCXOCLOCK_52	= 3, 
	WL12XX_TCXOCLOCK_16_368	= 4, 
	WL12XX_TCXOCLOCK_32_736	= 5, 
	WL12XX_TCXOCLOCK_16_8	= 6, 
	WL12XX_TCXOCLOCK_33_6	= 7, 
};

struct wl12xx_platform_data {
	void (*set_power)(bool enable);
	
	int irq;
	bool use_eeprom;
	int board_ref_clock;
	int board_tcxo_clock;
	unsigned long platform_quirks;
	bool pwr_in_suspend;

	struct wl1271_if_operations *ops;
};

#define WL12XX_PLATFORM_QUIRK_EDGE_IRQ	BIT(0)

#ifdef CONFIG_WL12XX_PLATFORM_DATA

int wl12xx_set_platform_data(const struct wl12xx_platform_data *data);

#else

static inline
int wl12xx_set_platform_data(const struct wl12xx_platform_data *data)
{
	return -ENOSYS;
}

#endif

struct wl12xx_platform_data *wl12xx_get_platform_data(void);

#endif
