/*
 * Sony CXD2820R demodulator driver
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
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef CXD2820R_H
#define CXD2820R_H

#include <linux/dvb/frontend.h>

#define CXD2820R_GPIO_D (0 << 0) 
#define CXD2820R_GPIO_E (1 << 0) 
#define CXD2820R_GPIO_O (0 << 1) 
#define CXD2820R_GPIO_I (1 << 1) 
#define CXD2820R_GPIO_L (0 << 2) 
#define CXD2820R_GPIO_H (1 << 2) 

#define CXD2820R_TS_SERIAL        0x08
#define CXD2820R_TS_SERIAL_MSB    0x28
#define CXD2820R_TS_PARALLEL      0x30
#define CXD2820R_TS_PARALLEL_MSB  0x70

struct cxd2820r_config {
	u8 i2c_address;

	u8 ts_mode;

	bool if_agc_polarity;

	bool spec_inv;

	u8 gpio_dvbt[3];
	u8 gpio_dvbt2[3];
	u8 gpio_dvbc[3];
};


#if defined(CONFIG_DVB_CXD2820R) || \
	(defined(CONFIG_DVB_CXD2820R_MODULE) && defined(MODULE))
extern struct dvb_frontend *cxd2820r_attach(
	const struct cxd2820r_config *config,
	struct i2c_adapter *i2c
);
#else
static inline struct dvb_frontend *cxd2820r_attach(
	const struct cxd2820r_config *config,
	struct i2c_adapter *i2c
)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}

#endif

#endif 
