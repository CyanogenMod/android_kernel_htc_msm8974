#ifndef __SOUND_EMU10K1_H
#define __SOUND_EMU10K1_H

#include <linux/types.h>

/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>,
 *		     Creative Labs, Inc.
 *  Definitions for EMU10K1 (SB Live!) chips
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

#ifdef __KERNEL__

#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/hwdep.h>
#include <sound/ac97_codec.h>
#include <sound/util_mem.h>
#include <sound/pcm-indirect.h>
#include <sound/timer.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>

#include <asm/io.h>


#define EMUPAGESIZE     4096
#define MAXREQVOICES    8
#define MAXPAGES        8192
#define RESERVED        0
#define NUM_MIDI        16
#define NUM_G           64              
#define NUM_FXSENDS     4
#define NUM_EFX_PLAYBACK    16

#define EMU10K1_DMA_MASK	0x7fffffffUL	
#define AUDIGY_DMA_MASK		0x7fffffffUL	
						

#define TMEMSIZE        256*1024
#define TMEMSIZEREG     4

#define IP_TO_CP(ip) ((ip == 0) ? 0 : (((0x00001000uL | (ip & 0x00000FFFL)) << (((ip >> 12) & 0x000FL) + 4)) & 0xFFFF0000uL))



#define PTR			0x00		
						
						
#define PTR_CHANNELNUM_MASK	0x0000003f	
						
						
						
#define PTR_ADDRESS_MASK	0x07ff0000	
#define A_PTR_ADDRESS_MASK	0x0fff0000

#define DATA			0x04		

#define IPR			0x08		
						
						
#define IPR_P16V		0x80000000	
#define IPR_GPIOMSG		0x20000000	

#define IPR_A_MIDITRANSBUFEMPTY2 0x10000000	
#define IPR_A_MIDIRECVBUFEMPTY2	0x08000000	

#define IPR_SPDIFBUFFULL	0x04000000	
#define IPR_SPDIFBUFHALFFULL	0x02000000	

#define IPR_SAMPLERATETRACKER	0x01000000	
#define IPR_FXDSP		0x00800000	
#define IPR_FORCEINT		0x00400000	
#define IPR_PCIERROR		0x00200000	
#define IPR_VOLINCR		0x00100000	
#define IPR_VOLDECR		0x00080000	
#define IPR_MUTE		0x00040000	
#define IPR_MICBUFFULL		0x00020000	
#define IPR_MICBUFHALFFULL	0x00010000	
#define IPR_ADCBUFFULL		0x00008000	
#define IPR_ADCBUFHALFFULL	0x00004000	
#define IPR_EFXBUFFULL		0x00002000	
#define IPR_EFXBUFHALFFULL	0x00001000	
#define IPR_GPSPDIFSTATUSCHANGE	0x00000800	
#define IPR_CDROMSTATUSCHANGE	0x00000400	
#define IPR_INTERVALTIMER	0x00000200	
#define IPR_MIDITRANSBUFEMPTY	0x00000100	
#define IPR_MIDIRECVBUFEMPTY	0x00000080	
#define IPR_CHANNELLOOP		0x00000040	
#define IPR_CHANNELNUMBERMASK	0x0000003f	
						
						/* or HLIPH.  When IP is written with CL set,	*/
						
						/* to the CIN value written will be cleared.	*/

#define INTE			0x0c		
#define INTE_VIRTUALSB_MASK	0xc0000000	
#define INTE_VIRTUALSB_220	0x00000000	
#define INTE_VIRTUALSB_240	0x40000000	
#define INTE_VIRTUALSB_260	0x80000000	
#define INTE_VIRTUALSB_280	0xc0000000	
#define INTE_VIRTUALMPU_MASK	0x30000000	
#define INTE_VIRTUALMPU_300	0x00000000	
#define INTE_VIRTUALMPU_310	0x10000000	
#define INTE_VIRTUALMPU_320	0x20000000	
#define INTE_VIRTUALMPU_330	0x30000000	
#define INTE_MASTERDMAENABLE	0x08000000	
#define INTE_SLAVEDMAENABLE	0x04000000	
#define INTE_MASTERPICENABLE	0x02000000	
#define INTE_SLAVEPICENABLE	0x01000000	
#define INTE_VSBENABLE		0x00800000	
#define INTE_ADLIBENABLE	0x00400000	
#define INTE_MPUENABLE		0x00200000	
#define INTE_FORCEINT		0x00100000	

#define INTE_MRHANDENABLE	0x00080000	
						
						
						
						

#define INTE_A_MIDITXENABLE2	0x00020000	
#define INTE_A_MIDIRXENABLE2	0x00010000	


#define INTE_SAMPLERATETRACKER	0x00002000	
						
#define INTE_FXDSPENABLE	0x00001000	
#define INTE_PCIERRORENABLE	0x00000800	
#define INTE_VOLINCRENABLE	0x00000400	
#define INTE_VOLDECRENABLE	0x00000200	
#define INTE_MUTEENABLE		0x00000100	
#define INTE_MICBUFENABLE	0x00000080	
#define INTE_ADCBUFENABLE	0x00000040	
#define INTE_EFXBUFENABLE	0x00000020	
#define INTE_GPSPDIFENABLE	0x00000010	
#define INTE_CDSPDIFENABLE	0x00000008	
#define INTE_INTERVALTIMERENB	0x00000004	
#define INTE_MIDITXENABLE	0x00000002	
#define INTE_MIDIRXENABLE	0x00000001	

#define WC			0x10		
#define WC_SAMPLECOUNTER_MASK	0x03FFFFC0	
#define WC_SAMPLECOUNTER	0x14060010
#define WC_CURRENTCHANNEL	0x0000003F	
						
						

#define HCFG			0x14		
						
						
						
						
#define HCFG_LEGACYFUNC_MASK	0xe0000000	
#define HCFG_LEGACYFUNC_MPU	0x00000000	
#define HCFG_LEGACYFUNC_SB	0x40000000	
#define HCFG_LEGACYFUNC_AD	0x60000000	
#define HCFG_LEGACYFUNC_MPIC	0x80000000	
#define HCFG_LEGACYFUNC_MDMA	0xa0000000	
#define HCFG_LEGACYFUNC_SPCI	0xc0000000	
#define HCFG_LEGACYFUNC_SDMA	0xe0000000	
#define HCFG_IOCAPTUREADDR	0x1f000000	
#define HCFG_LEGACYWRITE	0x00800000	
#define HCFG_LEGACYWORD		0x00400000	
#define HCFG_LEGACYINT		0x00200000	
						
						
#define HCFG_PUSH_BUTTON_ENABLE 0x00100000	
#define HCFG_BAUD_RATE		0x00080000	
#define HCFG_EXPANDED_MEM	0x00040000	
#define HCFG_CODECFORMAT_MASK	0x00030000	

#define HCFG_CODECFORMAT_AC97_1	0x00000000	
#define HCFG_CODECFORMAT_AC97_2	0x00010000	
#define HCFG_AUTOMUTE_ASYNC	0x00008000	
						
						
						
#define HCFG_AUTOMUTE_SPDIF	0x00004000	
						
						
#define HCFG_EMU32_SLAVE	0x00002000	
#define HCFG_SLOW_RAMP		0x00001000	
#define HCFG_PHASE_TRACK_MASK	0x00000700	
						
						
#define HCFG_I2S_ASRC_ENABLE	0x00000070	
						
 						



#define HCFG_CODECFORMAT_AC97	0x00000000	
#define HCFG_CODECFORMAT_I2S	0x00010000	
#define HCFG_GPINPUT0		0x00004000	
#define HCFG_GPINPUT1		0x00002000	
#define HCFG_GPOUTPUT_MASK	0x00001c00	
#define HCFG_GPOUT0		0x00001000	
#define HCFG_GPOUT1		0x00000800	
#define HCFG_GPOUT2		0x00000400	
#define HCFG_JOYENABLE      	0x00000200	
#define HCFG_PHASETRACKENABLE	0x00000100	
						
						
#define HCFG_AC3ENABLE_MASK	0x000000e0	
#define HCFG_AC3ENABLE_ZVIDEO	0x00000080	
#define HCFG_AC3ENABLE_CDSPDIF	0x00000040	
#define HCFG_AC3ENABLE_GPSPDIF  0x00000020      
#define HCFG_AUTOMUTE		0x00000010	
						
						
						
#define HCFG_LOCKSOUNDCACHE	0x00000008	
						
#define HCFG_LOCKTANKCACHE_MASK	0x00000004	
						
#define HCFG_LOCKTANKCACHE	0x01020014
#define HCFG_MUTEBUTTONENABLE	0x00000002	
						
						
						
						
						/* been written.       				*/
#define HCFG_AUDIOENABLE	0x00000001	
						
						


#define MUDATA			0x18		

#define MUCMD			0x19		
#define MUCMD_RESET		0xff		
#define MUCMD_ENTERUARTMODE	0x3f		
						

#define MUSTAT			MUCMD		
#define MUSTAT_IRDYN		0x80		
#define MUSTAT_ORDYN		0x40		

#define A_IOCFG			0x18		
#define A_GPINPUT_MASK		0xff00
#define A_GPOUTPUT_MASK		0x00ff

#define A_IOCFG_GPOUT0		0x0044		
#define A_IOCFG_DISABLE_ANALOG	0x0040		
#define A_IOCFG_ENABLE_DIGITAL	0x0004
#define A_IOCFG_ENABLE_DIGITAL_AUDIGY4	0x0080
#define A_IOCFG_UNKNOWN_20      0x0020
#define A_IOCFG_DISABLE_AC97_FRONT      0x0080  
#define A_IOCFG_GPOUT1		0x0002		
#define A_IOCFG_GPOUT2		0x0001		
#define A_IOCFG_MULTIPURPOSE_JACK	0x2000  
                                                
#define A_IOCFG_DIGITAL_JACK    0x1000          
#define A_IOCFG_FRONT_JACK      0x4000
#define A_IOCFG_REAR_JACK       0x8000
#define A_IOCFG_PHONES_JACK     0x0100          


#define TIMER			0x1a		
						
						
						
#define TIMER_RATE_MASK		0x000003ff	
						
#define TIMER_RATE		0x0a00001a

#define AC97DATA		0x1c		

#define AC97ADDRESS		0x1e		
#define AC97ADDRESS_READY	0x80		
#define AC97ADDRESS_ADDRESS	0x7f		

