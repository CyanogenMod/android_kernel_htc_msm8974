/*
 *		sonix sn9c102 (bayer) library
 *
 * Copyright (C) 2009-2011 Jean-François Moine <http://moinejf.free.fr>
 * Copyright (C) 2003 2004 Michel Xhaard mxhaard@magic.fr
 * Add Pas106 Stefano Mozzi (C) 2004
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
 */


#define MODULE_NAME "sonixb"

#include <linux/input.h>
#include "gspca.h"

MODULE_AUTHOR("Jean-François Moine <http://moinejf.free.fr>");
MODULE_DESCRIPTION("GSPCA/SN9C102 USB Camera Driver");
MODULE_LICENSE("GPL");

enum e_ctrl {
	BRIGHTNESS,
	GAIN,
	EXPOSURE,
	AUTOGAIN,
	FREQ,
	NCTRLS		
};

struct sd {
	struct gspca_dev gspca_dev;	

	struct gspca_ctrl ctrls[NCTRLS];

	atomic_t avg_lum;
	int prev_avg_lum;
	int exp_too_low_cnt;
	int exp_too_high_cnt;
	int header_read;
	u8 header[12]; 

	unsigned char autogain_ignore_frames;
	unsigned char frames_to_drop;

	__u8 bridge;			
#define BRIDGE_101 0
#define BRIDGE_102 0 
#define BRIDGE_103 1

	__u8 sensor;			
#define SENSOR_HV7131D 0
#define SENSOR_HV7131R 1
#define SENSOR_OV6650 2
#define SENSOR_OV7630 3
#define SENSOR_PAS106 4
#define SENSOR_PAS202 5
#define SENSOR_TAS5110C 6
#define SENSOR_TAS5110D 7
#define SENSOR_TAS5130CXX 8
	__u8 reg11;
};

typedef const __u8 sensor_init_t[8];

struct sensor_data {
	const __u8 *bridge_init;
	sensor_init_t *sensor_init;
	int sensor_init_size;
	int flags;
	unsigned ctrl_dis;
	__u8 sensor_addr;
};

#define F_GAIN 0x01		
#define F_SIF  0x02		
#define F_COARSE_EXPO 0x04	

#define MODE_RAW 0x10		
#define MODE_REDUCED_SIF 0x20	

#define NO_EXPO ((1 << EXPOSURE) | (1 << AUTOGAIN))
#define NO_FREQ (1 << FREQ)
#define NO_BRIGHTNESS (1 << BRIGHTNESS)

#define COMP 0xc7		
#define COMP1 0xc9		

#define MCK_INIT 0x63
#define MCK_INIT1 0x20		

#define SYS_CLK 0x04

#define SENS(bridge, sensor, _flags, _ctrl_dis, _sensor_addr) \
{ \
	.bridge_init = bridge, \
	.sensor_init = sensor, \
	.sensor_init_size = sizeof(sensor), \
	.flags = _flags, .ctrl_dis = _ctrl_dis, .sensor_addr = _sensor_addr \
}

#define AUTOGAIN_IGNORE_FRAMES 1

static void setbrightness(struct gspca_dev *gspca_dev);
static void setgain(struct gspca_dev *gspca_dev);
static void setexposure(struct gspca_dev *gspca_dev);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static void setfreq(struct gspca_dev *gspca_dev);

static const struct ctrl sd_ctrls[NCTRLS] = {
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
	    .set_control = setbrightness
	},
[GAIN] = {
	    {
		.id      = V4L2_CID_GAIN,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gain",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
#define GAIN_KNEE 230
		.default_value = 127,
	    },
	    .set_control = setgain
	},
[EXPOSURE] = {
		{
			.id = V4L2_CID_EXPOSURE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Exposure",
			.minimum = 0,
			.maximum = 1023,
			.step = 1,
			.default_value = 66,
				
#define EXPOSURE_KNEE 200	
			.flags = 0,
		},
		.set_control = setexposure
	},
