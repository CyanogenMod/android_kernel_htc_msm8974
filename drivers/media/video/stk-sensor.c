/* stk-sensor.c: Driver for ov96xx sensor (used in some Syntek webcams)
 *
 * Copyright 2007-2008 Jaime Velasco Juan <jsagarribay@gmail.com>
 *
 * Some parts derived from ov7670.c:
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * This file may be distributed under the terms of the GNU General
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include "stk-webcam.h"

#define STK_IIC_BASE		(0x0200)
#  define STK_IIC_OP		(STK_IIC_BASE)
#    define STK_IIC_OP_TX	(0x05)
#    define STK_IIC_OP_RX	(0x70)
#  define STK_IIC_STAT		(STK_IIC_BASE+1)
#    define STK_IIC_STAT_TX_OK	(0x04)
#    define STK_IIC_STAT_RX_OK	(0x01)
#  define STK_IIC_ENABLE	(STK_IIC_BASE+2)
#    define STK_IIC_ENABLE_NO	(0x00)
#    define STK_IIC_ENABLE_YES	(0x1e)
#  define STK_IIC_ADDR		(STK_IIC_BASE+3)
#  define STK_IIC_TX_INDEX	(STK_IIC_BASE+4)
#  define STK_IIC_TX_VALUE	(STK_IIC_BASE+5)
#  define STK_IIC_RX_INDEX	(STK_IIC_BASE+8)
#  define STK_IIC_RX_VALUE	(STK_IIC_BASE+9)

#define MAX_RETRIES		(50)

#define SENSOR_ADDRESS		(0x60)


#define REG_GAIN	0x00	
#define REG_BLUE	0x01	
#define REG_RED		0x02	
#define REG_VREF	0x03	
#define REG_COM1	0x04	
#define  COM1_CCIR656	  0x40  
#define  COM1_QFMT	  0x20  
#define  COM1_SKIP_0	  0x00  
#define  COM1_SKIP_2	  0x04  
#define  COM1_SKIP_3	  0x08  
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
#define   CLK_PLL	  0x80	  
#define   CLK_EXT	  0x40	  
#define   CLK_SCALE	  0x3f	  
#define REG_COM7	0x12	
#define   COM7_RESET	  0x80	  
#define   COM7_FMT_MASK	  0x38
#define   COM7_FMT_SXGA	  0x00
#define   COM7_FMT_VGA	  0x40
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
#define REG_ADVFL	0x2d	
#define REG_ADVFH	0x2e	
#define REG_HSYST	0x30	
#define REG_HSYEN	0x31	
#define REG_HREF	0x32	
#define REG_TSLB	0x3a	
#define   TSLB_YLAST	  0x04	  
#define   TSLB_BYTEORD	  0x08	  
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
#define	  COM13_CMATRIX	  0x10	  
#define   COM13_UVSWAP	  0x01	  
#define REG_COM14	0x3e	
#define   COM14_DCWEN	  0x10	  
#define REG_EDGE	0x3f	
#define REG_COM15	0x40	
#define   COM15_R10F0	  0x00	  
#define	  COM15_R01FE	  0x80	  
#define   COM15_R00FF	  0xc0	  
#define   COM15_RGB565	  0x10	  
#define   COM15_RGBFIXME	  0x20	  
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




static int stk_sensor_outb(struct stk_camera *dev, u8 reg, u8 val)
{
	int i = 0;
	int tmpval = 0;

	if (stk_camera_write_reg(dev, STK_IIC_TX_INDEX, reg))
		return 1;
	if (stk_camera_write_reg(dev, STK_IIC_TX_VALUE, val))
		return 1;
	if (stk_camera_write_reg(dev, STK_IIC_OP, STK_IIC_OP_TX))
		return 1;
	do {
		if (stk_camera_read_reg(dev, STK_IIC_STAT, &tmpval))
			return 1;
		i++;
	} while (tmpval == 0 && i < MAX_RETRIES);
	if (tmpval != STK_IIC_STAT_TX_OK) {
		if (tmpval)
			STK_ERROR("stk_sensor_outb failed, status=0x%02x\n",
				tmpval);
		return 1;
	} else
		return 0;
}

static int stk_sensor_inb(struct stk_camera *dev, u8 reg, u8 *val)
{
	int i = 0;
	int tmpval = 0;

	if (stk_camera_write_reg(dev, STK_IIC_RX_INDEX, reg))
		return 1;
	if (stk_camera_write_reg(dev, STK_IIC_OP, STK_IIC_OP_RX))
		return 1;
	do {
		if (stk_camera_read_reg(dev, STK_IIC_STAT, &tmpval))
			return 1;
		i++;
	} while (tmpval == 0 && i < MAX_RETRIES);
	if (tmpval != STK_IIC_STAT_RX_OK) {
		if (tmpval)
			STK_ERROR("stk_sensor_inb failed, status=0x%02x\n",
				tmpval);
		return 1;
	}

	if (stk_camera_read_reg(dev, STK_IIC_RX_VALUE, &tmpval))
		return 1;

	*val = (u8) tmpval;
	return 0;
}

static int stk_sensor_write_regvals(struct stk_camera *dev,
		struct regval *rv)
{
	int ret;
	if (rv == NULL)
		return 0;
	while (rv->reg != 0xff || rv->val != 0xff) {
		ret = stk_sensor_outb(dev, rv->reg, rv->val);
		if (ret != 0)
			return ret;
		rv++;
	}
	return 0;
}

int stk_sensor_sleep(struct stk_camera *dev)
{
	u8 tmp;
	return stk_sensor_inb(dev, REG_COM2, &tmp)
		|| stk_sensor_outb(dev, REG_COM2, tmp|COM2_SSLEEP);
}

int stk_sensor_wakeup(struct stk_camera *dev)
{
	u8 tmp;
	return stk_sensor_inb(dev, REG_COM2, &tmp)
		|| stk_sensor_outb(dev, REG_COM2, tmp&~COM2_SSLEEP);
}

static struct regval ov_initvals[] = {
	{REG_CLKRC, CLK_PLL},
	{REG_COM11, 0x01},
	{0x6a, 0x7d},
	{REG_AECH, 0x40},
	{REG_GAIN, 0x00},
	{REG_BLUE, 0x80},
	{REG_RED, 0x80},
	
	
	{REG_COM8, COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC},
	{0x39, 0x50}, {0x38, 0x93},
	{0x37, 0x00}, {0x35, 0x81},
	{REG_COM5, 0x20},
	{REG_COM1, 0x00},
	{REG_COM3, 0x00},
	{REG_COM4, 0x00},
	{REG_PSHFT, 0x00},
	{0x16, 0x07},
	{0x33, 0xe2}, {0x34, 0xbf},
	{REG_COM16, 0x00},
	{0x96, 0x04},
	
	{REG_GFIX, 0x40},
	{0x8e, 0x00},
	{REG_COM12, 0x73},
	{0x8f, 0xdf}, {0x8b, 0x06},
	{0x8c, 0x20},
	{0x94, 0x88}, {0x95, 0x88},
	{0x29, 0x3f},
	{REG_COM6, 0x42},
	{REG_BD50MAX, 0x80},
	{REG_HAECC6, 0xb8}, {REG_HAECC7, 0x92},
	{REG_BD60MAX, 0x0a},
	{0x90, 0x00}, {0x91, 0x00},
	{REG_HAECC1, 0x00}, {REG_HAECC2, 0x00},
	{REG_AEW, 0x68}, {REG_AEB, 0x5c},
	{REG_VPT, 0xc3},
	{REG_COM9, 0x2e},
	{0x2a, 0x00}, {0x2b, 0x00},

	{0xff, 0xff}, 
};

int stk_sensor_init(struct stk_camera *dev)
{
	u8 idl = 0;
	u8 idh = 0;

	if (stk_camera_write_reg(dev, STK_IIC_ENABLE, STK_IIC_ENABLE_YES)
		|| stk_camera_write_reg(dev, STK_IIC_ADDR, SENSOR_ADDRESS)
		|| stk_sensor_outb(dev, REG_COM7, COM7_RESET)) {
		STK_ERROR("Sensor resetting failed\n");
		return -ENODEV;
	}
	msleep(10);
	
	if (stk_sensor_inb(dev, REG_MIDH, &idh)
	    || stk_sensor_inb(dev, REG_MIDL, &idl)) {
		STK_ERROR("Strange error reading sensor ID\n");
		return -ENODEV;
	}
	if (idh != 0x7f || idl != 0xa2) {
		STK_ERROR("Huh? you don't have a sensor from ovt\n");
		return -ENODEV;
	}
	if (stk_sensor_inb(dev, REG_PID, &idh)
	    || stk_sensor_inb(dev, REG_VER, &idl)) {
		STK_ERROR("Could not read sensor model\n");
		return -ENODEV;
	}
	stk_sensor_write_regvals(dev, ov_initvals);
	msleep(10);
	STK_INFO("OmniVision sensor detected, id %02X%02X"
		" at address %x\n", idh, idl, SENSOR_ADDRESS);
	return 0;
}

static struct regval ov_fmt_uyvy[] = {
	{REG_TSLB, TSLB_YLAST|0x08 },
	{ 0x4f, 0x80 }, 	
	{ 0x50, 0x80 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x22 }, 	
	{ 0x53, 0x5e }, 	
	{ 0x54, 0x80 }, 	
	{REG_COM13, COM13_UVSAT|COM13_CMATRIX},
	{REG_COM15, COM15_R00FF },
	{0xff, 0xff}, 
};
static struct regval ov_fmt_yuyv[] = {
	{REG_TSLB, 0 },
	{ 0x4f, 0x80 }, 	
	{ 0x50, 0x80 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x22 }, 	
	{ 0x53, 0x5e }, 	
	{ 0x54, 0x80 }, 	
	{REG_COM13, COM13_UVSAT|COM13_CMATRIX},
	{REG_COM15, COM15_R00FF },
	{0xff, 0xff}, 
};

static struct regval ov_fmt_rgbr[] = {
	{ REG_RGB444, 0 },	
	{REG_TSLB, 0x00},
	{ REG_COM1, 0x0 },
	{ REG_COM9, 0x38 }, 	
	{ 0x4f, 0xb3 }, 	
	{ 0x50, 0xb3 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x3d }, 	
	{ 0x53, 0xa7 }, 	
	{ 0x54, 0xe4 }, 	
	{ REG_COM13, COM13_GAMMA },
	{ REG_COM15, COM15_RGB565|COM15_R00FF },
	{ 0xff, 0xff },
};

static struct regval ov_fmt_rgbp[] = {
	{ REG_RGB444, 0 },	
	{REG_TSLB, TSLB_BYTEORD },
	{ REG_COM1, 0x0 },
	{ REG_COM9, 0x38 }, 	
	{ 0x4f, 0xb3 }, 	
	{ 0x50, 0xb3 }, 	
	{ 0x51, 0    },		
	{ 0x52, 0x3d }, 	
	{ 0x53, 0xa7 }, 	
	{ 0x54, 0xe4 }, 	
	{ REG_COM13, COM13_GAMMA },
	{ REG_COM15, COM15_RGB565|COM15_R00FF },
	{ 0xff, 0xff },
};

static struct regval ov_fmt_bayer[] = {
	
	{REG_TSLB, 0x40}, 
	 
	{REG_COM15, COM15_R00FF },
	{0xff, 0xff}, 
};
static int stk_sensor_set_hw(struct stk_camera *dev,
		int hstart, int hstop, int vstart, int vstop)
{
	int ret;
	unsigned char v;
	ret =  stk_sensor_outb(dev, REG_HSTART, (hstart >> 3) & 0xff);
	ret += stk_sensor_outb(dev, REG_HSTOP, (hstop >> 3) & 0xff);
	ret += stk_sensor_inb(dev, REG_HREF, &v);
	v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
	msleep(10);
	ret += stk_sensor_outb(dev, REG_HREF, v);
	ret += stk_sensor_outb(dev, REG_VSTART, (vstart >> 3) & 0xff);
	ret += stk_sensor_outb(dev, REG_VSTOP, (vstop >> 3) & 0xff);
	ret += stk_sensor_inb(dev, REG_VREF, &v);
	v = (v & 0xc0) | ((vstop & 0x7) << 3) | (vstart & 0x7);
	msleep(10);
	ret += stk_sensor_outb(dev, REG_VREF, v);
	return ret;
}


int stk_sensor_configure(struct stk_camera *dev)
{
	int com7;
	unsigned dummylines;
	int flip;
	struct regval *rv;

	switch (dev->vsettings.mode) {
	case MODE_QCIF: com7 = COM7_FMT_QCIF;
		dummylines = 604;
		break;
	case MODE_QVGA: com7 = COM7_FMT_QVGA;
		dummylines = 267;
		break;
	case MODE_CIF: com7 = COM7_FMT_CIF;
		dummylines = 412;
		break;
	case MODE_VGA: com7 = COM7_FMT_VGA;
		dummylines = 11;
		break;
	case MODE_SXGA: com7 = COM7_FMT_SXGA;
		dummylines = 0;
		break;
	default: STK_ERROR("Unsupported mode %d\n", dev->vsettings.mode);
		return -EFAULT;
	}
	switch (dev->vsettings.palette) {
	case V4L2_PIX_FMT_UYVY:
		com7 |= COM7_YUV;
		rv = ov_fmt_uyvy;
		break;
	case V4L2_PIX_FMT_YUYV:
		com7 |= COM7_YUV;
		rv = ov_fmt_yuyv;
		break;
	case V4L2_PIX_FMT_RGB565:
		com7 |= COM7_RGB;
		rv = ov_fmt_rgbp;
		break;
	case V4L2_PIX_FMT_RGB565X:
		com7 |= COM7_RGB;
		rv = ov_fmt_rgbr;
		break;
	case V4L2_PIX_FMT_SBGGR8:
		com7 |= COM7_PBAYER;
		rv = ov_fmt_bayer;
		break;
	default: STK_ERROR("Unsupported colorspace\n");
		return -EFAULT;
	}
	stk_sensor_outb(dev, REG_COM7, com7);
	msleep(50);
	stk_sensor_write_regvals(dev, rv);
	flip = (dev->vsettings.vflip?MVFP_FLIP:0)
		| (dev->vsettings.hflip?MVFP_MIRROR:0);
	stk_sensor_outb(dev, REG_MVFP, flip);
	if (dev->vsettings.palette == V4L2_PIX_FMT_SBGGR8
			&& !dev->vsettings.vflip)
		stk_sensor_outb(dev, REG_TSLB, 0x08);
	stk_sensor_outb(dev, REG_ADVFH, dummylines >> 8);
	stk_sensor_outb(dev, REG_ADVFL, dummylines & 0xff);
	msleep(50);
	switch (dev->vsettings.mode) {
	case MODE_VGA:
		if (stk_sensor_set_hw(dev, 302, 1582, 6, 486))
			STK_ERROR("stk_sensor_set_hw failed (VGA)\n");
		break;
	case MODE_SXGA:
	case MODE_CIF:
	case MODE_QVGA:
	case MODE_QCIF:
		break;
	}
	msleep(10);
	return 0;
}

int stk_sensor_set_brightness(struct stk_camera *dev, int br)
{
	if (br < 0 || br > 0xff)
		return -EINVAL;
	stk_sensor_outb(dev, REG_AEB, max(0x00, br - 6));
	stk_sensor_outb(dev, REG_AEW, min(0xff, br + 6));
	return 0;
}

