/*
 * cmmdefs.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Global MEM constants and types.
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

#ifndef CMMDEFS_
#define CMMDEFS_


struct cmm_mgrattrs {
	
	u32 min_block_size;
};

struct cmm_attrs {
	u32 seg_id;		
	u32 alignment;		
};

#define CMM_SUBFROMDSPPA	-1
#define CMM_ADDTODSPPA		1

#define CMM_ALLSEGMENTS         0xFFFFFF	
#define CMM_MAXGPPSEGS          1	


struct cmm_seginfo {
	u32 seg_base_pa;	
	
	u32 total_seg_size;
	u32 gpp_base_pa;	
	u32 gpp_size;	
	u32 dsp_base_va;	
	u32 dsp_size;		
	
	u32 in_use_cnt;
	u32 seg_base_va;	

};

struct cmm_info {
	
	u32 num_gppsm_segs;
	
	u32 total_in_use_cnt;
	
	u32 min_block_size;
	
	struct cmm_seginfo seg_info[CMM_MAXGPPSEGS];
};

struct cmm_xlatorattrs {
	u32 seg_id;		
	u32 dsp_bufs;		
	u32 dsp_buf_size;	
	
	void *vm_base;
	
	u32 vm_size;
};

enum cmm_xlatetype {
	CMM_VA2PA = 0,		
	CMM_PA2VA = 1,		
	CMM_VA2DSPPA = 2,	
	CMM_PA2DSPPA = 3,	
	CMM_DSPPA2PA = 4,	
};

struct cmm_object;
struct cmm_xlatorobject;

#endif 
