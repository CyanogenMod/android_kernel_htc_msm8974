/*
 * pwr.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef PWR_
#define PWR_

#include <dspbridge/dbdefs.h>
#include <dspbridge/mbx_sh.h>

#define PWR_DEEPSLEEP           MBX_PM_DSPIDLE
#define PWR_EMERGENCYDEEPSLEEP  MBX_PM_EMERGENCYSLEEP
#define PWR_WAKEUP              MBX_PM_DSPWAKEUP


extern int pwr_sleep_dsp(const u32 sleep_code, const u32 timeout);

extern int pwr_wake_dsp(const u32 timeout);

extern int pwr_pm_pre_scale(u16 voltage_domain, u32 level);

extern int pwr_pm_post_scale(u16 voltage_domain, u32 level);

#endif 
