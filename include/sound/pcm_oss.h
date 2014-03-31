#ifndef __SOUND_PCM_OSS_H
#define __SOUND_PCM_OSS_H

/*
 *  Digital Audio (PCM) - OSS compatibility abstract layer
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
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

struct snd_pcm_oss_setup {
	char *task_name;
	unsigned int disable:1,
		     direct:1,
		     block:1,
		     nonblock:1,
		     partialfrag:1,
		     nosilence:1,
		     buggyptr:1;
	unsigned int periods;
	unsigned int period_size;
	struct snd_pcm_oss_setup *next;
};

struct snd_pcm_oss_runtime {
	unsigned params: 1,			
		 prepare: 1,			
		 trigger: 1,			
		 sync_trigger: 1;		
	int rate;				
	int format;				
	unsigned int channels;			
	unsigned int fragshift;
	unsigned int maxfrags;
	unsigned int subdivision;		
	size_t period_bytes;			
	size_t period_frames;			
	size_t period_ptr;			
	unsigned int periods;
	size_t buffer_bytes;			
	size_t bytes;				
	size_t mmap_bytes;
	char *buffer;				
	size_t buffer_used;			
	struct mutex params_lock;
#ifdef CONFIG_SND_PCM_OSS_PLUGINS
	struct snd_pcm_plugin *plugin_first;
	struct snd_pcm_plugin *plugin_last;
#endif
	unsigned int prev_hw_ptr_period;
};

struct snd_pcm_oss_file {
	struct snd_pcm_substream *streams[2];
};

struct snd_pcm_oss_substream {
	unsigned oss: 1;			
	struct snd_pcm_oss_setup setup;		
};

struct snd_pcm_oss_stream {
	struct snd_pcm_oss_setup *setup_list;	
	struct mutex setup_mutex;
#ifdef CONFIG_SND_VERBOSE_PROCFS
	struct snd_info_entry *proc_entry;
#endif
};

struct snd_pcm_oss {
	int reg;
	unsigned int reg_mask;
};

#endif 
