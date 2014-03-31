/*
 * Driver for Digigram pcxhr soundcards
 *
 * main header file
 *
 * Copyright (c) 2004 by Digigram <alsa@digigram.com>
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
 */

#ifndef __SOUND_PCXHR_H
#define __SOUND_PCXHR_H

#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <sound/pcm.h>

#define PCXHR_DRIVER_VERSION		0x000906	
#define PCXHR_DRIVER_VERSION_STRING	"0.9.6"		


#define PCXHR_MAX_CARDS		6
#define PCXHR_PLAYBACK_STREAMS	4

#define PCXHR_GRANULARITY	96	
#define PCXHR_GRANULARITY_MIN	96
#define PCXHR_GRANULARITY_HR22	192	

struct snd_pcxhr;
struct pcxhr_mgr;

struct pcxhr_stream;
struct pcxhr_pipe;

enum pcxhr_clock_type {
	PCXHR_CLOCK_TYPE_INTERNAL = 0,
	PCXHR_CLOCK_TYPE_WORD_CLOCK,
	PCXHR_CLOCK_TYPE_AES_SYNC,
	PCXHR_CLOCK_TYPE_AES_1,
	PCXHR_CLOCK_TYPE_AES_2,
	PCXHR_CLOCK_TYPE_AES_3,
	PCXHR_CLOCK_TYPE_AES_4,
	PCXHR_CLOCK_TYPE_MAX = PCXHR_CLOCK_TYPE_AES_4,
	HR22_CLOCK_TYPE_INTERNAL = PCXHR_CLOCK_TYPE_INTERNAL,
	HR22_CLOCK_TYPE_AES_SYNC,
	HR22_CLOCK_TYPE_AES_1,
	HR22_CLOCK_TYPE_MAX = HR22_CLOCK_TYPE_AES_1,
};

struct pcxhr_mgr {
	unsigned int num_cards;
	struct snd_pcxhr *chip[PCXHR_MAX_CARDS];

	struct pci_dev *pci;

	int irq;

	int granularity;

	
	unsigned long port[3];

	
	char shortname[32];		
	char longname[96];		

	
	struct tasklet_struct msg_taskq;
	struct pcxhr_rmh *prmh;
	
	struct tasklet_struct trigger_taskq;

	spinlock_t lock;		
	spinlock_t msg_lock;		

	struct mutex setup_mutex;	
	struct mutex mixer_mutex;	

	
	unsigned int dsp_loaded;	
	unsigned int dsp_version;	
	int playback_chips;
	int capture_chips;
	int fw_file_set;
	int firmware_num;
	unsigned int is_hr_stereo:1;
	unsigned int board_has_aes1:1;	
	unsigned int board_has_analog:1; 
	unsigned int board_has_mic:1; 
	unsigned int board_aes_in_192k:1;
	unsigned int mono_capture:1; 

	struct snd_dma_buffer hostport;

	enum pcxhr_clock_type use_clock_type;	
	enum pcxhr_clock_type cur_clock_type;	
	int sample_rate;
	int ref_count_rate;
	int timer_toggle;		
	int dsp_time_last;		
	int dsp_time_err;		
	unsigned int src_it_dsp;	
	unsigned int io_num_reg_cont;	
	unsigned int codec_speed;	
	unsigned int sample_rate_real;	
	int last_reg_stat;
	int async_err_stream_xrun;
	int async_err_pipe_xrun;
	int async_err_other_last;

	unsigned char xlx_cfg;		
	unsigned char xlx_selmic;	
	unsigned char dsp_reset;	
};


enum pcxhr_stream_status {
	PCXHR_STREAM_STATUS_FREE,
	PCXHR_STREAM_STATUS_OPEN,
	PCXHR_STREAM_STATUS_SCHEDULE_RUN,
	PCXHR_STREAM_STATUS_STARTED,
	PCXHR_STREAM_STATUS_RUNNING,
	PCXHR_STREAM_STATUS_SCHEDULE_STOP,
	PCXHR_STREAM_STATUS_STOPPED,
	PCXHR_STREAM_STATUS_PAUSED
};

struct pcxhr_stream {
	struct snd_pcm_substream *substream;
	snd_pcm_format_t format;
	struct pcxhr_pipe *pipe;

	enum pcxhr_stream_status status;	

	u_int64_t timer_abs_periods;	
	u_int32_t timer_period_frag;	
	u_int32_t timer_buf_periods;	
	int timer_is_synced;		

	int channels;
};


enum pcxhr_pipe_status {
	PCXHR_PIPE_UNDEFINED,
	PCXHR_PIPE_DEFINED
};

struct pcxhr_pipe {
	enum pcxhr_pipe_status status;
	int is_capture;		
	int first_audio;	
};


struct snd_pcxhr {
	struct snd_card *card;
	struct pcxhr_mgr *mgr;
	int chip_idx;		

	struct snd_pcm *pcm;		

	struct pcxhr_pipe playback_pipe;	
	struct pcxhr_pipe capture_pipe[2];	

	struct pcxhr_stream playback_stream[PCXHR_PLAYBACK_STREAMS];
	struct pcxhr_stream capture_stream[2];	
	int nb_streams_play;
	int nb_streams_capt;

	int analog_playback_active[2];	
	int analog_playback_volume[2];	
	int analog_capture_volume[2];	
	int digital_playback_active[PCXHR_PLAYBACK_STREAMS][2];
	int digital_playback_volume[PCXHR_PLAYBACK_STREAMS][2];
	int digital_capture_volume[2];	
	int monitoring_active[2];	
	int monitoring_volume[2];	
	int audio_capture_source;	
	int mic_volume;			
	int mic_boost;			
	int mic_active;			
	int analog_capture_active;	
	int phantom_power;		

	unsigned char aes_bits[5];	
};

struct pcxhr_hostport
{
	char purgebuffer[6];
	char reserved[2];
};

int pcxhr_create_pcm(struct snd_pcxhr *chip);
int pcxhr_set_clock(struct pcxhr_mgr *mgr, unsigned int rate);
int pcxhr_get_external_clock(struct pcxhr_mgr *mgr,
			     enum pcxhr_clock_type clock_type,
			     int *sample_rate);

#endif 
