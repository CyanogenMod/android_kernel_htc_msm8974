/* -*- linux-c -*- *
 *
 * ALSA driver for the digigram lx6464es interface
 *
 * Copyright (c) 2009 Tim Blechmann <tim@klingt.org>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef LX6464ES_H
#define LX6464ES_H

#include <linux/spinlock.h>
#include <linux/atomic.h>

#include <sound/core.h>
#include <sound/pcm.h>

#include "lx_core.h"

#define LXP "LX6464ES: "

enum {
    ES_cmd_free         = 0,    
    ES_cmd_processing   = 1,	
    ES_read_pending     = 2,    
    ES_read_finishing   = 3,    
};

enum lx_stream_status {
	LX_STREAM_STATUS_FREE,
	LX_STREAM_STATUS_SCHEDULE_RUN,
	LX_STREAM_STATUS_RUNNING,
	LX_STREAM_STATUS_SCHEDULE_STOP,
};


struct lx_stream {
	struct snd_pcm_substream  *stream;
	snd_pcm_uframes_t          frame_pos;
	enum lx_stream_status      status; 
	unsigned int               is_capture:1;
};


struct lx6464es {
	struct snd_card        *card;
	struct pci_dev         *pci;
	int			irq;

	u8			mac_address[6];

	spinlock_t		lock;        
	struct mutex            setup_mutex; 

	struct tasklet_struct   trigger_tasklet; 
	struct tasklet_struct   tasklet_capture;
	struct tasklet_struct   tasklet_playback;

	
	unsigned long		port_plx;	   
	void __iomem           *port_plx_remapped; 
	void __iomem           *port_dsp_bar;      

	
	spinlock_t		msg_lock;          
	struct lx_rmh           rmh;

	
	uint			freq_ratio : 2;
	uint                    playback_mute : 1;
	uint                    hardware_running[2];
	u32                     board_sample_rate; 
	u16                     pcm_granularity;   

	
	struct snd_dma_buffer   capture_dma_buf;
	struct snd_dma_buffer   playback_dma_buf;

	
	struct snd_pcm         *pcm;

	
	struct lx_stream        capture_stream;
	struct lx_stream        playback_stream;
};


#endif 
