/*
 * Copyright (C) 2009 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef VPIF_CAPTURE_H
#define VPIF_CAPTURE_H

#ifdef __KERNEL__

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/videobuf-core.h>
#include <media/videobuf-dma-contig.h>
#include <media/davinci/vpif_types.h>

#include "vpif.h"

#define VPIF_CAPTURE_VERSION		"0.0.2"

#define VPIF_VALID_FIELD(field)		(((V4L2_FIELD_ANY == field) || \
	(V4L2_FIELD_NONE == field)) || \
	(((V4L2_FIELD_INTERLACED == field) || \
	(V4L2_FIELD_SEQ_TB == field)) || \
	(V4L2_FIELD_SEQ_BT == field)))

#define VPIF_CAPTURE_MAX_DEVICES	2
#define VPIF_VIDEO_INDEX		0
#define VPIF_NUMBER_OF_OBJECTS		1

enum vpif_channel_id {
	VPIF_CHANNEL0_VIDEO = 0,
	VPIF_CHANNEL1_VIDEO,
};

struct video_obj {
	enum v4l2_field buf_field;
	
	v4l2_std_id stdid;
	u32 dv_preset;
	struct v4l2_bt_timings bt_timings;
	
	u32 input_idx;
};

struct common_obj {
	
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
	
	void (*set_addr) (unsigned long, unsigned long, unsigned long,
			  unsigned long);
	
	u32 ytop_off;
	
	u32 ybtm_off;
	
	u32 ctop_off;
	
	u32 cbtm_off;
	
	u32 width;
	
	u32 height;
};

struct channel_obj {
	
	struct video_device *video_dev;
	
	struct v4l2_prio_state prio;
	
	int usrs;
	
	u32 field_id;
	
	u8 initialized;
	
	enum vpif_channel_id channel_id;
	
	int curr_sd_index;
	
	struct vpif_subdev_info *curr_subdev_info;
	
	struct vpif_params vpifparams;
	
	struct common_obj common[VPIF_NUMBER_OF_OBJECTS];
	
	struct video_obj video;
};

struct vpif_fh {
	
	struct channel_obj *channel;
	
	u8 io_allowed[VPIF_NUMBER_OF_OBJECTS];
	
	enum v4l2_priority prio;
	
	u8 initialized;
};

struct vpif_device {
	struct v4l2_device v4l2_dev;
	struct channel_obj *dev[VPIF_CAPTURE_NUM_CHANNELS];
	struct v4l2_subdev **sd;
};

struct vpif_config_params {
	u8 min_numbuffers;
	u8 numbuffers[VPIF_CAPTURE_NUM_CHANNELS];
	s8 device_type;
	u32 min_bufsize[VPIF_CAPTURE_NUM_CHANNELS];
	u32 channel_bufsize[VPIF_CAPTURE_NUM_CHANNELS];
	u8 default_device[VPIF_CAPTURE_NUM_CHANNELS];
	u8 max_device_type;
};
struct vpif_service_line {
	u16 service_id;
	u16 service_line[2];
};
#endif				
#endif				
