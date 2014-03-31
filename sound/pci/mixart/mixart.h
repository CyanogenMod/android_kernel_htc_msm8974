/*
 * Driver for Digigram miXart soundcards
 *
 * main header file
 *
 * Copyright (c) 2003 by Digigram <alsa@digigram.com>
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

#ifndef __SOUND_MIXART_H
#define __SOUND_MIXART_H

#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <sound/pcm.h>

#define MIXART_DRIVER_VERSION	0x000100	



struct mixart_uid {
	u32 object_id;
	u32 desc;
};

struct mem_area {
	unsigned long phys;
	void __iomem *virt;
	struct resource *res;
};


struct mixart_route {
	unsigned char connected;
	unsigned char phase_inv;
	int volume;
};


#define MIXART_MOTHERBOARD_XLX_INDEX  0
#define MIXART_MOTHERBOARD_ELF_INDEX  1
#define MIXART_AESEBUBOARD_XLX_INDEX  2
#define MIXART_HARDW_FILES_MAX_INDEX  3  

#define MIXART_MAX_CARDS	4
#define MSG_FIFO_SIZE           16

#define MIXART_MAX_PHYS_CONNECTORS  (MIXART_MAX_CARDS * 2 * 2) 

struct mixart_mgr {
	unsigned int num_cards;
	struct snd_mixart *chip[MIXART_MAX_CARDS];

	struct pci_dev *pci;

	int irq;

	
	struct mem_area mem[2];

	
	char shortname[32];         
	char longname[80];          

	
	struct tasklet_struct msg_taskq;

	
	u32 pending_event;
	wait_queue_head_t msg_sleep;

	
	u32 msg_fifo[MSG_FIFO_SIZE];
	int msg_fifo_readptr;
	int msg_fifo_writeptr;
	atomic_t msg_processed;       

	spinlock_t lock;              
	spinlock_t msg_lock;          
	struct mutex msg_mutex;   

	struct mutex setup_mutex; 

	
	unsigned int dsp_loaded;      
	unsigned int board_type;      

	struct snd_dma_buffer flowinfo;
	struct snd_dma_buffer bufferinfo;

	struct mixart_uid         uid_console_manager;
	int sample_rate;
	int ref_count_rate;

	struct mutex mixer_mutex; 

};


#define MIXART_STREAM_STATUS_FREE	0
#define MIXART_STREAM_STATUS_OPEN	1
#define MIXART_STREAM_STATUS_RUNNING	2
#define MIXART_STREAM_STATUS_DRAINING	3
#define MIXART_STREAM_STATUS_PAUSE	4

#define MIXART_PLAYBACK_STREAMS		4
#define MIXART_CAPTURE_STREAMS		1

#define MIXART_PCM_ANALOG		0
#define MIXART_PCM_DIGITAL		1
#define MIXART_PCM_TOTAL		2

#define MIXART_MAX_STREAM_PER_CARD  (MIXART_PCM_TOTAL * (MIXART_PLAYBACK_STREAMS + MIXART_CAPTURE_STREAMS) )


#define MIXART_NOTIFY_CARD_MASK		0xF000
#define MIXART_NOTIFY_CARD_OFFSET	12
#define MIXART_NOTIFY_PCM_MASK		0x0F00
#define MIXART_NOTIFY_PCM_OFFSET	8
#define MIXART_NOTIFY_CAPT_MASK		0x0080
#define MIXART_NOTIFY_SUBS_MASK		0x007F


struct mixart_stream {
	struct snd_pcm_substream *substream;
	struct mixart_pipe *pipe;
	int pcm_number;

	int status;      

	u64  abs_period_elapsed;  
	u32  buf_periods;         
	u32  buf_period_frag;     

	int channels;
};


enum mixart_pipe_status {
	PIPE_UNDEFINED,
	PIPE_STOPPED,
	PIPE_RUNNING,
	PIPE_CLOCK_SET
};

struct mixart_pipe {
	struct mixart_uid group_uid;			
	int          stream_count;
	struct mixart_uid uid_left_connector;	
	struct mixart_uid uid_right_connector;
	enum mixart_pipe_status status;
	int references;             
	int monitoring;             
};


struct snd_mixart {
	struct snd_card *card;
	struct mixart_mgr *mgr;
	int chip_idx;               
	struct snd_hwdep *hwdep;	    

	struct snd_pcm *pcm;             
	struct snd_pcm *pcm_dig;         

	
	struct mixart_pipe pipe_in_ana;
	struct mixart_pipe pipe_out_ana;

	
	struct mixart_pipe pipe_in_dig;
	struct mixart_pipe pipe_out_dig;

	struct mixart_stream playback_stream[MIXART_PCM_TOTAL][MIXART_PLAYBACK_STREAMS]; 
	struct mixart_stream capture_stream[MIXART_PCM_TOTAL];                           

	
	struct mixart_uid uid_out_analog_physio;
	struct mixart_uid uid_in_analog_physio;

	int analog_playback_active[2];		
	int analog_playback_volume[2];		
	int analog_capture_volume[2];		
	int digital_playback_active[2*MIXART_PLAYBACK_STREAMS][2];	
	int digital_playback_volume[2*MIXART_PLAYBACK_STREAMS][2];	
	int digital_capture_volume[2][2];	
	int monitoring_active[2];		
	int monitoring_volume[2];		
};

struct mixart_bufferinfo
{
	u32 buffer_address;
	u32 reserved[5];
	u32 available_length;
	u32 buffer_id;
};

struct mixart_flowinfo
{
	u32 bufferinfo_array_phy_address;
	u32 reserved[11];
	u32 bufferinfo_count;
	u32 capture;
};

int snd_mixart_create_pcm(struct snd_mixart * chip);
struct mixart_pipe *snd_mixart_add_ref_pipe(struct snd_mixart *chip, int pcm_number, int capture, int monitoring);
int snd_mixart_kill_ref_pipe(struct mixart_mgr *mgr, struct mixart_pipe *pipe, int monitoring);

#endif 
