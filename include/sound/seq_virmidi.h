#ifndef __SOUND_SEQ_VIRMIDI_H
#define __SOUND_SEQ_VIRMIDI_H

/*
 *  Virtual Raw MIDI client on Sequencer
 *  Copyright (c) 2000 by Takashi Iwai <tiwai@suse.de>,
 *                        Jaroslav Kysela <perex@perex.cz>
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

#include "rawmidi.h"
#include "seq_midi_event.h"

struct snd_virmidi {
	struct list_head list;
	int seq_mode;
	int client;
	int port;
	unsigned int trigger: 1;
	struct snd_midi_event *parser;
	struct snd_seq_event event;
	struct snd_virmidi_dev *rdev;
	struct snd_rawmidi_substream *substream;
};

#define SNDRV_VIRMIDI_SUBSCRIBE		(1<<0)
#define SNDRV_VIRMIDI_USE		(1<<1)

struct snd_virmidi_dev {
	struct snd_card *card;		
	struct snd_rawmidi *rmidi;		
	int seq_mode;			
	int device;			
	int client;			
	int port;			
	unsigned int flags;		
	rwlock_t filelist_lock;
	struct list_head filelist;
};

#define SNDRV_VIRMIDI_SEQ_NONE		0
#define SNDRV_VIRMIDI_SEQ_ATTACH	1
#define SNDRV_VIRMIDI_SEQ_DISPATCH	2

int snd_virmidi_new(struct snd_card *card, int device, struct snd_rawmidi **rrmidi);

#endif 