#define COARSE_EXPOSURE_MIN 2
#define COARSE_EXPOSURE_MAX 15
#define COARSE_EXPOSURE_DEF  2 
[AUTOGAIN] = {
		{
			.id = V4L2_CID_AUTOGAIN,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Automatic Gain (and Exposure)",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
#define AUTOGAIN_DEF 1
			.default_value = AUTOGAIN_DEF,
			.flags = V4L2_CTRL_FLAG_UPDATE
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
#define FREQ_DEF 0
			.default_value = FREQ_DEF,
		},
		.set_control = setfreq
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{160, 120, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2 | MODE_RAW},
	{160, 120, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{320, 240, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};
static const struct v4l2_pix_format sif_mode[] = {
	{160, 120, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1 | MODE_RAW | MODE_REDUCED_SIF},
	{160, 120, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1 | MODE_REDUCED_SIF},
	{176, 144, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1 | MODE_RAW},
	{176, 144, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{320, 240, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0 | MODE_REDUCED_SIF},
	{352, 288, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 5 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

static const __u8 initHv7131d[] = {
	0x04, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x00,
	0x28, 0x1e, 0x60, 0x8e, 0x42,
};
static const __u8 hv7131d_sensor_init[][8] = {
	{0xa0, 0x11, 0x01, 0x04, 0x00, 0x00, 0x00, 0x17},
	{0xa0, 0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x17},
	{0xa0, 0x11, 0x28, 0x00, 0x00, 0x00, 0x00, 0x17},
	{0xa0, 0x11, 0x30, 0x30, 0x00, 0x00, 0x00, 0x17}, 
	{0xa0, 0x11, 0x34, 0x02, 0x00, 0x00, 0x00, 0x17}, 
};

static const __u8 initHv7131r[] = {
	0x46, 0x77, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x01, 0x00,
	0x28, 0x1e, 0x60, 0x8a, 0x20,
};
static const __u8 hv7131r_sensor_init[][8] = {
	{0xc0, 0x11, 0x31, 0x38, 0x2a, 0x2e, 0x00, 0x10},
	{0xa0, 0x11, 0x01, 0x08, 0x2a, 0x2e, 0x00, 0x10},
	{0xb0, 0x11, 0x20, 0x00, 0xd0, 0x2e, 0x00, 0x10},
	{0xc0, 0x11, 0x25, 0x03, 0x0e, 0x28, 0x00, 0x16},
	{0xa0, 0x11, 0x30, 0x10, 0x0e, 0x28, 0x00, 0x15},
};
static const __u8 initOv6650[] = {
	0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x01, 0x0a, 0x16, 0x12, 0x68, 0x8b,
	0x10,
};
static const __u8 ov6650_sensor_init[][8] = {
	

	
	{0xa0, 0x60, 0x12, 0x80, 0x00, 0x00, 0x00, 0x10},
	
	{0xd0, 0x60, 0x11, 0xc0, 0x1b, 0x18, 0xc1, 0x10},
	
	{0xb0, 0x60, 0x15, 0x00, 0x02, 0x18, 0xc1, 0x10},
	{0xd0, 0x60, 0x26, 0x01, 0x14, 0xd8, 0xa4, 0x10}, 
	{0xd0, 0x60, 0x26, 0x01, 0x14, 0xd8, 0xa4, 0x10},
	{0xa0, 0x60, 0x30, 0x3d, 0x0a, 0xd8, 0xa4, 0x10},
	
	{0xa0, 0x60, 0x61, 0x08, 0x00, 0x00, 0x00, 0x10},
	
	{0xa0, 0x60, 0x68, 0x04, 0x68, 0xd8, 0xa4, 0x10},
	{0xd0, 0x60, 0x17, 0x24, 0xd6, 0x04, 0x94, 0x10}, 
};

static const __u8 initOv7630[] = {
	0x04, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,	
	0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x01, 0x01, 0x0a,				
	0x28, 0x1e,			
	0x68, 0x8f, MCK_INIT1,				
};
static const __u8 ov7630_sensor_init[][8] = {
	{0xa0, 0x21, 0x12, 0x80, 0x00, 0x00, 0x00, 0x10},
	{0xb0, 0x21, 0x01, 0x77, 0x3a, 0x00, 0x00, 0x10},
	{0xd0, 0x21, 0x12, 0x5c, 0x00, 0x80, 0x34, 0x10},	
	{0xa0, 0x21, 0x1b, 0x04, 0x00, 0x80, 0x34, 0x10},
	{0xa0, 0x21, 0x20, 0x44, 0x00, 0x80, 0x34, 0x10},
	{0xa0, 0x21, 0x23, 0xee, 0x00, 0x80, 0x34, 0x10},
	{0xd0, 0x21, 0x26, 0xa0, 0x9a, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x2a, 0x80, 0x00, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x2f, 0x3d, 0x24, 0xa0, 0x30, 0x10},
	{0xa0, 0x21, 0x32, 0x86, 0x24, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x60, 0xa9, 0x4a, 0xa0, 0x30, 0x10},
	{0xa0, 0x21, 0x65, 0x00, 0x42, 0xa0, 0x30, 0x10},
	{0xa0, 0x21, 0x69, 0x38, 0x42, 0xa0, 0x30, 0x10},
	{0xc0, 0x21, 0x6f, 0x88, 0x0b, 0x00, 0x30, 0x10},
	{0xc0, 0x21, 0x74, 0x21, 0x8e, 0x00, 0x30, 0x10},
	{0xa0, 0x21, 0x7d, 0xf7, 0x8e, 0x00, 0x30, 0x10},
	{0xd0, 0x21, 0x17, 0x1c, 0xbd, 0x06, 0xf6, 0x10},
};

static const __u8 initPas106[] = {
	0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x01, 0x00,
	0x16, 0x12, 0x24, COMP1, MCK_INIT1,
};


static const __u8 pas106_sensor_init[][8] = {
	
	{ 0xa1, 0x40, 0x02, 0x04, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x03, 0x13, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x04, 0x06, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x05, 0x65, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x06, 0xcd, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x07, 0xc1, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x08, 0x06, 0x00, 0x00, 0x00, 0x14 },
	{ 0xa1, 0x40, 0x08, 0x06, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x09, 0x05, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0a, 0x04, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0b, 0x04, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0c, 0x05, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0e, 0x0e, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x10, 0x06, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x11, 0x06, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x12, 0x06, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x14, 0x02, 0x00, 0x00, 0x00, 0x14 },
	
	{ 0xa1, 0x40, 0x13, 0x01, 0x00, 0x00, 0x00, 0x14 },
};

static const __u8 initPas202[] = {
	0x44, 0x44, 0x21, 0x30, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x06, 0x03, 0x0a,
	0x28, 0x1e, 0x20, 0x89, 0x20,
};

static const __u8 pas202_sensor_init[][8] = {
	{0xa0, 0x40, 0x02, 0x04, 0x00, 0x00, 0x00, 0x10},
	{0xd0, 0x40, 0x04, 0x07, 0x34, 0x00, 0x09, 0x10},
	{0xd0, 0x40, 0x08, 0x01, 0x00, 0x00, 0x01, 0x10},
	{0xd0, 0x40, 0x0c, 0x00, 0x0c, 0x01, 0x32, 0x10},
	{0xd0, 0x40, 0x10, 0x00, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x15, 0x70, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x18, 0x00, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x11, 0x01, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x03, 0x56, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x11, 0x01, 0x01, 0x00, 0x63, 0x10},
};

static const __u8 initTas5110c[] = {
	0x44, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x45, 0x09, 0x0a,
	0x16, 0x12, 0x60, 0x86, 0x2b,
};
static const __u8 initTas5110d[] = {
	0x44, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x41, 0x09, 0x0a,
	0x16, 0x12, 0x60, 0x86, 0x2b,
};
static const __u8 tas5110c_sensor_init[][8] = {
	{0x30, 0x11, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x10},
	{0x30, 0x11, 0x02, 0x20, 0xa9, 0x00, 0x00, 0x10},
};
/* Known TAS5110D registers
 * reg02: gain, bit order reversed!! 0 == max gain, 255 == min gain
 * reg03: bit3: vflip, bit4: ~hflip, bit7: ~gainboost (~ == inverted)
 *        Note: writing reg03 seems to only work when written together with 02
 */
static const __u8 tas5110d_sensor_init[][8] = {
	{0xa0, 0x61, 0x9a, 0xca, 0x00, 0x00, 0x00, 0x17}, 
};

static const __u8 initTas5130[] = {
	0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x68, 0x0c, 0x0a,
	0x28, 0x1e, 0x60, COMP, MCK_INIT,
};
static const __u8 tas5130_sensor_init[][8] = {
	{0x30, 0x11, 0x00, 0x40, 0x01, 0x00, 0x00, 0x10},
					
	{0x30, 0x11, 0x02, 0x20, 0x70, 0x00, 0x00, 0x10},
};

static const struct sensor_data sensor_data[] = {
SENS(initHv7131d, hv7131d_sensor_init, F_GAIN, NO_BRIGHTNESS|NO_FREQ, 0),
SENS(initHv7131r, hv7131r_sensor_init, 0, NO_BRIGHTNESS|NO_EXPO|NO_FREQ, 0),
SENS(initOv6650, ov6650_sensor_init, F_GAIN|F_SIF, 0, 0x60),
SENS(initOv7630, ov7630_sensor_init, F_GAIN, 0, 0x21),
SENS(initPas106, pas106_sensor_init, F_GAIN|F_SIF, NO_FREQ, 0),
SENS(initPas202, pas202_sensor_init, F_GAIN, NO_FREQ, 0),
SENS(initTas5110c, tas5110c_sensor_init, F_GAIN|F_SIF|F_COARSE_EXPO,
	NO_BRIGHTNESS|NO_FREQ, 0),
SENS(initTas5110d, tas5110d_sensor_init, F_GAIN|F_SIF|F_COARSE_EXPO,
	NO_BRIGHTNESS|NO_FREQ, 0),
SENS(initTas5130, tas5130_sensor_init, F_GAIN,
	NO_BRIGHTNESS|NO_EXPO|NO_FREQ, 0),
};

static void reg_r(struct gspca_dev *gspca_dev,
		  __u16 value)
{
	usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			0,			
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			value,
			0,			
			gspca_dev->usb_buf, 1,
			500);
}

static void reg_w(struct gspca_dev *gspca_dev,
		  __u16 value,
		  const __u8 *buffer,
		  int len)
{
#ifdef GSPCA_DEBUG
	if (len > USB_BUF_SZ) {
		PDEBUG(D_ERR|D_PACK, "reg_w: buffer overflow");
		return;
	}
#endif
	memcpy(gspca_dev->usb_buf, buffer, len);
	usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0x08,			
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			value,
			0,			
			gspca_dev->usb_buf, len,
			500);
}

static int i2c_w(struct gspca_dev *gspca_dev, const __u8 *buffer)
{
	int retry = 60;

	
	reg_w(gspca_dev, 0x08, buffer, 8);
	while (retry--) {
		msleep(10);
		reg_r(gspca_dev, 0x08);
		if (gspca_dev->usb_buf[0] & 0x04) {
			if (gspca_dev->usb_buf[0] & 0x08)
				return -1;
			return 0;
		}
	}
	return -1;
}

static void i2c_w_vector(struct gspca_dev *gspca_dev,
			const __u8 buffer[][8], int len)
{
	for (;;) {
		reg_w(gspca_dev, 0x08, *buffer, 8);
		len -= 8;
		if (len <= 0)
			break;
		buffer++;
	}
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
	case  SENSOR_OV6650:
	case  SENSOR_OV7630: {
		__u8 i2cOV[] =
			{0xa0, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x10};

		
		i2cOV[1] = sensor_data[sd->sensor].sensor_addr;
		i2cOV[3] = sd->ctrls[BRIGHTNESS].val;
		if (i2c_w(gspca_dev, i2cOV) < 0)
			goto err;
		break;
	    }
	case SENSOR_PAS106:
	case SENSOR_PAS202: {
		__u8 i2cpbright[] =
			{0xb0, 0x40, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x16};
		__u8 i2cpdoit[] =
			{0xa0, 0x40, 0x11, 0x01, 0x00, 0x00, 0x00, 0x16};

		
		if (sd->sensor == SENSOR_PAS106) {
			i2cpbright[2] = 7;
			i2cpdoit[2] = 0x13;
		}

		if (sd->ctrls[BRIGHTNESS].val < 127) {
			
			i2cpbright[3] = 0x01;
			
			i2cpbright[4] = 127 - sd->ctrls[BRIGHTNESS].val;
		} else
			i2cpbright[4] = sd->ctrls[BRIGHTNESS].val - 127;

		if (i2c_w(gspca_dev, i2cpbright) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		break;
	    }
	}
	return;
err:
	PDEBUG(D_ERR, "i2c error brightness");
}

static void setsensorgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 gain = sd->ctrls[GAIN].val;

	switch (sd->sensor) {
	case SENSOR_HV7131D: {
		__u8 i2c[] =
			{0xc0, 0x11, 0x31, 0x00, 0x00, 0x00, 0x00, 0x17};

		i2c[3] = 0x3f - (gain / 4);
		i2c[4] = 0x3f - (gain / 4);
		i2c[5] = 0x3f - (gain / 4);

		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }
	case SENSOR_TAS5110C:
	case SENSOR_TAS5130CXX: {
		__u8 i2c[] =
			{0x30, 0x11, 0x02, 0x20, 0x70, 0x00, 0x00, 0x10};

		i2c[4] = 255 - gain;
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }
	case SENSOR_TAS5110D: {
		__u8 i2c[] = {
			0xb0, 0x61, 0x02, 0x00, 0x10, 0x00, 0x00, 0x17 };
		gain = 255 - gain;
		
		i2c[3] |= (gain & 0x80) >> 7;
		i2c[3] |= (gain & 0x40) >> 5;
		i2c[3] |= (gain & 0x20) >> 3;
		i2c[3] |= (gain & 0x10) >> 1;
		i2c[3] |= (gain & 0x08) << 1;
		i2c[3] |= (gain & 0x04) << 3;
		i2c[3] |= (gain & 0x02) << 5;
		i2c[3] |= (gain & 0x01) << 7;
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }

	case SENSOR_OV6650:
		gain >>= 1;
		
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

		i2c[1] = sensor_data[sd->sensor].sensor_addr;
		i2c[3] = gain >> 2;
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }
	case SENSOR_PAS106:
	case SENSOR_PAS202: {
		__u8 i2cpgain[] =
			{0xa0, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x15};
		__u8 i2cpcolorgain[] =
			{0xc0, 0x40, 0x07, 0x00, 0x00, 0x00, 0x00, 0x15};
		__u8 i2cpdoit[] =
			{0xa0, 0x40, 0x11, 0x01, 0x00, 0x00, 0x00, 0x16};

		
		if (sd->sensor == SENSOR_PAS106) {
			i2cpgain[2] = 0x0e;
			i2cpcolorgain[0] = 0xd0;
			i2cpcolorgain[2] = 0x09;
			i2cpdoit[2] = 0x13;
		}

		i2cpgain[3] = gain >> 3;
		i2cpcolorgain[3] = gain >> 4;
		i2cpcolorgain[4] = gain >> 4;
		i2cpcolorgain[5] = gain >> 4;
		i2cpcolorgain[6] = gain >> 4;

		if (i2c_w(gspca_dev, i2cpgain) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpcolorgain) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		break;
	    }
	}
	return;
err:
	PDEBUG(D_ERR, "i2c error gain");
}

static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 gain;
	__u8 buf[3] = { 0, 0, 0 };

	if (sensor_data[sd->sensor].flags & F_GAIN) {
		
		setsensorgain(gspca_dev);
		return;
	}

	if (sd->bridge == BRIDGE_103) {
		gain = sd->ctrls[GAIN].val >> 1;
		buf[0] = gain; 
		buf[1] = gain; 
		buf[2] = gain; 
		reg_w(gspca_dev, 0x05, buf, 3);
	} else {
		gain = sd->ctrls[GAIN].val >> 4;
		buf[0] = gain << 4 | gain; 
		buf[1] = gain; 
		reg_w(gspca_dev, 0x10, buf, 2);
	}
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
	case SENSOR_HV7131D: {
		__u8 i2c[] = {0xc0, 0x11, 0x25, 0x00, 0x00, 0x00, 0x00, 0x17};
		u16 reg = sd->ctrls[EXPOSURE].val * 6;

		i2c[3] = reg >> 8;
		i2c[4] = reg & 0xff;
		if (i2c_w(gspca_dev, i2c) != 0)
			goto err;
		break;
	    }
	case SENSOR_TAS5110C:
	case SENSOR_TAS5110D: {
		u8 reg = sd->ctrls[EXPOSURE].val;

		reg = (reg << 4) | 0x0b;
		reg_w(gspca_dev, 0x19, &reg, 1);
		break;
	    }
	case SENSOR_OV6650:
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xb0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10};
		int reg10, reg11, reg10_max;

		if (sd->sensor == SENSOR_OV6650) {
			reg10_max = 0x4d;
			i2c[4] = 0xc0; 
		} else
			reg10_max = 0x41;

		reg11 = (15 * sd->ctrls[EXPOSURE].val + 999) / 1000;
		if (reg11 < 1)
			reg11 = 1;
		else if (reg11 > 16)
			reg11 = 16;

		if (gspca_dev->width == 640 && reg11 < 4)
			reg11 = 4;

		reg10 = (sd->ctrls[EXPOSURE].val * 15 * reg10_max)
				/ (1000 * reg11);

		if (sd->ctrls[AUTOGAIN].val && reg10 < 10)
			reg10 = 10;
		else if (reg10 > reg10_max)
			reg10 = reg10_max;

		
		i2c[1] = sensor_data[sd->sensor].sensor_addr;
		i2c[3] = reg10;
		i2c[4] |= reg11 - 1;

		
		if (sd->reg11 == reg11)
			i2c[0] = 0xa0;

		if (i2c_w(gspca_dev, i2c) == 0)
			sd->reg11 = reg11;
		else
			goto err;
		break;
	    }
	case SENSOR_PAS202: {
		__u8 i2cpframerate[] =
			{0xb0, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x16};
		__u8 i2cpexpo[] =
			{0xa0, 0x40, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x16};
		const __u8 i2cpdoit[] =
			{0xa0, 0x40, 0x11, 0x01, 0x00, 0x00, 0x00, 0x16};
		int framerate_ctrl;

		if (sd->ctrls[EXPOSURE].val < 200) {
			i2cpexpo[3] = 255 - (sd->ctrls[EXPOSURE].val * 255)
						/ 200;
			framerate_ctrl = 500;
		} else {
			framerate_ctrl = (sd->ctrls[EXPOSURE].val - 200)
							* 1000 / 229 +  500;
		}

		i2cpframerate[3] = framerate_ctrl >> 6;
		i2cpframerate[4] = framerate_ctrl & 0x3f;
		if (i2c_w(gspca_dev, i2cpframerate) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpexpo) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		break;
	    }
	case SENSOR_PAS106: {
		__u8 i2cpframerate[] =
			{0xb1, 0x40, 0x03, 0x00, 0x00, 0x00, 0x00, 0x14};
		__u8 i2cpexpo[] =
			{0xa1, 0x40, 0x05, 0x00, 0x00, 0x00, 0x00, 0x14};
		const __u8 i2cpdoit[] =
			{0xa1, 0x40, 0x13, 0x01, 0x00, 0x00, 0x00, 0x14};
		int framerate_ctrl;

		if (sd->ctrls[EXPOSURE].val < 150) {
			i2cpexpo[3] = 150 - sd->ctrls[EXPOSURE].val;
			framerate_ctrl = 300;
		} else {
			framerate_ctrl = (sd->ctrls[EXPOSURE].val - 150)
						* 1000 / 230 + 300;
		}

		i2cpframerate[3] = framerate_ctrl >> 4;
		i2cpframerate[4] = framerate_ctrl & 0x0f;
		if (i2c_w(gspca_dev, i2cpframerate) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpexpo) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		break;
	    }
	}
	return;
