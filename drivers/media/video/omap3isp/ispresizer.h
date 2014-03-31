/*
 * ispresizer.h
 *
 * TI OMAP3 ISP - Resizer module
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2009 Texas Instruments, Inc
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef OMAP3_ISP_RESIZER_H
#define OMAP3_ISP_RESIZER_H

#include <linux/types.h>

#define COEFF_CNT		32

struct isprsz_coef {
	u16 h_filter_coef_4tap[32];
	u16 v_filter_coef_4tap[32];
	
	u16 h_filter_coef_7tap[32];
	u16 v_filter_coef_7tap[32];
};

enum resizer_chroma_algo {
	RSZ_THE_SAME = 0,	
	RSZ_BILINEAR = 1,	
};

enum resizer_colors_type {
	RSZ_YUV422 = 0,		
	RSZ_COLOR8 = 1,		
};

struct resizer_ratio {
	u32 horz;
	u32 vert;
};

struct resizer_luma_yenh {
	u8 algo;		
	u8 gain;		
	u8 slope;		
	u8 core;		
};

enum resizer_input_entity {
	RESIZER_INPUT_NONE,
	RESIZER_INPUT_VP,	
	RESIZER_INPUT_MEMORY,
};

#define RESZ_PAD_SINK			0
#define RESZ_PAD_SOURCE			1
#define RESZ_PADS_NUM			2

struct isp_res_device {
	struct v4l2_subdev subdev;
	struct media_pad pads[RESZ_PADS_NUM];
	struct v4l2_mbus_framefmt formats[RESZ_PADS_NUM];

	enum resizer_input_entity input;
	struct isp_video video_in;
	struct isp_video video_out;

	u32 addr_base;   
	u32 crop_offset; 
	struct resizer_ratio ratio;
	int pm_state;
	unsigned int applycrop:1;
	enum isp_pipeline_stream_state state;
	wait_queue_head_t wait;
	atomic_t stopping;

	struct {
		struct v4l2_rect request;
		struct v4l2_rect active;
	} crop;
};

struct isp_device;

int omap3isp_resizer_init(struct isp_device *isp);
void omap3isp_resizer_cleanup(struct isp_device *isp);

int omap3isp_resizer_register_entities(struct isp_res_device *res,
				       struct v4l2_device *vdev);
void omap3isp_resizer_unregister_entities(struct isp_res_device *res);
void omap3isp_resizer_isr_frame_sync(struct isp_res_device *res);
void omap3isp_resizer_isr(struct isp_res_device *isp_res);

void omap3isp_resizer_max_rate(struct isp_res_device *res,
			       unsigned int *max_rate);

void omap3isp_resizer_suspend(struct isp_res_device *isp_res);

void omap3isp_resizer_resume(struct isp_res_device *isp_res);

int omap3isp_resizer_busy(struct isp_res_device *isp_res);

#endif	
