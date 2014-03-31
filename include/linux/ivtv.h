/*
    Public ivtv API header
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>
    Copyright (C) 2004-2007  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_IVTV_H__
#define __LINUX_IVTV_H__

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/videodev2.h>


struct ivtv_dma_frame {
	enum v4l2_buf_type type; 
	__u32 pixelformat;	 
	void __user *y_source;   
	void __user *uv_source;  
	struct v4l2_rect src;
	struct v4l2_rect dst;
	__u32 src_width;
	__u32 src_height;
};

#define IVTV_IOC_DMA_FRAME		_IOW ('V', BASE_VIDIOC_PRIVATE+0, struct ivtv_dma_frame)

#define IVTV_IOC_PASSTHROUGH_MODE	_IOW ('V', BASE_VIDIOC_PRIVATE+1, int)

#define IVTV_SLICED_TYPE_TELETEXT_B     V4L2_MPEG_VBI_IVTV_TELETEXT_B
#define IVTV_SLICED_TYPE_CAPTION_525    V4L2_MPEG_VBI_IVTV_CAPTION_525
#define IVTV_SLICED_TYPE_WSS_625        V4L2_MPEG_VBI_IVTV_WSS_625
#define IVTV_SLICED_TYPE_VPS            V4L2_MPEG_VBI_IVTV_VPS

#endif 
