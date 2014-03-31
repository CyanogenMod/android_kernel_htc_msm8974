/*
 * A V4L2 driver for OmniVision OV7670 cameras.
 *
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <media/ov7670.h>

MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for OmniVision ov7670 sensors");
MODULE_LICENSE("GPL");

static bool debug;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define QVGA_WIDTH	320
#define QVGA_HEIGHT	240
#define CIF_WIDTH	352
#define CIF_HEIGHT	288
#define QCIF_WIDTH	176
#define	QCIF_HEIGHT	144

#define OV7670_I2C_ADDR 0x42

#define REG_GAIN	0x00	
#define REG_BLUE	0x01	
#define REG_RED		0x02	
#define REG_VREF	0x03	
#define REG_COM1	0x04	
#define  COM1_CCIR656	  0x40  
#define REG_BAVE	0x05	
#define REG_GbAVE	0x06	
#define REG_AECHH	0x07	
#define REG_RAVE	0x08	
#define REG_COM2	0x09	
#define  COM2_SSLEEP	  0x10	
#define REG_PID		0x0a	
#define REG_VER		0x0b	
#define REG_COM3	0x0c	
#define  COM3_SWAP	  0x40	  
#define  COM3_SCALEEN	  0x08	  
#define  COM3_DCWEN	  0x04	  
#define REG_COM4	0x0d	
#define REG_COM5	0x0e	
#define REG_COM6	0x0f	
#define REG_AECH	0x10	
#define REG_CLKRC	0x11	
#define   CLK_EXT	  0x40	  
#define   CLK_SCALE	  0x3f	  
#define REG_COM7	0x12	
#define   COM7_RESET	  0x80	  
#define   COM7_FMT_MASK	  0x38
#define   COM7_FMT_VGA	  0x00
#define	  COM7_FMT_CIF	  0x20	  
#define   COM7_FMT_QVGA	  0x10	  
#define   COM7_FMT_QCIF	  0x08	  
#define	  COM7_RGB	  0x04	  
#define	  COM7_YUV	  0x00	  
#define	  COM7_BAYER	  0x01	  
#define	  COM7_PBAYER	  0x05	  
#define REG_COM8	0x13	
#define   COM8_FASTAEC	  0x80	  
#define   COM8_AECSTEP	  0x40	  
#define   COM8_BFILT	  0x20	  
#define   COM8_AGC	  0x04	  
#define   COM8_AWB	  0x02	  
#define   COM8_AEC	  0x01	  
#define REG_COM9	0x14	
#define REG_COM10	0x15	
#define   COM10_HSYNC	  0x40	  
#define   COM10_PCLK_HB	  0x20	  
#define   COM10_HREF_REV  0x08	  
#define   COM10_VS_LEAD	  0x04	  
#define   COM10_VS_NEG	  0x02	  
#define   COM10_HS_NEG	  0x01	  
#define REG_HSTART	0x17	
#define REG_HSTOP	0x18	
#define REG_VSTART	0x19	
#define REG_VSTOP	0x1a	
#define REG_PSHFT	0x1b	
#define REG_MIDH	0x1c	
#define REG_MIDL	0x1d	
#define REG_MVFP	0x1e	
#define   MVFP_MIRROR	  0x20	  
#define   MVFP_FLIP	  0x10	  

#define REG_AEW		0x24	
#define REG_AEB		0x25	
#define REG_VPT		0x26	
#define REG_HSYST	0x30	
#define REG_HSYEN	0x31	
#define REG_HREF	0x32	
#define REG_TSLB	0x3a	
#define   TSLB_YLAST	  0x04	  
#define REG_COM11	0x3b	
#define   COM11_NIGHT	  0x80	  
#define   COM11_NMFR	  0x60	  
#define   COM11_HZAUTO	  0x10	  
#define	  COM11_50HZ	  0x08	  
#define   COM11_EXP	  0x02
#define REG_COM12	0x3c	
#define   COM12_HREF	  0x80	  
#define REG_COM13	0x3d	
#define   COM13_GAMMA	  0x80	  
#define	  COM13_UVSAT	  0x40	  
#define   COM13_UVSWAP	  0x01	  
#define REG_COM14	0x3e	
#define   COM14_DCWEN	  0x10	  
#define REG_EDGE	0x3f	
#define REG_COM15	0x40	
#define   COM15_R10F0	  0x00	  
#define	  COM15_R01FE	  0x80	  
#define   COM15_R00FF	  0xc0	  
#define   COM15_RGB565	  0x10	  
#define   COM15_RGB555	  0x30	  
#define REG_COM16	0x41	
#define   COM16_AWBGAIN   0x08	  
#define REG_COM17	0x42	
#define   COM17_AECWIN	  0xc0	  
#define   COM17_CBAR	  0x08	  

#define	REG_CMATRIX_BASE 0x4f
#define   CMATRIX_LEN 6
#define REG_CMATRIX_SIGN 0x58


#define REG_BRIGHT	0x55	
#define REG_CONTRAS	0x56	

#define REG_GFIX	0x69	

#define REG_REG76	0x76	
#define   R76_BLKPCOR	  0x80	  
#define   R76_WHTPCOR	  0x40	  

#define REG_RGB444	0x8c	
#define   R444_ENABLE	  0x02	  
#define   R444_RGBX	  0x01	  

#define REG_HAECC1	0x9f	
#define REG_HAECC2	0xa0	

#define REG_BD50MAX	0xa5	
#define REG_HAECC3	0xa6	
#define REG_HAECC4	0xa7	
#define REG_HAECC5	0xa8	
#define REG_HAECC6	0xa9	
#define REG_HAECC7	0xaa	
#define REG_BD60MAX	0xab	


struct ov7670_format_struct;  
struct ov7670_info {
	struct v4l2_subdev sd;
	struct ov7670_format_struct *fmt;  
	unsigned char sat;		
	int hue;			
	int min_width;			
	int min_height;			
	int clock_speed;		
	u8 clkrc;			
	bool use_smbus;			
};

static inline struct ov7670_info *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov7670_info, sd);
}




struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list ov7670_default_regs[] = {
	{ REG_COM7, COM7_RESET },
	{ REG_CLKRC, 0x1 },	
	{ REG_TSLB,  0x04 },	
	{ REG_COM7, 0 },	
	{ REG_HSTART, 0x13 },	{ REG_HSTOP, 0x01 },
	{ REG_HREF, 0xb6 },	{ REG_VSTART, 0x02 },
	{ REG_VSTOP, 0x7a },	{ REG_VREF, 0x0a },

	{ REG_COM3, 0 },	{ REG_COM14, 0 },
	
	{ 0x70, 0x3a },		{ 0x71, 0x35 },
	{ 0x72, 0x11 },		{ 0x73, 0xf0 },
	{ 0xa2, 0x02 },		{ REG_COM10, 0x0 },

	
	{ 0x7a, 0x20 },		{ 0x7b, 0x10 },
	{ 0x7c, 0x1e },		{ 0x7d, 0x35 },
	{ 0x7e, 0x5a },		{ 0x7f, 0x69 },
	{ 0x80, 0x76 },		{ 0x81, 0x80 },
	{ 0x82, 0x88 },		{ 0x83, 0x8f },
	{ 0x84, 0x96 },		{ 0x85, 0xa3 },
	{ 0x86, 0xaf },		{ 0x87, 0xc4 },
	{ 0x88, 0xd7 },		{ 0x89, 0xe8 },

	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT },
	{ REG_GAIN, 0 },	{ REG_AECH, 0 },
	{ REG_COM4, 0x40 }, 
	{ REG_COM9, 0x18 }, 
	{ REG_BD50MAX, 0x05 },	{ REG_BD60MAX, 0x07 },
	{ REG_AEW, 0x95 },	{ REG_AEB, 0x33 },
	{ REG_VPT, 0xe3 },	{ REG_HAECC1, 0x78 },
	{ REG_HAECC2, 0x68 },	{ 0xa1, 0x03 }, 
	{ REG_HAECC3, 0xd8 },	{ REG_HAECC4, 0xd8 },
	{ REG_HAECC5, 0xf0 },	{ REG_HAECC6, 0x90 },
	{ REG_HAECC7, 0x94 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC },

	
	{ REG_COM5, 0x61 },	{ REG_COM6, 0x4b },
	{ 0x16, 0x02 },		{ REG_MVFP, 0x07 },
	{ 0x21, 0x02 },		{ 0x22, 0x91 },
	{ 0x29, 0x07 },		{ 0x33, 0x0b },
	{ 0x35, 0x0b },		{ 0x37, 0x1d },
	{ 0x38, 0x71 },		{ 0x39, 0x2a },
	{ REG_COM12, 0x78 },	{ 0x4d, 0x40 },
	{ 0x4e, 0x20 },		{ REG_GFIX, 0 },
	{ 0x6b, 0x4a },		{ 0x74, 0x10 },
	{ 0x8d, 0x4f },		{ 0x8e, 0 },
	{ 0x8f, 0 },		{ 0x90, 0 },
	{ 0x91, 0 },		{ 0x96, 0 },
	{ 0x9a, 0 },		{ 0xb0, 0x84 },
	{ 0xb1, 0x0c },		{ 0xb2, 0x0e },
	{ 0xb3, 0x82 },		{ 0xb8, 0x0a },

	
	{ 0x43, 0x0a },		{ 0x44, 0xf0 },
	{ 0x45, 0x34 },		{ 0x46, 0x58 },
	{ 0x47, 0x28 },		{ 0x48, 0x3a },
	{ 0x59, 0x88 },		{ 0x5a, 0x88 },
	{ 0x5b, 0x44 },		{ 0x5c, 0x67 },
	{ 0x5d, 0x49 },		{ 0x5e, 0x0e },
	{ 0x6c, 0x0a },		{ 0x6d, 0x55 },
	{ 0x6e, 0x11 },		{ 0x6f, 0x9f }, 
	{ 0x6a, 0x40 },		{ REG_BLUE, 0x40 },
	{ REG_RED, 0x60 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC|COM8_AWB },

	
	{ 0x4f, 0x80 },		{ 0x50, 0x80 },
	{ 0x51, 0 },		{ 0x52, 0x22 },
	{ 0x53, 0x5e },		{ 0x54, 0x80 },
	{ 0x58, 0x9e },

	{ REG_COM16, COM16_AWBGAIN },	{ REG_EDGE, 0 },
	{ 0x75, 0x05 },		{ 0x76, 0xe1 },
	{ 0x4c, 0 },		{ 0x77, 0x01 },
	{ REG_COM13, 0xc3 },	{ 0x4b, 0x09 },
	{ 0xc9, 0x60 },		{ REG_COM16, 0x38 },
	{ 0x56, 0x40 },

	{ 0x34, 0x11 },		{ REG_COM11, COM11_EXP|COM11_HZAUTO },
	{ 0xa4, 0x88 },		{ 0x96, 0 },
	{ 0x97, 0x30 },		{ 0x98, 0x20 },
	{ 0x99, 0x30 },		{ 0x9a, 0x84 },
	{ 0x9b, 0x29 },		{ 0x9c, 0x03 },
	{ 0x9d, 0x4c },		{ 0x9e, 0x3f },
	{ 0x78, 0x04 },

	
	{ 0x79, 0x01 },		{ 0xc8, 0xf0 },
	{ 0x79, 0x0f },		{ 0xc8, 0x00 },
	{ 0x79, 0x10 },		{ 0xc8, 0x7e },
	{ 0x79, 0x0a },		{ 0xc8, 0x80 },
	{ 0x79, 0x0b },		{ 0xc8, 0x01 },
	{ 0x79, 0x0c },		{ 0xc8, 0x0f },
	{ 0x79, 0x0d },		{ 0xc8, 0x20 },
	{ 0x79, 0x09 },		{ 0xc8, 0x80 },
	{ 0x79, 0x02 },		{ 0xc8, 0xc0 },
	{ 0x79, 0x03 },		{ 0xc8, 0x40 },
	{ 0x79, 0x05 },		{ 0xc8, 0x30 },
	{ 0x79, 0x26 },

	{ 0xff, 0xff },	
};




static struct regval_list ov7670_fmt_yuv422[] = {
	{ REG_COM7, 0x0 },  
	{ REG_RGB444, 0 },	
	{ REG_COM1, 0 },	
	{ REG_COM15, COM15_R00FF },
	{ REG_COM9, 0x18 }, 
	{ 0x4f, 0x80 }, 	
	{ 0x50, 0x80 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x22 }, 	
	{ 0x53, 0x5e }, 	
	{ 0x54, 0x80 }, 	
	{ REG_COM13, COM13_GAMMA|COM13_UVSAT },
	{ 0xff, 0xff },
};

static struct regval_list ov7670_fmt_rgb565[] = {
	{ REG_COM7, COM7_RGB },	
	{ REG_RGB444, 0 },	
	{ REG_COM1, 0x0 },	
	{ REG_COM15, COM15_RGB565 },
	{ REG_COM9, 0x38 }, 	
	{ 0x4f, 0xb3 }, 	
	{ 0x50, 0xb3 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x3d }, 	
	{ 0x53, 0xa7 }, 	
	{ 0x54, 0xe4 }, 	
	{ REG_COM13, COM13_GAMMA|COM13_UVSAT },
	{ 0xff, 0xff },
};

static struct regval_list ov7670_fmt_rgb444[] = {
	{ REG_COM7, COM7_RGB },	
	{ REG_RGB444, R444_ENABLE },	
	{ REG_COM1, 0x0 },	
	{ REG_COM15, COM15_R01FE|COM15_RGB565 }, 
	{ REG_COM9, 0x38 }, 	
	{ 0x4f, 0xb3 }, 	
	{ 0x50, 0xb3 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x3d }, 	
	{ 0x53, 0xa7 }, 	
	{ 0x54, 0xe4 }, 	
	{ REG_COM13, COM13_GAMMA|COM13_UVSAT|0x2 },  
	{ 0xff, 0xff },
};

static struct regval_list ov7670_fmt_raw[] = {
	{ REG_COM7, COM7_BAYER },
	{ REG_COM13, 0x08 }, 
	{ REG_COM16, 0x3d }, 
	{ REG_REG76, 0xe1 }, 
	{ 0xff, 0xff },
};



static int ov7670_read_smbus(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret >= 0) {
		*value = (unsigned char)ret;
		ret = 0;
	}
	return ret;
}


static int ov7670_write_smbus(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = i2c_smbus_write_byte_data(client, reg, value);

	if (reg == REG_COM7 && (value & COM7_RESET))
		msleep(5);  
	return ret;
}

static int ov7670_read_i2c(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 data = reg;
	struct i2c_msg msg;
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		printk(KERN_ERR "Error %d on register write\n", ret);
		return ret;
	}
	msg.flags = I2C_M_RD;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0) {
		*value = data;
		ret = 0;
	}
	return ret;
}


static int ov7670_write_i2c(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	unsigned char data[2] = { reg, value };
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;
	if (reg == REG_COM7 && (value & COM7_RESET))
		msleep(5);  
	return ret;
}

static int ov7670_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct ov7670_info *info = to_state(sd);
	if (info->use_smbus)
		return ov7670_read_smbus(sd, reg, value);
	else
		return ov7670_read_i2c(sd, reg, value);
}

static int ov7670_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct ov7670_info *info = to_state(sd);
	if (info->use_smbus)
		return ov7670_write_smbus(sd, reg, value);
	else
		return ov7670_write_i2c(sd, reg, value);
}

static int ov7670_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != 0xff || vals->value != 0xff) {
		int ret = ov7670_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}


static int ov7670_reset(struct v4l2_subdev *sd, u32 val)
{
	ov7670_write(sd, REG_COM7, COM7_RESET);
	msleep(1);
	return 0;
}


static int ov7670_init(struct v4l2_subdev *sd, u32 val)
{
	return ov7670_write_array(sd, ov7670_default_regs);
}



static int ov7670_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov7670_init(sd, 0);
	if (ret < 0)
		return ret;
	ret = ov7670_read(sd, REG_MIDH, &v);
	if (ret < 0)
		return ret;
	if (v != 0x7f) 
		return -ENODEV;
	ret = ov7670_read(sd, REG_MIDL, &v);
	if (ret < 0)
		return ret;
	if (v != 0xa2)
		return -ENODEV;
	ret = ov7670_read(sd, REG_PID, &v);
	if (ret < 0)
		return ret;
	if (v != 0x76)  
		return -ENODEV;
	ret = ov7670_read(sd, REG_VER, &v);
	if (ret < 0)
		return ret;
	if (v != 0x73)  
		return -ENODEV;
	return 0;
}


static struct ov7670_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
	int cmatrix[CMATRIX_LEN];
} ov7670_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= ov7670_fmt_yuv422,
		.cmatrix	= { 128, -128, 0, -34, -94, 128 },
	},
	{
		.mbus_code	= V4L2_MBUS_FMT_RGB444_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs		= ov7670_fmt_rgb444,
		.cmatrix	= { 179, -179, 0, -61, -176, 228 },
	},
	{
		.mbus_code	= V4L2_MBUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs		= ov7670_fmt_rgb565,
		.cmatrix	= { 179, -179, 0, -61, -176, 228 },
	},
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov7670_fmt_raw,
		.cmatrix	= { 0, 0, 0, 0, 0, 0 },
	},
};
#define N_OV7670_FMTS ARRAY_SIZE(ov7670_formats)



static struct regval_list ov7670_qcif_regs[] = {
	{ REG_COM3, COM3_SCALEEN|COM3_DCWEN },
	{ REG_COM3, COM3_DCWEN },
	{ REG_COM14, COM14_DCWEN | 0x01},
	{ 0x73, 0xf1 },
	{ 0xa2, 0x52 },
	{ 0x7b, 0x1c },
	{ 0x7c, 0x28 },
	{ 0x7d, 0x3c },
	{ 0x7f, 0x69 },
	{ REG_COM9, 0x38 },
	{ 0xa1, 0x0b },
	{ 0x74, 0x19 },
	{ 0x9a, 0x80 },
	{ 0x43, 0x14 },
	{ REG_COM13, 0xc0 },
	{ 0xff, 0xff },
};

static struct ov7670_win_size {
	int	width;
	int	height;
	unsigned char com7_bit;
	int	hstart;		
	int	hstop;		
	int	vstart;		
	int	vstop;		
	struct regval_list *regs; 
} ov7670_win_sizes[] = {
	
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.com7_bit	= COM7_FMT_VGA,
		.hstart		= 158,		
		.hstop		=  14,		
		.vstart		=  10,
		.vstop		= 490,
		.regs 		= NULL,
	},
	
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.com7_bit	= COM7_FMT_CIF,
		.hstart		= 170,		
		.hstop		=  90,
		.vstart		=  14,
		.vstop		= 494,
		.regs 		= NULL,
	},
	
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.com7_bit	= COM7_FMT_QVGA,
		.hstart		= 168,		
		.hstop		=  24,
		.vstart		=  12,
		.vstop		= 492,
		.regs 		= NULL,
	},
	
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.com7_bit	= COM7_FMT_VGA, 
		.hstart		= 456,		
		.hstop		=  24,
		.vstart		=  14,
		.vstop		= 494,
		.regs 		= ov7670_qcif_regs,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(ov7670_win_sizes))


static int ov7670_set_hw(struct v4l2_subdev *sd, int hstart, int hstop,
		int vstart, int vstop)
{
	int ret;
	unsigned char v;
	ret =  ov7670_write(sd, REG_HSTART, (hstart >> 3) & 0xff);
	ret += ov7670_write(sd, REG_HSTOP, (hstop >> 3) & 0xff);
	ret += ov7670_read(sd, REG_HREF, &v);
	v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
	msleep(10);
	ret += ov7670_write(sd, REG_HREF, v);
	ret += ov7670_write(sd, REG_VSTART, (vstart >> 2) & 0xff);
	ret += ov7670_write(sd, REG_VSTOP, (vstop >> 2) & 0xff);
	ret += ov7670_read(sd, REG_VREF, &v);
	v = (v & 0xf0) | ((vstop & 0x3) << 2) | (vstart & 0x3);
	msleep(10);
	ret += ov7670_write(sd, REG_VREF, v);
	return ret;
}


static int ov7670_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV7670_FMTS)
		return -EINVAL;

	*code = ov7670_formats[index].mbus_code;
	return 0;
}

static int ov7670_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov7670_format_struct **ret_fmt,
		struct ov7670_win_size **ret_wsize)
{
	int index;
	struct ov7670_win_size *wsize;

	for (index = 0; index < N_OV7670_FMTS; index++)
		if (ov7670_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV7670_FMTS) {
		
		index = 0;
		fmt->code = ov7670_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov7670_formats + index;
	fmt->field = V4L2_FIELD_NONE;
	for (wsize = ov7670_win_sizes; wsize < ov7670_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;
	if (wsize >= ov7670_win_sizes + N_WIN_SIZES)
		wsize--;   
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov7670_formats[index].colorspace;
	return 0;
}

static int ov7670_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov7670_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov7670_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov7670_format_struct *ovfmt;
	struct ov7670_win_size *wsize;
	struct ov7670_info *info = to_state(sd);
	unsigned char com7;
	int ret;

	ret = ov7670_try_fmt_internal(sd, fmt, &ovfmt, &wsize);

	if (ret)
		return ret;
	/*
	 * COM7 is a pain in the ass, it doesn't like to be read then
	 * quickly written afterward.  But we have everything we need
	 * to set it absolutely here, as long as the format-specific
	 * register sets list it first.
	 */
	com7 = ovfmt->regs[0].value;
	com7 |= wsize->com7_bit;
	ov7670_write(sd, REG_COM7, com7);
	ov7670_write_array(sd, ovfmt->regs + 1);
	ov7670_set_hw(sd, wsize->hstart, wsize->hstop, wsize->vstart,
			wsize->vstop);
	ret = 0;
	if (wsize->regs)
		ret = ov7670_write_array(sd, wsize->regs);
	info->fmt = ovfmt;

	if (ret == 0)
		ret = ov7670_write(sd, REG_CLKRC, info->clkrc);
	return 0;
}

