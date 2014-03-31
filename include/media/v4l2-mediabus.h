/*
 * Media Bus API header
 *
 * Copyright (C) 2009, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef V4L2_MEDIABUS_H
#define V4L2_MEDIABUS_H

#include <linux/v4l2-mediabus.h>

#define V4L2_MBUS_MASTER			(1 << 0)
#define V4L2_MBUS_SLAVE				(1 << 1)
#define V4L2_MBUS_HSYNC_ACTIVE_HIGH		(1 << 2)
#define V4L2_MBUS_HSYNC_ACTIVE_LOW		(1 << 3)
#define V4L2_MBUS_VSYNC_ACTIVE_HIGH		(1 << 4)
#define V4L2_MBUS_VSYNC_ACTIVE_LOW		(1 << 5)
#define V4L2_MBUS_PCLK_SAMPLE_RISING		(1 << 6)
#define V4L2_MBUS_PCLK_SAMPLE_FALLING		(1 << 7)
#define V4L2_MBUS_DATA_ACTIVE_HIGH		(1 << 8)
#define V4L2_MBUS_DATA_ACTIVE_LOW		(1 << 9)
#define V4L2_MBUS_FIELD_EVEN_HIGH		(1 << 10)
#define V4L2_MBUS_FIELD_EVEN_LOW		(1 << 11)

#define V4L2_MBUS_CSI2_1_LANE			(1 << 0)
#define V4L2_MBUS_CSI2_2_LANE			(1 << 1)
#define V4L2_MBUS_CSI2_3_LANE			(1 << 2)
#define V4L2_MBUS_CSI2_4_LANE			(1 << 3)
#define V4L2_MBUS_CSI2_CHANNEL_0		(1 << 4)
#define V4L2_MBUS_CSI2_CHANNEL_1		(1 << 5)
#define V4L2_MBUS_CSI2_CHANNEL_2		(1 << 6)
#define V4L2_MBUS_CSI2_CHANNEL_3		(1 << 7)
#define V4L2_MBUS_CSI2_CONTINUOUS_CLOCK		(1 << 8)
#define V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK	(1 << 9)

#define V4L2_MBUS_CSI2_LANES		(V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_2_LANE | \
					 V4L2_MBUS_CSI2_3_LANE | V4L2_MBUS_CSI2_4_LANE)
#define V4L2_MBUS_CSI2_CHANNELS		(V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1 | \
					 V4L2_MBUS_CSI2_CHANNEL_2 | V4L2_MBUS_CSI2_CHANNEL_3)

enum v4l2_mbus_type {
	V4L2_MBUS_PARALLEL,
	V4L2_MBUS_BT656,
	V4L2_MBUS_CSI2,
};

struct v4l2_mbus_config {
	enum v4l2_mbus_type type;
	unsigned int flags;
};

static inline void v4l2_fill_pix_format(struct v4l2_pix_format *pix_fmt,
				const struct v4l2_mbus_framefmt *mbus_fmt)
{
	pix_fmt->width = mbus_fmt->width;
	pix_fmt->height = mbus_fmt->height;
	pix_fmt->field = mbus_fmt->field;
	pix_fmt->colorspace = mbus_fmt->colorspace;
}

static inline void v4l2_fill_mbus_format(struct v4l2_mbus_framefmt *mbus_fmt,
			   const struct v4l2_pix_format *pix_fmt,
			   enum v4l2_mbus_pixelcode code)
{
	mbus_fmt->width = pix_fmt->width;
	mbus_fmt->height = pix_fmt->height;
	mbus_fmt->field = pix_fmt->field;
	mbus_fmt->colorspace = pix_fmt->colorspace;
	mbus_fmt->code = code;
}

#endif
