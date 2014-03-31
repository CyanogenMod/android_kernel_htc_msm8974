/*
 *  Driver for Quantek QT1010 silicon tuner
 *
 *  Copyright (C) 2006 Antti Palosaari <crope@iki.fi>
 *                     Aapo Tahkola <aet@rasterburn.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef QT1010_PRIV_H
#define QT1010_PRIV_H


#define QT1010_STEP         125000 
#define QT1010_MIN_FREQ   48000000 
#define QT1010_MAX_FREQ  860000000 
#define QT1010_OFFSET   1246000000 

#define QT1010_WR 0
#define QT1010_RD 1
#define QT1010_M1 3

typedef struct {
	u8 oper, reg, val;
} qt1010_i2c_oper_t;

struct qt1010_priv {
	struct qt1010_config *cfg;
	struct i2c_adapter   *i2c;

	u8 reg1f_init_val;
	u8 reg20_init_val;
	u8 reg25_init_val;

	u32 frequency;
};

#endif