static int ov7670_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct ov7670_info *info = to_state(sd);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = info->clock_speed;
	if ((info->clkrc & CLK_EXT) == 0 && (info->clkrc & CLK_SCALE) > 1)
		cp->timeperframe.denominator /= (info->clkrc & CLK_SCALE);
	return 0;
}

static int ov7670_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;
	struct ov7670_info *info = to_state(sd);
	int div;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (cp->extendedmode != 0)
		return -EINVAL;

	if (tpf->numerator == 0 || tpf->denominator == 0)
		div = 1;  
	else
		div = (tpf->numerator * info->clock_speed) / tpf->denominator;
	if (div == 0)
		div = 1;
	else if (div > CLK_SCALE)
		div = CLK_SCALE;
	info->clkrc = (info->clkrc & 0x80) | div;
	tpf->numerator = 1;
	tpf->denominator = info->clock_speed / div;
	return ov7670_write(sd, REG_CLKRC, info->clkrc);
}



static int ov7670_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov7670_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov7670_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov7670_frame_rates[interval->index];
	return 0;
}

static int ov7670_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	struct ov7670_info *info = to_state(sd);
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	for (i = 0; i < N_WIN_SIZES; i++) {
		struct ov7670_win_size *win = &ov7670_win_sizes[index];
		if (info->min_width && win->width < info->min_width)
			continue;
		if (info->min_height && win->height < info->min_height)
			continue;
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}


