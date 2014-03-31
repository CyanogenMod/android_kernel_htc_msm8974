/*
 * Realtek RTL28xxU DVB USB driver
 *
 * Copyright (C) 2009 Antti Palosaari <crope@iki.fi>
 * Copyright (C) 2011 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef RTL28XXU_H
#define RTL28XXU_H

#define DVB_USB_LOG_PREFIX "rtl28xxu"
#include "dvb-usb.h"

#define deb_info(args...) dprintk(dvb_usb_rtl28xxu_debug, 0x01, args)
#define deb_rc(args...)   dprintk(dvb_usb_rtl28xxu_debug, 0x02, args)
#define deb_xfer(args...) dprintk(dvb_usb_rtl28xxu_debug, 0x04, args)
#define deb_reg(args...)  dprintk(dvb_usb_rtl28xxu_debug, 0x08, args)
#define deb_i2c(args...)  dprintk(dvb_usb_rtl28xxu_debug, 0x10, args)
#define deb_fw(args...)   dprintk(dvb_usb_rtl28xxu_debug, 0x20, args)

#define deb_dump(r, t, v, i, b, l, func) { \
	int loop_; \
	func("%02x %02x %02x %02x %02x %02x %02x %02x", \
		t, r, v & 0xff, v >> 8, i & 0xff, i >> 8, l & 0xff, l >> 8); \
	if (t == (USB_TYPE_VENDOR | USB_DIR_OUT)) \
		func(" >>> "); \
	else \
		func(" <<< "); \
	for (loop_ = 0; loop_ < l; loop_++) \
		func("%02x ", b[loop_]); \
	func("\n");\
}


#define DEMOD            0x0000
#define USB              0x0100
#define SYS              0x0200
#define I2C              0x0300
#define I2C_DA           0x0600

#define CMD_WR_FLAG      0x0010
#define CMD_DEMOD_RD     0x0000
#define CMD_DEMOD_WR     0x0010
#define CMD_USB_RD       0x0100
#define CMD_USB_WR       0x0110
#define CMD_SYS_RD       0x0200
#define CMD_IR_RD        0x0201
#define CMD_IR_WR        0x0211
#define CMD_SYS_WR       0x0210
#define CMD_I2C_RD       0x0300
#define CMD_I2C_WR       0x0310
#define CMD_I2C_DA_RD    0x0600
#define CMD_I2C_DA_WR    0x0610


struct rtl28xxu_priv {
	u8 chip_id;
	u8 tuner;
	u8 page; 
	bool rc_active;
};

enum rtl28xxu_chip_id {
	CHIP_ID_NONE,
	CHIP_ID_RTL2831U,
	CHIP_ID_RTL2832U,
};

enum rtl28xxu_tuner {
	TUNER_NONE,

	TUNER_RTL2830_QT1010,
	TUNER_RTL2830_MT2060,
	TUNER_RTL2830_MXL5005S,

	TUNER_RTL2832_MT2266,
	TUNER_RTL2832_FC2580,
	TUNER_RTL2832_MT2063,
	TUNER_RTL2832_MAX3543,
	TUNER_RTL2832_TUA9001,
	TUNER_RTL2832_MXL5007T,
	TUNER_RTL2832_FC0012,
	TUNER_RTL2832_E4000,
	TUNER_RTL2832_TDA18272,
	TUNER_RTL2832_FC0013,
};

struct rtl28xxu_req {
	u16 value;
	u16 index;
	u16 size;
	u8 *data;
};

struct rtl28xxu_reg_val {
	u16 reg;
	u8 val;
};


#define USB_SYSCTL         0x2000 
#define USB_SYSCTL_0       0x2000 
#define USB_SYSCTL_1       0x2001 
#define USB_SYSCTL_2       0x2002 
#define USB_SYSCTL_3       0x2003 
#define USB_IRQSTAT        0x2008 
#define USB_IRQEN          0x200C 
#define USB_CTRL           0x2010 
#define USB_STAT           0x2014 
#define USB_DEVADDR        0x2018 
#define USB_TEST           0x201C 
#define USB_FRAME_NUMBER   0x2020 
#define USB_FIFO_ADDR      0x2028 
#define USB_FIFO_CMD       0x202A 
#define USB_FIFO_DATA      0x2030 
#define EP0_SETUPA         0x20F8 
#define EP0_SETUPB         0x20FC 
#define USB_EP0_CFG        0x2104 
#define USB_EP0_CTL        0x2108 
#define USB_EP0_STAT       0x210C 
#define USB_EP0_IRQSTAT    0x2110 
#define USB_EP0_IRQEN      0x2114 
#define USB_EP0_MAXPKT     0x2118 
#define USB_EP0_BC         0x2120 
#define USB_EPA_CFG        0x2144 
#define USB_EPA_CFG_0      0x2144 
#define USB_EPA_CFG_1      0x2145 
#define USB_EPA_CFG_2      0x2146 
#define USB_EPA_CFG_3      0x2147 
#define USB_EPA_CTL        0x2148 
#define USB_EPA_CTL_0      0x2148 
#define USB_EPA_CTL_1      0x2149 
#define USB_EPA_CTL_2      0x214A 
#define USB_EPA_CTL_3      0x214B 
#define USB_EPA_STAT       0x214C 
#define USB_EPA_IRQSTAT    0x2150 
#define USB_EPA_IRQEN      0x2154 
#define USB_EPA_MAXPKT     0x2158 
#define USB_EPA_MAXPKT_0   0x2158 
#define USB_EPA_MAXPKT_1   0x2159 
#define USB_EPA_MAXPKT_2   0x215A 
#define USB_EPA_MAXPKT_3   0x215B 
#define USB_EPA_FIFO_CFG   0x2160 
#define USB_EPA_FIFO_CFG_0 0x2160 
#define USB_EPA_FIFO_CFG_1 0x2161 
#define USB_EPA_FIFO_CFG_2 0x2162 
#define USB_EPA_FIFO_CFG_3 0x2163 
#define USB_PHYTSTDIS      0x2F04 
#define USB_TOUT_VAL       0x2F08 
#define USB_VDRCTRL        0x2F10 
#define USB_VSTAIN         0x2F14 
#define USB_VLOADM         0x2F18 
#define USB_VSTAOUT        0x2F1C 
#define USB_UTMI_TST       0x2F80 
#define USB_UTMI_STATUS    0x2F84 
#define USB_TSTCTL         0x2F88 
#define USB_TSTCTL2        0x2F8C 
#define USB_PID_FORCE      0x2F90 
#define USB_PKTERR_CNT     0x2F94 
#define USB_RXERR_CNT      0x2F98 
#define USB_MEM_BIST       0x2F9C 
#define USB_SLBBIST        0x2FA0 
#define USB_CNTTEST        0x2FA4 
#define USB_PHYTST         0x2FC0 
#define USB_DBGIDX         0x2FF0 
#define USB_DBGMUX         0x2FF4 

#define SYS_SYS0           0x3000 
#define SYS_DEMOD_CTL      0x3000 
#define SYS_GPIO_OUT_VAL   0x3001 
#define SYS_GPIO_IN_VAL    0x3002 
#define SYS_GPIO_OUT_EN    0x3003 
#define SYS_SYS1           0x3004 
#define SYS_GPIO_DIR       0x3004 
#define SYS_SYSINTE        0x3005 
#define SYS_SYSINTS        0x3006 
#define SYS_GPIO_CFG0      0x3007 
#define SYS_SYS2           0x3008 
#define SYS_GPIO_CFG1      0x3008 
#define SYS_DEMOD_CTL1     0x300B

#define SYS_IRRC_PSR       0x3020 
#define SYS_IRRC_PER       0x3024 
#define SYS_IRRC_SF        0x3028 
#define SYS_IRRC_DPIR      0x302C 
#define SYS_IRRC_CR        0x3030 
#define SYS_IRRC_RP        0x3034 
#define SYS_IRRC_SR        0x3038 
#define SYS_I2CCR          0x3040 
#define SYS_I2CMCR         0x3044 
#define SYS_I2CMSTR        0x3048 
#define SYS_I2CMSR         0x304C 
#define SYS_I2CMFR         0x3050 

#define IR_RX_BUF          0xFC00
#define IR_RX_IE           0xFD00
#define IR_RX_IF           0xFD01
#define IR_RX_CTRL         0xFD02
#define IR_RX_CFG          0xFD03
#define IR_MAX_DURATION0   0xFD04
#define IR_MAX_DURATION1   0xFD05
#define IR_IDLE_LEN0       0xFD06
#define IR_IDLE_LEN1       0xFD07
#define IR_GLITCH_LEN      0xFD08
#define IR_RX_BUF_CTRL     0xFD09
#define IR_RX_BUF_DATA     0xFD0A
#define IR_RX_BC           0xFD0B
#define IR_RX_CLK          0xFD0C
#define IR_RX_C_COUNT_L    0xFD0D
#define IR_RX_C_COUNT_H    0xFD0E
#define IR_SUSPEND_CTRL    0xFD10
#define IR_ERR_TOL_CTRL    0xFD11
#define IR_UNIT_LEN        0xFD12
#define IR_ERR_TOL_LEN     0xFD13
#define IR_MAX_H_TOL_LEN   0xFD14
#define IR_MAX_L_TOL_LEN   0xFD15
#define IR_MASK_CTRL       0xFD16
#define IR_MASK_DATA       0xFD17
#define IR_RES_MASK_ADDR   0xFD18
#define IR_RES_MASK_T_LEN  0xFD19

#endif
