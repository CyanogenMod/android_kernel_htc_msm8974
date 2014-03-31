/*
 * OMAP2/3/4 Power/Reset Management (PRM) bitfield definitions
 *
 * Copyright (C) 2007-2009 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 *
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_PRM_H
#define __ARCH_ARM_MACH_OMAP2_PRM_H

#include "prcm-common.h"

#define OMAP_INTRANSITION_MASK				(1 << 20)


#define OMAP_POWERSTATEST_SHIFT				0
#define OMAP_POWERSTATEST_MASK				(0x3 << 0)

#define OMAP_POWERSTATE_SHIFT				0
#define OMAP_POWERSTATE_MASK				(0x3 << 0)


#endif