#define PTR2			0x20		
#define DATA2			0x24		
#define IPR2			0x28		
#define IPR2_PLAYBACK_CH_0_LOOP      0x00001000 
#define IPR2_PLAYBACK_CH_0_HALF_LOOP 0x00000100 
#define IPR2_CAPTURE_CH_0_LOOP       0x00100000 
#define IPR2_CAPTURE_CH_0_HALF_LOOP  0x00010000 
#define INTE2			0x2c		
#define INTE2_PLAYBACK_CH_0_LOOP      0x00001000 
#define INTE2_PLAYBACK_CH_0_HALF_LOOP 0x00000100 
#define INTE2_PLAYBACK_CH_1_LOOP      0x00002000 
#define INTE2_PLAYBACK_CH_1_HALF_LOOP 0x00000200 
#define INTE2_PLAYBACK_CH_2_LOOP      0x00004000 
#define INTE2_PLAYBACK_CH_2_HALF_LOOP 0x00000400 
#define INTE2_PLAYBACK_CH_3_LOOP      0x00008000 
#define INTE2_PLAYBACK_CH_3_HALF_LOOP 0x00000800 
#define INTE2_CAPTURE_CH_0_LOOP       0x00100000 
#define INTE2_CAPTURE_CH_0_HALF_LOOP  0x00010000 
#define HCFG2			0x34		
						
						
						
						
#define IPR3			0x38		
#define INTE3			0x3c		

#define JOYSTICK1		0x00		
#define JOYSTICK2		0x01		
#define JOYSTICK3		0x02		
#define JOYSTICK4		0x03		
#define JOYSTICK5		0x04		
#define JOYSTICK6		0x05		
#define JOYSTICK7		0x06		
#define JOYSTICK8		0x07		

#define JOYSTICK_BUTTONS	0x0f		
#define JOYSTICK_COMPARATOR	0xf0		



#define CPF			0x00		
#define CPF_CURRENTPITCH_MASK	0xffff0000	
#define CPF_CURRENTPITCH	0x10100000
#define CPF_STEREO_MASK		0x00008000	
#define CPF_STOP_MASK		0x00004000	
#define CPF_FRACADDRESS_MASK	0x00003fff	

#define PTRX			0x01		
#define PTRX_PITCHTARGET_MASK	0xffff0000	
#define PTRX_PITCHTARGET	0x10100001
#define PTRX_FXSENDAMOUNT_A_MASK 0x0000ff00	
#define PTRX_FXSENDAMOUNT_A	0x08080001
#define PTRX_FXSENDAMOUNT_B_MASK 0x000000ff	
#define PTRX_FXSENDAMOUNT_B	0x08000001

#define CVCF			0x02		
#define CVCF_CURRENTVOL_MASK	0xffff0000	
#define CVCF_CURRENTVOL		0x10100002
#define CVCF_CURRENTFILTER_MASK	0x0000ffff	
#define CVCF_CURRENTFILTER	0x10000002

#define VTFT			0x03		
#define VTFT_VOLUMETARGET_MASK	0xffff0000	
#define VTFT_VOLUMETARGET	0x10100003
#define VTFT_FILTERTARGET_MASK	0x0000ffff	
#define VTFT_FILTERTARGET	0x10000003

#define Z1			0x05		

#define Z2			0x04		

#define PSST			0x06		
#define PSST_FXSENDAMOUNT_C_MASK 0xff000000	

#define PSST_FXSENDAMOUNT_C	0x08180006

#define PSST_LOOPSTARTADDR_MASK	0x00ffffff	
#define PSST_LOOPSTARTADDR	0x18000006

#define DSL			0x07		
#define DSL_FXSENDAMOUNT_D_MASK	0xff000000	

#define DSL_FXSENDAMOUNT_D	0x08180007

#define DSL_LOOPENDADDR_MASK	0x00ffffff	
#define DSL_LOOPENDADDR		0x18000007

#define CCCA			0x08		
#define CCCA_RESONANCE		0xf0000000	
#define CCCA_INTERPROMMASK	0x0e000000	
						
						
						
						
						
#define CCCA_INTERPROM_0	0x00000000	
#define CCCA_INTERPROM_1	0x02000000	
#define CCCA_INTERPROM_2	0x04000000	
#define CCCA_INTERPROM_3	0x06000000	
#define CCCA_INTERPROM_4	0x08000000	
#define CCCA_INTERPROM_5	0x0a000000	
#define CCCA_INTERPROM_6	0x0c000000	
#define CCCA_INTERPROM_7	0x0e000000	
#define CCCA_8BITSELECT		0x01000000	
#define CCCA_CURRADDR_MASK	0x00ffffff	
#define CCCA_CURRADDR		0x18000008

#undef CCR
#define CCR			0x09		
#define CCR_CACHEINVALIDSIZE	0x07190009
#define CCR_CACHEINVALIDSIZE_MASK	0xfe000000	
#define CCR_CACHELOOPFLAG	0x01000000	
#define CCR_INTERLEAVEDSAMPLES	0x00800000	
#define CCR_WORDSIZEDSAMPLES	0x00400000	
#define CCR_READADDRESS		0x06100009
#define CCR_READADDRESS_MASK	0x003f0000	
#define CCR_LOOPINVALSIZE	0x0000fe00	
						
#define CCR_LOOPFLAG		0x00000100	
#define CCR_CACHELOOPADDRHI	0x000000ff	

#define CLP			0x0a		
						
#define CLP_CACHELOOPADDR	0x0000ffff	

#define FXRT			0x0b		
						
						
#define FXRT_CHANNELA		0x000f0000	
#define FXRT_CHANNELB		0x00f00000	
#define FXRT_CHANNELC		0x0f000000	
#define FXRT_CHANNELD		0xf0000000	

#define A_HR			0x0b	
#define MAPA			0x0c		

#define MAPB			0x0d		

#define MAP_PTE_MASK		0xffffe000	
#define MAP_PTI_MASK		0x00001fff	


#define ENVVOL			0x10		
#define ENVVOL_MASK		0x0000ffff	  
						

#define ATKHLDV 		0x11		
#define ATKHLDV_PHASE0		0x00008000	
#define ATKHLDV_HOLDTIME_MASK	0x00007f00	
#define ATKHLDV_ATTACKTIME_MASK	0x0000007f	
						

#define DCYSUSV 		0x12		
#define DCYSUSV_PHASE1_MASK	0x00008000	
#define DCYSUSV_SUSTAINLEVEL_MASK 0x00007f00	
#define DCYSUSV_CHANNELENABLE_MASK 0x00000080	
						
						
#define DCYSUSV_DECAYTIME_MASK	0x0000007f	
						

#define LFOVAL1 		0x13		
#define LFOVAL_MASK		0x0000ffff	
						

#define ENVVAL			0x14		
#define ENVVAL_MASK		0x0000ffff	
						

#define ATKHLDM			0x15		
#define ATKHLDM_PHASE0		0x00008000	
#define ATKHLDM_HOLDTIME	0x00007f00	
#define ATKHLDM_ATTACKTIME	0x0000007f	
						

#define DCYSUSM			0x16		
#define DCYSUSM_PHASE1_MASK	0x00008000	
#define DCYSUSM_SUSTAINLEVEL_MASK 0x00007f00	
#define DCYSUSM_DECAYTIME_MASK	0x0000007f	
						

#define LFOVAL2 		0x17		
#define LFOVAL2_MASK		0x0000ffff	
						

#define IP			0x18		
#define IP_MASK			0x0000ffff	
						
#define IP_UNITY		0x0000e000	

#define IFATN			0x19		
#define IFATN_FILTERCUTOFF_MASK	0x0000ff00	
						
						
#define IFATN_FILTERCUTOFF	0x08080019
#define IFATN_ATTENUATION_MASK	0x000000ff	
#define IFATN_ATTENUATION	0x08000019


#define PEFE			0x1a		
#define PEFE_PITCHAMOUNT_MASK	0x0000ff00	
						
#define PEFE_PITCHAMOUNT	0x0808001a
#define PEFE_FILTERAMOUNT_MASK	0x000000ff	
						
#define PEFE_FILTERAMOUNT	0x0800001a
#define FMMOD			0x1b		
#define FMMOD_MODVIBRATO	0x0000ff00	
						
#define FMMOD_MOFILTER		0x000000ff	
						


#define TREMFRQ 		0x1c		
#define TREMFRQ_DEPTH		0x0000ff00	
						

#define TREMFRQ_FREQUENCY	0x000000ff	
						
#define FM2FRQ2 		0x1d		
#define FM2FRQ2_DEPTH		0x0000ff00	
						
#define FM2FRQ2_FREQUENCY	0x000000ff	
						

#define TEMPENV 		0x1e		
#define TEMPENV_MASK		0x0000ffff	
						
						


#define CD0			0x20		
#define CD1			0x21		
#define CD2			0x22		
#define CD3			0x23		
#define CD4			0x24		
#define CD5			0x25		
#define CD6			0x26		
#define CD7			0x27		
#define CD8			0x28		
#define CD9			0x29		
#define CDA			0x2a		
#define CDB			0x2b		
#define CDC			0x2c		
#define CDD			0x2d		
#define CDE			0x2e		
#define CDF			0x2f		


#define PTB			0x40		
#define PTB_MASK		0xfffff000	

#define TCB			0x41		
#define TCB_MASK		0xfffff000	

#define ADCCR			0x42		
#define ADCCR_RCHANENABLE	0x00000010	
#define ADCCR_LCHANENABLE	0x00000008	
						
						
#define A_ADCCR_RCHANENABLE	0x00000020
#define A_ADCCR_LCHANENABLE	0x00000010

#define A_ADCCR_SAMPLERATE_MASK 0x0000000F      
#define ADCCR_SAMPLERATE_MASK	0x00000007	
#define ADCCR_SAMPLERATE_48	0x00000000	
#define ADCCR_SAMPLERATE_44	0x00000001	
#define ADCCR_SAMPLERATE_32	0x00000002	
#define ADCCR_SAMPLERATE_24	0x00000003	
#define ADCCR_SAMPLERATE_22	0x00000004	
#define ADCCR_SAMPLERATE_16	0x00000005	
#define ADCCR_SAMPLERATE_11	0x00000006	
#define ADCCR_SAMPLERATE_8	0x00000007	
#define A_ADCCR_SAMPLERATE_12	0x00000006	
#define A_ADCCR_SAMPLERATE_11	0x00000007	
#define A_ADCCR_SAMPLERATE_8	0x00000008	

#define FXWC			0x43		
						
						
						
						
						

