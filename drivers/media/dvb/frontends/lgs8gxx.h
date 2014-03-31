/*
 *    Support for Legend Silicon GB20600 (a.k.a DMB-TH) demodulator
 *    LGS8913, LGS8GL5, LGS8G75
 *    experimental support LGS8G42, LGS8G52
 *
 *    Copyright (C) 2007-2009 David T.L. Wong <davidtlwong@gmail.com>
 *    Copyright (C) 2008 Sirius International (Hong Kong) Limited
 *    Timothy Lee <timothy.lee@siriushk.com> (for initial work on LGS8GL5)
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
 *
 */

#ifndef __LGS8GXX_H__
#define __LGS8GXX_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

#define LGS8GXX_PROD_LGS8913 0
#define LGS8GXX_PROD_LGS8GL5 1
#define LGS8GXX_PROD_LGS8G42 3
#define LGS8GXX_PROD_LGS8G52 4
#define LGS8GXX_PROD_LGS8G54 5
#define LGS8GXX_PROD_LGS8G75 6

struct lgs8gxx_config {

	
	u8 prod;

	
	u8 demod_address;

	
	u8 serial_ts;

	
	u8 ts_clk_pol;

	
	u8 ts_clk_gated;

	
	u32 if_clk_freq; 

	
	u32 if_freq; 

	
	u8 ext_adc;

	
	u8 adc_signed;

	
	u8 if_neg_edge;

	
	u8 if_neg_center;

	
	
	u8 adc_vpp;

	
	u8 tuner_address;
};

#if defined(CONFIG_DVB_LGS8GXX) || \
	(defined(CONFIG_DVB_LGS8GXX_MODULE) && defined(MODULE))
extern struct dvb_frontend *lgs8gxx_attach(const struct lgs8gxx_config *config,
					   struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *lgs8gxx_attach(const struct lgs8gxx_config *config,
				    struct i2c_adapter *i2c) {
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
