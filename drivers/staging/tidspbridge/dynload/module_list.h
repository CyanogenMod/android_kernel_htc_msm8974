/*
 * dspbridge/mpu_driver/src/dynload/module_list.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef _MODULE_LIST_H_
#define _MODULE_LIST_H_

#include <linux/types.h>

#define MODULES_HEADER "_DLModules"
#define MODULES_HEADER_NO_UNDERSCORE "DLModules"

#define INIT_VERSION 1

#define VERIFICATION 0x79

struct dll_module;
struct dll_sect;

struct modules_header {

	u32 first_module;

	u16 first_module_size;

	u16 update_flag;

};

#define MODULES_HEADER_BITMAP 0x2

struct dll_sect {

	u32 sect_load_adr;

	u32 sect_run_adr;

};

struct dll_module {

	u32 next_module;

	u16 next_module_size;

	
	u16 version;

	
	u16 verification;

	
	u16 num_sects;

	u32 timestamp;

	
	struct dll_sect sects[1];
};

#define DLL_MODULE_BITMAP 0x6	

#endif 
