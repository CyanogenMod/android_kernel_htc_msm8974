/*
 * Driver for PowerMac AWACS onboard soundchips
 * Copyright (c) 2001 by Takashi Iwai <tiwai@suse.de>
 *   based on dmasound.c.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#ifndef __AWACS_H
#define __AWACS_H


struct awacs_regs {
    unsigned	control;	
    unsigned	pad0[3];
    unsigned	codec_ctrl;	
    unsigned	pad1[3];
    unsigned	codec_stat;	
    unsigned	pad2[3];
    unsigned	clip_count;	
    unsigned	pad3[3];
    unsigned	byteswap;	
};


#define MASK_ISFSEL	(0xf)		
#define MASK_OSFSEL	(0xf << 4)	
#define MASK_RATE	(0x7 << 8)	
#define MASK_CNTLERR	(0x1 << 11)	
#define MASK_PORTCHG	(0x1 << 12)	
#define MASK_IEE	(0x1 << 13)	
#define MASK_IEPC	(0x1 << 14)	
#define MASK_SSFSEL	(0x3 << 15)	

#define MASK_NEWECMD	(0x1 << 24)	
#define MASK_EMODESEL	(0x3 << 22)	
#define MASK_EXMODEADDR	(0x3ff << 12)	
#define MASK_EXMODEDATA	(0xfff)		

#define MASK_ADDR0	(0x0 << 12)	
#define MASK_ADDR_MUX	MASK_ADDR0	
#define MASK_ADDR_GAIN	MASK_ADDR0

#define MASK_ADDR1	(0x1 << 12)	
#define MASK_ADDR_MUTE	MASK_ADDR1
#define MASK_ADDR_RATE	MASK_ADDR1

#define MASK_ADDR2	(0x2 << 12)	
#define MASK_ADDR_VOLA	MASK_ADDR2	
#define MASK_ADDR_VOLHD MASK_ADDR2

#define MASK_ADDR4	(0x4 << 12)	
#define MASK_ADDR_VOLC	MASK_ADDR4	
#define MASK_ADDR_VOLSPK MASK_ADDR4

#define MASK_ADDR5	(0x5 << 12)	
#define MASK_ADDR6	(0x6 << 12)	
#define MASK_ADDR7	(0x7 << 12)	

#define MASK_GAINRIGHT	(0xf)		
#define MASK_GAINLEFT	(0xf << 4)	
#define MASK_GAINLINE	(0x1 << 8)	
#define MASK_GAINMIC	(0x0 << 8)	
#define MASK_MUX_CD	(0x1 << 9)	
#define MASK_MUX_MIC	(0x1 << 10)	
#define MASK_MUX_AUDIN	(0x1 << 11)	
#define MASK_MUX_LINE	MASK_MUX_AUDIN
#define SHIFT_GAINLINE	8
#define SHIFT_MUX_CD	9
#define SHIFT_MUX_MIC	10
#define SHIFT_MUX_LINE	11

#define GAINRIGHT(x)	((x) & MASK_GAINRIGHT)
#define GAINLEFT(x)	(((x) << 4) & MASK_GAINLEFT)

#define MASK_ADDR1RES1	(0x3)		
#define MASK_RECALIBRATE (0x1 << 2)	
#define MASK_SAMPLERATE	(0x7 << 3)	
#define MASK_LOOPTHRU	(0x1 << 6)	
#define SHIFT_LOOPTHRU	6
#define MASK_CMUTE	(0x1 << 7)	
#define MASK_SPKMUTE	MASK_CMUTE
#define SHIFT_SPKMUTE	7
#define MASK_ADDR1RES2	(0x1 << 8)	
#define MASK_AMUTE	(0x1 << 9)	
#define MASK_HDMUTE	MASK_AMUTE
#define SHIFT_HDMUTE	9
#define MASK_PAROUT	(0x3 << 10)	
#define MASK_PAROUT0	(0x1 << 10)	
#define MASK_PAROUT1	(0x1 << 11)	
#define SHIFT_PAROUT	10
#define SHIFT_PAROUT0	10
#define SHIFT_PAROUT1	11

#define SAMPLERATE_48000	(0x0 << 3)	
#define SAMPLERATE_32000	(0x1 << 3)	
#define SAMPLERATE_24000	(0x2 << 3)	
#define SAMPLERATE_19200	(0x3 << 3)	
#define SAMPLERATE_16000	(0x4 << 3)	
#define SAMPLERATE_12000	(0x5 << 3)	
#define SAMPLERATE_9600		(0x6 << 3)	
#define SAMPLERATE_8000		(0x7 << 3)	

#define MASK_OUTVOLRIGHT (0xf)		
#define MASK_ADDR2RES1	(0x2 << 4)	
#define MASK_ADDR4RES1	MASK_ADDR2RES1
#define MASK_OUTVOLLEFT	(0xf << 6)	
#define MASK_ADDR2RES2	(0x2 << 10)	
#define MASK_ADDR4RES2	MASK_ADDR2RES2

#define VOLRIGHT(x)	(((~(x)) & MASK_OUTVOLRIGHT))
#define VOLLEFT(x)	(((~(x)) << 6) & MASK_OUTVOLLEFT)

#define MASK_MIC_BOOST  (0x4)		
#define SHIFT_MIC_BOOST	2

#define MASK_EXTEND	(0x1 << 23)	
#define MASK_VALID	(0x1 << 22)	
#define MASK_OFLEFT	(0x1 << 21)	
#define MASK_OFRIGHT	(0x1 << 20)	
#define MASK_ERRCODE	(0xf << 16)	
#define MASK_REVISION	(0xf << 12)	
#define MASK_MFGID	(0xf << 8)	
#define MASK_CODSTATRES	(0xf << 4)	
#define MASK_INSENSE	(0xf)		
#define MASK_HDPCONN		8	
#define MASK_LOCONN		4	
#define MASK_LICONN		2	
#define MASK_MICCONN		1	
#define MASK_LICONN_IMAC	8	
#define MASK_HDPRCONN_IMAC	4	
#define MASK_HDPLCONN_IMAC	2	
#define MASK_LOCONN_IMAC	1	

#define MASK_CLIPLEFT	(0xff << 7)	
#define MASK_CLIPRIGHT	(0xff)		

#define MASK_CSERR	(0x1 << 7)	
#define MASK_EOI	(0x1 << 6)	
#define MASK_CSUNUSED	(0x1f << 1)	
#define MASK_WAIT	(0x1)		

#define RATE_48000	(0x0 << 8)	
#define RATE_44100	(0x0 << 8)	
#define RATE_32000	(0x1 << 8)	
#define RATE_29400	(0x1 << 8)	
#define RATE_24000	(0x2 << 8)	
#define RATE_22050	(0x2 << 8)	
#define RATE_19200	(0x3 << 8)	
#define RATE_17640	(0x3 << 8)	
#define RATE_16000	(0x4 << 8)	
#define RATE_14700	(0x4 << 8)	
#define RATE_12000	(0x5 << 8)	
#define RATE_11025	(0x5 << 8)	
#define RATE_9600	(0x6 << 8)	
#define RATE_8820	(0x6 << 8)	
#define RATE_8000	(0x7 << 8)	
#define RATE_7350	(0x7 << 8)	

#define RATE_LOW	1	


#endif 