#define FXWC_DEFAULTROUTE_C     (1<<0)		
#define FXWC_DEFAULTROUTE_B     (1<<1)		
#define FXWC_DEFAULTROUTE_A     (1<<12)
#define FXWC_DEFAULTROUTE_D     (1<<13)
#define FXWC_ADCLEFT            (1<<18)
#define FXWC_CDROMSPDIFLEFT     (1<<18)
#define FXWC_ADCRIGHT           (1<<19)
#define FXWC_CDROMSPDIFRIGHT    (1<<19)
#define FXWC_MIC                (1<<20)
#define FXWC_ZOOMLEFT           (1<<20)
#define FXWC_ZOOMRIGHT          (1<<21)
#define FXWC_SPDIFLEFT          (1<<22)		
#define FXWC_SPDIFRIGHT         (1<<23)		

#define A_TBLSZ			0x43	

#define TCBS			0x44		
#define TCBS_MASK		0x00000007	
#define TCBS_BUFFSIZE_16K	0x00000000
#define TCBS_BUFFSIZE_32K	0x00000001
#define TCBS_BUFFSIZE_64K	0x00000002
#define TCBS_BUFFSIZE_128K	0x00000003
#define TCBS_BUFFSIZE_256K	0x00000004
#define TCBS_BUFFSIZE_512K	0x00000005
#define TCBS_BUFFSIZE_1024K	0x00000006
#define TCBS_BUFFSIZE_2048K	0x00000007

#define MICBA			0x45		
#define MICBA_MASK		0xfffff000	

#define ADCBA			0x46		
#define ADCBA_MASK		0xfffff000	

#define FXBA			0x47		
#define FXBA_MASK		0xfffff000	

#define A_HWM			0x48	

#define MICBS			0x49		

#define ADCBS			0x4a		

#define FXBS			0x4b		


#define ADCBS_BUFSIZE_NONE	0x00000000
#define ADCBS_BUFSIZE_384	0x00000001
#define ADCBS_BUFSIZE_448	0x00000002
#define ADCBS_BUFSIZE_512	0x00000003
#define ADCBS_BUFSIZE_640	0x00000004
#define ADCBS_BUFSIZE_768	0x00000005
#define ADCBS_BUFSIZE_896	0x00000006
#define ADCBS_BUFSIZE_1024	0x00000007
#define ADCBS_BUFSIZE_1280	0x00000008
#define ADCBS_BUFSIZE_1536	0x00000009
#define ADCBS_BUFSIZE_1792	0x0000000a
#define ADCBS_BUFSIZE_2048	0x0000000b
#define ADCBS_BUFSIZE_2560	0x0000000c
#define ADCBS_BUFSIZE_3072	0x0000000d
#define ADCBS_BUFSIZE_3584	0x0000000e
#define ADCBS_BUFSIZE_4096	0x0000000f
#define ADCBS_BUFSIZE_5120	0x00000010
#define ADCBS_BUFSIZE_6144	0x00000011
#define ADCBS_BUFSIZE_7168	0x00000012
#define ADCBS_BUFSIZE_8192	0x00000013
#define ADCBS_BUFSIZE_10240	0x00000014
#define ADCBS_BUFSIZE_12288	0x00000015
#define ADCBS_BUFSIZE_14366	0x00000016
#define ADCBS_BUFSIZE_16384	0x00000017
#define ADCBS_BUFSIZE_20480	0x00000018
#define ADCBS_BUFSIZE_24576	0x00000019
#define ADCBS_BUFSIZE_28672	0x0000001a
#define ADCBS_BUFSIZE_32768	0x0000001b
#define ADCBS_BUFSIZE_40960	0x0000001c
#define ADCBS_BUFSIZE_49152	0x0000001d
#define ADCBS_BUFSIZE_57344	0x0000001e
#define ADCBS_BUFSIZE_65536	0x0000001f

#define A_CSBA			0x4c

#define A_CSDC			0x4d

#define A_CSFE			0x4e

#define A_CSHG			0x4f


#define CDCS			0x50		

#define GPSCS			0x51		

#define DBG			0x52		

#define A_SPSC			0x52

#define REG53			0x53		

#define A_DBG			 0x53
#define A_DBG_SINGLE_STEP	 0x00020000	
#define A_DBG_ZC		 0x40000000	
#define A_DBG_STEP_ADDR		 0x000003ff
#define A_DBG_SATURATION_OCCURED 0x20000000
#define A_DBG_SATURATION_ADDR	 0x0ffc0000

#define SPCS0			0x54		

#define SPCS1			0x55		

#define SPCS2			0x56		

#define SPCS_CLKACCYMASK	0x30000000	
#define SPCS_CLKACCY_1000PPM	0x00000000	
#define SPCS_CLKACCY_50PPM	0x10000000	
#define SPCS_CLKACCY_VARIABLE	0x20000000	
#define SPCS_SAMPLERATEMASK	0x0f000000	
#define SPCS_SAMPLERATE_44	0x00000000	
#define SPCS_SAMPLERATE_48	0x02000000	
#define SPCS_SAMPLERATE_32	0x03000000	
#define SPCS_CHANNELNUMMASK	0x00f00000	
#define SPCS_CHANNELNUM_UNSPEC	0x00000000	
#define SPCS_CHANNELNUM_LEFT	0x00100000	
#define SPCS_CHANNELNUM_RIGHT	0x00200000	
#define SPCS_SOURCENUMMASK	0x000f0000	
#define SPCS_SOURCENUM_UNSPEC	0x00000000	
#define SPCS_GENERATIONSTATUS	0x00008000	
#define SPCS_CATEGORYCODEMASK	0x00007f00	
#define SPCS_MODEMASK		0x000000c0	
#define SPCS_EMPHASISMASK	0x00000038	
#define SPCS_EMPHASIS_NONE	0x00000000	
#define SPCS_EMPHASIS_50_15	0x00000008	
#define SPCS_COPYRIGHT		0x00000004	/* Copyright asserted flag -- do not modify	*/
#define SPCS_NOTAUDIODATA	0x00000002	
#define SPCS_PROFESSIONAL	0x00000001	


#define CLIEL			0x58		

#define CLIEH			0x59		

#define CLIPL			0x5a		

#define CLIPH			0x5b		

#define SOLEL			0x5c		

#define SOLEH			0x5d		

#define SPBYPASS		0x5e		
#define SPBYPASS_SPDIF0_MASK	0x00000003	
#define SPBYPASS_SPDIF1_MASK	0x0000000c	
#define SPBYPASS_FORMAT		0x00000f00      

#define AC97SLOT		0x5f            
#define AC97SLOT_REAR_RIGHT	0x01		
#define AC97SLOT_REAR_LEFT	0x02		
#define AC97SLOT_CNTR		0x10            
#define AC97SLOT_LFE		0x20            

#define A_PCB			0x5f

#define CDSRCS			0x60		

#define GPSRCS			0x61		

#define ZVSRCS			0x62		
						
						

#define SRCS_SPDIFVALID		0x04000000	
#define SRCS_SPDIFLOCKED	0x02000000	
#define SRCS_RATELOCKED		0x01000000	
#define SRCS_ESTSAMPLERATE	0x0007ffff	

#define SRCS_SPDIFRATE_44	0x0003acd9
#define SRCS_SPDIFRATE_48	0x00040000
#define SRCS_SPDIFRATE_96	0x00080000

#define MICIDX                  0x63            
#define MICIDX_MASK             0x0000ffff      
#define MICIDX_IDX		0x10000063

#define ADCIDX			0x64		
#define ADCIDX_MASK		0x0000ffff	
#define ADCIDX_IDX		0x10000064

#define A_ADCIDX		0x63
#define A_ADCIDX_IDX		0x10000063

#define A_MICIDX		0x64
#define A_MICIDX_IDX		0x10000064

#define FXIDX			0x65		
#define FXIDX_MASK		0x0000ffff	
#define FXIDX_IDX		0x10000065

#define HLIEL			0x66		

#define HLIEH			0x67		

#define HLIPL			0x68		

#define HLIPH			0x69		

#define A_SPRI			0x6a
#define A_SPRA			0x6b
#define A_SPRC			0x6c
#define A_DICE			0x6d
#define A_TTB			0x6e
#define A_TDOF			0x6f

#define A_MUDATA1		0x70
#define A_MUCMD1		0x71
#define A_MUSTAT1		A_MUCMD1

#define A_MUDATA2		0x72
#define A_MUCMD2		0x73
#define A_MUSTAT2		A_MUCMD2	

#define A_FXWC1			0x74            
#define A_FXWC2			0x75		

#define A_SPDIF_SAMPLERATE	0x76		
#define A_SAMPLE_RATE		0x76		
#define A_SAMPLE_RATE_NOT_USED  0x0ffc111e	
#define A_SAMPLE_RATE_UNKNOWN	0xf0030001	
#define A_SPDIF_RATE_MASK	0x000000e0	
#define A_SPDIF_48000		0x00000000
#define A_SPDIF_192000		0x00000020
#define A_SPDIF_96000		0x00000040
#define A_SPDIF_44100		0x00000080

#define A_I2S_CAPTURE_RATE_MASK	0x00000e00	
#define A_I2S_CAPTURE_48000	0x00000000	
#define A_I2S_CAPTURE_192000	0x00000200
#define A_I2S_CAPTURE_96000	0x00000400
#define A_I2S_CAPTURE_44100	0x00000800

#define A_PCM_RATE_MASK		0x0000e000	
#define A_PCM_48000		0x00000000
#define A_PCM_192000		0x00002000
#define A_PCM_96000		0x00004000
#define A_PCM_44100		0x00008000

#define A_SRT3			0x77

#define A_SRT4			0x78

#define A_SRT5			0x79

#define A_TTDA			0x7a
#define A_TTDD			0x7b

#define A_FXRT2			0x7c
#define A_FXRT_CHANNELE		0x0000003f	
#define A_FXRT_CHANNELF		0x00003f00	
#define A_FXRT_CHANNELG		0x003f0000	
#define A_FXRT_CHANNELH		0x3f000000	

#define A_SENDAMOUNTS		0x7d
#define A_FXSENDAMOUNT_E_MASK	0xFF000000
#define A_FXSENDAMOUNT_F_MASK	0x00FF0000
#define A_FXSENDAMOUNT_G_MASK	0x0000FF00
#define A_FXSENDAMOUNT_H_MASK	0x000000FF
 
#define A_FXRT1			0x7e
#define A_FXRT_CHANNELA		0x0000003f
#define A_FXRT_CHANNELB		0x00003f00
#define A_FXRT_CHANNELC		0x003f0000
#define A_FXRT_CHANNELD		0x3f000000

#define FXGPREGBASE		0x100		
#define A_FXGPREGBASE		0x400		

