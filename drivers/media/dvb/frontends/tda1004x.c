  /*
     Driver for Philips tda1004xh OFDM Demodulator

     (c) 2003, 2004 Andrew de Quincey & Robert Schlabbach

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
#define TDA10045_DEFAULT_FIRMWARE "dvb-fe-tda10045.fw"
#define TDA10046_DEFAULT_FIRMWARE "dvb-fe-tda10046.fw"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "dvb_frontend.h"
#include "tda1004x.h"

static int debug;
#define dprintk(args...) \
	do { \
		if (debug) printk(KERN_DEBUG "tda1004x: " args); \
	} while (0)

#define TDA1004X_CHIPID		 0x00
#define TDA1004X_AUTO		 0x01
#define TDA1004X_IN_CONF1	 0x02
#define TDA1004X_IN_CONF2	 0x03
#define TDA1004X_OUT_CONF1	 0x04
#define TDA1004X_OUT_CONF2	 0x05
#define TDA1004X_STATUS_CD	 0x06
#define TDA1004X_CONFC4		 0x07
#define TDA1004X_DSSPARE2	 0x0C
#define TDA10045H_CODE_IN	 0x0D
#define TDA10045H_FWPAGE	 0x0E
#define TDA1004X_SCAN_CPT	 0x10
#define TDA1004X_DSP_CMD	 0x11
#define TDA1004X_DSP_ARG	 0x12
#define TDA1004X_DSP_DATA1	 0x13
#define TDA1004X_DSP_DATA2	 0x14
#define TDA1004X_CONFADC1	 0x15
#define TDA1004X_CONFC1		 0x16
#define TDA10045H_S_AGC		 0x1a
#define TDA10046H_AGC_TUN_LEVEL	 0x1a
#define TDA1004X_SNR		 0x1c
#define TDA1004X_CONF_TS1	 0x1e
#define TDA1004X_CONF_TS2	 0x1f
#define TDA1004X_CBER_RESET	 0x20
#define TDA1004X_CBER_MSB	 0x21
#define TDA1004X_CBER_LSB	 0x22
#define TDA1004X_CVBER_LUT	 0x23
#define TDA1004X_VBER_MSB	 0x24
#define TDA1004X_VBER_MID	 0x25
#define TDA1004X_VBER_LSB	 0x26
#define TDA1004X_UNCOR		 0x27

#define TDA10045H_CONFPLL_P	 0x2D
#define TDA10045H_CONFPLL_M_MSB	 0x2E
#define TDA10045H_CONFPLL_M_LSB	 0x2F
#define TDA10045H_CONFPLL_N	 0x30

#define TDA10046H_CONFPLL1	 0x2D
#define TDA10046H_CONFPLL2	 0x2F
#define TDA10046H_CONFPLL3	 0x30
#define TDA10046H_TIME_WREF1	 0x31
#define TDA10046H_TIME_WREF2	 0x32
#define TDA10046H_TIME_WREF3	 0x33
#define TDA10046H_TIME_WREF4	 0x34
#define TDA10046H_TIME_WREF5	 0x35

#define TDA10045H_UNSURW_MSB	 0x31
#define TDA10045H_UNSURW_LSB	 0x32
#define TDA10045H_WREF_MSB	 0x33
#define TDA10045H_WREF_MID	 0x34
#define TDA10045H_WREF_LSB	 0x35
#define TDA10045H_MUXOUT	 0x36
#define TDA1004X_CONFADC2	 0x37

#define TDA10045H_IOFFSET	 0x38

#define TDA10046H_CONF_TRISTATE1 0x3B
#define TDA10046H_CONF_TRISTATE2 0x3C
#define TDA10046H_CONF_POLARITY	 0x3D
#define TDA10046H_FREQ_OFFSET	 0x3E
#define TDA10046H_GPIO_OUT_SEL	 0x41
#define TDA10046H_GPIO_SELECT	 0x42
#define TDA10046H_AGC_CONF	 0x43
#define TDA10046H_AGC_THR	 0x44
#define TDA10046H_AGC_RENORM	 0x45
#define TDA10046H_AGC_GAINS	 0x46
#define TDA10046H_AGC_TUN_MIN	 0x47
#define TDA10046H_AGC_TUN_MAX	 0x48
#define TDA10046H_AGC_IF_MIN	 0x49
#define TDA10046H_AGC_IF_MAX	 0x4A

#define TDA10046H_FREQ_PHY2_MSB	 0x4D
#define TDA10046H_FREQ_PHY2_LSB	 0x4E

#define TDA10046H_CVBER_CTRL	 0x4F
#define TDA10046H_AGC_IF_LEVEL	 0x52
#define TDA10046H_CODE_CPT	 0x57
#define TDA10046H_CODE_IN	 0x58


static int tda1004x_write_byteI(struct tda1004x_state *state, int reg, int data)
{
	int ret;
	u8 buf[] = { reg, data };
	struct i2c_msg msg = { .flags = 0, .buf = buf, .len = 2 };

	dprintk("%s: reg=0x%x, data=0x%x\n", __func__, reg, data);

	msg.addr = state->config->demod_address;
	ret = i2c_transfer(state->i2c, &msg, 1);

	if (ret != 1)
		dprintk("%s: error reg=0x%x, data=0x%x, ret=%i\n",
			__func__, reg, data, ret);

	dprintk("%s: success reg=0x%x, data=0x%x, ret=%i\n", __func__,
		reg, data, ret);
	return (ret != 1) ? -1 : 0;
}

static int tda1004x_read_byte(struct tda1004x_state *state, int reg)
{
	int ret;
	u8 b0[] = { reg };
	u8 b1[] = { 0 };
	struct i2c_msg msg[] = {{ .flags = 0, .buf = b0, .len = 1 },
				{ .flags = I2C_M_RD, .buf = b1, .len = 1 }};

	dprintk("%s: reg=0x%x\n", __func__, reg);

	msg[0].addr = state->config->demod_address;
	msg[1].addr = state->config->demod_address;
	ret = i2c_transfer(state->i2c, msg, 2);

	if (ret != 2) {
		dprintk("%s: error reg=0x%x, ret=%i\n", __func__, reg,
			ret);
		return -EINVAL;
	}

	dprintk("%s: success reg=0x%x, data=0x%x, ret=%i\n", __func__,
		reg, b1[0], ret);
	return b1[0];
}

static int tda1004x_write_mask(struct tda1004x_state *state, int reg, int mask, int data)
{
	int val;
	dprintk("%s: reg=0x%x, mask=0x%x, data=0x%x\n", __func__, reg,
		mask, data);

	
	val = tda1004x_read_byte(state, reg);
	if (val < 0)
		return val;

	
	val = val & ~mask;
	val |= data & 0xff;

	
	return tda1004x_write_byteI(state, reg, val);
}

static int tda1004x_write_buf(struct tda1004x_state *state, int reg, unsigned char *buf, int len)
{
	int i;
	int result;

	dprintk("%s: reg=0x%x, len=0x%x\n", __func__, reg, len);

	result = 0;
	for (i = 0; i < len; i++) {
		result = tda1004x_write_byteI(state, reg + i, buf[i]);
		if (result != 0)
			break;
	}

	return result;
}

static int tda1004x_enable_tuner_i2c(struct tda1004x_state *state)
{
	int result;
	dprintk("%s\n", __func__);

	result = tda1004x_write_mask(state, TDA1004X_CONFC4, 2, 2);
	msleep(20);
	return result;
}

static int tda1004x_disable_tuner_i2c(struct tda1004x_state *state)
{
	dprintk("%s\n", __func__);

	return tda1004x_write_mask(state, TDA1004X_CONFC4, 2, 0);
}

static int tda10045h_set_bandwidth(struct tda1004x_state *state,
				   u32 bandwidth)
{
	static u8 bandwidth_6mhz[] = { 0x02, 0x00, 0x3d, 0x00, 0x60, 0x1e, 0xa7, 0x45, 0x4f };
	static u8 bandwidth_7mhz[] = { 0x02, 0x00, 0x37, 0x00, 0x4a, 0x2f, 0x6d, 0x76, 0xdb };
	static u8 bandwidth_8mhz[] = { 0x02, 0x00, 0x3d, 0x00, 0x48, 0x17, 0x89, 0xc7, 0x14 };

	switch (bandwidth) {
	case 6000000:
		tda1004x_write_buf(state, TDA10045H_CONFPLL_P, bandwidth_6mhz, sizeof(bandwidth_6mhz));
		break;

	case 7000000:
		tda1004x_write_buf(state, TDA10045H_CONFPLL_P, bandwidth_7mhz, sizeof(bandwidth_7mhz));
		break;

	case 8000000:
		tda1004x_write_buf(state, TDA10045H_CONFPLL_P, bandwidth_8mhz, sizeof(bandwidth_8mhz));
		break;

	default:
		return -EINVAL;
	}

	tda1004x_write_byteI(state, TDA10045H_IOFFSET, 0);

	return 0;
}

static int tda10046h_set_bandwidth(struct tda1004x_state *state,
				   u32 bandwidth)
{
	static u8 bandwidth_6mhz_53M[] = { 0x7b, 0x2e, 0x11, 0xf0, 0xd2 };
	static u8 bandwidth_7mhz_53M[] = { 0x6a, 0x02, 0x6a, 0x43, 0x9f };
	static u8 bandwidth_8mhz_53M[] = { 0x5c, 0x32, 0xc2, 0x96, 0x6d };

	static u8 bandwidth_6mhz_48M[] = { 0x70, 0x02, 0x49, 0x24, 0x92 };
	static u8 bandwidth_7mhz_48M[] = { 0x60, 0x02, 0xaa, 0xaa, 0xab };
	static u8 bandwidth_8mhz_48M[] = { 0x54, 0x03, 0x0c, 0x30, 0xc3 };
	int tda10046_clk53m;

	if ((state->config->if_freq == TDA10046_FREQ_045) ||
	    (state->config->if_freq == TDA10046_FREQ_052))
		tda10046_clk53m = 0;
	else
		tda10046_clk53m = 1;
	switch (bandwidth) {
	case 6000000:
		if (tda10046_clk53m)
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_6mhz_53M,
						  sizeof(bandwidth_6mhz_53M));
		else
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_6mhz_48M,
						  sizeof(bandwidth_6mhz_48M));
		if (state->config->if_freq == TDA10046_FREQ_045) {
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0x0a);
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0xab);
		}
		break;

	case 7000000:
		if (tda10046_clk53m)
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_7mhz_53M,
						  sizeof(bandwidth_7mhz_53M));
		else
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_7mhz_48M,
						  sizeof(bandwidth_7mhz_48M));
		if (state->config->if_freq == TDA10046_FREQ_045) {
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0x0c);
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0x00);
		}
		break;

	case 8000000:
		if (tda10046_clk53m)
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_8mhz_53M,
						  sizeof(bandwidth_8mhz_53M));
		else
			tda1004x_write_buf(state, TDA10046H_TIME_WREF1, bandwidth_8mhz_48M,
						  sizeof(bandwidth_8mhz_48M));
		if (state->config->if_freq == TDA10046_FREQ_045) {
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0x0d);
			tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0x55);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int tda1004x_do_upload(struct tda1004x_state *state,
			      const unsigned char *mem, unsigned int len,
			      u8 dspCodeCounterReg, u8 dspCodeInReg)
{
	u8 buf[65];
	struct i2c_msg fw_msg = { .flags = 0, .buf = buf, .len = 0 };
	int tx_size;
	int pos = 0;

	
	tda1004x_write_byteI(state, dspCodeCounterReg, 0);
	fw_msg.addr = state->config->demod_address;

	buf[0] = dspCodeInReg;
	while (pos != len) {
		
		tx_size = len - pos;
		if (tx_size > 0x10)
			tx_size = 0x10;

		
		memcpy(buf + 1, mem + pos, tx_size);
		fw_msg.len = tx_size + 1;
		if (i2c_transfer(state->i2c, &fw_msg, 1) != 1) {
			printk(KERN_ERR "tda1004x: Error during firmware upload\n");
			return -EIO;
		}
		pos += tx_size;

		dprintk("%s: fw_pos=0x%x\n", __func__, pos);
	}
	
	msleep(100);

	return 0;
}

static int tda1004x_check_upload_ok(struct tda1004x_state *state)
{
	u8 data1, data2;
	unsigned long timeout;

	if (state->demod_type == TDA1004X_DEMOD_TDA10046) {
		timeout = jiffies + 2 * HZ;
		while(!(tda1004x_read_byte(state, TDA1004X_STATUS_CD) & 0x20)) {
			if (time_after(jiffies, timeout)) {
				printk(KERN_ERR "tda1004x: timeout waiting for DSP ready\n");
				break;
			}
			msleep(1);
		}
	} else
		msleep(100);

	
	tda1004x_write_mask(state, TDA1004X_CONFC4, 0x10, 0); 
	tda1004x_write_byteI(state, TDA1004X_DSP_CMD, 0x67);

	data1 = tda1004x_read_byte(state, TDA1004X_DSP_DATA1);
	data2 = tda1004x_read_byte(state, TDA1004X_DSP_DATA2);
	if (data1 != 0x67 || data2 < 0x20 || data2 > 0x2e) {
		printk(KERN_INFO "tda1004x: found firmware revision %x -- invalid\n", data2);
		return -EIO;
	}
	printk(KERN_INFO "tda1004x: found firmware revision %x -- ok\n", data2);
	return 0;
}

static int tda10045_fwupload(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int ret;
	const struct firmware *fw;

	
	if (tda1004x_check_upload_ok(state) == 0)
		return 0;

	
	printk(KERN_INFO "tda1004x: waiting for firmware upload (%s)...\n", TDA10045_DEFAULT_FIRMWARE);
	ret = state->config->request_firmware(fe, &fw, TDA10045_DEFAULT_FIRMWARE);
	if (ret) {
		printk(KERN_ERR "tda1004x: no firmware upload (timeout or file not found?)\n");
		return ret;
	}

	
	tda1004x_write_mask(state, TDA1004X_CONFC4, 0x10, 0);
	tda1004x_write_mask(state, TDA1004X_CONFC4, 8, 8);
	tda1004x_write_mask(state, TDA1004X_CONFC4, 8, 0);
	msleep(10);

	
	tda10045h_set_bandwidth(state, 8000000);

	ret = tda1004x_do_upload(state, fw->data, fw->size, TDA10045H_FWPAGE, TDA10045H_CODE_IN);
	release_firmware(fw);
	if (ret)
		return ret;
	printk(KERN_INFO "tda1004x: firmware upload complete\n");

	
	
	msleep(100);

	return tda1004x_check_upload_ok(state);
}

static void tda10046_init_plls(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int tda10046_clk53m;

	if ((state->config->if_freq == TDA10046_FREQ_045) ||
	    (state->config->if_freq == TDA10046_FREQ_052))
		tda10046_clk53m = 0;
	else
		tda10046_clk53m = 1;

	tda1004x_write_byteI(state, TDA10046H_CONFPLL1, 0xf0);
	if(tda10046_clk53m) {
		printk(KERN_INFO "tda1004x: setting up plls for 53MHz sampling clock\n");
		tda1004x_write_byteI(state, TDA10046H_CONFPLL2, 0x08); 
	} else {
		printk(KERN_INFO "tda1004x: setting up plls for 48MHz sampling clock\n");
		tda1004x_write_byteI(state, TDA10046H_CONFPLL2, 0x03); 
	}
	if (state->config->xtal_freq == TDA10046_XTAL_4M ) {
		dprintk("%s: setting up PLLs for a 4 MHz Xtal\n", __func__);
		tda1004x_write_byteI(state, TDA10046H_CONFPLL3, 0); 
	} else {
		dprintk("%s: setting up PLLs for a 16 MHz Xtal\n", __func__);
		tda1004x_write_byteI(state, TDA10046H_CONFPLL3, 3); 
	}
	if(tda10046_clk53m)
		tda1004x_write_byteI(state, TDA10046H_FREQ_OFFSET, 0x67);
	else
		tda1004x_write_byteI(state, TDA10046H_FREQ_OFFSET, 0x72);
	
	switch (state->config->if_freq) {
	case TDA10046_FREQ_045:
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0x0c);
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0x00);
		break;
	case TDA10046_FREQ_052:
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0x0d);
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0xc7);
		break;
	case TDA10046_FREQ_3617:
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0xd7);
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0x59);
		break;
	case TDA10046_FREQ_3613:
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_MSB, 0xd7);
		tda1004x_write_byteI(state, TDA10046H_FREQ_PHY2_LSB, 0x3f);
		break;
	}
	tda10046h_set_bandwidth(state, 8000000); 
	
	msleep(120);
}

static int tda10046_fwupload(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int ret, confc4;
	const struct firmware *fw;

	
	if (state->config->xtal_freq == TDA10046_XTAL_4M) {
		confc4 = 0;
	} else {
		dprintk("%s: 16MHz Xtal, reducing I2C speed\n", __func__);
		confc4 = 0x80;
	}
	tda1004x_write_byteI(state, TDA1004X_CONFC4, confc4);

	tda1004x_write_mask(state, TDA10046H_CONF_TRISTATE1, 1, 0);
	
	if (state->config->gpio_config != TDA10046_GPTRI) {
		tda1004x_write_byteI(state, TDA10046H_CONF_TRISTATE2, 0x33);
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0x0f, state->config->gpio_config &0x0f);
	}
	
	msleep(10);

	
	tda10046_init_plls(fe);
	tda1004x_write_mask(state, TDA1004X_CONFADC2, 0xc0, 0);

	
	if (tda1004x_check_upload_ok(state) == 0)
		return 0;

	printk(KERN_INFO "tda1004x: trying to boot from eeprom\n");
	tda1004x_write_byteI(state, TDA1004X_CONFC4, 4);
	msleep(300);
	tda1004x_write_byteI(state, TDA1004X_CONFC4, confc4);

	
	if (tda1004x_check_upload_ok(state) == 0)
		return 0;

	

	if (state->config->request_firmware != NULL) {
		
		printk(KERN_INFO "tda1004x: waiting for firmware upload...\n");
		ret = state->config->request_firmware(fe, &fw, TDA10046_DEFAULT_FIRMWARE);
		if (ret) {
			
			ret = state->config->request_firmware(fe, &fw, TDA10045_DEFAULT_FIRMWARE);
			if (ret) {
				printk(KERN_ERR "tda1004x: no firmware upload (timeout or file not found?)\n");
				return ret;
			} else {
				printk(KERN_INFO "tda1004x: please rename the firmware file to %s\n",
						  TDA10046_DEFAULT_FIRMWARE);
			}
		}
	} else {
		printk(KERN_ERR "tda1004x: no request function defined, can't upload from file\n");
		return -EIO;
	}
	tda1004x_write_mask(state, TDA1004X_CONFC4, 8, 8); 
	ret = tda1004x_do_upload(state, fw->data, fw->size, TDA10046H_CODE_CPT, TDA10046H_CODE_IN);
	release_firmware(fw);
	return tda1004x_check_upload_ok(state);
}

static int tda1004x_encode_fec(int fec)
{
	
	switch (fec) {
	case FEC_1_2:
		return 0;
	case FEC_2_3:
		return 1;
	case FEC_3_4:
		return 2;
	case FEC_5_6:
		return 3;
	case FEC_7_8:
		return 4;
	}

	
	return -EINVAL;
}

static int tda1004x_decode_fec(int tdafec)
{
	
	switch (tdafec) {
	case 0:
		return FEC_1_2;
	case 1:
		return FEC_2_3;
	case 2:
		return FEC_3_4;
	case 3:
		return FEC_5_6;
	case 4:
		return FEC_7_8;
	}

	
	return -1;
}

static int tda1004x_write(struct dvb_frontend* fe, const u8 buf[], int len)
{
	struct tda1004x_state* state = fe->demodulator_priv;

	if (len != 2)
		return -EINVAL;

	return tda1004x_write_byteI(state, buf[0], buf[1]);
}

static int tda10045_init(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;

	dprintk("%s\n", __func__);

	if (tda10045_fwupload(fe)) {
		printk("tda1004x: firmware upload failed\n");
		return -EIO;
	}

	tda1004x_write_mask(state, TDA1004X_CONFADC1, 0x10, 0); 

	
	tda1004x_write_mask(state, TDA1004X_CONFC4, 0x20, 0); 
	tda1004x_write_mask(state, TDA1004X_AUTO, 8, 0); 
	tda1004x_write_mask(state, TDA1004X_CONFC1, 0x40, 0); 
	tda1004x_write_mask(state, TDA1004X_CONFC1, 0x80, 0x80); 
	tda1004x_write_mask(state, TDA1004X_AUTO, 0x10, 0x10); 
	tda1004x_write_mask(state, TDA1004X_IN_CONF2, 0xC0, 0x0); 
	tda1004x_write_byteI(state, TDA1004X_CONF_TS1, 0); 
	tda1004x_write_byteI(state, TDA1004X_CONF_TS2, 0); 
	tda1004x_write_mask(state, TDA1004X_VBER_MSB, 0xe0, 0xa0); 
	tda1004x_write_mask(state, TDA1004X_CONFC1, 0x10, 0); 
	tda1004x_write_byteI(state, TDA1004X_CONFADC1, 0x2e);

	tda1004x_write_mask(state, 0x1f, 0x01, state->config->invert_oclk);

	return 0;
}

static int tda10046_init(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	dprintk("%s\n", __func__);

	if (tda10046_fwupload(fe)) {
		printk("tda1004x: firmware upload failed\n");
			return -EIO;
	}

	
	tda1004x_write_mask(state, TDA1004X_CONFC4, 0x20, 0); 
	tda1004x_write_byteI(state, TDA1004X_AUTO, 0x87);    
	tda1004x_write_byteI(state, TDA1004X_CONFC1, 0x88);      

	switch (state->config->agc_config) {
	case TDA10046_AGC_DEFAULT:
		tda1004x_write_byteI(state, TDA10046H_AGC_CONF, 0x00); 
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0xf0, 0x60);  
		break;
	case TDA10046_AGC_IFO_AUTO_NEG:
		tda1004x_write_byteI(state, TDA10046H_AGC_CONF, 0x0a); 
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0xf0, 0x60);  
		break;
	case TDA10046_AGC_IFO_AUTO_POS:
		tda1004x_write_byteI(state, TDA10046H_AGC_CONF, 0x0a); 
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0xf0, 0x00);  
		break;
	case TDA10046_AGC_TDA827X:
		tda1004x_write_byteI(state, TDA10046H_AGC_CONF, 0x02);   
		tda1004x_write_byteI(state, TDA10046H_AGC_THR, 0x70);    
		tda1004x_write_byteI(state, TDA10046H_AGC_RENORM, 0x08); 
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0xf0, 0x60);  
		break;
	}
	if (state->config->ts_mode == 0) {
		tda1004x_write_mask(state, TDA10046H_CONF_TRISTATE1, 0xc0, 0x40);
		tda1004x_write_mask(state, 0x3a, 0x80, state->config->invert_oclk << 7);
	} else {
		tda1004x_write_mask(state, TDA10046H_CONF_TRISTATE1, 0xc0, 0x80);
		tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0x10,
							state->config->invert_oclk << 4);
	}
	tda1004x_write_byteI(state, TDA1004X_CONFADC2, 0x38);
	tda1004x_write_mask (state, TDA10046H_CONF_TRISTATE1, 0x3e, 0x38); 
	tda1004x_write_byteI(state, TDA10046H_AGC_TUN_MIN, 0);	  
	tda1004x_write_byteI(state, TDA10046H_AGC_TUN_MAX, 0xff); 
	tda1004x_write_byteI(state, TDA10046H_AGC_IF_MIN, 0);	  
	tda1004x_write_byteI(state, TDA10046H_AGC_IF_MAX, 0xff);  
	tda1004x_write_byteI(state, TDA10046H_AGC_GAINS, 0x12); 
	tda1004x_write_byteI(state, TDA10046H_CVBER_CTRL, 0x1a); 
	tda1004x_write_byteI(state, TDA1004X_CONF_TS1, 7); 
	tda1004x_write_byteI(state, TDA1004X_CONF_TS2, 0xc0); 
	

	return 0;
}

static int tda1004x_set_fe(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *fe_params = &fe->dtv_property_cache;
	struct tda1004x_state* state = fe->demodulator_priv;
	int tmp;
	int inversion;

	dprintk("%s\n", __func__);

	if (state->demod_type == TDA1004X_DEMOD_TDA10046) {
		
		tda1004x_write_mask(state, TDA1004X_AUTO, 0x10, 0x10);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x80, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF2, 0xC0, 0);

		
		tda1004x_write_mask(state, TDA10046H_AGC_CONF, 4, 0);
	}

	
	if (fe->ops.tuner_ops.set_params) {
		fe->ops.tuner_ops.set_params(fe);
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 0);
	}

	
	
	if (state->demod_type == TDA1004X_DEMOD_TDA10045) {
		fe_params->code_rate_HP = FEC_AUTO;
		fe_params->guard_interval = GUARD_INTERVAL_AUTO;
		fe_params->transmission_mode = TRANSMISSION_MODE_AUTO;
	}

	
	if ((fe_params->code_rate_HP == FEC_AUTO) ||
		(fe_params->code_rate_LP == FEC_AUTO) ||
		(fe_params->modulation == QAM_AUTO) ||
		(fe_params->hierarchy == HIERARCHY_AUTO)) {
		tda1004x_write_mask(state, TDA1004X_AUTO, 1, 1);	
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x03, 0);	
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x60, 0);	
		tda1004x_write_mask(state, TDA1004X_IN_CONF2, 0x3f, 0);	
	} else {
		tda1004x_write_mask(state, TDA1004X_AUTO, 1, 0);	

		
		tmp = tda1004x_encode_fec(fe_params->code_rate_HP);
		if (tmp < 0)
			return tmp;
		tda1004x_write_mask(state, TDA1004X_IN_CONF2, 7, tmp);

		
		tmp = tda1004x_encode_fec(fe_params->code_rate_LP);
		if (tmp < 0)
			return tmp;
		tda1004x_write_mask(state, TDA1004X_IN_CONF2, 0x38, tmp << 3);

		
		switch (fe_params->modulation) {
		case QPSK:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 3, 0);
			break;

		case QAM_16:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 3, 1);
			break;

		case QAM_64:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 3, 2);
			break;

		default:
			return -EINVAL;
		}

		
		switch (fe_params->hierarchy) {
		case HIERARCHY_NONE:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x60, 0 << 5);
			break;

		case HIERARCHY_1:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x60, 1 << 5);
			break;

		case HIERARCHY_2:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x60, 2 << 5);
			break;

		case HIERARCHY_4:
			tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x60, 3 << 5);
			break;

		default:
			return -EINVAL;
		}
	}

	
	switch (state->demod_type) {
	case TDA1004X_DEMOD_TDA10045:
		tda10045h_set_bandwidth(state, fe_params->bandwidth_hz);
		break;

	case TDA1004X_DEMOD_TDA10046:
		tda10046h_set_bandwidth(state, fe_params->bandwidth_hz);
		break;
	}

	
	inversion = fe_params->inversion;
	if (state->config->invert)
		inversion = inversion ? INVERSION_OFF : INVERSION_ON;
	switch (inversion) {
	case INVERSION_OFF:
		tda1004x_write_mask(state, TDA1004X_CONFC1, 0x20, 0);
		break;

	case INVERSION_ON:
		tda1004x_write_mask(state, TDA1004X_CONFC1, 0x20, 0x20);
		break;

	default:
		return -EINVAL;
	}

	
	switch (fe_params->guard_interval) {
	case GUARD_INTERVAL_1_32:
		tda1004x_write_mask(state, TDA1004X_AUTO, 2, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x0c, 0 << 2);
		break;

	case GUARD_INTERVAL_1_16:
		tda1004x_write_mask(state, TDA1004X_AUTO, 2, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x0c, 1 << 2);
		break;

	case GUARD_INTERVAL_1_8:
		tda1004x_write_mask(state, TDA1004X_AUTO, 2, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x0c, 2 << 2);
		break;

	case GUARD_INTERVAL_1_4:
		tda1004x_write_mask(state, TDA1004X_AUTO, 2, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x0c, 3 << 2);
		break;

	case GUARD_INTERVAL_AUTO:
		tda1004x_write_mask(state, TDA1004X_AUTO, 2, 2);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x0c, 0 << 2);
		break;

	default:
		return -EINVAL;
	}

	
	switch (fe_params->transmission_mode) {
	case TRANSMISSION_MODE_2K:
		tda1004x_write_mask(state, TDA1004X_AUTO, 4, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x10, 0 << 4);
		break;

	case TRANSMISSION_MODE_8K:
		tda1004x_write_mask(state, TDA1004X_AUTO, 4, 0);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x10, 1 << 4);
		break;

	case TRANSMISSION_MODE_AUTO:
		tda1004x_write_mask(state, TDA1004X_AUTO, 4, 4);
		tda1004x_write_mask(state, TDA1004X_IN_CONF1, 0x10, 0);
		break;

	default:
		return -EINVAL;
	}

	
	switch (state->demod_type) {
	case TDA1004X_DEMOD_TDA10045:
		tda1004x_write_mask(state, TDA1004X_CONFC4, 8, 8);
		tda1004x_write_mask(state, TDA1004X_CONFC4, 8, 0);
		break;

	case TDA1004X_DEMOD_TDA10046:
		tda1004x_write_mask(state, TDA1004X_AUTO, 0x40, 0x40);
		msleep(1);
		tda1004x_write_mask(state, TDA10046H_AGC_CONF, 4, 1);
		break;
	}

	msleep(10);

	return 0;
}

static int tda1004x_get_fe(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *fe_params = &fe->dtv_property_cache;
	struct tda1004x_state* state = fe->demodulator_priv;

	dprintk("%s\n", __func__);

	
	fe_params->inversion = INVERSION_OFF;
	if (tda1004x_read_byte(state, TDA1004X_CONFC1) & 0x20)
		fe_params->inversion = INVERSION_ON;
	if (state->config->invert)
		fe_params->inversion = fe_params->inversion ? INVERSION_OFF : INVERSION_ON;

	
	switch (state->demod_type) {
	case TDA1004X_DEMOD_TDA10045:
		switch (tda1004x_read_byte(state, TDA10045H_WREF_LSB)) {
		case 0x14:
			fe_params->bandwidth_hz = 8000000;
			break;
		case 0xdb:
			fe_params->bandwidth_hz = 7000000;
			break;
		case 0x4f:
			fe_params->bandwidth_hz = 6000000;
			break;
		}
		break;
	case TDA1004X_DEMOD_TDA10046:
		switch (tda1004x_read_byte(state, TDA10046H_TIME_WREF1)) {
		case 0x5c:
		case 0x54:
			fe_params->bandwidth_hz = 8000000;
			break;
		case 0x6a:
		case 0x60:
			fe_params->bandwidth_hz = 7000000;
			break;
		case 0x7b:
		case 0x70:
			fe_params->bandwidth_hz = 6000000;
			break;
		}
		break;
	}

	
	fe_params->code_rate_HP =
	    tda1004x_decode_fec(tda1004x_read_byte(state, TDA1004X_OUT_CONF2) & 7);
	fe_params->code_rate_LP =
	    tda1004x_decode_fec((tda1004x_read_byte(state, TDA1004X_OUT_CONF2) >> 3) & 7);

	
	switch (tda1004x_read_byte(state, TDA1004X_OUT_CONF1) & 3) {
	case 0:
		fe_params->modulation = QPSK;
		break;
	case 1:
		fe_params->modulation = QAM_16;
		break;
	case 2:
		fe_params->modulation = QAM_64;
		break;
	}

	
	fe_params->transmission_mode = TRANSMISSION_MODE_2K;
	if (tda1004x_read_byte(state, TDA1004X_OUT_CONF1) & 0x10)
		fe_params->transmission_mode = TRANSMISSION_MODE_8K;

	
	switch ((tda1004x_read_byte(state, TDA1004X_OUT_CONF1) & 0x0c) >> 2) {
	case 0:
		fe_params->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		fe_params->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		fe_params->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		fe_params->guard_interval = GUARD_INTERVAL_1_4;
		break;
	}

	
	switch ((tda1004x_read_byte(state, TDA1004X_OUT_CONF1) & 0x60) >> 5) {
	case 0:
		fe_params->hierarchy = HIERARCHY_NONE;
		break;
	case 1:
		fe_params->hierarchy = HIERARCHY_1;
		break;
	case 2:
		fe_params->hierarchy = HIERARCHY_2;
		break;
	case 3:
		fe_params->hierarchy = HIERARCHY_4;
		break;
	}

	return 0;
}

static int tda1004x_read_status(struct dvb_frontend* fe, fe_status_t * fe_status)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int status;
	int cber;
	int vber;

	dprintk("%s\n", __func__);

	
	status = tda1004x_read_byte(state, TDA1004X_STATUS_CD);
	if (status == -1)
		return -EIO;

	
	*fe_status = 0;
	if (status & 4)
		*fe_status |= FE_HAS_SIGNAL;
	if (status & 2)
		*fe_status |= FE_HAS_CARRIER;
	if (status & 8)
		*fe_status |= FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;

	
	
	if (!(*fe_status & FE_HAS_VITERBI)) {
		
		cber = tda1004x_read_byte(state, TDA1004X_CBER_LSB);
		if (cber == -1)
			return -EIO;
		status = tda1004x_read_byte(state, TDA1004X_CBER_MSB);
		if (status == -1)
			return -EIO;
		cber |= (status << 8);
		
		tda1004x_read_byte(state, TDA1004X_CBER_RESET);

		if (cber != 65535)
			*fe_status |= FE_HAS_VITERBI;
	}

	
	
	if ((*fe_status & FE_HAS_VITERBI) && (!(*fe_status & FE_HAS_SYNC))) {
		
		vber = tda1004x_read_byte(state, TDA1004X_VBER_LSB);
		if (vber == -1)
			return -EIO;
		status = tda1004x_read_byte(state, TDA1004X_VBER_MID);
		if (status == -1)
			return -EIO;
		vber |= (status << 8);
		status = tda1004x_read_byte(state, TDA1004X_VBER_MSB);
		if (status == -1)
			return -EIO;
		vber |= (status & 0x0f) << 16;
		
		tda1004x_read_byte(state, TDA1004X_CVBER_LUT);

		
		
		if (vber < 16632)
			*fe_status |= FE_HAS_SYNC;
	}

	
	dprintk("%s: fe_status=0x%x\n", __func__, *fe_status);
	return 0;
}

static int tda1004x_read_signal_strength(struct dvb_frontend* fe, u16 * signal)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int tmp;
	int reg = 0;

	dprintk("%s\n", __func__);

	
	switch (state->demod_type) {
	case TDA1004X_DEMOD_TDA10045:
		reg = TDA10045H_S_AGC;
		break;

	case TDA1004X_DEMOD_TDA10046:
		reg = TDA10046H_AGC_IF_LEVEL;
		break;
	}

	
	tmp = tda1004x_read_byte(state, reg);
	if (tmp < 0)
		return -EIO;

	*signal = (tmp << 8) | tmp;
	dprintk("%s: signal=0x%x\n", __func__, *signal);
	return 0;
}

static int tda1004x_read_snr(struct dvb_frontend* fe, u16 * snr)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int tmp;

	dprintk("%s\n", __func__);

	
	tmp = tda1004x_read_byte(state, TDA1004X_SNR);
	if (tmp < 0)
		return -EIO;
	tmp = 255 - tmp;

	*snr = ((tmp << 8) | tmp);
	dprintk("%s: snr=0x%x\n", __func__, *snr);
	return 0;
}

static int tda1004x_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int tmp;
	int tmp2;
	int counter;

	dprintk("%s\n", __func__);

	
	counter = 0;
	tmp = tda1004x_read_byte(state, TDA1004X_UNCOR);
	if (tmp < 0)
		return -EIO;
	tmp &= 0x7f;
	while (counter++ < 5) {
		tda1004x_write_mask(state, TDA1004X_UNCOR, 0x80, 0);
		tda1004x_write_mask(state, TDA1004X_UNCOR, 0x80, 0);
		tda1004x_write_mask(state, TDA1004X_UNCOR, 0x80, 0);

		tmp2 = tda1004x_read_byte(state, TDA1004X_UNCOR);
		if (tmp2 < 0)
			return -EIO;
		tmp2 &= 0x7f;
		if ((tmp2 < tmp) || (tmp2 == 0))
			break;
	}

	if (tmp != 0x7f)
		*ucblocks = tmp;
	else
		*ucblocks = 0xffffffff;

	dprintk("%s: ucblocks=0x%x\n", __func__, *ucblocks);
	return 0;
}

static int tda1004x_read_ber(struct dvb_frontend* fe, u32* ber)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int tmp;

	dprintk("%s\n", __func__);

	
	tmp = tda1004x_read_byte(state, TDA1004X_CBER_LSB);
	if (tmp < 0)
		return -EIO;
	*ber = tmp << 1;
	tmp = tda1004x_read_byte(state, TDA1004X_CBER_MSB);
	if (tmp < 0)
		return -EIO;
	*ber |= (tmp << 9);
	
	tda1004x_read_byte(state, TDA1004X_CBER_RESET);

	dprintk("%s: ber=0x%x\n", __func__, *ber);
	return 0;
}

static int tda1004x_sleep(struct dvb_frontend* fe)
{
	struct tda1004x_state* state = fe->demodulator_priv;
	int gpio_conf;

	switch (state->demod_type) {
	case TDA1004X_DEMOD_TDA10045:
		tda1004x_write_mask(state, TDA1004X_CONFADC1, 0x10, 0x10);
		break;

	case TDA1004X_DEMOD_TDA10046:
		
		tda1004x_write_byteI(state, TDA10046H_CONF_TRISTATE1, 0xff);
		
		gpio_conf = state->config->gpio_config;
		if (gpio_conf >= TDA10046_GP00_I)
			tda1004x_write_mask(state, TDA10046H_CONF_POLARITY, 0x0f,
							(gpio_conf & 0x0f) ^ 0x0a);

		tda1004x_write_mask(state, TDA1004X_CONFADC2, 0xc0, 0xc0);
		tda1004x_write_mask(state, TDA1004X_CONFC4, 1, 1);
		break;
	}

	return 0;
}

static int tda1004x_i2c_gate_ctrl(struct dvb_frontend* fe, int enable)
{
	struct tda1004x_state* state = fe->demodulator_priv;

	if (enable) {
		return tda1004x_enable_tuner_i2c(state);
	} else {
		return tda1004x_disable_tuner_i2c(state);
	}
}

static int tda1004x_get_tune_settings(struct dvb_frontend* fe, struct dvb_frontend_tune_settings* fesettings)
{
	fesettings->min_delay_ms = 800;
	
	fesettings->step_size = 0;
	fesettings->max_drift = 0;
	return 0;
}

static void tda1004x_release(struct dvb_frontend* fe)
{
	struct tda1004x_state *state = fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops tda10045_ops = {
	.delsys = { SYS_DVBT },
	.info = {
		.name = "Philips TDA10045H DVB-T",
		.frequency_min = 51000000,
		.frequency_max = 858000000,
		.frequency_stepsize = 166667,
		.caps =
		    FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		    FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		    FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
		    FE_CAN_TRANSMISSION_MODE_AUTO | FE_CAN_GUARD_INTERVAL_AUTO
	},

	.release = tda1004x_release,

	.init = tda10045_init,
	.sleep = tda1004x_sleep,
	.write = tda1004x_write,
	.i2c_gate_ctrl = tda1004x_i2c_gate_ctrl,

	.set_frontend = tda1004x_set_fe,
	.get_frontend = tda1004x_get_fe,
	.get_tune_settings = tda1004x_get_tune_settings,

	.read_status = tda1004x_read_status,
	.read_ber = tda1004x_read_ber,
	.read_signal_strength = tda1004x_read_signal_strength,
	.read_snr = tda1004x_read_snr,
	.read_ucblocks = tda1004x_read_ucblocks,
};

struct dvb_frontend* tda10045_attach(const struct tda1004x_config* config,
				     struct i2c_adapter* i2c)
{
	struct tda1004x_state *state;
	int id;

	
	state = kzalloc(sizeof(struct tda1004x_state), GFP_KERNEL);
	if (!state) {
		printk(KERN_ERR "Can't allocate memory for tda10045 state\n");
		return NULL;
	}

	
	state->config = config;
	state->i2c = i2c;
	state->demod_type = TDA1004X_DEMOD_TDA10045;

	
	id = tda1004x_read_byte(state, TDA1004X_CHIPID);
	if (id < 0) {
		printk(KERN_ERR "tda10045: chip is not answering. Giving up.\n");
		kfree(state);
		return NULL;
	}

	if (id != 0x25) {
		printk(KERN_ERR "Invalid tda1004x ID = 0x%02x. Can't proceed\n", id);
		kfree(state);
		return NULL;
	}

	
	memcpy(&state->frontend.ops, &tda10045_ops, sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;
	return &state->frontend;
}

static struct dvb_frontend_ops tda10046_ops = {
	.delsys = { SYS_DVBT },
	.info = {
		.name = "Philips TDA10046H DVB-T",
		.frequency_min = 51000000,
		.frequency_max = 858000000,
		.frequency_stepsize = 166667,
		.caps =
		    FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		    FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		    FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
		    FE_CAN_TRANSMISSION_MODE_AUTO | FE_CAN_GUARD_INTERVAL_AUTO
	},

	.release = tda1004x_release,

	.init = tda10046_init,
	.sleep = tda1004x_sleep,
	.write = tda1004x_write,
	.i2c_gate_ctrl = tda1004x_i2c_gate_ctrl,

	.set_frontend = tda1004x_set_fe,
	.get_frontend = tda1004x_get_fe,
	.get_tune_settings = tda1004x_get_tune_settings,

	.read_status = tda1004x_read_status,
	.read_ber = tda1004x_read_ber,
	.read_signal_strength = tda1004x_read_signal_strength,
	.read_snr = tda1004x_read_snr,
	.read_ucblocks = tda1004x_read_ucblocks,
};

struct dvb_frontend* tda10046_attach(const struct tda1004x_config* config,
				     struct i2c_adapter* i2c)
{
	struct tda1004x_state *state;
	int id;

	
	state = kzalloc(sizeof(struct tda1004x_state), GFP_KERNEL);
	if (!state) {
		printk(KERN_ERR "Can't allocate memory for tda10046 state\n");
		return NULL;
	}

	
	state->config = config;
	state->i2c = i2c;
	state->demod_type = TDA1004X_DEMOD_TDA10046;

	
	id = tda1004x_read_byte(state, TDA1004X_CHIPID);
	if (id < 0) {
		printk(KERN_ERR "tda10046: chip is not answering. Giving up.\n");
		kfree(state);
		return NULL;
	}
	if (id != 0x46) {
		printk(KERN_ERR "Invalid tda1004x ID = 0x%02x. Can't proceed\n", id);
		kfree(state);
		return NULL;
	}

	
	memcpy(&state->frontend.ops, &tda10046_ops, sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;
	return &state->frontend;
}

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");

MODULE_DESCRIPTION("Philips TDA10045H & TDA10046H DVB-T Demodulator");
MODULE_AUTHOR("Andrew de Quincey & Robert Schlabbach");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(tda10045_attach);
EXPORT_SYMBOL(tda10046_attach);
