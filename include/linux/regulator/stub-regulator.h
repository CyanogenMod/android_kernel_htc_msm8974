/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __STUB_REGULATOR_H__
#define __STUB_REGULATOR_H__

#include <linux/regulator/machine.h>

#define STUB_REGULATOR_DRIVER_NAME "stub-regulator"

struct stub_regulator_pdata {
	struct regulator_init_data	init_data;
	int				hpm_min_load;
	int				system_uA;
};

#ifdef CONFIG_REGULATOR_STUB


int __init regulator_stub_init(void);

#else

static inline int __init regulator_stub_init(void)
{
	return -ENODEV;
}

#endif 

#endif
