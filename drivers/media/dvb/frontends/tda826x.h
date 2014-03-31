  /*
     Driver for Philips tda8262/tda8263 DVBS Silicon tuners

     (c) 2006 Andrew de Quincey

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  */

#ifndef __DVB_TDA826X_H__
#define __DVB_TDA826X_H__

#include <linux/i2c.h>
#include "dvb_frontend.h"

#if defined(CONFIG_DVB_TDA826X) || (defined(CONFIG_DVB_TDA826X_MODULE) && defined(MODULE))
extern struct dvb_frontend* tda826x_attach(struct dvb_frontend *fe, int addr,
					   struct i2c_adapter *i2c,
					   int has_loopthrough);
#else
static inline struct dvb_frontend* tda826x_attach(struct dvb_frontend *fe,
						  int addr,
						  struct i2c_adapter *i2c,
						  int has_loopthrough)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