#define A_TANKMEMCTLREGBASE	0x100		
#define A_TANKMEMCTLREG_MASK	0x1f		

#define TANKMEMDATAREGBASE	0x200		
#define TANKMEMDATAREG_MASK	0x000fffff	

#define TANKMEMADDRREGBASE	0x300		
#define TANKMEMADDRREG_ADDR_MASK 0x000fffff	
#define TANKMEMADDRREG_CLEAR	0x00800000	
#define TANKMEMADDRREG_ALIGN	0x00400000	
#define TANKMEMADDRREG_WRITE	0x00200000	
#define TANKMEMADDRREG_READ	0x00100000	

#define MICROCODEBASE		0x400		

#define LOWORD_OPX_MASK		0x000ffc00	
#define LOWORD_OPY_MASK		0x000003ff	
#define HIWORD_OPCODE_MASK	0x00f00000	
#define HIWORD_RESULT_MASK	0x000ffc00	
#define HIWORD_OPA_MASK		0x000003ff	


#define A_MICROCODEBASE		0x600
#define A_LOWORD_OPY_MASK	0x000007ff
#define A_LOWORD_OPX_MASK	0x007ff000
#define A_HIWORD_OPCODE_MASK	0x0f000000
#define A_HIWORD_RESULT_MASK	0x007ff000
#define A_HIWORD_OPA_MASK	0x000007ff

#define EMU_HANA_DESTHI		0x00	
#define EMU_HANA_DESTLO		0x01	
#define EMU_HANA_SRCHI		0x02	
#define EMU_HANA_SRCLO		0x03	
#define EMU_HANA_DOCK_PWR	0x04	
#define EMU_HANA_DOCK_PWR_ON		0x01 
#define EMU_HANA_WCLOCK		0x05	
					/* Must be written after power on to reset DLL */
					
#define EMU_HANA_WCLOCK_SRC_MASK	0x07
#define EMU_HANA_WCLOCK_INT_48K		0x00
#define EMU_HANA_WCLOCK_INT_44_1K	0x01
#define EMU_HANA_WCLOCK_HANA_SPDIF_IN	0x02
#define EMU_HANA_WCLOCK_HANA_ADAT_IN	0x03
#define EMU_HANA_WCLOCK_SYNC_BNCN	0x04
#define EMU_HANA_WCLOCK_2ND_HANA	0x05
#define EMU_HANA_WCLOCK_SRC_RESERVED	0x06
#define EMU_HANA_WCLOCK_OFF		0x07 
#define EMU_HANA_WCLOCK_MULT_MASK	0x18
#define EMU_HANA_WCLOCK_1X		0x00
#define EMU_HANA_WCLOCK_2X		0x08
#define EMU_HANA_WCLOCK_4X		0x10
#define EMU_HANA_WCLOCK_MULT_RESERVED	0x18

#define EMU_HANA_DEFCLOCK	0x06	
#define EMU_HANA_DEFCLOCK_48K		0x00
#define EMU_HANA_DEFCLOCK_44_1K		0x01

#define EMU_HANA_UNMUTE		0x07	
#define EMU_MUTE			0x00
#define EMU_UNMUTE			0x01

#define EMU_HANA_FPGA_CONFIG	0x08	
#define EMU_HANA_FPGA_CONFIG_AUDIODOCK	0x01 
#define EMU_HANA_FPGA_CONFIG_HANA	0x02 

#define EMU_HANA_IRQ_ENABLE	0x09	
#define EMU_HANA_IRQ_WCLK_CHANGED	0x01
#define EMU_HANA_IRQ_ADAT		0x02
#define EMU_HANA_IRQ_DOCK		0x04
#define EMU_HANA_IRQ_DOCK_LOST		0x08

#define EMU_HANA_SPDIF_MODE	0x0a	
#define EMU_HANA_SPDIF_MODE_TX_COMSUMER	0x00
#define EMU_HANA_SPDIF_MODE_TX_PRO	0x01
#define EMU_HANA_SPDIF_MODE_TX_NOCOPY	0x02
#define EMU_HANA_SPDIF_MODE_RX_COMSUMER	0x00
#define EMU_HANA_SPDIF_MODE_RX_PRO	0x04
#define EMU_HANA_SPDIF_MODE_RX_NOCOPY	0x08
#define EMU_HANA_SPDIF_MODE_RX_INVALID	0x10

#define EMU_HANA_OPTICAL_TYPE	0x0b	
#define EMU_HANA_OPTICAL_IN_SPDIF	0x00
#define EMU_HANA_OPTICAL_IN_ADAT	0x01
#define EMU_HANA_OPTICAL_OUT_SPDIF	0x00
#define EMU_HANA_OPTICAL_OUT_ADAT	0x02

#define EMU_HANA_MIDI_IN		0x0c	
#define EMU_HANA_MIDI_IN_FROM_HAMOA	0x00 
#define EMU_HANA_MIDI_IN_FROM_DOCK	0x01 

#define EMU_HANA_DOCK_LEDS_1	0x0d	
#define EMU_HANA_DOCK_LEDS_1_MIDI1	0x01	
#define EMU_HANA_DOCK_LEDS_1_MIDI2	0x02	
#define EMU_HANA_DOCK_LEDS_1_SMPTE_IN	0x04	
#define EMU_HANA_DOCK_LEDS_1_SMPTE_OUT	0x08	

#define EMU_HANA_DOCK_LEDS_2	0x0e	
#define EMU_HANA_DOCK_LEDS_2_44K	0x01	
#define EMU_HANA_DOCK_LEDS_2_48K	0x02	
#define EMU_HANA_DOCK_LEDS_2_96K	0x04	
#define EMU_HANA_DOCK_LEDS_2_192K	0x08	
#define EMU_HANA_DOCK_LEDS_2_LOCK	0x10	
#define EMU_HANA_DOCK_LEDS_2_EXT	0x20	

#define EMU_HANA_DOCK_LEDS_3	0x0f	
#define EMU_HANA_DOCK_LEDS_3_CLIP_A	0x01	
#define EMU_HANA_DOCK_LEDS_3_CLIP_B	0x02	
#define EMU_HANA_DOCK_LEDS_3_SIGNAL_A	0x04	
#define EMU_HANA_DOCK_LEDS_3_SIGNAL_B	0x08	
#define EMU_HANA_DOCK_LEDS_3_MANUAL_CLIP	0x10	
#define EMU_HANA_DOCK_LEDS_3_MANUAL_SIGNAL	0x20	

#define EMU_HANA_ADC_PADS	0x10	
#define EMU_HANA_DOCK_ADC_PAD1	0x01	
#define EMU_HANA_DOCK_ADC_PAD2	0x02	
#define EMU_HANA_DOCK_ADC_PAD3	0x04	
#define EMU_HANA_0202_ADC_PAD1	0x08	

#define EMU_HANA_DOCK_MISC	0x11	
#define EMU_HANA_DOCK_DAC1_MUTE	0x01	
#define EMU_HANA_DOCK_DAC2_MUTE	0x02	
#define EMU_HANA_DOCK_DAC3_MUTE	0x04	
#define EMU_HANA_DOCK_DAC4_MUTE	0x08	
#define EMU_HANA_DOCK_PHONES_192_DAC1	0x00	
#define EMU_HANA_DOCK_PHONES_192_DAC2	0x10	
#define EMU_HANA_DOCK_PHONES_192_DAC3	0x20	
#define EMU_HANA_DOCK_PHONES_192_DAC4	0x30	

#define EMU_HANA_MIDI_OUT	0x12	
#define EMU_HANA_MIDI_OUT_0202	0x01 
#define EMU_HANA_MIDI_OUT_DOCK1	0x02 
#define EMU_HANA_MIDI_OUT_DOCK2	0x04 
#define EMU_HANA_MIDI_OUT_SYNC2	0x08 
#define EMU_HANA_MIDI_OUT_LOOP	0x10 

#define EMU_HANA_DAC_PADS	0x13	
#define EMU_HANA_DOCK_DAC_PAD1	0x01	
#define EMU_HANA_DOCK_DAC_PAD2	0x02	
#define EMU_HANA_DOCK_DAC_PAD3	0x04	
#define EMU_HANA_DOCK_DAC_PAD4	0x08	
#define EMU_HANA_0202_DAC_PAD1	0x10	

#define EMU_HANA_IRQ_STATUS	0x20	
#if 0  
#define EMU_HANA_IRQ_WCLK_CHANGED	0x01
#define EMU_HANA_IRQ_ADAT		0x02
#define EMU_HANA_IRQ_DOCK		0x04
#define EMU_HANA_IRQ_DOCK_LOST		0x08
#endif

#define EMU_HANA_OPTION_CARDS	0x21	
#define EMU_HANA_OPTION_HAMOA	0x01	
#define EMU_HANA_OPTION_SYNC	0x02	
#define EMU_HANA_OPTION_DOCK_ONLINE	0x04	
#define EMU_HANA_OPTION_DOCK_OFFLINE	0x08	

#define EMU_HANA_ID		0x22	

#define EMU_HANA_MAJOR_REV	0x23	
#define EMU_HANA_MINOR_REV	0x24	

#define EMU_DOCK_MAJOR_REV	0x25	
#define EMU_DOCK_MINOR_REV	0x26	

#define EMU_DOCK_BOARD_ID	0x27	
#define EMU_DOCK_BOARD_ID0	0x00	
#define EMU_DOCK_BOARD_ID1	0x03	

#define EMU_HANA_WC_SPDIF_HI	0x28	
#define EMU_HANA_WC_SPDIF_LO	0x29	

#define EMU_HANA_WC_ADAT_HI	0x2a	
#define EMU_HANA_WC_ADAT_LO	0x2b	

#define EMU_HANA_WC_BNC_LO	0x2c	
#define EMU_HANA_WC_BNC_HI	0x2d	

#define EMU_HANA2_WC_SPDIF_HI	0x2e	
#define EMU_HANA2_WC_SPDIF_LO	0x2f	

