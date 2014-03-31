#ifndef __SOUND_DELTA_H
#define __SOUND_DELTA_H

/*
 *   ALSA driver for ICEnsemble ICE1712 (Envy24)
 *
 *   Lowlevel functions for M-Audio Delta 1010, 44, 66, Dio2496, Audiophile
 *                          Digigram VX442
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

#define DELTA_DEVICE_DESC \
		"{MidiMan M Audio,Delta 1010},"\
		"{MidiMan M Audio,Delta 1010LT},"\
		"{MidiMan M Audio,Delta DiO 2496},"\
		"{MidiMan M Audio,Delta 66},"\
		"{MidiMan M Audio,Delta 44},"\
		"{MidiMan M Audio,Delta 410},"\
		"{MidiMan M Audio,Audiophile 24/96},"\
		"{Digigram,VX442},"\
		"{Lionstracs,Mediastation},"\
		"{Edirol,DA2496},"

#define ICE1712_SUBDEVICE_DELTA1010	0x121430d6
#define ICE1712_SUBDEVICE_DELTA1010E	0xff1430d6
#define ICE1712_SUBDEVICE_DELTADIO2496	0x121431d6
#define ICE1712_SUBDEVICE_DELTA66	0x121432d6
#define ICE1712_SUBDEVICE_DELTA66E	0xff1432d6
#define ICE1712_SUBDEVICE_DELTA44	0x121433d6
#define ICE1712_SUBDEVICE_AUDIOPHILE	0x121434d6
#define ICE1712_SUBDEVICE_DELTA410	0x121438d6
#define ICE1712_SUBDEVICE_DELTA1010LT	0x12143bd6
#define ICE1712_SUBDEVICE_VX442		0x12143cd6
#define ICE1712_SUBDEVICE_MEDIASTATION	0x694c0100
#define ICE1712_SUBDEVICE_EDIROLDA2496	0xce164010

extern struct snd_ice1712_card_info snd_ice1712_delta_cards[];



#define ICE1712_DELTA_DFS 0x01		
					
#define ICE1712_DELTA_SPDIF_IN_STAT 0x02
					
					
					
					
#define ICE1712_DELTA_SPDIF_OUT_STAT_CLOCK 0x04
					
					
					
					
#define ICE1712_DELTA_SPDIF_OUT_STAT_DATA 0x08
					
					
					
#define ICE1712_DELTA_SPDIF_INPUT_SELECT 0x10
					
					

#define ICE1712_DELTA_WORD_CLOCK_SELECT 0x10
					
					
					
#define ICE1712_DELTA_WORD_CLOCK_STATUS	0x20
					

#define ICE1712_DELTA_CODEC_SERIAL_DATA 0x10
					
#define ICE1712_DELTA_CODEC_SERIAL_CLOCK 0x20
					
					
#define ICE1712_DELTA_CODEC_CHIP_A	0x40
#define ICE1712_DELTA_CODEC_CHIP_B	0x80
					


#define ICE1712_DELTA_AP_CCLK	0x02	
					
#define ICE1712_DELTA_AP_DIN	0x04	
#define ICE1712_DELTA_AP_DOUT	0x08	
#define ICE1712_DELTA_AP_CS_DIGITAL 0x10 
					
#define ICE1712_DELTA_AP_CS_CODEC 0x20	
					

#define ICE1712_DELTA_1010LT_CCLK	0x02	
#define ICE1712_DELTA_1010LT_DIN	0x04	
#define ICE1712_DELTA_1010LT_DOUT	0x08	
#define ICE1712_DELTA_1010LT_CS		0x70	
#define ICE1712_DELTA_1010LT_CS_CHIP_A	0x00	
#define ICE1712_DELTA_1010LT_CS_CHIP_B	0x10	
#define ICE1712_DELTA_1010LT_CS_CHIP_C	0x20	
#define ICE1712_DELTA_1010LT_CS_CHIP_D	0x30	
#define ICE1712_DELTA_1010LT_CS_CS8427	0x40	
#define ICE1712_DELTA_1010LT_CS_NONE	0x50	
#define ICE1712_DELTA_1010LT_WORDCLOCK 0x80	

#define ICE1712_DELTA_66E_CCLK		0x02	
#define ICE1712_DELTA_66E_DIN		0x04	
#define ICE1712_DELTA_66E_DOUT		0x08	
#define ICE1712_DELTA_66E_CS_CS8427	0x10	
#define ICE1712_DELTA_66E_CS_CHIP_A	0x20	
#define ICE1712_DELTA_66E_CS_CHIP_B	0x40	

#define ICE1712_VX442_CCLK		0x02	
#define ICE1712_VX442_DIN		0x04	
#define ICE1712_VX442_DOUT		0x08	
#define ICE1712_VX442_CS_DIGITAL	0x10	
#define ICE1712_VX442_CODEC_CHIP_A	0x20	
#define ICE1712_VX442_CODEC_CHIP_B	0x40	

#endif 
