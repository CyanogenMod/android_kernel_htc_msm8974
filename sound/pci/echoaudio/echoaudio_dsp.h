/****************************************************************************

   Copyright Echo Digital Audio Corporation (c) 1998 - 2004
   All rights reserved
   www.echoaudio.com

   This file is part of Echo Digital Audio's generic driver library.

   Echo Digital Audio's generic driver library is free software;
   you can redistribute it and/or modify it under the terms of
   the GNU General Public License as published by the Free Software
   Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA  02111-1307, USA.

   *************************************************************************

 Translation from C++ and adaptation for use in ALSA-Driver
 were made by Giuliano Pochini <pochini@shiny.it>

****************************************************************************/

#ifndef _ECHO_DSP_
#define _ECHO_DSP_


#if defined(ECHOGALS_FAMILY)

#define NUM_ASIC_TESTS		5
#define READ_DSP_TIMEOUT	1000000L	

#elif defined(ECHO24_FAMILY)

#define DSP_56361			
#define READ_DSP_TIMEOUT	100000L		

#elif defined(ECHO3G_FAMILY)

#define DSP_56361
#define READ_DSP_TIMEOUT 	100000L		
#define MIN_MTC_1X_RATE		32000

#elif defined(INDIGO_FAMILY)

#define DSP_56361
#define READ_DSP_TIMEOUT	100000L		

#else

#error No family is defined

#endif




#define DSP_MAXAUDIOINPUTS		16	
#define DSP_MAXAUDIOOUTPUTS		16	
#define DSP_MAXPIPES			32	



#define CHI32_CONTROL_REG		4
#define CHI32_STATUS_REG		5
#define CHI32_VECTOR_REG		6
#define CHI32_DATA_REG			7



#define CHI32_VECTOR_BUSY		0x00000001
#define CHI32_STATUS_REG_HF3		0x00000008
#define CHI32_STATUS_REG_HF4		0x00000010
#define CHI32_STATUS_REG_HF5		0x00000020
#define CHI32_STATUS_HOST_READ_FULL	0x00000004
#define CHI32_STATUS_HOST_WRITE_EMPTY	0x00000002
#define CHI32_STATUS_IRQ		0x00000040



#define DSP_FNC_SET_COMMPAGE_ADDR		0x02
#define DSP_FNC_LOAD_LAYLA_ASIC			0xa0
#define DSP_FNC_LOAD_GINA24_ASIC		0xa0
#define DSP_FNC_LOAD_MONA_PCI_CARD_ASIC		0xa0
#define DSP_FNC_LOAD_LAYLA24_PCI_CARD_ASIC	0xa0
#define DSP_FNC_LOAD_MONA_EXTERNAL_ASIC		0xa1
#define DSP_FNC_LOAD_LAYLA24_EXTERNAL_ASIC	0xa1
#define DSP_FNC_LOAD_3G_ASIC			0xa0



#define MIDI_IN_STATE_NORMAL	0
#define MIDI_IN_STATE_TS_HIGH	1
#define MIDI_IN_STATE_TS_LOW	2
#define MIDI_IN_STATE_F1_DATA 	3
#define MIDI_IN_SKIP_DATA	(-1)



#define LAYLA24_MAGIC_NUMBER			677376000
#define LAYLA24_CONTINUOUS_CLOCK		0x000e



#define DSP_VC_RESET				0x80ff

#ifndef DSP_56361

#define DSP_VC_ACK_INT				0x8073
#define DSP_VC_SET_VMIXER_GAIN			0x0000	
#define DSP_VC_START_TRANSFER			0x0075	
#define DSP_VC_METERS_ON			0x0079
#define DSP_VC_METERS_OFF			0x007b
#define DSP_VC_UPDATE_OUTVOL			0x007d	
#define DSP_VC_UPDATE_INGAIN			0x007f	
#define DSP_VC_ADD_AUDIO_BUFFER			0x0081	
#define DSP_VC_TEST_ASIC			0x00eb
#define DSP_VC_UPDATE_CLOCKS			0x00ef	
#define DSP_VC_SET_LAYLA_SAMPLE_RATE		0x00f1	
#define DSP_VC_SET_GD_AUDIO_STATE		0x00f1	
#define DSP_VC_WRITE_CONTROL_REG		0x00f1	
#define DSP_VC_MIDI_WRITE			0x00f5	
#define DSP_VC_STOP_TRANSFER			0x00f7	
#define DSP_VC_UPDATE_FLAGS			0x00fd	
#define DSP_VC_GO_COMATOSE			0x00f9