err:
	PDEBUG(D_ERR, "i2c error exposure");
}

static void setfreq(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
	case SENSOR_OV6650:
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xa0, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x10};
		switch (sd->ctrls[FREQ].val) {
		default:
			i2c[3] = 0;
			break;
		case 1:			
			i2c[3] = (sd->sensor == SENSOR_OV6650)
					? 0x4f : 0x8a;
			break;
		}
		i2c[1] = sensor_data[sd->sensor].sensor_addr;
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error setfreq");
		break;
	    }
	}
}

#include "autogain_functions.h"

static void do_autogain(struct gspca_dev *gspca_dev)
{
	int deadzone, desired_avg_lum, result;
	struct sd *sd = (struct sd *) gspca_dev;
	int avg_lum = atomic_read(&sd->avg_lum);

	if ((gspca_dev->ctrl_dis & (1 << AUTOGAIN)) ||
	    avg_lum == -1 || !sd->ctrls[AUTOGAIN].val)
		return;

	if (sd->autogain_ignore_frames > 0) {
		sd->autogain_ignore_frames--;
		return;
	}

	if (sensor_data[sd->sensor].flags & F_SIF) {
		deadzone = 500;
		
		desired_avg_lum = 5000;
	} else {
		deadzone = 1500;
		desired_avg_lum = 13000;
	}

	if (sensor_data[sd->sensor].flags & F_COARSE_EXPO)
		result = coarse_grained_expo_autogain(gspca_dev, avg_lum,
				sd->ctrls[BRIGHTNESS].val
						* desired_avg_lum / 127,
				deadzone);
	else
		result = auto_gain_n_exposure(gspca_dev, avg_lum,
				sd->ctrls[BRIGHTNESS].val
						* desired_avg_lum / 127,
				deadzone, GAIN_KNEE, EXPOSURE_KNEE);

	if (result) {
		PDEBUG(D_FRAM, "autogain: gain changed: gain: %d expo: %d",
			(int) sd->ctrls[GAIN].val,
			(int) sd->ctrls[EXPOSURE].val);
		sd->autogain_ignore_frames = AUTOGAIN_IGNORE_FRAMES;
	}
}

