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
 */

#ifndef __REGULATOR_CPR_REGULATOR_H__
#define __REGULATOR_CPR_REGULATOR_H__

#include <linux/regulator/machine.h>

#define CPR_REGULATOR_DRIVER_NAME	"qti,cpr-regulator"

#define CPR_PVS_EFUSE_BITS_MAX		5
#define CPR_PVS_EFUSE_BINS_MAX		(1 << CPR_PVS_EFUSE_BITS_MAX)

enum cpr_fuse_corner_enum {
	CPR_FUSE_CORNER_SVS = 1,
	CPR_FUSE_CORNER_NORMAL,
	CPR_FUSE_CORNER_TURBO,
	CPR_FUSE_CORNER_MAX,
};

enum cpr_corner_enum {
	CPR_CORNER_1 = 1,
	CPR_CORNER_2,
	CPR_CORNER_3,
	CPR_CORNER_4,
	CPR_CORNER_5,
	CPR_CORNER_6,
	CPR_CORNER_7,
	CPR_CORNER_8,
	CPR_CORNER_9,
	CPR_CORNER_10,
	CPR_CORNER_11,
	CPR_CORNER_12,
};

enum apc_pvs_process_enum {
	APC_PVS_NO,
	APC_PVS_SLOW,
	APC_PVS_NOM,
	APC_PVS_FAST,
	NUM_APC_PVS,
};

enum vdd_mx_vmin_method {
	VDD_MX_VMIN_APC,
	VDD_MX_VMIN_APC_CORNER_CEILING,
	VDD_MX_VMIN_APC_SLOW_CORNER_CEILING,
	VDD_MX_VMIN_MX_VMAX,
};

#ifdef CONFIG_MSM_CPR_REGULATOR

int __init cpr_regulator_init(void);

#else

static inline int __init cpr_regulator_init(void)
{
	return -ENODEV;
}

#endif 

#endif 
