/*
 * hw_defs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global HW definitions
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _HW_DEFS_H
#define _HW_DEFS_H

#define HW_PAGE_SIZE4KB   0x1000
#define HW_PAGE_SIZE64KB  0x10000
#define HW_PAGE_SIZE1MB   0x100000
#define HW_PAGE_SIZE16MB  0x1000000

typedef long hw_status;

#define HW_CLEAR	0
#define HW_SET		1

enum hw_endianism_t {
	HW_LITTLE_ENDIAN,
	HW_BIG_ENDIAN
};

enum hw_element_size_t {
	HW_ELEM_SIZE8BIT,
	HW_ELEM_SIZE16BIT,
	HW_ELEM_SIZE32BIT,
	HW_ELEM_SIZE64BIT
};

enum hw_idle_mode_t {
	HW_FORCE_IDLE,
	HW_NO_IDLE,
	HW_SMART_IDLE
};

#endif 
