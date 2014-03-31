/*
    Driver for ST STV0299 demodulator

    Copyright (C) 2001-2002 Convergence Integrated Media GmbH
	<ralph@convergence.de>,
	<holger@convergence.de>,
	<js@convergence.de>


    Philips SU1278/SH

    Copyright (C) 2002 by Peter Schildmann <peter.schildmann@web.de>


    LG TDQF-S001F

    Copyright (C) 2002 Felix Domke <tmbinc@elitedvb.net>
		     & Andreas Oberritter <obi@linuxtv.org>


    Support for Samsung TBMU24112IMB used on Technisat SkyStar2 rev. 2.6B

    Copyright (C) 2003 Vadim Catana <skystar@moldova.cc>:

    Support for Philips SU1278 on Technotrend hardware

    Copyright (C) 2004 Andrew de Quincey <adq_dvb@lidskialf.net>

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

#ifndef STV0299_H
#define STV0299_H

#include <linux/dvb/frontend.h>
#include "dvb_frontend.h"

#define STV0299_LOCKOUTPUT_0  0
#define STV0299_LOCKOUTPUT_1  1
#define STV0299_LOCKOUTPUT_CF 2
#define STV0299_LOCKOUTPUT_LK 3

#define STV0299_VOLT13_OP0 0
#define STV0299_VOLT13_OP1 1

struct stv0299_config
{
	
	u8 demod_address;

	const u8* inittab;

	
	u32 mclk;

	
	u8 invert:1;

	
	u8 skip_reinit:1;

	
	u8 lock_output:2;

	
	u8 volt13_op0_op1:1;

	
	u8 op0_off:1;

	
	int min_delay_ms;

	
	int (*set_symbol_rate)(struct dvb_frontend *fe, u32 srate, u32 ratio);

	
	int (*set_ts_params)(struct dvb_frontend *fe, int is_punctured);
};

#if defined(CONFIG_DVB_STV0299) || (defined(CONFIG_DVB_STV0299_MODULE) && defined(MODULE))
extern struct dvb_frontend *stv0299_attach(const struct stv0299_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *stv0299_attach(const struct stv0299_config *config,
					   struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

static inline int stv0299_writereg(struct dvb_frontend *fe, u8 reg, u8 val) {
	int r = 0;
	u8 buf[] = {reg, val};
	if (fe->ops.write)
		r = fe->ops.write(fe, buf, 2);
	return r;
}

#endif 
