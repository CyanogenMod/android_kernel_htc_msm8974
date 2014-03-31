#ifndef __SOUND_AK4531_CODEC_H
#define __SOUND_AK4531_CODEC_H

/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Universal interface for Audio Codec '97
 *
 *  For more details look to AC '97 component specification revision 2.1
 *  by Intel Corporation (http://developer.intel.com).
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

#include "info.h"
#include "control.h"



#define AK4531_LMASTER  0x00	
#define AK4531_RMASTER  0x01	
#define AK4531_LVOICE   0x02	
#define AK4531_RVOICE   0x03	
#define AK4531_LFM      0x04	
#define AK4531_RFM      0x05	
#define AK4531_LCD      0x06	
#define AK4531_RCD      0x07	
#define AK4531_LLINE    0x08	
#define AK4531_RLINE    0x09	
#define AK4531_LAUXA    0x0a	
#define AK4531_RAUXA    0x0b	
#define AK4531_MONO1    0x0c	
#define AK4531_MONO2    0x0d	
#define AK4531_MIC      0x0e	
#define AK4531_MONO_OUT 0x0f	
#define AK4531_OUT_SW1  0x10	
#define AK4531_OUT_SW2  0x11	
#define AK4531_LIN_SW1  0x12	
#define AK4531_RIN_SW1  0x13	
#define AK4531_LIN_SW2  0x14	
#define AK4531_RIN_SW2  0x15	
#define AK4531_RESET    0x16	
#define AK4531_CLOCK    0x17	
#define AK4531_AD_IN    0x18	
#define AK4531_MIC_GAIN 0x19	

struct snd_ak4531 {
	void (*write) (struct snd_ak4531 *ak4531, unsigned short reg,
		       unsigned short val);
	void *private_data;
	void (*private_free) (struct snd_ak4531 *ak4531);
	
	unsigned char regs[0x20];
	struct mutex reg_mutex;
};

int snd_ak4531_mixer(struct snd_card *card, struct snd_ak4531 *_ak4531,
		     struct snd_ak4531 **rak4531);

#ifdef CONFIG_PM
void snd_ak4531_suspend(struct snd_ak4531 *ak4531);
void snd_ak4531_resume(struct snd_ak4531 *ak4531);
#endif

#endif 
