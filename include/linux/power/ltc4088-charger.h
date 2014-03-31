/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef LTC4088_CHARGER_H_
#define LTC4088_CHARGER_H_

#define LTC4088_CHARGER_DEV_NAME    "ltc4088-charger"

struct ltc4088_charger_platform_data {
	unsigned int	gpio_mode_select_d0;
	unsigned int	gpio_mode_select_d1;
	unsigned int	gpio_mode_select_d2;
};

#endif 