static int ov7670_store_cmatrix(struct v4l2_subdev *sd,
		int matrix[CMATRIX_LEN])
{
	int i, ret;
	unsigned char signbits = 0;

	ret = ov7670_read(sd, REG_CMATRIX_SIGN, &signbits);
	signbits &= 0xc0;

	for (i = 0; i < CMATRIX_LEN; i++) {
		unsigned char raw;

		if (matrix[i] < 0) {
			signbits |= (1 << i);
			if (matrix[i] < -255)
				raw = 0xff;
			else
				raw = (-1 * matrix[i]) & 0xff;
		}
		else {
			if (matrix[i] > 255)
				raw = 0xff;
			else
				raw = matrix[i] & 0xff;
		}
		ret += ov7670_write(sd, REG_CMATRIX_BASE + i, raw);
	}
	ret += ov7670_write(sd, REG_CMATRIX_SIGN, signbits);
	return ret;
}


#define SIN_STEP 5
static const int ov7670_sin_table[] = {
	   0,	 87,   173,   258,   342,   422,
	 499,	573,   642,   707,   766,   819,
	 866,	906,   939,   965,   984,   996,
	1000
};

static int ov7670_sine(int theta)
{
	int chs = 1;
	int sine;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sine = ov7670_sin_table[theta/SIN_STEP];
	else {
		theta -= 90;
		sine = 1000 - ov7670_sin_table[theta/SIN_STEP];
	}
	return sine*chs;
}

