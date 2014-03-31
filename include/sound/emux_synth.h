#ifndef __SOUND_EMUX_SYNTH_H
#define __SOUND_EMUX_SYNTH_H

/*
 *  Defines for the Emu-series WaveTable chip
 *
 *  Copyright (C) 2000 Takashi Iwai <tiwai@suse.de>
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

#include "seq_kernel.h"
#include "seq_device.h"
#include "soundfont.h"
#include "seq_midi_emul.h"
#ifdef CONFIG_SND_SEQUENCER_OSS
#include "seq_oss.h"
#endif
#include "emux_legacy.h"
#include "seq_virmidi.h"

#define SNDRV_EMUX_USE_RAW_EFFECT

struct snd_emux;
struct snd_emux_port;
struct snd_emux_voice;
struct snd_emux_effect_table;

struct snd_emux_operators {
	struct module *owner;
	struct snd_emux_voice *(*get_voice)(struct snd_emux *emu,
					    struct snd_emux_port *port);
	int (*prepare)(struct snd_emux_voice *vp);
	void (*trigger)(struct snd_emux_voice *vp);
	void (*release)(struct snd_emux_voice *vp);
	void (*update)(struct snd_emux_voice *vp, int update);
	void (*terminate)(struct snd_emux_voice *vp);
	void (*free_voice)(struct snd_emux_voice *vp);
	void (*reset)(struct snd_emux *emu, int ch);
	
	int (*sample_new)(struct snd_emux *emu, struct snd_sf_sample *sp,
			  struct snd_util_memhdr *hdr,
			  const void __user *data, long count);
	int (*sample_free)(struct snd_emux *emu, struct snd_sf_sample *sp,
			   struct snd_util_memhdr *hdr);
	void (*sample_reset)(struct snd_emux *emu);
	int (*load_fx)(struct snd_emux *emu, int type, int arg,
		       const void __user *data, long count);
	void (*sysex)(struct snd_emux *emu, char *buf, int len, int parsed,
		      struct snd_midi_channel_set *chset);
#ifdef CONFIG_SND_SEQUENCER_OSS
	int (*oss_ioctl)(struct snd_emux *emu, int cmd, int p1, int p2);
#endif
};


#define SNDRV_EMUX_MAX_PORTS		32	
#define SNDRV_EMUX_MAX_VOICES		64	
#define SNDRV_EMUX_MAX_MULTI_VOICES	16	

#define SNDRV_EMUX_ACCEPT_ROM		(1<<0)

struct snd_emux {

	struct snd_card *card;	

	
	int max_voices;		
	int mem_size;		
	int num_ports;		
	int pitch_shift;	
	struct snd_emux_operators ops;	
	void *hw;		
	unsigned long flags;	
	int midi_ports;		
	int midi_devidx;	
	unsigned int linear_panning: 1; 
	int hwdep_idx;		
	struct snd_hwdep *hwdep;	

	
	int num_voices;		
	struct snd_sf_list *sflist;	
	struct snd_emux_voice *voices;	
	int use_time;	
	spinlock_t voice_lock;	
	struct mutex register_mutex;
	int client;		
	int ports[SNDRV_EMUX_MAX_PORTS];	
	struct snd_emux_port *portptrs[SNDRV_EMUX_MAX_PORTS];
	int used;	
	char *name;	
	struct snd_rawmidi **vmidi;
	struct timer_list tlist;	
	int timer_active;

	struct snd_util_memhdr *memhdr;	

#ifdef CONFIG_PROC_FS
	struct snd_info_entry *proc;
#endif

#ifdef CONFIG_SND_SEQUENCER_OSS
	struct snd_seq_device *oss_synth;
#endif
};


struct snd_emux_port {

	struct snd_midi_channel_set chset;
	struct snd_emux *emu;

	char port_mode;			
	int volume_atten;		
	unsigned long drum_flags;	
	int ctrls[EMUX_MD_END];		
#ifdef SNDRV_EMUX_USE_RAW_EFFECT
	struct snd_emux_effect_table *effect;
#endif
#ifdef CONFIG_SND_SEQUENCER_OSS
	struct snd_seq_oss_arg *oss_arg;
#endif
};

#define SNDRV_EMUX_PORT_MODE_MIDI		0	
#define SNDRV_EMUX_PORT_MODE_OSS_SYNTH	1	
#define SNDRV_EMUX_PORT_MODE_OSS_MIDI	2	

struct snd_emux_voice {
	int  ch;		

	int  state;		
#define SNDRV_EMUX_ST_OFF		0x00	
#define SNDRV_EMUX_ST_ON		0x01	
#define SNDRV_EMUX_ST_RELEASED 	(0x02|SNDRV_EMUX_ST_ON)	
#define SNDRV_EMUX_ST_SUSTAINED	(0x04|SNDRV_EMUX_ST_ON)	
#define SNDRV_EMUX_ST_STANDBY	(0x08|SNDRV_EMUX_ST_ON)	
#define SNDRV_EMUX_ST_PENDING 	(0x10|SNDRV_EMUX_ST_ON)	
#define SNDRV_EMUX_ST_LOCKED		0x100	

	unsigned int  time;	
	unsigned char note;	
	unsigned char key;
	unsigned char velocity;	

	struct snd_sf_zone *zone;	
	void *block;		
	struct snd_midi_channel *chan;	
	struct snd_emux_port *port;	
	struct snd_emux *emu;	
	void *hw;		
	unsigned long ontime;	
	
	
	struct soundfont_voice_info reg;

	
	int avol;		
	int acutoff;		
	int apitch;		
	int apan;		
	int aaux;
	int ptarget;		
	int vtarget;		
	int ftarget;		

};

#define SNDRV_EMUX_UPDATE_VOLUME		(1<<0)
#define SNDRV_EMUX_UPDATE_PITCH		(1<<1)
#define SNDRV_EMUX_UPDATE_PAN		(1<<2)
#define SNDRV_EMUX_UPDATE_FMMOD		(1<<3)
#define SNDRV_EMUX_UPDATE_TREMFREQ	(1<<4)
#define SNDRV_EMUX_UPDATE_FM2FRQ2		(1<<5)
#define SNDRV_EMUX_UPDATE_Q		(1<<6)


#ifdef SNDRV_EMUX_USE_RAW_EFFECT
struct snd_emux_effect_table {
	
	short val[EMUX_NUM_EFFECTS];
	unsigned char flag[EMUX_NUM_EFFECTS];
};
#endif 


int snd_emux_new(struct snd_emux **remu);
int snd_emux_register(struct snd_emux *emu, struct snd_card *card, int index, char *name);
int snd_emux_free(struct snd_emux *emu);

void snd_emux_terminate_all(struct snd_emux *emu);
void snd_emux_lock_voice(struct snd_emux *emu, int voice);
void snd_emux_unlock_voice(struct snd_emux *emu, int voice);

#endif 
