/*
 * ov772x Camera Driver
 *
 * Copyright (C) 2008 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * Based on ov7670 and soc_camera_platform driver,
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 * Copyright (C) 2008 Magnus Damm
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/ov772x.h>
#include <media/soc_camera.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>

#define GAIN        0x00 
#define BLUE        0x01 
#define RED         0x02 
#define GREEN       0x03 
#define COM1        0x04 
#define BAVG        0x05 
#define GAVG        0x06 
#define RAVG        0x07 
#define AECH        0x08 
#define COM2        0x09 
#define PID         0x0A 
#define VER         0x0B 
#define COM3        0x0C 
#define COM4        0x0D 
#define COM5        0x0E 
#define COM6        0x0F 
#define AEC         0x10 
#define CLKRC       0x11 
#define COM7        0x12 
#define COM8        0x13 
#define COM9        0x14 
#define COM10       0x15 
#define REG16       0x16 
#define HSTART      0x17 
#define HSIZE       0x18 
#define VSTART      0x19 
#define VSIZE       0x1A 
#define PSHFT       0x1B 
#define MIDH        0x1C 
#define MIDL        0x1D 
#define LAEC        0x1F 
#define COM11       0x20 
#define BDBASE      0x22 
#define DBSTEP      0x23 
#define AEW         0x24 
#define AEB         0x25 
#define VPT         0x26 
#define REG28       0x28 
#define HOUTSIZE    0x29 
#define EXHCH       0x2A 
#define EXHCL       0x2B 
#define VOUTSIZE    0x2C 
#define ADVFL       0x2D 
#define ADVFH       0x2E 
#define YAVE        0x2F 
#define LUMHTH      0x30 
#define LUMLTH      0x31 
#define HREF        0x32 
#define DM_LNL      0x33 
#define DM_LNH      0x34 
#define ADOFF_B     0x35 
#define ADOFF_R     0x36 
#define ADOFF_GB    0x37 
#define ADOFF_GR    0x38 
#define OFF_B       0x39 
#define OFF_R       0x3A 
#define OFF_GB      0x3B 
#define OFF_GR      0x3C 
#define COM12       0x3D 
#define COM13       0x3E 
#define COM14       0x3F 
#define COM15       0x40 
#define COM16       0x41 
#define TGT_B       0x42 
#define TGT_R       0x43 
#define TGT_GB      0x44 
#define TGT_GR      0x45 
#define LCC0        0x46 
#define LCC1        0x47 
#define LCC2        0x48 
#define LCC3        0x49 
#define LCC4        0x4A 
#define LCC5        0x4B 
#define LCC6        0x4C 
#define LC_CTR      0x46 
#define LC_XC       0x47 
#define LC_YC       0x48 
#define LC_COEF     0x49 
#define LC_RADI     0x4A 
#define LC_COEFB    0x4B 
#define LC_COEFR    0x4C 

#define FIXGAIN     0x4D 
#define AREF0       0x4E 
#define AREF1       0x4F 
#define AREF2       0x50 
#define AREF3       0x51 
#define AREF4       0x52 
#define AREF5       0x53 
#define AREF6       0x54 
#define AREF7       0x55 
#define UFIX        0x60 
#define VFIX        0x61 
#define AWBB_BLK    0x62 
#define AWB_CTRL0   0x63 
#define DSP_CTRL1   0x64 
#define DSP_CTRL2   0x65 
#define DSP_CTRL3   0x66 
#define DSP_CTRL4   0x67 
#define AWB_BIAS    0x68 
#define AWB_CTRL1   0x69 
#define AWB_CTRL2   0x6A 
#define AWB_CTRL3   0x6B 
#define AWB_CTRL4   0x6C 
#define AWB_CTRL5   0x6D 
#define AWB_CTRL6   0x6E 
#define AWB_CTRL7   0x6F 
#define AWB_CTRL8   0x70 
#define AWB_CTRL9   0x71 
#define AWB_CTRL10  0x72 
#define AWB_CTRL11  0x73 
#define AWB_CTRL12  0x74 
#define AWB_CTRL13  0x75 
#define AWB_CTRL14  0x76 
#define AWB_CTRL15  0x77 
#define AWB_CTRL16  0x78 
#define AWB_CTRL17  0x79 
#define AWB_CTRL18  0x7A 
#define AWB_CTRL19  0x7B 
#define AWB_CTRL20  0x7C 
#define AWB_CTRL21  0x7D 
#define GAM1        0x7E 
#define GAM2        0x7F 
#define GAM3        0x80 
#define GAM4        0x81 
#define GAM5        0x82 
#define GAM6        0x83 
#define GAM7        0x84 
#define GAM8        0x85 
#define GAM9        0x86 
#define GAM10       0x87 
#define GAM11       0x88 
#define GAM12       0x89 
#define GAM13       0x8A 
#define GAM14       0x8B 
#define GAM15       0x8C 
#define SLOP        0x8D 
#define DNSTH       0x8E 
#define EDGE_STRNGT 0x8F 
#define EDGE_TRSHLD 0x90 
#define DNSOFF      0x91 
#define EDGE_UPPER  0x92 
#define EDGE_LOWER  0x93 
#define MTX1        0x94 
#define MTX2        0x95 
#define MTX3        0x96 
#define MTX4        0x97 
#define MTX5        0x98 
#define MTX6        0x99 
#define MTX_CTRL    0x9A 
#define BRIGHT      0x9B 
#define CNTRST      0x9C 
#define CNTRST_CTRL 0x9D 
#define UVAD_J0     0x9E 
#define UVAD_J1     0x9F 
#define SCAL0       0xA0 
#define SCAL1       0xA1 
#define SCAL2       0xA2 
#define FIFODLYM    0xA3 
#define FIFODLYA    0xA4 
#define SDE         0xA6 
#define USAT        0xA7 
#define VSAT        0xA8 
#define HUE0        0xA9 
#define HUE1        0xAA 
#define HUECOS      0xA9 
#define HUESIN      0xAA 

#define SIGN        0xAB 
#define DSPAUTO     0xAC 


#define SOFT_SLEEP_MODE 0x10	
				
#define OCAP_1x         0x00	
#define OCAP_2x         0x01	
#define OCAP_3x         0x02	
#define OCAP_4x         0x03	

#define SWAP_MASK       (SWAP_RGB | SWAP_YUV | SWAP_ML)
#define IMG_MASK        (VFLIP_IMG | HFLIP_IMG)

#define VFLIP_IMG       0x80	
#define HFLIP_IMG       0x40	
#define SWAP_RGB        0x20	
#define SWAP_YUV        0x10	
#define SWAP_ML         0x08	
				
#define NOTRI_CLOCK     0x04	
				
				
#define NOTRI_DATA      0x02	
				
#define SCOLOR_TEST     0x01	

				
#define PLL_BYPASS      0x00	
#define PLL_4x          0x40	
#define PLL_6x          0x80	
#define PLL_8x          0xc0	
				
#define AEC_FULL        0x00	
#define AEC_1p2         0x10	
#define AEC_1p4         0x20	
#define AEC_2p3         0x30	

#define AFR_ON_OFF      0x80	
#define AFR_SPPED       0x40	
				
#define AFR_NO_RATE     0x00	
#define AFR_1p2         0x10	
#define AFR_1p4         0x20	
#define AFR_1p8         0x30	
				
#define AF_2x           0x00	
#define AF_4x           0x04	
#define AF_8x           0x08	
#define AF_16x          0x0c	
				
#define AEC_NO_LIMIT    0x01	
				

				
#define SCCB_RESET      0x80	
				
				
#define SLCT_MASK       0x40	
#define SLCT_VGA        0x00	
#define SLCT_QVGA       0x40	
#define ITU656_ON_OFF   0x20	
				
#define FMT_MASK        0x0c	
#define FMT_GBR422      0x00	
#define FMT_RGB565      0x04	
#define FMT_RGB555      0x08	
#define FMT_RGB444      0x0c	
				
#define OFMT_MASK       0x03    
#define OFMT_YUV        0x00	
#define OFMT_P_BRAW     0x01	
#define OFMT_RGB        0x02	
#define OFMT_BRAW       0x03	

#define FAST_ALGO       0x80	
				
#define UNLMT_STEP      0x40	
				
#define BNDF_ON_OFF     0x20	
#define AEC_BND         0x10	
#define AEC_ON_OFF      0x08	
#define AGC_ON          0x04	
#define AWB_ON          0x02	
#define AEC_ON          0x01	

#define BASE_AECAGC     0x80	
				
#define GAIN_2x         0x00	
#define GAIN_4x         0x10	
#define GAIN_8x         0x20	
#define GAIN_16x        0x30	
#define GAIN_32x        0x40	
#define GAIN_64x        0x50	
#define GAIN_128x       0x60	
#define DROP_VSYNC      0x04	
#define DROP_HREF       0x02	

#define SGLF_ON_OFF     0x02	
#define SGLF_TRIG       0x01	

#define VSIZE_LSB       0x04	

#define FIFO_ON         0x80	
#define UV_ON_OFF       0x40	
#define YUV444_2_422    0x20	
#define CLR_MTRX_ON_OFF 0x10	
#define INTPLT_ON_OFF   0x08	
#define GMM_ON_OFF      0x04	
#define AUTO_BLK_ON_OFF 0x02	
#define AUTO_WHT_ON_OFF 0x01	

#define UV_MASK         0x80	
#define UV_ON           0x80	
#define UV_OFF          0x00	
#define CBAR_MASK       0x20	
#define CBAR_ON         0x20	
#define CBAR_OFF        0x00	

#define HST_VGA         0x23
#define HST_QVGA        0x3F

#define HSZ_VGA         0xA0
#define HSZ_QVGA        0x50

#define VST_VGA         0x07
#define VST_QVGA        0x03

#define VSZ_VGA         0xF0
#define VSZ_QVGA        0x78

#define HOSZ_VGA        0xA0
#define HOSZ_QVGA       0x50

#define VOSZ_VGA        0xF0
#define VOSZ_QVGA       0x78

#define AWB_ACTRL       0x80 
#define DENOISE_ACTRL   0x40 
#define EDGE_ACTRL      0x20 
#define UV_ACTRL        0x10 
#define SCAL0_ACTRL     0x08 
#define SCAL1_2_ACTRL   0x04 

#define OV7720  0x7720
#define OV7725  0x7721
#define VERSION(pid, ver) ((pid<<8)|(ver&0xFF))

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct ov772x_color_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u8 dsp3;
	u8 com3;
	u8 com7;
};

struct ov772x_win_size {
	char                     *name;
	__u32                     width;
	__u32                     height;
	unsigned char             com7_bit;
	const struct regval_list *regs;
};

struct ov772x_priv {
	struct v4l2_subdev                subdev;
	struct v4l2_ctrl_handler	  hdl;
	struct ov772x_camera_info        *info;
	const struct ov772x_color_format *cfmt;
	const struct ov772x_win_size     *win;
	int                               model;
	unsigned short                    flag_vflip:1;
	unsigned short                    flag_hflip:1;
	
	unsigned short                    band_filter;
};

#define ENDMARKER { 0xff, 0xff }

static const struct regval_list ov772x_qvga_regs[] = {
	{ HSTART,   HST_QVGA },
	{ HSIZE,    HSZ_QVGA },
	{ VSTART,   VST_QVGA },
	{ VSIZE,    VSZ_QVGA  },
	{ HOUTSIZE, HOSZ_QVGA },
	{ VOUTSIZE, VOSZ_QVGA },
	ENDMARKER,
};

static const struct regval_list ov772x_vga_regs[] = {
	{ HSTART,   HST_VGA },
	{ HSIZE,    HSZ_VGA },
	{ VSTART,   VST_VGA },
	{ VSIZE,    VSZ_VGA },
	{ HOUTSIZE, HOSZ_VGA },
	{ VOUTSIZE, VOSZ_VGA },
	ENDMARKER,
};

static const struct ov772x_color_format ov772x_cfmts[] = {
	{
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.dsp3		= 0x0,
		.com3		= SWAP_YUV,
		.com7		= OFMT_YUV,
	},
	{
		.code		= V4L2_MBUS_FMT_YVYU8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.dsp3		= UV_ON,
		.com3		= SWAP_YUV,
		.com7		= OFMT_YUV,
	},
	{
		.code		= V4L2_MBUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.dsp3		= 0x0,
		.com3		= 0x0,
		.com7		= OFMT_YUV,
	},
	{
		.code		= V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.com3		= SWAP_RGB,
		.com7		= FMT_RGB555 | OFMT_RGB,
	},
	{
		.code		= V4L2_MBUS_FMT_RGB555_2X8_PADHI_BE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.com3		= 0x0,
		.com7		= FMT_RGB555 | OFMT_RGB,
	},
	{
		.code		= V4L2_MBUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.com3		= SWAP_RGB,
		.com7		= FMT_RGB565 | OFMT_RGB,
	},
	{
		.code		= V4L2_MBUS_FMT_RGB565_2X8_BE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.com3		= 0x0,
		.com7		= FMT_RGB565 | OFMT_RGB,
	},
};


#define VGA_WIDTH   640
#define VGA_HEIGHT  480
#define QVGA_WIDTH  320
#define QVGA_HEIGHT 240
#define MAX_WIDTH   VGA_WIDTH
#define MAX_HEIGHT  VGA_HEIGHT

static const struct ov772x_win_size ov772x_win_vga = {
	.name     = "VGA",
	.width    = VGA_WIDTH,
	.height   = VGA_HEIGHT,
	.com7_bit = SLCT_VGA,
	.regs     = ov772x_vga_regs,
};

static const struct ov772x_win_size ov772x_win_qvga = {
	.name     = "QVGA",
	.width    = QVGA_WIDTH,
	.height   = QVGA_HEIGHT,
	.com7_bit = SLCT_QVGA,
	.regs     = ov772x_qvga_regs,
};


static struct ov772x_priv *to_ov772x(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov772x_priv,
			    subdev);
}

static int ov772x_write_array(struct i2c_client        *client,
			      const struct regval_list *vals)
{
	while (vals->reg_num != 0xff) {
		int ret = i2c_smbus_write_byte_data(client,
						    vals->reg_num,
						    vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int ov772x_mask_set(struct i2c_client *client,
					  u8  command,
					  u8  mask,
					  u8  set)
{
	s32 val = i2c_smbus_read_byte_data(client, command);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return i2c_smbus_write_byte_data(client, command, val);
}

static int ov772x_reset(struct i2c_client *client)
{
	int ret = i2c_smbus_write_byte_data(client, COM7, SCCB_RESET);
	msleep(1);
	return ret;
}


static int ov772x_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov772x_priv *priv = container_of(sd, struct ov772x_priv, subdev);

	if (!enable) {
		ov772x_mask_set(client, COM2, SOFT_SLEEP_MODE, SOFT_SLEEP_MODE);
		return 0;
	}

	if (!priv->win || !priv->cfmt) {
		dev_err(&client->dev, "norm or win select error\n");
		return -EPERM;
	}

	ov772x_mask_set(client, COM2, SOFT_SLEEP_MODE, 0);

	dev_dbg(&client->dev, "format %d, win %s\n",
		priv->cfmt->code, priv->win->name);

	return 0;
}

static int ov772x_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov772x_priv *priv = container_of(ctrl->handler,
						struct ov772x_priv, hdl);
	struct v4l2_subdev *sd = &priv->subdev;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 val;

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		val = ctrl->val ? VFLIP_IMG : 0x00;
		priv->flag_vflip = ctrl->val;
		if (priv->info->flags & OV772X_FLAG_VFLIP)
			val ^= VFLIP_IMG;
		return ov772x_mask_set(client, COM3, VFLIP_IMG, val);
	case V4L2_CID_HFLIP:
		val = ctrl->val ? HFLIP_IMG : 0x00;
		priv->flag_hflip = ctrl->val;
		if (priv->info->flags & OV772X_FLAG_HFLIP)
			val ^= HFLIP_IMG;
		return ov772x_mask_set(client, COM3, HFLIP_IMG, val);
	case V4L2_CID_BAND_STOP_FILTER:
		if (!ctrl->val) {
			
			ret = ov772x_mask_set(client, BDBASE, 0xff, 0xff);
			if (!ret)
				ret = ov772x_mask_set(client, COM8,
						      BNDF_ON_OFF, 0);
		} else {
			
			val = 256 - ctrl->val;
			ret = ov772x_mask_set(client, COM8,
					      BNDF_ON_OFF, BNDF_ON_OFF);
			if (!ret)
				ret = ov772x_mask_set(client, BDBASE,
						      0xff, val);
		}
		if (!ret)
			priv->band_filter = ctrl->val;
		return ret;
	}

	return -EINVAL;
}

static int ov772x_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct ov772x_priv *priv = container_of(sd, struct ov772x_priv, subdev);

	id->ident    = priv->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov772x_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	reg->size = 1;
	if (reg->reg > 0xff)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(client, reg->reg);
	if (ret < 0)
		return ret;

	reg->val = (__u64)ret;

	return 0;
}

static int ov772x_s_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return i2c_smbus_write_byte_data(client, reg->reg, reg->val);
}
#endif

static const struct ov772x_win_size *ov772x_select_win(u32 width, u32 height)
{
	__u32 diff;
	const struct ov772x_win_size *win;

	
	diff = abs(width - ov772x_win_qvga.width) +
		abs(height - ov772x_win_qvga.height);
	win = &ov772x_win_qvga;

	
	if (diff >
	    abs(width  - ov772x_win_vga.width) +
	    abs(height - ov772x_win_vga.height))
		win = &ov772x_win_vga;

	return win;
}

static int ov772x_set_params(struct i2c_client *client, u32 *width, u32 *height,
			     enum v4l2_mbus_pixelcode code)
{
	struct ov772x_priv *priv = to_ov772x(client);
	int ret = -EINVAL;
	u8  val;
	int i;

	priv->cfmt = NULL;
	for (i = 0; i < ARRAY_SIZE(ov772x_cfmts); i++) {
		if (code == ov772x_cfmts[i].code) {
			priv->cfmt = ov772x_cfmts + i;
			break;
		}
	}
	if (!priv->cfmt)
		goto ov772x_set_fmt_error;

	priv->win = ov772x_select_win(*width, *height);

	ov772x_reset(client);

	if (priv->info->edgectrl.strength & OV772X_MANUAL_EDGE_CTRL) {


		ret = ov772x_mask_set(client, DSPAUTO, EDGE_ACTRL, 0x00);
		if (ret < 0)
			goto ov772x_set_fmt_error;

		ret = ov772x_mask_set(client,
				      EDGE_TRSHLD, OV772X_EDGE_THRESHOLD_MASK,
				      priv->info->edgectrl.threshold);
		if (ret < 0)
			goto ov772x_set_fmt_error;

		ret = ov772x_mask_set(client,
				      EDGE_STRNGT, OV772X_EDGE_STRENGTH_MASK,
				      priv->info->edgectrl.strength);
		if (ret < 0)
			goto ov772x_set_fmt_error;

	} else if (priv->info->edgectrl.upper > priv->info->edgectrl.lower) {
		ret = ov772x_mask_set(client,
				      EDGE_UPPER, OV772X_EDGE_UPPER_MASK,
				      priv->info->edgectrl.upper);
		if (ret < 0)
			goto ov772x_set_fmt_error;

		ret = ov772x_mask_set(client,
				      EDGE_LOWER, OV772X_EDGE_LOWER_MASK,
				      priv->info->edgectrl.lower);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	ret = ov772x_write_array(client, priv->win->regs);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	val = priv->cfmt->dsp3;
	if (val) {
		ret = ov772x_mask_set(client,
				      DSP_CTRL3, UV_MASK, val);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	val = priv->cfmt->com3;
	if (priv->info->flags & OV772X_FLAG_VFLIP)
		val |= VFLIP_IMG;
	if (priv->info->flags & OV772X_FLAG_HFLIP)
		val |= HFLIP_IMG;
	if (priv->flag_vflip)
		val ^= VFLIP_IMG;
	if (priv->flag_hflip)
		val ^= HFLIP_IMG;

	ret = ov772x_mask_set(client,
			      COM3, SWAP_MASK | IMG_MASK, val);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	val = priv->win->com7_bit | priv->cfmt->com7;
	ret = ov772x_mask_set(client,
			      COM7, SLCT_MASK | FMT_MASK | OFMT_MASK,
			      val);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	if (priv->band_filter) {
		ret = ov772x_mask_set(client, COM8, BNDF_ON_OFF, 1);
		if (!ret)
			ret = ov772x_mask_set(client, BDBASE,
					      0xff, 256 - priv->band_filter);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	*width = priv->win->width;
	*height = priv->win->height;

	return ret;

ov772x_set_fmt_error:

	ov772x_reset(client);
	priv->win = NULL;
	priv->cfmt = NULL;

	return ret;
}

static int ov772x_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= VGA_WIDTH;
	a->c.height	= VGA_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov772x_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= VGA_WIDTH;
	a->bounds.height		= VGA_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int ov772x_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov772x_priv *priv = container_of(sd, struct ov772x_priv, subdev);

	if (!priv->win || !priv->cfmt) {
		u32 width = VGA_WIDTH, height = VGA_HEIGHT;
		int ret = ov772x_set_params(client, &width, &height,
					    V4L2_MBUS_FMT_YUYV8_2X8);
		if (ret < 0)
			return ret;
	}

	mf->width	= priv->win->width;
	mf->height	= priv->win->height;
	mf->code	= priv->cfmt->code;
	mf->colorspace	= priv->cfmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov772x_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov772x_priv *priv = container_of(sd, struct ov772x_priv, subdev);
	int ret = ov772x_set_params(client, &mf->width, &mf->height,
				    mf->code);

	if (!ret)
		mf->colorspace = priv->cfmt->colorspace;

	return ret;
}

static int ov772x_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	struct ov772x_priv *priv = container_of(sd, struct ov772x_priv, subdev);
	const struct ov772x_win_size *win;
	int i;

	win = ov772x_select_win(mf->width, mf->height);

	mf->width	= win->width;
	mf->height	= win->height;
	mf->field	= V4L2_FIELD_NONE;

	for (i = 0; i < ARRAY_SIZE(ov772x_cfmts); i++)
		if (mf->code == ov772x_cfmts[i].code)
			break;

	if (i == ARRAY_SIZE(ov772x_cfmts)) {
		
		if (priv->cfmt) {
			
			mf->colorspace = priv->cfmt->colorspace;
			mf->code = priv->cfmt->code;
		} else {
			
			mf->colorspace = ov772x_cfmts[0].colorspace;
			mf->code = ov772x_cfmts[0].code;
		}
	} else {
		
		mf->colorspace	= ov772x_cfmts[i].colorspace;
	}

	return 0;
}

static int ov772x_video_probe(struct i2c_client *client)
{
	struct ov772x_priv *priv = to_ov772x(client);
	u8                  pid, ver;
	const char         *devname;

	pid = i2c_smbus_read_byte_data(client, PID);
	ver = i2c_smbus_read_byte_data(client, VER);

	switch (VERSION(pid, ver)) {
	case OV7720:
		devname     = "ov7720";
		priv->model = V4L2_IDENT_OV7720;
		break;
	case OV7725:
		devname     = "ov7725";
		priv->model = V4L2_IDENT_OV7725;
		break;
	default:
		dev_err(&client->dev,
			"Product ID error %x:%x\n", pid, ver);
		return -ENODEV;
	}

	dev_info(&client->dev,
		 "%s Product ID %0x:%0x Manufacturer ID %x:%x\n",
		 devname,
		 pid,
		 ver,
		 i2c_smbus_read_byte_data(client, MIDH),
		 i2c_smbus_read_byte_data(client, MIDL));
	return v4l2_ctrl_handler_setup(&priv->hdl);
}

static const struct v4l2_ctrl_ops ov772x_ctrl_ops = {
	.s_ctrl = ov772x_s_ctrl,
};

static struct v4l2_subdev_core_ops ov772x_subdev_core_ops = {
	.g_chip_ident	= ov772x_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov772x_g_register,
	.s_register	= ov772x_s_register,
#endif
};

static int ov772x_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov772x_cfmts))
		return -EINVAL;

	*code = ov772x_cfmts[index].code;
	return 0;
}

static int ov772x_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_MASTER |
		V4L2_MBUS_VSYNC_ACTIVE_HIGH | V4L2_MBUS_HSYNC_ACTIVE_HIGH |
		V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = soc_camera_apply_board_flags(icl, cfg);

	return 0;
}

static struct v4l2_subdev_video_ops ov772x_subdev_video_ops = {
	.s_stream	= ov772x_s_stream,
	.g_mbus_fmt	= ov772x_g_fmt,
	.s_mbus_fmt	= ov772x_s_fmt,
	.try_mbus_fmt	= ov772x_try_fmt,
	.cropcap	= ov772x_cropcap,
	.g_crop		= ov772x_g_crop,
	.enum_mbus_fmt	= ov772x_enum_fmt,
	.g_mbus_config	= ov772x_g_mbus_config,
};

static struct v4l2_subdev_ops ov772x_subdev_ops = {
	.core	= &ov772x_subdev_core_ops,
	.video	= &ov772x_subdev_video_ops,
};


static int ov772x_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct ov772x_priv	*priv;
	struct soc_camera_link	*icl = soc_camera_i2c_to_link(client);
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int			ret;

	if (!icl || !icl->priv) {
		dev_err(&client->dev, "OV772X: missing platform data!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			"I2C-Adapter doesn't support "
			"I2C_FUNC_SMBUS_BYTE_DATA\n");
		return -EIO;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = icl->priv;

	v4l2_i2c_subdev_init(&priv->subdev, client, &ov772x_subdev_ops);
	v4l2_ctrl_handler_init(&priv->hdl, 3);
	v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
			V4L2_CID_BAND_STOP_FILTER, 0, 256, 1, 0);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		int err = priv->hdl.error;

		kfree(priv);
		return err;
	}

	ret = ov772x_video_probe(client);
	if (ret) {
		v4l2_ctrl_handler_free(&priv->hdl);
		kfree(priv);
	}

	return ret;
}

static int ov772x_remove(struct i2c_client *client)
{
	struct ov772x_priv *priv = to_ov772x(client);

	v4l2_device_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);
	kfree(priv);
	return 0;
}

static const struct i2c_device_id ov772x_id[] = {
	{ "ov772x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov772x_id);

static struct i2c_driver ov772x_i2c_driver = {
	.driver = {
		.name = "ov772x",
	},
	.probe    = ov772x_probe,
	.remove   = ov772x_remove,
	.id_table = ov772x_id,
};

module_i2c_driver(ov772x_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for ov772x");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
