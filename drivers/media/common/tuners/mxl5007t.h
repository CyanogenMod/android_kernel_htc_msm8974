/*
 *  mxl5007t.h - driver for the MaxLinear MxL5007T silicon tuner
 *
 *  Copyright (C) 2008 Michael Krufky <mkrufky@linuxtv.org>
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

#ifndef __MXL5007T_H__
#define __MXL5007T_H__

#include "dvb_frontend.h"


enum mxl5007t_if_freq {
	MxL_IF_4_MHZ,      
	MxL_IF_4_5_MHZ,    
	MxL_IF_4_57_MHZ,   
	MxL_IF_5_MHZ,      
	MxL_IF_5_38_MHZ,   
	MxL_IF_6_MHZ,      
	MxL_IF_6_28_MHZ,   
	MxL_IF_9_1915_MHZ, 
	MxL_IF_35_25_MHZ,  
	MxL_IF_36_15_MHZ,  
	MxL_IF_44_MHZ,     
};

enum mxl5007t_xtal_freq {
	MxL_XTAL_16_MHZ,      
	MxL_XTAL_20_MHZ,      
	MxL_XTAL_20_25_MHZ,   
	MxL_XTAL_20_48_MHZ,   
	MxL_XTAL_24_MHZ,      
	MxL_XTAL_25_MHZ,      
	MxL_XTAL_25_14_MHZ,   
	MxL_XTAL_27_MHZ,      
	MxL_XTAL_28_8_MHZ,    
	MxL_XTAL_32_MHZ,      
	MxL_XTAL_40_MHZ,      
	MxL_XTAL_44_MHZ,      
	MxL_XTAL_48_MHZ,      
	MxL_XTAL_49_3811_MHZ, 
};

enum mxl5007t_clkout_amp {
	MxL_CLKOUT_AMP_0_94V = 0,
	MxL_CLKOUT_AMP_0_53V = 1,
	MxL_CLKOUT_AMP_0_37V = 2,
	MxL_CLKOUT_AMP_0_28V = 3,
	MxL_CLKOUT_AMP_0_23V = 4,
	MxL_CLKOUT_AMP_0_20V = 5,
	MxL_CLKOUT_AMP_0_17V = 6,
	MxL_CLKOUT_AMP_0_15V = 7,
};

struct mxl5007t_config {
	s32 if_diff_out_level;
	enum mxl5007t_clkout_amp clk_out_amp;
	enum mxl5007t_xtal_freq xtal_freq_hz;
	enum mxl5007t_if_freq if_freq_hz;
	unsigned int invert_if:1;
	unsigned int loop_thru_enable:1;
	unsigned int clk_out_enable:1;
};

#if defined(CONFIG_MEDIA_TUNER_MXL5007T) || (defined(CONFIG_MEDIA_TUNER_MXL5007T_MODULE) && defined(MODULE))
extern struct dvb_frontend *mxl5007t_attach(struct dvb_frontend *fe,
					    struct i2c_adapter *i2c, u8 addr,
					    struct mxl5007t_config *cfg);
#else
static inline struct dvb_frontend *mxl5007t_attach(struct dvb_frontend *fe,
						   struct i2c_adapter *i2c,
						   u8 addr,
						   struct mxl5007t_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif 


