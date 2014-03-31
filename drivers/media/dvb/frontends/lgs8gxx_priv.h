/*
 *    Support for Legend Silicon GB20600 (a.k.a DMB-TH) demodulator
 *    LGS8913, LGS8GL5, LGS8G75
 *    experimental support LGS8G42, LGS8G52
 *
 *    Copyright (C) 2007-2009 David T.L. Wong <davidtlwong@gmail.com>
 *    Copyright (C) 2008 Sirius International (Hong Kong) Limited
 *    Timothy Lee <timothy.lee@siriushk.com> (for initial work on LGS8GL5)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef LGS8913_PRIV_H
#define LGS8913_PRIV_H

struct lgs8gxx_state {
	struct i2c_adapter *i2c;
	
	const struct lgs8gxx_config *config;
	struct dvb_frontend frontend;
	u16 curr_gi; 
};

#define SC_MASK		0x1C	
#define SC_QAM64	0x10	
#define SC_QAM32	0x0C	
#define SC_QAM16	0x08	
#define SC_QAM4NR	0x04	
#define SC_QAM4		0x00	

#define LGS_FEC_MASK	0x03	
#define LGS_FEC_0_4	0x00	
#define LGS_FEC_0_6	0x01	
#define LGS_FEC_0_8	0x02	

#define TIM_MASK	  0x20	
#define TIM_LONG	  0x20	
#define TIM_MIDDLE     0x00   

#define CF_MASK	0x80	
#define CF_EN	0x80	

#define GI_MASK	0x03	
#define GI_420	0x00	
#define GI_595	0x01	
#define GI_945	0x02	


#define TS_PARALLEL	0x00	
#define TS_SERIAL	0x01	
#define TS_CLK_NORMAL		0x00	
#define TS_CLK_INVERTED		0x02	
#define TS_CLK_GATED		0x00	
#define TS_CLK_FREERUN		0x04	


#endif
