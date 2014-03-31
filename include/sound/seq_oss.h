#ifndef __SOUND_SEQ_OSS_H
#define __SOUND_SEQ_OSS_H

/*
 * OSS compatible sequencer driver
 *
 * Copyright (C) 1998,99 Takashi Iwai
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "asequencer.h"
#include "seq_kernel.h"

struct snd_seq_oss_arg {
	
	int app_index;	
	int file_mode;	
	int seq_mode;	

	
	struct snd_seq_addr addr;	
	void *private_data;	

	int event_passing;
};


struct snd_seq_oss_callback {
	struct module *owner;
	int (*open)(struct snd_seq_oss_arg *p, void *closure);
	int (*close)(struct snd_seq_oss_arg *p);
	int (*ioctl)(struct snd_seq_oss_arg *p, unsigned int cmd, unsigned long arg);
	int (*load_patch)(struct snd_seq_oss_arg *p, int format, const char __user *buf, int offs, int count);
	int (*reset)(struct snd_seq_oss_arg *p);
	int (*raw_event)(struct snd_seq_oss_arg *p, unsigned char *data);
};

#define SNDRV_SEQ_OSS_FILE_ACMODE		3
#define SNDRV_SEQ_OSS_FILE_READ		1
#define SNDRV_SEQ_OSS_FILE_WRITE		2
#define SNDRV_SEQ_OSS_FILE_NONBLOCK	4

#define SNDRV_SEQ_OSS_MODE_SYNTH		0
#define SNDRV_SEQ_OSS_MODE_MUSIC		1

#define SNDRV_SEQ_OSS_PROCESS_EVENTS	0	
#define SNDRV_SEQ_OSS_PASS_EVENTS		1	
#define SNDRV_SEQ_OSS_PROCESS_KEYPRESS	2	

#define SNDRV_SEQ_OSS_CTRLRATE		100

#define SNDRV_SEQ_OSS_MAX_QLEN		1024


struct snd_seq_oss_reg {
	int type;
	int subtype;
	int nvoices;
	struct snd_seq_oss_callback oper;
	void *private_data;
};

#define SNDRV_SEQ_DEV_ID_OSS		"seq-oss"

#endif 
