/*
 * OMAP SoC specific OPP Data helpers
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *	Kevin Hilman
 * Copyright (C) 2010 Nokia Corporation.
 *      Eduardo Valentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_OMAP_OPP_DATA_H
#define __ARCH_ARM_MACH_OMAP2_OMAP_OPP_DATA_H

#include <plat/omap_hwmod.h>

#include "voltage.h"


struct omap_opp_def {
	char *hwmod_name;

	unsigned long freq;
	unsigned long u_volt;

	bool default_available;
};

#define OPP_INITIALIZER(_hwmod_name, _enabled, _freq, _uv)	\
{								\
	.hwmod_name	= _hwmod_name,				\
	.default_available	= _enabled,			\
	.freq		= _freq,				\
	.u_volt		= _uv,					\
}

#define VOLT_DATA_DEFINE(_v_nom, _efuse_offs, _errminlimit, _errgain)  \
{								       \
	.volt_nominal	= _v_nom,				       \
	.sr_efuse_offs	= _efuse_offs,				       \
	.sr_errminlimit = _errminlimit,				       \
	.vp_errgain	= _errgain				       \
}

extern int __init omap_init_opp_table(struct omap_opp_def *opp_def,
		u32 opp_def_size);


extern struct omap_volt_data omap34xx_vddmpu_volt_data[];
extern struct omap_volt_data omap34xx_vddcore_volt_data[];
extern struct omap_volt_data omap36xx_vddmpu_volt_data[];
extern struct omap_volt_data omap36xx_vddcore_volt_data[];

extern struct omap_volt_data omap44xx_vdd_mpu_volt_data[];
extern struct omap_volt_data omap44xx_vdd_iva_volt_data[];
extern struct omap_volt_data omap44xx_vdd_core_volt_data[];

#endif		
