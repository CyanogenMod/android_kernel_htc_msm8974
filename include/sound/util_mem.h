#ifndef __SOUND_UTIL_MEM_H
#define __SOUND_UTIL_MEM_H

#include <linux/mutex.h>
/*
 *  Copyright (C) 2000 Takashi Iwai <tiwai@suse.de>
 *
 *  Generic memory management routines for soundcard memory allocation
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

struct snd_util_memblk {
	unsigned int size;		
	unsigned int offset;		
	struct list_head list;		
};

#define snd_util_memblk_argptr(blk)	(void*)((char*)(blk) + sizeof(struct snd_util_memblk))

struct snd_util_memhdr {
	unsigned int size;		
	struct list_head block;		
	int nblocks;			
	unsigned int used;		
	int block_extra_size;		
	struct mutex block_mutex;	
};

struct snd_util_memhdr *snd_util_memhdr_new(int memsize);
void snd_util_memhdr_free(struct snd_util_memhdr *hdr);
struct snd_util_memblk *snd_util_mem_alloc(struct snd_util_memhdr *hdr, int size);
int snd_util_mem_free(struct snd_util_memhdr *hdr, struct snd_util_memblk *blk);
int snd_util_mem_avail(struct snd_util_memhdr *hdr);

struct snd_util_memblk *__snd_util_mem_alloc(struct snd_util_memhdr *hdr, int size);
void __snd_util_mem_free(struct snd_util_memhdr *hdr, struct snd_util_memblk *blk);
struct snd_util_memblk *__snd_util_memblk_new(struct snd_util_memhdr *hdr,
					      unsigned int units,
					      struct list_head *prev);

#endif 
