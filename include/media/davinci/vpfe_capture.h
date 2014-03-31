/*
 * Copyright (C) 2008-2009 Texas Instruments Inc
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

#ifndef _VPFE_CAPTURE_H
#define _VPFE_CAPTURE_H

#ifdef __KERNEL__

#include <media/v4l2-dev.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/videobuf-dma-contig.h>
#include <media/davinci/vpfe_types.h>

#define VPFE_CAPTURE_NUM_DECODERS        5

#define VPFE_MAJOR_RELEASE              0
#define VPFE_MINOR_RELEASE              0
#define VPFE_BUILD                      1
#define VPFE_CAPTURE_VERSION_CODE       ((VPFE_MAJOR_RELEASE << 16) | \
					(VPFE_MINOR_RELEASE << 8)  | \
					VPFE_BUILD)

#define CAPTURE_DRV_NAME		"vpfe-capture"

struct vpfe_pixel_format {
	struct v4l2_fmtdesc fmtdesc;
	
	int bpp;
};

struct vpfe_std_info {
	int active_pixels;
	int active_lines;
	
	int frame_format;
};

struct vpfe_route {
	u32 input;
	u32 output;
};

struct vpfe_subdev_info {
	
	char name[32];
	
	int grp_id;
	
	int num_inputs;
	
	struct v4l2_input *inputs;
	
	struct vpfe_route *routes;
	
	int can_route;
	
	struct vpfe_hw_if_param ccdc_if_params;
	
	struct i2c_board_info board_info;
};

struct vpfe_config {
	
	int num_subdevs;
	
	int i2c_adapter_id;
	
	struct vpfe_subdev_info *sub_devs;
	
	char *card_name;
	
	char *ccdc;
	
	struct clk *vpssclk;
	struct clk *slaveclk;
	
	void (*clr_intr)(int vdint);
};

struct vpfe_device {
	
	
	struct video_device *video_dev;
	
	struct v4l2_subdev **sd;
	
	struct vpfe_config *cfg;
	
	struct v4l2_device v4l2_dev;
	
	struct device *pdev;
	
	struct v4l2_prio_state prio;
	
	u32 usrs;
	
	u32 field_id;
	
	u8 initialized;
	
	struct vpfe_hw_if_param vpfe_if_params;
	
	struct vpfe_subdev_info *current_subdev;
	
	int current_input;
	
	struct vpfe_std_info std_info;
	
	int std_index;
	
	unsigned int ccdc_irq0;
	unsigned int ccdc_irq1;
	
	u32 numbuffers;
	
	u8 *fbuffers[VIDEO_MAX_FRAME];
	
	struct videobuf_buffer *cur_frm;
	
	struct videobuf_buffer *next_frm;
	enum v4l2_memory memory;
	
	struct v4l2_format fmt;
	struct v4l2_rect crop;
	
	struct videobuf_queue buffer_queue;
	
	struct list_head dma_queue;
	
	spinlock_t irqlock;
	
	spinlock_t dma_queue_lock;
	
	struct mutex lock;
	
	u32 io_usrs;
	
	u8 started;
	u32 field_off;
};

struct vpfe_fh {
	struct vpfe_device *vpfe_dev;
	
	u8 io_allowed;
	
	enum v4l2_priority prio;
};

struct vpfe_config_params {
	u8 min_numbuffers;
	u8 numbuffers;
	u32 min_bufsize;
	u32 device_bufsize;
};

#endif				
#define VPFE_CMD_S_CCDC_RAW_PARAMS _IOW('V', BASE_VIDIOC_PRIVATE + 1, \
					void *)
#endif				
