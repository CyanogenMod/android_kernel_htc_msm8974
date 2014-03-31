/*
 * brddefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global BRD constants and types, shared between DSP API and Bridge driver.
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

#ifndef BRDDEFS_
#define BRDDEFS_

#define BRD_STOPPED     0x0	
#define BRD_IDLE        0x1	
#define BRD_RUNNING     0x2	
#define BRD_UNKNOWN     0x3	
#define BRD_LOADED      0x5
#define BRD_SLEEP_TRANSITION 0x6	
#define BRD_HIBERNATION 0x7	
#define BRD_RETENTION     0x8	
#define BRD_DSP_HIBERNATION     0x9	
#define BRD_ERROR		0xA	

struct brd_object;

#endif 
