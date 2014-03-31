/*
 *  Driver for Microtune MT2060 "Single chip dual conversion broadband tuner"
 *
 *  Copyright (c) 2006 Olivier DANET <odanet@caramail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.=
 */

#ifndef MT2060_PRIV_H
#define MT2060_PRIV_H



#define I2C_ADDRESS 0x60

#define REG_PART_REV   0
#define REG_LO1C1      1
#define REG_LO1C2      2
#define REG_LO2C1      3
#define REG_LO2C2      4
#define REG_LO2C3      5
#define REG_LO_STATUS  6
#define REG_FM_FREQ    7
#define REG_MISC_STAT  8
#define REG_MISC_CTRL  9
#define REG_RESERVED_A 0x0A
#define REG_VGAG       0x0B
#define REG_LO1B1      0x0C
#define REG_LO1B2      0x0D
#define REG_LOTO       0x11

#define PART_REV 0x63 

struct mt2060_priv {
	struct mt2060_config *cfg;
	struct i2c_adapter   *i2c;

	u32 frequency;
	u16 if1_freq;
	u8  fmfreq;
};

#endif
