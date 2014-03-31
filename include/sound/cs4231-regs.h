#ifndef __SOUND_CS4231_REGS_H
#define __SOUND_CS4231_REGS_H

/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Definitions for CS4231 & InterWave chips & compatible chips registers
 *
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
 *
 */


#define CS4231P(x)		(c_d_c_CS4231##x)

#define c_d_c_CS4231REGSEL	0
#define c_d_c_CS4231REG		1
#define c_d_c_CS4231STATUS	2
#define c_d_c_CS4231PIO		3


#define CS4231_LEFT_INPUT	0x00	
#define CS4231_RIGHT_INPUT	0x01	
#define CS4231_AUX1_LEFT_INPUT	0x02	
#define CS4231_AUX1_RIGHT_INPUT	0x03	
#define CS4231_AUX2_LEFT_INPUT	0x04	
#define CS4231_AUX2_RIGHT_INPUT	0x05	
#define CS4231_LEFT_OUTPUT	0x06	
#define CS4231_RIGHT_OUTPUT	0x07	
#define CS4231_PLAYBK_FORMAT	0x08	
#define CS4231_IFACE_CTRL	0x09	
#define CS4231_PIN_CTRL		0x0a	
#define CS4231_TEST_INIT	0x0b	
#define CS4231_MISC_INFO	0x0c	
#define CS4231_LOOPBACK		0x0d	
#define CS4231_PLY_UPR_CNT	0x0e	
#define CS4231_PLY_LWR_CNT	0x0f	
#define CS4231_ALT_FEATURE_1	0x10	
#define AD1845_AF1_MIC_LEFT	0x10	
#define CS4231_ALT_FEATURE_2	0x11	
#define AD1845_AF2_MIC_RIGHT	0x11	
#define CS4231_LEFT_LINE_IN	0x12	
#define CS4231_RIGHT_LINE_IN	0x13	
#define CS4231_TIMER_LOW	0x14	
#define CS4231_TIMER_HIGH	0x15	
#define CS4231_LEFT_MIC_INPUT	0x16	
#define AD1845_UPR_FREQ_SEL	0x16	
#define CS4231_RIGHT_MIC_INPUT	0x17	
#define AD1845_LWR_FREQ_SEL	0x17	
#define CS4236_EXT_REG		0x17	
#define CS4231_IRQ_STATUS	0x18	
#define CS4231_LINE_LEFT_OUTPUT	0x19	
#define CS4231_VERSION		0x19	
#define CS4231_MONO_CTRL	0x1a	
#define CS4231_LINE_RIGHT_OUTPUT 0x1b	
#define AD1845_PWR_DOWN		0x1b	
#define CS4235_LEFT_MASTER	0x1b	
#define CS4231_REC_FORMAT	0x1c	
#define AD1845_CLOCK		0x1d	
#define CS4235_RIGHT_MASTER	0x1d	
#define CS4231_REC_UPR_CNT	0x1e	
#define CS4231_REC_LWR_CNT	0x1f	


#define CS4231_INIT		0x80	
#define CS4231_MCE		0x40	
#define CS4231_TRD		0x20	


#define CS4231_GLOBALIRQ	0x01	


#define CS4231_PLAYBACK_IRQ	0x10
#define CS4231_RECORD_IRQ	0x20
#define CS4231_TIMER_IRQ	0x40
#define CS4231_ALL_IRQS		0x70
#define CS4231_REC_UNDERRUN	0x08
#define CS4231_REC_OVERRUN	0x04
#define CS4231_PLY_OVERRUN	0x02
#define CS4231_PLY_UNDERRUN	0x01


#define CS4231_ENABLE_MIC_GAIN	0x20

#define CS4231_MIXS_LINE	0x00
#define CS4231_MIXS_AUX1	0x40
#define CS4231_MIXS_MIC		0x80
#define CS4231_MIXS_ALL		0xc0


#define CS4231_LINEAR_8		0x00	
#define CS4231_ALAW_8		0x60	
#define CS4231_ULAW_8		0x20	
#define CS4231_LINEAR_16	0x40	
#define CS4231_LINEAR_16_BIG	0xc0	
#define CS4231_ADPCM_16		0xa0	
#define CS4231_STEREO		0x10	
#define CS4231_XTAL1		0x00	
#define CS4231_XTAL2		0x01	


#define CS4231_RECORD_PIO	0x80	
#define CS4231_PLAYBACK_PIO	0x40	
#define CS4231_CALIB_MODE	0x18	
#define CS4231_AUTOCALIB	0x08	
#define CS4231_SINGLE_DMA	0x04	
#define CS4231_RECORD_ENABLE	0x02	
#define CS4231_PLAYBACK_ENABLE	0x01	


#define CS4231_IRQ_ENABLE	0x02	
#define CS4231_XCTL1		0x40	
#define CS4231_XCTL0		0x80	


#define CS4231_CALIB_IN_PROGRESS 0x20	
#define CS4231_DMA_REQUEST	0x10	


#define CS4231_MODE2		0x40	
#define CS4231_IW_MODE3		0x6c	
#define CS4231_4236_MODE3	0xe0	


#define	CS4231_DACZ		0x01	
#define CS4231_TIMER_ENABLE	0x40	
#define CS4231_OLB		0x80	


#define CS4236_REG(i23val)	(((i23val << 2) & 0x10) | ((i23val >> 4) & 0x0f))
#define CS4236_I23VAL(reg)	((((reg)&0xf) << 4) | (((reg)&0x10) >> 2) | 0x8)

#define CS4236_LEFT_LINE	0x08	
#define CS4236_RIGHT_LINE	0x18	
#define CS4236_LEFT_MIC		0x28	
#define CS4236_RIGHT_MIC	0x38	
#define CS4236_LEFT_MIX_CTRL	0x48	
#define CS4236_RIGHT_MIX_CTRL	0x58	
#define CS4236_LEFT_FM		0x68	
#define CS4236_RIGHT_FM		0x78	
#define CS4236_LEFT_DSP		0x88	
#define CS4236_RIGHT_DSP	0x98	
#define CS4236_RIGHT_LOOPBACK	0xa8	
#define CS4236_DAC_MUTE		0xb8	
#define CS4236_ADC_RATE		0xc8	
#define CS4236_DAC_RATE		0xd8	
#define CS4236_LEFT_MASTER	0xe8	
#define CS4236_RIGHT_MASTER	0xf8	
#define CS4236_LEFT_WAVE	0x0c	
#define CS4236_RIGHT_WAVE	0x1c	
#define CS4236_VERSION		0x9c	

#define OPTi931_AUX_LEFT_INPUT	0x10
#define OPTi931_AUX_RIGHT_INPUT	0x11
#define OPTi93X_MIC_LEFT_INPUT	0x14
#define OPTi93X_MIC_RIGHT_INPUT	0x15
#define OPTi93X_OUT_LEFT	0x16
#define OPTi93X_OUT_RIGHT	0x17

#endif 
