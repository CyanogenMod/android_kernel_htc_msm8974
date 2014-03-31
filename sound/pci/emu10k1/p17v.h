/*
 *  Copyright (c) by James Courtier-Dutton <James@superbug.demon.co.uk>
 *  Driver p17v chips
 *  Version: 0.01
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


#define P17V_PLAYBACK_FIFO_PTR	0x08	
  
#define P17V_CAPTURE_FIFO_PTR	0x13	
  
#define P17V_PB_CHN_SEL		0x18	
#define P17V_SE_SLOT_SEL_L	0x19	
#define P17V_SE_SLOT_SEL_H	0x1a	
#define P17V_SPI		0x3c	
#define P17V_I2C_ADDR		0x3d	
#define P17V_I2C_0		0x3e	
#define P17V_I2C_1		0x3f	
#define I2C_A_ADC_ADD_MASK	0x000000fe	
#define I2C_A_ADC_RW_MASK	0x00000001	
#define I2C_A_ADC_TRANS_MASK	0x00000010  	
#define I2C_A_ADC_ABORT_MASK	0x00000020	
#define I2C_A_ADC_LAST_MASK	0x00000040	
#define I2C_A_ADC_BYTE_MASK	0x00000080	

#define I2C_A_ADC_ADD		0x00000034	
#define I2C_A_ADC_READ		0x00000001	
#define I2C_A_ADC_START		0x00000100	
#define I2C_A_ADC_ABORT		0x00000200	
#define I2C_A_ADC_LAST		0x00000400	
#define I2C_A_ADC_BYTE		0x00000800	

#define I2C_D_ADC_REG_MASK	0xfe000000  	 
#define I2C_D_ADC_DAT_MASK	0x01ff0000  	

#define ADC_TIMEOUT		0x00000007	
#define ADC_IFC_CTRL		0x0000000b	
#define ADC_MASTER		0x0000000c	
#define ADC_POWER		0x0000000d	
#define ADC_ATTEN_ADCL		0x0000000e	
#define ADC_ATTEN_ADCR		0x0000000f	
#define ADC_ALC_CTRL1		0x00000010	
#define ADC_ALC_CTRL2		0x00000011	
#define ADC_ALC_CTRL3		0x00000012	
#define ADC_NOISE_CTRL		0x00000013	
#define ADC_LIMIT_CTRL		0x00000014	
#define ADC_MUX			0x00000015  	
#if 0
#define ADC_GAIN_MASK		0x000000ff	
#define ADC_ZERODB		0x000000cf	
#define ADC_MUTE_MASK		0x000000c0	
#define ADC_MUTE		0x000000c0	
#define ADC_OSR			0x00000008	
#define ADC_TIMEOUT_DISABLE	0x00000008	
#define ADC_HPF_DISABLE		0x00000100	
#define ADC_TRANWIN_MASK	0x00000070	
#endif

#define ADC_MUX_MASK		0x0000000f	
#define ADC_MUX_0		0x00000001	
#define ADC_MUX_1		0x00000002	
#define ADC_MUX_2		0x00000004	
#define ADC_MUX_3		0x00000008	

#define P17V_START_AUDIO	0x40	
#define P17V_START_CAPTURE	0x48	
#define P17V_CAPTURE_FIFO_BASE	0x49	
#define P17V_CAPTURE_FIFO_SIZE	0x4a	
#define P17V_CAPTURE_FIFO_INDEX	0x4b	
#define P17V_CAPTURE_VOL_H	0x4c	
#define P17V_CAPTURE_VOL_L	0x4d	
#define P17V_SRCSel		0x60	
#define P17V_MIXER_AC97_10K1_VOL_L	0x61	
#define P17V_MIXER_AC97_10K1_VOL_H	0x62	
#define P17V_MIXER_AC97_P17V_VOL_L	0x63	
#define P17V_MIXER_AC97_P17V_VOL_H	0x64	
#define P17V_MIXER_AC97_SRP_REC_VOL_L	0x65	
#define P17V_MIXER_AC97_SRP_REC_VOL_H	0x66	
#define P17V_MIXER_Spdif_10K1_VOL_L	0x69	
#define P17V_MIXER_Spdif_10K1_VOL_H	0x6A	
#define P17V_MIXER_Spdif_P17V_VOL_L	0x6B	
#define P17V_MIXER_Spdif_P17V_VOL_H	0x6C	
#define P17V_MIXER_Spdif_SRP_REC_VOL_L	0x6D	
#define P17V_MIXER_Spdif_SRP_REC_VOL_H	0x6E	
#define P17V_MIXER_I2S_10K1_VOL_L	0x71	
#define P17V_MIXER_I2S_10K1_VOL_H	0x72	
#define P17V_MIXER_I2S_P17V_VOL_L	0x73	
#define P17V_MIXER_I2S_P17V_VOL_H	0x74	
#define P17V_MIXER_I2S_SRP_REC_VOL_L	0x75	
#define P17V_MIXER_I2S_SRP_REC_VOL_H	0x76	
#define P17V_MIXER_AC97_ENABLE		0x79	
#define P17V_MIXER_SPDIF_ENABLE		0x7A	
#define P17V_MIXER_I2S_ENABLE		0x7B	
#define P17V_AUDIO_OUT_ENABLE		0x7C	
#define P17V_MIXER_ATT			0x7D	
#define P17V_SRP_RECORD_SRR		0x7E	
#define P17V_SOFT_RESET_SRP_MIXER	0x7F	

#define P17V_AC97_OUT_MASTER_VOL_L	0x80	
#define P17V_AC97_OUT_MASTER_VOL_H	0x81	
#define P17V_SPDIF_OUT_MASTER_VOL_L	0x82	
#define P17V_SPDIF_OUT_MASTER_VOL_H	0x83	
#define P17V_I2S_OUT_MASTER_VOL_L	0x84	
#define P17V_I2S_OUT_MASTER_VOL_H	0x85	
#define P17V_I2S_CHANNEL_SWAP_PHASE_INVERSE	0x88	
#define P17V_SPDIF_CHANNEL_SWAP_PHASE_INVERSE	0x89	
#define P17V_SRP_P17V_ESR		0x8B	
#define P17V_SRP_REC_ESR		0x8C	
#define P17V_SRP_BYPASS			0x8D	
#define P17V_I2S_SRC_SEL		0x93	






