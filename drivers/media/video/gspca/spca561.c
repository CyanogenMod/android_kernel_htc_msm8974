/*
 * Sunplus spca561 subdriver
 *
 * Copyright (C) 2004 Michel Xhaard mxhaard@magic.fr
 *
 * V4L2 by Jean-Francois Moine <http://moinejf.free.fr>
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_NAME "spca561"

#include <linux/input.h>
#include "gspca.h"

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>");
MODULE_DESCRIPTION("GSPCA/SPCA561 USB Camera Driver");
MODULE_LICENSE("GPL");

struct sd {
	struct gspca_dev gspca_dev;	

	__u16 exposure;			
#define EXPOSURE_MIN 1
#define EXPOSURE_DEF 700		
#define EXPOSURE_MAX (2047 + 325)	

	__u8 contrast;			
#define CONTRAST_MIN 0x00
#define CONTRAST_DEF 0x20
#define CONTRAST_MAX 0x3f

	__u8 brightness;		
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_DEF 0x20
#define BRIGHTNESS_MAX 0x3f

	__u8 white;
#define HUE_MIN 1
#define HUE_DEF 0x40
#define HUE_MAX 0x7f

	__u8 autogain;
#define AUTOGAIN_MIN 0
#define AUTOGAIN_DEF 1
#define AUTOGAIN_MAX 1

	__u8 gain;			
#define GAIN_MIN 0
#define GAIN_DEF 63
#define GAIN_MAX 255

#define EXPO12A_DEF 3
	__u8 expo12a;		

	__u8 chip_revision;
#define Rev012A 0
#define Rev072A 1

	signed char ag_cnt;
#define AG_CNT_START 13
};

static const struct v4l2_pix_format sif_012a_mode[] = {
	{160, 120, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{320, 240, V4L2_PIX_FMT_SPCA561, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 4 / 8,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_SPCA561, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 4 / 8,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

static const struct v4l2_pix_format sif_072a_mode[] = {
	{160, 120, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 3},
	{176, 144, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{320, 240, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_SGBRG8, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

#define SPCA561_OFFSET_SNAP 1
#define SPCA561_OFFSET_TYPE 2
#define SPCA561_OFFSET_COMPRESS 3
#define SPCA561_OFFSET_FRAMSEQ   4
#define SPCA561_OFFSET_GPIO 5
#define SPCA561_OFFSET_USBBUFF 6
#define SPCA561_OFFSET_WIN2GRAVE 7
#define SPCA561_OFFSET_WIN2RAVE 8
#define SPCA561_OFFSET_WIN2BAVE 9
#define SPCA561_OFFSET_WIN2GBAVE 10
#define SPCA561_OFFSET_WIN1GRAVE 11
#define SPCA561_OFFSET_WIN1RAVE 12
#define SPCA561_OFFSET_WIN1BAVE 13
#define SPCA561_OFFSET_WIN1GBAVE 14
#define SPCA561_OFFSET_FREQ 15
#define SPCA561_OFFSET_VSYNC 16
#define SPCA561_INDEX_I2C_BASE 0x8800
#define SPCA561_SNAPBIT 0x20
#define SPCA561_SNAPCTRL 0x40

static const u16 rev72a_reset[][2] = {
	{0x0000, 0x8114},	
	{0x0001, 0x8114},	
	{0x0000, 0x8112},	
	{}
};
static const __u16 rev72a_init_data1[][2] = {
	{0x0003, 0x8701},	
	{0x0001, 0x8703},	
	{0x0011, 0x8118},	
	{0x0001, 0x8118},	
	{0x0092, 0x8804},	
	{0x0010, 0x8802},	
	{}
};
static const u16 rev72a_init_sensor1[][2] = {
	{0x0001, 0x000d},
	{0x0002, 0x0018},
	{0x0004, 0x0165},
	{0x0005, 0x0021},
	{0x0007, 0x00aa},
	{0x0020, 0x1504},
	{0x0039, 0x0002},
	{0x0035, 0x0010},
	{0x0009, 0x1049},
	{0x0028, 0x000b},
	{0x003b, 0x000f},
	{0x003c, 0x0000},
	{}
};
static const __u16 rev72a_init_data2[][2] = {
	{0x0018, 0x8601},	
	{0x0000, 0x8602},	
	{0x0060, 0x8604},	
	{0x0002, 0x8605},	
	{0x0000, 0x8603},	
	{0x0002, 0x865b},	
	{0x0000, 0x865f},	
	{0x00b0, 0x865d},	
	{0x0090, 0x865e},	
	{0x00e0, 0x8406},	
	{0x0000, 0x8660},	
	{0x0002, 0x8201},	
	{0x0008, 0x8200},	
	{0x0001, 0x8200},	
	{0x0000, 0x8611},	
	{0x00fd, 0x8612},	
	{0x0003, 0x8613},	
	{0x0000, 0x8614},	
	{0x0035, 0x8651},	
	{0x0040, 0x8652},	
	{0x005f, 0x8653},	
	{0x0040, 0x8654},	
	{0x0002, 0x8502},	
	{0x0011, 0x8802},

	{0x0087, 0x8700},	
	{0x0081, 0x8702},	

	{0x0000, 0x8500},	
	

	{0x0002, 0x865b},	
	{0x0003, 0x865c},	
	{}
};
static const u16 rev72a_init_sensor2[][2] = {
	{0x0003, 0x0121},
	{0x0004, 0x0165},
	{0x0005, 0x002f},	
	{0x0006, 0x0000},	
	{0x000a, 0x0002},
	{0x0009, 0x1061},	
	{0x0035, 0x0014},
	{}
};

static const __u16 Pb100_1map8300[][2] = {
	
	{0x8320, 0x3304},

	{0x8303, 0x0125},	
	{0x8304, 0x0169},
	{0x8328, 0x000b},
	{0x833c, 0x0001},		

	{0x832f, 0x1904},		
	{0x8307, 0x00aa},
	{0x8301, 0x0003},
	{0x8302, 0x000e},
	{}
};
static const __u16 Pb100_2map8300[][2] = {
	
	{0x8339, 0x0000},
	{0x8307, 0x00aa},
	{}
};

static const __u16 spca561_161rev12A_data1[][2] = {
	{0x29, 0x8118},		
	{0x08, 0x8114},		
	{0x0e, 0x8112},		
	{0x00, 0x8102},		
	{0x92, 0x8804},
	{0x04, 0x8802},		
	{}
};
static const __u16 spca561_161rev12A_data2[][2] = {
	{0x21, 0x8118},
	{0x10, 0x8500},
	{0x07, 0x8601},
	{0x07, 0x8602},
	{0x04, 0x8501},

	{0x07, 0x8201},		
	{0x08, 0x8200},
	{0x01, 0x8200},

	{0x90, 0x8604},
	{0x00, 0x8605},
	{0xb0, 0x8603},

	
	{0x07, 0x8601},		
	{0x07, 0x8602},		
	{0x00, 0x8610},		
	{0x00, 0x8611},		
	{0x00, 0x8612},		
	{0x00, 0x8613},		
	{0x43, 0x8614},		
	{0x40, 0x8615},		
	{0x71, 0x8616},		
	{0x40, 0x8617},		

	{0x0c, 0x8620},		
	{0xc8, 0x8631},		
	{0xc8, 0x8634},		
	{0x23, 0x8635},		
	{0x1f, 0x8636},		
	{0xdd, 0x8637},		
	{0xe1, 0x8638},		
	{0x1d, 0x8639},		
	{0x21, 0x863a},		
	{0xe3, 0x863b},		
	{0xdf, 0x863c},		
	{0xf0, 0x8505},
	{0x32, 0x850a},
	{0x29, 0x8118},
	{}
};

static void reg_w_val(struct usb_device *dev, __u16 index, __u8 value)
{
	int ret;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			      0,		
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      value, index, NULL, 0, 500);
	PDEBUG(D_USBO, "reg write: 0x%02x:0x%02x", index, value);
	if (ret < 0)
		pr_err("reg write: error %d\n", ret);
}

static void write_vector(struct gspca_dev *gspca_dev,
			const __u16 data[][2])
{
	struct usb_device *dev = gspca_dev->dev;
	int i;

	i = 0;
	while (data[i][1] != 0) {
		reg_w_val(dev, data[i][1], data[i][0]);
		i++;
	}
}

static void reg_r(struct gspca_dev *gspca_dev,
		  __u16 index, __u16 length)
{
	usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			0,			
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,			
			index, gspca_dev->usb_buf, length, 500);
}

static void reg_w_buf(struct gspca_dev *gspca_dev,
		      __u16 index, __u16 len)
{
	usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0,			
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,			
			index, gspca_dev->usb_buf, len, 500);
}

static void i2c_write(struct gspca_dev *gspca_dev, __u16 value, __u16 reg)
{
	int retry = 60;

	reg_w_val(gspca_dev->dev, 0x8801, reg);
	reg_w_val(gspca_dev->dev, 0x8805, value);
	reg_w_val(gspca_dev->dev, 0x8800, value >> 8);
	do {
		reg_r(gspca_dev, 0x8803, 1);
		if (!gspca_dev->usb_buf[0])
			return;
		msleep(10);
	} while (--retry);
}

static int i2c_read(struct gspca_dev *gspca_dev, __u16 reg, __u8 mode)
{
	int retry = 60;
	__u8 value;

	reg_w_val(gspca_dev->dev, 0x8804, 0x92);
	reg_w_val(gspca_dev->dev, 0x8801, reg);
	reg_w_val(gspca_dev->dev, 0x8802, mode | 0x01);
	do {
		reg_r(gspca_dev, 0x8803, 1);
		if (!gspca_dev->usb_buf[0]) {
			reg_r(gspca_dev, 0x8800, 1);
			value = gspca_dev->usb_buf[0];
			reg_r(gspca_dev, 0x8805, 1);
			return ((int) value << 8) | gspca_dev->usb_buf[0];
		}
		msleep(10);
	} while (--retry);
	return -1;
}

static void sensor_mapwrite(struct gspca_dev *gspca_dev,
			    const __u16 (*sensormap)[2])
{
	while ((*sensormap)[0]) {
		gspca_dev->usb_buf[0] = (*sensormap)[1];
		gspca_dev->usb_buf[1] = (*sensormap)[1] >> 8;
		reg_w_buf(gspca_dev, (*sensormap)[0], 2);
		sensormap++;
	}
}

static void write_sensor_72a(struct gspca_dev *gspca_dev,
			    const __u16 (*sensor)[2])
{
	while ((*sensor)[0]) {
		i2c_write(gspca_dev, (*sensor)[1], (*sensor)[0]);
		sensor++;
	}
}

static void init_161rev12A(struct gspca_dev *gspca_dev)
{
	write_vector(gspca_dev, spca561_161rev12A_data1);
	sensor_mapwrite(gspca_dev, Pb100_1map8300);
	write_vector(gspca_dev, spca561_161rev12A_data2);
	sensor_mapwrite(gspca_dev, Pb100_2map8300);
}

static int sd_config(struct gspca_dev *gspca_dev,
		     const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;
	__u16 vendor, product;
	__u8 data1, data2;

	reg_r(gspca_dev, 0x8104, 1);
	data1 = gspca_dev->usb_buf[0];
	reg_r(gspca_dev, 0x8105, 1);
	data2 = gspca_dev->usb_buf[0];
	vendor = (data2 << 8) | data1;
	reg_r(gspca_dev, 0x8106, 1);
	data1 = gspca_dev->usb_buf[0];
	reg_r(gspca_dev, 0x8107, 1);
	data2 = gspca_dev->usb_buf[0];
	product = (data2 << 8) | data1;
	if (vendor != id->idVendor || product != id->idProduct) {
		PDEBUG(D_PROBE, "Bad vendor / product from device");
		return -EINVAL;
	}

	cam = &gspca_dev->cam;
	cam->needs_full_bandwidth = 1;

	sd->chip_revision = id->driver_info;
	if (sd->chip_revision == Rev012A) {
		cam->cam_mode = sif_012a_mode;
		cam->nmodes = ARRAY_SIZE(sif_012a_mode);
	} else {
		cam->cam_mode = sif_072a_mode;
		cam->nmodes = ARRAY_SIZE(sif_072a_mode);
	}
	sd->brightness = BRIGHTNESS_DEF;
	sd->contrast = CONTRAST_DEF;
	sd->white = HUE_DEF;
	sd->exposure = EXPOSURE_DEF;
	sd->autogain = AUTOGAIN_DEF;
	sd->gain = GAIN_DEF;
	sd->expo12a = EXPO12A_DEF;
	return 0;
}

static int sd_init_12a(struct gspca_dev *gspca_dev)
{
	PDEBUG(D_STREAM, "Chip revision: 012a");
	init_161rev12A(gspca_dev);
	return 0;
}
static int sd_init_72a(struct gspca_dev *gspca_dev)
{
	PDEBUG(D_STREAM, "Chip revision: 072a");
	write_vector(gspca_dev, rev72a_reset);
	msleep(200);
	write_vector(gspca_dev, rev72a_init_data1);
	write_sensor_72a(gspca_dev, rev72a_init_sensor1);
	write_vector(gspca_dev, rev72a_init_data2);
	write_sensor_72a(gspca_dev, rev72a_init_sensor2);
	reg_w_val(gspca_dev->dev, 0x8112, 0x30);
	return 0;
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	__u8 value;

	value = sd->brightness;

	
	reg_w_val(dev, 0x8611, value);		
	reg_w_val(dev, 0x8612, value);		
	reg_w_val(dev, 0x8613, value);		
	reg_w_val(dev, 0x8614, value);		
}

static void setwhite(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u16 white;
	__u8 blue, red;
	__u16 reg;

	
	white = sd->white;
	red = 0x20 + white * 3 / 8;
	blue = 0x90 - white * 5 / 8;
	if (sd->chip_revision == Rev012A) {
		reg = 0x8614;
	} else {
		reg = 0x8651;
		red += sd->contrast - 0x20;
		blue += sd->contrast - 0x20;
	}
	reg_w_val(gspca_dev->dev, reg, red);
	reg_w_val(gspca_dev->dev, reg + 2, blue);
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	__u8 value;

	if (sd->chip_revision != Rev072A)
		return;
	value = sd->contrast + 0x20;

	
	setwhite(gspca_dev);
	reg_w_val(dev, 0x8652, value);		
	reg_w_val(dev, 0x8654, value);		
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, expo = 0;


	int table[] =  { 0, 450, 550, 625, EXPOSURE_MAX };

	for (i = 0; i < ARRAY_SIZE(table) - 1; i++) {
		if (sd->exposure <= table[i + 1]) {
			expo  = sd->exposure - table[i];
			if (i)
				expo += 300;
			expo |= i << 11;
			break;
		}
	}

	gspca_dev->usb_buf[0] = expo;
	gspca_dev->usb_buf[1] = expo >> 8;
	reg_w_buf(gspca_dev, 0x8309, 2);
}

static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->gain < 64)
		gspca_dev->usb_buf[0] = sd->gain;
	else if (sd->gain < 128)
		gspca_dev->usb_buf[0] = (sd->gain / 2) | 0x40;
	else
		gspca_dev->usb_buf[0] = (sd->gain / 4) | 0xc0;

	gspca_dev->usb_buf[1] = 0;
	reg_w_buf(gspca_dev, 0x8335, 2);
}

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->autogain)
		sd->ag_cnt = AG_CNT_START;
	else
		sd->ag_cnt = -1;
}

static int sd_start_12a(struct gspca_dev *gspca_dev)
{
	struct usb_device *dev = gspca_dev->dev;
	int mode;
	static const __u8 Reg8391[8] =
		{0x92, 0x30, 0x20, 0x00, 0x0c, 0x00, 0x00, 0x00};

	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv;
	if (mode <= 1) {
		
		reg_w_val(dev, 0x8500, 0x10 | mode);
	} else {
		reg_w_val(dev, 0x8500, mode);
	}		

	gspca_dev->usb_buf[0] = 0xaa;
	gspca_dev->usb_buf[1] = 0x00;
	reg_w_buf(gspca_dev, 0x8307, 2);
	
	reg_w_val(gspca_dev->dev, 0x8700, 0x8a);
					
	reg_w_val(gspca_dev->dev, 0x8112, 0x1e | 0x20);
	reg_w_val(gspca_dev->dev, 0x850b, 0x03);
	memcpy(gspca_dev->usb_buf, Reg8391, 8);
	reg_w_buf(gspca_dev, 0x8391, 8);
	reg_w_buf(gspca_dev, 0x8390, 8);
	setwhite(gspca_dev);
	setgain(gspca_dev);
	setexposure(gspca_dev);

	
	reg_w_val(gspca_dev->dev, 0x8114, 0x00);
	return 0;
}
static int sd_start_72a(struct gspca_dev *gspca_dev)
{
	struct usb_device *dev = gspca_dev->dev;
	int Clck;
	int mode;

	write_vector(gspca_dev, rev72a_reset);
	msleep(200);
	write_vector(gspca_dev, rev72a_init_data1);
	write_sensor_72a(gspca_dev, rev72a_init_sensor1);

	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv;
	switch (mode) {
	default:
	case 0:
		Clck = 0x27;		
		break;
	case 1:
		Clck = 0x25;
		break;
	case 2:
		Clck = 0x22;
		break;
	case 3:
		Clck = 0x21;
		break;
	}
	reg_w_val(dev, 0x8700, Clck);	
	reg_w_val(dev, 0x8702, 0x81);
	reg_w_val(dev, 0x8500, mode);	
	write_sensor_72a(gspca_dev, rev72a_init_sensor2);
	setcontrast(gspca_dev);
	setautogain(gspca_dev);
	reg_w_val(dev, 0x8112, 0x10 | 0x20);
	return 0;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->chip_revision == Rev012A) {
		reg_w_val(gspca_dev->dev, 0x8112, 0x0e);
		
		reg_w_val(gspca_dev->dev, 0x8114, 0x08);
	} else {
		reg_w_val(gspca_dev->dev, 0x8112, 0x20);
	}
}

static void do_autogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int expotimes;
	int pixelclk;
	int gainG;
	__u8 R, Gr, Gb, B;
	int y;
	__u8 luma_mean = 110;
	__u8 luma_delta = 20;
	__u8 spring = 4;

	if (sd->ag_cnt < 0)
		return;
	if (--sd->ag_cnt >= 0)
		return;
	sd->ag_cnt = AG_CNT_START;

	switch (sd->chip_revision) {
	case Rev072A:
		reg_r(gspca_dev, 0x8621, 1);
		Gr = gspca_dev->usb_buf[0];
		reg_r(gspca_dev, 0x8622, 1);
		R = gspca_dev->usb_buf[0];
		reg_r(gspca_dev, 0x8623, 1);
		B = gspca_dev->usb_buf[0];
		reg_r(gspca_dev, 0x8624, 1);
		Gb = gspca_dev->usb_buf[0];
		y = (77 * R + 75 * (Gr + Gb) + 29 * B) >> 8;
		
		
		

		if (y < luma_mean - luma_delta ||
		    y > luma_mean + luma_delta) {
			expotimes = i2c_read(gspca_dev, 0x09, 0x10);
			pixelclk = 0x0800;
			expotimes = expotimes & 0x07ff;
			gainG = i2c_read(gspca_dev, 0x35, 0x10);

			expotimes += (luma_mean - y) >> spring;
			gainG += (luma_mean - y) / 50;

			if (gainG > 0x3f)
				gainG = 0x3f;
			else if (gainG < 3)
				gainG = 3;
			i2c_write(gspca_dev, gainG, 0x35);

			if (expotimes > 0x0256)
				expotimes = 0x0256;
			else if (expotimes < 3)
				expotimes = 3;
			i2c_write(gspca_dev, expotimes | pixelclk, 0x09);
		}
		break;
	}
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,		
			int len)		
{
	struct sd *sd = (struct sd *) gspca_dev;

	len--;
	switch (*data++) {			
	case 0:					
		gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);

		
		if (len < 2) {
			PDEBUG(D_ERR, "Short SOF packet, ignoring");
			gspca_dev->last_packet_type = DISCARD_PACKET;
			return;
		}

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
		if (data[0] & 0x20) {
			input_report_key(gspca_dev->input_dev, KEY_CAMERA, 1);
			input_sync(gspca_dev->input_dev);
			input_report_key(gspca_dev->input_dev, KEY_CAMERA, 0);
			input_sync(gspca_dev->input_dev);
		}
#endif

		if (data[1] & 0x10) {
			
			gspca_frame_add(gspca_dev, FIRST_PACKET, data, len);
		} else {
			
			if (sd->chip_revision == Rev012A) {
				data += 20;
				len -= 20;
			} else {
				data += 16;
				len -= 16;
			}
			gspca_frame_add(gspca_dev, FIRST_PACKET, data, len);
		}
		return;
	case 0xff:			
		return;
	}
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->brightness = val;
	if (gspca_dev->streaming)
		setbrightness(gspca_dev);
	return 0;
}

static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->brightness;
	return 0;
}

static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->contrast = val;
	if (gspca_dev->streaming)
		setcontrast(gspca_dev);
	return 0;
}

static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->contrast;
	return 0;
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->autogain = val;
	if (gspca_dev->streaming)
		setautogain(gspca_dev);
	return 0;
}

static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->autogain;
	return 0;
}

static int sd_setwhite(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->white = val;
	if (gspca_dev->streaming)
		setwhite(gspca_dev);
	return 0;
}

static int sd_getwhite(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->white;
	return 0;
}

static int sd_setexposure(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->exposure = val;
	if (gspca_dev->streaming)
		setexposure(gspca_dev);
	return 0;
}

static int sd_getexposure(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->exposure;
	return 0;
}

static int sd_setgain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->gain = val;
	if (gspca_dev->streaming)
		setgain(gspca_dev);
	return 0;
}

static int sd_getgain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->gain;
	return 0;
}

static const struct ctrl sd_ctrls_12a[] = {
	{
	    {
		.id = V4L2_CID_HUE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hue",
		.minimum = HUE_MIN,
		.maximum = HUE_MAX,
		.step = 1,
		.default_value = HUE_DEF,
	    },
	    .set = sd_setwhite,
	    .get = sd_getwhite,
	},
	{
	    {
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Exposure",
		.minimum = EXPOSURE_MIN,
		.maximum = EXPOSURE_MAX,
		.step = 1,
		.default_value = EXPOSURE_DEF,
	    },
	    .set = sd_setexposure,
	    .get = sd_getexposure,
	},
	{
	    {
		.id = V4L2_CID_GAIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Gain",
		.minimum = GAIN_MIN,
		.maximum = GAIN_MAX,
		.step = 1,
		.default_value = GAIN_DEF,
	    },
	    .set = sd_setgain,
	    .get = sd_getgain,
	},
};

static const struct ctrl sd_ctrls_72a[] = {
	{
	    {
		.id = V4L2_CID_HUE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Hue",
		.minimum = HUE_MIN,
		.maximum = HUE_MAX,
		.step = 1,
		.default_value = HUE_DEF,
	    },
	    .set = sd_setwhite,
	    .get = sd_getwhite,
	},
	{
	   {
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Brightness",
		.minimum = BRIGHTNESS_MIN,
		.maximum = BRIGHTNESS_MAX,
		.step = 1,
		.default_value = BRIGHTNESS_DEF,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
	{
	    {
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = CONTRAST_MIN,
		.maximum = CONTRAST_MAX,
		.step = 1,
		.default_value = CONTRAST_DEF,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getcontrast,
	},
	{
	    {
		.id = V4L2_CID_AUTOGAIN,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto Gain",
		.minimum = AUTOGAIN_MIN,
		.maximum = AUTOGAIN_MAX,
		.step = 1,
		.default_value = AUTOGAIN_DEF,
	    },
	    .set = sd_setautogain,
	    .get = sd_getautogain,
	},
};

static const struct sd_desc sd_desc_12a = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls_12a,
	.nctrls = ARRAY_SIZE(sd_ctrls_12a),
	.config = sd_config,
	.init = sd_init_12a,
	.start = sd_start_12a,
	.stopN = sd_stopN,
	.pkt_scan = sd_pkt_scan,
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.other_input = 1,
#endif
};
static const struct sd_desc sd_desc_72a = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls_72a,
	.nctrls = ARRAY_SIZE(sd_ctrls_72a),
	.config = sd_config,
	.init = sd_init_72a,
	.start = sd_start_72a,
	.stopN = sd_stopN,
	.pkt_scan = sd_pkt_scan,
	.dq_callback = do_autogain,
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	.other_input = 1,
#endif
};
static const struct sd_desc *sd_desc[2] = {
	&sd_desc_12a,
	&sd_desc_72a
};

static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x041e, 0x401a), .driver_info = Rev072A},
	{USB_DEVICE(0x041e, 0x403b), .driver_info = Rev012A},
	{USB_DEVICE(0x0458, 0x7004), .driver_info = Rev072A},
	{USB_DEVICE(0x0461, 0x0815), .driver_info = Rev072A},
	{USB_DEVICE(0x046d, 0x0928), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x0929), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092a), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092b), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092c), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092d), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092e), .driver_info = Rev012A},
	{USB_DEVICE(0x046d, 0x092f), .driver_info = Rev012A},
	{USB_DEVICE(0x04fc, 0x0561), .driver_info = Rev072A},
	{USB_DEVICE(0x060b, 0xa001), .driver_info = Rev072A},
	{USB_DEVICE(0x10fd, 0x7e50), .driver_info = Rev072A},
	{USB_DEVICE(0xabcd, 0xcdee), .driver_info = Rev072A},
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

static int sd_probe(struct usb_interface *intf,
		    const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id,
				sd_desc[id->driver_info],
				sizeof(struct sd),
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
