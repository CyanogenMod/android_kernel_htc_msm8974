/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_BLUETOOTH_POWER_H
#define __LINUX_BLUETOOTH_POWER_H

struct bt_power_vreg_data {
	
	struct regulator *reg;
	
	const char *name;
	
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	bool set_voltage_sup;
	
	bool is_enabled;
};

struct bluetooth_power_platform_data {
	
	int bt_gpio_sys_rst;
	
	struct bt_power_vreg_data *bt_vdd_io;
	
	struct bt_power_vreg_data *bt_vdd_pa;
	
	struct bt_power_vreg_data *bt_vdd_ldo;
	struct bt_power_vreg_data *bt_chip_pwd;
	
	int (*bt_power_setup) (int);
};

#endif 
