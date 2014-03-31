/**
 * Driver for Sharp IX2505V (marked B0017) DVB-S silicon tuner
 *
 * Copyright (C) 2010 Malcolm Priestley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef DVB_IX2505V_H
#define DVB_IX2505V_H

#include <linux/i2c.h>
#include "dvb_frontend.h"


struct ix2505v_config {
	u8 tuner_address;

	
	u8 tuner_gain;

	
	u8 tuner_chargepump;

	
	int min_delay_ms;

	
	u8 tuner_write_only;

};

#if defined(CONFIG_DVB_IX2505V) || \
	(defined(CONFIG_DVB_IX2505V_MODULE) && defined(MODULE))
extern struct dvb_frontend *ix2505v_attach(struct dvb_frontend *fe,
	const struct ix2505v_config *config, struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *ix2505v_attach(struct dvb_frontend *fe,
	const struct ix2505v_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif 
