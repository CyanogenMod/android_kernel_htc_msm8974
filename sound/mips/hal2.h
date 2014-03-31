#ifndef __HAL2_H
#define __HAL2_H

/*
 *  Driver for HAL2 sound processors
 *  Copyright (c) 1999 Ulf Carlsson <ulfc@bun.falkenberg.se>
 *  Copyright (c) 2001, 2002, 2003 Ladislav Michl <ladis@linux-mips.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/types.h>


#define H2_ISR_TSTATUS		0x01	
#define H2_ISR_USTATUS		0x02	
#define H2_ISR_QUAD_MODE	0x04	
#define H2_ISR_GLOBAL_RESET_N	0x08	
#define H2_ISR_CODEC_RESET_N	0x10	


#define H2_REV_AUDIO_PRESENT	0x8000	
#define H2_REV_BOARD_M		0x7000	
#define H2_REV_MAJOR_CHIP_M	0x00F0	
#define H2_REV_MINOR_CHIP_M	0x000F	



#define H2_IAR_TYPE_M		0xF000	
					
					
					
					
					
#define H2_IAR_NUM_M		0x0F00	
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					
#define H2_IAR_ACCESS_SELECT	0x0080	
#define H2_IAR_PARAM		0x000C	
#define H2_IAR_RB_INDEX_M	0x0003	
					
					
					
					

#define H2I_RELAY_C		0x9100
#define H2I_RELAY_C_STATE	0x01		


#define H2I_DMA_PORT_EN		0x9104
#define H2I_DMA_PORT_EN_SY_IN	0x01		
#define H2I_DMA_PORT_EN_AESRX	0x02		
#define H2I_DMA_PORT_EN_AESTX	0x04		
#define H2I_DMA_PORT_EN_CODECTX	0x08		
#define H2I_DMA_PORT_EN_CODECR	0x10		

#define H2I_DMA_END		0x9108 		
#define H2I_DMA_END_SY_IN	0x01		
#define H2I_DMA_END_AESRX	0x02		
#define H2I_DMA_END_AESTX	0x04		
#define H2I_DMA_END_CODECTX	0x08		
#define H2I_DMA_END_CODECR	0x10		
						

#define H2I_DMA_DRV		0x910C  	

#define H2I_SYNTH_C		0x1104		

#define H2I_AESRX_C		0x1204	 	

#define H2I_C_TS_EN		0x20		
#define H2I_C_TS_FRMT		0x40		
#define H2I_C_NAUDIO		0x80		


#define H2I_AESTX_C		0x1304		
#define H2I_AESTX_C_CLKID_SHIFT	3		
#define H2I_AESTX_C_CLKID_M	0x18
#define H2I_AESTX_C_DATAT_SHIFT	8		
#define H2I_AESTX_C_DATAT_M	0x300


#define H2I_DAC_C1		0x1404 		
#define H2I_DAC_C2		0x1408		
#define H2I_ADC_C1		0x1504 		
#define H2I_ADC_C2		0x1508		


#define H2I_C1_DMA_SHIFT	0		
#define H2I_C1_DMA_M		0x7
#define H2I_C1_CLKID_SHIFT	3		
#define H2I_C1_CLKID_M		0x18
#define H2I_C1_DATAT_SHIFT	8		
#define H2I_C1_DATAT_M		0x300


#define H2I_C2_R_GAIN_SHIFT	0		
#define H2I_C2_R_GAIN_M		0xf
#define H2I_C2_L_GAIN_SHIFT	4		
#define H2I_C2_L_GAIN_M		0xf0
#define H2I_C2_R_SEL		0x100		
#define H2I_C2_L_SEL		0x200		
#define H2I_C2_MUTE		0x400		
#define H2I_C2_DO1		0x00010000	
#define H2I_C2_DO2		0x00020000	
#define H2I_C2_R_ATT_SHIFT	18		
#define H2I_C2_R_ATT_M		0x007c0000	
#define H2I_C2_L_ATT_SHIFT	23		
#define H2I_C2_L_ATT_M		0x0f800000	

#define H2I_SYNTH_MAP_C		0x1104		


#define H2I_BRES1_C1		0x2104
#define H2I_BRES2_C1		0x2204
#define H2I_BRES3_C1		0x2304

#define H2I_BRES_C1_SHIFT	0		
#define H2I_BRES_C1_M		0x03


#define H2I_BRES1_C2		0x2108
#define H2I_BRES2_C2		0x2208
#define H2I_BRES3_C2		0x2308

#define H2I_BRES_C2_INC_SHIFT	0		
#define H2I_BRES_C2_INC_M	0xffff
#define H2I_BRES_C2_MOD_SHIFT	16		
#define H2I_BRES_C2_MOD_M	0xffff0000	


#define H2I_UTIME		0x3104
#define H2I_UTIME_0_LD		0xffff		
#define H2I_UTIME_1_LD0		0x0f		
#define H2I_UTIME_1_LD1		0xf0		
#define H2I_UTIME_2_LD		0xffff		
#define H2I_UTIME_3_LD		0xffff		

struct hal2_ctl_regs {
	u32 _unused0[4];
	u32 isr;		
	u32 _unused1[3];
	u32 rev;		
	u32 _unused2[3];
	u32 iar;		
	u32 _unused3[3];
	u32 idr0;		
	u32 _unused4[3];
	u32 idr1;		
	u32 _unused5[3];
	u32 idr2;		
	u32 _unused6[3];
	u32 idr3;		
};

struct hal2_aes_regs {
	u32 rx_stat[2];	
	u32 rx_cr[2];		
	u32 rx_ud[4];		
	u32 rx_st[24];		

	u32 tx_stat[1];	
	u32 tx_cr[3];		
	u32 tx_ud[4];		
	u32 tx_st[24];		
};

struct hal2_vol_regs {
	u32 right;		
	u32 left;		
};

struct hal2_syn_regs {
	u32 _unused0[2];
	u32 page;		
	u32 regsel;		
	u32 dlow;		
	u32 dhigh;		
	u32 irq;		
	u32 dram;		
};

#endif	
