/*
 * cfgdefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global CFG constants and types, shared between DSP API and Bridge driver.
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

#ifndef CFGDEFS_
#define CFGDEFS_

#define CFG_MAXMEMREGISTERS     9

#define CFG_IRQSHARED           0x01	

struct cfg_devnode;

struct cfg_hostres {
	u32 num_mem_windows;	
	
	u32 mem_base[CFG_MAXMEMREGISTERS];	
	u32 mem_length[CFG_MAXMEMREGISTERS];	
	u32 mem_phys[CFG_MAXMEMREGISTERS];	
	u8 birq_registers;	
	u8 birq_attrib;		
	u32 offset_for_monitor;	
	u32 chnl_offset;
	u32 chnl_buf_size;
	u32 num_chnls;
	void __iomem *per_base;
	u32 per_pm_base;
	u32 core_pm_base;
	void __iomem *dmmu_base;
};

#endif 
