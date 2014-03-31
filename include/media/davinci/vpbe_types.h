/*
 * Copyright (C) 2010 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _VPBE_TYPES_H
#define _VPBE_TYPES_H

enum vpbe_version {
	VPBE_VERSION_1 = 1,
	VPBE_VERSION_2,
	VPBE_VERSION_3,
};

enum vpbe_enc_timings_type {
	VPBE_ENC_STD = 0x1,
	VPBE_ENC_DV_PRESET = 0x2,
	VPBE_ENC_CUSTOM_TIMINGS = 0x4,
	
	VPBE_ENC_TIMINGS_INVALID = 0x8,
};

union vpbe_timings {
	v4l2_std_id std_id;
	unsigned int dv_preset;
};

struct vpbe_enc_mode_info {
	unsigned char *name;
	enum vpbe_enc_timings_type timings_type;
	union vpbe_timings timings;
	unsigned int interlaced;
	unsigned int xres;
	unsigned int yres;
	struct v4l2_fract aspect;
	struct v4l2_fract fps;
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
	unsigned int flags;
};

#endif
