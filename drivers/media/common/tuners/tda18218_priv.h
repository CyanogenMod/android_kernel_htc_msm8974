/*
 * NXP TDA18218HN silicon tuner driver
 *
 * Copyright (C) 2010 Antti Palosaari <crope@iki.fi>
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
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TDA18218_PRIV_H
#define TDA18218_PRIV_H

#define LOG_PREFIX "tda18218"

#undef dbg
#define dbg(f, arg...) \
	if (debug) \
		printk(KERN_DEBUG   LOG_PREFIX": " f "\n" , ## arg)
#undef err
#define err(f, arg...)  printk(KERN_ERR     LOG_PREFIX": " f "\n" , ## arg)
#undef info
#define info(f, arg...) printk(KERN_INFO    LOG_PREFIX": " f "\n" , ## arg)
#undef warn
#define warn(f, arg...) printk(KERN_WARNING LOG_PREFIX": " f "\n" , ## arg)

#define R00_ID         0x00	
#define R01_R1         0x01	
#define R02_R2         0x02	
#define R03_R3         0x03	
#define R04_R4         0x04	
#define R05_R5         0x05	
#define R06_R6         0x06	
#define R07_MD1        0x07	
#define R08_PSM1       0x08	
#define R09_MD2        0x09	
#define R0A_MD3        0x0a	
#define R0B_MD4        0x0b	
#define R0C_MD5        0x0c	
#define R0D_MD6        0x0d	
#define R0E_MD7        0x0e	
#define R0F_MD8        0x0f	
#define R10_CD1        0x10	
#define R11_CD2        0x11	
#define R12_CD3        0x12	
#define R13_CD4        0x13	
#define R14_CD5        0x14	
#define R15_CD6        0x15	
#define R16_CD7        0x16	
#define R17_PD1        0x17	
#define R18_PD2        0x18	
#define R19_XTOUT      0x19	
#define R1A_IF1        0x1a	
#define R1B_IF2        0x1b	
#define R1C_AGC2B      0x1c	
#define R1D_PSM2       0x1d	
#define R1E_PSM3       0x1e	
#define R1F_PSM4       0x1f	
#define R20_AGC11      0x20	
#define R21_AGC12      0x21	
#define R22_AGC13      0x22	
#define R23_AGC21      0x23	
#define R24_AGC22      0x24	
#define R25_AAGC       0x25	
#define R26_RC         0x26	
#define R27_RSSI       0x27	
#define R28_IRCAL1     0x28	
#define R29_IRCAL2     0x29	
#define R2A_IRCAL3     0x2a	
#define R2B_IRCAL4     0x2b	
#define R2C_RFCAL1     0x2c	
#define R2D_RFCAL2     0x2d	
#define R2E_RFCAL3     0x2e	
#define R2F_RFCAL4     0x2f	
#define R30_RFCAL5     0x30	
#define R31_RFCAL6     0x31	
#define R32_RFCAL7     0x32	
#define R33_RFCAL8     0x33	
#define R34_RFCAL9     0x34	
#define R35_RFCAL10    0x35	
#define R36_RFCALRAM1  0x36	
#define R37_RFCALRAM2  0x37	
#define R38_MARGIN     0x38	
#define R39_FMAX1      0x39	
#define R3A_FMAX2      0x3a	

#define TDA18218_NUM_REGS 59

struct tda18218_priv {
	struct tda18218_config *cfg;
	struct i2c_adapter *i2c;

	u32 if_frequency;

	u8 regs[TDA18218_NUM_REGS];
};

#endif
