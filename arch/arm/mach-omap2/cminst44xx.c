/*
 * OMAP4 CM instance functions
 *
 * Copyright (C) 2009 Nokia Corporation
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is needed since CM instances can be in the PRM, PRCM_MPU, CM1,
 * or CM2 hardware modules.  For example, the EMU_CM CM instance is in
 * the PRM hardware module.  What a mess...
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include "iomap.h"
#include "common.h"
#include "cm.h"
#include "cm1_44xx.h"
#include "cm2_44xx.h"
#include "cm44xx.h"
#include "cminst44xx.h"
#include "cm-regbits-34xx.h"
#include "cm-regbits-44xx.h"
#include "prcm44xx.h"
#include "prm44xx.h"
#include "prcm_mpu44xx.h"

#define CLKCTRL_IDLEST_FUNCTIONAL		0x0
#define CLKCTRL_IDLEST_INTRANSITION		0x1
#define CLKCTRL_IDLEST_INTERFACE_IDLE		0x2
#define CLKCTRL_IDLEST_DISABLED			0x3

static u32 _cm_bases[OMAP4_MAX_PRCM_PARTITIONS] = {
	[OMAP4430_INVALID_PRCM_PARTITION]	= 0,
	[OMAP4430_PRM_PARTITION]		= OMAP4430_PRM_BASE,
	[OMAP4430_CM1_PARTITION]		= OMAP4430_CM1_BASE,
	[OMAP4430_CM2_PARTITION]		= OMAP4430_CM2_BASE,
	[OMAP4430_SCRM_PARTITION]		= 0,
	[OMAP4430_PRCM_MPU_PARTITION]		= OMAP4430_PRCM_MPU_BASE,
};


static u32 _clkctrl_idlest(u8 part, u16 inst, s16 cdoffs, u16 clkctrl_offs)
{
	u32 v = omap4_cminst_read_inst_reg(part, inst, clkctrl_offs);
	v &= OMAP4430_IDLEST_MASK;
	v >>= OMAP4430_IDLEST_SHIFT;
	return v;
}

static bool _is_module_ready(u8 part, u16 inst, s16 cdoffs, u16 clkctrl_offs)
{
	u32 v;

	v = _clkctrl_idlest(part, inst, cdoffs, clkctrl_offs);

	return (v == CLKCTRL_IDLEST_FUNCTIONAL ||
		v == CLKCTRL_IDLEST_INTERFACE_IDLE) ? true : false;
}


u32 omap4_cminst_read_inst_reg(u8 part, s16 inst, u16 idx)
{
	BUG_ON(part >= OMAP4_MAX_PRCM_PARTITIONS ||
	       part == OMAP4430_INVALID_PRCM_PARTITION ||
	       !_cm_bases[part]);
	return __raw_readl(OMAP2_L4_IO_ADDRESS(_cm_bases[part] + inst + idx));
}

void omap4_cminst_write_inst_reg(u32 val, u8 part, s16 inst, u16 idx)
{
	BUG_ON(part >= OMAP4_MAX_PRCM_PARTITIONS ||
	       part == OMAP4430_INVALID_PRCM_PARTITION ||
	       !_cm_bases[part]);
	__raw_writel(val, OMAP2_L4_IO_ADDRESS(_cm_bases[part] + inst + idx));
}

u32 omap4_cminst_rmw_inst_reg_bits(u32 mask, u32 bits, u8 part, s16 inst,
				   s16 idx)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, idx);
	v &= ~mask;
	v |= bits;
	omap4_cminst_write_inst_reg(v, part, inst, idx);

	return v;
}

u32 omap4_cminst_set_inst_reg_bits(u32 bits, u8 part, s16 inst, s16 idx)
{
	return omap4_cminst_rmw_inst_reg_bits(bits, bits, part, inst, idx);
}

u32 omap4_cminst_clear_inst_reg_bits(u32 bits, u8 part, s16 inst, s16 idx)
{
	return omap4_cminst_rmw_inst_reg_bits(bits, 0x0, part, inst, idx);
}

u32 omap4_cminst_read_inst_reg_bits(u8 part, u16 inst, s16 idx, u32 mask)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, idx);
	v &= mask;
	v >>= __ffs(mask);

	return v;
}


static void _clktrctrl_write(u8 c, u8 part, s16 inst, u16 cdoffs)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, cdoffs + OMAP4_CM_CLKSTCTRL);
	v &= ~OMAP4430_CLKTRCTRL_MASK;
	v |= c << OMAP4430_CLKTRCTRL_SHIFT;
	omap4_cminst_write_inst_reg(v, part, inst, cdoffs + OMAP4_CM_CLKSTCTRL);
}

bool omap4_cminst_is_clkdm_in_hwsup(u8 part, s16 inst, u16 cdoffs)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, cdoffs + OMAP4_CM_CLKSTCTRL);
	v &= OMAP4430_CLKTRCTRL_MASK;
	v >>= OMAP4430_CLKTRCTRL_SHIFT;

	return (v == OMAP34XX_CLKSTCTRL_ENABLE_AUTO) ? true : false;
}

void omap4_cminst_clkdm_enable_hwsup(u8 part, s16 inst, u16 cdoffs)
{
	_clktrctrl_write(OMAP34XX_CLKSTCTRL_ENABLE_AUTO, part, inst, cdoffs);
}

void omap4_cminst_clkdm_disable_hwsup(u8 part, s16 inst, u16 cdoffs)
{
	_clktrctrl_write(OMAP34XX_CLKSTCTRL_DISABLE_AUTO, part, inst, cdoffs);
}

void omap4_cminst_clkdm_force_sleep(u8 part, s16 inst, u16 cdoffs)
{
	_clktrctrl_write(OMAP34XX_CLKSTCTRL_FORCE_SLEEP, part, inst, cdoffs);
}

void omap4_cminst_clkdm_force_wakeup(u8 part, s16 inst, u16 cdoffs)
{
	_clktrctrl_write(OMAP34XX_CLKSTCTRL_FORCE_WAKEUP, part, inst, cdoffs);
}


int omap4_cminst_wait_module_ready(u8 part, u16 inst, s16 cdoffs,
				   u16 clkctrl_offs)
{
	int i = 0;

	if (!clkctrl_offs)
		return 0;

	omap_test_timeout(_is_module_ready(part, inst, cdoffs, clkctrl_offs),
			  MAX_MODULE_READY_TIME, i);

	return (i < MAX_MODULE_READY_TIME) ? 0 : -EBUSY;
}

int omap4_cminst_wait_module_idle(u8 part, u16 inst, s16 cdoffs, u16 clkctrl_offs)
{
	int i = 0;

	if (!clkctrl_offs)
		return 0;

	omap_test_timeout((_clkctrl_idlest(part, inst, cdoffs, clkctrl_offs) ==
			   CLKCTRL_IDLEST_DISABLED),
			  MAX_MODULE_READY_TIME, i);

	return (i < MAX_MODULE_READY_TIME) ? 0 : -EBUSY;
}

void omap4_cminst_module_enable(u8 mode, u8 part, u16 inst, s16 cdoffs,
			    u16 clkctrl_offs)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, clkctrl_offs);
	v &= ~OMAP4430_MODULEMODE_MASK;
	v |= mode << OMAP4430_MODULEMODE_SHIFT;
	omap4_cminst_write_inst_reg(v, part, inst, clkctrl_offs);
}

void omap4_cminst_module_disable(u8 part, u16 inst, s16 cdoffs,
			     u16 clkctrl_offs)
{
	u32 v;

	v = omap4_cminst_read_inst_reg(part, inst, clkctrl_offs);
	v &= ~OMAP4430_MODULEMODE_MASK;
	omap4_cminst_write_inst_reg(v, part, inst, clkctrl_offs);
}
