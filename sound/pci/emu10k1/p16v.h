/*
 *  Copyright (c) by James Courtier-Dutton <James@superbug.demon.co.uk>
 *  Driver p16v chips
 *  Version: 0.21
 *
 *  FEATURES currently supported:
 *    Output fixed at S32_LE, 2 channel to hw:0,0
 *    Rates: 44.1, 48, 96, 192.
 *
 *  Changelog:
 *  0.8
 *    Use separate card based buffer for periods table.
 *  0.9
 *    Use 2 channel output streams instead of 8 channel.
 *       (8 channel output streams might be good for ASIO type output)
 *    Corrected speaker output, so Front -> Front etc.
 *  0.10
 *    Fixed missed interrupts.
 *  0.11
 *    Add Sound card model number and names.
 *    Add Analog volume controls.
 *  0.12
 *    Corrected playback interrupts. Now interrupt per period, instead of half period.
 *  0.13
 *    Use single trigger for multichannel.
 *  0.14
 *    Mic capture now works at fixed: S32_LE, 96000Hz, Stereo.
 *  0.15
 *    Force buffer_size / period_size == INTEGER.
 *  0.16
 *    Update p16v.c to work with changed alsa api.
 *  0.17
 *    Update p16v.c to work with changed alsa api. Removed boot_devs.
 *  0.18
 *    Merging with snd-emu10k1 driver.
 *  0.19
 *    One stereo channel at 24bit now works.
 *  0.20
 *    Added better register defines.
 *  0.21
 *    Split from p16v.c
 *
 *
 *  BUGS:
 *    Some stability problems when unloading the snd-p16v kernel module.
 *    --
 *
 *  TODO:
 *    SPDIF out.
 *    Find out how to change capture sample rates. E.g. To record SPDIF at 48000Hz.
 *    Currently capture fixed at 48000Hz.
 *
 *    --
 *  GENERAL INFO:
 *    Model: SB0240
 *    P16V Chip: CA0151-DBS
 *    Audigy 2 Chip: CA0102-IAT
 *    AC97 Codec: STAC 9721
 *    ADC: Philips 1361T (Stereo 24bit)
 *    DAC: CS4382-K (8-channel, 24bit, 192Khz)
 *
 *  This code was initially based on code from ALSA's emu10k1x.c which is:
 *  Copyright (c) by Francisco Moraes <fmoraes@nc.rr.com>
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

                                                                                                                           

#define PLAYBACK_LIST_ADDR	0x00		
#define PLAYBACK_LIST_SIZE	0x01		
#define PLAYBACK_LIST_PTR	0x02		
#define PLAYBACK_UNKNOWN3	0x03		
#define PLAYBACK_DMA_ADDR	0x04		
#define PLAYBACK_PERIOD_SIZE	0x05		
#define PLAYBACK_POINTER	0x06		
#define PLAYBACK_FIFO_END_ADDRESS	0x07		
#define PLAYBACK_FIFO_POINTER	0x08		
#define PLAYBACK_UNKNOWN9	0x09		
#define CAPTURE_DMA_ADDR	0x10		
#define CAPTURE_BUFFER_SIZE	0x11		
#define CAPTURE_POINTER		0x12		
#define CAPTURE_FIFO_POINTER	0x13		
#define CAPTURE_P16V_VOLUME1	0x14		
#define CAPTURE_P16V_VOLUME2	0x15		
#define CAPTURE_P16V_SOURCE     0x16            

#define CAPTURE_RATE_STATUS		0x17		
#define PLAYBACK_LAST_SAMPLE    0x20		
#define BASIC_INTERRUPT         0x40		
						
						
#define WATERMARK            0x46		
#define SRCSel			0x60            
						
						
						

#define PLAYBACK_VOLUME_MIXER1  0x61		
#define PLAYBACK_VOLUME_MIXER2  0x62		
#define PLAYBACK_VOLUME_MIXER3  0x63		
#define PLAYBACK_VOLUME_MIXER4  0x64		
#define PLAYBACK_VOLUME_MIXER5  0x65		
#define PLAYBACK_VOLUME_MIXER6  0x66		
#define PLAYBACK_VOLUME_MIXER7  0x67		
#define PLAYBACK_VOLUME_MIXER8  0x68		
#define PLAYBACK_VOLUME_MIXER9  0x69		
#define PLAYBACK_VOLUME_MIXER10  0x6a		
						
#define PLAYBACK_VOLUME_MIXER11  0x6b		
#define PLAYBACK_VOLUME_MIXER12 0x6c		

#define SRC48_ENABLE            0x6d            
						
#define SRCMULTI_ENABLE         0x6e            
						
#define AUDIO_OUT_ENABLE        0x6f            
						
						
#define PLAYBACK_SPDIF_SELECT     0x70          
						
						
						
#define PLAYBACK_SPDIF_SRC_SELECT 0x71          
						
						
#define PLAYBACK_SPDIF_USER_DATA0 0x72		
#define PLAYBACK_SPDIF_USER_DATA1 0x73		
#define CAPTURE_SPDIF_CONTROL	0x76		
#define CAPTURE_SPDIF_STATUS	0x77		
#define CAPURE_SPDIF_USER_DATA0 0x78		
#define CAPURE_SPDIF_USER_DATA1 0x79		
#define CAPURE_SPDIF_USER_DATA2 0x7a		

