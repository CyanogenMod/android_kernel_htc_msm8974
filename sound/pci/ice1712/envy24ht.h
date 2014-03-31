#ifndef __SOUND_VT1724_H
#define __SOUND_VT1724_H

/*
 *   ALSA driver for ICEnsemble VT1724 (Envy24)
 *
 *	Copyright (c) 2000 Jaroslav Kysela <perex@perex.cz>
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

#include <sound/control.h>
#include <sound/ac97_codec.h>
#include <sound/rawmidi.h>
#include <sound/i2c.h>
#include <sound/pcm.h>

#include "ice1712.h"

enum {
	ICE_EEP2_SYSCONF = 0,	
	ICE_EEP2_ACLINK,	
	ICE_EEP2_I2S,		
	ICE_EEP2_SPDIF,		
	ICE_EEP2_GPIO_DIR,	
	ICE_EEP2_GPIO_DIR1,	
	ICE_EEP2_GPIO_DIR2,	
	ICE_EEP2_GPIO_MASK,	
	ICE_EEP2_GPIO_MASK1,	
	ICE_EEP2_GPIO_MASK2,	
	ICE_EEP2_GPIO_STATE,	
	ICE_EEP2_GPIO_STATE1,	
	ICE_EEP2_GPIO_STATE2	
};
	

#define ICEREG1724(ice, x) ((ice)->port + VT1724_REG_##x)

#define VT1724_REG_CONTROL		0x00	
#define   VT1724_RESET			0x80	
#define VT1724_REG_IRQMASK		0x01	
#define   VT1724_IRQ_MPU_RX		0x80
#define   VT1724_IRQ_MPU_TX		0x20
#define   VT1724_IRQ_MTPCM		0x10
#define VT1724_REG_IRQSTAT		0x02	
#define VT1724_REG_SYS_CFG		0x04	
#define   VT1724_CFG_CLOCK	0xc0
#define     VT1724_CFG_CLOCK512	0x00	
#define     VT1724_CFG_CLOCK384  0x40	
#define   VT1724_CFG_MPU401	0x20		
#define   VT1724_CFG_ADC_MASK	0x0c	
#define   VT1724_CFG_ADC_NONE	0x0c	
#define   VT1724_CFG_DAC_MASK	0x03	

#define VT1724_REG_AC97_CFG		0x05	
#define   VT1724_CFG_PRO_I2S	0x80	
#define   VT1724_CFG_AC97_PACKED	0x01	

#define VT1724_REG_I2S_FEATURES		0x06	
#define   VT1724_CFG_I2S_VOLUME	0x80	
#define   VT1724_CFG_I2S_96KHZ	0x40	
#define   VT1724_CFG_I2S_RESMASK	0x30	
#define   VT1724_CFG_I2S_192KHZ	0x08	
#define   VT1724_CFG_I2S_OTHER	0x07	

#define VT1724_REG_SPDIF_CFG		0x07	
#define   VT1724_CFG_SPDIF_OUT_EN	0x80	
#define   VT1724_CFG_SPDIF_OUT_INT	0x40	
#define   VT1724_CFG_I2S_CHIPID	0x3c	
#define   VT1724_CFG_SPDIF_IN	0x02	
#define   VT1724_CFG_SPDIF_OUT	0x01	


#define VT1724_REG_MPU_TXFIFO		0x0a	
#define VT1724_REG_MPU_RXFIFO		0x0b	

#define VT1724_REG_MPU_DATA		0x0c	
#define VT1724_REG_MPU_CTRL		0x0d	
#define   VT1724_MPU_UART	0x01
#define   VT1724_MPU_TX_EMPTY	0x02
#define   VT1724_MPU_TX_FULL	0x04
#define   VT1724_MPU_RX_EMPTY	0x08
#define   VT1724_MPU_RX_FULL	0x10

#define VT1724_REG_MPU_FIFO_WM	0x0e	
#define   VT1724_MPU_RX_FIFO	0x20	
#define   VT1724_MPU_FIFO_MASK	0x1f	

#define VT1724_REG_I2C_DEV_ADDR	0x10	
#define   VT1724_I2C_WRITE		0x01	
#define VT1724_REG_I2C_BYTE_ADDR	0x11	
#define VT1724_REG_I2C_DATA		0x12	
#define VT1724_REG_I2C_CTRL		0x13	
#define   VT1724_I2C_EEPROM		0x80	
#define   VT1724_I2C_BUSY		0x01	

#define VT1724_REG_GPIO_DATA	0x14	
#define VT1724_REG_GPIO_WRITE_MASK	0x16 
#define VT1724_REG_GPIO_DIRECTION	0x18 
#define VT1724_REG_POWERDOWN	0x1c
#define VT1724_REG_GPIO_DATA_22	0x1e 
#define VT1724_REG_GPIO_WRITE_MASK_22	0x1f 



#define ICEMT1724(ice, x) ((ice)->profi_port + VT1724_MT_##x)

#define VT1724_MT_IRQ			0x00	
#define   VT1724_MULTI_PDMA4	0x80	
#define	  VT1724_MULTI_PDMA3	0x40	
#define   VT1724_MULTI_PDMA2	0x20	
#define   VT1724_MULTI_PDMA1	0x10	
#define   VT1724_MULTI_FIFO_ERR 0x08	
#define   VT1724_MULTI_RDMA1	0x04	
#define   VT1724_MULTI_RDMA0	0x02	
#define   VT1724_MULTI_PDMA0	0x01	

#define VT1724_MT_RATE			0x01	
#define   VT1724_SPDIF_MASTER		0x10	
#define VT1724_MT_I2S_FORMAT		0x02	
#define   VT1724_MT_I2S_MCLK_128X	0x08
#define   VT1724_MT_I2S_FORMAT_MASK	0x03
#define   VT1724_MT_I2S_FORMAT_I2S	0x00
#define VT1724_MT_DMA_INT_MASK		0x03	
#define VT1724_MT_AC97_INDEX		0x04	
#define VT1724_MT_AC97_CMD		0x05	
#define   VT1724_AC97_COLD	0x80	
#define   VT1724_AC97_WARM	0x40	
#define   VT1724_AC97_WRITE	0x20	
#define   VT1724_AC97_READ	0x10	
#define   VT1724_AC97_READY	0x08	
#define   VT1724_AC97_ID_MASK	0x03	
#define VT1724_MT_AC97_DATA		0x06	
#define VT1724_MT_PLAYBACK_ADDR		0x10	
#define VT1724_MT_PLAYBACK_SIZE		0x14	
#define VT1724_MT_DMA_CONTROL		0x18	
#define   VT1724_PDMA4_START	0x80	
#define   VT1724_PDMA3_START	0x40	
#define   VT1724_PDMA2_START	0x20	
#define   VT1724_PDMA1_START	0x10	
#define   VT1724_RDMA1_START	0x04	
#define   VT1724_RDMA0_START	0x02	
#define   VT1724_PDMA0_START	0x01	
#define VT1724_MT_BURST			0x19	
#define VT1724_MT_DMA_FIFO_ERR		0x1a	
#define   VT1724_PDMA4_UNDERRUN		0x80
#define   VT1724_PDMA2_UNDERRUN		0x40
#define   VT1724_PDMA3_UNDERRUN		0x20
#define   VT1724_PDMA1_UNDERRUN		0x10
#define   VT1724_RDMA1_UNDERRUN		0x04
#define   VT1724_RDMA0_UNDERRUN		0x02
#define   VT1724_PDMA0_UNDERRUN		0x01
#define VT1724_MT_DMA_PAUSE		0x1b	
#define	  VT1724_PDMA4_PAUSE	0x80
#define	  VT1724_PDMA3_PAUSE	0x40
#define	  VT1724_PDMA2_PAUSE	0x20
#define	  VT1724_PDMA1_PAUSE	0x10
#define	  VT1724_RDMA1_PAUSE	0x04
#define	  VT1724_RDMA0_PAUSE	0x02
#define	  VT1724_PDMA0_PAUSE	0x01
#define VT1724_MT_PLAYBACK_COUNT	0x1c	
#define VT1724_MT_CAPTURE_ADDR		0x20	
#define VT1724_MT_CAPTURE_SIZE		0x24	
#define VT1724_MT_CAPTURE_COUNT		0x26	

#define VT1724_MT_ROUTE_PLAYBACK	0x2c	

#define VT1724_MT_RDMA1_ADDR		0x30	
#define VT1724_MT_RDMA1_SIZE		0x34	
#define VT1724_MT_RDMA1_COUNT		0x36	

#define VT1724_MT_SPDIF_CTRL		0x3c	
#define VT1724_MT_MONITOR_PEAKINDEX	0x3e	
#define VT1724_MT_MONITOR_PEAKDATA	0x3f	

#define VT1724_MT_PDMA4_ADDR		0x40	
#define VT1724_MT_PDMA4_SIZE		0x44	
#define VT1724_MT_PDMA4_COUNT		0x46	
#define VT1724_MT_PDMA3_ADDR		0x50	
#define VT1724_MT_PDMA3_SIZE		0x54	
#define VT1724_MT_PDMA3_COUNT		0x56	
#define VT1724_MT_PDMA2_ADDR		0x60	
#define VT1724_MT_PDMA2_SIZE		0x64	
#define VT1724_MT_PDMA2_COUNT		0x66	
#define VT1724_MT_PDMA1_ADDR		0x70	
#define VT1724_MT_PDMA1_SIZE		0x74	
#define VT1724_MT_PDMA1_COUNT		0x76	


unsigned char snd_vt1724_read_i2c(struct snd_ice1712 *ice, unsigned char dev, unsigned char addr);
void snd_vt1724_write_i2c(struct snd_ice1712 *ice, unsigned char dev, unsigned char addr, unsigned char data);

#endif 
