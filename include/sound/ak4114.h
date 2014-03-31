#ifndef __SOUND_AK4114_H
#define __SOUND_AK4114_H

/*
 *  Routines for Asahi Kasei AK4114
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>,
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

#define AK4114_REG_PWRDN	0x00	
#define AK4114_REG_FORMAT	0x01	
#define AK4114_REG_IO0		0x02	
#define AK4114_REG_IO1		0x03	
#define AK4114_REG_INT0_MASK	0x04	
#define AK4114_REG_INT1_MASK	0x05	
#define AK4114_REG_RCS0		0x06	
#define AK4114_REG_RCS1		0x07	
#define AK4114_REG_RXCSB0	0x08	
#define AK4114_REG_RXCSB1	0x09	
#define AK4114_REG_RXCSB2	0x0a	
#define AK4114_REG_RXCSB3	0x0b	
#define AK4114_REG_RXCSB4	0x0c	
#define AK4114_REG_TXCSB0	0x0d	
#define AK4114_REG_TXCSB1	0x0e	
#define AK4114_REG_TXCSB2	0x0f	
#define AK4114_REG_TXCSB3	0x10	
#define AK4114_REG_TXCSB4	0x11	
#define AK4114_REG_Pc0		0x12	
#define AK4114_REG_Pc1		0x13	
#define AK4114_REG_Pd0		0x14	
#define AK4114_REG_Pd1		0x15	
#define AK4114_REG_QSUB_ADDR	0x16	
#define AK4114_REG_QSUB_TRACK	0x17	
#define AK4114_REG_QSUB_INDEX	0x18	
#define AK4114_REG_QSUB_MINUTE	0x19	
#define AK4114_REG_QSUB_SECOND	0x1a	
#define AK4114_REG_QSUB_FRAME	0x1b	
#define AK4114_REG_QSUB_ZERO	0x1c	
#define AK4114_REG_QSUB_ABSMIN	0x1d	
#define AK4114_REG_QSUB_ABSSEC	0x1e	
#define AK4114_REG_QSUB_ABSFRM	0x1f	

#define AK4114_REG_RXCSB_SIZE	((AK4114_REG_RXCSB4-AK4114_REG_RXCSB0)+1)
#define AK4114_REG_TXCSB_SIZE	((AK4114_REG_TXCSB4-AK4114_REG_TXCSB0)+1)
#define AK4114_REG_QSUB_SIZE	((AK4114_REG_QSUB_ABSFRM-AK4114_REG_QSUB_ADDR)+1)

#define AK4114_CS12		(1<<7)	
#define AK4114_BCU		(1<<6)	
#define AK4114_CM1		(1<<5)	
#define AK4114_CM0		(1<<4)	
#define AK4114_OCKS1		(1<<3)	
#define AK4114_OCKS0		(1<<2)	
#define AK4114_PWN		(1<<1)	
#define AK4114_RST		(1<<0)	

#define AK4114_MONO		(1<<7)	
#define AK4114_DIF2		(1<<6)	
#define AK4114_DIF1		(1<<5)	
#define AK4114_DIF0		(1<<4)	
#define AK4114_DIF_16R		(0)				
#define AK4114_DIF_18R		(AK4114_DIF0)			
#define AK4114_DIF_20R		(AK4114_DIF1)			
#define AK4114_DIF_24R		(AK4114_DIF1|AK4114_DIF0)	
#define AK4114_DIF_24L		(AK4114_DIF2)			
#define AK4114_DIF_24I2S	(AK4114_DIF2|AK4114_DIF0)	
#define AK4114_DIF_I24L		(AK4114_DIF2|AK4114_DIF1)	
#define AK4114_DIF_I24I2S	(AK4114_DIF2|AK4114_DIF1|AK4114_DIF0) 
#define AK4114_DEAU		(1<<3)	
#define AK4114_DEM1		(1<<2)	
#define AK4114_DEM0		(1<<1)	
#define AK4114_DEM_44KHZ	(0)
#define AK4114_DEM_48KHZ	(AK4114_DEM1)
#define AK4114_DEM_32KHZ	(AK4114_DEM0|AK4114_DEM1)
#define AK4114_DEM_96KHZ	(AK4114_DEM1)	
#define AK4114_DFS		(1<<0)	

#define AK4114_TX1E		(1<<7)	
#define AK4114_OPS12		(1<<6)	
#define AK4114_OPS11		(1<<5)	
#define AK4114_OPS10		(1<<4)	
#define AK4114_TX0E		(1<<3)	
#define AK4114_OPS02		(1<<2)	
#define AK4114_OPS01		(1<<1)	
#define AK4114_OPS00		(1<<0)	

#define AK4114_EFH1		(1<<7)	
#define AK4114_EFH0		(1<<6)	
#define AK4114_EFH_512		(0)
#define AK4114_EFH_1024		(AK4114_EFH0)
#define AK4114_EFH_2048		(AK4114_EFH1)
#define AK4114_EFH_4096		(AK4114_EFH1|AK4114_EFH0)
#define AK4114_UDIT		(1<<5)	
#define AK4114_TLR		(1<<4)	
#define AK4114_DIT		(1<<3)	
#define AK4114_IPS2		(1<<2)	
#define AK4114_IPS1		(1<<1)	
#define AK4114_IPS0		(1<<0)	
#define AK4114_IPS(x)		((x)&7)

#define AK4117_MQI              (1<<7)  
#define AK4117_MAT              (1<<6)  
#define AK4117_MCI              (1<<5)  
#define AK4117_MUL              (1<<4)  
#define AK4117_MDTS             (1<<3)  
#define AK4117_MPE              (1<<2)  
#define AK4117_MAN              (1<<1)  
#define AK4117_MPR              (1<<0)  

#define AK4114_QINT		(1<<7)	
#define AK4114_AUTO		(1<<6)	
#define AK4114_CINT		(1<<5)	
#define AK4114_UNLCK		(1<<4)	
#define AK4114_DTSCD		(1<<3)	
#define AK4114_PEM		(1<<2)	
#define AK4114_AUDION		(1<<1)	
#define AK4114_PAR		(1<<0)	

#define AK4114_FS3		(1<<7)	
#define AK4114_FS2		(1<<6)
#define AK4114_FS1		(1<<5)
#define AK4114_FS0		(1<<4)
#define AK4114_FS_44100HZ	(0)
#define AK4114_FS_48000HZ	(AK4114_FS1)
#define AK4114_FS_32000HZ	(AK4114_FS1|AK4114_FS0)
#define AK4114_FS_88200HZ	(AK4114_FS3)
#define AK4114_FS_96000HZ	(AK4114_FS3|AK4114_FS1)
#define AK4114_FS_176400HZ	(AK4114_FS3|AK4114_FS2)
#define AK4114_FS_192000HZ	(AK4114_FS3|AK4114_FS2|AK4114_FS1)
#define AK4114_V		(1<<3)	
#define AK4114_QCRC		(1<<1)	
#define AK4114_CCRC		(1<<0)	

#define AK4114_CHECK_NO_STAT	(1<<0)	
#define AK4114_CHECK_NO_RATE	(1<<1)	

#define AK4114_CONTROLS		15

typedef void (ak4114_write_t)(void *private_data, unsigned char addr, unsigned char data);
typedef unsigned char (ak4114_read_t)(void *private_data, unsigned char addr);

struct ak4114 {
	struct snd_card *card;
	ak4114_write_t * write;
	ak4114_read_t * read;
	void * private_data;
	unsigned int init: 1;
	spinlock_t lock;
	unsigned char regmap[7];
	unsigned char txcsb[5];
	struct snd_kcontrol *kctls[AK4114_CONTROLS];
	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;
	unsigned long parity_errors;
	unsigned long v_bit_errors;
	unsigned long qcrc_errors;
	unsigned long ccrc_errors;
	unsigned char rcs0;
	unsigned char rcs1;
	struct delayed_work work;
	unsigned int check_flags;
	void *change_callback_private;
	void (*change_callback)(struct ak4114 *ak4114, unsigned char c0, unsigned char c1);
};

int snd_ak4114_create(struct snd_card *card,
		      ak4114_read_t *read, ak4114_write_t *write,
		      const unsigned char pgm[7], const unsigned char txcsb[5],
		      void *private_data, struct ak4114 **r_ak4114);
void snd_ak4114_reg_write(struct ak4114 *ak4114, unsigned char reg, unsigned char mask, unsigned char val);
void snd_ak4114_reinit(struct ak4114 *ak4114);
int snd_ak4114_build(struct ak4114 *ak4114,
		     struct snd_pcm_substream *playback_substream,
                     struct snd_pcm_substream *capture_substream);
int snd_ak4114_external_rate(struct ak4114 *ak4114);
int snd_ak4114_check_rate_and_errors(struct ak4114 *ak4114, unsigned int flags);

#endif 

