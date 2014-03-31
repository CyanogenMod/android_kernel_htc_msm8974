  /*
     Driver for Philips tda1004xh OFDM Frontend

     (c) 2004 Andrew de Quincey

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

#ifndef TDA1004X_H
#define TDA1004X_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

enum tda10046_xtal {
	TDA10046_XTAL_4M,
	TDA10046_XTAL_16M,
};

enum tda10046_agc {
	TDA10046_AGC_DEFAULT,		
	TDA10046_AGC_IFO_AUTO_NEG,	
	TDA10046_AGC_IFO_AUTO_POS,	
	TDA10046_AGC_TDA827X,		
};

enum tda10046_gpio {
	TDA10046_GPTRI  = 0x00,		
	TDA10046_GP00   = 0x40,		
	TDA10046_GP01   = 0x42,		
	TDA10046_GP10   = 0x48,		
	TDA10046_GP11   = 0x4a,		
	TDA10046_GP00_I = 0x80,		
	TDA10046_GP01_I = 0x82,		
	TDA10046_GP10_I = 0x88,		
	TDA10046_GP11_I = 0x8a,		
};

enum tda10046_if {
	TDA10046_FREQ_3617,		
	TDA10046_FREQ_3613,		
	TDA10046_FREQ_045,		
	TDA10046_FREQ_052,		
};

enum tda10046_tsout {
	TDA10046_TS_PARALLEL  = 0x00,	
	TDA10046_TS_SERIAL    = 0x01,	
};

struct tda1004x_config
{
	
	u8 demod_address;

	
	u8 invert;

	
	u8 invert_oclk;

	
	enum tda10046_tsout ts_mode;

	
	enum tda10046_xtal xtal_freq;

	
	enum tda10046_if if_freq;

	
	enum tda10046_agc agc_config;

	
	enum tda10046_gpio gpio_config;

	
	u8 tuner_address;
	u8 antenna_switch;

	
	u8 i2c_gate;

	
	int (*request_firmware)(struct dvb_frontend* fe, const struct firmware **fw, char* name);
};

enum tda1004x_demod {
	TDA1004X_DEMOD_TDA10045,
	TDA1004X_DEMOD_TDA10046,
};

struct tda1004x_state {
	struct i2c_adapter* i2c;
	const struct tda1004x_config* config;
	struct dvb_frontend frontend;

	
	enum tda1004x_demod demod_type;
};

#if defined(CONFIG_DVB_TDA1004X) || (defined(CONFIG_DVB_TDA1004X_MODULE) && defined(MODULE))
extern struct dvb_frontend* tda10045_attach(const struct tda1004x_config* config,
					    struct i2c_adapter* i2c);

extern struct dvb_frontend* tda10046_attach(const struct tda1004x_config* config,
					    struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* tda10045_attach(const struct tda1004x_config* config,
					    struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct dvb_frontend* tda10046_attach(const struct tda1004x_config* config,
					    struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

static inline int tda1004x_writereg(struct dvb_frontend *fe, u8 reg, u8 val) {
	int r = 0;
	u8 buf[] = {reg, val};
	if (fe->ops.write)
		r = fe->ops.write(fe, buf, 2);
	return r;
}

#endif 
