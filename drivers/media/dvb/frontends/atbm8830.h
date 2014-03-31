/*
 *    Support for AltoBeam GB20600 (a.k.a DMB-TH) demodulator
 *    ATBM8830, ATBM8831
 *
 *    Copyright (C) 2009 David T.L. Wong <davidtlwong@gmail.com>
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

#ifndef __ATBM8830_H__
#define __ATBM8830_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

#define ATBM8830_PROD_8830 0
#define ATBM8830_PROD_8831 1

struct atbm8830_config {

	
	u8 prod;

	
	u8 demod_address;

	
	u8 serial_ts;

	
	u8 ts_clk_gated;

	
	u8 ts_sampling_edge;

	
	u32 osc_clk_freq; 

	
	u32 if_freq; 

	
	u8 zif_swap_iq;

	
	u8 agc_min;
	u8 agc_max;
	u8 agc_hold_loop;
};

#if defined(CONFIG_DVB_ATBM8830) || \
	(defined(CONFIG_DVB_ATBM8830_MODULE) && defined(MODULE))
extern struct dvb_frontend *atbm8830_attach(const struct atbm8830_config *config,
		struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *atbm8830_attach(const struct atbm8830_config *config,
		struct i2c_adapter *i2c) {
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
