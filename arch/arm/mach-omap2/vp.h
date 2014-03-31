/*
 * OMAP3/4 Voltage Processor (VP) structure and macro definitions
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
#ifndef __ARCH_ARM_MACH_OMAP2_VP_H
#define __ARCH_ARM_MACH_OMAP2_VP_H

#include <linux/kernel.h>

struct voltagedomain;

#define OMAP3_VP_VDD_MPU_ID 0
#define OMAP3_VP_VDD_CORE_ID 1
#define OMAP4_VP_VDD_CORE_ID 0
#define OMAP4_VP_VDD_IVA_ID 1
#define OMAP4_VP_VDD_MPU_ID 2

#define VP_IDLE_TIMEOUT		200
#define VP_TRANXDONE_TIMEOUT	300

struct omap_vp_ops {
	u32 (*check_txdone)(u8 vp_id);
	void (*clear_txdone)(u8 vp_id);
};

struct omap_vp_common {
	u32 vpconfig_erroroffset_mask;
	u32 vpconfig_errorgain_mask;
	u32 vpconfig_initvoltage_mask;
	u8 vpconfig_timeouten;
	u8 vpconfig_initvdd;
	u8 vpconfig_forceupdate;
	u8 vpconfig_vpenable;
	u8 vstepmin_stepmin_shift;
	u8 vstepmin_smpswaittimemin_shift;
	u8 vstepmax_stepmax_shift;
	u8 vstepmax_smpswaittimemax_shift;
	u8 vlimitto_vddmin_shift;
	u8 vlimitto_vddmax_shift;
	u8 vlimitto_timeout_shift;
	u8 vpvoltage_mask;

	const struct omap_vp_ops *ops;
};

struct omap_vp_instance {
	const struct omap_vp_common *common;
	u8 vpconfig;
	u8 vstepmin;
	u8 vstepmax;
	u8 vlimitto;
	u8 vstatus;
	u8 voltage;
	u8 id;
	bool enabled;
};

extern struct omap_vp_instance omap3_vp_mpu;
extern struct omap_vp_instance omap3_vp_core;

extern struct omap_vp_instance omap4_vp_mpu;
extern struct omap_vp_instance omap4_vp_iva;
extern struct omap_vp_instance omap4_vp_core;

void omap_vp_init(struct voltagedomain *voltdm);
void omap_vp_enable(struct voltagedomain *voltdm);
void omap_vp_disable(struct voltagedomain *voltdm);
int omap_vp_forceupdate_scale(struct voltagedomain *voltdm,
			      unsigned long target_volt);
int omap_vp_update_errorgain(struct voltagedomain *voltdm,
			     unsigned long target_volt);

#endif
