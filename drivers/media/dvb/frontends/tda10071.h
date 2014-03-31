/*
 * NXP TDA10071 + Conexant CX24118A DVB-S/S2 demodulator + tuner driver
 *
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

#ifndef TDA10071_H
#define TDA10071_H

#include <linux/dvb/frontend.h>

struct tda10071_config {
	u8 i2c_address;

	u16 i2c_wr_max;

#define TDA10071_TS_SERIAL        0
#define TDA10071_TS_PARALLEL      1
	u8 ts_mode;

	bool spec_inv;

	u32 xtal;

	u8 pll_multiplier;
};


#if defined(CONFIG_DVB_TDA10071) || \
	(defined(CONFIG_DVB_TDA10071_MODULE) && defined(MODULE))
extern struct dvb_frontend *tda10071_attach(
	const struct tda10071_config *config, struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *tda10071_attach(
	const struct tda10071_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif 