#define EMU_DST_ALICE2_EMU32_0	0x000f	
#define EMU_DST_ALICE2_EMU32_1	0x0000	
#define EMU_DST_ALICE2_EMU32_2	0x0001	
#define EMU_DST_ALICE2_EMU32_3	0x0002	
#define EMU_DST_ALICE2_EMU32_4	0x0003	
#define EMU_DST_ALICE2_EMU32_5	0x0004	
#define EMU_DST_ALICE2_EMU32_6	0x0005	
#define EMU_DST_ALICE2_EMU32_7	0x0006	
#define EMU_DST_ALICE2_EMU32_8	0x0007	
#define EMU_DST_ALICE2_EMU32_9	0x0008	
#define EMU_DST_ALICE2_EMU32_A	0x0009	
#define EMU_DST_ALICE2_EMU32_B	0x000a	
#define EMU_DST_ALICE2_EMU32_C	0x000b	
#define EMU_DST_ALICE2_EMU32_D	0x000c	
#define EMU_DST_ALICE2_EMU32_E	0x000d	
#define EMU_DST_ALICE2_EMU32_F	0x000e	
#define EMU_DST_DOCK_DAC1_LEFT1	0x0100	
#define EMU_DST_DOCK_DAC1_LEFT2	0x0101	
#define EMU_DST_DOCK_DAC1_LEFT3	0x0102	
#define EMU_DST_DOCK_DAC1_LEFT4	0x0103	
#define EMU_DST_DOCK_DAC1_RIGHT1	0x0104	
#define EMU_DST_DOCK_DAC1_RIGHT2	0x0105	
#define EMU_DST_DOCK_DAC1_RIGHT3	0x0106	
#define EMU_DST_DOCK_DAC1_RIGHT4	0x0107	
#define EMU_DST_DOCK_DAC2_LEFT1	0x0108	
#define EMU_DST_DOCK_DAC2_LEFT2	0x0109	
#define EMU_DST_DOCK_DAC2_LEFT3	0x010a	
#define EMU_DST_DOCK_DAC2_LEFT4	0x010b	
#define EMU_DST_DOCK_DAC2_RIGHT1	0x010c	
#define EMU_DST_DOCK_DAC2_RIGHT2	0x010d	
#define EMU_DST_DOCK_DAC2_RIGHT3	0x010e	
#define EMU_DST_DOCK_DAC2_RIGHT4	0x010f	
#define EMU_DST_DOCK_DAC3_LEFT1	0x0110	
#define EMU_DST_DOCK_DAC3_LEFT2	0x0111	
#define EMU_DST_DOCK_DAC3_LEFT3	0x0112	
#define EMU_DST_DOCK_DAC3_LEFT4	0x0113	
#define EMU_DST_DOCK_PHONES_LEFT1	0x0112	
#define EMU_DST_DOCK_PHONES_LEFT2	0x0113	
#define EMU_DST_DOCK_DAC3_RIGHT1	0x0114	
#define EMU_DST_DOCK_DAC3_RIGHT2	0x0115	
#define EMU_DST_DOCK_DAC3_RIGHT3	0x0116	
#define EMU_DST_DOCK_DAC3_RIGHT4	0x0117	
#define EMU_DST_DOCK_PHONES_RIGHT1	0x0116	
#define EMU_DST_DOCK_PHONES_RIGHT2	0x0117	
#define EMU_DST_DOCK_DAC4_LEFT1	0x0118	
#define EMU_DST_DOCK_DAC4_LEFT2	0x0119	
#define EMU_DST_DOCK_DAC4_LEFT3	0x011a	
#define EMU_DST_DOCK_DAC4_LEFT4	0x011b	
#define EMU_DST_DOCK_SPDIF_LEFT1	0x011a	
#define EMU_DST_DOCK_SPDIF_LEFT2	0x011b	
#define EMU_DST_DOCK_DAC4_RIGHT1	0x011c	
#define EMU_DST_DOCK_DAC4_RIGHT2	0x011d	
#define EMU_DST_DOCK_DAC4_RIGHT3	0x011e	
#define EMU_DST_DOCK_DAC4_RIGHT4	0x011f	
#define EMU_DST_DOCK_SPDIF_RIGHT1	0x011e	
#define EMU_DST_DOCK_SPDIF_RIGHT2	0x011f	
#define EMU_DST_HANA_SPDIF_LEFT1	0x0200	
#define EMU_DST_HANA_SPDIF_LEFT2	0x0202	
#define EMU_DST_HANA_SPDIF_RIGHT1	0x0201	
#define EMU_DST_HANA_SPDIF_RIGHT2	0x0203	
#define EMU_DST_HAMOA_DAC_LEFT1	0x0300	
#define EMU_DST_HAMOA_DAC_LEFT2	0x0302	
#define EMU_DST_HAMOA_DAC_LEFT3	0x0304	
#define EMU_DST_HAMOA_DAC_LEFT4	0x0306	
#define EMU_DST_HAMOA_DAC_RIGHT1	0x0301	
#define EMU_DST_HAMOA_DAC_RIGHT2	0x0303	
#define EMU_DST_HAMOA_DAC_RIGHT3	0x0305	
#define EMU_DST_HAMOA_DAC_RIGHT4	0x0307	
#define EMU_DST_HANA_ADAT	0x0400	
#define EMU_DST_ALICE_I2S0_LEFT		0x0500	
#define EMU_DST_ALICE_I2S0_RIGHT	0x0501	
#define EMU_DST_ALICE_I2S1_LEFT		0x0600	
#define EMU_DST_ALICE_I2S1_RIGHT	0x0601	
#define EMU_DST_ALICE_I2S2_LEFT		0x0700	
#define EMU_DST_ALICE_I2S2_RIGHT	0x0701	

#define EMU_DST_MDOCK_SPDIF_LEFT1	0x0112
#define EMU_DST_MDOCK_SPDIF_LEFT2	0x0113
#define EMU_DST_MDOCK_SPDIF_RIGHT1	0x0116
#define EMU_DST_MDOCK_SPDIF_RIGHT2	0x0117
#define EMU_DST_MDOCK_ADAT		0x0118

#define EMU_DST_MANA_DAC_LEFT		0x0300
#define EMU_DST_MANA_DAC_RIGHT		0x0301


#define EMU_SRC_SILENCE		0x0000	
#define EMU_SRC_DOCK_MIC_A1	0x0100	
#define EMU_SRC_DOCK_MIC_A2	0x0101	
#define EMU_SRC_DOCK_MIC_A3	0x0102	
#define EMU_SRC_DOCK_MIC_A4	0x0103	
#define EMU_SRC_DOCK_MIC_B1	0x0104	
#define EMU_SRC_DOCK_MIC_B2	0x0105	
#define EMU_SRC_DOCK_MIC_B3	0x0106	
#define EMU_SRC_DOCK_MIC_B4	0x0107	
#define EMU_SRC_DOCK_ADC1_LEFT1	0x0108	
#define EMU_SRC_DOCK_ADC1_LEFT2	0x0109	
#define EMU_SRC_DOCK_ADC1_LEFT3	0x010a	
#define EMU_SRC_DOCK_ADC1_LEFT4	0x010b	
#define EMU_SRC_DOCK_ADC1_RIGHT1	0x010c	
#define EMU_SRC_DOCK_ADC1_RIGHT2	0x010d	
#define EMU_SRC_DOCK_ADC1_RIGHT3	0x010e	
#define EMU_SRC_DOCK_ADC1_RIGHT4	0x010f	
#define EMU_SRC_DOCK_ADC2_LEFT1	0x0110	
#define EMU_SRC_DOCK_ADC2_LEFT2	0x0111	
#define EMU_SRC_DOCK_ADC2_LEFT3	0x0112	
#define EMU_SRC_DOCK_ADC2_LEFT4	0x0113	
#define EMU_SRC_DOCK_ADC2_RIGHT1	0x0114	
#define EMU_SRC_DOCK_ADC2_RIGHT2	0x0115	
#define EMU_SRC_DOCK_ADC2_RIGHT3	0x0116	
#define EMU_SRC_DOCK_ADC2_RIGHT4	0x0117	
#define EMU_SRC_DOCK_ADC3_LEFT1	0x0118	
#define EMU_SRC_DOCK_ADC3_LEFT2	0x0119	
#define EMU_SRC_DOCK_ADC3_LEFT3	0x011a	
#define EMU_SRC_DOCK_ADC3_LEFT4	0x011b	
#define EMU_SRC_DOCK_ADC3_RIGHT1	0x011c	
#define EMU_SRC_DOCK_ADC3_RIGHT2	0x011d	
#define EMU_SRC_DOCK_ADC3_RIGHT3	0x011e	
#define EMU_SRC_DOCK_ADC3_RIGHT4	0x011f	
#define EMU_SRC_HAMOA_ADC_LEFT1	0x0200	
#define EMU_SRC_HAMOA_ADC_LEFT2	0x0202	
#define EMU_SRC_HAMOA_ADC_LEFT3	0x0204	
#define EMU_SRC_HAMOA_ADC_LEFT4	0x0206	
#define EMU_SRC_HAMOA_ADC_RIGHT1	0x0201	
#define EMU_SRC_HAMOA_ADC_RIGHT2	0x0203	
#define EMU_SRC_HAMOA_ADC_RIGHT3	0x0205	
#define EMU_SRC_HAMOA_ADC_RIGHT4	0x0207	
#define EMU_SRC_ALICE_EMU32A		0x0300	
#define EMU_SRC_ALICE_EMU32B		0x0310	
#define EMU_SRC_HANA_ADAT	0x0400	
#define EMU_SRC_HANA_SPDIF_LEFT1	0x0500	
#define EMU_SRC_HANA_SPDIF_LEFT2	0x0502	
#define EMU_SRC_HANA_SPDIF_RIGHT1	0x0501	
#define EMU_SRC_HANA_SPDIF_RIGHT2	0x0503	

#define EMU_SRC_MDOCK_SPDIF_LEFT1	0x0112
#define EMU_SRC_MDOCK_SPDIF_LEFT2	0x0113
#define EMU_SRC_MDOCK_SPDIF_RIGHT1	0x0116
#define EMU_SRC_MDOCK_SPDIF_RIGHT2	0x0117
#define EMU_SRC_MDOCK_ADAT		0x0118



enum {
	EMU10K1_EFX,
	EMU10K1_PCM,
	EMU10K1_SYNTH,
	EMU10K1_MIDI
};

struct snd_emu10k1;

struct snd_emu10k1_voice {
	struct snd_emu10k1 *emu;
	int number;
	unsigned int use: 1,
	    pcm: 1,
	    efx: 1,
	    synth: 1,
	    midi: 1;
	void (*interrupt)(struct snd_emu10k1 *emu, struct snd_emu10k1_voice *pvoice);

	struct snd_emu10k1_pcm *epcm;
};

enum {
	PLAYBACK_EMUVOICE,
	PLAYBACK_EFX,
	CAPTURE_AC97ADC,
	CAPTURE_AC97MIC,
	CAPTURE_EFX
};

