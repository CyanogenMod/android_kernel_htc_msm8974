/*
 * mt9t112 Camera Driver
 *
 * Copyright (C) 2009 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * Based on ov772x driver, mt9m111 driver,
 *
 * Copyright (C) 2008 Kuninori Morimoto <morimoto.kuninori@renesas.com>
 * Copyright (C) 2008, Robert Jarzmik <robert.jarzmik@free.fr>
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 * Copyright (C) 2008 Magnus Damm
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/mt9t112.h>
#include <media/soc_camera.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-common.h>


#define MAX_WIDTH   2048
#define MAX_HEIGHT  1536

#define VGA_WIDTH   640
#define VGA_HEIGHT  480

#define ECHECKER(ret, x)		\
	do {				\
		(ret) = (x);		\
		if ((ret) < 0)		\
			return (ret);	\
	} while (0)

#define mt9t112_reg_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_reg_write(client, a, b))
#define mt9t112_mcu_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_mcu_write(client, a, b))

#define mt9t112_reg_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_reg_mask_set(client, a, b, c))
#define mt9t112_mcu_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_mcu_mask_set(client, a, b, c))

#define mt9t112_reg_read(ret, client, a) \
	ECHECKER(ret, __mt9t112_reg_read(client, a))

#define _VAR(id, offset, base)	(base | (id & 0x1f) << 10 | (offset & 0x3ff))
#define VAR(id, offset)  _VAR(id, offset, 0x0000)
#define VAR8(id, offset) _VAR(id, offset, 0x8000)

struct mt9t112_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

struct mt9t112_priv {
	struct v4l2_subdev		 subdev;
	struct mt9t112_camera_info	*info;
	struct i2c_client		*client;
	struct v4l2_rect		 frame;
	const struct mt9t112_format	*format;
	int				 model;
	u32				 flags;
#define INIT_DONE	(1 << 0)
#define PCLK_RISING	(1 << 1)
};


static const struct mt9t112_format mt9t112_cfmts[] = {
	{
		.code		= V4L2_MBUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 0,
	}, {
		.code		= V4L2_MBUS_FMT_VYUY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 1,
	}, {
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 2,
	}, {
		.code		= V4L2_MBUS_FMT_YVYU8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 3,
	}, {
		.code		= V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 8,
		.order		= 2,
	}, {
		.code		= V4L2_MBUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 4,
		.order		= 2,
	},
};

static struct mt9t112_priv *to_mt9t112(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct mt9t112_priv,
			    subdev);
}

static int __mt9t112_reg_read(const struct i2c_client *client, u16 command)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	command = swab16(command);

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *)&command;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 2;
	msg[1].buf   = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	memcpy(&ret, buf, 2);
	return swab16(ret);
}

static int __mt9t112_reg_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	command = swab16(command);
	data = swab16(data);

	memcpy(buf + 0, &command, 2);
	memcpy(buf + 2, &data,    2);

	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 4;
	msg.buf   = buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		ret = 0;

	return ret;
}

static int __mt9t112_reg_mask_set(const struct i2c_client *client,
				  u16  command,
				  u16  mask,
				  u16  set)
{
	int val = __mt9t112_reg_read(client, command);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_reg_write(client, command, val);
}

static int __mt9t112_mcu_read(const struct i2c_client *client, u16 command)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_read(client, 0x0990);
}

static int __mt9t112_mcu_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_write(client, 0x0990, data);
}

static int __mt9t112_mcu_mask_set(const struct i2c_client *client,
				  u16  command,
				  u16  mask,
				  u16  set)
{
	int val = __mt9t112_mcu_read(client, command);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_mcu_write(client, command, val);
}

static int mt9t112_reset(const struct i2c_client *client)
{
	int ret;

	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0001);
	msleep(1);
	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0000);

	return ret;
}

#ifndef EXT_CLOCK
#define CLOCK_INFO(a, b)
#else
#define CLOCK_INFO(a, b) mt9t112_clock_info(a, b)
static int mt9t112_clock_info(const struct i2c_client *client, u32 ext)
{
	int m, n, p1, p2, p3, p4, p5, p6, p7;
	u32 vco, clk;
	char *enable;

	ext /= 1000; 

	mt9t112_reg_read(n, client, 0x0012);
	p1 = n & 0x000f;
	n = n >> 4;
	p2 = n & 0x000f;
	n = n >> 4;
	p3 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002a);
	p4 = n & 0x000f;
	n = n >> 4;
	p5 = n & 0x000f;
	n = n >> 4;
	p6 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002c);
	p7 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x0010);
	m = n & 0x00ff;
	n = (n >> 8) & 0x003f;

	enable = ((6000 > ext) || (54000 < ext)) ? "X" : "";
	dev_dbg(&client->dev, "EXTCLK          : %10u K %s\n", ext, enable);

	vco = 2 * m * ext / (n+1);
	enable = ((384000 > vco) || (768000 < vco)) ? "X" : "";
	dev_dbg(&client->dev, "VCO             : %10u K %s\n", vco, enable);

	clk = vco / (p1+1) / (p2+1);
	enable = (96000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "PIXCLK          : %10u K %s\n", clk, enable);

	clk = vco / (p3+1);
	enable = (768000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "MIPICLK         : %10u K %s\n", clk, enable);

	clk = vco / (p6+1);
	enable = (96000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "MCU CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p5+1);
	enable = (54000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "SOC CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p4+1);
	enable = (70000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "Sensor CLK      : %10u K %s\n", clk, enable);

	clk = vco / (p7+1);
	dev_dbg(&client->dev, "External sensor : %10u K\n", clk);

	clk = ext / (n+1);
	enable = ((2000 > clk) || (24000 < clk)) ? "X" : "";
	dev_dbg(&client->dev, "PFD             : %10u K %s\n", clk, enable);

	return 0;
}
#endif

static void mt9t112_frame_check(u32 *width, u32 *height, u32 *left, u32 *top)
{
	soc_camera_limit_side(left, width, 0, 0, MAX_WIDTH);
	soc_camera_limit_side(top, height, 0, 0, MAX_HEIGHT);
}

static int mt9t112_set_a_frame_size(const struct i2c_client *client,
				   u16 width,
				   u16 height)
{
	int ret;
	u16 wstart = (MAX_WIDTH - width) / 2;
	u16 hstart = (MAX_HEIGHT - height) / 2;

	
	mt9t112_mcu_write(ret, client, VAR(26, 0), width);
	mt9t112_mcu_write(ret, client, VAR(26, 2), height);

	
	mt9t112_mcu_write(ret, client, VAR(18, 43), 8 + width);
	mt9t112_mcu_write(ret, client, VAR(18, 45), 8 + height);

	
	mt9t112_mcu_write(ret, client, VAR(18, 2), 4 + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 4), 4 + wstart);

	
	mt9t112_mcu_write(ret, client, VAR(18, 6), 11 + height + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 8), 11 + width  + wstart);

	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	return ret;
}

static int mt9t112_set_pll_dividers(const struct i2c_client *client,
				    u8 m, u8 n,
				    u8 p1, u8 p2, u8 p3,
				    u8 p4, u8 p5, u8 p6,
				    u8 p7)
{
	int ret;
	u16 val;

	
	val = (n << 8) |
	      (m << 0);
	mt9t112_reg_mask_set(ret, client, 0x0010, 0x3fff, val);

	
	val = ((p3 & 0x0F) << 8) |
	      ((p2 & 0x0F) << 4) |
	      ((p1 & 0x0F) << 0);
	mt9t112_reg_mask_set(ret, client, 0x0012, 0x0fff, val);

	
	val = (0x7         << 12) |
	      ((p6 & 0x0F) <<  8) |
	      ((p5 & 0x0F) <<  4) |
	      ((p4 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002A, 0x7fff, val);

	
	val = (0x1         << 12) |
	      ((p7 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002C, 0x100f, val);

	return ret;
}

static int mt9t112_init_pll(const struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	int data, i, ret;

	mt9t112_reg_mask_set(ret, client, 0x0014, 0x003, 0x0001);

	
	mt9t112_reg_write(ret, client, 0x0014, 0x2145);

	
	mt9t112_set_pll_dividers(client,
				 priv->info->divider.m,
				 priv->info->divider.n,
				 priv->info->divider.p1,
				 priv->info->divider.p2,
				 priv->info->divider.p3,
				 priv->info->divider.p4,
				 priv->info->divider.p5,
				 priv->info->divider.p6,
				 priv->info->divider.p7);

	mt9t112_reg_write(ret, client, 0x0014, 0x2525);
	mt9t112_reg_write(ret, client, 0x0014, 0x2527);
	mt9t112_reg_write(ret, client, 0x0014, 0x3427);
	mt9t112_reg_write(ret, client, 0x0014, 0x3027);

	mdelay(10);

	mt9t112_reg_write(ret, client, 0x0014, 0x3046);
	mt9t112_reg_write(ret, client, 0x0022, 0x0190);
	mt9t112_reg_write(ret, client, 0x3B84, 0x0212);

	
	mt9t112_reg_write(ret, client, 0x002E, 0x0500);

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0002, 0x0002);
	mt9t112_reg_mask_set(ret, client, 0x3B82, 0x0004, 0x0004);

	
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0x0004);

	
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0001, 0);

	mdelay(50);

	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);

	
	for (i = 0; i < 100; i++) {
		mt9t112_reg_read(data, client, 0x0018);
		if (!(0x4000 & data))
			break;

		mdelay(10);
	}

	return ret;
}

static int mt9t112_init_setting(const struct i2c_client *client)
{

	int ret;

	
	mt9t112_mcu_mask_set(ret, client, VAR(26, 160), 0x0040, 0x0000);

	
	mt9t112_mcu_write(ret, client, VAR(18, 12), 0x0024);

	
	mt9t112_mcu_write(ret, client, VAR(18, 15), 0x00CC);

	
	mt9t112_mcu_write(ret, client, VAR(18, 17), 0x01f1);

	
	mt9t112_mcu_write(ret, client, VAR(18, 19), 0x00fF);

	
	mt9t112_mcu_write(ret, client, VAR(18, 29), 0x032D);

	
	mt9t112_mcu_write(ret, client, VAR(18, 31), 0x073a);

	
	mt9t112_mcu_write(ret, client, VAR(18, 37), 0x07d0);

	
	mt9t112_mcu_mask_set(ret, client, VAR(27, 160), 0x0040, 0x0000);

	
	mt9t112_mcu_write(ret, client, VAR(18, 74), 0x004);

	
	mt9t112_mcu_write(ret, client, VAR(18, 76), 0x004);

	
	mt9t112_mcu_write(ret, client, VAR(18, 78), 0x60B);

	
	mt9t112_mcu_write(ret, client, VAR(18, 80), 0x80B);

	
	mt9t112_mcu_write(ret, client, VAR(18, 87), 0x008C);

	
	mt9t112_mcu_write(ret, client, VAR(18, 89), 0x01F1);

	
	mt9t112_mcu_write(ret, client, VAR(18, 91), 0x00FF);

	
	mt9t112_mcu_write(ret, client, VAR(18, 101), 0x0668);

	
	mt9t112_mcu_write(ret, client, VAR(18, 103), 0x0AF0);

	
	mt9t112_mcu_write(ret, client, VAR(18, 109), 0x0AF0);


	
	mt9t112_mcu_write(ret, client, VAR8(8, 5), 0x01);

	
	mt9t112_mcu_write(ret, client, VAR(27, 17), 0x0003);

	
	mt9t112_mcu_write(ret, client, VAR(26, 17), 0x0003);


	
	mt9t112_mcu_write(ret, client, VAR8(18, 165), 0x25);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 166), 0x28);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 167), 0x2C);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 168), 0x2F);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 68), 0xBA);

	
	
	mt9t112_mcu_write(ret, client, VAR8(18, 303), 0x00);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 69), 0x9B);

	
	
	mt9t112_mcu_write(ret, client, VAR8(18, 301), 0x00);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 140), 0x82);

	
	
	mt9t112_mcu_write(ret, client, VAR8(18, 304), 0x00);

	
	mt9t112_mcu_write(ret, client, VAR8(18, 141), 0x6D);

	
	
	mt9t112_mcu_write(ret, client, VAR8(18, 302), 0x00);

	
	mt9t112_mcu_write(ret, client, VAR8(8, 2), 0x10);

	
	mt9t112_mcu_write(ret, client, VAR8(8, 9), 0x02);

	
	mt9t112_mcu_write(ret, client, VAR8(8, 10), 0x03);

	
	mt9t112_mcu_write(ret, client, VAR8(8, 12), 0x0A);

	
	mt9t112_mcu_write(ret, client, VAR(18, 70), 0x0014);

	
	mt9t112_mcu_write(ret, client, VAR(18, 142), 0x0014);

	mt9t112_mcu_write(ret, client, VAR8(18, 0x0044), 133);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0045), 110);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008c), 130);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008d), 108);

	mt9t112_mcu_write(ret, client, VAR8(18, 0x00A5), 27);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a6), 30);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a7), 32);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a8), 35);

	return ret;
}

static int mt9t112_auto_focus_setting(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR(12, 13),	0x000F);
	mt9t112_mcu_write(ret, client, VAR(12, 23),	0x0F0F);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x06);

	mt9t112_reg_write(ret, client, 0x0614, 0x0000);

	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);
	mt9t112_mcu_write(ret, client, VAR8(12, 2),	0x02);
	mt9t112_mcu_write(ret, client, VAR(12, 3),	0x0002);
	mt9t112_mcu_write(ret, client, VAR(17, 3),	0x8001);
	mt9t112_mcu_write(ret, client, VAR(17, 11),	0x0025);
	mt9t112_mcu_write(ret, client, VAR(17, 13),	0x0193);
	mt9t112_mcu_write(ret, client, VAR8(17, 33),	0x18);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);

	return ret;
}

static int mt9t112_auto_focus_trigger(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR8(12, 25), 0x01);

	return ret;
}

static int mt9t112_init_camera(const struct i2c_client *client)
{
	int ret;

	ECHECKER(ret, mt9t112_reset(client));

	ECHECKER(ret, mt9t112_init_pll(client));

	ECHECKER(ret, mt9t112_init_setting(client));

	ECHECKER(ret, mt9t112_auto_focus_setting(client));

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0);

	
	mt9t112_reg_write(ret, client, 0x3084, 0x2409);
	mt9t112_reg_write(ret, client, 0x3092, 0x0A49);
	mt9t112_reg_write(ret, client, 0x3094, 0x4949);
	mt9t112_reg_write(ret, client, 0x3096, 0x4950);

	mt9t112_mcu_write(ret, client, VAR(26, 160), 0x0A2E);
	mt9t112_mcu_write(ret, client, VAR(27, 160), 0x0A2E);

	
	
	mt9t112_mcu_write(ret, client, VAR(27, 144), 0x0CB4);

	
	
	mt9t112_mcu_write(ret, client, VAR8(27, 142), 0x01);

	
	mt9t112_reg_write(ret, client, 0x316C, 0x350F);

	
	mt9t112_reg_write(ret, client, 0x1E, 0x777);

	return ret;
}

static int mt9t112_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	id->ident    = priv->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9t112_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int                ret;

	reg->size = 2;
	mt9t112_reg_read(ret, client, reg->reg);

	reg->val = (__u64)ret;

	return 0;
}

static int mt9t112_s_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mt9t112_reg_write(ret, client, reg->reg, reg->val);

	return ret;
}
#endif

static struct v4l2_subdev_core_ops mt9t112_subdev_core_ops = {
	.g_chip_ident	= mt9t112_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9t112_g_register,
	.s_register	= mt9t112_s_register,
#endif
};


static int mt9t112_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	int ret = 0;

	if (!enable) {
		mt9t112_set_a_frame_size(client, VGA_WIDTH, VGA_HEIGHT);
		return ret;
	}

	if (!(priv->flags & INIT_DONE)) {
		u16 param = PCLK_RISING & priv->flags ? 0x0001 : 0x0000;

		ECHECKER(ret, mt9t112_init_camera(client));

		
		mt9t112_reg_write(ret, client, 0x3C20, param);

		mdelay(5);

		priv->flags |= INIT_DONE;
	}

	mt9t112_mcu_write(ret, client, VAR(26, 7), priv->format->fmt);
	mt9t112_mcu_write(ret, client, VAR(26, 9), priv->format->order);
	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	mt9t112_set_a_frame_size(client,
				 priv->frame.width,
				 priv->frame.height);

	ECHECKER(ret, mt9t112_auto_focus_trigger(client));

	dev_dbg(&client->dev, "format : %d\n", priv->format->code);
	dev_dbg(&client->dev, "size   : %d x %d\n",
		priv->frame.width,
		priv->frame.height);

	CLOCK_INFO(client, EXT_CLOCK);

	return ret;
}

static int mt9t112_set_params(struct mt9t112_priv *priv,
			      const struct v4l2_rect *rect,
			      enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt9t112_cfmts); i++)
		if (mt9t112_cfmts[i].code == code)
			break;

	if (i == ARRAY_SIZE(mt9t112_cfmts))
		return -EINVAL;

	priv->frame  = *rect;

	mt9t112_frame_check(&priv->frame.width, &priv->frame.height,
			    &priv->frame.left, &priv->frame.top);

	priv->format = mt9t112_cfmts + i;

	return 0;
}

static int mt9t112_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	a->defrect.left			= 0;
	a->defrect.top			= 0;
	a->defrect.width		= VGA_WIDTH;
	a->defrect.height		= VGA_HEIGHT;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int mt9t112_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	a->c	= priv->frame;
	a->type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int mt9t112_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct v4l2_rect *rect = &a->c;

	return mt9t112_set_params(priv, rect, priv->format->code);
}

static int mt9t112_g_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	mf->width	= priv->frame.width;
	mf->height	= priv->frame.height;
	mf->colorspace	= priv->format->colorspace;
	mf->code	= priv->format->code;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int mt9t112_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct v4l2_rect rect = {
		.width = mf->width,
		.height = mf->height,
		.left = priv->frame.left,
		.top = priv->frame.top,
	};
	int ret;

	ret = mt9t112_set_params(priv, &rect, mf->code);

	if (!ret)
		mf->colorspace = priv->format->colorspace;

	return ret;
}

static int mt9t112_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	unsigned int top, left;
	int i;

	for (i = 0; i < ARRAY_SIZE(mt9t112_cfmts); i++)
		if (mt9t112_cfmts[i].code == mf->code)
			break;

	if (i == ARRAY_SIZE(mt9t112_cfmts)) {
		mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	} else {
		mf->colorspace	= mt9t112_cfmts[i].colorspace;
	}

	mt9t112_frame_check(&mf->width, &mf->height, &left, &top);

	mf->field = V4L2_FIELD_NONE;

	return 0;
}

static int mt9t112_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(mt9t112_cfmts))
		return -EINVAL;

	*code = mt9t112_cfmts[index].code;

	return 0;
}

static int mt9t112_g_mbus_config(struct v4l2_subdev *sd,
				 struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	cfg->flags = V4L2_MBUS_MASTER | V4L2_MBUS_VSYNC_ACTIVE_HIGH |
		V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_DATA_ACTIVE_HIGH |
		V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_PCLK_SAMPLE_FALLING;
	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = soc_camera_apply_board_flags(icl, cfg);

	return 0;
}

static int mt9t112_s_mbus_config(struct v4l2_subdev *sd,
				 const struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	struct mt9t112_priv *priv = to_mt9t112(client);

	if (soc_camera_apply_board_flags(icl, cfg) & V4L2_MBUS_PCLK_SAMPLE_RISING)
		priv->flags |= PCLK_RISING;

	return 0;
}

static struct v4l2_subdev_video_ops mt9t112_subdev_video_ops = {
	.s_stream	= mt9t112_s_stream,
	.g_mbus_fmt	= mt9t112_g_fmt,
	.s_mbus_fmt	= mt9t112_s_fmt,
	.try_mbus_fmt	= mt9t112_try_fmt,
	.cropcap	= mt9t112_cropcap,
	.g_crop		= mt9t112_g_crop,
	.s_crop		= mt9t112_s_crop,
	.enum_mbus_fmt	= mt9t112_enum_fmt,
	.g_mbus_config	= mt9t112_g_mbus_config,
	.s_mbus_config	= mt9t112_s_mbus_config,
};

static struct v4l2_subdev_ops mt9t112_subdev_ops = {
	.core	= &mt9t112_subdev_core_ops,
	.video	= &mt9t112_subdev_video_ops,
};

static int mt9t112_camera_probe(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	const char          *devname;
	int                  chipid;

	mt9t112_reg_read(chipid, client, 0x0000);

	switch (chipid) {
	case 0x2680:
		devname = "mt9t111";
		priv->model = V4L2_IDENT_MT9T111;
		break;
	case 0x2682:
		devname = "mt9t112";
		priv->model = V4L2_IDENT_MT9T112;
		break;
	default:
		dev_err(&client->dev, "Product ID error %04x\n", chipid);
		return -ENODEV;
	}

	dev_info(&client->dev, "%s chip ID %04x\n", devname, chipid);

	return 0;
}

static int mt9t112_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9t112_priv *priv;
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	struct v4l2_rect rect = {
		.width = VGA_WIDTH,
		.height = VGA_HEIGHT,
		.left = (MAX_WIDTH - VGA_WIDTH) / 2,
		.top = (MAX_HEIGHT - VGA_HEIGHT) / 2,
	};
	int ret;

	if (!icl || !icl->priv) {
		dev_err(&client->dev, "mt9t112: missing platform data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = icl->priv;

	v4l2_i2c_subdev_init(&priv->subdev, client, &mt9t112_subdev_ops);

	ret = mt9t112_camera_probe(client);
	if (ret) {
		kfree(priv);
		return ret;
	}

	
	mt9t112_set_params(priv, &rect, V4L2_MBUS_FMT_UYVY8_2X8);

	return ret;
}

static int mt9t112_remove(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);

	kfree(priv);
	return 0;
}

static const struct i2c_device_id mt9t112_id[] = {
	{ "mt9t112", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9t112_id);

static struct i2c_driver mt9t112_i2c_driver = {
	.driver = {
		.name = "mt9t112",
	},
	.probe    = mt9t112_probe,
	.remove   = mt9t112_remove,
	.id_table = mt9t112_id,
};

module_i2c_driver(mt9t112_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for mt9t112");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
