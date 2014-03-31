/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1999-2007 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef BOOT_VESA_H
#define BOOT_VESA_H

typedef struct {
	u16 off, seg;
} far_ptr;

struct vesa_general_info {
	u32 signature;		
	u16 version;		
	far_ptr vendor_string;	
	u32 capabilities;	
	far_ptr video_mode_ptr;	
	u16 total_memory;	

	u8 reserved[236];	
} __attribute__ ((packed));

#define VESA_MAGIC ('V' + ('E' << 8) + ('S' << 16) + ('A' << 24))

struct vesa_mode_info {
	u16 mode_attr;		
	u8 win_attr[2];		
	u16 win_grain;		
	u16 win_size;		
	u16 win_seg[2];		
	far_ptr win_scheme;	
	u16 logical_scan;	

	u16 h_res;		
	u16 v_res;		
	u8 char_width;		
	u8 char_height;		
	u8 memory_planes;	
	u8 bpp;			
	u8 banks;		
	u8 memory_layout;	
	u8 bank_size;		
	u8 image_planes;	
	u8 page_function;	

	u8 rmask;		
	u8 rpos;		
	u8 gmask;		
	u8 gpos;		
	u8 bmask;		
	u8 bpos;		
	u8 resv_mask;		
	u8 resv_pos;		
	u8 dcm_info;		

	u32 lfb_ptr;		
	u32 offscreen_ptr;	
	u16 offscreen_size;	

	u8 reserved[206];	
} __attribute__ ((packed));

#endif				
