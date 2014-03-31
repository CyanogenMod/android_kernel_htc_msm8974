/*
    Samsung S5H1409 VSB/QAM demodulator driver

    Copyright (C) 2006 Steven Toth <stoth@linuxtv.org>

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

#ifndef __S5H1409_H__
#define __S5H1409_H__

#include <linux/dvb/frontend.h>

struct s5h1409_config {
	
	u8 demod_address;

	
#define S5H1409_PARALLEL_OUTPUT 0
#define S5H1409_SERIAL_OUTPUT   1
	u8 output_mode;

	
#define S5H1409_GPIO_OFF 0
#define S5H1409_GPIO_ON  1
	u8 gpio;

	
	u16 qam_if;

	
#define S5H1409_INVERSION_OFF 0
#define S5H1409_INVERSION_ON  1
	u8 inversion;

	
#define S5H1409_TUNERLOCKING 0
#define S5H1409_DEMODLOCKING 1
	u8 status_mode;

	
#define S5H1409_MPEGTIMING_CONTINOUS_INVERTING_CLOCK       0
#define S5H1409_MPEGTIMING_CONTINOUS_NONINVERTING_CLOCK    1
#define S5H1409_MPEGTIMING_NONCONTINOUS_INVERTING_CLOCK    2
#define S5H1409_MPEGTIMING_NONCONTINOUS_NONINVERTING_CLOCK 3
	u16 mpeg_timing;

#define S5H1409_HVR1600_NOOPTIMIZE 0
#define S5H1409_HVR1600_OPTIMIZE   1
	u8 hvr1600_opt;
};

#if defined(CONFIG_DVB_S5H1409) || (defined(CONFIG_DVB_S5H1409_MODULE) \
	&& defined(MODULE))
extern struct dvb_frontend *s5h1409_attach(const struct s5h1409_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *s5h1409_attach(
	const struct s5h1409_config *config,
	struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 

