/*
 * DM646x display header file
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DAVINCIHD_DISPLAY_H
#define DAVINCIHD_DISPLAY_H

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/videobuf-core.h>
#include <media/videobuf-dma-contig.h>
#include <media/davinci/vpif_types.h>

#include "vpif.h"

#define VPIF_DISPLAY_VERSION	"0.0.2"

#define VPIF_VALID_FIELD(field) \
	(((V4L2_FIELD_ANY == field) || (V4L2_FIELD_NONE == field)) || \
	(((V4L2_FIELD_INTERLACED == field) || (V4L2_FIELD_SEQ_TB == field)) || \
	(V4L2_FIELD_SEQ_BT == field)))

#define VPIF_DISPLAY_MAX_DEVICES	(2)
#define VPIF_SLICED_BUF_SIZE		(256)
#define VPIF_SLICED_MAX_SERVICES	(3)
#define VPIF_VIDEO_INDEX		(0)
#define VPIF_VBI_INDEX			(1)
#define VPIF_HBI_INDEX			(2)

#define VPIF_NUMOBJECTS	(1)

#define ISALIGNED(a)    (0 == ((a) & 7))

enum vpif_channel_id {
	VPIF_CHANNEL2_VIDEO = 0,	
	VPIF_CHANNEL3_VIDEO,		
};


struct video_obj {
	enum v4l2_field buf_field;
	u32 latest_only;		
	v4l2_std_id stdid;		
	u32 dv_preset;
	struct v4l2_bt_timings bt_timings;
	u32 output_id;			
};

struct vbi_obj {
	int num_services;
	struct vpif_vbi_params vbiparams;	
};

struct common_obj {
	
	u8 *fbuffers[VIDEO_MAX_FRAME];		
	u32 numbuffers;				
	struct videobuf_buffer *cur_frm;	
	struct videobuf_buffer *next_frm;	
	enum v4l2_memory memory;		
	struct v4l2_format fmt;			
	struct videobuf_queue buffer_queue;	
	struct list_head dma_queue;		
	spinlock_t irqlock;			

	
	struct mutex lock;			
	u32 io_usrs;				
	u8 started;				
	u32 ytop_off;				
	u32 ybtm_off;				
	u32 ctop_off;				
	u32 cbtm_off;				
	
	void (*set_addr) (unsigned long, unsigned long,
				unsigned long, unsigned long);
	u32 height;
	u32 width;
};

struct channel_obj {
	
	struct video_device *video_dev;	
	struct v4l2_prio_state prio;	
	atomic_t usrs;			
	u32 field_id;			
	u8 initialized;			

	enum vpif_channel_id channel_id;
	struct vpif_params vpifparams;
	struct common_obj common[VPIF_NUMOBJECTS];
	struct video_obj video;
	struct vbi_obj vbi;
};

struct vpif_fh {
	struct channel_obj *channel;	
	u8 io_allowed[VPIF_NUMOBJECTS];	
	enum v4l2_priority prio;	
	u8 initialized;			
};

struct vpif_device {
	struct v4l2_device v4l2_dev;
	struct channel_obj *dev[VPIF_DISPLAY_NUM_CHANNELS];
	struct v4l2_subdev **sd;

};

struct vpif_config_params {
	u32 min_bufsize[VPIF_DISPLAY_NUM_CHANNELS];
	u32 channel_bufsize[VPIF_DISPLAY_NUM_CHANNELS];
	u8 numbuffers[VPIF_DISPLAY_NUM_CHANNELS];
	u8 min_numbuffers;
};

struct vpif_service_line {
	u16 service_id;
	u16 service_line[2];
	u16 enc_service_id;
	u8 bytestowrite;
};

#endif				