static int ov7670_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return ov7670_sine(theta);
}




static void ov7670_calc_cmatrix(struct ov7670_info *info,
		int matrix[CMATRIX_LEN])
{
	int i;
	for (i = 0; i < CMATRIX_LEN; i++)
		matrix[i] = (info->fmt->cmatrix[i]*info->sat) >> 7;
	if (info->hue != 0) {
		int sinth, costh, tmpmatrix[CMATRIX_LEN];

		memcpy(tmpmatrix, matrix, CMATRIX_LEN*sizeof(int));
		sinth = ov7670_sine(info->hue);
		costh = ov7670_cosine(info->hue);

		matrix[0] = (matrix[3]*sinth + matrix[0]*costh)/1000;
		matrix[1] = (matrix[4]*sinth + matrix[1]*costh)/1000;
		matrix[2] = (matrix[5]*sinth + matrix[2]*costh)/1000;
		matrix[3] = (matrix[3]*costh - matrix[0]*sinth)/1000;
		matrix[4] = (matrix[4]*costh - matrix[1]*sinth)/1000;
		matrix[5] = (matrix[5]*costh - matrix[2]*sinth)/1000;
	}
}



static int ov7670_s_sat(struct v4l2_subdev *sd, int value)
{
	struct ov7670_info *info = to_state(sd);
	int matrix[CMATRIX_LEN];
	int ret;

	info->sat = value;
	ov7670_calc_cmatrix(info, matrix);
	ret = ov7670_store_cmatrix(sd, matrix);
	return ret;
}

