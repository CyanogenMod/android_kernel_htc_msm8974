/*
 * Copyright (C) 2005-2006 Micronas USA Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#define	I2C_DRIVERID_WIS_SAA7115	0xf0f0
#define	I2C_DRIVERID_WIS_UDA1342	0xf0f1
#define	I2C_DRIVERID_WIS_SONY_TUNER	0xf0f2
#define	I2C_DRIVERID_WIS_TW9903		0xf0f3
#define	I2C_DRIVERID_WIS_SAA7113	0xf0f4
#define	I2C_DRIVERID_WIS_OV7640		0xf0f5
#define	I2C_DRIVERID_WIS_TW2804		0xf0f6
#define	I2C_DRIVERID_S2250		0xf0f7

#define	I2C_CLIENT_SCCB			0x10


struct video_decoder_resolution {
	unsigned int width;
	unsigned int height;
};

#define	DECODER_SET_RESOLUTION	_IOW('d', 200, struct video_decoder_resolution)
#define	DECODER_SET_CHANNEL	_IOW('d', 201, int)


#define TUNER_SONY_BTF_PG472Z		200
#define TUNER_SONY_BTF_PK467Z		201
#define TUNER_SONY_BTF_PB463Z		202
