/*
 * OMAP4 PRCM definitions
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 *
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains macros and functions that are common to all of
 * the PRM/CM/PRCM blocks on the OMAP4 devices: PRM, CM1, CM2,
 * PRCM_MPU, SCRM
 */

#ifndef __ARCH_ARM_MACH_OMAP2_PRCM44XX_H
#define __ARCH_ARM_MACH_OMAP2_PRCM44XX_H

#define OMAP4430_INVALID_PRCM_PARTITION		0
#define OMAP4430_PRM_PARTITION			1
#define OMAP4430_CM1_PARTITION			2
#define OMAP4430_CM2_PARTITION			3
#define OMAP4430_SCRM_PARTITION			4
#define OMAP4430_PRCM_MPU_PARTITION		5

#define OMAP4_MAX_PRCM_PARTITIONS		6


#endif