static int ov7670_g_sat(struct v4l2_subdev *sd, __s32 *value)
{
	struct ov7670_info *info = to_state(sd);

	*value = info->sat;
	return 0;
}

static int ov7670_s_hue(struct v4l2_subdev *sd, int value)
{
	struct ov7670_info *info = to_state(sd);
	int matrix[CMATRIX_LEN];
	int ret;

	if (value < -180 || value > 180)
		return -EINVAL;
	info->hue = value;
	ov7670_calc_cmatrix(info, matrix);
	ret = ov7670_store_cmatrix(sd, matrix);
	return ret;
}


static int ov7670_g_hue(struct v4l2_subdev *sd, __s32 *value)
{
	struct ov7670_info *info = to_state(sd);

	*value = info->hue;
	return 0;
}


static unsigned char ov7670_sm_to_abs(unsigned char v)
{
	if ((v & 0x80) == 0)
		return v + 128;
	return 128 - (v & 0x7f);
}


static unsigned char ov7670_abs_to_sm(unsigned char v)
{
	if (v > 127)
		return v & 0x7f;
	return (128 - v) | 0x80;
}

static int ov7670_s_brightness(struct v4l2_subdev *sd, int value)
{
	unsigned char com8 = 0, v;
	int ret;

	ov7670_read(sd, REG_COM8, &com8);
	com8 &= ~COM8_AEC;
	ov7670_write(sd, REG_COM8, com8);
	v = ov7670_abs_to_sm(value);
	ret = ov7670_write(sd, REG_BRIGHT, v);
	return ret;
}

