/*
 * rmm.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This memory manager provides general heap management and arbitrary
 * alignment for any number of memory segments, and management of overlay
 * memory.
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

#ifndef RMM_
#define RMM_

struct rmm_addr {
	u32 addr;
	s32 segid;
};

struct rmm_segment {
	u32 base;		
	u32 length;		
	s32 space;		
	u32 number;		
};

struct rmm_target_obj;

extern int rmm_alloc(struct rmm_target_obj *target, u32 segid, u32 size,
			u32 align, u32 *dsp_address, bool reserve);

extern int rmm_create(struct rmm_target_obj **target_obj,
			     struct rmm_segment seg_tab[], u32 num_segs);

extern void rmm_delete(struct rmm_target_obj *target);

extern bool rmm_free(struct rmm_target_obj *target, u32 segid, u32 dsp_addr,
		     u32 size, bool reserved);

extern bool rmm_stat(struct rmm_target_obj *target, enum dsp_memtype segid,
		     struct dsp_memstat *mem_stat_buf);

#endif 
