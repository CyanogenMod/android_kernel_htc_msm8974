/*
 * Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  Freescale DIU Frame Buffer device driver
 *
 *  Authors: Hongjun Chen <hong-jun.chen@freescale.com>
 *           Paul Widmer <paul.widmer@freescale.com>
 *           Srikanth Srinivasan <srikanth.srinivasan@freescale.com>
 *           York Sun <yorksun@freescale.com>
 *
 *   Based on imxfb.c Copyright (C) 2004 S.Hauer, Pengutronix
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef __FSL_DIU_FB_H__
#define __FSL_DIU_FB_H__

#include <linux/types.h>

struct mfb_chroma_key {
	int enable;
	__u8  red_max;
	__u8  green_max;
	__u8  blue_max;
	__u8  red_min;
	__u8  green_min;
	__u8  blue_min;
};

struct aoi_display_offset {
	__s32 x_aoi_d;
	__s32 y_aoi_d;
};

#define MFB_SET_CHROMA_KEY	_IOW('M', 1, struct mfb_chroma_key)
#define MFB_SET_BRIGHTNESS	_IOW('M', 3, __u8)
#define MFB_SET_ALPHA		_IOW('M', 0, __u8)
#define MFB_GET_ALPHA		_IOR('M', 0, __u8)
#define MFB_SET_AOID		_IOW('M', 4, struct aoi_display_offset)
#define MFB_GET_AOID		_IOR('M', 4, struct aoi_display_offset)
#define MFB_SET_PIXFMT		_IOW('M', 8, __u32)
#define MFB_GET_PIXFMT		_IOR('M', 8, __u32)

#define MFB_SET_PIXFMT_OLD	0x80014d08
#define MFB_GET_PIXFMT_OLD	0x40014d08

#ifdef __KERNEL__

struct diu_ad {
	

	__be32 pix_fmt; 

	
	__le32 addr;

	
	__le32 src_size_g_alpha;

	
	__le32 aoi_size;

	
	__le32 offset_xyi;

	
	__le32 offset_xyd;


	
	__u8 ckmax_r;
	__u8 ckmax_g;
	__u8 ckmax_b;
	__u8 res9;

	
	__u8 ckmin_r;
	__u8 ckmin_g;
	__u8 ckmin_b;
	__u8 res10;

	
	__le32 next_ad;

	
	__u32 paddr;
} __attribute__ ((packed));

struct diu {
	__be32 desc[3];
	__be32 gamma;
	__be32 pallete;
	__be32 cursor;
	__be32 curs_pos;
	__be32 diu_mode;
	__be32 bgnd;
	__be32 bgnd_wb;
	__be32 disp_size;
	__be32 wb_size;
	__be32 wb_mem_addr;
	__be32 hsyn_para;
	__be32 vsyn_para;
	__be32 syn_pol;
	__be32 thresholds;
	__be32 int_status;
	__be32 int_mask;
	__be32 colorbar[8];
	__be32 filling;
	__be32 plut;
} __attribute__ ((packed));

#define MFB_MODE0	0	
#define MFB_MODE1	1	

#endif 
#endif 
