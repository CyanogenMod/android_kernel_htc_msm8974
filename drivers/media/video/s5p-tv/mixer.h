/*
 * Samsung TV Mixer driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#ifndef SAMSUNG_MIXER_H
#define SAMSUNG_MIXER_H

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MIXER_DEBUG
	#define DEBUG
#endif

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "regs-mixer.h"

#define MXR_MAX_OUTPUTS 2
#define MXR_MAX_LAYERS 3
#define MXR_DRIVER_NAME "s5p-mixer"
#define MXR_MAX_PLANES	2

#define MXR_ENABLE 1
#define MXR_DISABLE 0

struct mxr_block {
	
	unsigned int width;
	
	unsigned int height;
	
	unsigned int size;
};

struct mxr_format {
	
	const char *name;
	
	u32 fourcc;
	
	enum v4l2_colorspace colorspace;
	
	int num_planes;
	
	struct mxr_block plane[MXR_MAX_PLANES];
	
	int num_subframes;
	
	int plane2subframe[MXR_MAX_PLANES];
	
	unsigned long cookie;
};

struct mxr_crop {
	
	unsigned int full_width;
	
	unsigned int full_height;
	
	unsigned int x_offset;
	
	unsigned int y_offset;
	
	unsigned int width;
	
	unsigned int height;
	
	unsigned int field;
};

enum mxr_geometry_stage {
	MXR_GEOMETRY_SINK,
	MXR_GEOMETRY_COMPOSE,
	MXR_GEOMETRY_CROP,
	MXR_GEOMETRY_SOURCE,
};

#define MXR_NO_OFFSET	0x80000000

struct mxr_geometry {
	
	struct mxr_crop src;
	
	struct mxr_crop dst;
	
	unsigned int x_ratio;
	
	unsigned int y_ratio;
};

struct mxr_buffer {
	
	struct vb2_buffer	vb;
	
	struct list_head	list;
};


enum mxr_layer_state {
	
	MXR_LAYER_IDLE = 0,
	
	MXR_LAYER_STREAMING,
	
	MXR_LAYER_STREAMING_FINISH,
};

struct mxr_device;
struct mxr_layer;

struct mxr_layer_ops {
	
	
	void (*release)(struct mxr_layer *);
	
	void (*buffer_set)(struct mxr_layer *, struct mxr_buffer *);
	
	void (*format_set)(struct mxr_layer *);
	
	void (*stream_set)(struct mxr_layer *, int);
	
	void (*fix_geometry)(struct mxr_layer *,
		enum mxr_geometry_stage, unsigned long);
};

struct mxr_layer {
	
	struct mxr_device *mdev;
	
	int idx;
	
	struct mxr_layer_ops ops;
	
	const struct mxr_format **fmt_array;
	
	unsigned long fmt_array_size;

	
	spinlock_t enq_slock;
	
	struct list_head enq_list;
	
	struct mxr_buffer *update_buf;
	
	struct mxr_buffer *shadow_buf;
	
	enum mxr_layer_state state;

	
	struct mutex mutex;
	
	struct video_device vfd;
	
	struct vb2_queue vb_queue;
	
	const struct mxr_format *fmt;
	
	struct mxr_geometry geo;
};

struct mxr_output {
	
	char name[32];
	
	struct v4l2_subdev *sd;
	
	int cookie;
};

struct mxr_output_conf {
	
	char *output_name;
	
	char *module_name;
	
	int cookie;
};

struct clk;
struct regulator;

struct mxr_resources {
	
	int irq;
	
	void __iomem *mxr_regs;
	
	void __iomem *vp_regs;
	
	struct clk *mixer;
	struct clk *vp;
	struct clk *sclk_mixer;
	struct clk *sclk_hdmi;
	struct clk *sclk_dac;
};

enum mxr_devide_flags {
	MXR_EVENT_VSYNC = 0,
};

struct mxr_device {
	
	struct device *dev;
	
	struct mxr_layer *layer[MXR_MAX_LAYERS];
	
	struct mxr_output *output[MXR_MAX_OUTPUTS];
	
	int output_cnt;

	

	
	struct v4l2_device v4l2_dev;
	
	void *alloc_ctx;
	
	wait_queue_head_t event_queue;
	
	unsigned long event_flags;

	
	spinlock_t reg_slock;

	
	struct mutex mutex;
	
	int n_output;
	
	int n_streamer;
	
	int current_output;
	
	struct mxr_resources res;
};

static inline struct mxr_device *to_mdev(struct device *dev)
{
	struct v4l2_device *vdev = dev_get_drvdata(dev);
	return container_of(vdev, struct mxr_device, v4l2_dev);
}

static inline struct mxr_output *to_output(struct mxr_device *mdev)
{
	return mdev->output[mdev->current_output];
}

static inline struct v4l2_subdev *to_outsd(struct mxr_device *mdev)
{
	struct mxr_output *out = to_output(mdev);
	return out ? out->sd : NULL;
}

struct mxr_platform_data;

int __devinit mxr_acquire_video(struct mxr_device *mdev,
	struct mxr_output_conf *output_cont, int output_count);

void __devexit mxr_release_video(struct mxr_device *mdev);

struct mxr_layer *mxr_graph_layer_create(struct mxr_device *mdev, int idx);
struct mxr_layer *mxr_vp_layer_create(struct mxr_device *mdev, int idx);
struct mxr_layer *mxr_base_layer_create(struct mxr_device *mdev,
	int idx, char *name, struct mxr_layer_ops *ops);

void mxr_base_layer_release(struct mxr_layer *layer);
void mxr_layer_release(struct mxr_layer *layer);

int mxr_base_layer_register(struct mxr_layer *layer);
void mxr_base_layer_unregister(struct mxr_layer *layer);

unsigned long mxr_get_plane_size(const struct mxr_block *blk,
	unsigned int width, unsigned int height);

int __must_check mxr_power_get(struct mxr_device *mdev);
void mxr_power_put(struct mxr_device *mdev);
void mxr_output_get(struct mxr_device *mdev);
void mxr_output_put(struct mxr_device *mdev);
void mxr_streamer_get(struct mxr_device *mdev);
void mxr_streamer_put(struct mxr_device *mdev);
void mxr_get_mbus_fmt(struct mxr_device *mdev,
	struct v4l2_mbus_framefmt *mbus_fmt);


#define mxr_err(mdev, fmt, ...)  dev_err(mdev->dev, fmt, ##__VA_ARGS__)
#define mxr_warn(mdev, fmt, ...) dev_warn(mdev->dev, fmt, ##__VA_ARGS__)
#define mxr_info(mdev, fmt, ...) dev_info(mdev->dev, fmt, ##__VA_ARGS__)

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MIXER_DEBUG
	#define mxr_dbg(mdev, fmt, ...)  dev_dbg(mdev->dev, fmt, ##__VA_ARGS__)
#else
	#define mxr_dbg(mdev, fmt, ...)  do { (void) mdev; } while (0)
#endif


void mxr_vsync_set_update(struct mxr_device *mdev, int en);
void mxr_reg_reset(struct mxr_device *mdev);
irqreturn_t mxr_irq_handler(int irq, void *dev_data);
void mxr_reg_s_output(struct mxr_device *mdev, int cookie);
void mxr_reg_streamon(struct mxr_device *mdev);
void mxr_reg_streamoff(struct mxr_device *mdev);
int mxr_reg_wait4vsync(struct mxr_device *mdev);
void mxr_reg_set_mbus_fmt(struct mxr_device *mdev,
	struct v4l2_mbus_framefmt *fmt);
void mxr_reg_graph_layer_stream(struct mxr_device *mdev, int idx, int en);
void mxr_reg_graph_buffer(struct mxr_device *mdev, int idx, dma_addr_t addr);
void mxr_reg_graph_format(struct mxr_device *mdev, int idx,
	const struct mxr_format *fmt, const struct mxr_geometry *geo);

void mxr_reg_vp_layer_stream(struct mxr_device *mdev, int en);
void mxr_reg_vp_buffer(struct mxr_device *mdev,
	dma_addr_t luma_addr[2], dma_addr_t chroma_addr[2]);
void mxr_reg_vp_format(struct mxr_device *mdev,
	const struct mxr_format *fmt, const struct mxr_geometry *geo);
void mxr_reg_dump(struct mxr_device *mdev);

#endif 