struct snd_emu10k1_pcm {
	struct snd_emu10k1 *emu;
	int type;
	struct snd_pcm_substream *substream;
	struct snd_emu10k1_voice *voices[NUM_EFX_PLAYBACK];
	struct snd_emu10k1_voice *extra;
	unsigned short running;
	unsigned short first_ptr;
	struct snd_util_memblk *memblk;
	unsigned int start_addr;
	unsigned int ccca_start_addr;
	unsigned int capture_ipr;	
	unsigned int capture_inte;	
	unsigned int capture_ba_reg;	
	unsigned int capture_bs_reg;	
	unsigned int capture_idx_reg;	
	unsigned int capture_cr_val;	
	unsigned int capture_cr_val2;	
	unsigned int capture_bs_val;	
	unsigned int capture_bufsize;	
};

struct snd_emu10k1_pcm_mixer {
	
	unsigned char send_routing[3][8];
	unsigned char send_volume[3][8];
	unsigned short attn[3];
	struct snd_emu10k1_pcm *epcm;
};

#define snd_emu10k1_compose_send_routing(route) \
((route[0] | (route[1] << 4) | (route[2] << 8) | (route[3] << 12)) << 16)

#define snd_emu10k1_compose_audigy_fxrt1(route) \
((unsigned int)route[0] | ((unsigned int)route[1] << 8) | ((unsigned int)route[2] << 16) | ((unsigned int)route[3] << 24))

#define snd_emu10k1_compose_audigy_fxrt2(route) \
((unsigned int)route[4] | ((unsigned int)route[5] << 8) | ((unsigned int)route[6] << 16) | ((unsigned int)route[7] << 24))

struct snd_emu10k1_memblk {
	struct snd_util_memblk mem;
	
	int first_page, last_page, pages, mapped_page;
	unsigned int map_locked;
	struct list_head mapped_link;
	struct list_head mapped_order_link;
};

#define snd_emu10k1_memblk_offset(blk)	(((blk)->mapped_page << PAGE_SHIFT) | ((blk)->mem.offset & (PAGE_SIZE - 1)))

#define EMU10K1_MAX_TRAM_BLOCKS_PER_CODE	16

struct snd_emu10k1_fx8010_ctl {
	struct list_head list;		
	unsigned int vcount;
	unsigned int count;		
	unsigned short gpr[32];		
	unsigned int value[32];
	unsigned int min;		
	unsigned int max;		
	unsigned int translation;	
	struct snd_kcontrol *kcontrol;
};

typedef void (snd_fx8010_irq_handler_t)(struct snd_emu10k1 *emu, void *private_data);

struct snd_emu10k1_fx8010_irq {
	struct snd_emu10k1_fx8010_irq *next;
	snd_fx8010_irq_handler_t *handler;
	unsigned short gpr_running;
	void *private_data;
};

struct snd_emu10k1_fx8010_pcm {
	unsigned int valid: 1,
		     opened: 1,
		     active: 1;
	unsigned int channels;		
	unsigned int tram_start;	
	unsigned int buffer_size;	
	unsigned short gpr_size;		
	unsigned short gpr_ptr;		
	unsigned short gpr_count;	
	unsigned short gpr_tmpcount;	
	unsigned short gpr_trigger;	
	unsigned short gpr_running;	
	unsigned char etram[32];	
	struct snd_pcm_indirect pcm_rec;
	unsigned int tram_pos;
	unsigned int tram_shift;
	struct snd_emu10k1_fx8010_irq *irq;
};

struct snd_emu10k1_fx8010 {
	unsigned short fxbus_mask;	
	unsigned short extin_mask;	
	unsigned short extout_mask;	
	unsigned short pad1;
	unsigned int itram_size;	
	struct snd_dma_buffer etram_pages; 
	unsigned int dbg;		
	unsigned char name[128];
	int gpr_size;			
	int gpr_count;			
	struct list_head gpr_ctl;	
	struct mutex lock;
	struct snd_emu10k1_fx8010_pcm pcm[8];
	spinlock_t irq_lock;
	struct snd_emu10k1_fx8010_irq *irq_handlers;
};

struct snd_emu10k1_midi {
	struct snd_emu10k1 *emu;
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *substream_input;
	struct snd_rawmidi_substream *substream_output;
	unsigned int midi_mode;
	spinlock_t input_lock;
	spinlock_t output_lock;
	spinlock_t open_lock;
	int tx_enable, rx_enable;
	int port;
	int ipr_tx, ipr_rx;
	void (*interrupt)(struct snd_emu10k1 *emu, unsigned int status);
};

enum {
	EMU_MODEL_SB,
	EMU_MODEL_EMU1010,
	EMU_MODEL_EMU1010B,
	EMU_MODEL_EMU1616,
	EMU_MODEL_EMU0404,
};

struct snd_emu_chip_details {
	u32 vendor;
	u32 device;
	u32 subsystem;
	unsigned char revision;
	unsigned char emu10k1_chip; 
	unsigned char emu10k2_chip; 
	unsigned char ca0102_chip;  
	unsigned char ca0108_chip;  
	unsigned char ca_cardbus_chip; 
	unsigned char ca0151_chip;  
	unsigned char spk71;        
	unsigned char sblive51;	    
	unsigned char spdif_bug;    
	unsigned char ac97_chip;    
	unsigned char ecard;        
	unsigned char emu_model;     
	unsigned char spi_dac;      
	unsigned char i2c_adc;      
	unsigned char adc_1361t;    
	unsigned char invert_shared_spdif; 
	const char *driver;
	const char *name;
	const char *id;		
};

struct snd_emu1010 {
	unsigned int output_source[64];
	unsigned int input_source[64];
	unsigned int adc_pads; 
	unsigned int dac_pads; 
	unsigned int internal_clock; 
	unsigned int optical_in; 
	unsigned int optical_out; 
	struct task_struct *firmware_thread;
};

struct snd_emu10k1 {
	int irq;

	unsigned long port;			
	unsigned int tos_link: 1,		
		rear_ac97: 1,			
		enable_ir: 1;
	unsigned int support_tlv :1;
	
	const struct snd_emu_chip_details *card_capabilities;
	unsigned int audigy;			
	unsigned int revision;			
	unsigned int serial;			
	unsigned short model;			
	unsigned int card_type;			
	unsigned int ecard_ctrl;		
	unsigned long dma_mask;			
	unsigned int delay_pcm_irq;		
	int max_cache_pages;			
	struct snd_dma_buffer silent_page;	
	struct snd_dma_buffer ptb_pages;	
	struct snd_dma_device p16v_dma_dev;
	struct snd_dma_buffer p16v_buffer;

	struct snd_util_memhdr *memhdr;		
	struct snd_emu10k1_memblk *reserved_page;	

	struct list_head mapped_link_head;
	struct list_head mapped_order_link_head;
	void **page_ptr_table;
	unsigned long *page_addr_table;
	spinlock_t memblk_lock;

	unsigned int spdif_bits[3];		
	unsigned int i2c_capture_source;
	u8 i2c_capture_volume[4][2];

	struct snd_emu10k1_fx8010 fx8010;		
	int gpr_base;
	
	struct snd_ac97 *ac97;

	struct pci_dev *pci;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_mic;
	struct snd_pcm *pcm_efx;
	struct snd_pcm *pcm_multi;
	struct snd_pcm *pcm_p16v;

	spinlock_t synth_lock;
	void *synth;
	int (*get_synth_voice)(struct snd_emu10k1 *emu);

	spinlock_t reg_lock;
	spinlock_t emu_lock;
	spinlock_t voice_lock;
	spinlock_t spi_lock; 
	spinlock_t i2c_lock; 

	struct snd_emu10k1_voice voices[NUM_G];
	struct snd_emu10k1_voice p16v_voices[4];
	struct snd_emu10k1_voice p16v_capture_voice;
	int p16v_device_offset;
	u32 p16v_capture_source;
	u32 p16v_capture_channel;
        struct snd_emu1010 emu1010;
	struct snd_emu10k1_pcm_mixer pcm_mixer[32];
	struct snd_emu10k1_pcm_mixer efx_pcm_mixer[NUM_EFX_PLAYBACK];
	struct snd_kcontrol *ctl_send_routing;
	struct snd_kcontrol *ctl_send_volume;
	struct snd_kcontrol *ctl_attn;
	struct snd_kcontrol *ctl_efx_send_routing;
	struct snd_kcontrol *ctl_efx_send_volume;
	struct snd_kcontrol *ctl_efx_attn;

	void (*hwvol_interrupt)(struct snd_emu10k1 *emu, unsigned int status);
	void (*capture_interrupt)(struct snd_emu10k1 *emu, unsigned int status);
	void (*capture_mic_interrupt)(struct snd_emu10k1 *emu, unsigned int status);
	void (*capture_efx_interrupt)(struct snd_emu10k1 *emu, unsigned int status);
	void (*spdif_interrupt)(struct snd_emu10k1 *emu, unsigned int status);
	void (*dsp_interrupt)(struct snd_emu10k1 *emu);

	struct snd_pcm_substream *pcm_capture_substream;
	struct snd_pcm_substream *pcm_capture_mic_substream;
	struct snd_pcm_substream *pcm_capture_efx_substream;
	struct snd_pcm_substream *pcm_playback_efx_substream;

	struct snd_timer *timer;

	struct snd_emu10k1_midi midi;
	struct snd_emu10k1_midi midi2; 

	unsigned int efx_voices_mask[2];
	unsigned int next_free_voice;

#ifdef CONFIG_PM
	unsigned int *saved_ptr;
	unsigned int *saved_gpr;
	unsigned int *tram_val_saved;
	unsigned int *tram_addr_saved;
	unsigned int *saved_icode;
	unsigned int *p16v_saved;
	unsigned int saved_a_iocfg, saved_hcfg;
#endif

};

int snd_emu10k1_create(struct snd_card *card,
		       struct pci_dev *pci,
		       unsigned short extin_mask,
		       unsigned short extout_mask,
		       long max_cache_bytes,
		       int enable_ir,
		       uint subsystem,
		       struct snd_emu10k1 ** remu);

