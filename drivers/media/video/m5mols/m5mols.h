/*
 * Header for M-5MOLS 8M Pixel camera sensor with ISP
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * Author: HeungJun Kim <riverful.kim@samsung.com>
 *
 * Copyright (C) 2009 Samsung Electronics Co., Ltd.
 * Author: Dongsoo Nathaniel Kim <dongsoo45.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef M5MOLS_H
#define M5MOLS_H

#include <media/v4l2-subdev.h>
#include "m5mols_reg.h"

extern int m5mols_debug;

#define to_m5mols(__sd)	container_of(__sd, struct m5mols_info, sd)

#define to_sd(__ctrl) \
	(&container_of(__ctrl->handler, struct m5mols_info, handle)->sd)

enum m5mols_restype {
	M5MOLS_RESTYPE_MONITOR,
	M5MOLS_RESTYPE_CAPTURE,
	M5MOLS_RESTYPE_MAX,
};

struct m5mols_resolution {
	u8 reg;
	enum m5mols_restype type;
	u16 width;
	u16 height;
};

/**
 * struct m5mols_exif - structure for the EXIF information of M-5MOLS
 * @exposure_time: exposure time register value
 * @shutter_speed: speed of the shutter register value
 * @aperture: aperture register value
 * @exposure_bias: it calls also EV bias
 * @iso_speed: ISO register value
 * @flash: status register value of the flash
 * @sdr: status register value of the Subject Distance Range
 * @qval: not written exact meaning in document
 */
struct m5mols_exif {
	u32 exposure_time;
	u32 shutter_speed;
	u32 aperture;
	u32 brightness;
	u32 exposure_bias;
	u16 iso_speed;
	u16 flash;
	u16 sdr;
	u16 qval;
};

struct m5mols_capture {
	struct m5mols_exif exif;
	u32 main;
	u32 thumb;
	u32 total;
};

struct m5mols_scenemode {
	u8 metering;
	u8 ev_bias;
	u8 wb_mode;
	u8 wb_preset;
	u8 chroma_en;
	u8 chroma_lvl;
	u8 edge_en;
	u8 edge_lvl;
	u8 af_range;
	u8 fd_mode;
	u8 mcc;
	u8 light;
	u8 flash;
	u8 tone;
	u8 iso;
	u8 capt_mode;
	u8 wdr;
};

#define VERSION_STRING_SIZE	22
struct m5mols_version {
	u8	customer;
	u8	project;
	u16	fw;
	u16	hw;
	u16	param;
	u16	awb;
	u8	str[VERSION_STRING_SIZE];
	u8	af;
};

struct m5mols_info {
	const struct m5mols_platform_data *pdata;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt ffmt[M5MOLS_RESTYPE_MAX];
	int res_type;

	wait_queue_head_t irq_waitq;
	atomic_t irq_done;

	struct v4l2_ctrl_handler handle;

	
	struct v4l2_ctrl *autoexposure;
	struct v4l2_ctrl *exposure;

	struct v4l2_ctrl *autowb;
	struct v4l2_ctrl *colorfx;
	struct v4l2_ctrl *saturation;
	struct v4l2_ctrl *zoom;

	struct m5mols_version ver;
	struct m5mols_capture cap;

	unsigned int isp_ready:1;
	unsigned int power:1;
	unsigned int ctrl_sync:1;

	bool lock_ae;
	bool lock_awb;
	u8 resolution;
	u8 mode;

	int (*set_power)(struct device *dev, int on);
};

#define is_available_af(__info)	(__info->ver.af)
#define is_code(__code, __type) (__code == m5mols_default_ffmt[__type].code)
#define is_manufacturer(__info, __manufacturer)	\
				(__info->ver.str[0] == __manufacturer[0] && \
				 __info->ver.str[1] == __manufacturer[1])
int m5mols_read_u8(struct v4l2_subdev *sd, u32 reg_comb, u8 *val);
int m5mols_read_u16(struct v4l2_subdev *sd, u32 reg_comb, u16 *val);
int m5mols_read_u32(struct v4l2_subdev *sd, u32 reg_comb, u32 *val);
int m5mols_write(struct v4l2_subdev *sd, u32 reg_comb, u32 val);

int m5mols_busy_wait(struct v4l2_subdev *sd, u32 reg, u32 value, u32 mask,
		     int timeout);

#define M5MOLS_I2C_RDY_WAIT_FL		(1 << 16)
#define M5MOLS_MODE_CHANGE_TIMEOUT	200
#define M5MOLS_BUSY_WAIT_DEF_TIMEOUT	250

int m5mols_mode(struct m5mols_info *info, u8 mode);

int m5mols_enable_interrupt(struct v4l2_subdev *sd, u8 reg);
int m5mols_wait_interrupt(struct v4l2_subdev *sd, u8 condition, u32 timeout);
int m5mols_restore_controls(struct m5mols_info *info);
int m5mols_start_capture(struct m5mols_info *info);
int m5mols_do_scenemode(struct m5mols_info *info, u8 mode);
int m5mols_lock_3a(struct m5mols_info *info, bool lock);
int m5mols_set_ctrl(struct v4l2_ctrl *ctrl);

int m5mols_update_fw(struct v4l2_subdev *sd,
		     int (*set_power)(struct m5mols_info *, bool));

#endif	