static int ov7670_g_brightness(struct v4l2_subdev *sd, __s32 *value)
{
	unsigned char v = 0;
	int ret = ov7670_read(sd, REG_BRIGHT, &v);

	*value = ov7670_sm_to_abs(v);
	return ret;
}

static int ov7670_s_contrast(struct v4l2_subdev *sd, int value)
{
	return ov7670_write(sd, REG_CONTRAS, (unsigned char) value);
}

static int ov7670_g_contrast(struct v4l2_subdev *sd, __s32 *value)
{
	unsigned char v = 0;
	int ret = ov7670_read(sd, REG_CONTRAS, &v);

	*value = v;
	return ret;
}

static int ov7670_g_hflip(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char v = 0;

	ret = ov7670_read(sd, REG_MVFP, &v);
	*value = (v & MVFP_MIRROR) == MVFP_MIRROR;
	return ret;
}


static int ov7670_s_hflip(struct v4l2_subdev *sd, int value)
{
	unsigned char v = 0;
	int ret;

	ret = ov7670_read(sd, REG_MVFP, &v);
	if (value)
		v |= MVFP_MIRROR;
	else
		v &= ~MVFP_MIRROR;
	msleep(10);  
	ret += ov7670_write(sd, REG_MVFP, v);
	return ret;
}



