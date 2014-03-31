/* linux/drivers/media/video/s5p-jpeg/jpeg-core.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JPEG_CORE_H_
#define JPEG_CORE_H_

#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>

#define S5P_JPEG_M2M_NAME		"s5p-jpeg"

#define S5P_JPEG_COMPR_QUAL_BEST	0
#define S5P_JPEG_COMPR_QUAL_WORST	3

#define S5P_JPEG_COEF11			0x4d
#define S5P_JPEG_COEF12			0x97
#define S5P_JPEG_COEF13			0x1e
#define S5P_JPEG_COEF21			0x2c
#define S5P_JPEG_COEF22			0x57
#define S5P_JPEG_COEF23			0x83
#define S5P_JPEG_COEF31			0x83
#define S5P_JPEG_COEF32			0x6e
#define S5P_JPEG_COEF33			0x13

#define TEM				0x01
#define SOF0				0xc0
#define RST				0xd0
#define SOI				0xd8
#define EOI				0xd9
#define DHP				0xde

#define MEM2MEM_CAPTURE			(1 << 0)
#define MEM2MEM_OUTPUT			(1 << 1)

struct s5p_jpeg {
	struct mutex		lock;
	struct spinlock		slock;

	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_encoder;
	struct video_device	*vfd_decoder;
	struct v4l2_m2m_dev	*m2m_dev;

	struct resource		*ioarea;
	void __iomem		*regs;
	unsigned int		irq;
	struct clk		*clk;
	struct device		*dev;
	void			*alloc_ctx;
};

struct s5p_jpeg_fmt {
	char	*name;
	u32	fourcc;
	int	depth;
	int	colplanes;
	int	h_align;
	int	v_align;
	u32	types;
};

struct s5p_jpeg_q_data {
	struct s5p_jpeg_fmt	*fmt;
	u32			w;
	u32			h;
	u32			size;
};

struct s5p_jpeg_ctx {
	struct s5p_jpeg		*jpeg;
	unsigned int		mode;
	unsigned short		compr_quality;
	unsigned short		restart_interval;
	unsigned short		subsampling;
	struct v4l2_m2m_ctx	*m2m_ctx;
	struct s5p_jpeg_q_data	out_q;
	struct s5p_jpeg_q_data	cap_q;
	struct v4l2_fh		fh;
	bool			hdr_parsed;
	struct v4l2_ctrl_handler ctrl_handler;
};

struct s5p_jpeg_buffer {
	unsigned long size;
	unsigned long curr;
	unsigned long data;
};

#endif 
