/**
 * OV519 driver
 *
 * Copyright (C) 2008-2011 Jean-François Moine <moinejf@free.fr>
 * Copyright (C) 2009 Hans de Goede <hdegoede@redhat.com>
 *
 * This module is adapted from the ov51x-jpeg package, which itself
 * was adapted from the ov511 driver.
 *
 * Original copyright for the ov511 driver is:
 *
 * Copyright (c) 1999-2006 Mark W. McClelland
 * Support for OV519, OV8610 Copyright (c) 2003 Joerg Heckenbach
 * Many improvements by Bret Wallach <bwallac1@san.rr.com>
 * Color fixes by by Orion Sky Lawlor <olawlor@acm.org> (2/26/2000)
 * OV7620 fixes by Charl P. Botha <cpbotha@ieee.org>
 * Changes by Claudio Matsuoka <claudio@conectiva.com>
 *
 * ov51x-jpeg original copyright is:
 *
 * Copyright (c) 2004-2007 Romain Beauxis <toots@rastageeks.org>
 * Support for OV7670 sensors was contributed by Sam Skipsey <aoanla@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_NAME "ov519"

#include <linux/input.h>
#include "gspca.h"

#define CONEX_CAM
#include "jpeg.h"

MODULE_AUTHOR("Jean-Francois Moine <http://moinejf.free.fr>");
MODULE_DESCRIPTION("OV519 USB Camera Driver");
MODULE_LICENSE("GPL");

static int frame_rate;

static int i2c_detect_tries = 10;

enum e_ctrl {
	BRIGHTNESS,
	CONTRAST,
	EXPOSURE,
	COLORS,
	HFLIP,
	VFLIP,
	AUTOBRIGHT,
	AUTOGAIN,
	FREQ,
	NCTRL		
};

struct sd {
	struct gspca_dev gspca_dev;		

	struct gspca_ctrl ctrls[NCTRL];

	u8 packet_nr;

	char bridge;
#define BRIDGE_OV511		0
#define BRIDGE_OV511PLUS	1
#define BRIDGE_OV518		2
#define BRIDGE_OV518PLUS	3
#define BRIDGE_OV519		4		
#define BRIDGE_OVFX2		5
#define BRIDGE_W9968CF		6
#define BRIDGE_MASK		7

	char invert_led;
#define BRIDGE_INVERT_LED	8

	char snapshot_pressed;
	char snapshot_needs_reset;

	
	u8 sif;

	u8 quality;
#define QUALITY_MIN 50
#define QUALITY_MAX 70
#define QUALITY_DEF 50

	u8 stopped;		
	u8 first_frame;

	u8 frame_rate;		
	u8 clockdiv;		

	s8 sensor;		

	u8 sensor_addr;
	u16 sensor_width;
	u16 sensor_height;
	s16 sensor_reg_cache[256];

	u8 jpeg_hdr[JPEG_HDR_SZ];
};
enum sensors {
	SEN_OV2610,
	SEN_OV2610AE,
	SEN_OV3610,
	SEN_OV6620,
	SEN_OV6630,
	SEN_OV66308AF,
	SEN_OV7610,
	SEN_OV7620,
	SEN_OV7620AE,
	SEN_OV7640,
	SEN_OV7648,
	SEN_OV7660,
	SEN_OV7670,
	SEN_OV76BE,
	SEN_OV8610,
	SEN_OV9600,
};

#include "w996Xcf.c"

static void setbrightness(struct gspca_dev *gspca_dev);
static void setcontrast(struct gspca_dev *gspca_dev);
static void setexposure(struct gspca_dev *gspca_dev);
static void setcolors(struct gspca_dev *gspca_dev);
static void sethvflip(struct gspca_dev *gspca_dev);
static void setautobright(struct gspca_dev *gspca_dev);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static void setfreq(struct gspca_dev *gspca_dev);
static void setfreq_i(struct sd *sd);

static const struct ctrl sd_ctrls[] = {
[BRIGHTNESS] = {
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setbrightness,
	},
[CONTRAST] = {
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setcontrast,
	},
[EXPOSURE] = {
	    {
		.id      = V4L2_CID_EXPOSURE,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Exposure",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setexposure,
	},
[COLORS] = {
	    {
		.id      = V4L2_CID_SATURATION,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Color",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 127,
	    },
	    .set_control = setcolors,
	},
[HFLIP] = {
	    {
		.id      = V4L2_CID_HFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Mirror",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = sethvflip,
	},
[VFLIP] = {
	    {
		.id      = V4L2_CID_VFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Vflip",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = sethvflip,
	},
[AUTOBRIGHT] = {
	    {
		.id      = V4L2_CID_AUTOBRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Brightness",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 1,
	    },
	    .set_control = setautobright,
	},
[AUTOGAIN] = {
	    {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 1,
		.flags	 = V4L2_CTRL_FLAG_UPDATE
	    },
	    .set = sd_setautogain,
	},
[FREQ] = {
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	
		.step    = 1,
		.default_value = 0,
	    },
	    .set_control = setfreq,
	},
};

static const unsigned ctrl_dis[] = {
[SEN_OV2610] =		((1 << NCTRL) - 1)	
			^ ((1 << EXPOSURE)	
			 | (1 << AUTOGAIN)),	

[SEN_OV2610AE] =	((1 << NCTRL) - 1)	
			^ ((1 << EXPOSURE)	
			 | (1 << AUTOGAIN)),	

[SEN_OV3610] =		(1 << NCTRL) - 1,	

[SEN_OV6620] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV6630] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV66308AF] =	(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7610] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7620] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7620AE] =	(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7640] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << AUTOBRIGHT) |
			(1 << CONTRAST) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7648] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << AUTOBRIGHT) |
			(1 << CONTRAST) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7660] =		(1 << AUTOBRIGHT) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV7670] =		(1 << COLORS) |
			(1 << AUTOBRIGHT) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV76BE] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN),

[SEN_OV8610] =		(1 << HFLIP) |
			(1 << VFLIP) |
			(1 << EXPOSURE) |
			(1 << AUTOGAIN) |
			(1 << FREQ),
[SEN_OV9600] =		((1 << NCTRL) - 1)	
			^ ((1 << EXPOSURE)	
			 | (1 << AUTOGAIN)),	

};

static const struct v4l2_pix_format ov519_vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};
static const struct v4l2_pix_format ov519_sif_mode[] = {
	{160, 120, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 2},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format ov518_vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};
static const struct v4l2_pix_format ov518_sif_mode[] = {
	{160, 120, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 70000,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 70000,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{320, 240, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 2},
	{352, 288, V4L2_PIX_FMT_OV518, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format ov511_vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 2,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};
static const struct v4l2_pix_format ov511_sif_mode[] = {
	{160, 120, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 70000,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 70000,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{320, 240, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 2},
	{352, 288, V4L2_PIX_FMT_OV511, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format ovfx2_vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};
static const struct v4l2_pix_format ovfx2_cif_mode[] = {
	{160, 120, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{320, 240, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{352, 288, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};
static const struct v4l2_pix_format ovfx2_ov2610_mode[] = {
	{800, 600, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 800,
		.sizeimage = 800 * 600,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{1600, 1200, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 1600,
		.sizeimage = 1600 * 1200,
		.colorspace = V4L2_COLORSPACE_SRGB},
};
static const struct v4l2_pix_format ovfx2_ov3610_mode[] = {
	{640, 480, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{800, 600, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 800,
		.sizeimage = 800 * 600,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{1024, 768, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 1024,
		.sizeimage = 1024 * 768,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{1600, 1200, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 1600,
		.sizeimage = 1600 * 1200,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
	{2048, 1536, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 2048,
		.sizeimage = 2048 * 1536,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};
static const struct v4l2_pix_format ovfx2_ov9600_mode[] = {
	{640, 480, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{1280, 1024, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 1280,
		.sizeimage = 1280 * 1024,
		.colorspace = V4L2_COLORSPACE_SRGB},
};

#define R51x_FIFO_PSIZE			0x30	
#define R51x_SYS_RESET			0x50
	
	#define	OV511_RESET_OMNICE	0x08
#define R51x_SYS_INIT			0x53
#define R51x_SYS_SNAP			0x52
#define R51x_SYS_CUST_ID		0x5f
#define R51x_COMP_LUT_BEGIN		0x80

#define R511_CAM_DELAY			0x10
#define R511_CAM_EDGE			0x11
#define R511_CAM_PXCNT			0x12
#define R511_CAM_LNCNT			0x13
#define R511_CAM_PXDIV			0x14
#define R511_CAM_LNDIV			0x15
#define R511_CAM_UV_EN			0x16
#define R511_CAM_LINE_MODE		0x17
#define R511_CAM_OPTS			0x18

#define R511_SNAP_FRAME			0x19
#define R511_SNAP_PXCNT			0x1a
#define R511_SNAP_LNCNT			0x1b
#define R511_SNAP_PXDIV			0x1c
#define R511_SNAP_LNDIV			0x1d
#define R511_SNAP_UV_EN			0x1e
#define R511_SNAP_OPTS			0x1f

#define R511_DRAM_FLOW_CTL		0x20
#define R511_FIFO_OPTS			0x31
#define R511_I2C_CTL			0x40
#define R511_SYS_LED_CTL		0x55	
#define R511_COMP_EN			0x78
#define R511_COMP_LUT_EN		0x79

#define R518_GPIO_OUT			0x56	
#define R518_GPIO_CTL			0x57	

#define OV519_R10_H_SIZE		0x10
#define OV519_R11_V_SIZE		0x11
#define OV519_R12_X_OFFSETL		0x12
#define OV519_R13_X_OFFSETH		0x13
#define OV519_R14_Y_OFFSETL		0x14
#define OV519_R15_Y_OFFSETH		0x15
#define OV519_R16_DIVIDER		0x16
#define OV519_R20_DFR			0x20
#define OV519_R25_FORMAT		0x25

#define OV519_R51_RESET1		0x51
#define OV519_R54_EN_CLK1		0x54
#define OV519_R57_SNAPSHOT		0x57

#define OV519_GPIO_DATA_OUT0		0x71
#define OV519_GPIO_IO_CTRL0		0x72


#define OVFX2_BULK_SIZE (13 * 4096)

#define R51x_I2C_W_SID		0x41
#define R51x_I2C_SADDR_3	0x42
#define R51x_I2C_SADDR_2	0x43
#define R51x_I2C_R_SID		0x44
#define R51x_I2C_DATA		0x45
#define R518_I2C_CTL		0x47	
#define OVFX2_I2C_ADDR		0x00

#define OV7xx0_SID   0x42
#define OV_HIRES_SID 0x60		
#define OV8xx0_SID   0xa0
#define OV6xx0_SID   0xc0

#define OV7610_REG_GAIN		0x00	
#define OV7610_REG_BLUE		0x01	
#define OV7610_REG_RED		0x02	
#define OV7610_REG_SAT		0x03	
#define OV8610_REG_HUE		0x04	
#define OV7610_REG_CNT		0x05	
#define OV7610_REG_BRT		0x06	
#define OV7610_REG_COM_C	0x14	
#define OV7610_REG_ID_HIGH	0x1c	
#define OV7610_REG_ID_LOW	0x1d	
#define OV7610_REG_COM_I	0x29	

#define OV7670_R00_GAIN		0x00	
#define OV7670_R01_BLUE		0x01	
#define OV7670_R02_RED		0x02	
#define OV7670_R03_VREF		0x03	
#define OV7670_R04_COM1		0x04	
#define OV7670_R0C_COM3		0x0c	
#define OV7670_R0D_COM4		0x0d	
#define OV7670_R0E_COM5		0x0e	
#define OV7670_R0F_COM6		0x0f	
#define OV7670_R10_AECH		0x10	
#define OV7670_R11_CLKRC	0x11	
#define OV7670_R12_COM7		0x12	
#define   OV7670_COM7_FMT_VGA	 0x00
#define   OV7670_COM7_FMT_QVGA	 0x10	
#define   OV7670_COM7_FMT_MASK	 0x38
#define   OV7670_COM7_RESET	 0x80	
#define OV7670_R13_COM8		0x13	
#define   OV7670_COM8_AEC	 0x01	
#define   OV7670_COM8_AWB	 0x02	
#define   OV7670_COM8_AGC	 0x04	
#define   OV7670_COM8_BFILT	 0x20	
#define   OV7670_COM8_AECSTEP	 0x40	
#define   OV7670_COM8_FASTAEC	 0x80	
#define OV7670_R14_COM9		0x14	
#define OV7670_R15_COM10	0x15	
#define OV7670_R17_HSTART	0x17	
#define OV7670_R18_HSTOP	0x18	
#define OV7670_R19_VSTART	0x19	
#define OV7670_R1A_VSTOP	0x1a	
#define OV7670_R1E_MVFP		0x1e	
#define   OV7670_MVFP_VFLIP	 0x10	
#define   OV7670_MVFP_MIRROR	 0x20	
#define OV7670_R24_AEW		0x24	
#define OV7670_R25_AEB		0x25	
#define OV7670_R26_VPT		0x26	
#define OV7670_R32_HREF		0x32	
#define OV7670_R3A_TSLB		0x3a	
#define OV7670_R3B_COM11	0x3b	
#define   OV7670_COM11_EXP	 0x02
#define   OV7670_COM11_HZAUTO	 0x10	
#define OV7670_R3C_COM12	0x3c	
#define OV7670_R3D_COM13	0x3d	
#define   OV7670_COM13_GAMMA	 0x80	
#define   OV7670_COM13_UVSAT	 0x40	
#define OV7670_R3E_COM14	0x3e	
#define OV7670_R3F_EDGE		0x3f	
#define OV7670_R40_COM15	0x40	
#define OV7670_R41_COM16	0x41	
#define   OV7670_COM16_AWBGAIN	 0x08	
#define OV7670_R55_BRIGHT	0x55	
#define OV7670_R56_CONTRAS	0x56	
#define OV7670_R69_GFIX		0x69	
#define OV7670_R9F_HAECC1	0x9f	
#define OV7670_RA0_HAECC2	0xa0	
#define OV7670_RA5_BD50MAX	0xa5	
#define OV7670_RA6_HAECC3	0xa6	
#define OV7670_RA7_HAECC4	0xa7	
#define OV7670_RA8_HAECC5	0xa8	
#define OV7670_RA9_HAECC6	0xa9	
#define OV7670_RAA_HAECC7	0xaa	
#define OV7670_RAB_BD60MAX	0xab	

struct ov_regvals {
	u8 reg;
	u8 val;
};
struct ov_i2c_regvals {
	u8 reg;
	u8 val;
};

static const struct ov_i2c_regvals norm_2610[] = {
	{ 0x12, 0x80 },	
};

static const struct ov_i2c_regvals norm_2610ae[] = {
	{0x12, 0x80},	
	{0x13, 0xcd},
	{0x09, 0x01},
	{0x0d, 0x00},
	{0x11, 0x80},
	{0x12, 0x20},	
	{0x33, 0x0c},
	{0x35, 0x90},
	{0x36, 0x37},
	{0x11, 0x83},	
	{0x2d, 0x00},	
	{0x24, 0xb0},	
	{0x25, 0x90},
	{0x10, 0x43},
};

static const struct ov_i2c_regvals norm_3620b[] = {
	{ 0x12, 0x80 }, 
	{ 0x12, 0x00 }, 

	{ 0x11, 0x80 },

	{ 0x13, 0xc0 },

	{ 0x09, 0x08 },

	{ 0x0c, 0x08 },

	{ 0x0d, 0xa1 },

	{ 0x0e, 0x70 },

	{ 0x0f, 0x42 },

	{ 0x14, 0xc6 },

	{ 0x15, 0x02 },

	{ 0x33, 0x09 },

	{ 0x34, 0x50 },

	{ 0x36, 0x00 },

	{ 0x37, 0x04 },

	{ 0x38, 0x52 },

	{ 0x3a, 0x00 },

	{ 0x3c, 0x1f },

	{ 0x44, 0x00 },

	{ 0x40, 0x00 },

	{ 0x41, 0x00 },

	{ 0x42, 0x00 },

	{ 0x43, 0x00 },

	{ 0x45, 0x80 },

	{ 0x48, 0xc0 },

	{ 0x49, 0x19 },

	{ 0x4b, 0x80 },

	{ 0x4d, 0xc4 },

	{ 0x35, 0x4c },

	{ 0x3d, 0x00 },

	{ 0x3e, 0x00 },

	{ 0x3b, 0x18 },

	{ 0x33, 0x19 },

	{ 0x34, 0x5a },

	{ 0x3b, 0x00 },

	{ 0x33, 0x09 },

	{ 0x34, 0x50 },

	{ 0x12, 0x40 },

	{ 0x17, 0x1f },

	{ 0x18, 0x5f },

	{ 0x19, 0x00 },

	{ 0x1a, 0x60 },

	{ 0x32, 0x12 },

	{ 0x03, 0x4a },

	{ 0x11, 0x80 },

	{ 0x12, 0x00 },

	{ 0x12, 0x40 },

	{ 0x17, 0x1f },

	{ 0x18, 0x5f },

	{ 0x19, 0x00 },

	{ 0x1a, 0x60 },

	{ 0x32, 0x12 },

	{ 0x03, 0x4a },

	{ 0x02, 0xaf },

	{ 0x2d, 0xd2 },

	{ 0x00, 0x18 },

	{ 0x01, 0xf0 },

	{ 0x10, 0x0a },

	{ 0xe1, 0x67 },
	{ 0xe3, 0x03 },
	{ 0xe4, 0x26 },
	{ 0xe5, 0x3e },
	{ 0xf8, 0x01 },
	{ 0xff, 0x01 },
};

static const struct ov_i2c_regvals norm_6x20[] = {
	{ 0x12, 0x80 }, 
	{ 0x11, 0x01 },
	{ 0x03, 0x60 },
	{ 0x05, 0x7f }, 
	{ 0x07, 0xa8 },
	
	{ 0x0c, 0x24 },
	{ 0x0d, 0x24 },
	{ 0x0f, 0x15 }, 
	{ 0x10, 0x75 }, 
	{ 0x12, 0x24 }, 
	{ 0x14, 0x04 },
	
	{ 0x16, 0x06 },
	{ 0x26, 0xb2 }, 
	
	{ 0x28, 0x05 },
	{ 0x2a, 0x04 }, 
	{ 0x2d, 0x85 },
	{ 0x33, 0xa0 }, 
	{ 0x34, 0xd2 }, 
	{ 0x38, 0x8b },
	{ 0x39, 0x40 },

	{ 0x3c, 0x39 }, 
	{ 0x3c, 0x3c }, 
	{ 0x3c, 0x24 }, 

	{ 0x3d, 0x80 },
	{ 0x4a, 0x80 },
	{ 0x4b, 0x80 },
	{ 0x4d, 0xd2 }, 
	{ 0x4e, 0xc1 },
	{ 0x4f, 0x04 },
};

static const struct ov_i2c_regvals norm_6x30[] = {
	{ 0x12, 0x80 }, 
	{ 0x00, 0x1f }, 
	{ 0x01, 0x99 }, 
	{ 0x02, 0x7c }, 
	{ 0x03, 0xc0 }, 
	{ 0x05, 0x0a }, 
	{ 0x06, 0x95 }, 
	{ 0x07, 0x2d }, 
	{ 0x0c, 0x20 },
	{ 0x0d, 0x20 },
	{ 0x0e, 0xa0 }, 
	{ 0x0f, 0x05 },
	{ 0x10, 0x9a },
	{ 0x11, 0x00 }, 
	{ 0x12, 0x24 }, 
	{ 0x13, 0x21 },
	{ 0x14, 0x80 },
	{ 0x15, 0x01 },
	{ 0x16, 0x03 },
	{ 0x17, 0x38 },
	{ 0x18, 0xea },
	{ 0x19, 0x04 },
	{ 0x1a, 0x93 },
	{ 0x1b, 0x00 },
	{ 0x1e, 0xc4 },
	{ 0x1f, 0x04 },
	{ 0x20, 0x20 },
	{ 0x21, 0x10 },
	{ 0x22, 0x88 },
	{ 0x23, 0xc0 }, 
	{ 0x25, 0x9a }, 
	{ 0x26, 0xb2 }, 
	{ 0x27, 0xa2 },
	{ 0x28, 0x00 },
	{ 0x29, 0x00 },
	{ 0x2a, 0x84 }, 
	{ 0x2b, 0xa8 }, 
	{ 0x2c, 0xa0 },
	{ 0x2d, 0x95 }, 
	{ 0x2e, 0x88 },
	{ 0x33, 0x26 },
	{ 0x34, 0x03 },
	{ 0x36, 0x8f },
	{ 0x37, 0x80 },
	{ 0x38, 0x83 },
	{ 0x39, 0x80 },
	{ 0x3a, 0x0f },
	{ 0x3b, 0x3c },
	{ 0x3c, 0x1a },
	{ 0x3d, 0x80 },
	{ 0x3e, 0x80 },
	{ 0x3f, 0x0e },
	{ 0x40, 0x00 }, 
	{ 0x41, 0x00 }, 
	{ 0x42, 0x80 },
	{ 0x43, 0x3f }, 
	{ 0x44, 0x80 },
	{ 0x45, 0x20 },
	{ 0x46, 0x20 },
	{ 0x47, 0x80 },
	{ 0x48, 0x7f },
	{ 0x49, 0x00 },
	{ 0x4a, 0x00 },
	{ 0x4b, 0x80 },
	{ 0x4c, 0xd0 },
	{ 0x4d, 0x10 }, 
	{ 0x4e, 0x40 },
	{ 0x4f, 0x07 }, 
	{ 0x50, 0xff },
	{ 0x54, 0x23 }, 
	{ 0x55, 0xff },
	{ 0x56, 0x12 },
	{ 0x57, 0x81 },
	{ 0x58, 0x75 },
	{ 0x59, 0x01 }, 
	{ 0x5a, 0x2c },
	{ 0x5b, 0x0f }, 
	{ 0x5c, 0x10 },
	{ 0x3d, 0x80 },
	{ 0x27, 0xa6 },
	{ 0x12, 0x20 }, 
	{ 0x12, 0x24 },
};

static const struct ov_i2c_regvals norm_7610[] = {
	{ 0x10, 0xff },
	{ 0x16, 0x06 },
	{ 0x28, 0x24 },
	{ 0x2b, 0xac },
	{ 0x12, 0x00 },
	{ 0x38, 0x81 },
	{ 0x28, 0x24 },	
	{ 0x0f, 0x85 },	
	{ 0x15, 0x01 },
	{ 0x20, 0x1c },
	{ 0x23, 0x2a },
	{ 0x24, 0x10 },
	{ 0x25, 0x8a },
	{ 0x26, 0xa2 },
	{ 0x27, 0xc2 },
	{ 0x2a, 0x04 },
	{ 0x2c, 0xfe },
	{ 0x2d, 0x93 },
	{ 0x30, 0x71 },
	{ 0x31, 0x60 },
	{ 0x32, 0x26 },
	{ 0x33, 0x20 },
	{ 0x34, 0x48 },
	{ 0x12, 0x24 },
	{ 0x11, 0x01 },
	{ 0x0c, 0x24 },
	{ 0x0d, 0x24 },
};

static const struct ov_i2c_regvals norm_7620[] = {
	{ 0x12, 0x80 },		
	{ 0x00, 0x00 },		
	{ 0x01, 0x80 },		
	{ 0x02, 0x80 },		
	{ 0x03, 0xc0 },		
	{ 0x06, 0x60 },
	{ 0x07, 0x00 },
	{ 0x0c, 0x24 },
	{ 0x0c, 0x24 },
	{ 0x0d, 0x24 },
	{ 0x11, 0x01 },
	{ 0x12, 0x24 },
	{ 0x13, 0x01 },
	{ 0x14, 0x84 },
	{ 0x15, 0x01 },
	{ 0x16, 0x03 },
	{ 0x17, 0x2f },
	{ 0x18, 0xcf },
	{ 0x19, 0x06 },
	{ 0x1a, 0xf5 },
	{ 0x1b, 0x00 },
	{ 0x20, 0x18 },
	{ 0x21, 0x80 },
	{ 0x22, 0x80 },
	{ 0x23, 0x00 },
	{ 0x26, 0xa2 },
	{ 0x27, 0xea },
	{ 0x28, 0x22 }, 
	{ 0x29, 0x00 },
	{ 0x2a, 0x10 },
	{ 0x2b, 0x00 },
	{ 0x2c, 0x88 },
	{ 0x2d, 0x91 },
	{ 0x2e, 0x80 },
	{ 0x2f, 0x44 },
	{ 0x60, 0x27 },
	{ 0x61, 0x02 },
	{ 0x62, 0x5f },
	{ 0x63, 0xd5 },
	{ 0x64, 0x57 },
	{ 0x65, 0x83 },
	{ 0x66, 0x55 },
	{ 0x67, 0x92 },
	{ 0x68, 0xcf },
	{ 0x69, 0x76 },
	{ 0x6a, 0x22 },
	{ 0x6b, 0x00 },
	{ 0x6c, 0x02 },
	{ 0x6d, 0x44 },
	{ 0x6e, 0x80 },
	{ 0x6f, 0x1d },
	{ 0x70, 0x8b },
	{ 0x71, 0x00 },
	{ 0x72, 0x14 },
	{ 0x73, 0x54 },
	{ 0x74, 0x00 },
	{ 0x75, 0x8e },
	{ 0x76, 0x00 },
	{ 0x77, 0xff },
	{ 0x78, 0x80 },
	{ 0x79, 0x80 },
	{ 0x7a, 0x80 },
	{ 0x7b, 0xe2 },
	{ 0x7c, 0x00 },
};

static const struct ov_i2c_regvals norm_7640[] = {
	{ 0x12, 0x80 },
	{ 0x12, 0x14 },
};

static const struct ov_regvals init_519_ov7660[] = {
	{ 0x5d,	0x03 }, 
	{ 0x53,	0x9b }, 
	{ 0x54,	0x0f }, 
	{ 0xa2,	0x20 }, 
	{ 0xa3,	0x18 },
	{ 0xa4,	0x04 },
	{ 0xa5,	0x28 },
	{ 0x37,	0x00 },	
	{ 0x55,	0x02 }, 
	
	{ 0x20,	0x0c },	
	{ 0x21,	0x38 },
	{ 0x22,	0x1d },
	{ 0x17,	0x50 }, 
	{ 0x37,	0x00 }, 
	{ 0x40,	0xff }, 
	{ 0x46,	0x00 }, 
};
static const struct ov_i2c_regvals norm_7660[] = {
	{OV7670_R12_COM7, OV7670_COM7_RESET},
	{OV7670_R11_CLKRC, 0x81},
	{0x92, 0x00},			
	{0x93, 0x00},			
	{0x9d, 0x4c},			
	{0x9e, 0x3f},			
	{OV7670_R3B_COM11, 0x02},
	{OV7670_R13_COM8, 0xf5},
	{OV7670_R10_AECH, 0x00},
	{OV7670_R00_GAIN, 0x00},
	{OV7670_R01_BLUE, 0x7c},
	{OV7670_R02_RED, 0x9d},
	{OV7670_R12_COM7, 0x00},
	{OV7670_R04_COM1, 00},
	{OV7670_R18_HSTOP, 0x01},
	{OV7670_R17_HSTART, 0x13},
	{OV7670_R32_HREF, 0x92},
	{OV7670_R19_VSTART, 0x02},
	{OV7670_R1A_VSTOP, 0x7a},
	{OV7670_R03_VREF, 0x00},
	{OV7670_R0E_COM5, 0x04},
	{OV7670_R0F_COM6, 0x62},
	{OV7670_R15_COM10, 0x00},
	{0x16, 0x02},			
	{0x1b, 0x00},			
	{OV7670_R1E_MVFP, 0x01},
	{0x29, 0x3c},			
	{0x33, 0x00},			
	{0x34, 0x07},			
	{0x35, 0x84},			
	{0x36, 0x00},			
	{0x37, 0x04},			
	{0x39, 0x43},			
	{OV7670_R3A_TSLB, 0x00},
	{OV7670_R3C_COM12, 0x6c},
	{OV7670_R3D_COM13, 0x98},
	{OV7670_R3F_EDGE, 0x23},
	{OV7670_R40_COM15, 0xc1},
	{OV7670_R41_COM16, 0x22},
	{0x6b, 0x0a},			
	{0xa1, 0x08},			
	{0x69, 0x80},			
	{0x43, 0xf0},			
	{0x44, 0x10},
	{0x45, 0x78},
	{0x46, 0xa8},
	{0x47, 0x60},
	{0x48, 0x80},
	{0x59, 0xba},
	{0x5a, 0x9a},
	{0x5b, 0x22},
	{0x5c, 0xb9},
	{0x5d, 0x9b},
	{0x5e, 0x10},
	{0x5f, 0xe0},
	{0x60, 0x85},
	{0x61, 0x60},
	{0x9f, 0x9d},			
	{0xa0, 0xa0},			
	{0x4f, 0x60},			
	{0x50, 0x64},
	{0x51, 0x04},
	{0x52, 0x18},
	{0x53, 0x3c},
	{0x54, 0x54},
	{0x55, 0x40},
	{0x56, 0x40},
	{0x57, 0x40},
	{0x58, 0x0d},			
	{0x8b, 0xcc},			
	{0x8c, 0xcc},
	{0x8d, 0xcf},
	{0x6c, 0x40},			
	{0x6d, 0xe0},
	{0x6e, 0xa0},
	{0x6f, 0x80},
	{0x70, 0x70},
	{0x71, 0x80},
	{0x72, 0x60},
	{0x73, 0x60},
	{0x74, 0x50},
	{0x75, 0x40},
	{0x76, 0x38},
	{0x77, 0x3c},
	{0x78, 0x32},
	{0x79, 0x1a},
	{0x7a, 0x28},
	{0x7b, 0x24},
	{0x7c, 0x04},			
	{0x7d, 0x12},
	{0x7e, 0x26},
	{0x7f, 0x46},
	{0x80, 0x54},
	{0x81, 0x64},
	{0x82, 0x70},
	{0x83, 0x7c},
	{0x84, 0x86},
	{0x85, 0x8e},
	{0x86, 0x9c},
	{0x87, 0xab},
	{0x88, 0xc4},
	{0x89, 0xd1},
	{0x8a, 0xe5},
	{OV7670_R14_COM9, 0x1e},
	{OV7670_R24_AEW, 0x80},
	{OV7670_R25_AEB, 0x72},
	{OV7670_R26_VPT, 0xb3},
	{0x62, 0x80},			
	{0x63, 0x80},			
	{0x64, 0x06},			
	{0x65, 0x00},			
	{0x66, 0x01},			
	{0x94, 0x0e},			
	{0x95, 0x14},
	{OV7670_R13_COM8, OV7670_COM8_FASTAEC
			| OV7670_COM8_AECSTEP
			| OV7670_COM8_BFILT
			| 0x10
			| OV7670_COM8_AGC
			| OV7670_COM8_AWB
			| OV7670_COM8_AEC},
	{0xa1, 0xc8}
};
static const struct ov_i2c_regvals norm_9600[] = {
	{0x12, 0x80},
	{0x0c, 0x28},
	{0x11, 0x80},
	{0x13, 0xb5},
	{0x14, 0x3e},
	{0x1b, 0x04},
	{0x24, 0xb0},
	{0x25, 0x90},
	{0x26, 0x94},
	{0x35, 0x90},
	{0x37, 0x07},
	{0x38, 0x08},
	{0x01, 0x8e},
	{0x02, 0x85}
};

static const struct ov_i2c_regvals norm_7670[] = {
	{ OV7670_R12_COM7, OV7670_COM7_RESET },
	{ OV7670_R3A_TSLB, 0x04 },		
	{ OV7670_R12_COM7, OV7670_COM7_FMT_VGA }, 
	{ OV7670_R11_CLKRC, 0x01 },
	{ OV7670_R17_HSTART, 0x13 },
	{ OV7670_R18_HSTOP, 0x01 },
	{ OV7670_R32_HREF, 0xb6 },
	{ OV7670_R19_VSTART, 0x02 },
	{ OV7670_R1A_VSTOP, 0x7a },
	{ OV7670_R03_VREF, 0x0a },

	{ OV7670_R0C_COM3, 0x00 },
	{ OV7670_R3E_COM14, 0x00 },
	{ 0x70, 0x3a },
	{ 0x71, 0x35 },
	{ 0x72, 0x11 },
	{ 0x73, 0xf0 },
	{ 0xa2, 0x02 },

	{ 0x7a, 0x20 },
	{ 0x7b, 0x10 },
	{ 0x7c, 0x1e },
	{ 0x7d, 0x35 },
	{ 0x7e, 0x5a },
	{ 0x7f, 0x69 },
	{ 0x80, 0x76 },
	{ 0x81, 0x80 },
	{ 0x82, 0x88 },
	{ 0x83, 0x8f },
	{ 0x84, 0x96 },
	{ 0x85, 0xa3 },
	{ 0x86, 0xaf },
	{ 0x87, 0xc4 },
	{ 0x88, 0xd7 },
	{ 0x89, 0xe8 },

	{ OV7670_R13_COM8, OV7670_COM8_FASTAEC
			 | OV7670_COM8_AECSTEP
			 | OV7670_COM8_BFILT },
	{ OV7670_R00_GAIN, 0x00 },
	{ OV7670_R10_AECH, 0x00 },
	{ OV7670_R0D_COM4, 0x40 }, 
	{ OV7670_R14_COM9, 0x18 }, 
	{ OV7670_RA5_BD50MAX, 0x05 },
	{ OV7670_RAB_BD60MAX, 0x07 },
	{ OV7670_R24_AEW, 0x95 },
	{ OV7670_R25_AEB, 0x33 },
	{ OV7670_R26_VPT, 0xe3 },
	{ OV7670_R9F_HAECC1, 0x78 },
	{ OV7670_RA0_HAECC2, 0x68 },
	{ 0xa1, 0x03 }, 
	{ OV7670_RA6_HAECC3, 0xd8 },
	{ OV7670_RA7_HAECC4, 0xd8 },
	{ OV7670_RA8_HAECC5, 0xf0 },
	{ OV7670_RA9_HAECC6, 0x90 },
	{ OV7670_RAA_HAECC7, 0x94 },
	{ OV7670_R13_COM8, OV7670_COM8_FASTAEC
			| OV7670_COM8_AECSTEP
			| OV7670_COM8_BFILT
			| OV7670_COM8_AGC
			| OV7670_COM8_AEC },

	{ OV7670_R0E_COM5, 0x61 },
	{ OV7670_R0F_COM6, 0x4b },
	{ 0x16, 0x02 },
	{ OV7670_R1E_MVFP, 0x07 },
	{ 0x21, 0x02 },
	{ 0x22, 0x91 },
	{ 0x29, 0x07 },
	{ 0x33, 0x0b },
	{ 0x35, 0x0b },
	{ 0x37, 0x1d },
	{ 0x38, 0x71 },
	{ 0x39, 0x2a },
	{ OV7670_R3C_COM12, 0x78 },
	{ 0x4d, 0x40 },
	{ 0x4e, 0x20 },
	{ OV7670_R69_GFIX, 0x00 },
	{ 0x6b, 0x4a },
	{ 0x74, 0x10 },
	{ 0x8d, 0x4f },
	{ 0x8e, 0x00 },
	{ 0x8f, 0x00 },
	{ 0x90, 0x00 },
	{ 0x91, 0x00 },
	{ 0x96, 0x00 },
	{ 0x9a, 0x00 },
	{ 0xb0, 0x84 },
	{ 0xb1, 0x0c },
	{ 0xb2, 0x0e },
	{ 0xb3, 0x82 },
	{ 0xb8, 0x0a },

	{ 0x43, 0x0a },
	{ 0x44, 0xf0 },
	{ 0x45, 0x34 },
	{ 0x46, 0x58 },
	{ 0x47, 0x28 },
	{ 0x48, 0x3a },
	{ 0x59, 0x88 },
	{ 0x5a, 0x88 },
	{ 0x5b, 0x44 },
	{ 0x5c, 0x67 },
	{ 0x5d, 0x49 },
	{ 0x5e, 0x0e },
	{ 0x6c, 0x0a },
	{ 0x6d, 0x55 },
	{ 0x6e, 0x11 },
	{ 0x6f, 0x9f },			
	{ 0x6a, 0x40 },
	{ OV7670_R01_BLUE, 0x40 },
	{ OV7670_R02_RED, 0x60 },
	{ OV7670_R13_COM8, OV7670_COM8_FASTAEC
			| OV7670_COM8_AECSTEP
			| OV7670_COM8_BFILT
			| OV7670_COM8_AGC
			| OV7670_COM8_AEC
			| OV7670_COM8_AWB },

	{ 0x4f, 0x80 },
	{ 0x50, 0x80 },
	{ 0x51, 0x00 },
	{ 0x52, 0x22 },
	{ 0x53, 0x5e },
	{ 0x54, 0x80 },
	{ 0x58, 0x9e },

	{ OV7670_R41_COM16, OV7670_COM16_AWBGAIN },
	{ OV7670_R3F_EDGE, 0x00 },
	{ 0x75, 0x05 },
	{ 0x76, 0xe1 },
	{ 0x4c, 0x00 },
	{ 0x77, 0x01 },
	{ OV7670_R3D_COM13, OV7670_COM13_GAMMA
			  | OV7670_COM13_UVSAT
			  | 2},		
	{ 0x4b, 0x09 },
	{ 0xc9, 0x60 },
	{ OV7670_R41_COM16, 0x38 },
	{ 0x56, 0x40 },

	{ 0x34, 0x11 },
	{ OV7670_R3B_COM11, OV7670_COM11_EXP|OV7670_COM11_HZAUTO },
	{ 0xa4, 0x88 },
	{ 0x96, 0x00 },
	{ 0x97, 0x30 },
	{ 0x98, 0x20 },
	{ 0x99, 0x30 },
	{ 0x9a, 0x84 },
	{ 0x9b, 0x29 },
	{ 0x9c, 0x03 },
	{ 0x9d, 0x4c },
	{ 0x9e, 0x3f },
	{ 0x78, 0x04 },

	{ 0x79, 0x01 },
	{ 0xc8, 0xf0 },
	{ 0x79, 0x0f },
	{ 0xc8, 0x00 },
	{ 0x79, 0x10 },
	{ 0xc8, 0x7e },
	{ 0x79, 0x0a },
	{ 0xc8, 0x80 },
	{ 0x79, 0x0b },
	{ 0xc8, 0x01 },
	{ 0x79, 0x0c },
	{ 0xc8, 0x0f },
	{ 0x79, 0x0d },
	{ 0xc8, 0x20 },
	{ 0x79, 0x09 },
	{ 0xc8, 0x80 },
	{ 0x79, 0x02 },
	{ 0xc8, 0xc0 },
	{ 0x79, 0x03 },
	{ 0xc8, 0x40 },
	{ 0x79, 0x05 },
	{ 0xc8, 0x30 },
	{ 0x79, 0x26 },
};

static const struct ov_i2c_regvals norm_8610[] = {
	{ 0x12, 0x80 },
	{ 0x00, 0x00 },
	{ 0x01, 0x80 },
	{ 0x02, 0x80 },
	{ 0x03, 0xc0 },
	{ 0x04, 0x30 },
	{ 0x05, 0x30 }, 
	{ 0x06, 0x70 }, 
	{ 0x0a, 0x86 },
	{ 0x0b, 0xb0 },
	{ 0x0c, 0x20 },
	{ 0x0d, 0x20 },
	{ 0x11, 0x01 },
	{ 0x12, 0x25 },
	{ 0x13, 0x01 },
	{ 0x14, 0x04 },
	{ 0x15, 0x01 }, 
	{ 0x16, 0x03 },
	{ 0x17, 0x38 }, 
	{ 0x18, 0xea }, 
	{ 0x19, 0x02 }, 
	{ 0x1a, 0xf5 },
	{ 0x1b, 0x00 },
	{ 0x20, 0xd0 }, 
	{ 0x23, 0xc0 }, 
	{ 0x24, 0x30 }, 
	{ 0x25, 0x50 }, 
	{ 0x26, 0xa2 },
	{ 0x27, 0xea },
	{ 0x28, 0x00 },
	{ 0x29, 0x00 },
	{ 0x2a, 0x80 },
	{ 0x2b, 0xc8 }, 
	{ 0x2c, 0xac },
	{ 0x2d, 0x45 }, 
	{ 0x2e, 0x80 },
	{ 0x2f, 0x14 }, 
	{ 0x4c, 0x00 },
	{ 0x4d, 0x30 }, 
	{ 0x60, 0x02 }, 
	{ 0x61, 0x00 }, 
	{ 0x62, 0x5f }, 
	{ 0x63, 0xff },
	{ 0x64, 0x53 }, 
	{ 0x65, 0x00 },
	{ 0x66, 0x55 },
	{ 0x67, 0xb0 },
	{ 0x68, 0xc0 }, 
	{ 0x69, 0x02 },
	{ 0x6a, 0x22 },
	{ 0x6b, 0x00 },
	{ 0x6c, 0x99 }, 
	{ 0x6d, 0x11 }, 
	{ 0x6e, 0x11 }, 
	{ 0x6f, 0x01 },
	{ 0x70, 0x8b },
	{ 0x71, 0x00 },
	{ 0x72, 0x14 },
	{ 0x73, 0x54 },
	{ 0x74, 0x00 },
	{ 0x75, 0x0e },
	{ 0x76, 0x02 }, 
	{ 0x77, 0xff },
	{ 0x78, 0x80 },
	{ 0x79, 0x80 },
	{ 0x7a, 0x80 },
	{ 0x7b, 0x10 }, 
	{ 0x7c, 0x00 },
	{ 0x7d, 0x08 }, 
	{ 0x7e, 0x08 }, 
	{ 0x7f, 0xfb },
	{ 0x80, 0x28 },
	{ 0x81, 0x00 },
	{ 0x82, 0x23 },
	{ 0x83, 0x0b },
	{ 0x84, 0x00 },
	{ 0x85, 0x62 }, 
	{ 0x86, 0xc9 },
	{ 0x87, 0x00 },
	{ 0x88, 0x00 },
	{ 0x89, 0x01 },
	{ 0x12, 0x20 },
	{ 0x12, 0x25 }, 
};

static unsigned char ov7670_abs_to_sm(unsigned char v)
{
	if (v > 127)
		return v & 0x7f;
	return (128 - v) | 0x80;
}

static void reg_w(struct sd *sd, u16 index, u16 value)
{
	int ret, req = 0;

	if (sd->gspca_dev.usb_err < 0)
		return;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		req = 2;
		break;
	case BRIDGE_OVFX2:
		req = 0x0a;
		
	case BRIDGE_W9968CF:
		PDEBUG(D_USBO, "SET %02x %04x %04x",
				req, value, index);
		ret = usb_control_msg(sd->gspca_dev.dev,
			usb_sndctrlpipe(sd->gspca_dev.dev, 0),
			req,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, NULL, 0, 500);
		goto leave;
	default:
		req = 1;
	}

	PDEBUG(D_USBO, "SET %02x 0000 %04x %02x",
			req, index, value);
	sd->gspca_dev.usb_buf[0] = value;
	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_sndctrlpipe(sd->gspca_dev.dev, 0),
			req,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index,
			sd->gspca_dev.usb_buf, 1, 500);
leave:
	if (ret < 0) {
		pr_err("reg_w %02x failed %d\n", index, ret);
		sd->gspca_dev.usb_err = ret;
		return;
	}
}

static int reg_r(struct sd *sd, u16 index)
{
	int ret;
	int req;

	if (sd->gspca_dev.usb_err < 0)
		return -1;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		req = 3;
		break;
	case BRIDGE_OVFX2:
		req = 0x0b;
		break;
	default:
		req = 1;
	}

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_rcvctrlpipe(sd->gspca_dev.dev, 0),
			req,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index, sd->gspca_dev.usb_buf, 1, 500);

	if (ret >= 0) {
		ret = sd->gspca_dev.usb_buf[0];
		PDEBUG(D_USBI, "GET %02x 0000 %04x %02x",
			req, index, ret);
	} else {
		pr_err("reg_r %02x failed %d\n", index, ret);
		sd->gspca_dev.usb_err = ret;
	}

	return ret;
}

static int reg_r8(struct sd *sd,
		  u16 index)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return -1;

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_rcvctrlpipe(sd->gspca_dev.dev, 0),
			1,			
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index, sd->gspca_dev.usb_buf, 8, 500);

	if (ret >= 0) {
		ret = sd->gspca_dev.usb_buf[0];
	} else {
		pr_err("reg_r8 %02x failed %d\n", index, ret);
		sd->gspca_dev.usb_err = ret;
	}

	return ret;
}

static void reg_w_mask(struct sd *sd,
			u16 index,
			u8 value,
			u8 mask)
{
	int ret;
	u8 oldval;

	if (mask != 0xff) {
		value &= mask;			
		ret = reg_r(sd, index);
		if (ret < 0)
			return;

		oldval = ret & ~mask;		
		value |= oldval;		
	}
	reg_w(sd, index, value);
}

static void ov518_reg_w32(struct sd *sd, u16 index, u32 value, int n)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return;

	*((__le32 *) sd->gspca_dev.usb_buf) = __cpu_to_le32(value);

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_sndctrlpipe(sd->gspca_dev.dev, 0),
			1 ,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index,
			sd->gspca_dev.usb_buf, n, 500);
	if (ret < 0) {
		pr_err("reg_w32 %02x failed %d\n", index, ret);
		sd->gspca_dev.usb_err = ret;
	}
}

static void ov511_i2c_w(struct sd *sd, u8 reg, u8 value)
{
	int rc, retries;

	PDEBUG(D_USBO, "ov511_i2c_w %02x %02x", reg, value);

	
	for (retries = 6; ; ) {
		
		reg_w(sd, R51x_I2C_SADDR_3, reg);

		
		reg_w(sd, R51x_I2C_DATA, value);

		
		reg_w(sd, R511_I2C_CTL, 0x01);

		do {
			rc = reg_r(sd, R511_I2C_CTL);
		} while (rc > 0 && ((rc & 1) == 0)); 

		if (rc < 0)
			return;

		if ((rc & 2) == 0) 
			break;
		if (--retries < 0) {
			PDEBUG(D_USBO, "i2c write retries exhausted");
			return;
		}
	}
}

static int ov511_i2c_r(struct sd *sd, u8 reg)
{
	int rc, value, retries;

	
	for (retries = 6; ; ) {
		
		reg_w(sd, R51x_I2C_SADDR_2, reg);

		
		reg_w(sd, R511_I2C_CTL, 0x03);

		do {
			rc = reg_r(sd, R511_I2C_CTL);
		} while (rc > 0 && ((rc & 1) == 0)); 

		if (rc < 0)
			return rc;

		if ((rc & 2) == 0) 
			break;

		
		reg_w(sd, R511_I2C_CTL, 0x10);

		if (--retries < 0) {
			PDEBUG(D_USBI, "i2c write retries exhausted");
			return -1;
		}
	}

	
	for (retries = 6; ; ) {
		
		reg_w(sd, R511_I2C_CTL, 0x05);

		do {
			rc = reg_r(sd, R511_I2C_CTL);
		} while (rc > 0 && ((rc & 1) == 0)); 

		if (rc < 0)
			return rc;

		if ((rc & 2) == 0) 
			break;

		
		reg_w(sd, R511_I2C_CTL, 0x10);

		if (--retries < 0) {
			PDEBUG(D_USBI, "i2c read retries exhausted");
			return -1;
		}
	}

	value = reg_r(sd, R51x_I2C_DATA);

	PDEBUG(D_USBI, "ov511_i2c_r %02x %02x", reg, value);

	
	reg_w(sd, R511_I2C_CTL, 0x05);

	return value;
}

static void ov518_i2c_w(struct sd *sd,
		u8 reg,
		u8 value)
{
	PDEBUG(D_USBO, "ov518_i2c_w %02x %02x", reg, value);

	
	reg_w(sd, R51x_I2C_SADDR_3, reg);

	
	reg_w(sd, R51x_I2C_DATA, value);

	
	reg_w(sd, R518_I2C_CTL, 0x01);

	
	msleep(4);
	reg_r8(sd, R518_I2C_CTL);
}

static int ov518_i2c_r(struct sd *sd, u8 reg)
{
	int value;

	
	reg_w(sd, R51x_I2C_SADDR_2, reg);

	
	reg_w(sd, R518_I2C_CTL, 0x03);
	reg_r8(sd, R518_I2C_CTL);

	
	reg_w(sd, R518_I2C_CTL, 0x05);
	reg_r8(sd, R518_I2C_CTL);

	value = reg_r(sd, R51x_I2C_DATA);
	PDEBUG(D_USBI, "ov518_i2c_r %02x %02x", reg, value);
	return value;
}

static void ovfx2_i2c_w(struct sd *sd, u8 reg, u8 value)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return;

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_sndctrlpipe(sd->gspca_dev.dev, 0),
			0x02,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			(u16) value, (u16) reg, NULL, 0, 500);

	if (ret < 0) {
		pr_err("ovfx2_i2c_w %02x failed %d\n", reg, ret);
		sd->gspca_dev.usb_err = ret;
	}

	PDEBUG(D_USBO, "ovfx2_i2c_w %02x %02x", reg, value);
}

static int ovfx2_i2c_r(struct sd *sd, u8 reg)
{
	int ret;

	if (sd->gspca_dev.usb_err < 0)
		return -1;

	ret = usb_control_msg(sd->gspca_dev.dev,
			usb_rcvctrlpipe(sd->gspca_dev.dev, 0),
			0x03,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, (u16) reg, sd->gspca_dev.usb_buf, 1, 500);

	if (ret >= 0) {
		ret = sd->gspca_dev.usb_buf[0];
		PDEBUG(D_USBI, "ovfx2_i2c_r %02x %02x", reg, ret);
	} else {
		pr_err("ovfx2_i2c_r %02x failed %d\n", reg, ret);
		sd->gspca_dev.usb_err = ret;
	}

	return ret;
}

static void i2c_w(struct sd *sd, u8 reg, u8 value)
{
	if (sd->sensor_reg_cache[reg] == value)
		return;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		ov511_i2c_w(sd, reg, value);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
	case BRIDGE_OV519:
		ov518_i2c_w(sd, reg, value);
		break;
	case BRIDGE_OVFX2:
		ovfx2_i2c_w(sd, reg, value);
		break;
	case BRIDGE_W9968CF:
		w9968cf_i2c_w(sd, reg, value);
		break;
	}

	if (sd->gspca_dev.usb_err >= 0) {
		
		if (reg == 0x12 && (value & 0x80))
			memset(sd->sensor_reg_cache, -1,
				sizeof(sd->sensor_reg_cache));
		else
			sd->sensor_reg_cache[reg] = value;
	}
}

static int i2c_r(struct sd *sd, u8 reg)
{
	int ret = -1;

	if (sd->sensor_reg_cache[reg] != -1)
		return sd->sensor_reg_cache[reg];

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		ret = ov511_i2c_r(sd, reg);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
	case BRIDGE_OV519:
		ret = ov518_i2c_r(sd, reg);
		break;
	case BRIDGE_OVFX2:
		ret = ovfx2_i2c_r(sd, reg);
		break;
	case BRIDGE_W9968CF:
		ret = w9968cf_i2c_r(sd, reg);
		break;
	}

	if (ret >= 0)
		sd->sensor_reg_cache[reg] = ret;

	return ret;
}

static void i2c_w_mask(struct sd *sd,
			u8 reg,
			u8 value,
			u8 mask)
{
	int rc;
	u8 oldval;

	value &= mask;			
	rc = i2c_r(sd, reg);
	if (rc < 0)
		return;
	oldval = rc & ~mask;		
	value |= oldval;		
	i2c_w(sd, reg, value);
}

static inline void ov51x_stop(struct sd *sd)
{
	PDEBUG(D_STREAM, "stopping");
	sd->stopped = 1;
	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		reg_w(sd, R51x_SYS_RESET, 0x3d);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		reg_w_mask(sd, R51x_SYS_RESET, 0x3a, 0x3a);
		break;
	case BRIDGE_OV519:
		reg_w(sd, OV519_R51_RESET1, 0x0f);
		reg_w(sd, OV519_R51_RESET1, 0x00);
		reg_w(sd, 0x22, 0x00);		
		break;
	case BRIDGE_OVFX2:
		reg_w_mask(sd, 0x0f, 0x00, 0x02);
		break;
	case BRIDGE_W9968CF:
		reg_w(sd, 0x3c, 0x0a05); 
		break;
	}
}

static inline void ov51x_restart(struct sd *sd)
{
	PDEBUG(D_STREAM, "restarting");
	if (!sd->stopped)
		return;
	sd->stopped = 0;

	
	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		reg_w(sd, R51x_SYS_RESET, 0x00);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		reg_w(sd, 0x2f, 0x80);
		reg_w(sd, R51x_SYS_RESET, 0x00);
		break;
	case BRIDGE_OV519:
		reg_w(sd, OV519_R51_RESET1, 0x0f);
		reg_w(sd, OV519_R51_RESET1, 0x00);
		reg_w(sd, 0x22, 0x1d);		
		break;
	case BRIDGE_OVFX2:
		reg_w_mask(sd, 0x0f, 0x02, 0x02);
		break;
	case BRIDGE_W9968CF:
		reg_w(sd, 0x3c, 0x8a05); 
		break;
	}
}

static void ov51x_set_slave_ids(struct sd *sd, u8 slave);

static int init_ov_sensor(struct sd *sd, u8 slave)
{
	int i;

	ov51x_set_slave_ids(sd, slave);

	
	i2c_w(sd, 0x12, 0x80);

	
	msleep(150);

	for (i = 0; i < i2c_detect_tries; i++) {
		if (i2c_r(sd, OV7610_REG_ID_HIGH) == 0x7f &&
		    i2c_r(sd, OV7610_REG_ID_LOW) == 0xa2) {
			PDEBUG(D_PROBE, "I2C synced in %d attempt(s)", i);
			return 0;
		}

		
		i2c_w(sd, 0x12, 0x80);

		
		msleep(150);

		
		if (i2c_r(sd, 0x00) < 0)
			return -1;
	}
	return -1;
}

static void ov51x_set_slave_ids(struct sd *sd,
				u8 slave)
{
	switch (sd->bridge) {
	case BRIDGE_OVFX2:
		reg_w(sd, OVFX2_I2C_ADDR, slave);
		return;
	case BRIDGE_W9968CF:
		sd->sensor_addr = slave;
		return;
	}

	reg_w(sd, R51x_I2C_W_SID, slave);
	reg_w(sd, R51x_I2C_R_SID, slave + 1);
}

static void write_regvals(struct sd *sd,
			 const struct ov_regvals *regvals,
			 int n)
{
	while (--n >= 0) {
		reg_w(sd, regvals->reg, regvals->val);
		regvals++;
	}
}

static void write_i2c_regvals(struct sd *sd,
			const struct ov_i2c_regvals *regvals,
			int n)
{
	while (--n >= 0) {
		i2c_w(sd, regvals->reg, regvals->val);
		regvals++;
	}
}


static void ov_hires_configure(struct sd *sd)
{
	int high, low;

	if (sd->bridge != BRIDGE_OVFX2) {
		pr_err("error hires sensors only supported with ovfx2\n");
		return;
	}

	PDEBUG(D_PROBE, "starting ov hires configuration");

	
	high = i2c_r(sd, 0x0a);
	low = i2c_r(sd, 0x0b);
	
	switch (high) {
	case 0x96:
		switch (low) {
		case 0x40:
			PDEBUG(D_PROBE, "Sensor is a OV2610");
			sd->sensor = SEN_OV2610;
			return;
		case 0x41:
			PDEBUG(D_PROBE, "Sensor is a OV2610AE");
			sd->sensor = SEN_OV2610AE;
			return;
		case 0xb1:
			PDEBUG(D_PROBE, "Sensor is a OV9600");
			sd->sensor = SEN_OV9600;
			return;
		}
		break;
	case 0x36:
		if ((low & 0x0f) == 0x00) {
			PDEBUG(D_PROBE, "Sensor is a OV3610");
			sd->sensor = SEN_OV3610;
			return;
		}
		break;
	}
	pr_err("Error unknown sensor type: %02x%02x\n", high, low);
}

static void ov8xx0_configure(struct sd *sd)
{
	int rc;

	PDEBUG(D_PROBE, "starting ov8xx0 configuration");

	
	rc = i2c_r(sd, OV7610_REG_COM_I);
	if (rc < 0) {
		PDEBUG(D_ERR, "Error detecting sensor type");
		return;
	}
	if ((rc & 3) == 1)
		sd->sensor = SEN_OV8610;
	else
		pr_err("Unknown image sensor version: %d\n", rc & 3);
}

static void ov7xx0_configure(struct sd *sd)
{
	int rc, high, low;

	PDEBUG(D_PROBE, "starting OV7xx0 configuration");

	
	rc = i2c_r(sd, OV7610_REG_COM_I);

	if (rc < 0) {
		PDEBUG(D_ERR, "Error detecting sensor type");
		return;
	}
	if ((rc & 3) == 3) {
		
		high = i2c_r(sd, 0x0a);
		low = i2c_r(sd, 0x0b);
		
		if (high == 0x76 && (low & 0xf0) == 0x70) {
			PDEBUG(D_PROBE, "Sensor is an OV76%02x", low);
			sd->sensor = SEN_OV7670;
		} else {
			PDEBUG(D_PROBE, "Sensor is an OV7610");
			sd->sensor = SEN_OV7610;
		}
	} else if ((rc & 3) == 1) {
		
		if (i2c_r(sd, 0x15) & 1) {
			PDEBUG(D_PROBE, "Sensor is an OV7620AE");
			sd->sensor = SEN_OV7620AE;
		} else {
			PDEBUG(D_PROBE, "Sensor is an OV76BE");
			sd->sensor = SEN_OV76BE;
		}
	} else if ((rc & 3) == 0) {
		
		high = i2c_r(sd, 0x0a);
		if (high < 0) {
			PDEBUG(D_ERR, "Error detecting camera chip PID");
			return;
		}
		low = i2c_r(sd, 0x0b);
		if (low < 0) {
			PDEBUG(D_ERR, "Error detecting camera chip VER");
			return;
		}
		if (high == 0x76) {
			switch (low) {
			case 0x30:
				pr_err("Sensor is an OV7630/OV7635\n");
				pr_err("7630 is not supported by this driver\n");
				return;
			case 0x40:
				PDEBUG(D_PROBE, "Sensor is an OV7645");
				sd->sensor = SEN_OV7640; 
				break;
			case 0x45:
				PDEBUG(D_PROBE, "Sensor is an OV7645B");
				sd->sensor = SEN_OV7640; 
				break;
			case 0x48:
				PDEBUG(D_PROBE, "Sensor is an OV7648");
				sd->sensor = SEN_OV7648;
				break;
			case 0x60:
				PDEBUG(D_PROBE, "Sensor is a OV7660");
				sd->sensor = SEN_OV7660;
				break;
			default:
				PDEBUG(D_PROBE, "Unknown sensor: 0x76%x", low);
				return;
			}
		} else {
			PDEBUG(D_PROBE, "Sensor is an OV7620");
			sd->sensor = SEN_OV7620;
		}
	} else {
		pr_err("Unknown image sensor version: %d\n", rc & 3);
	}
}

static void ov6xx0_configure(struct sd *sd)
{
	int rc;
	PDEBUG(D_PROBE, "starting OV6xx0 configuration");

	
	rc = i2c_r(sd, OV7610_REG_COM_I);
	if (rc < 0) {
		PDEBUG(D_ERR, "Error detecting sensor type");
		return;
	}

	switch (rc) {
	case 0x00:
		sd->sensor = SEN_OV6630;
		pr_warn("WARNING: Sensor is an OV66308. Your camera may have been misdetected in previous driver versions.\n");
		break;
	case 0x01:
		sd->sensor = SEN_OV6620;
		PDEBUG(D_PROBE, "Sensor is an OV6620");
		break;
	case 0x02:
		sd->sensor = SEN_OV6630;
		PDEBUG(D_PROBE, "Sensor is an OV66308AE");
		break;
	case 0x03:
		sd->sensor = SEN_OV66308AF;
		PDEBUG(D_PROBE, "Sensor is an OV66308AF");
		break;
	case 0x90:
		sd->sensor = SEN_OV6630;
		pr_warn("WARNING: Sensor is an OV66307. Your camera may have been misdetected in previous driver versions.\n");
		break;
	default:
		pr_err("FATAL: Unknown sensor version: 0x%02x\n", rc);
		return;
	}

	
	sd->sif = 1;
}

static void ov51x_led_control(struct sd *sd, int on)
{
	if (sd->invert_led)
		on = !on;

	switch (sd->bridge) {
	
	case BRIDGE_OV511PLUS:
		reg_w(sd, R511_SYS_LED_CTL, on);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		reg_w_mask(sd, R518_GPIO_OUT, 0x02 * on, 0x02);
		break;
	case BRIDGE_OV519:
		reg_w_mask(sd, OV519_GPIO_DATA_OUT0, on, 1);
		break;
	}
}

static void sd_reset_snapshot(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (!sd->snapshot_needs_reset)
		return;

	sd->snapshot_needs_reset = 0;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		reg_w(sd, R51x_SYS_SNAP, 0x02);
		reg_w(sd, R51x_SYS_SNAP, 0x00);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		reg_w(sd, R51x_SYS_SNAP, 0x02); 
		reg_w(sd, R51x_SYS_SNAP, 0x01); 
		break;
	case BRIDGE_OV519:
		reg_w(sd, R51x_SYS_RESET, 0x40);
		reg_w(sd, R51x_SYS_RESET, 0x00);
		break;
	}
}

static void ov51x_upload_quan_tables(struct sd *sd)
{
	const unsigned char yQuanTable511[] = {
		0, 1, 1, 2, 2, 3, 3, 4,
		1, 1, 1, 2, 2, 3, 4, 4,
		1, 1, 2, 2, 3, 4, 4, 4,
		2, 2, 2, 3, 4, 4, 4, 4,
		2, 2, 3, 4, 4, 5, 5, 5,
		3, 3, 4, 4, 5, 5, 5, 5,
		3, 4, 4, 4, 5, 5, 5, 5,
		4, 4, 4, 4, 5, 5, 5, 5
	};

	const unsigned char uvQuanTable511[] = {
		0, 2, 2, 3, 4, 4, 4, 4,
		2, 2, 2, 4, 4, 4, 4, 4,
		2, 2, 3, 4, 4, 4, 4, 4,
		3, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4
	};

	
	const unsigned char yQuanTable518[] = {
		5, 4, 5, 6, 6, 7, 7, 7,
		5, 5, 5, 5, 6, 7, 7, 7,
		6, 6, 6, 6, 7, 7, 7, 8,
		7, 7, 6, 7, 7, 7, 8, 8
	};
	const unsigned char uvQuanTable518[] = {
		6, 6, 6, 7, 7, 7, 7, 7,
		6, 6, 6, 7, 7, 7, 7, 7,
		6, 6, 6, 7, 7, 7, 7, 8,
		7, 7, 7, 7, 7, 7, 8, 8
	};

	const unsigned char *pYTable, *pUVTable;
	unsigned char val0, val1;
	int i, size, reg = R51x_COMP_LUT_BEGIN;

	PDEBUG(D_PROBE, "Uploading quantization tables");

	if (sd->bridge == BRIDGE_OV511 || sd->bridge == BRIDGE_OV511PLUS) {
		pYTable = yQuanTable511;
		pUVTable = uvQuanTable511;
		size = 32;
	} else {
		pYTable = yQuanTable518;
		pUVTable = uvQuanTable518;
		size = 16;
	}

	for (i = 0; i < size; i++) {
		val0 = *pYTable++;
		val1 = *pYTable++;
		val0 &= 0x0f;
		val1 &= 0x0f;
		val0 |= val1 << 4;
		reg_w(sd, reg, val0);

		val0 = *pUVTable++;
		val1 = *pUVTable++;
		val0 &= 0x0f;
		val1 &= 0x0f;
		val0 |= val1 << 4;
		reg_w(sd, reg + size, val0);

		reg++;
	}
}

static void ov511_configure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	
	const struct ov_regvals init_511[] = {
		{ R51x_SYS_RESET,	0x7f },
		{ R51x_SYS_INIT,	0x01 },
		{ R51x_SYS_RESET,	0x7f },
		{ R51x_SYS_INIT,	0x01 },
		{ R51x_SYS_RESET,	0x3f },
		{ R51x_SYS_INIT,	0x01 },
		{ R51x_SYS_RESET,	0x3d },
	};

	const struct ov_regvals norm_511[] = {
		{ R511_DRAM_FLOW_CTL,	0x01 },
		{ R51x_SYS_SNAP,	0x00 },
		{ R51x_SYS_SNAP,	0x02 },
		{ R51x_SYS_SNAP,	0x00 },
		{ R511_FIFO_OPTS,	0x1f },
		{ R511_COMP_EN,		0x00 },
		{ R511_COMP_LUT_EN,	0x03 },
	};

	const struct ov_regvals norm_511_p[] = {
		{ R511_DRAM_FLOW_CTL,	0xff },
		{ R51x_SYS_SNAP,	0x00 },
		{ R51x_SYS_SNAP,	0x02 },
		{ R51x_SYS_SNAP,	0x00 },
		{ R511_FIFO_OPTS,	0xff },
		{ R511_COMP_EN,		0x00 },
		{ R511_COMP_LUT_EN,	0x03 },
	};

	const struct ov_regvals compress_511[] = {
		{ 0x70, 0x1f },
		{ 0x71, 0x05 },
		{ 0x72, 0x06 },
		{ 0x73, 0x06 },
		{ 0x74, 0x14 },
		{ 0x75, 0x03 },
		{ 0x76, 0x04 },
		{ 0x77, 0x04 },
	};

	PDEBUG(D_PROBE, "Device custom id %x", reg_r(sd, R51x_SYS_CUST_ID));

	write_regvals(sd, init_511, ARRAY_SIZE(init_511));

	switch (sd->bridge) {
	case BRIDGE_OV511:
		write_regvals(sd, norm_511, ARRAY_SIZE(norm_511));
		break;
	case BRIDGE_OV511PLUS:
		write_regvals(sd, norm_511_p, ARRAY_SIZE(norm_511_p));
		break;
	}

	
	write_regvals(sd, compress_511, ARRAY_SIZE(compress_511));

	ov51x_upload_quan_tables(sd);
}

static void ov518_configure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	
	const struct ov_regvals init_518[] = {
		{ R51x_SYS_RESET,	0x40 },
		{ R51x_SYS_INIT,	0xe1 },
		{ R51x_SYS_RESET,	0x3e },
		{ R51x_SYS_INIT,	0xe1 },
		{ R51x_SYS_RESET,	0x00 },
		{ R51x_SYS_INIT,	0xe1 },
		{ 0x46,			0x00 },
		{ 0x5d,			0x03 },
	};

	const struct ov_regvals norm_518[] = {
		{ R51x_SYS_SNAP,	0x02 }, 
		{ R51x_SYS_SNAP,	0x01 }, 
		{ 0x31,			0x0f },
		{ 0x5d,			0x03 },
		{ 0x24,			0x9f },
		{ 0x25,			0x90 },
		{ 0x20,			0x00 },
		{ 0x51,			0x04 },
		{ 0x71,			0x19 },
		{ 0x2f,			0x80 },
	};

	const struct ov_regvals norm_518_p[] = {
		{ R51x_SYS_SNAP,	0x02 }, 
		{ R51x_SYS_SNAP,	0x01 }, 
		{ 0x31,			0x0f },
		{ 0x5d,			0x03 },
		{ 0x24,			0x9f },
		{ 0x25,			0x90 },
		{ 0x20,			0x60 },
		{ 0x51,			0x02 },
		{ 0x71,			0x19 },
		{ 0x40,			0xff },
		{ 0x41,			0x42 },
		{ 0x46,			0x00 },
		{ 0x33,			0x04 },
		{ 0x21,			0x19 },
		{ 0x3f,			0x10 },
		{ 0x2f,			0x80 },
	};

	
	PDEBUG(D_PROBE, "Device revision %d",
		0x1f & reg_r(sd, R51x_SYS_CUST_ID));

	write_regvals(sd, init_518, ARRAY_SIZE(init_518));

	
	reg_w_mask(sd, R518_GPIO_CTL, 0x00, 0x02);

	switch (sd->bridge) {
	case BRIDGE_OV518:
		write_regvals(sd, norm_518, ARRAY_SIZE(norm_518));
		break;
	case BRIDGE_OV518PLUS:
		write_regvals(sd, norm_518_p, ARRAY_SIZE(norm_518_p));
		break;
	}

	ov51x_upload_quan_tables(sd);

	reg_w(sd, 0x2f, 0x80);
}

static void ov519_configure(struct sd *sd)
{
	static const struct ov_regvals init_519[] = {
		{ 0x5a, 0x6d }, 
		{ 0x53, 0x9b }, 
		{ OV519_R54_EN_CLK1, 0xff }, 
		{ 0x5d, 0x03 },
		{ 0x49, 0x01 },
		{ 0x48, 0x00 },
		{ OV519_GPIO_IO_CTRL0,   0xee },
		{ OV519_R51_RESET1, 0x0f },
		{ OV519_R51_RESET1, 0x00 },
		{ 0x22, 0x00 },
		
	};

	write_regvals(sd, init_519, ARRAY_SIZE(init_519));
}

static void ovfx2_configure(struct sd *sd)
{
	static const struct ov_regvals init_fx2[] = {
		{ 0x00, 0x60 },
		{ 0x02, 0x01 },
		{ 0x0f, 0x1d },
		{ 0xe9, 0x82 },
		{ 0xea, 0xc7 },
		{ 0xeb, 0x10 },
		{ 0xec, 0xf6 },
	};

	sd->stopped = 1;

	write_regvals(sd, init_fx2, ARRAY_SIZE(init_fx2));
}

static void ov519_set_mode(struct sd *sd)
{
	static const struct ov_regvals bridge_ov7660[2][10] = {
		{{0x10, 0x14}, {0x11, 0x1e}, {0x12, 0x00}, {0x13, 0x00},
		 {0x14, 0x00}, {0x15, 0x00}, {0x16, 0x00}, {0x20, 0x0c},
		 {0x25, 0x01}, {0x26, 0x00}},
		{{0x10, 0x28}, {0x11, 0x3c}, {0x12, 0x00}, {0x13, 0x00},
		 {0x14, 0x00}, {0x15, 0x00}, {0x16, 0x00}, {0x20, 0x0c},
		 {0x25, 0x03}, {0x26, 0x00}}
	};
	static const struct ov_i2c_regvals sensor_ov7660[2][3] = {
		{{0x12, 0x00}, {0x24, 0x00}, {0x0c, 0x0c}},
		{{0x12, 0x00}, {0x04, 0x00}, {0x0c, 0x00}}
	};
	static const struct ov_i2c_regvals sensor_ov7660_2[] = {
		{OV7670_R17_HSTART, 0x13},
		{OV7670_R18_HSTOP, 0x01},
		{OV7670_R32_HREF, 0x92},
		{OV7670_R19_VSTART, 0x02},
		{OV7670_R1A_VSTOP, 0x7a},
		{OV7670_R03_VREF, 0x00},
	};

	write_regvals(sd, bridge_ov7660[sd->gspca_dev.curr_mode],
			ARRAY_SIZE(bridge_ov7660[0]));
	write_i2c_regvals(sd, sensor_ov7660[sd->gspca_dev.curr_mode],
			ARRAY_SIZE(sensor_ov7660[0]));
	write_i2c_regvals(sd, sensor_ov7660_2,
			ARRAY_SIZE(sensor_ov7660_2));
}

static void ov519_set_fr(struct sd *sd)
{
	int fr;
	u8 clock;
	static const u8 fr_tb[2][6][3] = {
		{{0x04, 0xff, 0x00},
		 {0x04, 0x1f, 0x00},
		 {0x04, 0x1b, 0x00},
		 {0x04, 0x15, 0x00},
		 {0x04, 0x09, 0x00},
		 {0x04, 0x01, 0x00}},
		{{0x0c, 0xff, 0x00},
		 {0x0c, 0x1f, 0x00},
		 {0x0c, 0x1b, 0x00},
		 {0x04, 0xff, 0x01},
		 {0x04, 0x1f, 0x01},
		 {0x04, 0x1b, 0x01}},
	};

	if (frame_rate > 0)
		sd->frame_rate = frame_rate;
	if (sd->frame_rate >= 30)
		fr = 0;
	else if (sd->frame_rate >= 25)
		fr = 1;
	else if (sd->frame_rate >= 20)
		fr = 2;
	else if (sd->frame_rate >= 15)
		fr = 3;
	else if (sd->frame_rate >= 10)
		fr = 4;
	else
		fr = 5;
	reg_w(sd, 0xa4, fr_tb[sd->gspca_dev.curr_mode][fr][0]);
	reg_w(sd, 0x23, fr_tb[sd->gspca_dev.curr_mode][fr][1]);
	clock = fr_tb[sd->gspca_dev.curr_mode][fr][2];
	if (sd->sensor == SEN_OV7660)
		clock |= 0x80;		
	ov518_i2c_w(sd, OV7670_R11_CLKRC, clock);
}

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	i2c_w_mask(sd, 0x13, sd->ctrls[AUTOGAIN].val ? 0x05 : 0x00, 0x05);
}

static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;

	sd->bridge = id->driver_info & BRIDGE_MASK;
	sd->invert_led = (id->driver_info & BRIDGE_INVERT_LED) != 0;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		cam->cam_mode = ov511_vga_mode;
		cam->nmodes = ARRAY_SIZE(ov511_vga_mode);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		cam->cam_mode = ov518_vga_mode;
		cam->nmodes = ARRAY_SIZE(ov518_vga_mode);
		break;
	case BRIDGE_OV519:
		cam->cam_mode = ov519_vga_mode;
		cam->nmodes = ARRAY_SIZE(ov519_vga_mode);
		break;
	case BRIDGE_OVFX2:
		cam->cam_mode = ov519_vga_mode;
		cam->nmodes = ARRAY_SIZE(ov519_vga_mode);
		cam->bulk_size = OVFX2_BULK_SIZE;
		cam->bulk_nurbs = MAX_NURBS;
		cam->bulk = 1;
		break;
	case BRIDGE_W9968CF:
		cam->cam_mode = w9968cf_vga_mode;
		cam->nmodes = ARRAY_SIZE(w9968cf_vga_mode);
		break;
	}

	gspca_dev->cam.ctrls = sd->ctrls;
	sd->quality = QUALITY_DEF;
	sd->frame_rate = 15;

	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		ov511_configure(gspca_dev);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		ov518_configure(gspca_dev);
		break;
	case BRIDGE_OV519:
		ov519_configure(sd);
		break;
	case BRIDGE_OVFX2:
		ovfx2_configure(sd);
		break;
	case BRIDGE_W9968CF:
		w9968cf_configure(sd);
		break;
	}

	sd->sensor = -1;

	
	if (init_ov_sensor(sd, OV7xx0_SID) >= 0) {
		ov7xx0_configure(sd);

	
	} else if (init_ov_sensor(sd, OV6xx0_SID) >= 0) {
		ov6xx0_configure(sd);

	
	} else if (init_ov_sensor(sd, OV8xx0_SID) >= 0) {
		ov8xx0_configure(sd);

	
	} else if (init_ov_sensor(sd, OV_HIRES_SID) >= 0) {
		ov_hires_configure(sd);
	} else {
		pr_err("Can't determine sensor slave IDs\n");
		goto error;
	}

	if (sd->sensor < 0)
		goto error;

	ov51x_led_control(sd, 0);	

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		if (sd->sif) {
			cam->cam_mode = ov511_sif_mode;
			cam->nmodes = ARRAY_SIZE(ov511_sif_mode);
		}
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		if (sd->sif) {
			cam->cam_mode = ov518_sif_mode;
			cam->nmodes = ARRAY_SIZE(ov518_sif_mode);
		}
		break;
	case BRIDGE_OV519:
		if (sd->sif) {
			cam->cam_mode = ov519_sif_mode;
			cam->nmodes = ARRAY_SIZE(ov519_sif_mode);
		}
		break;
	case BRIDGE_OVFX2:
		switch (sd->sensor) {
		case SEN_OV2610:
		case SEN_OV2610AE:
			cam->cam_mode = ovfx2_ov2610_mode;
			cam->nmodes = ARRAY_SIZE(ovfx2_ov2610_mode);
			break;
		case SEN_OV3610:
			cam->cam_mode = ovfx2_ov3610_mode;
			cam->nmodes = ARRAY_SIZE(ovfx2_ov3610_mode);
			break;
		case SEN_OV9600:
			cam->cam_mode = ovfx2_ov9600_mode;
			cam->nmodes = ARRAY_SIZE(ovfx2_ov9600_mode);
			break;
		default:
			if (sd->sif) {
				cam->cam_mode = ov519_sif_mode;
				cam->nmodes = ARRAY_SIZE(ov519_sif_mode);
			}
			break;
		}
		break;
	case BRIDGE_W9968CF:
		if (sd->sif)
			cam->nmodes = ARRAY_SIZE(w9968cf_vga_mode) - 1;

		
		w9968cf_init(sd);
		break;
	}

	gspca_dev->ctrl_dis = ctrl_dis[sd->sensor];

	
	switch (sd->sensor) {
	case SEN_OV2610:
		write_i2c_regvals(sd, norm_2610, ARRAY_SIZE(norm_2610));

		
		i2c_w_mask(sd, 0x13, 0x27, 0x27);
		break;
	case SEN_OV2610AE:
		write_i2c_regvals(sd, norm_2610ae, ARRAY_SIZE(norm_2610ae));

		
		i2c_w_mask(sd, 0x13, 0x05, 0x05);
		break;
	case SEN_OV3610:
		write_i2c_regvals(sd, norm_3620b, ARRAY_SIZE(norm_3620b));

		
		i2c_w_mask(sd, 0x13, 0x27, 0x27);
		break;
	case SEN_OV6620:
		write_i2c_regvals(sd, norm_6x20, ARRAY_SIZE(norm_6x20));
		break;
	case SEN_OV6630:
	case SEN_OV66308AF:
		sd->ctrls[CONTRAST].def = 200;
				 
		write_i2c_regvals(sd, norm_6x30, ARRAY_SIZE(norm_6x30));
		break;
	default:
		write_i2c_regvals(sd, norm_7610, ARRAY_SIZE(norm_7610));
		i2c_w_mask(sd, 0x0e, 0x00, 0x40);
		break;
	case SEN_OV7620:
	case SEN_OV7620AE:
		write_i2c_regvals(sd, norm_7620, ARRAY_SIZE(norm_7620));
		break;
	case SEN_OV7640:
	case SEN_OV7648:
		write_i2c_regvals(sd, norm_7640, ARRAY_SIZE(norm_7640));
		break;
	case SEN_OV7660:
		i2c_w(sd, OV7670_R12_COM7, OV7670_COM7_RESET);
		msleep(14);
		reg_w(sd, OV519_R57_SNAPSHOT, 0x23);
		write_regvals(sd, init_519_ov7660,
				ARRAY_SIZE(init_519_ov7660));
		write_i2c_regvals(sd, norm_7660, ARRAY_SIZE(norm_7660));
		sd->gspca_dev.curr_mode = 1;	
		ov519_set_mode(sd);
		ov519_set_fr(sd);
		sd->ctrls[COLORS].max = 4;	
		sd->ctrls[COLORS].val =
			sd->ctrls[COLORS].def = 2;
		setcolors(gspca_dev);
		sd->ctrls[CONTRAST].max = 6;	
		sd->ctrls[CONTRAST].val =
			sd->ctrls[CONTRAST].def = 3;
		setcontrast(gspca_dev);
		sd->ctrls[BRIGHTNESS].max = 6;	
		sd->ctrls[BRIGHTNESS].val =
			sd->ctrls[BRIGHTNESS].def = 3;
		setbrightness(gspca_dev);
		sd_reset_snapshot(gspca_dev);
		ov51x_restart(sd);
		ov51x_stop(sd);			
		ov51x_led_control(sd, 0);
		break;
	case SEN_OV7670:
		sd->ctrls[FREQ].max = 3;	
		sd->ctrls[FREQ].def = 3;
		write_i2c_regvals(sd, norm_7670, ARRAY_SIZE(norm_7670));
		break;
	case SEN_OV8610:
		write_i2c_regvals(sd, norm_8610, ARRAY_SIZE(norm_8610));
		break;
	case SEN_OV9600:
		write_i2c_regvals(sd, norm_9600, ARRAY_SIZE(norm_9600));

		
		break;
	}
	return gspca_dev->usb_err;
error:
	PDEBUG(D_ERR, "OV519 Config failed");
	return -EINVAL;
}

static int sd_isoc_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->bridge) {
	case BRIDGE_OVFX2:
		if (gspca_dev->width != 800)
			gspca_dev->cam.bulk_size = OVFX2_BULK_SIZE;
		else
			gspca_dev->cam.bulk_size = 7 * 4096;
		break;
	}
	return 0;
}

static void ov511_mode_init_regs(struct sd *sd)
{
	int hsegs, vsegs, packet_size, fps, needed;
	int interlaced = 0;
	struct usb_host_interface *alt;
	struct usb_interface *intf;

	intf = usb_ifnum_to_if(sd->gspca_dev.dev, sd->gspca_dev.iface);
	alt = usb_altnum_to_altsetting(intf, sd->gspca_dev.alt);
	if (!alt) {
		pr_err("Couldn't get altsetting\n");
		sd->gspca_dev.usb_err = -EIO;
		return;
	}

	packet_size = le16_to_cpu(alt->endpoint[0].desc.wMaxPacketSize);
	reg_w(sd, R51x_FIFO_PSIZE, packet_size >> 5);

	reg_w(sd, R511_CAM_UV_EN, 0x01);
	reg_w(sd, R511_SNAP_UV_EN, 0x01);
	reg_w(sd, R511_SNAP_OPTS, 0x03);

	hsegs = (sd->gspca_dev.width >> 3) - 1;
	vsegs = (sd->gspca_dev.height >> 3) - 1;

	reg_w(sd, R511_CAM_PXCNT, hsegs);
	reg_w(sd, R511_CAM_LNCNT, vsegs);
	reg_w(sd, R511_CAM_PXDIV, 0x00);
	reg_w(sd, R511_CAM_LNDIV, 0x00);

	
	reg_w(sd, R511_CAM_OPTS, 0x03);

	
	reg_w(sd, R511_SNAP_PXCNT, hsegs);
	reg_w(sd, R511_SNAP_LNCNT, vsegs);
	reg_w(sd, R511_SNAP_PXDIV, 0x00);
	reg_w(sd, R511_SNAP_LNDIV, 0x00);

	
	if (frame_rate > 0)
		sd->frame_rate = frame_rate;

	switch (sd->sensor) {
	case SEN_OV6620:
		
		sd->clockdiv = 3;
		break;

	case SEN_OV7620:
	case SEN_OV7620AE:
	case SEN_OV7640:
	case SEN_OV7648:
	case SEN_OV76BE:
		if (sd->gspca_dev.width == 320)
			interlaced = 1;
		
	case SEN_OV6630:
	case SEN_OV7610:
	case SEN_OV7670:
		switch (sd->frame_rate) {
		case 30:
		case 25:
			
			if (sd->gspca_dev.width != 640) {
				sd->clockdiv = 0;
				break;
			}
			
		default:
			sd->clockdiv = 1;
			break;
		case 10:
			sd->clockdiv = 2;
			break;
		case 5:
			sd->clockdiv = 5;
			break;
		}
		if (interlaced) {
			sd->clockdiv = (sd->clockdiv + 1) * 2 - 1;
			
			if (sd->clockdiv > 10)
				sd->clockdiv = 10;
		}
		break;

	case SEN_OV8610:
		
		sd->clockdiv = 0;
		break;
	}

	
	fps = (interlaced ? 60 : 30) / (sd->clockdiv + 1) + 1;
	needed = fps * sd->gspca_dev.width * sd->gspca_dev.height * 3 / 2;
	
	if (needed > 1000 * packet_size) {
		
		reg_w(sd, R511_COMP_EN, 0x07);
		reg_w(sd, R511_COMP_LUT_EN, 0x03);
	} else {
		reg_w(sd, R511_COMP_EN, 0x06);
		reg_w(sd, R511_COMP_LUT_EN, 0x00);
	}

	reg_w(sd, R51x_SYS_RESET, OV511_RESET_OMNICE);
	reg_w(sd, R51x_SYS_RESET, 0);
}

static void ov518_mode_init_regs(struct sd *sd)
{
	int hsegs, vsegs, packet_size;
	struct usb_host_interface *alt;
	struct usb_interface *intf;

	intf = usb_ifnum_to_if(sd->gspca_dev.dev, sd->gspca_dev.iface);
	alt = usb_altnum_to_altsetting(intf, sd->gspca_dev.alt);
	if (!alt) {
		pr_err("Couldn't get altsetting\n");
		sd->gspca_dev.usb_err = -EIO;
		return;
	}

	packet_size = le16_to_cpu(alt->endpoint[0].desc.wMaxPacketSize);
	ov518_reg_w32(sd, R51x_FIFO_PSIZE, packet_size & ~7, 2);

	
	reg_w(sd, 0x2b, 0);
	reg_w(sd, 0x2c, 0);
	reg_w(sd, 0x2d, 0);
	reg_w(sd, 0x2e, 0);
	reg_w(sd, 0x3b, 0);
	reg_w(sd, 0x3c, 0);
	reg_w(sd, 0x3d, 0);
	reg_w(sd, 0x3e, 0);

	if (sd->bridge == BRIDGE_OV518) {
		
		reg_w_mask(sd, 0x20, 0x08, 0x08);

		
		reg_w_mask(sd, 0x28, 0x80, 0xf0);
		reg_w_mask(sd, 0x38, 0x80, 0xf0);
	} else {
		reg_w(sd, 0x28, 0x80);
		reg_w(sd, 0x38, 0x80);
	}

	hsegs = sd->gspca_dev.width / 16;
	vsegs = sd->gspca_dev.height / 4;

	reg_w(sd, 0x29, hsegs);
	reg_w(sd, 0x2a, vsegs);

	reg_w(sd, 0x39, hsegs);
	reg_w(sd, 0x3a, vsegs);

	
	reg_w(sd, 0x2f, 0x80);

	
	sd->clockdiv = 1;

	
	
	reg_w(sd, 0x51, 0x04);
	reg_w(sd, 0x22, 0x18);
	reg_w(sd, 0x23, 0xff);

	if (sd->bridge == BRIDGE_OV518PLUS) {
		switch (sd->sensor) {
		case SEN_OV7620AE:
			if (sd->gspca_dev.width == 320) {
				reg_w(sd, 0x20, 0x00);
				reg_w(sd, 0x21, 0x19);
			} else {
				reg_w(sd, 0x20, 0x60);
				reg_w(sd, 0x21, 0x1f);
			}
			break;
		case SEN_OV7620:
			reg_w(sd, 0x20, 0x00);
			reg_w(sd, 0x21, 0x19);
			break;
		default:
			reg_w(sd, 0x21, 0x19);
		}
	} else
		reg_w(sd, 0x71, 0x17);	

	
	
	i2c_w(sd, 0x54, 0x23);

	reg_w(sd, 0x2f, 0x80);

	if (sd->bridge == BRIDGE_OV518PLUS) {
		reg_w(sd, 0x24, 0x94);
		reg_w(sd, 0x25, 0x90);
		ov518_reg_w32(sd, 0xc4,    400, 2);	
		ov518_reg_w32(sd, 0xc6,    540, 2);	
		ov518_reg_w32(sd, 0xc7,    540, 2);	
		ov518_reg_w32(sd, 0xc8,    108, 2);	
		ov518_reg_w32(sd, 0xca, 131098, 3);	
		ov518_reg_w32(sd, 0xcb,    532, 2);	
		ov518_reg_w32(sd, 0xcc,   2400, 2);	
		ov518_reg_w32(sd, 0xcd,     32, 2);	
		ov518_reg_w32(sd, 0xce,    608, 2);	
	} else {
		reg_w(sd, 0x24, 0x9f);
		reg_w(sd, 0x25, 0x90);
		ov518_reg_w32(sd, 0xc4,    400, 2);	
		ov518_reg_w32(sd, 0xc6,    381, 2);	
		ov518_reg_w32(sd, 0xc7,    381, 2);	
		ov518_reg_w32(sd, 0xc8,    128, 2);	
		ov518_reg_w32(sd, 0xca, 183331, 3);	
		ov518_reg_w32(sd, 0xcb,    746, 2);	
		ov518_reg_w32(sd, 0xcc,   1750, 2);	
		ov518_reg_w32(sd, 0xcd,     45, 2);	
		ov518_reg_w32(sd, 0xce,    851, 2);	
	}

	reg_w(sd, 0x2f, 0x80);
}

static void ov519_mode_init_regs(struct sd *sd)
{
	static const struct ov_regvals mode_init_519_ov7670[] = {
		{ 0x5d,	0x03 }, 
		{ 0x53,	0x9f }, 
		{ OV519_R54_EN_CLK1, 0x0f }, 
		{ 0xa2,	0x20 }, 
		{ 0xa3,	0x18 },
		{ 0xa4,	0x04 },
		{ 0xa5,	0x28 },
		{ 0x37,	0x00 },	
		{ 0x55,	0x02 }, 
		
		{ 0x20,	0x0c },
		{ 0x21,	0x38 },
		{ 0x22,	0x1d },
		{ 0x17,	0x50 }, 
		{ 0x37,	0x00 }, 
		{ 0x40,	0xff }, 
		{ 0x46,	0x00 }, 
		{ 0x59,	0x04 },	
		{ 0xff,	0x00 }, 
		
	};

	static const struct ov_regvals mode_init_519[] = {
		{ 0x5d,	0x03 }, 
		{ 0x53,	0x9f }, 
		{ OV519_R54_EN_CLK1, 0x0f }, 
		{ 0xa2,	0x20 }, 
		{ 0xa3,	0x18 },
		{ 0xa4,	0x04 },
		{ 0xa5,	0x28 },
		{ 0x37,	0x00 },	
		{ 0x55,	0x02 }, 
		
		{ 0x22,	0x1d },
		{ 0x17,	0x50 }, 
		{ 0x37,	0x00 }, 
		{ 0x40,	0xff }, 
		{ 0x46,	0x00 }, 
		{ 0x59,	0x04 },	
		{ 0xff,	0x00 }, 
		
	};

	
	switch (sd->sensor) {
	default:
		write_regvals(sd, mode_init_519, ARRAY_SIZE(mode_init_519));
		if (sd->sensor == SEN_OV7640 ||
		    sd->sensor == SEN_OV7648) {
			
			reg_w_mask(sd, OV519_R20_DFR, 0x10, 0x10);
		}
		break;
	case SEN_OV7660:
		return;		
	case SEN_OV7670:
		write_regvals(sd, mode_init_519_ov7670,
				ARRAY_SIZE(mode_init_519_ov7670));
		break;
	}

	reg_w(sd, OV519_R10_H_SIZE,	sd->gspca_dev.width >> 4);
	reg_w(sd, OV519_R11_V_SIZE,	sd->gspca_dev.height >> 3);
	if (sd->sensor == SEN_OV7670 &&
	    sd->gspca_dev.cam.cam_mode[sd->gspca_dev.curr_mode].priv)
		reg_w(sd, OV519_R12_X_OFFSETL, 0x04);
	else if (sd->sensor == SEN_OV7648 &&
	    sd->gspca_dev.cam.cam_mode[sd->gspca_dev.curr_mode].priv)
		reg_w(sd, OV519_R12_X_OFFSETL, 0x01);
	else
		reg_w(sd, OV519_R12_X_OFFSETL, 0x00);
	reg_w(sd, OV519_R13_X_OFFSETH,	0x00);
	reg_w(sd, OV519_R14_Y_OFFSETL,	0x00);
	reg_w(sd, OV519_R15_Y_OFFSETH,	0x00);
	reg_w(sd, OV519_R16_DIVIDER,	0x00);
	reg_w(sd, OV519_R25_FORMAT,	0x03); 
	reg_w(sd, 0x26,			0x00); 

	
	if (frame_rate > 0)
		sd->frame_rate = frame_rate;

	sd->clockdiv = 0;
	switch (sd->sensor) {
	case SEN_OV7640:
	case SEN_OV7648:
		switch (sd->frame_rate) {
		default:
			reg_w(sd, 0xa4, 0x0c);
			reg_w(sd, 0x23, 0xff);
			break;
		case 25:
			reg_w(sd, 0xa4, 0x0c);
			reg_w(sd, 0x23, 0x1f);
			break;
		case 20:
			reg_w(sd, 0xa4, 0x0c);
			reg_w(sd, 0x23, 0x1b);
			break;
		case 15:
			reg_w(sd, 0xa4, 0x04);
			reg_w(sd, 0x23, 0xff);
			sd->clockdiv = 1;
			break;
		case 10:
			reg_w(sd, 0xa4, 0x04);
			reg_w(sd, 0x23, 0x1f);
			sd->clockdiv = 1;
			break;
		case 5:
			reg_w(sd, 0xa4, 0x04);
			reg_w(sd, 0x23, 0x1b);
			sd->clockdiv = 1;
			break;
		}
		break;
	case SEN_OV8610:
		switch (sd->frame_rate) {
		default:	
			reg_w(sd, 0xa4, 0x06);
			reg_w(sd, 0x23, 0xff);
			break;
		case 10:
			reg_w(sd, 0xa4, 0x06);
			reg_w(sd, 0x23, 0x1f);
			break;
		case 5:
			reg_w(sd, 0xa4, 0x06);
			reg_w(sd, 0x23, 0x1b);
			break;
		}
		break;
	case SEN_OV7670:		
		PDEBUG(D_STREAM, "Setting framerate to %d fps",
				 (sd->frame_rate == 0) ? 15 : sd->frame_rate);
		reg_w(sd, 0xa4, 0x10);
		switch (sd->frame_rate) {
		case 30:
			reg_w(sd, 0x23, 0xff);
			break;
		case 20:
			reg_w(sd, 0x23, 0x1b);
			break;
		default:
			reg_w(sd, 0x23, 0xff);
			sd->clockdiv = 1;
			break;
		}
		break;
	}
}

static void mode_init_ov_sensor_regs(struct sd *sd)
{
	struct gspca_dev *gspca_dev;
	int qvga, xstart, xend, ystart, yend;
	u8 v;

	gspca_dev = &sd->gspca_dev;
	qvga = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv & 1;

	
	switch (sd->sensor) {
	case SEN_OV2610:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x28, qvga ? 0x00 : 0x20, 0x20);
		i2c_w(sd, 0x24, qvga ? 0x20 : 0x3a);
		i2c_w(sd, 0x25, qvga ? 0x30 : 0x60);
		i2c_w_mask(sd, 0x2d, qvga ? 0x40 : 0x00, 0x40);
		i2c_w_mask(sd, 0x67, qvga ? 0xf0 : 0x90, 0xf0);
		i2c_w_mask(sd, 0x74, qvga ? 0x20 : 0x00, 0x20);
		return;
	case SEN_OV2610AE: {
		u8 v;

		v = 80;
		if (qvga) {
			if (sd->frame_rate < 25)
				v = 0x81;
		} else {
			if (sd->frame_rate < 10)
				v = 0x81;
		}
		i2c_w(sd, 0x11, v);
		i2c_w(sd, 0x12, qvga ? 0x60 : 0x20);
		return;
	    }
	case SEN_OV3610:
		if (qvga) {
			xstart = (1040 - gspca_dev->width) / 2 + (0x1f << 4);
			ystart = (776 - gspca_dev->height) / 2;
		} else {
			xstart = (2076 - gspca_dev->width) / 2 + (0x10 << 4);
			ystart = (1544 - gspca_dev->height) / 2;
		}
		xend = xstart + gspca_dev->width;
		yend = ystart + gspca_dev->height;
		i2c_w_mask(sd, 0x12, qvga ? 0x40 : 0x00, 0xf0);
		i2c_w_mask(sd, 0x32,
			   (((xend >> 1) & 7) << 3) | ((xstart >> 1) & 7),
			   0x3f);
		i2c_w_mask(sd, 0x03,
			   (((yend >> 1) & 3) << 2) | ((ystart >> 1) & 3),
			   0x0f);
		i2c_w(sd, 0x17, xstart >> 4);
		i2c_w(sd, 0x18, xend >> 4);
		i2c_w(sd, 0x19, ystart >> 3);
		i2c_w(sd, 0x1a, yend >> 3);
		return;
	case SEN_OV8610:
		
		i2c_w_mask(sd, OV7610_REG_COM_C, qvga ? (1 << 5) : 0, 1 << 5);
		i2c_w_mask(sd, 0x13, 0x00, 0x20); 
		i2c_w_mask(sd, 0x12, 0x04, 0x06); 
		i2c_w_mask(sd, 0x2d, 0x00, 0x40); 
		i2c_w_mask(sd, 0x28, 0x20, 0x20); 
		break;
	case SEN_OV7610:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w(sd, 0x35, qvga ? 0x1e : 0x9e);
		i2c_w_mask(sd, 0x13, 0x00, 0x20); 
		i2c_w_mask(sd, 0x12, 0x04, 0x06); 
		break;
	case SEN_OV7620:
	case SEN_OV7620AE:
	case SEN_OV76BE:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x28, qvga ? 0x00 : 0x20, 0x20);
		i2c_w(sd, 0x24, qvga ? 0x20 : 0x3a);
		i2c_w(sd, 0x25, qvga ? 0x30 : 0x60);
		i2c_w_mask(sd, 0x2d, qvga ? 0x40 : 0x00, 0x40);
		i2c_w_mask(sd, 0x67, qvga ? 0xb0 : 0x90, 0xf0);
		i2c_w_mask(sd, 0x74, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x13, 0x00, 0x20); 
		i2c_w_mask(sd, 0x12, 0x04, 0x06); 
		if (sd->sensor == SEN_OV76BE)
			i2c_w(sd, 0x35, qvga ? 0x1e : 0x9e);
		break;
	case SEN_OV7640:
	case SEN_OV7648:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x28, qvga ? 0x00 : 0x20, 0x20);
		i2c_w_mask(sd, 0x2d, qvga ? 0x40 : 0x00, 0x40);
		
		i2c_w_mask(sd, 0x67, qvga ? 0xf0 : 0x90, 0xf0);
		
		i2c_w_mask(sd, 0x74, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x12, 0x04, 0x04); 
		break;
	case SEN_OV7670:
		i2c_w_mask(sd, OV7670_R12_COM7,
			 qvga ? OV7670_COM7_FMT_QVGA : OV7670_COM7_FMT_VGA,
			 OV7670_COM7_FMT_MASK);
		i2c_w_mask(sd, 0x13, 0x00, 0x20); 
		i2c_w_mask(sd, OV7670_R13_COM8, OV7670_COM8_AWB,
				OV7670_COM8_AWB);
		if (qvga) {		
			xstart = 164;
			xend = 28;
			ystart = 14;
			yend = 494;
		} else {		
			xstart = 158;
			xend = 14;
			ystart = 10;
			yend = 490;
		}
		i2c_w(sd, OV7670_R17_HSTART, xstart >> 3);
		i2c_w(sd, OV7670_R18_HSTOP, xend >> 3);
		v = i2c_r(sd, OV7670_R32_HREF);
		v = (v & 0xc0) | ((xend & 0x7) << 3) | (xstart & 0x07);
		msleep(10);	
		i2c_w(sd, OV7670_R32_HREF, v);

		i2c_w(sd, OV7670_R19_VSTART, ystart >> 2);
		i2c_w(sd, OV7670_R1A_VSTOP, yend >> 2);
		v = i2c_r(sd, OV7670_R03_VREF);
		v = (v & 0xc0) | ((yend & 0x3) << 2) | (ystart & 0x03);
		msleep(10);	
		i2c_w(sd, OV7670_R03_VREF, v);
		break;
	case SEN_OV6620:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x13, 0x00, 0x20); 
		i2c_w_mask(sd, 0x12, 0x04, 0x06); 
		break;
	case SEN_OV6630:
	case SEN_OV66308AF:
		i2c_w_mask(sd, 0x14, qvga ? 0x20 : 0x00, 0x20);
		i2c_w_mask(sd, 0x12, 0x04, 0x06); 
		break;
	case SEN_OV9600: {
		const struct ov_i2c_regvals *vals;
		static const struct ov_i2c_regvals sxga_15[] = {
			{0x11, 0x80}, {0x14, 0x3e}, {0x24, 0x85}, {0x25, 0x75}
		};
		static const struct ov_i2c_regvals sxga_7_5[] = {
			{0x11, 0x81}, {0x14, 0x3e}, {0x24, 0x85}, {0x25, 0x75}
		};
		static const struct ov_i2c_regvals vga_30[] = {
			{0x11, 0x81}, {0x14, 0x7e}, {0x24, 0x70}, {0x25, 0x60}
		};
		static const struct ov_i2c_regvals vga_15[] = {
			{0x11, 0x83}, {0x14, 0x3e}, {0x24, 0x80}, {0x25, 0x70}
		};

		i2c_w_mask(sd, 0x12, qvga ? 0x40 : 0x00, 0x40);
		if (qvga)
			vals = sd->frame_rate < 30 ? vga_15 : vga_30;
		else
			vals = sd->frame_rate < 15 ? sxga_7_5 : sxga_15;
		write_i2c_regvals(sd, vals, ARRAY_SIZE(sxga_15));
		return;
	    }
	default:
		return;
	}

	
	i2c_w(sd, 0x11, sd->clockdiv);
}

static void sethvflip(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->gspca_dev.streaming)
		reg_w(sd, OV519_R51_RESET1, 0x0f);	
	i2c_w_mask(sd, OV7670_R1E_MVFP,
		OV7670_MVFP_MIRROR * sd->ctrls[HFLIP].val
			| OV7670_MVFP_VFLIP * sd->ctrls[VFLIP].val,
		OV7670_MVFP_MIRROR | OV7670_MVFP_VFLIP);
	if (sd->gspca_dev.streaming)
		reg_w(sd, OV519_R51_RESET1, 0x00);	
}

static void set_ov_sensor_window(struct sd *sd)
{
	struct gspca_dev *gspca_dev;
	int qvga, crop;
	int hwsbase, hwebase, vwsbase, vwebase, hwscale, vwscale;

	
	switch (sd->sensor) {
	case SEN_OV2610:
	case SEN_OV2610AE:
	case SEN_OV3610:
	case SEN_OV7670:
	case SEN_OV9600:
		mode_init_ov_sensor_regs(sd);
		return;
	case SEN_OV7660:
		ov519_set_mode(sd);
		ov519_set_fr(sd);
		return;
	}

	gspca_dev = &sd->gspca_dev;
	qvga = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv & 1;
	crop = gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv & 2;

	switch (sd->sensor) {
	case SEN_OV8610:
		hwsbase = 0x1e;
		hwebase = 0x1e;
		vwsbase = 0x02;
		vwebase = 0x02;
		break;
	case SEN_OV7610:
	case SEN_OV76BE:
		hwsbase = 0x38;
		hwebase = 0x3a;
		vwsbase = vwebase = 0x05;
		break;
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV66308AF:
		hwsbase = 0x38;
		hwebase = 0x3a;
		vwsbase = 0x05;
		vwebase = 0x06;
		if (sd->sensor == SEN_OV66308AF && qvga)
			
			hwsbase++;
		if (crop) {
			hwsbase += 8;
			hwebase += 8;
			vwsbase += 11;
			vwebase += 11;
		}
		break;
	case SEN_OV7620:
	case SEN_OV7620AE:
		hwsbase = 0x2f;		
		hwebase = 0x2f;
		vwsbase = vwebase = 0x05;
		break;
	case SEN_OV7640:
	case SEN_OV7648:
		hwsbase = 0x1a;
		hwebase = 0x1a;
		vwsbase = vwebase = 0x03;
		break;
	default:
		return;
	}

	switch (sd->sensor) {
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV66308AF:
		if (qvga) {		
			hwscale = 0;
			vwscale = 0;
		} else {		
			hwscale = 1;
			vwscale = 1;	
		}
		break;
	case SEN_OV8610:
		if (qvga) {		
			hwscale = 1;
			vwscale = 1;
		} else {		
			hwscale = 2;
			vwscale = 2;
		}
		break;
	default:			
		if (qvga) {		
			hwscale = 1;
			vwscale = 0;
		} else {		
			hwscale = 2;
			vwscale = 1;
		}
	}

	mode_init_ov_sensor_regs(sd);

	i2c_w(sd, 0x17, hwsbase);
	i2c_w(sd, 0x18, hwebase + (sd->sensor_width >> hwscale));
	i2c_w(sd, 0x19, vwsbase);
	i2c_w(sd, 0x1a, vwebase + (sd->sensor_height >> vwscale));
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	
	sd->sensor_width = sd->gspca_dev.width;
	sd->sensor_height = sd->gspca_dev.height;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		ov511_mode_init_regs(sd);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		ov518_mode_init_regs(sd);
		break;
	case BRIDGE_OV519:
		ov519_mode_init_regs(sd);
		break;
	
	case BRIDGE_W9968CF:
		w9968cf_mode_init_regs(sd);
		break;
	}

	set_ov_sensor_window(sd);

	if (!(sd->gspca_dev.ctrl_dis & (1 << CONTRAST)))
		setcontrast(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << BRIGHTNESS)))
		setbrightness(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << EXPOSURE)))
		setexposure(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << COLORS)))
		setcolors(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & ((1 << HFLIP) | (1 << VFLIP))))
		sethvflip(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << AUTOBRIGHT)))
		setautobright(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << AUTOGAIN)))
		setautogain(gspca_dev);
	if (!(sd->gspca_dev.ctrl_dis & (1 << FREQ)))
		setfreq_i(sd);

	sd->snapshot_needs_reset = 1;
	sd_reset_snapshot(gspca_dev);

	sd->first_frame = 3;

	ov51x_restart(sd);
	ov51x_led_control(sd, 1);
	return gspca_dev->usb_err;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	ov51x_stop(sd);
	ov51x_led_control(sd, 0);
}

static void sd_stop0(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (!sd->gspca_dev.present)
		return;
	if (sd->bridge == BRIDGE_W9968CF)
		w9968cf_stop0(sd);

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	
	if (sd->snapshot_pressed) {
		input_report_key(gspca_dev->input_dev, KEY_CAMERA, 0);
		input_sync(gspca_dev->input_dev);
		sd->snapshot_pressed = 0;
	}
#endif
	if (sd->bridge == BRIDGE_OV519)
		reg_w(sd, OV519_R57_SNAPSHOT, 0x23);
}

static void ov51x_handle_button(struct gspca_dev *gspca_dev, u8 state)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->snapshot_pressed != state) {
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
		input_report_key(gspca_dev->input_dev, KEY_CAMERA, state);
		input_sync(gspca_dev->input_dev);
#endif
		if (state)
			sd->snapshot_needs_reset = 1;

		sd->snapshot_pressed = state;
	} else {
		switch (sd->bridge) {
		case BRIDGE_OV511:
		case BRIDGE_OV511PLUS:
		case BRIDGE_OV519:
			if (state)
				sd->snapshot_needs_reset = 1;
			break;
		}
	}
}

static void ov511_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *in,			
			int len)		
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (!(in[0] | in[1] | in[2] | in[3] | in[4] | in[5] | in[6] | in[7]) &&
	    (in[8] & 0x08)) {
		ov51x_handle_button(gspca_dev, (in[8] >> 2) & 1);
		if (in[8] & 0x80) {
			
			if ((in[9] + 1) * 8 != gspca_dev->width ||
			    (in[10] + 1) * 8 != gspca_dev->height) {
				PDEBUG(D_ERR, "Invalid frame size, got: %dx%d,"
					" requested: %dx%d\n",
					(in[9] + 1) * 8, (in[10] + 1) * 8,
					gspca_dev->width, gspca_dev->height);
				gspca_dev->last_packet_type = DISCARD_PACKET;
				return;
			}
			
			gspca_frame_add(gspca_dev, LAST_PACKET, in, 11);
			return;
		} else {
			
			gspca_frame_add(gspca_dev, FIRST_PACKET, in, 0);
			sd->packet_nr = 0;
		}
	}

	
	len--;

	
	gspca_frame_add(gspca_dev, INTER_PACKET, in, len);
}

static void ov518_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	struct sd *sd = (struct sd *) gspca_dev;

	if ((!(data[0] | data[1] | data[2] | data[3] | data[5])) && data[6]) {
		ov51x_handle_button(gspca_dev, (data[6] >> 1) & 1);
		gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
		gspca_frame_add(gspca_dev, FIRST_PACKET, NULL, 0);
		sd->packet_nr = 0;
	}

	if (gspca_dev->last_packet_type == DISCARD_PACKET)
		return;

	
	if (len & 7) {
		len--;
		if (sd->packet_nr == data[len])
			sd->packet_nr++;
		else if (sd->packet_nr == 0 || data[len]) {
			PDEBUG(D_ERR, "Invalid packet nr: %d (expect: %d)",
				(int)data[len], (int)sd->packet_nr);
			gspca_dev->last_packet_type = DISCARD_PACKET;
			return;
		}
	}

	
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

static void ov519_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{

	if (data[0] == 0xff && data[1] == 0xff && data[2] == 0xff) {
		switch (data[3]) {
		case 0x50:		
#define HDRSZ 16
			data += HDRSZ;
			len -= HDRSZ;
#undef HDRSZ
			if (data[0] == 0xff || data[1] == 0xd8)
				gspca_frame_add(gspca_dev, FIRST_PACKET,
						data, len);
			else
				gspca_dev->last_packet_type = DISCARD_PACKET;
			return;
		case 0x51:		
			ov51x_handle_button(gspca_dev, data[11] & 1);
			if (data[9] != 0)
				gspca_dev->last_packet_type = DISCARD_PACKET;
			gspca_frame_add(gspca_dev, LAST_PACKET,
					NULL, 0);
			return;
		}
	}

	
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

static void ovfx2_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	struct sd *sd = (struct sd *) gspca_dev;

	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);

	
	if (len < gspca_dev->cam.bulk_size) {
		if (sd->first_frame) {
			sd->first_frame--;
			if (gspca_dev->image_len <
				  sd->gspca_dev.width * sd->gspca_dev.height)
				gspca_dev->last_packet_type = DISCARD_PACKET;
		}
		gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
		gspca_frame_add(gspca_dev, FIRST_PACKET, NULL, 0);
	}
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->bridge) {
	case BRIDGE_OV511:
	case BRIDGE_OV511PLUS:
		ov511_pkt_scan(gspca_dev, data, len);
		break;
	case BRIDGE_OV518:
	case BRIDGE_OV518PLUS:
		ov518_pkt_scan(gspca_dev, data, len);
		break;
	case BRIDGE_OV519:
		ov519_pkt_scan(gspca_dev, data, len);
		break;
	case BRIDGE_OVFX2:
		ovfx2_pkt_scan(gspca_dev, data, len);
		break;
	case BRIDGE_W9968CF:
		w9968cf_pkt_scan(gspca_dev, data, len);
		break;
	}
}


static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int val;
	static const struct ov_i2c_regvals brit_7660[][7] = {
		{{0x0f, 0x6a}, {0x24, 0x40}, {0x25, 0x2b}, {0x26, 0x90},
			{0x27, 0xe0}, {0x28, 0xe0}, {0x2c, 0xe0}},
		{{0x0f, 0x6a}, {0x24, 0x50}, {0x25, 0x40}, {0x26, 0xa1},
			{0x27, 0xc0}, {0x28, 0xc0}, {0x2c, 0xc0}},
		{{0x0f, 0x6a}, {0x24, 0x68}, {0x25, 0x58}, {0x26, 0xc2},
			{0x27, 0xa0}, {0x28, 0xa0}, {0x2c, 0xa0}},
		{{0x0f, 0x6a}, {0x24, 0x70}, {0x25, 0x68}, {0x26, 0xd3},
			{0x27, 0x80}, {0x28, 0x80}, {0x2c, 0x80}},
		{{0x0f, 0x6a}, {0x24, 0x80}, {0x25, 0x70}, {0x26, 0xd3},
			{0x27, 0x20}, {0x28, 0x20}, {0x2c, 0x20}},
		{{0x0f, 0x6a}, {0x24, 0x88}, {0x25, 0x78}, {0x26, 0xd3},
			{0x27, 0x40}, {0x28, 0x40}, {0x2c, 0x40}},
		{{0x0f, 0x6a}, {0x24, 0x90}, {0x25, 0x80}, {0x26, 0xd4},
			{0x27, 0x60}, {0x28, 0x60}, {0x2c, 0x60}}
	};

	val = sd->ctrls[BRIGHTNESS].val;
	switch (sd->sensor) {
	case SEN_OV8610:
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV66308AF:
	case SEN_OV7640:
	case SEN_OV7648:
		i2c_w(sd, OV7610_REG_BRT, val);
		break;
	case SEN_OV7620:
	case SEN_OV7620AE:
		
		if (!sd->ctrls[AUTOBRIGHT].val)
			i2c_w(sd, OV7610_REG_BRT, val);
		break;
	case SEN_OV7660:
		write_i2c_regvals(sd, brit_7660[val],
				ARRAY_SIZE(brit_7660[0]));
		break;
	case SEN_OV7670:
		i2c_w(sd, OV7670_R55_BRIGHT, ov7670_abs_to_sm(val));
		break;
	}
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int val;
	static const struct ov_i2c_regvals contrast_7660[][31] = {
		{{0x6c, 0xf0}, {0x6d, 0xf0}, {0x6e, 0xf8}, {0x6f, 0xa0},
		 {0x70, 0x58}, {0x71, 0x38}, {0x72, 0x30}, {0x73, 0x30},
		 {0x74, 0x28}, {0x75, 0x28}, {0x76, 0x24}, {0x77, 0x24},
		 {0x78, 0x22}, {0x79, 0x28}, {0x7a, 0x2a}, {0x7b, 0x34},
		 {0x7c, 0x0f}, {0x7d, 0x1e}, {0x7e, 0x3d}, {0x7f, 0x65},
		 {0x80, 0x70}, {0x81, 0x77}, {0x82, 0x7d}, {0x83, 0x83},
		 {0x84, 0x88}, {0x85, 0x8d}, {0x86, 0x96}, {0x87, 0x9f},
		 {0x88, 0xb0}, {0x89, 0xc4}, {0x8a, 0xd9}},
		{{0x6c, 0xf0}, {0x6d, 0xf0}, {0x6e, 0xf8}, {0x6f, 0x94},
		 {0x70, 0x58}, {0x71, 0x40}, {0x72, 0x30}, {0x73, 0x30},
		 {0x74, 0x30}, {0x75, 0x30}, {0x76, 0x2c}, {0x77, 0x24},
		 {0x78, 0x22}, {0x79, 0x28}, {0x7a, 0x2a}, {0x7b, 0x31},
		 {0x7c, 0x0f}, {0x7d, 0x1e}, {0x7e, 0x3d}, {0x7f, 0x62},
		 {0x80, 0x6d}, {0x81, 0x75}, {0x82, 0x7b}, {0x83, 0x81},
		 {0x84, 0x87}, {0x85, 0x8d}, {0x86, 0x98}, {0x87, 0xa1},
		 {0x88, 0xb2}, {0x89, 0xc6}, {0x8a, 0xdb}},
		{{0x6c, 0xf0}, {0x6d, 0xf0}, {0x6e, 0xf0}, {0x6f, 0x84},
		 {0x70, 0x58}, {0x71, 0x48}, {0x72, 0x40}, {0x73, 0x40},
		 {0x74, 0x28}, {0x75, 0x28}, {0x76, 0x28}, {0x77, 0x24},
		 {0x78, 0x26}, {0x79, 0x28}, {0x7a, 0x28}, {0x7b, 0x34},
		 {0x7c, 0x0f}, {0x7d, 0x1e}, {0x7e, 0x3c}, {0x7f, 0x5d},
		 {0x80, 0x68}, {0x81, 0x71}, {0x82, 0x79}, {0x83, 0x81},
		 {0x84, 0x86}, {0x85, 0x8b}, {0x86, 0x95}, {0x87, 0x9e},
		 {0x88, 0xb1}, {0x89, 0xc5}, {0x8a, 0xd9}},
		{{0x6c, 0xf0}, {0x6d, 0xf0}, {0x6e, 0xf0}, {0x6f, 0x70},
		 {0x70, 0x58}, {0x71, 0x58}, {0x72, 0x48}, {0x73, 0x48},
		 {0x74, 0x38}, {0x75, 0x40}, {0x76, 0x34}, {0x77, 0x34},
		 {0x78, 0x2e}, {0x79, 0x28}, {0x7a, 0x24}, {0x7b, 0x22},
		 {0x7c, 0x0f}, {0x7d, 0x1e}, {0x7e, 0x3c}, {0x7f, 0x58},
		 {0x80, 0x63}, {0x81, 0x6e}, {0x82, 0x77}, {0x83, 0x80},
		 {0x84, 0x87}, {0x85, 0x8f}, {0x86, 0x9c}, {0x87, 0xa9},
		 {0x88, 0xc0}, {0x89, 0xd4}, {0x8a, 0xe6}},
		{{0x6c, 0xa0}, {0x6d, 0xf0}, {0x6e, 0x90}, {0x6f, 0x80},
		 {0x70, 0x70}, {0x71, 0x80}, {0x72, 0x60}, {0x73, 0x60},
		 {0x74, 0x58}, {0x75, 0x60}, {0x76, 0x4c}, {0x77, 0x38},
		 {0x78, 0x38}, {0x79, 0x2a}, {0x7a, 0x20}, {0x7b, 0x0e},
		 {0x7c, 0x0a}, {0x7d, 0x14}, {0x7e, 0x26}, {0x7f, 0x46},
		 {0x80, 0x54}, {0x81, 0x64}, {0x82, 0x70}, {0x83, 0x7c},
		 {0x84, 0x87}, {0x85, 0x93}, {0x86, 0xa6}, {0x87, 0xb4},
		 {0x88, 0xd0}, {0x89, 0xe5}, {0x8a, 0xf5}},
		{{0x6c, 0x60}, {0x6d, 0x80}, {0x6e, 0x60}, {0x6f, 0x80},
		 {0x70, 0x80}, {0x71, 0x80}, {0x72, 0x88}, {0x73, 0x30},
		 {0x74, 0x70}, {0x75, 0x68}, {0x76, 0x64}, {0x77, 0x50},
		 {0x78, 0x3c}, {0x79, 0x22}, {0x7a, 0x10}, {0x7b, 0x08},
		 {0x7c, 0x06}, {0x7d, 0x0e}, {0x7e, 0x1a}, {0x7f, 0x3a},
		 {0x80, 0x4a}, {0x81, 0x5a}, {0x82, 0x6b}, {0x83, 0x7b},
		 {0x84, 0x89}, {0x85, 0x96}, {0x86, 0xaf}, {0x87, 0xc3},
		 {0x88, 0xe1}, {0x89, 0xf2}, {0x8a, 0xfa}},
		{{0x6c, 0x20}, {0x6d, 0x40}, {0x6e, 0x20}, {0x6f, 0x60},
		 {0x70, 0x88}, {0x71, 0xc8}, {0x72, 0xc0}, {0x73, 0xb8},
		 {0x74, 0xa8}, {0x75, 0xb8}, {0x76, 0x80}, {0x77, 0x5c},
		 {0x78, 0x26}, {0x79, 0x10}, {0x7a, 0x08}, {0x7b, 0x04},
		 {0x7c, 0x02}, {0x7d, 0x06}, {0x7e, 0x0a}, {0x7f, 0x22},
		 {0x80, 0x33}, {0x81, 0x4c}, {0x82, 0x64}, {0x83, 0x7b},
		 {0x84, 0x90}, {0x85, 0xa7}, {0x86, 0xc7}, {0x87, 0xde},
		 {0x88, 0xf1}, {0x89, 0xf9}, {0x8a, 0xfd}},
	};

	val = sd->ctrls[CONTRAST].val;
	switch (sd->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
		i2c_w(sd, OV7610_REG_CNT, val);
		break;
	case SEN_OV6630:
	case SEN_OV66308AF:
		i2c_w_mask(sd, OV7610_REG_CNT, val >> 4, 0x0f);
		break;
	case SEN_OV8610: {
		static const u8 ctab[] = {
			0x03, 0x09, 0x0b, 0x0f, 0x53, 0x6f, 0x35, 0x7f
		};

		
		i2c_w(sd, 0x64, ctab[val >> 5]);
		break;
	    }
	case SEN_OV7620:
	case SEN_OV7620AE: {
		static const u8 ctab[] = {
			0x01, 0x05, 0x09, 0x11, 0x15, 0x35, 0x37, 0x57,
			0x5b, 0xa5, 0xa7, 0xc7, 0xc9, 0xcf, 0xef, 0xff
		};

		
		i2c_w(sd, 0x64, ctab[val >> 4]);
		break;
	    }
	case SEN_OV7660:
		write_i2c_regvals(sd, contrast_7660[val],
					ARRAY_SIZE(contrast_7660[0]));
		break;
	case SEN_OV7670:
		
		i2c_w(sd, OV7670_R56_CONTRAS, val >> 1);
		break;
	}
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (!sd->ctrls[AUTOGAIN].val)
		i2c_w(sd, 0x10, sd->ctrls[EXPOSURE].val);
}

static void setcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int val;
	static const struct ov_i2c_regvals colors_7660[][6] = {
		{{0x4f, 0x28}, {0x50, 0x2a}, {0x51, 0x02}, {0x52, 0x0a},
		 {0x53, 0x19}, {0x54, 0x23}},
		{{0x4f, 0x47}, {0x50, 0x4a}, {0x51, 0x03}, {0x52, 0x11},
		 {0x53, 0x2c}, {0x54, 0x3e}},
		{{0x4f, 0x66}, {0x50, 0x6b}, {0x51, 0x05}, {0x52, 0x19},
		 {0x53, 0x40}, {0x54, 0x59}},
		{{0x4f, 0x84}, {0x50, 0x8b}, {0x51, 0x06}, {0x52, 0x20},
		 {0x53, 0x53}, {0x54, 0x73}},
		{{0x4f, 0xa3}, {0x50, 0xab}, {0x51, 0x08}, {0x52, 0x28},
		 {0x53, 0x66}, {0x54, 0x8e}},
	};

	val = sd->ctrls[COLORS].val;
	switch (sd->sensor) {
	case SEN_OV8610:
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV66308AF:
		i2c_w(sd, OV7610_REG_SAT, val);
		break;
	case SEN_OV7620:
	case SEN_OV7620AE:
		
		i2c_w(sd, OV7610_REG_SAT, val);
		break;
	case SEN_OV7640:
	case SEN_OV7648:
		i2c_w(sd, OV7610_REG_SAT, val & 0xf0);
		break;
	case SEN_OV7660:
		write_i2c_regvals(sd, colors_7660[val],
					ARRAY_SIZE(colors_7660[0]));
		break;
	case SEN_OV7670:
		
		break;
	}
}

static void setautobright(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	i2c_w_mask(sd, 0x2d, sd->ctrls[AUTOBRIGHT].val ? 0x10 : 0x00, 0x10);
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->ctrls[AUTOGAIN].val = val;
	if (val) {
		gspca_dev->ctrl_inac |= (1 << EXPOSURE);
	} else {
		gspca_dev->ctrl_inac &= ~(1 << EXPOSURE);
		sd->ctrls[EXPOSURE].val = i2c_r(sd, 0x10);
	}
	if (gspca_dev->streaming)
		setautogain(gspca_dev);
	return gspca_dev->usb_err;
}

static void setfreq_i(struct sd *sd)
{
	if (sd->sensor == SEN_OV7660
	 || sd->sensor == SEN_OV7670) {
		switch (sd->ctrls[FREQ].val) {
		case 0: 
			i2c_w_mask(sd, OV7670_R13_COM8, 0, OV7670_COM8_BFILT);
			break;
		case 1: 
			i2c_w_mask(sd, OV7670_R13_COM8, OV7670_COM8_BFILT,
				   OV7670_COM8_BFILT);
			i2c_w_mask(sd, OV7670_R3B_COM11, 0x08, 0x18);
			break;
		case 2: 
			i2c_w_mask(sd, OV7670_R13_COM8, OV7670_COM8_BFILT,
				   OV7670_COM8_BFILT);
			i2c_w_mask(sd, OV7670_R3B_COM11, 0x00, 0x18);
			break;
		case 3: 
			i2c_w_mask(sd, OV7670_R13_COM8, OV7670_COM8_BFILT,
				   OV7670_COM8_BFILT);
			i2c_w_mask(sd, OV7670_R3B_COM11, OV7670_COM11_HZAUTO,
				   0x18);
			break;
		}
	} else {
		switch (sd->ctrls[FREQ].val) {
		case 0: 
			i2c_w_mask(sd, 0x2d, 0x00, 0x04);
			i2c_w_mask(sd, 0x2a, 0x00, 0x80);
			break;
		case 1: 
			i2c_w_mask(sd, 0x2d, 0x04, 0x04);
			i2c_w_mask(sd, 0x2a, 0x80, 0x80);
			
			if (sd->sensor == SEN_OV6620 ||
			    sd->sensor == SEN_OV6630 ||
			    sd->sensor == SEN_OV66308AF)
				i2c_w(sd, 0x2b, 0x5e);
			else
				i2c_w(sd, 0x2b, 0xac);
			break;
		case 2: 
			i2c_w_mask(sd, 0x2d, 0x04, 0x04);
			if (sd->sensor == SEN_OV6620 ||
			    sd->sensor == SEN_OV6630 ||
			    sd->sensor == SEN_OV66308AF) {
				
				i2c_w_mask(sd, 0x2a, 0x80, 0x80);
				i2c_w(sd, 0x2b, 0xa8);
			} else {
				
				i2c_w_mask(sd, 0x2a, 0x00, 0x80);
			}
			break;
		}
	}
}
static void setfreq(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	setfreq_i(sd);

	
	if (sd->bridge == BRIDGE_W9968CF)
		w9968cf_set_crop_window(sd);
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
			struct v4l2_querymenu *menu)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (menu->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch (menu->index) {
		case 0:		
			strcpy((char *) menu->name, "NoFliker");
			return 0;
		case 1:		
			strcpy((char *) menu->name, "50 Hz");
			return 0;
		case 2:		
			strcpy((char *) menu->name, "60 Hz");
			return 0;
		case 3:
			if (sd->sensor != SEN_OV7670)
				return -EINVAL;

			strcpy((char *) menu->name, "Automatic");
			return 0;
		}
		break;
	}
	return -EINVAL;
}

static int sd_get_jcomp(struct gspca_dev *gspca_dev,
			struct v4l2_jpegcompression *jcomp)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->bridge != BRIDGE_W9968CF)
		return -EINVAL;

	memset(jcomp, 0, sizeof *jcomp);
	jcomp->quality = sd->quality;
	jcomp->jpeg_markers = V4L2_JPEG_MARKER_DHT | V4L2_JPEG_MARKER_DQT |
			      V4L2_JPEG_MARKER_DRI;
	return 0;
}

static int sd_set_jcomp(struct gspca_dev *gspca_dev,
			struct v4l2_jpegcompression *jcomp)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->bridge != BRIDGE_W9968CF)
		return -EINVAL;

	if (gspca_dev->streaming)
		return -EBUSY;

	if (jcomp->quality < QUALITY_MIN)
		sd->quality = QUALITY_MIN;
	else if (jcomp->quality > QUALITY_MAX)
		sd->quality = QUALITY_MAX;
	else
		sd->quality = jcomp->quality;

	
	sd_get_jcomp(gspca_dev, jcomp);

	return 0;
}

static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.isoc_init = sd_isoc_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.stop0 = sd_stop0,
	.pkt_scan = sd_pkt_scan,
	.dq_callback = sd_reset_snapshot,
	.querymenu = sd_querymenu,
	.get_jcomp = sd_get_jcomp,
	.set_jcomp = sd_set_jcomp,
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.other_input = 1,
#endif
};

static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x041e, 0x4003), .driver_info = BRIDGE_W9968CF },
	{USB_DEVICE(0x041e, 0x4052),
		.driver_info = BRIDGE_OV519 | BRIDGE_INVERT_LED },
	{USB_DEVICE(0x041e, 0x405f), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x041e, 0x4060), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x041e, 0x4061), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x041e, 0x4064), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x041e, 0x4067), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x041e, 0x4068), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x045e, 0x028c),
		.driver_info = BRIDGE_OV519 | BRIDGE_INVERT_LED },
	{USB_DEVICE(0x054c, 0x0154), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x054c, 0x0155), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x05a9, 0x0511), .driver_info = BRIDGE_OV511 },
	{USB_DEVICE(0x05a9, 0x0518), .driver_info = BRIDGE_OV518 },
	{USB_DEVICE(0x05a9, 0x0519),
		.driver_info = BRIDGE_OV519 | BRIDGE_INVERT_LED },
	{USB_DEVICE(0x05a9, 0x0530),
		.driver_info = BRIDGE_OV519 | BRIDGE_INVERT_LED },
	{USB_DEVICE(0x05a9, 0x2800), .driver_info = BRIDGE_OVFX2 },
	{USB_DEVICE(0x05a9, 0x4519), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x05a9, 0x8519), .driver_info = BRIDGE_OV519 },
	{USB_DEVICE(0x05a9, 0xa511), .driver_info = BRIDGE_OV511PLUS },
	{USB_DEVICE(0x05a9, 0xa518), .driver_info = BRIDGE_OV518PLUS },
	{USB_DEVICE(0x0813, 0x0002), .driver_info = BRIDGE_OV511PLUS },
	{USB_DEVICE(0x0b62, 0x0059), .driver_info = BRIDGE_OVFX2 },
	{USB_DEVICE(0x0e96, 0xc001), .driver_info = BRIDGE_OVFX2 },
	{USB_DEVICE(0x1046, 0x9967), .driver_info = BRIDGE_W9968CF },
	{USB_DEVICE(0x8020, 0xef04), .driver_info = BRIDGE_OVFX2 },
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

module_usb_driver(sd_driver);

module_param(frame_rate, int, 0644);
MODULE_PARM_DESC(frame_rate, "Frame rate (5, 10, 15, 20 or 30 fps)");
