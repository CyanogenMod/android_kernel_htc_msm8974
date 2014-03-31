/*
 *  mxl111sf-phy.c - driver for the MaxLinear MXL111SF
 *
 *  Copyright (C) 2010 Michael Krufky <mkrufky@kernellabs.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "mxl111sf-phy.h"
#include "mxl111sf-reg.h"

int mxl111sf_init_tuner_demod(struct mxl111sf_state *state)
{
	struct mxl111sf_reg_ctrl_info mxl_111_overwrite_default[] = {
		{0x07, 0xff, 0x0c},
		{0x58, 0xff, 0x9d},
		{0x09, 0xff, 0x00},
		{0x06, 0xff, 0x06},
		{0xc8, 0xff, 0x40}, 
		{0x8d, 0x01, 0x01}, 
		{0x32, 0xff, 0xac}, 
		{0x42, 0xff, 0x43}, 
		{0x74, 0xff, 0xc4}, 
		{0x71, 0xff, 0xe6}, 
		{0x83, 0xff, 0x64}, 
		{0x85, 0xff, 0x64}, 
		{0x88, 0xff, 0xf0}, 
		{0x6f, 0xf0, 0xb0}, 
		{0x00, 0xff, 0x01}, 
		{0x81, 0xff, 0x11}, 
		{0xf4, 0xff, 0x07}, 
		{0xd4, 0x1f, 0x0f}, 
		{0xd6, 0xff, 0x0c}, 
		{0x00, 0xff, 0x00}, 
		{0,    0,    0}
	};

	mxl_debug("()");

	return mxl111sf_ctrl_program_regs(state, mxl_111_overwrite_default);
}

int mxl1x1sf_soft_reset(struct mxl111sf_state *state)
{
	int ret;
	mxl_debug("()");

	ret = mxl111sf_write_reg(state, 0xff, 0x00); 
	if (mxl_fail(ret))
		goto fail;
	ret = mxl111sf_write_reg(state, 0x02, 0x01); 
	mxl_fail(ret);
fail:
	return ret;
}

int mxl1x1sf_set_device_mode(struct mxl111sf_state *state, int mode)
{
	int ret;

	mxl_debug("(%s)", MXL_SOC_MODE == mode ?
		"MXL_SOC_MODE" : "MXL_TUNER_MODE");

	
	ret = mxl111sf_write_reg(state, 0x03,
				 MXL_SOC_MODE == mode ? 0x01 : 0x00);
	if (mxl_fail(ret))
		goto fail;

	ret = mxl111sf_write_reg_mask(state,
				      0x7d, 0x40, MXL_SOC_MODE == mode ?
				      0x00 : 
				      0x40); 
	if (mxl_fail(ret))
		goto fail;

	state->device_mode = mode;
fail:
	return ret;
}

int mxl1x1sf_top_master_ctrl(struct mxl111sf_state *state, int onoff)
{
	mxl_debug("(%d)", onoff);

	return mxl111sf_write_reg(state, 0x01, onoff ? 0x01 : 0x00);
}

int mxl111sf_disable_656_port(struct mxl111sf_state *state)
{
	mxl_debug("()");

	return mxl111sf_write_reg_mask(state, 0x12, 0x04, 0x00);
}

int mxl111sf_enable_usb_output(struct mxl111sf_state *state)
{
	mxl_debug("()");

	return mxl111sf_write_reg_mask(state, 0x17, 0x40, 0x00);
}

int mxl111sf_config_mpeg_in(struct mxl111sf_state *state,
			    unsigned int parallel_serial,
			    unsigned int msb_lsb_1st,
			    unsigned int clock_phase,
			    unsigned int mpeg_valid_pol,
			    unsigned int mpeg_sync_pol)
{
	int ret;
	u8 mode, tmp;

	mxl_debug("(%u,%u,%u,%u,%u)", parallel_serial, msb_lsb_1st,
		  clock_phase, mpeg_valid_pol, mpeg_sync_pol);

	
	ret = mxl111sf_write_reg(state, V6_PIN_MUX_MODE_REG, V6_ENABLE_PIN_MUX);
	mxl_fail(ret);

	
	mxl111sf_read_reg(state, V6_MPEG_IN_CLK_INV_REG, &mode);

	if (clock_phase == TSIF_NORMAL)
		mode &= ~V6_INVERTED_CLK_PHASE;
	else
		mode |= V6_INVERTED_CLK_PHASE;

	ret = mxl111sf_write_reg(state, V6_MPEG_IN_CLK_INV_REG, mode);
	mxl_fail(ret);

	ret = mxl111sf_read_reg(state, V6_MPEG_IN_CTRL_REG, &mode);
	mxl_fail(ret);

	
	if (parallel_serial == TSIF_INPUT_PARALLEL) {
		
		mode &= ~V6_MPEG_IN_DATA_SERIAL;

		
		mode |= V6_MPEG_IN_DATA_PARALLEL;
	} else {
		
		mode &= ~V6_MPEG_IN_DATA_PARALLEL;

		
		mode |= V6_MPEG_IN_DATA_SERIAL;

		ret = mxl111sf_read_reg(state,
					V6_MPEG_INOUT_BIT_ORDER_CTRL_REG,
					&tmp);
		mxl_fail(ret);

		if (msb_lsb_1st == MPEG_SER_MSB_FIRST_ENABLED)
			tmp |= V6_MPEG_SER_MSB_FIRST;
		else
			tmp &= ~V6_MPEG_SER_MSB_FIRST;

		ret = mxl111sf_write_reg(state,
					 V6_MPEG_INOUT_BIT_ORDER_CTRL_REG,
					 tmp);
		mxl_fail(ret);
	}

	
	if (mpeg_sync_pol == TSIF_NORMAL)
		mode &= ~V6_INVERTED_MPEG_SYNC;
	else
		mode |= V6_INVERTED_MPEG_SYNC;

	
	if (mpeg_valid_pol == 0)
		mode &= ~V6_INVERTED_MPEG_VALID;
	else
		mode |= V6_INVERTED_MPEG_VALID;

	ret = mxl111sf_write_reg(state, V6_MPEG_IN_CTRL_REG, mode);
	mxl_fail(ret);

	return ret;
}

int mxl111sf_init_i2s_port(struct mxl111sf_state *state, u8 sample_size)
{
	static struct mxl111sf_reg_ctrl_info init_i2s[] = {
		{0x1b, 0xff, 0x1e}, 
		{0x15, 0x60, 0x60}, 
		{0x17, 0xe0, 0x20}, 
#if 0
		{0x12, 0x01, 0x00}, 
#endif
		{0x00, 0xff, 0x02}, 
		{0x26, 0x0d, 0x0d}, 
		{0x00, 0xff, 0x00},
		{0,    0,    0}
	};
	int ret;

	mxl_debug("(0x%02x)", sample_size);

	ret = mxl111sf_ctrl_program_regs(state, init_i2s);
	if (mxl_fail(ret))
		goto fail;

	ret = mxl111sf_write_reg(state, V6_I2S_NUM_SAMPLES_REG, sample_size);
	mxl_fail(ret);
fail:
	return ret;
}

int mxl111sf_disable_i2s_port(struct mxl111sf_state *state)
{
	static struct mxl111sf_reg_ctrl_info disable_i2s[] = {
		{0x15, 0x40, 0x00},
		{0,    0,    0}
	};

	mxl_debug("()");

	return mxl111sf_ctrl_program_regs(state, disable_i2s);
}

int mxl111sf_config_i2s(struct mxl111sf_state *state,
			u8 msb_start_pos, u8 data_width)
{
	int ret;
	u8 tmp;

	mxl_debug("(0x%02x, 0x%02x)", msb_start_pos, data_width);

	ret = mxl111sf_read_reg(state, V6_I2S_STREAM_START_BIT_REG, &tmp);
	if (mxl_fail(ret))
		goto fail;

	tmp &= 0xe0;
	tmp |= msb_start_pos;
	ret = mxl111sf_write_reg(state, V6_I2S_STREAM_START_BIT_REG, tmp);
	if (mxl_fail(ret))
		goto fail;

	ret = mxl111sf_read_reg(state, V6_I2S_STREAM_END_BIT_REG, &tmp);
	if (mxl_fail(ret))
		goto fail;

	tmp &= 0xe0;
	tmp |= data_width;
	ret = mxl111sf_write_reg(state, V6_I2S_STREAM_END_BIT_REG, tmp);
	mxl_fail(ret);
fail:
	return ret;
}

int mxl111sf_config_spi(struct mxl111sf_state *state, int onoff)
{
	u8 val;
	int ret;

	mxl_debug("(%d)", onoff);

	ret = mxl111sf_write_reg(state, 0x00, 0x02);
	if (mxl_fail(ret))
		goto fail;

	ret = mxl111sf_read_reg(state, V8_SPI_MODE_REG, &val);
	if (mxl_fail(ret))
		goto fail;

	if (onoff)
		val |= 0x04;
	else
		val &= ~0x04;

	ret = mxl111sf_write_reg(state, V8_SPI_MODE_REG, val);
	if (mxl_fail(ret))
		goto fail;

	ret = mxl111sf_write_reg(state, 0x00, 0x00);
	mxl_fail(ret);
fail:
	return ret;
}

int mxl111sf_idac_config(struct mxl111sf_state *state,
			 u8 control_mode, u8 current_setting,
			 u8 current_value, u8 hysteresis_value)
{
	int ret;
	u8 val;
	
	val = current_value;

	if (control_mode == IDAC_MANUAL_CONTROL) {
		
		val |= IDAC_MANUAL_CONTROL_BIT_MASK;

		if (current_setting == IDAC_CURRENT_SINKING_ENABLE)
			
			val |= IDAC_CURRENT_SINKING_BIT_MASK;
		else
			
			val &= ~IDAC_CURRENT_SINKING_BIT_MASK;
	} else {
		
		val &= ~IDAC_MANUAL_CONTROL_BIT_MASK;

		
		ret = mxl111sf_write_reg(state, V6_IDAC_HYSTERESIS_REG,
					 (hysteresis_value & 0x3F));
		mxl_fail(ret);
	}

	ret = mxl111sf_write_reg(state, V6_IDAC_SETTINGS_REG, val);
	mxl_fail(ret);

	return ret;
}

