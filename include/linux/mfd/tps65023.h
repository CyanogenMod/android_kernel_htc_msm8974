/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_I2C_TPS65023_H
#define __LINUX_I2C_TPS65023_H

#ifndef CONFIG_TPS65023
#define tps65023_set_dcdc1_level(mvolts)  (-ENODEV)

#define tps65023_get_dcdc1_level(mvolts)  (-ENODEV)

#else
extern int tps65023_set_dcdc1_level(int mvolts);

extern int tps65023_get_dcdc1_level(int *mvolts);
#endif

#endif
