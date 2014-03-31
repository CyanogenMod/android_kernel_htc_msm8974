/*
 *  Copyright (c) 2004 James Courtier-Dutton <James@superbug.demon.co.uk>
 *  Driver CA0106 chips. e.g. Sound Blaster Audigy LS and Live 24bit
 *  Version: 0.0.22
 *
 *  FEATURES currently supported:
 *    See ca0106_main.c for features.
 * 
 *  Changelog:
 *    Support interrupts per period.
 *    Removed noise from Center/LFE channel when in Analog mode.
 *    Rename and remove mixer controls.
 *  0.0.6
 *    Use separate card based DMA buffer for periods table list.
 *  0.0.7
 *    Change remove and rename ctrls into lists.
 *  0.0.8
 *    Try to fix capture sources.
 *  0.0.9
 *    Fix AC3 output.
 *    Enable S32_LE format support.
 *  0.0.10
 *    Enable playback 48000 and 96000 rates. (Rates other that these do not work, even with "plug:front".)
 *  0.0.11
 *    Add Model name recognition.
 *  0.0.12
 *    Correct interrupt timing. interrupt at end of period, instead of in the middle of a playback period.
 *    Remove redundent "voice" handling.
 *  0.0.13
 *    Single trigger call for multi channels.
 *  0.0.14
 *    Set limits based on what the sound card hardware can do.
 *    playback periods_min=2, periods_max=8
 *    capture hw constraints require period_size = n * 64 bytes.
 *    playback hw constraints require period_size = n * 64 bytes.
 *  0.0.15
 *    Separated ca0106.c into separate functional .c files.
 *  0.0.16
 *    Implement 192000 sample rate.
 *  0.0.17
 *    Add support for SB0410 and SB0413.
 *  0.0.18
 *    Modified Copyright message.
 *  0.0.19
 *    Added I2C and SPI registers. Filled in interrupt enable.
 *  0.0.20
 *    Added GPIO info for SB Live 24bit.
 *  0.0.21
 *   Implement support for Line-in capture on SB Live 24bit.
 *  0.0.22
 *    Add support for mute control on SB Live 24bit (cards w/ SPI DAC)
 *
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


#define PTR			0x00		
						
						
						

#define DATA			0x04		
						

#define IPR			0x08		
						
						
#define IPR_MIDI_RX_B		0x00020000	
#define IPR_MIDI_TX_B		0x00010000	
#define IPR_SPDIF_IN_USER	0x00004000      
#define IPR_SPDIF_OUT_USER	0x00002000      
#define IPR_SPDIF_OUT_FRAME	0x00001000      
#define IPR_SPI			0x00000800      
#define IPR_I2C_EEPROM		0x00000400      
#define IPR_I2C_DAC		0x00000200      
#define IPR_AI			0x00000100      
#define IPR_GPI			0x00000080      
#define IPR_SRC_LOCKED          0x00000040      
#define IPR_SPDIF_STATUS        0x00000020      
#define IPR_TIMER2              0x00000010      
#define IPR_TIMER1              0x00000008      
#define IPR_MIDI_RX_A		0x00000004	
#define IPR_MIDI_TX_A		0x00000002	
#define IPR_PCI			0x00000001	

#define INTE			0x0c		

#define INTE_MIDI_RX_B		0x00020000	
#define INTE_MIDI_TX_B		0x00010000	
#define INTE_SPDIF_IN_USER	0x00004000      
#define INTE_SPDIF_OUT_USER	0x00002000      
#define INTE_SPDIF_OUT_FRAME	0x00001000      
#define INTE_SPI		0x00000800      
#define INTE_I2C_EEPROM		0x00000400      
#define INTE_I2C_DAC		0x00000200      
#define INTE_AI			0x00000100      
#define INTE_GPI		0x00000080      
#define INTE_SRC_LOCKED         0x00000040      
#define INTE_SPDIF_STATUS       0x00000020      
#define INTE_TIMER2             0x00000010      
#define INTE_TIMER1             0x00000008      
#define INTE_MIDI_RX_A		0x00000004	
#define INTE_MIDI_TX_A		0x00000002	
#define INTE_PCI		0x00000001	

#define UNKNOWN10		0x10		
#define HCFG			0x14		
						

#define HCFG_STAC		0x10000000	
#define HCFG_CAPTURE_I2S_BYPASS	0x08000000	
#define HCFG_CAPTURE_SPDIF_BYPASS 0x04000000	
#define HCFG_PLAYBACK_I2S_BYPASS 0x02000000	
#define HCFG_FORCE_LOCK		0x01000000	
#define HCFG_PLAYBACK_ATTENUATION 0x00006000	
#define HCFG_PLAYBACK_DITHER	0x00001000	
#define HCFG_PLAYBACK_S32_LE	0x00000800	
#define HCFG_CAPTURE_S32_LE	0x00000400	
#define HCFG_8_CHANNEL_PLAY	0x00000200	
#define HCFG_8_CHANNEL_CAPTURE	0x00000100	
#define HCFG_MONO		0x00000080	
#define HCFG_I2S_OUTPUT		0x00000010	
#define HCFG_AC97		0x00000008	
#define HCFG_LOCK_PLAYBACK_CACHE 0x00000004	
						
#define HCFG_LOCK_CAPTURE_CACHE	0x00000002	
						
#define HCFG_AUDIOENABLE	0x00000001	
						
						
#define GPIO			0x18		
						
						
#define AC97DATA		0x1c		

#define AC97ADDRESS		0x1e		

                                                                                                                           
#define PLAYBACK_LIST_ADDR	0x00		
						
#define PLAYBACK_LIST_SIZE	0x01		
						
#define PLAYBACK_LIST_PTR	0x02		
						
#define PLAYBACK_UNKNOWN3	0x03		
#define PLAYBACK_DMA_ADDR	0x04		
						
#define PLAYBACK_PERIOD_SIZE	0x05		
						
#define PLAYBACK_POINTER	0x06		
						
#define PLAYBACK_PERIOD_END_ADDR 0x07		
						
#define PLAYBACK_FIFO_OFFSET_ADDRESS	0x08	
						
#define PLAYBACK_UNKNOWN9	0x09		
#define CAPTURE_DMA_ADDR	0x10		
						
#define CAPTURE_BUFFER_SIZE	0x11		
						
#define CAPTURE_POINTER		0x12		
						
#define CAPTURE_FIFO_OFFSET_ADDRESS	0x13	
						
#define PLAYBACK_LAST_SAMPLE    0x20		
#define BASIC_INTERRUPT         0x40		
						
						
						
#define SPCS0			0x41		
#define SPCS1			0x42		
#define SPCS2			0x43		
#define SPCS3			0x44		
						
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

						
#define SPCS_WORD_LENGTH_MASK	0x0000000f	
#define SPCS_WORD_LENGTH_16	0x00000008	
#define SPCS_WORD_LENGTH_17	0x00000006	
#define SPCS_WORD_LENGTH_18	0x00000004	
#define SPCS_WORD_LENGTH_19	0x00000002	
#define SPCS_WORD_LENGTH_20A	0x0000000a	
#define SPCS_WORD_LENGTH_20	0x00000009	
#define SPCS_WORD_LENGTH_21	0x00000007	
#define SPCS_WORD_LENGTH_22	0x00000005	
#define SPCS_WORD_LENGTH_23	0x00000003	
#define SPCS_WORD_LENGTH_24	0x0000000b	
#define SPCS_ORIGINAL_SAMPLE_RATE_MASK	0x000000f0 
#define SPCS_ORIGINAL_SAMPLE_RATE_NONE	0x00000000 
#define SPCS_ORIGINAL_SAMPLE_RATE_16000	0x00000010 
#define SPCS_ORIGINAL_SAMPLE_RATE_RES1	0x00000020 
#define SPCS_ORIGINAL_SAMPLE_RATE_32000	0x00000030 
#define SPCS_ORIGINAL_SAMPLE_RATE_12000	0x00000040 
#define SPCS_ORIGINAL_SAMPLE_RATE_11025	0x00000050 
#define SPCS_ORIGINAL_SAMPLE_RATE_8000	0x00000060 
#define SPCS_ORIGINAL_SAMPLE_RATE_RES2	0x00000070 
#define SPCS_ORIGINAL_SAMPLE_RATE_192000 0x00000080 
#define SPCS_ORIGINAL_SAMPLE_RATE_24000	0x00000090 
#define SPCS_ORIGINAL_SAMPLE_RATE_96000	0x000000a0 
#define SPCS_ORIGINAL_SAMPLE_RATE_48000	0x000000b0 
#define SPCS_ORIGINAL_SAMPLE_RATE_176400 0x000000c0 
#define SPCS_ORIGINAL_SAMPLE_RATE_22050	0x000000d0 
#define SPCS_ORIGINAL_SAMPLE_RATE_88200	0x000000e0 
#define SPCS_ORIGINAL_SAMPLE_RATE_44100	0x000000f0 

#define SPDIF_SELECT1		0x45		
#define WATERMARK		0x46		
#define SPDIF_INPUT_STATUS	0x49		
#define CAPTURE_CACHE_DATA	0x50		
#define CAPTURE_SOURCE          0x60            
#define CAPTURE_SOURCE_CHANNEL0 0xf0000000	
#define CAPTURE_SOURCE_CHANNEL1 0x0f000000	
#define CAPTURE_SOURCE_CHANNEL2 0x00f00000      
#define CAPTURE_SOURCE_CHANNEL3 0x000f0000	
#define CAPTURE_SOURCE_RECORD_MAP 0x0000ffff	
#define CAPTURE_VOLUME1         0x61            
#define CAPTURE_VOLUME2         0x62            

#define PLAYBACK_ROUTING1       0x63            
#define ROUTING1_REAR           0x77000000      
#define ROUTING1_NULL           0x00770000      
#define ROUTING1_CENTER_LFE     0x00007700      
#define ROUTING1_FRONT          0x00000077	
						
						

#define PLAYBACK_ROUTING2       0x64            
						

#define PLAYBACK_MUTE           0x65            
#define PLAYBACK_VOLUME1        0x66            
						
						
						
#define CAPTURE_ROUTING1        0x67            
						
#define CAPTURE_ROUTING2        0x68            
						
#define CAPTURE_MUTE            0x69            
						
#define PLAYBACK_VOLUME2        0x6a            
						
#define UNKNOWN6b               0x6b            
#define MIDI_UART_A_DATA		0x6c            
#define MIDI_UART_A_CMD		0x6d            
#define MIDI_UART_B_DATA		0x6e            
#define MIDI_UART_B_CMD		0x6f            


#define CA0106_MIDI_CHAN_A		0x1
#define CA0106_MIDI_CHAN_B		0x2


#define CA0106_MIDI_INPUT_AVAIL 	0x80
#define CA0106_MIDI_OUTPUT_READY	0x40
#define CA0106_MPU401_RESET		0xff
#define CA0106_MPU401_ENTER_UART	0x3f
#define CA0106_MPU401_ACK		0xfe

#define SAMPLE_RATE_TRACKER_STATUS 0x70         
#define CAPTURE_CONTROL         0x71            
						
						
 
#define SPDIF_SELECT2           0x72            
#define ROUTING2_FRONT_MASK     0x00010000      
#define ROUTING2_CENTER_LFE_MASK 0x00020000     
#define ROUTING2_REAR_MASK      0x00080000      
 
#define UNKNOWN73               0x73            
#define CHIP_VERSION            0x74            
#define EXTENDED_INT_MASK       0x75            
						
#define EXTENDED_INT            0x76            
						
						
#define COUNTER77               0x77		
#define COUNTER78               0x78		
#define EXTENDED_INT_TIMER      0x79            
						
#define SPI			0x7a		
#define I2C_A			0x7b		
#define I2C_D0			0x7c		
#define I2C_D1			0x7d		
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
#define ADC_MUX_PHONE		0x00000001	
#define ADC_MUX_MIC		0x00000002	
#define ADC_MUX_LINEIN		0x00000004	
#define ADC_MUX_AUX		0x00000008	

#define SET_CHANNEL 0  
#define PCM_FRONT_CHANNEL 0
#define PCM_REAR_CHANNEL 1
#define PCM_CENTER_LFE_CHANNEL 2
#define PCM_UNKNOWN_CHANNEL 3
#define CONTROL_FRONT_CHANNEL 0
#define CONTROL_REAR_CHANNEL 3
#define CONTROL_CENTER_LFE_CHANNEL 1
#define CONTROL_UNKNOWN_CHANNEL 2


#define SPI_REG_MASK	0x1ff	
#define SPI_REG_SHIFT	9	

#define SPI_LDA1_REG		0	
#define SPI_RDA1_REG		1
#define SPI_LDA2_REG		4
#define SPI_RDA2_REG		5
#define SPI_LDA3_REG		6
#define SPI_RDA3_REG		7
#define SPI_LDA4_REG		13
#define SPI_RDA4_REG		14
#define SPI_MASTDA_REG		8

#define SPI_DA_BIT_UPDATE	(1<<8)	
#define SPI_DA_BIT_0dB		0xff	
#define SPI_DA_BIT_infdB	0x00	

#define SPI_PL_REG		2
#define SPI_PL_BIT_L_M		(0<<5)	
#define SPI_PL_BIT_L_L		(1<<5)	
#define SPI_PL_BIT_L_R		(2<<5)	
#define SPI_PL_BIT_L_C		(3<<5)	
#define SPI_PL_BIT_R_M		(0<<7)	
#define SPI_PL_BIT_R_L		(1<<7)	
#define SPI_PL_BIT_R_R		(2<<7)	
#define SPI_PL_BIT_R_C		(3<<7)	
#define SPI_IZD_REG		2
#define SPI_IZD_BIT		(1<<4)	

#define SPI_FMT_REG		3
#define SPI_FMT_BIT_RJ		(0<<0)	
#define SPI_FMT_BIT_LJ		(1<<0)	
#define SPI_FMT_BIT_I2S		(2<<0)	
#define SPI_FMT_BIT_DSP		(3<<0)	
#define SPI_LRP_REG		3
#define SPI_LRP_BIT		(1<<2)	
#define SPI_BCP_REG		3
#define SPI_BCP_BIT		(1<<3)	
#define SPI_IWL_REG		3
#define SPI_IWL_BIT_16		(0<<4)	
#define SPI_IWL_BIT_20		(1<<4)	
#define SPI_IWL_BIT_24		(2<<4)	
#define SPI_IWL_BIT_32		(3<<4)	

#define SPI_MS_REG		10
#define SPI_MS_BIT		(1<<5)	
#define SPI_RATE_REG		10	
#define SPI_RATE_BIT_128	(0<<6)	
#define SPI_RATE_BIT_192	(1<<6)
#define SPI_RATE_BIT_256	(2<<6)
#define SPI_RATE_BIT_384	(3<<6)
#define SPI_RATE_BIT_512	(4<<6)
#define SPI_RATE_BIT_768	(5<<6)

#define SPI_DMUTE0_REG		9
#define SPI_DMUTE1_REG		9
#define SPI_DMUTE2_REG		9
#define SPI_DMUTE4_REG		15
#define SPI_DMUTE0_BIT		(1<<3)
#define SPI_DMUTE1_BIT		(1<<4)
#define SPI_DMUTE2_BIT		(1<<5)
#define SPI_DMUTE4_BIT		(1<<2)

#define SPI_PHASE0_REG		3
#define SPI_PHASE1_REG		3
#define SPI_PHASE2_REG		3
#define SPI_PHASE4_REG		15
#define SPI_PHASE0_BIT		(1<<6)
#define SPI_PHASE1_BIT		(1<<7)
#define SPI_PHASE2_BIT		(1<<8)
#define SPI_PHASE4_BIT		(1<<3)

#define SPI_PDWN_REG		2	
#define SPI_PDWN_BIT		(1<<2)
#define SPI_DACD0_REG		10	
#define SPI_DACD1_REG		10
#define SPI_DACD2_REG		10
#define SPI_DACD4_REG		15
#define SPI_DACD0_BIT		(1<<1)
#define SPI_DACD1_BIT		(1<<2)
#define SPI_DACD2_BIT		(1<<3)
#define SPI_DACD4_BIT		(1<<0)	

#define SPI_PWRDNALL_REG	10	
#define SPI_PWRDNALL_BIT	(1<<4)

#include "ca_midi.h"

struct snd_ca0106;

struct snd_ca0106_channel {
	struct snd_ca0106 *emu;
	int number;
	int use;
	void (*interrupt)(struct snd_ca0106 *emu, struct snd_ca0106_channel *channel);
	struct snd_ca0106_pcm *epcm;
};

struct snd_ca0106_pcm {
	struct snd_ca0106 *emu;
	struct snd_pcm_substream *substream;
        int channel_id;
	unsigned short running;
};

struct snd_ca0106_details {
        u32 serial;
        char * name;
	int ac97;	
	int gpio_type;	
	int i2c_adc;	
	u16 spi_dac;	
};

struct snd_ca0106 {
	struct snd_card *card;
	struct snd_ca0106_details *details;
	struct pci_dev *pci;

	unsigned long port;
	struct resource *res_port;
	int irq;

	unsigned int serial;            
	unsigned short model;		

	spinlock_t emu_lock;

	struct snd_ac97 *ac97;
	struct snd_pcm *pcm[4];

	struct snd_ca0106_channel playback_channels[4];
	struct snd_ca0106_channel capture_channels[4];
	u32 spdif_bits[4];             
	u32 spdif_str_bits[4];         
	int spdif_enable;
	int capture_source;
	int i2c_capture_source;
	u8 i2c_capture_volume[4][2];
	int capture_mic_line_in;

	struct snd_dma_buffer buffer;

	struct snd_ca_midi midi;
	struct snd_ca_midi midi2;

	u16 spi_dac_reg[16];

#ifdef CONFIG_PM
#define NUM_SAVED_VOLUMES	9
	unsigned int saved_vol[NUM_SAVED_VOLUMES];
#endif
};

int snd_ca0106_mixer(struct snd_ca0106 *emu);
int snd_ca0106_proc_init(struct snd_ca0106 * emu);

unsigned int snd_ca0106_ptr_read(struct snd_ca0106 * emu, 
				 unsigned int reg, 
				 unsigned int chn);

void snd_ca0106_ptr_write(struct snd_ca0106 *emu, 
			  unsigned int reg, 
			  unsigned int chn, 
			  unsigned int data);

int snd_ca0106_i2c_write(struct snd_ca0106 *emu, u32 reg, u32 value);

int snd_ca0106_spi_write(struct snd_ca0106 * emu,
				   unsigned int data);

#ifdef CONFIG_PM
void snd_ca0106_mixer_suspend(struct snd_ca0106 *chip);
void snd_ca0106_mixer_resume(struct snd_ca0106 *chip);
#else
#define snd_ca0106_mixer_suspend(chip)	do { } while (0)
#define snd_ca0106_mixer_resume(chip)	do { } while (0)
#endif