int snd_emu10k1_pcm(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_emu10k1_pcm_mic(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_emu10k1_pcm_efx(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_p16v_pcm(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_p16v_free(struct snd_emu10k1 * emu);
int snd_p16v_mixer(struct snd_emu10k1 * emu);
int snd_emu10k1_pcm_multi(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_emu10k1_fx8010_pcm(struct snd_emu10k1 * emu, int device, struct snd_pcm ** rpcm);
int snd_emu10k1_mixer(struct snd_emu10k1 * emu, int pcm_device, int multi_device);
int snd_emu10k1_timer(struct snd_emu10k1 * emu, int device);
int snd_emu10k1_fx8010_new(struct snd_emu10k1 *emu, int device, struct snd_hwdep ** rhwdep);

irqreturn_t snd_emu10k1_interrupt(int irq, void *dev_id);

void snd_emu10k1_voice_init(struct snd_emu10k1 * emu, int voice);
int snd_emu10k1_init_efx(struct snd_emu10k1 *emu);
void snd_emu10k1_free_efx(struct snd_emu10k1 *emu);
int snd_emu10k1_fx8010_tram_setup(struct snd_emu10k1 *emu, u32 size);
int snd_emu10k1_done(struct snd_emu10k1 * emu);

unsigned int snd_emu10k1_ptr_read(struct snd_emu10k1 * emu, unsigned int reg, unsigned int chn);
void snd_emu10k1_ptr_write(struct snd_emu10k1 *emu, unsigned int reg, unsigned int chn, unsigned int data);
unsigned int snd_emu10k1_ptr20_read(struct snd_emu10k1 * emu, unsigned int reg, unsigned int chn);
void snd_emu10k1_ptr20_write(struct snd_emu10k1 *emu, unsigned int reg, unsigned int chn, unsigned int data);
int snd_emu10k1_spi_write(struct snd_emu10k1 * emu, unsigned int data);
int snd_emu10k1_i2c_write(struct snd_emu10k1 *emu, u32 reg, u32 value);
int snd_emu1010_fpga_write(struct snd_emu10k1 * emu, u32 reg, u32 value);
int snd_emu1010_fpga_read(struct snd_emu10k1 * emu, u32 reg, u32 *value);
int snd_emu1010_fpga_link_dst_src_write(struct snd_emu10k1 * emu, u32 dst, u32 src);
unsigned int snd_emu10k1_efx_read(struct snd_emu10k1 *emu, unsigned int pc);
void snd_emu10k1_intr_enable(struct snd_emu10k1 *emu, unsigned int intrenb);
void snd_emu10k1_intr_disable(struct snd_emu10k1 *emu, unsigned int intrenb);
void snd_emu10k1_voice_intr_enable(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_intr_disable(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_intr_ack(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_half_loop_intr_enable(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_half_loop_intr_disable(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_half_loop_intr_ack(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_set_loop_stop(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_voice_clear_loop_stop(struct snd_emu10k1 *emu, unsigned int voicenum);
void snd_emu10k1_wait(struct snd_emu10k1 *emu, unsigned int wait);
static inline unsigned int snd_emu10k1_wc(struct snd_emu10k1 *emu) { return (inl(emu->port + WC) >> 6) & 0xfffff; }
unsigned short snd_emu10k1_ac97_read(struct snd_ac97 *ac97, unsigned short reg);
void snd_emu10k1_ac97_write(struct snd_ac97 *ac97, unsigned short reg, unsigned short data);
unsigned int snd_emu10k1_rate_to_pitch(unsigned int rate);

#ifdef CONFIG_PM
void snd_emu10k1_suspend_regs(struct snd_emu10k1 *emu);
void snd_emu10k1_resume_init(struct snd_emu10k1 *emu);
void snd_emu10k1_resume_regs(struct snd_emu10k1 *emu);
int snd_emu10k1_efx_alloc_pm_buffer(struct snd_emu10k1 *emu);
void snd_emu10k1_efx_free_pm_buffer(struct snd_emu10k1 *emu);
void snd_emu10k1_efx_suspend(struct snd_emu10k1 *emu);
void snd_emu10k1_efx_resume(struct snd_emu10k1 *emu);
int snd_p16v_alloc_pm_buffer(struct snd_emu10k1 *emu);
void snd_p16v_free_pm_buffer(struct snd_emu10k1 *emu);
void snd_p16v_suspend(struct snd_emu10k1 *emu);
void snd_p16v_resume(struct snd_emu10k1 *emu);
#endif

struct snd_util_memblk *snd_emu10k1_alloc_pages(struct snd_emu10k1 *emu, struct snd_pcm_substream *substream);
int snd_emu10k1_free_pages(struct snd_emu10k1 *emu, struct snd_util_memblk *blk);
struct snd_util_memblk *snd_emu10k1_synth_alloc(struct snd_emu10k1 *emu, unsigned int size);
int snd_emu10k1_synth_free(struct snd_emu10k1 *emu, struct snd_util_memblk *blk);
int snd_emu10k1_synth_bzero(struct snd_emu10k1 *emu, struct snd_util_memblk *blk, int offset, int size);
int snd_emu10k1_synth_copy_from_user(struct snd_emu10k1 *emu, struct snd_util_memblk *blk, int offset, const char __user *data, int size);
int snd_emu10k1_memblk_map(struct snd_emu10k1 *emu, struct snd_emu10k1_memblk *blk);

int snd_emu10k1_voice_alloc(struct snd_emu10k1 *emu, int type, int pair, struct snd_emu10k1_voice **rvoice);
int snd_emu10k1_voice_free(struct snd_emu10k1 *emu, struct snd_emu10k1_voice *pvoice);

int snd_emu10k1_midi(struct snd_emu10k1 * emu);
int snd_emu10k1_audigy_midi(struct snd_emu10k1 * emu);

int snd_emu10k1_proc_init(struct snd_emu10k1 * emu);

int snd_emu10k1_fx8010_register_irq_handler(struct snd_emu10k1 *emu,
					    snd_fx8010_irq_handler_t *handler,
					    unsigned char gpr_running,
					    void *private_data,
					    struct snd_emu10k1_fx8010_irq **r_irq);
int snd_emu10k1_fx8010_unregister_irq_handler(struct snd_emu10k1 *emu,
					      struct snd_emu10k1_fx8010_irq *irq);

#endif 


#define EMU10K1_CARD_CREATIVE			0x00000000
#define EMU10K1_CARD_EMUAPS			0x00000001

#define EMU10K1_FX8010_PCM_COUNT		8

#define iMAC0	 0x00	
#define iMAC1	 0x01	
#define iMAC2	 0x02	
#define iMAC3	 0x03	
#define iMACINT0 0x04	
#define iMACINT1 0x05	
#define iACC3	 0x06	
#define iMACMV   0x07	
#define iANDXOR  0x08	
#define iTSTNEG  0x09	
#define iLIMITGE 0x0a	
#define iLIMITLT 0x0b	
#define iLOG	 0x0c	
#define iEXP	 0x0d	
#define iINTERP  0x0e	
#define iSKIP    0x0f	

#define FXBUS(x)	(0x00 + (x))	
#define EXTIN(x)	(0x10 + (x))	
#define EXTOUT(x)	(0x20 + (x))	
#define FXBUS2(x)	(0x30 + (x))	
					

#define C_00000000	0x40
#define C_00000001	0x41
#define C_00000002	0x42
#define C_00000003	0x43
#define C_00000004	0x44
#define C_00000008	0x45
#define C_00000010	0x46
#define C_00000020	0x47
#define C_00000100	0x48
#define C_00010000	0x49
#define C_00080000	0x4a
#define C_10000000	0x4b
#define C_20000000	0x4c
#define C_40000000	0x4d
#define C_80000000	0x4e
#define C_7fffffff	0x4f
#define C_ffffffff	0x50
#define C_fffffffe	0x51
#define C_c0000000	0x52
#define C_4f1bbcdc	0x53
#define C_5a7ef9db	0x54
#define C_00100000	0x55		
#define GPR_ACCU	0x56		
#define GPR_COND	0x57		
#define GPR_NOISE0	0x58		
#define GPR_NOISE1	0x59		
#define GPR_IRQ		0x5a		
#define GPR_DBAC	0x5b		
#define GPR(x)		(FXGPREGBASE + (x)) 
#define ITRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x00 + (x)) 
#define ETRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x80 + (x)) 
#define ITRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x00 + (x)) 
#define ETRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x80 + (x)) 

#define A_ITRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x00 + (x)) 
#define A_ETRAM_DATA(x)	(TANKMEMDATAREGBASE + 0xc0 + (x)) 
#define A_ITRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x00 + (x)) 
#define A_ETRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0xc0 + (x)) 
#define A_ITRAM_CTL(x)	(A_TANKMEMCTLREGBASE + 0x00 + (x)) 
#define A_ETRAM_CTL(x)	(A_TANKMEMCTLREGBASE + 0xc0 + (x)) 

#define A_FXBUS(x)	(0x00 + (x))	
#define A_EXTIN(x)	(0x40 + (x))	
#define A_P16VIN(x)	(0x50 + (x))	
#define A_EXTOUT(x)	(0x60 + (x))	
#define A_FXBUS2(x)	(0x80 + (x))	
#define A_EMU32OUTH(x)	(0xa0 + (x))	
#define A_EMU32OUTL(x)	(0xb0 + (x))	
#define A3_EMU32IN(x)	(0x160 + (x))	
#define A3_EMU32OUT(x)	(0x1E0 + (x))	
#define A_GPR(x)	(A_FXGPREGBASE + (x))

#define CC_REG_NORMALIZED C_00000001
#define CC_REG_BORROW	C_00000002
#define CC_REG_MINUS	C_00000004
#define CC_REG_ZERO	C_00000008
#define CC_REG_SATURATE	C_00000010
#define CC_REG_NONZERO	C_00000100

#define FXBUS_PCM_LEFT		0x00
#define FXBUS_PCM_RIGHT		0x01
#define FXBUS_PCM_LEFT_REAR	0x02
#define FXBUS_PCM_RIGHT_REAR	0x03
#define FXBUS_MIDI_LEFT		0x04
#define FXBUS_MIDI_RIGHT	0x05
#define FXBUS_PCM_CENTER	0x06
#define FXBUS_PCM_LFE		0x07
#define FXBUS_PCM_LEFT_FRONT	0x08
#define FXBUS_PCM_RIGHT_FRONT	0x09
#define FXBUS_MIDI_REVERB	0x0c
#define FXBUS_MIDI_CHORUS	0x0d
#define FXBUS_PCM_LEFT_SIDE	0x0e
#define FXBUS_PCM_RIGHT_SIDE	0x0f
#define FXBUS_PT_LEFT		0x14
#define FXBUS_PT_RIGHT		0x15

#define EXTIN_AC97_L	   0x00	
#define EXTIN_AC97_R	   0x01	
#define EXTIN_SPDIF_CD_L   0x02	
#define EXTIN_SPDIF_CD_R   0x03	
#define EXTIN_ZOOM_L	   0x04	
#define EXTIN_ZOOM_R	   0x05	
#define EXTIN_TOSLINK_L	   0x06	
#define EXTIN_TOSLINK_R    0x07	
#define EXTIN_LINE1_L	   0x08	
#define EXTIN_LINE1_R	   0x09	
#define EXTIN_COAX_SPDIF_L 0x0a	
#define EXTIN_COAX_SPDIF_R 0x0b 
#define EXTIN_LINE2_L	   0x0c	
#define EXTIN_LINE2_R	   0x0d	

#define EXTOUT_AC97_L	   0x00	
#define EXTOUT_AC97_R	   0x01	
#define EXTOUT_TOSLINK_L   0x02	
#define EXTOUT_TOSLINK_R   0x03	
#define EXTOUT_AC97_CENTER 0x04	
#define EXTOUT_AC97_LFE	   0x05 
#define EXTOUT_HEADPHONE_L 0x06	
#define EXTOUT_HEADPHONE_R 0x07	
#define EXTOUT_REAR_L	   0x08	
#define EXTOUT_REAR_R	   0x09	
#define EXTOUT_ADC_CAP_L   0x0a	
#define EXTOUT_ADC_CAP_R   0x0b	
#define EXTOUT_MIC_CAP	   0x0c	
#define EXTOUT_AC97_REAR_L 0x0d	
#define EXTOUT_AC97_REAR_R 0x0e	
#define EXTOUT_ACENTER	   0x11 
#define EXTOUT_ALFE	   0x12 

#define A_EXTIN_AC97_L		0x00	
#define A_EXTIN_AC97_R		0x01	
#define A_EXTIN_SPDIF_CD_L	0x02	
#define A_EXTIN_SPDIF_CD_R	0x03	
#define A_EXTIN_OPT_SPDIF_L     0x04    
#define A_EXTIN_OPT_SPDIF_R     0x05     
#define A_EXTIN_LINE2_L		0x08	
#define A_EXTIN_LINE2_R		0x09	
#define A_EXTIN_ADC_L		0x0a    
#define A_EXTIN_ADC_R		0x0b    
#define A_EXTIN_AUX2_L		0x0c	
#define A_EXTIN_AUX2_R		0x0d	

#define A_EXTOUT_FRONT_L	0x00	
#define A_EXTOUT_FRONT_R	0x01	
#define A_EXTOUT_CENTER		0x02	
#define A_EXTOUT_LFE		0x03	
#define A_EXTOUT_HEADPHONE_L	0x04	
#define A_EXTOUT_HEADPHONE_R	0x05	
#define A_EXTOUT_REAR_L		0x06	
#define A_EXTOUT_REAR_R		0x07	
#define A_EXTOUT_AFRONT_L	0x08	
#define A_EXTOUT_AFRONT_R	0x09	
#define A_EXTOUT_ACENTER	0x0a	
#define A_EXTOUT_ALFE		0x0b	
#define A_EXTOUT_ASIDE_L	0x0c	
#define A_EXTOUT_ASIDE_R	0x0d	
#define A_EXTOUT_AREAR_L	0x0e	
#define A_EXTOUT_AREAR_R	0x0f	
#define A_EXTOUT_AC97_L		0x10	
#define A_EXTOUT_AC97_R		0x11	
#define A_EXTOUT_ADC_CAP_L	0x16	
#define A_EXTOUT_ADC_CAP_R	0x17	
#define A_EXTOUT_MIC_CAP	0x18	

#define A_C_00000000	0xc0
#define A_C_00000001	0xc1
#define A_C_00000002	0xc2
#define A_C_00000003	0xc3
#define A_C_00000004	0xc4
#define A_C_00000008	0xc5
#define A_C_00000010	0xc6
#define A_C_00000020	0xc7
#define A_C_00000100	0xc8
#define A_C_00010000	0xc9
#define A_C_00000800	0xca
#define A_C_10000000	0xcb
#define A_C_20000000	0xcc
#define A_C_40000000	0xcd
#define A_C_80000000	0xce
#define A_C_7fffffff	0xcf
#define A_C_ffffffff	0xd0
#define A_C_fffffffe	0xd1
#define A_C_c0000000	0xd2
#define A_C_4f1bbcdc	0xd3
#define A_C_5a7ef9db	0xd4
#define A_C_00100000	0xd5
#define A_GPR_ACCU	0xd6		
#define A_GPR_COND	0xd7		
#define A_GPR_NOISE0	0xd8		
#define A_GPR_NOISE1	0xd9		
#define A_GPR_IRQ	0xda		
#define A_GPR_DBAC	0xdb		
#define A_GPR_DBACE	0xde		

#define EMU10K1_DBG_ZC			0x80000000	
#define EMU10K1_DBG_SATURATION_OCCURED	0x02000000	
#define EMU10K1_DBG_SATURATION_ADDR	0x01ff0000	
#define EMU10K1_DBG_SINGLE_STEP		0x00008000	
#define EMU10K1_DBG_STEP		0x00004000	
#define EMU10K1_DBG_CONDITION_CODE	0x00003e00	
#define EMU10K1_DBG_SINGLE_STEP_ADDR	0x000001ff	

#ifndef __KERNEL__
#define TANKMEMADDRREG_ADDR_MASK 0x000fffff	
#define TANKMEMADDRREG_CLEAR	 0x00800000	
#define TANKMEMADDRREG_ALIGN	 0x00400000	
#define TANKMEMADDRREG_WRITE	 0x00200000	
#define TANKMEMADDRREG_READ	 0x00100000	
#endif

struct snd_emu10k1_fx8010_info {
	unsigned int internal_tram_size;	
	unsigned int external_tram_size;	
	char fxbus_names[16][32];		
	char extin_names[16][32];		
	char extout_names[32][32];		
	unsigned int gpr_controls;		
};

#define EMU10K1_GPR_TRANSLATION_NONE		0
#define EMU10K1_GPR_TRANSLATION_TABLE100	1
#define EMU10K1_GPR_TRANSLATION_BASS		2
#define EMU10K1_GPR_TRANSLATION_TREBLE		3
#define EMU10K1_GPR_TRANSLATION_ONOFF		4

struct snd_emu10k1_fx8010_control_gpr {
	struct snd_ctl_elem_id id;		
	unsigned int vcount;		
	unsigned int count;		
	unsigned short gpr[32];		
	unsigned int value[32];		
	unsigned int min;		
	unsigned int max;		
	unsigned int translation;	
	const unsigned int *tlv;
};

struct snd_emu10k1_fx8010_control_old_gpr {
	struct snd_ctl_elem_id id;
	unsigned int vcount;
	unsigned int count;
	unsigned short gpr[32];
	unsigned int value[32];
	unsigned int min;
	unsigned int max;
	unsigned int translation;
};

struct snd_emu10k1_fx8010_code {
	char name[128];

	DECLARE_BITMAP(gpr_valid, 0x200); 
	__u32 __user *gpr_map;		

	unsigned int gpr_add_control_count; 
	struct snd_emu10k1_fx8010_control_gpr __user *gpr_add_controls; 

	unsigned int gpr_del_control_count; 
	struct snd_ctl_elem_id __user *gpr_del_controls; 

	unsigned int gpr_list_control_count; 
	unsigned int gpr_list_control_total; 
	struct snd_emu10k1_fx8010_control_gpr __user *gpr_list_controls; 

	DECLARE_BITMAP(tram_valid, 0x100); 
	__u32 __user *tram_data_map;	  
	__u32 __user *tram_addr_map;	  

	DECLARE_BITMAP(code_valid, 1024); 
	__u32 __user *code;		  
};

struct snd_emu10k1_fx8010_tram {
	unsigned int address;		
	unsigned int size;		
	unsigned int *samples;		
					
};

struct snd_emu10k1_fx8010_pcm_rec {
	unsigned int substream;		
	unsigned int res1;		
	unsigned int channels;		
	unsigned int tram_start;	
	unsigned int buffer_size;	
	unsigned short gpr_size;		
	unsigned short gpr_ptr;		
	unsigned short gpr_count;	
	unsigned short gpr_tmpcount;	
	unsigned short gpr_trigger;	
	unsigned short gpr_running;	
	unsigned char pad;		
	unsigned char etram[32];	
	unsigned int res2;		
};

#define SNDRV_EMU10K1_VERSION		SNDRV_PROTOCOL_VERSION(1, 0, 1)

#define SNDRV_EMU10K1_IOCTL_INFO	_IOR ('H', 0x10, struct snd_emu10k1_fx8010_info)
#define SNDRV_EMU10K1_IOCTL_CODE_POKE	_IOW ('H', 0x11, struct snd_emu10k1_fx8010_code)
#define SNDRV_EMU10K1_IOCTL_CODE_PEEK	_IOWR('H', 0x12, struct snd_emu10k1_fx8010_code)
#define SNDRV_EMU10K1_IOCTL_TRAM_SETUP	_IOW ('H', 0x20, int)
#define SNDRV_EMU10K1_IOCTL_TRAM_POKE	_IOW ('H', 0x21, struct snd_emu10k1_fx8010_tram)
#define SNDRV_EMU10K1_IOCTL_TRAM_PEEK	_IOWR('H', 0x22, struct snd_emu10k1_fx8010_tram)
#define SNDRV_EMU10K1_IOCTL_PCM_POKE	_IOW ('H', 0x30, struct snd_emu10k1_fx8010_pcm_rec)
#define SNDRV_EMU10K1_IOCTL_PCM_PEEK	_IOWR('H', 0x31, struct snd_emu10k1_fx8010_pcm_rec)
#define SNDRV_EMU10K1_IOCTL_PVERSION	_IOR ('H', 0x40, int)
#define SNDRV_EMU10K1_IOCTL_STOP	_IO  ('H', 0x80)
#define SNDRV_EMU10K1_IOCTL_CONTINUE	_IO  ('H', 0x81)
#define SNDRV_EMU10K1_IOCTL_ZERO_TRAM_COUNTER _IO ('H', 0x82)
#define SNDRV_EMU10K1_IOCTL_SINGLE_STEP	_IOW ('H', 0x83, int)
#define SNDRV_EMU10K1_IOCTL_DBG_READ	_IOR ('H', 0x84, int)

typedef struct snd_emu10k1_fx8010_info emu10k1_fx8010_info_t;
typedef struct snd_emu10k1_fx8010_control_gpr emu10k1_fx8010_control_gpr_t;
typedef struct snd_emu10k1_fx8010_code emu10k1_fx8010_code_t;
typedef struct snd_emu10k1_fx8010_tram emu10k1_fx8010_tram_t;
typedef struct snd_emu10k1_fx8010_pcm_rec emu10k1_fx8010_pcm_t;

#endif	
