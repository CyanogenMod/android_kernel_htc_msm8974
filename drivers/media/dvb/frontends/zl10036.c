/**
 * Driver for Zarlink zl10036 DVB-S silicon tuner
 *
 * Copyright (C) 2006 Tino Reichardt
 * Copyright (C) 2007-2009 Matthias Schwarzott <zzam@gentoo.de>
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
 *
 **
 * The data sheet for this tuner can be found at:
 *    http://www.mcmilk.de/projects/dvb-card/datasheets/ZL10036.pdf
 *
 * This one is working: (at my Avermedia DVB-S Pro)
 * - zl10036 (40pin, FTA)
 *
 * A driver for zl10038 should be very similar.
 */

#include <linux/module.h>
#include <linux/dvb/frontend.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "zl10036.h"

static int zl10036_debug;
#define dprintk(level, args...) \
	do { if (zl10036_debug & level) printk(KERN_DEBUG "zl10036: " args); \
	} while (0)

#define deb_info(args...)  dprintk(0x01, args)
#define deb_i2c(args...)  dprintk(0x02, args)

struct zl10036_state {
	struct i2c_adapter *i2c;
	const struct zl10036_config *config;
	u32 frequency;
	u8 br, bf;
};


#define _XTAL 10111


#define _RDIV 10
#define _RDIV_REG 0x0a
#define _FR   (_XTAL/_RDIV)

#define STATUS_POR 0x80 
#define STATUS_FL  0x40 


static int zl10036_read_status_reg(struct zl10036_state *state)
{
	u8 status;
	struct i2c_msg msg[1] = {
		{ .addr = state->config->tuner_address, .flags = I2C_M_RD,
		  .buf = &status, .len = sizeof(status) },
	};

	if (i2c_transfer(state->i2c, msg, 1) != 1) {
		printk(KERN_ERR "%s: i2c read failed at addr=%02x\n",
			__func__, state->config->tuner_address);
		return -EIO;
	}

	deb_i2c("R(status): %02x  [FL=%d]\n", status,
		(status & STATUS_FL) ? 1 : 0);
	if (status & STATUS_POR)
		deb_info("%s: Power-On-Reset bit enabled - "
			"need to initialize the tuner\n", __func__);

	return status;
}

static int zl10036_write(struct zl10036_state *state, u8 buf[], u8 count)
{
	struct i2c_msg msg[1] = {
		{ .addr = state->config->tuner_address, .flags = 0,
		  .buf = buf, .len = count },
	};
	u8 reg = 0;
	int ret;

	if (zl10036_debug & 0x02) {
		if ((buf[0] & 0x80) == 0x00)
			reg = 2;
		else if ((buf[0] & 0xc0) == 0x80)
			reg = 4;
		else if ((buf[0] & 0xf0) == 0xc0)
			reg = 6;
		else if ((buf[0] & 0xf0) == 0xd0)
			reg = 8;
		else if ((buf[0] & 0xf0) == 0xe0)
			reg = 10;
		else if ((buf[0] & 0xf0) == 0xf0)
			reg = 12;

		deb_i2c("W(%d):", reg);
		{
			int i;
			for (i = 0; i < count; i++)
				printk(KERN_CONT " %02x", buf[i]);
			printk(KERN_CONT "\n");
		}
	}

	ret = i2c_transfer(state->i2c, msg, 1);
	if (ret != 1) {
		printk(KERN_ERR "%s: i2c error, ret=%d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

static int zl10036_release(struct dvb_frontend *fe)
{
	struct zl10036_state *state = fe->tuner_priv;

	fe->tuner_priv = NULL;
	kfree(state);

	return 0;
}

static int zl10036_sleep(struct dvb_frontend *fe)
{
	struct zl10036_state *state = fe->tuner_priv;
	u8 buf[] = { 0xf0, 0x80 }; 
	int ret;

	deb_info("%s\n", __func__);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); 

	ret = zl10036_write(state, buf, sizeof(buf));

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); 

	return ret;
}


static int zl10036_set_frequency(struct zl10036_state *state, u32 frequency)
{
	u8 buf[2];
	u32 div, foffset;

	div = (frequency + _FR/2) / _FR;
	state->frequency = div * _FR;

	foffset = frequency - state->frequency;

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = (div >> 0) & 0xff;

	deb_info("%s: ftodo=%u fpriv=%u ferr=%d div=%u\n", __func__,
		frequency, state->frequency, foffset, div);

	return zl10036_write(state, buf, sizeof(buf));
}

static int zl10036_set_bandwidth(struct zl10036_state *state, u32 fbw)
{
	
	u8 br, bf;
	int ret;
	u8 buf_bf[] = {
		0xc0, 0x00, 
	};
	u8 buf_br[] = {
		0xf0, 0x00, 
	};
	u8 zl10036_rsd_off[] = { 0xc8 }; 

	
	if (fbw > 35000)
		fbw = 35000;
	if (fbw <  8000)
		fbw =  8000;

#define _BR_MAXIMUM (_XTAL/575) 

	
	if (fbw <= 28820) {
		br = _BR_MAXIMUM;
	} else {
		br = ((_XTAL * 21 * 1000) / (fbw * 419));
	}

	
	if (br < 4)
		br = 4;
	if (br > _BR_MAXIMUM)
		br = _BR_MAXIMUM;


	bf = (fbw * br * 1257) / (_XTAL * 1000) - 1;

	
	if (bf > 62)
		bf = 62;

	buf_bf[1] = (bf << 1) & 0x7e;
	buf_br[1] = (br << 2) & 0x7c;
	deb_info("%s: BW=%d br=%u bf=%u\n", __func__, fbw, br, bf);

	if (br != state->br) {
		ret = zl10036_write(state, buf_br, sizeof(buf_br));
		if (ret < 0)
			return ret;
	}

	if (bf != state->bf) {
		ret = zl10036_write(state, buf_bf, sizeof(buf_bf));
		if (ret < 0)
			return ret;

		
		msleep(1);

		ret = zl10036_write(state, zl10036_rsd_off,
			sizeof(zl10036_rsd_off));
		if (ret < 0)
			return ret;
	}

	state->br = br;
	state->bf = bf;

	return 0;
}

static int zl10036_set_gain_params(struct zl10036_state *state,
	int c)
{
	u8 buf[2];
	u8 rfg, ba, bg;

	
	rfg = 0; 
	ba = 1;
	bg = 1;

	
	buf[0] = 0x80 | ((rfg << 5) & 0x20)
		| ((ba  << 3) & 0x18) | ((bg  << 1) & 0x06);

	if (!state->config->rf_loop_enable)
		buf[0] |= 0x01;

	
	buf[1] = _RDIV_REG | ((c << 5) & 0x60);

	deb_info("%s: c=%u rfg=%u ba=%u bg=%u\n", __func__, c, rfg, ba, bg);
	return zl10036_write(state, buf, sizeof(buf));
}

static int zl10036_set_params(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct zl10036_state *state = fe->tuner_priv;
	int ret = 0;
	u32 frequency = p->frequency;
	u32 fbw;
	int i;
	u8 c;

	if ((frequency < fe->ops.info.frequency_min)
	||  (frequency > fe->ops.info.frequency_max))
		return -EINVAL;

	fbw = (27 * p->symbol_rate) / 32;

	
	fbw /= 1000;

	
	fbw += 3000;

	
	if (frequency < 950000)
		return -EINVAL;
	else if (frequency < 1250000)
		c = 0;
	else if (frequency < 1750000)
		c = 1;
	else if (frequency < 2175000)
		c = 2;
	else
		return -EINVAL;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); 

	ret = zl10036_set_gain_params(state, c);
	if (ret < 0)
		goto error;

	ret = zl10036_set_frequency(state, p->frequency);
	if (ret < 0)
		goto error;

	ret = zl10036_set_bandwidth(state, fbw);
	if (ret < 0)
		goto error;

	
	for (i = 0; i < 20; i++) {
		ret = zl10036_read_status_reg(state);
		if (ret < 0)
			goto error;

		
		if (ret & STATUS_FL)
			break;

		msleep(10);
	}

error:
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); 

	return ret;
}