static int ov7670_g_vflip(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char v = 0;

	ret = ov7670_read(sd, REG_MVFP, &v);
	*value = (v & MVFP_FLIP) == MVFP_FLIP;
	return ret;
}


static int ov7670_s_vflip(struct v4l2_subdev *sd, int value)
{
	unsigned char v = 0;
	int ret;

	ret = ov7670_read(sd, REG_MVFP, &v);
	if (value)
		v |= MVFP_FLIP;
	else
		v &= ~MVFP_FLIP;
	msleep(10);  
	ret += ov7670_write(sd, REG_MVFP, v);
	return ret;
}

static int ov7670_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char gain;

	ret = ov7670_read(sd, REG_GAIN, &gain);
	*value = gain;
	return ret;
}

static int ov7670_s_gain(struct v4l2_subdev *sd, int value)
{
	int ret;
	unsigned char com8;

	ret = ov7670_write(sd, REG_GAIN, value & 0xff);
	
	if (ret == 0) {
		ret = ov7670_read(sd, REG_COM8, &com8);
		ret = ov7670_write(sd, REG_COM8, com8 & ~COM8_AGC);
	}
	return ret;
}

static int ov7670_g_autogain(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char com8;

	ret = ov7670_read(sd, REG_COM8, &com8);
	*value = (com8 & COM8_AGC) != 0;
	return ret;
}

static int ov7670_s_autogain(struct v4l2_subdev *sd, int value)
{
	int ret;
	unsigned char com8;

	ret = ov7670_read(sd, REG_COM8, &com8);
	if (ret == 0) {
		if (value)
			com8 |= COM8_AGC;
		else
			com8 &= ~COM8_AGC;
		ret = ov7670_write(sd, REG_COM8, com8);
	}
	return ret;
}

static int ov7670_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char com1, aech, aechh;

	ret = ov7670_read(sd, REG_COM1, &com1) +
		ov7670_read(sd, REG_AECH, &aech) +
		ov7670_read(sd, REG_AECHH, &aechh);
	*value = ((aechh & 0x3f) << 10) | (aech << 2) | (com1 & 0x03);
	return ret;
}

static int ov7670_s_exp(struct v4l2_subdev *sd, int value)
{
	int ret;
	unsigned char com1, com8, aech, aechh;

	ret = ov7670_read(sd, REG_COM1, &com1) +
		ov7670_read(sd, REG_COM8, &com8);
		ov7670_read(sd, REG_AECHH, &aechh);
	if (ret)
		return ret;

	com1 = (com1 & 0xfc) | (value & 0x03);
	aech = (value >> 2) & 0xff;
	aechh = (aechh & 0xc0) | ((value >> 10) & 0x3f);
	ret = ov7670_write(sd, REG_COM1, com1) +
		ov7670_write(sd, REG_AECH, aech) +
		ov7670_write(sd, REG_AECHH, aechh);
	
	if (ret == 0)
		ret = ov7670_write(sd, REG_COM8, com8 & ~COM8_AEC);
	return ret;
}

static int ov7670_g_autoexp(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char com8;
	enum v4l2_exposure_auto_type *atype = (enum v4l2_exposure_auto_type *) value;

	ret = ov7670_read(sd, REG_COM8, &com8);
	if (com8 & COM8_AEC)
		*atype = V4L2_EXPOSURE_AUTO;
	else
		*atype = V4L2_EXPOSURE_MANUAL;
	return ret;
}

static int ov7670_s_autoexp(struct v4l2_subdev *sd,
		enum v4l2_exposure_auto_type value)
{
	int ret;
	unsigned char com8;

	ret = ov7670_read(sd, REG_COM8, &com8);
	if (ret == 0) {
		if (value == V4L2_EXPOSURE_AUTO)
			com8 |= COM8_AEC;
		else
			com8 &= ~COM8_AEC;
		ret = ov7670_write(sd, REG_COM8, com8);
	}
	return ret;
}



