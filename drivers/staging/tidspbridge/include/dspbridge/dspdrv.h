/*
 * dspdrv.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This is the Stream Interface for the DSp API.
 * All Device operations are performed via DeviceIOControl.
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

#if !defined _DSPDRV_H_
#define _DSPDRV_H_

extern bool dsp_deinit(u32 device_context);

extern u32 dsp_init(u32 *init_status);

#endif