static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;

	reg_r(gspca_dev, 0x00);
	if (gspca_dev->usb_buf[0] != 0x10)
		return -ENODEV;

	
	sd->sensor = id->driver_info >> 8;
	sd->bridge = id->driver_info & 0xff;

	gspca_dev->ctrl_dis = sensor_data[sd->sensor].ctrl_dis;
#if AUTOGAIN_DEF
	if (!(gspca_dev->ctrl_dis & (1 << AUTOGAIN)))
		gspca_dev->ctrl_inac = (1 << GAIN) | (1 << EXPOSURE);
#endif

	cam = &gspca_dev->cam;
	cam->ctrls = sd->ctrls;
	if (!(sensor_data[sd->sensor].flags & F_SIF)) {
		cam->cam_mode = vga_mode;
		cam->nmodes = ARRAY_SIZE(vga_mode);
	} else {
		cam->cam_mode = sif_mode;
		cam->nmodes = ARRAY_SIZE(sif_mode);
	}
	cam->npkt = 36;			

	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	const __u8 stop = 0x09; 

	if (sensor_data[sd->sensor].flags & F_COARSE_EXPO) {
		sd->ctrls[EXPOSURE].min = COARSE_EXPOSURE_MIN;
		sd->ctrls[EXPOSURE].max = COARSE_EXPOSURE_MAX;
		sd->ctrls[EXPOSURE].def = COARSE_EXPOSURE_DEF;
		if (sd->ctrls[EXPOSURE].val > COARSE_EXPOSURE_MAX)
			sd->ctrls[EXPOSURE].val = COARSE_EXPOSURE_DEF;
	}

	reg_w(gspca_dev, 0x01, &stop, 1);

	return 0;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;
	int i, mode;
	__u8 regs[0x31];

	mode = cam->cam_mode[gspca_dev->curr_mode].priv & 0x07;
	
	memcpy(&regs[0x01], sensor_data[sd->sensor].bridge_init, 0x19);
	
	regs[0x18] |= mode << 4;

	
	if (sd->bridge == BRIDGE_103) {
		regs[0x05] = 0x20; 
		regs[0x06] = 0x20; 
		regs[0x07] = 0x20; 
	} else {
		regs[0x10] = 0x00; 
		regs[0x11] = 0x00; 
	}

	
	if (sensor_data[sd->sensor].flags & F_SIF) {
		regs[0x1a] = 0x14; 
		regs[0x1b] = 0x0a; 
		regs[0x1c] = 0x02; 
		regs[0x1d] = 0x02; 
		regs[0x1e] = 0x09; 
		regs[0x1f] = 0x07; 
	} else {
		regs[0x1a] = 0x1d; 
		regs[0x1b] = 0x10; 
		regs[0x1c] = 0x05; 
		regs[0x1d] = 0x03; 
		regs[0x1e] = 0x0f; 
		regs[0x1f] = 0x0c; 
	}

	
	for (i = 0; i < 16; i++)
		regs[0x20 + i] = i * 16;
	regs[0x20 + i] = 255;

	
	switch (sd->sensor) {
	case SENSOR_TAS5130CXX:
		regs[0x19] = mode ? 0x23 : 0x43;
		break;
	case SENSOR_OV7630:
		if (sd->bridge == BRIDGE_103) {
			regs[0x01] = 0x44; 
			regs[0x12] = 0x02; 
		}
	}
	
	if (cam->cam_mode[gspca_dev->curr_mode].priv & MODE_RAW)
		regs[0x18] &= ~0x80;

	
	if (cam->cam_mode[gspca_dev->curr_mode].priv & MODE_REDUCED_SIF) {
		regs[0x12] += 16;	
		regs[0x13] += 24;	
		regs[0x15]  = 320 / 16; 
		regs[0x16]  = 240 / 16; 
	}

	
	reg_w(gspca_dev, 0x01, &regs[0x01], 1);
	
	reg_w(gspca_dev, 0x17, &regs[0x17], 1);
	
	reg_w(gspca_dev, 0x01, &regs[0x01],
	      (sd->bridge == BRIDGE_103) ? 0x30 : 0x1f);

	
	i2c_w_vector(gspca_dev, sensor_data[sd->sensor].sensor_init,
			sensor_data[sd->sensor].sensor_init_size);

	
	switch (sd->sensor) {
	case SENSOR_PAS202: {
		const __u8 i2cpclockdiv[] =
			{0xa0, 0x40, 0x02, 0x03, 0x00, 0x00, 0x00, 0x10};
		
		if (mode)
			i2c_w(gspca_dev, i2cpclockdiv);
		break;
	    }
	case SENSOR_OV7630:
		if (sd->bridge == BRIDGE_103) {
			const __u8 i2c[] = { 0xa0, 0x21, 0x13,
					     0x80, 0x00, 0x00, 0x00, 0x10 };
			i2c_w(gspca_dev, i2c);
		}
		break;
	}
	
	reg_w(gspca_dev, 0x15, &regs[0x15], 2);
	
	reg_w(gspca_dev, 0x18, &regs[0x18], 1);
	
	reg_w(gspca_dev, 0x12, &regs[0x12], 1);
	
	reg_w(gspca_dev, 0x13, &regs[0x13], 1);
	
				
	reg_w(gspca_dev, 0x17, &regs[0x17], 1);
		
	reg_w(gspca_dev, 0x19, &regs[0x19], 1);
	
	reg_w(gspca_dev, 0x1c, &regs[0x1c], 4);
	
	reg_w(gspca_dev, 0x01, &regs[0x01], 1);
	
	reg_w(gspca_dev, 0x18, &regs[0x18], 2);
	msleep(20);

	sd->reg11 = -1;

	setgain(gspca_dev);
	setbrightness(gspca_dev);
	setexposure(gspca_dev);
	setfreq(gspca_dev);

	sd->frames_to_drop = 0;
	sd->autogain_ignore_frames = 0;
	sd->exp_too_high_cnt = 0;
	sd->exp_too_low_cnt = 0;
	atomic_set(&sd->avg_lum, -1);
	return 0;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	sd_init(gspca_dev);
}

