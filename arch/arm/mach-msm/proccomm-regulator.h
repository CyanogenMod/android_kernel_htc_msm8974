/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_PROCCOMM_REGULATOR_H__
#define __ARCH_ARM_MACH_MSM_PROCCOMM_REGULATOR_H__

#include <linux/regulator/machine.h>

#define PROCCOMM_REGULATOR_DEV_NAME "proccomm-regulator"

struct proccomm_regulator_info {
	struct regulator_init_data	init_data;
	int				id;
	int				rise_time;
	int				pulldown;
	int				negative;
	int				n_voltages;
};

struct proccomm_regulator_platform_data {
	struct proccomm_regulator_info	*regs;
	size_t				nregs;
};

#if defined(CONFIG_MSM_VREG_SWITCH_INVERTED)
#define VREG_SWITCH_ENABLE 0
#define VREG_SWITCH_DISABLE 1
#else
#define VREG_SWITCH_ENABLE 1
#define VREG_SWITCH_DISABLE 0
#endif

#endif
