#ifndef __SOUND_WSS_H
#define __SOUND_WSS_H

/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Definitions for CS4231 & InterWave chips & compatible chips
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

#include "control.h"
#include "pcm.h"
#include "timer.h"

#include "cs4231-regs.h"


#define WSS_MODE_NONE	0x0000
#define WSS_MODE_PLAY	0x0001
#define WSS_MODE_RECORD	0x0002
#define WSS_MODE_TIMER	0x0004
#define WSS_MODE_OPEN	(WSS_MODE_PLAY|WSS_MODE_RECORD|WSS_MODE_TIMER)


#define WSS_HW_DETECT        0x0000	
#define WSS_HW_DETECT3	0x0001	
#define WSS_HW_TYPE_MASK	0xff00	
#define WSS_HW_CS4231_MASK   0x0100	
#define WSS_HW_CS4231        0x0100	
#define WSS_HW_CS4231A       0x0101	
#define WSS_HW_AD1845	0x0102	
#define WSS_HW_CS4232_MASK   0x0200	
#define WSS_HW_CS4232        0x0200	
#define WSS_HW_CS4232A       0x0201	
#define WSS_HW_CS4236	0x0202	
#define WSS_HW_CS4236B_MASK	0x0400	
#define WSS_HW_CS4235	0x0400	
#define WSS_HW_CS4236B       0x0401	
#define WSS_HW_CS4237B       0x0402	
#define WSS_HW_CS4238B	0x0403	
#define WSS_HW_CS4239	0x0404	
#define WSS_HW_AD1848_MASK	0x0800	
#define WSS_HW_AD1847		0x0801	
#define WSS_HW_AD1848		0x0802	
#define WSS_HW_CS4248		0x0803	
#define WSS_HW_CMI8330		0x0804	
#define WSS_HW_THINKPAD		0x0805	
#define WSS_HW_INTERWAVE     0x1000	
#define WSS_HW_OPL3SA2       0x1101	
#define WSS_HW_OPTI93X 	0x1102	

#define WSS_HWSHARE_IRQ	(1<<0)
#define WSS_HWSHARE_DMA1	(1<<1)
#define WSS_HWSHARE_DMA2	(1<<2)

#define AD1848_THINKPAD_CTL_PORT1		0x15e8
#define AD1848_THINKPAD_CTL_PORT2		0x15e9
#define AD1848_THINKPAD_CS4248_ENABLE_BIT	0x02

struct snd_wss {
	unsigned long port;		
	struct resource *res_port;
	unsigned long cport;		
	struct resource *res_cport;
	int irq;			
	int dma1;			
	int dma2;			
	unsigned short version;		
	unsigned short mode;		
	unsigned short hardware;	
	unsigned short hwshare;		
	unsigned short single_dma:1,	
					
		       ebus_flag:1,	
		       thinkpad_flag:1;	

	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;
	struct snd_timer *timer;

	unsigned char image[32];	
	unsigned char eimage[32];	
	unsigned char cimage[16];	
	int mce_bit;
	int calibrate_mute;
	int sw_3d_bit;
	unsigned int p_dma_size;
	unsigned int c_dma_size;

	spinlock_t reg_lock;
	struct mutex mce_mutex;
	struct mutex open_mutex;

	int (*rate_constraint) (struct snd_pcm_runtime *runtime);
	void (*set_playback_format) (struct snd_wss *chip,
				     struct snd_pcm_hw_params *hw_params,
				     unsigned char pdfr);
	void (*set_capture_format) (struct snd_wss *chip,
				    struct snd_pcm_hw_params *hw_params,
				    unsigned char cdfr);
	void (*trigger) (struct snd_wss *chip, unsigned int what, int start);
#ifdef CONFIG_PM
	void (*suspend) (struct snd_wss *chip);
	void (*resume) (struct snd_wss *chip);
#endif
	void *dma_private_data;
	int (*claim_dma) (struct snd_wss *chip,
			  void *dma_private_data, int dma);
	int (*release_dma) (struct snd_wss *chip,
			    void *dma_private_data, int dma);
};


void snd_wss_out(struct snd_wss *chip, unsigned char reg, unsigned char val);
unsigned char snd_wss_in(struct snd_wss *chip, unsigned char reg);
void snd_cs4236_ext_out(struct snd_wss *chip,
			unsigned char reg, unsigned char val);
unsigned char snd_cs4236_ext_in(struct snd_wss *chip, unsigned char reg);
void snd_wss_mce_up(struct snd_wss *chip);
void snd_wss_mce_down(struct snd_wss *chip);

void snd_wss_overrange(struct snd_wss *chip);

irqreturn_t snd_wss_interrupt(int irq, void *dev_id);

const char *snd_wss_chip_id(struct snd_wss *chip);

int snd_wss_create(struct snd_card *card,
		      unsigned long port,
		      unsigned long cport,
		      int irq, int dma1, int dma2,
		      unsigned short hardware,
		      unsigned short hwshare,
		      struct snd_wss **rchip);
int snd_wss_pcm(struct snd_wss *chip, int device, struct snd_pcm **rpcm);
int snd_wss_timer(struct snd_wss *chip, int device, struct snd_timer **rtimer);
int snd_wss_mixer(struct snd_wss *chip);

const struct snd_pcm_ops *snd_wss_get_pcm_ops(int direction);

int snd_cs4236_create(struct snd_card *card,
		      unsigned long port,
		      unsigned long cport,
		      int irq, int dma1, int dma2,
		      unsigned short hardware,
		      unsigned short hwshare,
		      struct snd_wss **rchip);
int snd_cs4236_pcm(struct snd_wss *chip, int device, struct snd_pcm **rpcm);
int snd_cs4236_mixer(struct snd_wss *chip);


#define WSS_SINGLE(xname, xindex, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_wss_info_single, \
  .get = snd_wss_get_single, \
  .put = snd_wss_put_single, \
  .private_value = reg | (shift << 8) | (mask << 16) | (invert << 24) }

int snd_wss_info_single(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);
int snd_wss_get_single(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int snd_wss_put_single(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

#define WSS_DOUBLE(xname, xindex, left_reg, right_reg, shift_left, shift_right, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .name = xname, \
  .index = xindex, \
  .info = snd_wss_info_double, \
  .get = snd_wss_get_double, \
  .put = snd_wss_put_double, \
  .private_value = left_reg | (right_reg << 8) | (shift_left << 16) | \
		   (shift_right << 19) | (mask << 24) | (invert << 22) }

#define WSS_SINGLE_TLV(xname, xindex, reg, shift, mask, invert, xtlv) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ, \
  .name = xname, \
  .index = xindex, \
  .info = snd_wss_info_single, \
  .get = snd_wss_get_single, \
  .put = snd_wss_put_single, \
  .private_value = reg | (shift << 8) | (mask << 16) | (invert << 24), \
  .tlv = { .p = (xtlv) } }

#define WSS_DOUBLE_TLV(xname, xindex, left_reg, right_reg, \
			shift_left, shift_right, mask, invert, xtlv) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ, \
  .name = xname, \
  .index = xindex, \
  .info = snd_wss_info_double, \
  .get = snd_wss_get_double, \
  .put = snd_wss_put_double, \
  .private_value = left_reg | (right_reg << 8) | (shift_left << 16) | \
		   (shift_right << 19) | (mask << 24) | (invert << 22), \
  .tlv = { .p = (xtlv) } }


int snd_wss_info_double(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);
int snd_wss_get_double(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int snd_wss_put_double(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

#endif 