static u8* find_sof(struct gspca_dev *gspca_dev, u8 *data, int len)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, header_size = (sd->bridge == BRIDGE_103) ? 18 : 12;

	for (i = 0; i < len; i++) {
		switch (sd->header_read) {
		case 0:
			if (data[i] == 0xff)
				sd->header_read++;
			break;
		case 1:
			if (data[i] == 0xff)
				sd->header_read++;
			else
				sd->header_read = 0;
			break;
		case 2:
			if (data[i] == 0x00)
				sd->header_read++;
			else if (data[i] != 0xff)
				sd->header_read = 0;
			break;
		case 3:
			if (data[i] == 0xc4)
				sd->header_read++;
			else if (data[i] == 0xff)
				sd->header_read = 1;
			else
				sd->header_read = 0;
			break;
		case 4:
			if (data[i] == 0xc4)
				sd->header_read++;
			else if (data[i] == 0xff)
				sd->header_read = 1;
			else
				sd->header_read = 0;
			break;
		case 5:
			if (data[i] == 0x96)
				sd->header_read++;
			else if (data[i] == 0xff)
				sd->header_read = 1;
			else
				sd->header_read = 0;
			break;
		default:
			sd->header[sd->header_read - 6] = data[i];
			sd->header_read++;
			if (sd->header_read == header_size) {
				sd->header_read = 0;
				return data + i + 1;
			}
		}
	}
	return NULL;
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	int fr_h_sz = 0, lum_offset = 0, len_after_sof = 0;
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;
	u8 *sof;

	sof = find_sof(gspca_dev, data, len);
	if (sof) {
		if (sd->bridge == BRIDGE_103) {
			fr_h_sz = 18;
			lum_offset = 3;
		} else {
			fr_h_sz = 12;
			lum_offset = 2;
		}

		len_after_sof = len - (sof - data);
		len = (sof - data) - fr_h_sz;
		if (len < 0)
			len = 0;
	}

	if (cam->cam_mode[gspca_dev->curr_mode].priv & MODE_RAW) {
		int used;
		int size = cam->cam_mode[gspca_dev->curr_mode].sizeimage;

		used = gspca_dev->image_len;
		if (used + len > size)
			len = size - used;
	}

	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);

	if (sof) {
		int  lum = sd->header[lum_offset] +
			  (sd->header[lum_offset + 1] << 8);

		if (lum == 0 && sd->prev_avg_lum != 0) {
			lum = -1;
			sd->frames_to_drop = 2;
			sd->prev_avg_lum = 0;
		} else
			sd->prev_avg_lum = lum;
		atomic_set(&sd->avg_lum, lum);

		if (sd->frames_to_drop)
			sd->frames_to_drop--;
		else
			gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);

		gspca_frame_add(gspca_dev, FIRST_PACKET, sof, len_after_sof);
	}
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->ctrls[AUTOGAIN].val = val;
	sd->exp_too_high_cnt = 0;
	sd->exp_too_low_cnt = 0;

	if (sd->ctrls[AUTOGAIN].val
	 && !(sensor_data[sd->sensor].flags & F_COARSE_EXPO)) {
		sd->ctrls[EXPOSURE].val = sd->ctrls[EXPOSURE].def;
		sd->ctrls[GAIN].val = sd->ctrls[GAIN].def;
		if (gspca_dev->streaming) {
			sd->autogain_ignore_frames = AUTOGAIN_IGNORE_FRAMES;
			setexposure(gspca_dev);
			setgain(gspca_dev);
		}
	}

	if (sd->ctrls[AUTOGAIN].val)
		gspca_dev->ctrl_inac = (1 << GAIN) | (1 << EXPOSURE);
	else
		gspca_dev->ctrl_inac = 0;

	return 0;
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
			struct v4l2_querymenu *menu)
{
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
		}
		break;
	}
	return -EINVAL;
}

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
static int sd_int_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,		
			int len)		
{
	int ret = -EINVAL;

	if (len == 1 && data[0] == 1) {
		input_report_key(gspca_dev->input_dev, KEY_CAMERA, 1);
		input_sync(gspca_dev->input_dev);
		input_report_key(gspca_dev->input_dev, KEY_CAMERA, 0);
		input_sync(gspca_dev->input_dev);
		ret = 0;
	}

	return ret;
}
#endif