static int zl10036_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct zl10036_state *state = fe->tuner_priv;

	*frequency = state->frequency;

	return 0;
}

static int zl10036_init_regs(struct zl10036_state *state)
{
	int ret;
	int i;

	
	u8 zl10036_init_tab[][2] = {
		{ 0x04, 0x00 },		
		{ 0x8b, _RDIV_REG },	
					
		{ 0xc0, 0x20 },		
		{ 0xd3, 0x40 },		
		{ 0xe3, 0x5b },		
		{ 0xf0, 0x28 },		
		{ 0xe3, 0xf9 },		
	};

	
	state->br = 0xff;
	state->bf = 0xff;

	if (!state->config->rf_loop_enable)
		zl10036_init_tab[1][0] |= 0x01;

	deb_info("%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(zl10036_init_tab); i++) {
		ret = zl10036_write(state, zl10036_init_tab[i], 2);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int zl10036_init(struct dvb_frontend *fe)
{
	struct zl10036_state *state = fe->tuner_priv;
	int ret = 0;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); 

	ret = zl10036_read_status_reg(state);
	if (ret < 0)
		return ret;

	
	ret = zl10036_init_regs(state);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); 

	return ret;
}

static struct dvb_tuner_ops zl10036_tuner_ops = {
	.info = {
		.name = "Zarlink ZL10036",
		.frequency_min = 950000,
		.frequency_max = 2175000
	},
	.init = zl10036_init,
	.release = zl10036_release,
	.sleep = zl10036_sleep,
	.set_params = zl10036_set_params,
	.get_frequency = zl10036_get_frequency,
};

struct dvb_frontend *zl10036_attach(struct dvb_frontend *fe,
				    const struct zl10036_config *config,
				    struct i2c_adapter *i2c)
{
	struct zl10036_state *state;
	int ret;

	if (!config) {
		printk(KERN_ERR "%s: no config specified", __func__);
		return NULL;
	}

	state = kzalloc(sizeof(struct zl10036_state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->config = config;
	state->i2c = i2c;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); 

	ret = zl10036_read_status_reg(state);
	if (ret < 0) {
		printk(KERN_ERR "%s: No zl10036 found\n", __func__);
		goto error;
	}

	ret = zl10036_init_regs(state);
	if (ret < 0) {
		printk(KERN_ERR "%s: tuner initialization failed\n",
			__func__);
		goto error;
	}

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); 

	fe->tuner_priv = state;

	memcpy(&fe->ops.tuner_ops, &zl10036_tuner_ops,
		sizeof(struct dvb_tuner_ops));
	printk(KERN_INFO "%s: tuner initialization (%s addr=0x%02x) ok\n",
		__func__, fe->ops.tuner_ops.info.name, config->tuner_address);

	return fe;

error:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(zl10036_attach);

module_param_named(debug, zl10036_debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");
MODULE_DESCRIPTION("DVB ZL10036 driver");
MODULE_AUTHOR("Tino Reichardt");
MODULE_AUTHOR("Matthias Schwarzott");
MODULE_LICENSE("GPL");
