/*
 * OMAP3/4 Voltage Controller (VC) structure and macro definitions
 *
 * Copyright (C) 2007, 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 * Thara Gopinath <thara@ti.com>
 *
 * Copyright (C) 2008, 2011 Nokia Corporation
 * Kalle Jokiniemi
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_VC_H
#define __ARCH_ARM_MACH_OMAP2_VC_H

#include <linux/kernel.h>

struct voltagedomain;

struct omap_vc_common {
	u32 cmd_on_mask;
	u32 valid;
	u8 bypass_val_reg;
	u8 data_shift;
	u8 slaveaddr_shift;
	u8 regaddr_shift;
	u8 cmd_on_shift;
	u8 cmd_onlp_shift;
	u8 cmd_ret_shift;
	u8 cmd_off_shift;
	u8 i2c_cfg_reg;
	u8 i2c_cfg_hsen_mask;
	u8 i2c_mcode_mask;
};

#define OMAP_VC_CHANNEL_DEFAULT BIT(0)
#define OMAP_VC_CHANNEL_CFG_MUTANT BIT(1)

struct omap_vc_channel {
	
	u16 i2c_slave_addr;
	u16 volt_reg_addr;
	u16 cmd_reg_addr;
	u16 setup_time;
	u8 cfg_channel;
	bool i2c_high_speed;

	
	const struct omap_vc_common *common;
	u32 smps_sa_mask;
	u32 smps_volra_mask;
	u32 smps_cmdra_mask;
	u8 cmdval_reg;
	u8 smps_sa_reg;
	u8 smps_volra_reg;
	u8 smps_cmdra_reg;
	u8 cfg_channel_reg;
	u8 cfg_channel_sa_shift;
	u8 flags;
};

extern struct omap_vc_channel omap3_vc_mpu;
extern struct omap_vc_channel omap3_vc_core;

extern struct omap_vc_channel omap4_vc_mpu;
extern struct omap_vc_channel omap4_vc_iva;
extern struct omap_vc_channel omap4_vc_core;

void omap_vc_init_channel(struct voltagedomain *voltdm);
int omap_vc_pre_scale(struct voltagedomain *voltdm,
		      unsigned long target_volt,
		      u8 *target_vsel, u8 *current_vsel);
void omap_vc_post_scale(struct voltagedomain *voltdm,
			unsigned long target_volt,
			u8 target_vsel, u8 current_vsel);
int omap_vc_bypass_scale(struct voltagedomain *voltdm,
			 unsigned long target_volt);

#endif

