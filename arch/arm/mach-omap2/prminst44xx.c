/*
 * OMAP4 PRM instance functions
 *
 * Copyright (C) 2009 Nokia Corporation
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include "iomap.h"
#include "common.h"
#include "prm44xx.h"
#include "prminst44xx.h"
#include "prm-regbits-44xx.h"
#include "prcm44xx.h"
#include "prcm_mpu44xx.h"

static u32 _prm_bases[OMAP4_MAX_PRCM_PARTITIONS] = {
	[OMAP4430_INVALID_PRCM_PARTITION]	= 0,
	[OMAP4430_PRM_PARTITION]		= OMAP4430_PRM_BASE,
	[OMAP4430_CM1_PARTITION]		= 0,
	[OMAP4430_CM2_PARTITION]		= 0,
	[OMAP4430_SCRM_PARTITION]		= 0,
	[OMAP4430_PRCM_MPU_PARTITION]		= OMAP4430_PRCM_MPU_BASE,
};

u32 omap4_prminst_read_inst_reg(u8 part, s16 inst, u16 idx)
{
	BUG_ON(part >= OMAP4_MAX_PRCM_PARTITIONS ||
	       part == OMAP4430_INVALID_PRCM_PARTITION ||
	       !_prm_bases[part]);
	return __raw_readl(OMAP2_L4_IO_ADDRESS(_prm_bases[part] + inst +
					       idx));
}

void omap4_prminst_write_inst_reg(u32 val, u8 part, s16 inst, u16 idx)
{
	BUG_ON(part >= OMAP4_MAX_PRCM_PARTITIONS ||
	       part == OMAP4430_INVALID_PRCM_PARTITION ||
	       !_prm_bases[part]);
	__raw_writel(val, OMAP2_L4_IO_ADDRESS(_prm_bases[part] + inst + idx));
}

u32 omap4_prminst_rmw_inst_reg_bits(u32 mask, u32 bits, u8 part, s16 inst,
				    u16 idx)
{
	u32 v;

	v = omap4_prminst_read_inst_reg(part, inst, idx);
	v &= ~mask;
	v |= bits;
	omap4_prminst_write_inst_reg(v, part, inst, idx);

	return v;
}

#define OMAP4_RST_CTRL_ST_OFFSET		4

int omap4_prminst_is_hardreset_asserted(u8 shift, u8 part, s16 inst,
					u16 rstctrl_offs)
{
	u32 v;

	v = omap4_prminst_read_inst_reg(part, inst, rstctrl_offs);
	v &= 1 << shift;
	v >>= shift;

	return v;
}

int omap4_prminst_assert_hardreset(u8 shift, u8 part, s16 inst,
				   u16 rstctrl_offs)
{
	u32 mask = 1 << shift;

	omap4_prminst_rmw_inst_reg_bits(mask, mask, part, inst, rstctrl_offs);

	return 0;
}

int omap4_prminst_deassert_hardreset(u8 shift, u8 part, s16 inst,
				     u16 rstctrl_offs)
{
	int c;
	u32 mask = 1 << shift;
	u16 rstst_offs = rstctrl_offs + OMAP4_RST_CTRL_ST_OFFSET;

	
	if (omap4_prminst_is_hardreset_asserted(shift, part, inst,
						rstctrl_offs) == 0)
		return -EEXIST;

	
	omap4_prminst_rmw_inst_reg_bits(0xffffffff, mask, part, inst,
					rstst_offs);
	
	omap4_prminst_rmw_inst_reg_bits(mask, 0, part, inst, rstctrl_offs);
	
	omap_test_timeout(omap4_prminst_is_hardreset_asserted(shift, part, inst,
							      rstst_offs),
			  MAX_MODULE_HARDRESET_WAIT, c);

	return (c == MAX_MODULE_HARDRESET_WAIT) ? -EBUSY : 0;
}


void omap4_prminst_global_warm_sw_reset(void)
{
	u32 v;

	v = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
				    OMAP4430_PRM_DEVICE_INST,
				    OMAP4_PRM_RSTCTRL_OFFSET);
	v |= OMAP4430_RST_GLOBAL_WARM_SW_MASK;
	omap4_prminst_write_inst_reg(v, OMAP4430_PRM_PARTITION,
				 OMAP4430_PRM_DEVICE_INST,
				 OMAP4_PRM_RSTCTRL_OFFSET);

	
	v = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
				    OMAP4430_PRM_DEVICE_INST,
				    OMAP4_PRM_RSTCTRL_OFFSET);
}
