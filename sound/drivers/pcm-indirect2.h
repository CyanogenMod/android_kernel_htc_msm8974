/*
 * Helper functions for indirect PCM data transfer to a simple FIFO in
 * hardware (small, no possibility to read "hardware io position",
 * updating position done by interrupt, ...)
 *
 *  Copyright (c) by 2007  Joachim Foerster <JOFT@gmx.de>
 *
 *  Based on "pcm-indirect.h" (alsa-driver-1.0.13) by
 *
 *  Copyright (c) by Takashi Iwai <tiwai@suse.de>
 *                   Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SOUND_PCM_INDIRECT2_H
#define __SOUND_PCM_INDIRECT2_H

#include <sound/pcm.h>

#ifdef CONFIG_SND_DEBUG
#define SND_PCM_INDIRECT2_STAT    
#endif

struct snd_pcm_indirect2 {
	unsigned int hw_buffer_size;  
	int hw_ready;		      
	unsigned int min_multiple;
	int min_periods;	      
	int min_period_count;	      

	unsigned int sw_buffer_size;  

	unsigned int sw_data;         

	unsigned int sw_io;           

	int sw_ready;		  

	snd_pcm_uframes_t appl_ptr;   

	unsigned int bytes2hw;
	int check_alignment;

#ifdef SND_PCM_INDIRECT2_STAT
	unsigned int zeros2hw;
	unsigned int mul_elapsed;
	unsigned int mul_elapsed_real;
	unsigned long firstbytetime;
	unsigned long lastbytetime;
	unsigned long firstzerotime;
	unsigned int byte_sizes[64];
	unsigned int zero_sizes[64];
	unsigned int min_adds[8];
	unsigned int mul_adds[8];
	unsigned int zero_times[3750];	
	unsigned int zero_times_saved;
	unsigned int zero_times_notsaved;
	unsigned int irq_occured;
	unsigned int pointer_calls;
	unsigned int lastdifftime;
#endif
};

typedef size_t (*snd_pcm_indirect2_copy_t) (struct snd_pcm_substream *substream,
					   struct snd_pcm_indirect2 *rec,
					   size_t bytes);
typedef size_t (*snd_pcm_indirect2_zero_t) (struct snd_pcm_substream *substream,
					   struct snd_pcm_indirect2 *rec);

#ifdef SND_PCM_INDIRECT2_STAT
void snd_pcm_indirect2_stat(struct snd_pcm_substream *substream,
				   struct snd_pcm_indirect2 *rec);
#endif

snd_pcm_uframes_t
snd_pcm_indirect2_pointer(struct snd_pcm_substream *substream,
			  struct snd_pcm_indirect2 *rec);
void
snd_pcm_indirect2_playback_interrupt(struct snd_pcm_substream *substream,
				     struct snd_pcm_indirect2 *rec,
				     snd_pcm_indirect2_copy_t copy,
				     snd_pcm_indirect2_zero_t zero);
void
snd_pcm_indirect2_capture_interrupt(struct snd_pcm_substream *substream,
				    struct snd_pcm_indirect2 *rec,
				    snd_pcm_indirect2_copy_t copy,
				    snd_pcm_indirect2_zero_t null);

#endif 
