/*
 * Etoms Et61x151 GPL Linux driver by Michel Xhaard (09/09/2004)
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

#define MODULE_NAME "etoms"

#include "gspca.h"

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>");
MODULE_DESCRIPTION("Etoms USB Camera Driver");
MODULE_LICENSE("GPL");

struct sd {
	struct gspca_dev gspca_dev;	

	unsigned char brightness;
	unsigned char contrast;
	unsigned char colors;
	unsigned char autogain;

	char sensor;
#define SENSOR_PAS106 0
#define SENSOR_TAS5130CXX 1
	signed char ag_cnt;
#define AG_CNT_START 13
};

static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcolors(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcolors(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);

static const struct ctrl sd_ctrls[] = {
	{
	 {
	  .id = V4L2_CID_BRIGHTNESS,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .name = "Brightness",
	  .minimum = 1,
	  .maximum = 127,
	  .step = 1,
#define BRIGHTNESS_DEF 63
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
	  .minimum = 0,
	  .maximum = 255,
	  .step = 1,
#define CONTRAST_DEF 127
	  .default_value = CONTRAST_DEF,
	  },
	 .set = sd_setcontrast,
	 .get = sd_getcontrast,
	 },
#define COLOR_IDX 2
	{
	 {
	  .id = V4L2_CID_SATURATION,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .name = "Color",
	  .minimum = 0,
	  .maximum = 15,
	  .step = 1,
#define COLOR_DEF 7
	  .default_value = COLOR_DEF,
	  },
	 .set = sd_setcolors,
	 .get = sd_getcolors,
	 },
	{
	 {
	  .id = V4L2_CID_AUTOGAIN,
	  .type = V4L2_CTRL_TYPE_BOOLEAN,
	  .name = "Auto Gain",
	  .minimum = 0,
	  .maximum = 1,
	  .step = 1,
#define AUTOGAIN_DEF 1
	  .default_value = AUTOGAIN_DEF,
	  },
	 .set = sd_setautogain,
	 .get = sd_getautogain,
	 },
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
};

static const struct v4l2_pix_format sif_mode[] = {
	{176, 144, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_SBGGR8, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

#define ETOMS_ALT_SIZE_1000   12

#define ET_GPIO_DIR_CTRL 0x04	
#define ET_GPIO_OUT 0x05	
#define ET_GPIO_IN 0x06		
#define ET_RESET_ALL 0x03
#define ET_ClCK 0x01
#define ET_CTRL 0x02		

#define ET_COMP 0x12		
#define ET_MAXQt 0x13
#define ET_MINQt 0x14
#define ET_COMP_VAL0 0x02
#define ET_COMP_VAL1 0x03

#define ET_REG1d 0x1d
#define ET_REG1e 0x1e
#define ET_REG1f 0x1f
#define ET_REG20 0x20
#define ET_REG21 0x21
#define ET_REG22 0x22
#define ET_REG23 0x23
#define ET_REG24 0x24
#define ET_REG25 0x25
#define ET_LUMA_CENTER 0x39

#define ET_G_RED 0x4d
#define ET_G_GREEN1 0x4e
#define ET_G_BLUE 0x4f
#define ET_G_GREEN2 0x50
#define ET_G_GR_H 0x51
#define ET_G_GB_H 0x52

#define ET_O_RED 0x34
#define ET_O_GREEN1 0x35
#define ET_O_BLUE 0x36
#define ET_O_GREEN2 0x37

#define ET_SYNCHRO 0x68
#define ET_STARTX 0x69
#define ET_STARTY 0x6a
#define ET_WIDTH_LOW 0x6b
#define ET_HEIGTH_LOW 0x6c
#define ET_W_H_HEIGTH 0x6d

#define ET_REG6e 0x6e		
#define ET_REG6f 0x6f		
#define ET_REG70 0x70		
#define ET_REG71 0x71		
#define ET_REG72 0x72		
#define ET_REG73 0x73		
#define ET_REG74 0x74		
#define ET_REG75 0x75		

#define ET_I2C_CLK 0x8c
#define ET_PXL_CLK 0x60

#define ET_I2C_BASE 0x89
#define ET_I2C_COUNT 0x8a
#define ET_I2C_PREFETCH 0x8b
#define ET_I2C_REG 0x88
#define ET_I2C_DATA7 0x87
#define ET_I2C_DATA6 0x86
#define ET_I2C_DATA5 0x85
#define ET_I2C_DATA4 0x84
#define ET_I2C_DATA3 0x83
#define ET_I2C_DATA2 0x82
#define ET_I2C_DATA1 0x81
#define ET_I2C_DATA0 0x80

#define PAS106_REG2 0x02	
#define PAS106_REG3 0x03	
#define PAS106_REG4 0x04	
#define PAS106_REG5 0x05	
#define PAS106_REG6 0x06	
#define PAS106_REG7 0x07	
#define PAS106_REG9 0x09
#define PAS106_REG0e 0x0e	
#define PAS106_REG13 0x13	

static const __u8 GainRGBG[] = { 0x80, 0x80, 0x80, 0x80, 0x00, 0x00 };

static const __u8 I2c2[] = { 0x08, 0x08, 0x08, 0x08, 0x0d };

static const __u8 I2c3[] = { 0x12, 0x05 };

static const __u8 I2c4[] = { 0x41, 0x08 };

static void reg_r(struct gspca_dev *gspca_dev,
		  __u16 index,
		  __u16 len)
{
	struct usb_device *dev = gspca_dev->dev;

#ifdef GSPCA_DEBUG
	if (len > USB_BUF_SZ) {
		pr_err("reg_r: buffer overflow\n");
		return;
	}
#endif
	usb_control_msg(dev,
			usb_rcvctrlpipe(dev, 0),
			0,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			0,
			index, gspca_dev->usb_buf, len, 500);
	PDEBUG(D_USBI, "reg read [%02x] -> %02x ..",
			index, gspca_dev->usb_buf[0]);
}

static void reg_w_val(struct gspca_dev *gspca_dev,
			__u16 index,
			__u8 val)
{
	struct usb_device *dev = gspca_dev->dev;

	gspca_dev->usb_buf[0] = val;
	usb_control_msg(dev,
			usb_sndctrlpipe(dev, 0),
			0,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			0,
			index, gspca_dev->usb_buf, 1, 500);
}

static void reg_w(struct gspca_dev *gspca_dev,
		  __u16 index,
		  const __u8 *buffer,
		  __u16 len)
{
	struct usb_device *dev = gspca_dev->dev;

#ifdef GSPCA_DEBUG
	if (len > USB_BUF_SZ) {
		pr_err("reg_w: buffer overflow\n");
		return;
	}
	PDEBUG(D_USBO, "reg write [%02x] = %02x..", index, *buffer);
#endif
	memcpy(gspca_dev->usb_buf, buffer, len);
	usb_control_msg(dev,
			usb_sndctrlpipe(dev, 0),
			0,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			0, index, gspca_dev->usb_buf, len, 500);
}

static int i2c_w(struct gspca_dev *gspca_dev,
		 __u8 reg,
		 const __u8 *buffer,
		 int len, __u8 mode)
{
	
	__u8 ptchcount;

	
	reg_w_val(gspca_dev, ET_I2C_BASE, 0x40);
					 
	
	ptchcount = ((len & 0x07) << 4) | (mode & 0x03);
	reg_w_val(gspca_dev, ET_I2C_COUNT, ptchcount);
	
	reg_w_val(gspca_dev, ET_I2C_REG, reg);
	while (--len >= 0)
		reg_w_val(gspca_dev, ET_I2C_DATA0 + len, buffer[len]);
	return 0;
}

static int i2c_r(struct gspca_dev *gspca_dev,
			__u8 reg)
{
	
	reg_w_val(gspca_dev, ET_I2C_BASE, 0x40);
					
	
	reg_w_val(gspca_dev, ET_I2C_COUNT, 0x11);
	reg_w_val(gspca_dev, ET_I2C_REG, reg);	
	reg_w_val(gspca_dev, ET_I2C_PREFETCH, 0x02);	
	reg_w_val(gspca_dev, ET_I2C_PREFETCH, 0x00);
	reg_r(gspca_dev, ET_I2C_DATA0, 1);	
	return 0;
}

static int Et_WaitStatus(struct gspca_dev *gspca_dev)
{
	int retry = 10;

	while (retry--) {
		reg_r(gspca_dev, ET_ClCK, 1);
		if (gspca_dev->usb_buf[0] != 0)
			return 1;
	}
	return 0;
}

static int et_video(struct gspca_dev *gspca_dev,
		    int on)
{
	int ret;

	reg_w_val(gspca_dev, ET_GPIO_OUT,
		  on ? 0x10		
		     : 0);		
	ret = Et_WaitStatus(gspca_dev);
	if (ret != 0)
		PDEBUG(D_ERR, "timeout video on/off");
	return ret;
}

static void Et_init2(struct gspca_dev *gspca_dev)
{
	__u8 value;
	static const __u8 FormLine[] = { 0x84, 0x03, 0x14, 0xf4, 0x01, 0x05 };

	PDEBUG(D_STREAM, "Open Init2 ET");
	reg_w_val(gspca_dev, ET_GPIO_DIR_CTRL, 0x2f);
	reg_w_val(gspca_dev, ET_GPIO_OUT, 0x10);
	reg_r(gspca_dev, ET_GPIO_IN, 1);
	reg_w_val(gspca_dev, ET_ClCK, 0x14); 
	reg_w_val(gspca_dev, ET_CTRL, 0x1b);

	
	if (gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv)
		value = ET_COMP_VAL1;	
	else
		value = ET_COMP_VAL0;	
	reg_w_val(gspca_dev, ET_COMP, value);
	reg_w_val(gspca_dev, ET_MAXQt, 0x1f);
	reg_w_val(gspca_dev, ET_MINQt, 0x04);
	
	reg_w_val(gspca_dev, ET_REG1d, 0xff);
	reg_w_val(gspca_dev, ET_REG1e, 0xff);
	reg_w_val(gspca_dev, ET_REG1f, 0xff);
	reg_w_val(gspca_dev, ET_REG20, 0x35);
	reg_w_val(gspca_dev, ET_REG21, 0x01);
	reg_w_val(gspca_dev, ET_REG22, 0x00);
	reg_w_val(gspca_dev, ET_REG23, 0xff);
	reg_w_val(gspca_dev, ET_REG24, 0xff);
	reg_w_val(gspca_dev, ET_REG25, 0x0f);
	
	reg_w_val(gspca_dev, 0x30, 0x11);		
	reg_w_val(gspca_dev, 0x31, 0x40);
	reg_w_val(gspca_dev, 0x32, 0x00);
	reg_w_val(gspca_dev, ET_O_RED, 0x00);		
	reg_w_val(gspca_dev, ET_O_GREEN1, 0x00);
	reg_w_val(gspca_dev, ET_O_BLUE, 0x00);
	reg_w_val(gspca_dev, ET_O_GREEN2, 0x00);
	
	reg_w_val(gspca_dev, ET_G_RED, 0x80);		
	reg_w_val(gspca_dev, ET_G_GREEN1, 0x80);
	reg_w_val(gspca_dev, ET_G_BLUE, 0x80);
	reg_w_val(gspca_dev, ET_G_GREEN2, 0x80);
	reg_w_val(gspca_dev, ET_G_GR_H, 0x00);
	reg_w_val(gspca_dev, ET_G_GB_H, 0x00);		
	
	reg_w_val(gspca_dev, 0x61, 0x80);		
	reg_w_val(gspca_dev, 0x62, 0x02);
	reg_w_val(gspca_dev, 0x63, 0x03);
	reg_w_val(gspca_dev, 0x64, 0x14);
	reg_w_val(gspca_dev, 0x65, 0x0e);
	reg_w_val(gspca_dev, 0x66, 0x02);
	reg_w_val(gspca_dev, 0x67, 0x02);

	
	reg_w_val(gspca_dev, ET_SYNCHRO, 0x8f);		
	reg_w_val(gspca_dev, ET_STARTX, 0x69);		
	reg_w_val(gspca_dev, ET_STARTY, 0x0d);		
	reg_w_val(gspca_dev, ET_WIDTH_LOW, 0x80);
	reg_w_val(gspca_dev, ET_HEIGTH_LOW, 0xe0);
	reg_w_val(gspca_dev, ET_W_H_HEIGTH, 0x60);	
	reg_w_val(gspca_dev, ET_REG6e, 0x86);
	reg_w_val(gspca_dev, ET_REG6f, 0x01);
	reg_w_val(gspca_dev, ET_REG70, 0x26);
	reg_w_val(gspca_dev, ET_REG71, 0x7a);
	reg_w_val(gspca_dev, ET_REG72, 0x01);
	
	reg_w_val(gspca_dev, ET_REG73, 0x00);
	reg_w_val(gspca_dev, ET_REG74, 0x18);		
	reg_w_val(gspca_dev, ET_REG75, 0x0f);		
	
	reg_w_val(gspca_dev, 0x8a, 0x20);
	reg_w_val(gspca_dev, 0x8d, 0x0f);
	reg_w_val(gspca_dev, 0x8e, 0x08);
	
	reg_w_val(gspca_dev, 0x03, 0x08);
	reg_w_val(gspca_dev, ET_PXL_CLK, 0x03);
	reg_w_val(gspca_dev, 0x81, 0xff);
	reg_w_val(gspca_dev, 0x80, 0x00);
	reg_w_val(gspca_dev, 0x81, 0xff);
	reg_w_val(gspca_dev, 0x80, 0x20);
	reg_w_val(gspca_dev, 0x03, 0x01);
	reg_w_val(gspca_dev, 0x03, 0x00);
	reg_w_val(gspca_dev, 0x03, 0x08);
	

	
	if (gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv)
		value = 0x04;		
	else				
		value = 0x1e;	
	reg_w_val(gspca_dev, ET_PXL_CLK, value);
	
	reg_w(gspca_dev, 0x62, FormLine, 6);

	
	reg_w_val(gspca_dev, 0x81, 0x47);	
	reg_w_val(gspca_dev, 0x80, 0x40);	
	
	
	
	
	reg_w_val(gspca_dev, 0x81, 0x30);	
	reg_w_val(gspca_dev, 0x80, 0x20);	
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i;
	__u8 brightness = sd->brightness;

	for (i = 0; i < 4; i++)
		reg_w_val(gspca_dev, ET_O_RED + i, brightness);
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 RGBG[] = { 0x80, 0x80, 0x80, 0x80, 0x00, 0x00 };
	__u8 contrast = sd->contrast;

	memset(RGBG, contrast, sizeof(RGBG) - 2);
	reg_w(gspca_dev, ET_G_RED, RGBG, 6);
}

static void setcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 I2cc[] = { 0x05, 0x02, 0x02, 0x05, 0x0d };
	__u8 i2cflags = 0x01;
	
	__u8 colors = sd->colors;

	I2cc[3] = colors;	
	I2cc[0] = 15 - colors;	
	
	
	if (sd->sensor == SENSOR_PAS106) {
		i2c_w(gspca_dev, PAS106_REG13, &i2cflags, 1, 3);
		i2c_w(gspca_dev, PAS106_REG9, I2cc, sizeof I2cc, 1);
	}
}

static void getcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->sensor == SENSOR_PAS106) {
		i2c_r(gspca_dev, PAS106_REG9 + 3);	
		sd->colors = gspca_dev->usb_buf[0] & 0x0f;
	}
}

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->autogain)
		sd->ag_cnt = AG_CNT_START;
	else
		sd->ag_cnt = -1;
}

static void Et_init1(struct gspca_dev *gspca_dev)
{
	__u8 value;
	__u8 I2c0[] = { 0x0a, 0x12, 0x05, 0x6d, 0xcd, 0x00, 0x01, 0x00 };
						

	PDEBUG(D_STREAM, "Open Init1 ET");
	reg_w_val(gspca_dev, ET_GPIO_DIR_CTRL, 7);
	reg_r(gspca_dev, ET_GPIO_IN, 1);
	reg_w_val(gspca_dev, ET_RESET_ALL, 1);
	reg_w_val(gspca_dev, ET_RESET_ALL, 0);
	reg_w_val(gspca_dev, ET_ClCK, 0x10);
	reg_w_val(gspca_dev, ET_CTRL, 0x19);
	
	if (gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv)
		value = ET_COMP_VAL1;
	else
		value = ET_COMP_VAL0;
	PDEBUG(D_STREAM, "Open mode %d Compression %d",
	       gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv,
	       value);
	reg_w_val(gspca_dev, ET_COMP, value);
	reg_w_val(gspca_dev, ET_MAXQt, 0x1d);
	reg_w_val(gspca_dev, ET_MINQt, 0x02);
	
	reg_w_val(gspca_dev, ET_REG1d, 0xff);
	reg_w_val(gspca_dev, ET_REG1e, 0xff);
	reg_w_val(gspca_dev, ET_REG1f, 0xff);
	reg_w_val(gspca_dev, ET_REG20, 0x35);
	reg_w_val(gspca_dev, ET_REG21, 0x01);
	reg_w_val(gspca_dev, ET_REG22, 0x00);
	reg_w_val(gspca_dev, ET_REG23, 0xf7);
	reg_w_val(gspca_dev, ET_REG24, 0xff);
	reg_w_val(gspca_dev, ET_REG25, 0x07);
	
	reg_w_val(gspca_dev, ET_G_RED, 0x80);
	reg_w_val(gspca_dev, ET_G_GREEN1, 0x80);
	reg_w_val(gspca_dev, ET_G_BLUE, 0x80);
	reg_w_val(gspca_dev, ET_G_GREEN2, 0x80);
	reg_w_val(gspca_dev, ET_G_GR_H, 0x00);
	reg_w_val(gspca_dev, ET_G_GB_H, 0x00);
	
	reg_w_val(gspca_dev, ET_SYNCHRO, 0xf0);
	reg_w_val(gspca_dev, ET_STARTX, 0x56);		
	reg_w_val(gspca_dev, ET_STARTY, 0x05);		
	reg_w_val(gspca_dev, ET_WIDTH_LOW, 0x60);
	reg_w_val(gspca_dev, ET_HEIGTH_LOW, 0x20);
	reg_w_val(gspca_dev, ET_W_H_HEIGTH, 0x50);
	reg_w_val(gspca_dev, ET_REG6e, 0x86);
	reg_w_val(gspca_dev, ET_REG6f, 0x01);
	reg_w_val(gspca_dev, ET_REG70, 0x86);
	reg_w_val(gspca_dev, ET_REG71, 0x14);
	reg_w_val(gspca_dev, ET_REG72, 0x00);
	
	reg_w_val(gspca_dev, ET_REG73, 0x00);
	reg_w_val(gspca_dev, ET_REG74, 0x00);
	reg_w_val(gspca_dev, ET_REG75, 0x0a);
	reg_w_val(gspca_dev, ET_I2C_CLK, 0x04);
	reg_w_val(gspca_dev, ET_PXL_CLK, 0x01);
	
	if (gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv) {
		I2c0[0] = 0x06;
		i2c_w(gspca_dev, PAS106_REG2, I2c0, sizeof I2c0, 1);
		i2c_w(gspca_dev, PAS106_REG9, I2c2, sizeof I2c2, 1);
		value = 0x06;
		i2c_w(gspca_dev, PAS106_REG2, &value, 1, 1);
		i2c_w(gspca_dev, PAS106_REG3, I2c3, sizeof I2c3, 1);
		
		value = 0x04;
		i2c_w(gspca_dev, PAS106_REG0e, &value, 1, 1);
	} else {
		I2c0[0] = 0x0a;

		i2c_w(gspca_dev, PAS106_REG2, I2c0, sizeof I2c0, 1);
		i2c_w(gspca_dev, PAS106_REG9, I2c2, sizeof I2c2, 1);
		value = 0x0a;
		i2c_w(gspca_dev, PAS106_REG2, &value, 1, 1);
		i2c_w(gspca_dev, PAS106_REG3, I2c3, sizeof I2c3, 1);
		value = 0x04;
		
		i2c_w(gspca_dev, PAS106_REG0e, &value, 1, 1);
	}

	
	i2c_w(gspca_dev, PAS106_REG7, I2c4, sizeof I2c4, 1);
	
	reg_w(gspca_dev, ET_G_RED, GainRGBG, 6);
	getcolors(gspca_dev);
	setcolors(gspca_dev);
}

static int sd_config(struct gspca_dev *gspca_dev,
		     const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;

	cam = &gspca_dev->cam;
	sd->sensor = id->driver_info;
	if (sd->sensor == SENSOR_PAS106) {
		cam->cam_mode = sif_mode;
		cam->nmodes = ARRAY_SIZE(sif_mode);
	} else {
		cam->cam_mode = vga_mode;
		cam->nmodes = ARRAY_SIZE(vga_mode);
		gspca_dev->ctrl_dis = (1 << COLOR_IDX);
	}
	sd->brightness = BRIGHTNESS_DEF;
	sd->contrast = CONTRAST_DEF;
	sd->colors = COLOR_DEF;
	sd->autogain = AUTOGAIN_DEF;
	sd->ag_cnt = -1;
	return 0;
}

static int sd_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->sensor == SENSOR_PAS106)
		Et_init1(gspca_dev);
	else
		Et_init2(gspca_dev);
	reg_w_val(gspca_dev, ET_RESET_ALL, 0x08);
	et_video(gspca_dev, 0);		
	return 0;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->sensor == SENSOR_PAS106)
		Et_init1(gspca_dev);
	else
		Et_init2(gspca_dev);

	setautogain(gspca_dev);

	reg_w_val(gspca_dev, ET_RESET_ALL, 0x08);
	et_video(gspca_dev, 1);		
	return 0;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	et_video(gspca_dev, 0);		
}

static __u8 Et_getgainG(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->sensor == SENSOR_PAS106) {
		i2c_r(gspca_dev, PAS106_REG0e);
		PDEBUG(D_CONF, "Etoms gain G %d", gspca_dev->usb_buf[0]);
		return gspca_dev->usb_buf[0];
	}
	return 0x1f;
}

static void Et_setgainG(struct gspca_dev *gspca_dev, __u8 gain)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->sensor == SENSOR_PAS106) {
		__u8 i2cflags = 0x01;

		i2c_w(gspca_dev, PAS106_REG13, &i2cflags, 1, 3);
		i2c_w(gspca_dev, PAS106_REG0e, &gain, 1, 1);
	}
}

#define BLIMIT(bright) \
	(u8)((bright > 0x1f) ? 0x1f : ((bright < 4) ? 3 : bright))
#define LIMIT(color) \
	(u8)((color > 0xff) ? 0xff : ((color < 0) ? 0 : color))

static void do_autogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 luma;
	__u8 luma_mean = 128;
	__u8 luma_delta = 20;
	__u8 spring = 4;
	int Gbright;
	__u8 r, g, b;

	if (sd->ag_cnt < 0)
		return;
	if (--sd->ag_cnt >= 0)
		return;
	sd->ag_cnt = AG_CNT_START;

	Gbright = Et_getgainG(gspca_dev);
	reg_r(gspca_dev, ET_LUMA_CENTER, 4);
	g = (gspca_dev->usb_buf[0] + gspca_dev->usb_buf[3]) >> 1;
	r = gspca_dev->usb_buf[1];
	b = gspca_dev->usb_buf[2];
	r = ((r << 8) - (r << 4) - (r << 3)) >> 10;
	b = ((b << 7) >> 10);
	g = ((g << 9) + (g << 7) + (g << 5)) >> 10;
	luma = LIMIT(r + g + b);
	PDEBUG(D_FRAM, "Etoms luma G %d", luma);
	if (luma < luma_mean - luma_delta || luma > luma_mean + luma_delta) {
		Gbright += (luma_mean - luma) >> spring;
		Gbright = BLIMIT(Gbright);
		PDEBUG(D_FRAM, "Etoms Gbright %d", Gbright);
		Et_setgainG(gspca_dev, (__u8) Gbright);
	}
}

#undef BLIMIT
#undef LIMIT

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data,			
			int len)			
{
	int seqframe;

	seqframe = data[0] & 0x3f;
	len = (int) (((data[0] & 0xc0) << 2) | data[1]);
	if (seqframe == 0x3f) {
		PDEBUG(D_FRAM,
		       "header packet found datalength %d !!", len);
		PDEBUG(D_FRAM, "G %d R %d G %d B %d",
		       data[2], data[3], data[4], data[5]);
		data += 30;
		
		gspca_frame_add(gspca_dev, LAST_PACKET, NULL, 0);
		gspca_frame_add(gspca_dev, FIRST_PACKET, data, len);
		return;
	}
	if (len) {
		data += 8;
		gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
	} else {			
		gspca_dev->last_packet_type = DISCARD_PACKET;
	}
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

static int sd_setcolors(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->colors = val;
	if (gspca_dev->streaming)
		setcolors(gspca_dev);
	return 0;
}

static int sd_getcolors(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->colors;
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

static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.pkt_scan = sd_pkt_scan,
	.dq_callback = do_autogain,
};

static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x102c, 0x6151), .driver_info = SENSOR_PAS106},
#if !defined CONFIG_USB_ET61X251 && !defined CONFIG_USB_ET61X251_MODULE
	{USB_DEVICE(0x102c, 0x6251), .driver_info = SENSOR_TAS5130CXX},
#endif
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
