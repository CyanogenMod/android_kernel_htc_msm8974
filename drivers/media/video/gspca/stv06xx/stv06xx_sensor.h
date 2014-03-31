/*
 * Copyright (c) 2001 Jean-Fredric Clere, Nikolas Zimmermann, Georg Acher
 *		      Mark Cave-Ayland, Carlo E Prelz, Dick Streefland
 * Copyright (c) 2002, 2003 Tuukka Toivonen
 * Copyright (c) 2008 Erik Andrén
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * P/N 861037:      Sensor HDCS1000        ASIC STV0600
 * P/N 861050-0010: Sensor HDCS1000        ASIC STV0600
 * P/N 861050-0020: Sensor Photobit PB100  ASIC STV0600-1 - QuickCam Express
 * P/N 861055:      Sensor ST VV6410       ASIC STV0610   - LEGO cam
 * P/N 861075-0040: Sensor HDCS1000        ASIC
 * P/N 961179-0700: Sensor ST VV6410       ASIC STV0602   - Dexxa WebCam USB
 * P/N 861040-0000: Sensor ST VV6410       ASIC STV0610   - QuickCam Web
 */

#ifndef STV06XX_SENSOR_H_
#define STV06XX_SENSOR_H_

#include "stv06xx.h"

#define IS_1020(sd)	((sd)->sensor == &stv06xx_sensor_hdcs1020)

extern const struct stv06xx_sensor stv06xx_sensor_vv6410;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1x00;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1020;
extern const struct stv06xx_sensor stv06xx_sensor_pb0100;
extern const struct stv06xx_sensor stv06xx_sensor_st6422;

struct stv06xx_sensor {
	
	char name[32];

	
	u8 i2c_addr;

	
	u8 i2c_flush;

	
	u8 i2c_len;

	
	int min_packet_size[4];
	int max_packet_size[4];

	
	int (*probe)(struct sd *sd);

	
	int (*init)(struct sd *sd);

	
	void (*disconnect)(struct sd *sd);

	
	int (*read_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);

	
	int (*write_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);

	
	int (*start)(struct sd *sd);

	
	int (*stop)(struct sd *sd);

	
	int (*dump)(struct sd *sd);
};

#endif
