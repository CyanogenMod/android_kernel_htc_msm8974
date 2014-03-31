/*
    driver for LSI L64781 COFDM demodulator

    Copyright (C) 2001 Holger Waechtler for Convergence Integrated Media GmbH
		       Marko Kohtala <marko.kohtala@luukku.com>

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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "dvb_frontend.h"
#include "l64781.h"


struct l64781_state {
	struct i2c_adapter* i2c;
	const struct l64781_config* config;
	struct dvb_frontend frontend;

	
	unsigned int first:1;
};

#define dprintk(args...) \
	do { \
		if (debug) printk(KERN_DEBUG "l64781: " args); \
	} while (0)

static int debug;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");


static int l64781_writereg (struct l64781_state* state, u8 reg, u8 data)
{
	int ret;
	u8 buf [] = { reg, data };
	struct i2c_msg msg = { .addr = state->config->demod_address, .flags = 0, .buf = buf, .len = 2 };

	if ((ret = i2c_transfer(state->i2c, &msg, 1)) != 1)
		dprintk ("%s: write_reg error (reg == %02x) = %02x!\n",
			 __func__, reg, ret);

	return (ret != 1) ? -1 : 0;
}

static int l64781_readreg (struct l64781_state* state, u8 reg)
{
	int ret;
	u8 b0 [] = { reg };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { .addr = state->config->demod_address, .flags = 0, .buf = b0, .len = 1 },
			   { .addr = state->config->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	ret = i2c_transfer(state->i2c, msg, 2);

	if (ret != 2) return ret;

	return b1[0];
}

static void apply_tps (struct l64781_state* state)
{
	l64781_writereg (state, 0x2a, 0x00);
	l64781_writereg (state, 0x2a, 0x01);

	l64781_writereg (state, 0x2a, 0x02);
}


static void reset_afc (struct l64781_state* state)
{
	l64781_writereg (state, 0x07, 0x9e); 
	l64781_writereg (state, 0x08, 0);    
	l64781_writereg (state, 0x09, 0);
	l64781_writereg (state, 0x0a, 0);
	l64781_writereg (state, 0x07, 0x8e);
	l64781_writereg (state, 0x0e, 0);    
	l64781_writereg (state, 0x11, 0x80); 
	l64781_writereg (state, 0x10, 0);    
	l64781_writereg (state, 0x12, 0);
	l64781_writereg (state, 0x13, 0);
	l64781_writereg (state, 0x11, 0x00);
}

static int reset_and_configure (struct l64781_state* state)
{
	u8 buf [] = { 0x06 };
	struct i2c_msg msg = { .addr = 0x00, .flags = 0, .buf = buf, .len = 1 };
	

	return (i2c_transfer(state->i2c, &msg, 1) == 1) ? 0 : -ENODEV;
}

static int apply_frontend_param(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct l64781_state* state = fe->demodulator_priv;
	
	static const u8 fec_tab[] = { 7, 0, 1, 2, 9, 3, 10, 4 };
	
	static const u8 qam_tab [] = { 2, 4, 0, 6 };
	static const u8 guard_tab [] = { 1, 2, 4, 8 };
	
	static const u32 ppm = 8000;
	u32 ddfs_offset_fixed;
	u32 init_freq;
	u32 spi_bias;
	u8 val0x04;
	u8 val0x05;
	u8 val0x06;
	int bw;

	switch (p->bandwidth_hz) {
	case 8000000:
		bw = 8;
		break;
	case 7000000:
		bw = 7;
		break;
	case 6000000:
		bw = 6;
		break;
	default:
		return -EINVAL;
	}

	if (fe->ops.tuner_ops.set_params) {
		fe->ops.tuner_ops.set_params(fe);
		if (fe->ops.i2c_gate_ctrl) fe->ops.i2c_gate_ctrl(fe, 0);
	}

	if (p->inversion != INVERSION_ON &&
	    p->inversion != INVERSION_OFF)
		return -EINVAL;

	if (p->code_rate_HP != FEC_1_2 && p->code_rate_HP != FEC_2_3 &&
	    p->code_rate_HP != FEC_3_4 && p->code_rate_HP != FEC_5_6 &&
	    p->code_rate_HP != FEC_7_8)
		return -EINVAL;

	if (p->hierarchy != HIERARCHY_NONE &&
	    (p->code_rate_LP != FEC_1_2 && p->code_rate_LP != FEC_2_3 &&
	     p->code_rate_LP != FEC_3_4 && p->code_rate_LP != FEC_5_6 &&
	     p->code_rate_LP != FEC_7_8))
		return -EINVAL;

	if (p->modulation != QPSK && p->modulation != QAM_16 &&
	    p->modulation != QAM_64)
		return -EINVAL;

	if (p->transmission_mode != TRANSMISSION_MODE_2K &&
	    p->transmission_mode != TRANSMISSION_MODE_8K)
		return -EINVAL;

	if (p->guard_interval < GUARD_INTERVAL_1_32 ||
	    p->guard_interval > GUARD_INTERVAL_1_4)
		return -EINVAL;

	if (p->hierarchy < HIERARCHY_NONE ||
	    p->hierarchy > HIERARCHY_4)
		return -EINVAL;

	ddfs_offset_fixed = 0x4000-(ppm<<16)/bw/1000000;

	
	init_freq = (((8UL<<25) + (8UL<<19) / 25*ppm / (15625/25)) /
			bw & 0xFFFFFF);

	
	
	spi_bias = 378 * (1 << 10);
	spi_bias *= 16;
	spi_bias *= bw;
	spi_bias *= qam_tab[p->modulation];
	spi_bias /= p->code_rate_HP + 1;
	spi_bias /= (guard_tab[p->guard_interval] + 32);
	spi_bias *= 1000;
	spi_bias /= 1000 + ppm/1000;
	spi_bias *= p->code_rate_HP;

	val0x04 = (p->transmission_mode << 2) | p->guard_interval;
	val0x05 = fec_tab[p->code_rate_HP];

	if (p->hierarchy != HIERARCHY_NONE)
		val0x05 |= (p->code_rate_LP - FEC_1_2) << 3;

	val0x06 = (p->hierarchy << 2) | p->modulation;

	l64781_writereg (state, 0x04, val0x04);
	l64781_writereg (state, 0x05, val0x05);
	l64781_writereg (state, 0x06, val0x06);

	reset_afc (state);

	
	l64781_writereg (state, 0x15,
			 p->transmission_mode == TRANSMISSION_MODE_2K ? 1 : 3);
	l64781_writereg (state, 0x16, init_freq & 0xff);
	l64781_writereg (state, 0x17, (init_freq >> 8) & 0xff);
	l64781_writereg (state, 0x18, (init_freq >> 16) & 0xff);

	l64781_writereg (state, 0x1b, spi_bias & 0xff);
	l64781_writereg (state, 0x1c, (spi_bias >> 8) & 0xff);
	l64781_writereg (state, 0x1d, ((spi_bias >> 16) & 0x7f) |
		(p->inversion == INVERSION_ON ? 0x80 : 0x00));

	l64781_writereg (state, 0x22, ddfs_offset_fixed & 0xff);
	l64781_writereg (state, 0x23, (ddfs_offset_fixed >> 8) & 0x3f);

	l64781_readreg (state, 0x00);  
	l64781_readreg (state, 0x01);  

	apply_tps (state);

	return 0;
}

static int get_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct l64781_state* state = fe->demodulator_priv;
	int tmp;


	tmp = l64781_readreg(state, 0x04);
	switch(tmp & 3) {
	case 0:
		p->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		p->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		p->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		p->guard_interval = GUARD_INTERVAL_1_4;
		break;
	}
	switch((tmp >> 2) & 3) {
	case 0:
		p->transmission_mode = TRANSMISSION_MODE_2K;
		break;
	case 1:
		p->transmission_mode = TRANSMISSION_MODE_8K;
		break;
	default:
		printk(KERN_WARNING "Unexpected value for transmission_mode\n");
	}

	tmp = l64781_readreg(state, 0x05);
	switch(tmp & 7) {
	case 0:
		p->code_rate_HP = FEC_1_2;
		break;
	case 1:
		p->code_rate_HP = FEC_2_3;
		break;
	case 2:
		p->code_rate_HP = FEC_3_4;
		break;
	case 3:
		p->code_rate_HP = FEC_5_6;
		break;
	case 4:
		p->code_rate_HP = FEC_7_8;
		break;
	default:
		printk("Unexpected value for code_rate_HP\n");
	}
	switch((tmp >> 3) & 7) {
	case 0:
		p->code_rate_LP = FEC_1_2;
		break;
	case 1:
		p->code_rate_LP = FEC_2_3;
		break;
	case 2:
		p->code_rate_LP = FEC_3_4;
		break;
	case 3:
		p->code_rate_LP = FEC_5_6;
		break;
	case 4:
		p->code_rate_LP = FEC_7_8;
		break;
	default:
		printk("Unexpected value for code_rate_LP\n");
	}

	tmp = l64781_readreg(state, 0x06);
	switch(tmp & 3) {
	case 0:
		p->modulation = QPSK;
		break;
	case 1:
		p->modulation = QAM_16;
		break;
	case 2:
		p->modulation = QAM_64;
		break;
	default:
		printk(KERN_WARNING "Unexpected value for modulation\n");
	}
	switch((tmp >> 2) & 7) {
	case 0:
		p->hierarchy = HIERARCHY_NONE;
		break;
	case 1:
		p->hierarchy = HIERARCHY_1;
		break;
	case 2:
		p->hierarchy = HIERARCHY_2;
		break;
	case 3:
		p->hierarchy = HIERARCHY_4;
		break;
	default:
		printk("Unexpected value for hierarchy\n");
	}


	tmp = l64781_readreg (state, 0x1d);
	p->inversion = (tmp & 0x80) ? INVERSION_ON : INVERSION_OFF;

	tmp = (int) (l64781_readreg (state, 0x08) |
		     (l64781_readreg (state, 0x09) << 8) |
		     (l64781_readreg (state, 0x0a) << 16));
	p->frequency += tmp;

	return 0;
}

static int l64781_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	struct l64781_state* state = fe->demodulator_priv;
	int sync = l64781_readreg (state, 0x32);
	int gain = l64781_readreg (state, 0x0e);

	l64781_readreg (state, 0x00);  
	l64781_readreg (state, 0x01);  

	*status = 0;

	if (gain > 5)
		*status |= FE_HAS_SIGNAL;

	if (sync & 0x02) 
		*status |= FE_HAS_CARRIER;

	if (sync & 0x20)
		*status |= FE_HAS_VITERBI;

	if (sync & 0x40)
		*status |= FE_HAS_SYNC;

	if (sync == 0x7f)
		*status |= FE_HAS_LOCK;

	return 0;
}

static int l64781_read_ber(struct dvb_frontend* fe, u32* ber)
{
	struct l64781_state* state = fe->demodulator_priv;

	*ber = l64781_readreg (state, 0x39)
	    | (l64781_readreg (state, 0x3a) << 8);

	return 0;
}

static int l64781_read_signal_strength(struct dvb_frontend* fe, u16* signal_strength)
{
	struct l64781_state* state = fe->demodulator_priv;

	u8 gain = l64781_readreg (state, 0x0e);
	*signal_strength = (gain << 8) | gain;

	return 0;
}

static int l64781_read_snr(struct dvb_frontend* fe, u16* snr)
{
	struct l64781_state* state = fe->demodulator_priv;

	u8 avg_quality = 0xff - l64781_readreg (state, 0x33);
	*snr = (avg_quality << 8) | avg_quality; 

	return 0;
}

static int l64781_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	struct l64781_state* state = fe->demodulator_priv;

	*ucblocks = l64781_readreg (state, 0x37)
	   | (l64781_readreg (state, 0x38) << 8);

	return 0;
}

static int l64781_sleep(struct dvb_frontend* fe)
{
	struct l64781_state* state = fe->demodulator_priv;

	
	return l64781_writereg (state, 0x3e, 0x5a);
}

static int l64781_init(struct dvb_frontend* fe)
{
	struct l64781_state* state = fe->demodulator_priv;

	reset_and_configure (state);

	
	l64781_writereg (state, 0x3e, 0xa5);

	
	l64781_writereg (state, 0x2a, 0x04);
	l64781_writereg (state, 0x2a, 0x00);

	
	
	l64781_writereg (state, 0x07, 0x8e);

	
	l64781_writereg (state, 0x0b, 0x81);

	
	l64781_writereg (state, 0x0c, 0x84);

	
	l64781_writereg (state, 0x0d, 0x8c);

	

	
	l64781_writereg (state, 0x1e, 0x09);

	
	if (state->first) {
		state->first = 0;
		msleep(200);
	}

	return 0;
}

static int l64781_get_tune_settings(struct dvb_frontend* fe,
				    struct dvb_frontend_tune_settings* fesettings)
{
	fesettings->min_delay_ms = 4000;
	fesettings->step_size = 0;
	fesettings->max_drift = 0;
	return 0;
}

static void l64781_release(struct dvb_frontend* fe)
{
	struct l64781_state* state = fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops l64781_ops;

struct dvb_frontend* l64781_attach(const struct l64781_config* config,
				   struct i2c_adapter* i2c)
{
	struct l64781_state* state = NULL;
	int reg0x3e = -1;
	u8 b0 [] = { 0x1a };
	u8 b1 [] = { 0x00 };
	struct i2c_msg msg [] = { { .addr = config->demod_address, .flags = 0, .buf = b0, .len = 1 },
			   { .addr = config->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	
	state = kzalloc(sizeof(struct l64781_state), GFP_KERNEL);
	if (state == NULL) goto error;

	
	state->config = config;
	state->i2c = i2c;
	state->first = 1;

	if (reset_and_configure(state) < 0) {
		dprintk("No response to reset and configure broadcast...\n");
		goto error;
	}

	
	if (i2c_transfer(state->i2c, msg, 2) != 2) {
		dprintk("No response to read on I2C bus\n");
		goto error;
	}

	
	reg0x3e = l64781_readreg(state, 0x3e);

	
	if (reg0x3e != 0) {
		dprintk("Device doesn't look like L64781\n");
		goto error;
	}

	
	l64781_writereg (state, 0x3e, 0x5a);

	
	if (l64781_readreg(state, 0x1a) != 0) {
		dprintk("Read 1 returned unexpcted value\n");
		goto error;
	}

	
	l64781_writereg (state, 0x3e, 0xa5);

	
	if (l64781_readreg(state, 0x1a) != 0xa1) {
		dprintk("Read 2 returned unexpcted value\n");
		goto error;
	}

	
	memcpy(&state->frontend.ops, &l64781_ops, sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;
	return &state->frontend;

error:
	if (reg0x3e >= 0)
		l64781_writereg (state, 0x3e, reg0x3e);  
	kfree(state);
	return NULL;
}

static struct dvb_frontend_ops l64781_ops = {
	.delsys = { SYS_DVBT },
	.info = {
		.name = "LSI L64781 DVB-T",
	
	
		.frequency_stepsize = 166666,
	
	
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		      FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 |
		      FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
		      FE_CAN_MUTE_TS
	},

	.release = l64781_release,

	.init = l64781_init,
	.sleep = l64781_sleep,

	.set_frontend = apply_frontend_param,
	.get_frontend = get_frontend,
	.get_tune_settings = l64781_get_tune_settings,

	.read_status = l64781_read_status,
	.read_ber = l64781_read_ber,
	.read_signal_strength = l64781_read_signal_strength,
	.read_snr = l64781_read_snr,
	.read_ucblocks = l64781_read_ucblocks,
};

MODULE_DESCRIPTION("LSI L64781 DVB-T Demodulator driver");
MODULE_AUTHOR("Holger Waechtler, Marko Kohtala");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(l64781_attach);
