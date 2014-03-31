#ifndef __SOUND_SOUNDFONT_H
#define __SOUND_SOUNDFONT_H

/*
 *  Soundfont defines and definitions.
 *
 *  Copyright (C) 1999 Steve Ratcliffe
 *  Copyright (c) 1999-2000 Takashi iwai <tiwai@suse.de>
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

#include "sfnt_info.h"
#include "util_mem.h"

#define SF_MAX_INSTRUMENTS	128	
#define SF_MAX_PRESETS  256	
#define SF_IS_DRUM_BANK(z) ((z) == 128)

struct snd_sf_zone {
	struct snd_sf_zone *next;	
	unsigned char bank;		
	unsigned char instr;		
	unsigned char mapped;		

	struct soundfont_voice_info v;	
	int counter;
	struct snd_sf_sample *sample;	

	
	struct snd_sf_zone *next_instr;	
	struct snd_sf_zone *next_zone;	
};

struct snd_sf_sample {
	struct soundfont_sample_info v;
	int counter;
	struct snd_util_memblk *block;	
	struct snd_sf_sample *next;
};

struct snd_soundfont {
	struct snd_soundfont *next;	
		
	short  id;		
	short  type;		
	unsigned char name[SNDRV_SFNT_PATCH_NAME_LEN];	
	struct snd_sf_zone *zones; 
	struct snd_sf_sample *samples; 
};

struct snd_sf_callback {
	void *private_data;
	int (*sample_new)(void *private_data, struct snd_sf_sample *sp,
			  struct snd_util_memhdr *hdr,
			  const void __user *buf, long count);
	int (*sample_free)(void *private_data, struct snd_sf_sample *sp,
			   struct snd_util_memhdr *hdr);
	void (*sample_reset)(void *private);
};

struct snd_sf_list {
	struct snd_soundfont *currsf; 
	int open_client;	
	int mem_used;		
	struct snd_sf_zone *presets[SF_MAX_PRESETS];
	struct snd_soundfont *fonts; 
	int fonts_size;	
	int zone_counter;	
	int sample_counter;	
	int zone_locked;	
	int sample_locked;	
	struct snd_sf_callback callback;	
	int presets_locked;
	struct mutex presets_mutex;
	spinlock_t lock;
	struct snd_util_memhdr *memhdr;
};

int snd_soundfont_load(struct snd_sf_list *sflist, const void __user *data,
		       long count, int client);
int snd_soundfont_load_guspatch(struct snd_sf_list *sflist, const char __user *data,
				long count, int client);
int snd_soundfont_close_check(struct snd_sf_list *sflist, int client);

struct snd_sf_list *snd_sf_new(struct snd_sf_callback *callback,
			       struct snd_util_memhdr *hdr);
void snd_sf_free(struct snd_sf_list *sflist);

int snd_soundfont_remove_samples(struct snd_sf_list *sflist);
int snd_soundfont_remove_unlocked(struct snd_sf_list *sflist);

int snd_soundfont_search_zone(struct snd_sf_list *sflist, int *notep, int vel,
			      int preset, int bank,
			      int def_preset, int def_bank,
			      struct snd_sf_zone **table, int max_layers);

int snd_sf_calc_parm_hold(int msec);
int snd_sf_calc_parm_attack(int msec);
int snd_sf_calc_parm_decay(int msec);
#define snd_sf_calc_parm_delay(msec) (0x8000 - (msec) * 1000 / 725)
extern int snd_sf_vol_table[128];
int snd_sf_linear_to_log(unsigned int amount, int offset, int ratio);


#endif 