static int ov7670_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	
	switch (qc->id) {
	case V4L2_CID_BRIGHTNESS:
		return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
	case V4L2_CID_CONTRAST:
		return v4l2_ctrl_query_fill(qc, 0, 127, 1, 64);
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_SATURATION:
		return v4l2_ctrl_query_fill(qc, 0, 256, 1, 128);
	case V4L2_CID_HUE:
		return v4l2_ctrl_query_fill(qc, -180, 180, 5, 0);
	case V4L2_CID_GAIN:
		return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
	case V4L2_CID_AUTOGAIN:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, 0, 65535, 1, 500);
	case V4L2_CID_EXPOSURE_AUTO:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	}
	return -EINVAL;
}

static int ov7670_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return ov7670_g_brightness(sd, &ctrl->value);
	case V4L2_CID_CONTRAST:
		return ov7670_g_contrast(sd, &ctrl->value);
	case V4L2_CID_SATURATION:
		return ov7670_g_sat(sd, &ctrl->value);
	case V4L2_CID_HUE:
		return ov7670_g_hue(sd, &ctrl->value);
	case V4L2_CID_VFLIP:
		return ov7670_g_vflip(sd, &ctrl->value);
	case V4L2_CID_HFLIP:
		return ov7670_g_hflip(sd, &ctrl->value);
	case V4L2_CID_GAIN:
		return ov7670_g_gain(sd, &ctrl->value);
	case V4L2_CID_AUTOGAIN:
		return ov7670_g_autogain(sd, &ctrl->value);
	case V4L2_CID_EXPOSURE:
		return ov7670_g_exp(sd, &ctrl->value);
	case V4L2_CID_EXPOSURE_AUTO:
		return ov7670_g_autoexp(sd, &ctrl->value);
	}
	return -EINVAL;
}

static int ov7670_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return ov7670_s_brightness(sd, ctrl->value);
	case V4L2_CID_CONTRAST:
		return ov7670_s_contrast(sd, ctrl->value);
	case V4L2_CID_SATURATION:
		return ov7670_s_sat(sd, ctrl->value);
	case V4L2_CID_HUE:
		return ov7670_s_hue(sd, ctrl->value);
	case V4L2_CID_VFLIP:
		return ov7670_s_vflip(sd, ctrl->value);
	case V4L2_CID_HFLIP:
		return ov7670_s_hflip(sd, ctrl->value);
	case V4L2_CID_GAIN:
		return ov7670_s_gain(sd, ctrl->value);
	case V4L2_CID_AUTOGAIN:
		return ov7670_s_autogain(sd, ctrl->value);
	case V4L2_CID_EXPOSURE:
		return ov7670_s_exp(sd, ctrl->value);
	case V4L2_CID_EXPOSURE_AUTO:
		return ov7670_s_autoexp(sd,
				(enum v4l2_exposure_auto_type) ctrl->value);
	}
	return -EINVAL;
}

static int ov7670_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV7670, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov7670_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov7670_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int ov7670_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov7670_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif


static const struct v4l2_subdev_core_ops ov7670_core_ops = {
	.g_chip_ident = ov7670_g_chip_ident,
	.g_ctrl = ov7670_g_ctrl,
	.s_ctrl = ov7670_s_ctrl,
	.queryctrl = ov7670_queryctrl,
	.reset = ov7670_reset,
	.init = ov7670_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov7670_g_register,
	.s_register = ov7670_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov7670_video_ops = {
	.enum_mbus_fmt = ov7670_enum_mbus_fmt,
	.try_mbus_fmt = ov7670_try_mbus_fmt,
	.s_mbus_fmt = ov7670_s_mbus_fmt,
	.s_parm = ov7670_s_parm,
	.g_parm = ov7670_g_parm,
	.enum_frameintervals = ov7670_enum_frameintervals,
	.enum_framesizes = ov7670_enum_framesizes,
};

static const struct v4l2_subdev_ops ov7670_ops = {
	.core = &ov7670_core_ops,
	.video = &ov7670_video_ops,
};


static int ov7670_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov7670_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov7670_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov7670_ops);

	info->clock_speed = 30; 
	if (client->dev.platform_data) {
		struct ov7670_config *config = client->dev.platform_data;

		info->min_width = config->min_width;
		info->min_height = config->min_height;
		info->use_smbus = config->use_smbus;

		if (config->clock_speed)
			info->clock_speed = config->clock_speed;
	}

	
	ret = ov7670_detect(sd);
	if (ret) {
		v4l_dbg(1, debug, client,
			"chip found @ 0x%x (%s) is not an ov7670 chip.\n",
			client->addr << 1, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr << 1, client->adapter->name);

	info->fmt = &ov7670_formats[0];
	info->sat = 128;	
	info->clkrc = info->clock_speed / 30;
	return 0;
}


static int ov7670_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id ov7670_id[] = {
	{ "ov7670", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov7670_id);

static struct i2c_driver ov7670_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov7670",
	},
	.probe		= ov7670_probe,
	.remove		= ov7670_remove,
	.id_table	= ov7670_id,
};

module_i2c_driver(ov7670_driver);