#else 

#define DSP_VC_ACK_INT				0x80F5
#define DSP_VC_SET_VMIXER_GAIN			0x00DB	
#define DSP_VC_START_TRANSFER			0x00DD	
#define DSP_VC_METERS_ON			0x00EF
#define DSP_VC_METERS_OFF			0x00F1
#define DSP_VC_UPDATE_OUTVOL			0x00E3	
#define DSP_VC_UPDATE_INGAIN			0x00E5	
#define DSP_VC_ADD_AUDIO_BUFFER			0x00E1	
#define DSP_VC_TEST_ASIC			0x00ED
#define DSP_VC_UPDATE_CLOCKS			0x00E9	
#define DSP_VC_SET_LAYLA24_FREQUENCY_REG	0x00E9	
#define DSP_VC_SET_LAYLA_SAMPLE_RATE		0x00EB	
#define DSP_VC_SET_GD_AUDIO_STATE		0x00EB	
#define DSP_VC_WRITE_CONTROL_REG		0x00EB	
#define DSP_VC_MIDI_WRITE			0x00E7	
#define DSP_VC_STOP_TRANSFER			0x00DF	
#define DSP_VC_UPDATE_FLAGS			0x00FB	
#define DSP_VC_GO_COMATOSE			0x00d9

#endif 



#define HANDSHAKE_TIMEOUT		20000	
#define VECTOR_BUSY_TIMEOUT		100000	
#define MIDI_OUT_DELAY_USEC		2000	



#define DSP_FLAG_MIDI_INPUT		0x0001	
#define DSP_FLAG_SPDIF_NONAUDIO		0x0002	
#define DSP_FLAG_PROFESSIONAL_SPDIF	0x0008	



#define GLDM_CLOCK_DETECT_BIT_WORD	0x0002
#define GLDM_CLOCK_DETECT_BIT_SUPER	0x0004
#define GLDM_CLOCK_DETECT_BIT_SPDIF	0x0008
#define GLDM_CLOCK_DETECT_BIT_ESYNC	0x0010



#define GML_CLOCK_DETECT_BIT_WORD96	0x0002
#define GML_CLOCK_DETECT_BIT_WORD48	0x0004
#define GML_CLOCK_DETECT_BIT_SPDIF48	0x0008
#define GML_CLOCK_DETECT_BIT_SPDIF96	0x0010
#define GML_CLOCK_DETECT_BIT_WORD	(GML_CLOCK_DETECT_BIT_WORD96 | GML_CLOCK_DETECT_BIT_WORD48)
#define GML_CLOCK_DETECT_BIT_SPDIF	(GML_CLOCK_DETECT_BIT_SPDIF48 | GML_CLOCK_DETECT_BIT_SPDIF96)
#define GML_CLOCK_DETECT_BIT_ESYNC	0x0020
#define GML_CLOCK_DETECT_BIT_ADAT	0x0040



#define LAYLA20_CLOCK_INTERNAL		0
#define LAYLA20_CLOCK_SPDIF		1
#define LAYLA20_CLOCK_WORD		2
#define LAYLA20_CLOCK_SUPER		3



#define GD_CLOCK_NOCHANGE		0
#define GD_CLOCK_44			1
#define GD_CLOCK_48			2
#define GD_CLOCK_SPDIFIN		3
#define GD_CLOCK_UNDEF			0xff



#define GD_SPDIF_STATUS_NOCHANGE	0
#define GD_SPDIF_STATUS_44		1
#define GD_SPDIF_STATUS_48		2
#define GD_SPDIF_STATUS_UNDEF		0xff



#define LAYLA20_OUTPUT_CLOCK_SUPER	0
#define LAYLA20_OUTPUT_CLOCK_WORD	1



#define GD24_96000	0x0
#define GD24_48000	0x1
#define GD24_44100	0x2
#define GD24_32000	0x3
#define GD24_22050	0x4
#define GD24_16000	0x5
#define GD24_11025	0x6
#define GD24_8000	0x7
#define GD24_88200	0x8
#define GD24_EXT_SYNC	0x9



#define ASIC_ALREADY_LOADED	0x1
#define ASIC_NOT_LOADED		0x0