static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.pkt_scan = sd_pkt_scan,
	.querymenu = sd_querymenu,
	.dq_callback = do_autogain,
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.int_pkt_scan = sd_int_pkt_scan,
#endif
};

#define SB(sensor, bridge) \
	.driver_info = (SENSOR_ ## sensor << 8) | BRIDGE_ ## bridge


static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x0c45, 0x6001), SB(TAS5110C, 102)}, 
	{USB_DEVICE(0x0c45, 0x6005), SB(TAS5110C, 101)}, 
	{USB_DEVICE(0x0c45, 0x6007), SB(TAS5110D, 101)}, 
	{USB_DEVICE(0x0c45, 0x6009), SB(PAS106, 101)},
	{USB_DEVICE(0x0c45, 0x600d), SB(PAS106, 101)},
	{USB_DEVICE(0x0c45, 0x6011), SB(OV6650, 101)},
	{USB_DEVICE(0x0c45, 0x6019), SB(OV7630, 101)},
#if !defined CONFIG_USB_SN9C102 && !defined CONFIG_USB_SN9C102_MODULE
	{USB_DEVICE(0x0c45, 0x6024), SB(TAS5130CXX, 102)},
	{USB_DEVICE(0x0c45, 0x6025), SB(TAS5130CXX, 102)},
#endif
	{USB_DEVICE(0x0c45, 0x6028), SB(PAS202, 102)},
	{USB_DEVICE(0x0c45, 0x6029), SB(PAS106, 102)},
	{USB_DEVICE(0x0c45, 0x602a), SB(HV7131D, 102)},
	
	{USB_DEVICE(0x0c45, 0x602c), SB(OV7630, 102)},
	{USB_DEVICE(0x0c45, 0x602d), SB(HV7131R, 102)},
	{USB_DEVICE(0x0c45, 0x602e), SB(OV7630, 102)},
	 
	 
	{USB_DEVICE(0x0c45, 0x6083), SB(HV7131D, 103)},
	{USB_DEVICE(0x0c45, 0x608c), SB(HV7131R, 103)},
	
	{USB_DEVICE(0x0c45, 0x608f), SB(OV7630, 103)},
	{USB_DEVICE(0x0c45, 0x60a8), SB(PAS106, 103)},
	{USB_DEVICE(0x0c45, 0x60aa), SB(TAS5130CXX, 103)},
	{USB_DEVICE(0x0c45, 0x60af), SB(PAS202, 103)},
	{USB_DEVICE(0x0c45, 0x60b0), SB(OV7630, 103)},
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