#define DSP_AUDIOFORM_MS_8	0	
#define DSP_AUDIOFORM_MS_16LE	1	
#define DSP_AUDIOFORM_MS_24LE	2	
#define DSP_AUDIOFORM_MS_32LE	3	
#define DSP_AUDIOFORM_SS_8	4	
#define DSP_AUDIOFORM_SS_16LE	5	
#define DSP_AUDIOFORM_SS_24LE	6	
#define DSP_AUDIOFORM_SS_32LE	7	
#define DSP_AUDIOFORM_MM_32LE	8	
#define DSP_AUDIOFORM_MM_32BE	9	
#define DSP_AUDIOFORM_SS_32BE	10	
#define DSP_AUDIOFORM_INVALID	0xFF	



#define DSP_AUDIOFORM_SUPER_INTERLEAVE_16LE	0x40
#define DSP_AUDIOFORM_SUPER_INTERLEAVE_24LE	0xc0
#define DSP_AUDIOFORM_SUPER_INTERLEAVE_32LE	0x80



#define GML_CONVERTER_ENABLE	0x0010
#define GML_SPDIF_PRO_MODE	0x0020	
#define GML_SPDIF_SAMPLE_RATE0	0x0040
#define GML_SPDIF_SAMPLE_RATE1	0x0080
#define GML_SPDIF_TWO_CHANNEL	0x0100	
#define GML_SPDIF_NOT_AUDIO	0x0200
#define GML_SPDIF_COPY_PERMIT	0x0400
#define GML_SPDIF_24_BIT	0x0800	
#define GML_ADAT_MODE		0x1000	
#define GML_SPDIF_OPTICAL_MODE	0x2000	
#define GML_SPDIF_CDROM_MODE	0x3000	
#define GML_DOUBLE_SPEED_MODE	0x4000	

#define GML_DIGITAL_IN_AUTO_MUTE 0x800000

#define GML_96KHZ		(0x0 | GML_DOUBLE_SPEED_MODE)
#define GML_88KHZ		(0x1 | GML_DOUBLE_SPEED_MODE)
#define GML_48KHZ		0x2
#define GML_44KHZ		0x3
#define GML_32KHZ		0x4
#define GML_22KHZ		0x5
#define GML_16KHZ		0x6
#define GML_11KHZ		0x7
#define GML_8KHZ		0x8
#define GML_SPDIF_CLOCK		0x9
#define GML_ADAT_CLOCK		0xA
#define GML_WORD_CLOCK		0xB
#define GML_ESYNC_CLOCK		0xC
#define GML_ESYNCx2_CLOCK	0xD

#define GML_CLOCK_CLEAR_MASK		0xffffbff0
#define GML_SPDIF_RATE_CLEAR_MASK	(~(GML_SPDIF_SAMPLE_RATE0|GML_SPDIF_SAMPLE_RATE1))
#define GML_DIGITAL_MODE_CLEAR_MASK	0xffffcfff
#define GML_SPDIF_FORMAT_CLEAR_MASK	0xfffff01f



#define MIA_32000	0x0040
#define MIA_44100	0x0042
#define MIA_48000	0x0041
#define MIA_88200	0x0142
#define MIA_96000	0x0141

#define MIA_SPDIF	0x00000044
#define MIA_SPDIF96	0x00000144

#define MIA_MIDI_REV	1	



#define E3G_CONVERTER_ENABLE	0x0010
#define E3G_SPDIF_PRO_MODE	0x0020	
#define E3G_SPDIF_SAMPLE_RATE0	0x0040
#define E3G_SPDIF_SAMPLE_RATE1	0x0080
#define E3G_SPDIF_TWO_CHANNEL	0x0100	
#define E3G_SPDIF_NOT_AUDIO	0x0200
#define E3G_SPDIF_COPY_PERMIT	0x0400
#define E3G_SPDIF_24_BIT	0x0800	
#define E3G_DOUBLE_SPEED_MODE	0x4000	
#define E3G_PHANTOM_POWER	0x8000	

#define E3G_96KHZ		(0x0 | E3G_DOUBLE_SPEED_MODE)
#define E3G_88KHZ		(0x1 | E3G_DOUBLE_SPEED_MODE)
#define E3G_48KHZ		0x2
#define E3G_44KHZ		0x3
#define E3G_32KHZ		0x4
#define E3G_22KHZ		0x5
#define E3G_16KHZ		0x6
#define E3G_11KHZ		0x7
#define E3G_8KHZ		0x8
#define E3G_SPDIF_CLOCK		0x9
#define E3G_ADAT_CLOCK		0xA
#define E3G_WORD_CLOCK		0xB
#define E3G_CONTINUOUS_CLOCK	0xE

#define E3G_ADAT_MODE		0x1000
#define E3G_SPDIF_OPTICAL_MODE	0x2000

#define E3G_CLOCK_CLEAR_MASK		0xbfffbff0
#define E3G_DIGITAL_MODE_CLEAR_MASK	0xffffcfff
#define E3G_SPDIF_FORMAT_CLEAR_MASK	0xfffff01f

#define E3G_CLOCK_DETECT_BIT_WORD96	0x0001
#define E3G_CLOCK_DETECT_BIT_WORD48	0x0002
#define E3G_CLOCK_DETECT_BIT_SPDIF48	0x0004
#define E3G_CLOCK_DETECT_BIT_ADAT	0x0004
#define E3G_CLOCK_DETECT_BIT_SPDIF96	0x0008
#define E3G_CLOCK_DETECT_BIT_WORD	(E3G_CLOCK_DETECT_BIT_WORD96|E3G_CLOCK_DETECT_BIT_WORD48)
#define E3G_CLOCK_DETECT_BIT_SPDIF	(E3G_CLOCK_DETECT_BIT_SPDIF48|E3G_CLOCK_DETECT_BIT_SPDIF96)

#define E3G_MAGIC_NUMBER		677376000
#define E3G_FREQ_REG_DEFAULT		(E3G_MAGIC_NUMBER / 48000 - 2)
#define E3G_FREQ_REG_MAX		0xffff

#define E3G_GINA3G_BOX_TYPE		0x00
#define E3G_LAYLA3G_BOX_TYPE		0x10
#define E3G_ASIC_NOT_LOADED		0xffff
#define E3G_BOX_TYPE_MASK		0xf0

#define INDIGO_EXPRESS_32000		0x02
#define INDIGO_EXPRESS_44100		0x01
#define INDIGO_EXPRESS_48000		0x00
#define INDIGO_EXPRESS_DOUBLE_SPEED	0x10
#define INDIGO_EXPRESS_QUAD_SPEED	0x04
#define INDIGO_EXPRESS_CLOCK_MASK	0x17



#define GL20_INPUT_GAIN_MAGIC_NUMBER	0xC8



#define DSP_LOAD_ATTEMPT_PERIOD		1000000L	



#define MONITOR_ARRAY_SIZE	0x180
#define VMIXER_ARRAY_SIZE	0x40
#define MIDI_OUT_BUFFER_SIZE	32
#define MIDI_IN_BUFFER_SIZE	256
#define MAX_PLAY_TAPS		168
#define MAX_REC_TAPS		192
#define DSP_MIDI_OUT_FIFO_SIZE	64



#define MAX_SGLIST_ENTRIES 512

struct sg_entry {
	u32 addr;
	u32 size;
};


/****************************************************************************

  The comm page.  This structure is read and written by the DSP; the
  DSP code is a firm believer in the byte offsets written in the comments
  at the end of each line.  This structure should not be changed.

  Any reads from or writes to this structure should be in little-endian format.

 ****************************************************************************/

struct comm_page {		
	u32 comm_size;		
	u32 flags;		
	u32 unused;		
	u32 sample_rate;	
	u32 handshake;		
	u32 cmd_start;		
	u32 cmd_stop;		
	u32 cmd_reset;		
	u16 audio_format[DSP_MAXPIPES];	
	struct sg_entry sglist_addr[DSP_MAXPIPES];
				
	u32 position[DSP_MAXPIPES];
				
	s8 vu_meter[DSP_MAXPIPES];
				
	s8 peak_meter[DSP_MAXPIPES];
				
	s8 line_out_level[DSP_MAXAUDIOOUTPUTS];
				
	s8 line_in_level[DSP_MAXAUDIOINPUTS];
				
	s8 monitors[MONITOR_ARRAY_SIZE];
				
	u32 play_coeff[MAX_PLAY_TAPS];
			
	u32 rec_coeff[MAX_REC_TAPS];
			
	u16 midi_input[MIDI_IN_BUFFER_SIZE];
			
	u8 gd_clock_state;	
	u8 gd_spdif_status;	
	u8 gd_resampler_state;	
	u8 filler2;		
	u32 nominal_level_mask;	
	u16 input_clock;	
	u16 output_clock;	
	u32 status_clocks;	
	u32 ext_box_status;	
	u32 cmd_add_buffer;	
	u32 midi_out_free_count;
			
	u32 unused2;		
	u32 control_register;
			
	u32 e3g_frq_register;	
	u8 filler[24];		
	s8 vmixer[VMIXER_ARRAY_SIZE];
				
	u8 midi_output[MIDI_OUT_BUFFER_SIZE];
				
};

#endif 
