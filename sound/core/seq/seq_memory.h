/*
 *  ALSA sequencer Memory Manager
 *  Copyright (c) 1998 by Frank van de Pol <fvdpol@coil.demon.nl>
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
#ifndef __SND_SEQ_MEMORYMGR_H
#define __SND_SEQ_MEMORYMGR_H

#include <sound/seq_kernel.h>
#include <linux/poll.h>

struct snd_info_buffer;

struct snd_seq_event_cell {
	struct snd_seq_event event;
	struct snd_seq_pool *pool;				
	struct snd_seq_event_cell *next;	
};


struct snd_seq_pool {
	struct snd_seq_event_cell *ptr;	
	struct snd_seq_event_cell *free;	

	int total_elements;	
	atomic_t counter;	

	int size;		
	int room;		

	int closing;

	
	int max_used;
	int event_alloc_nopool;
	int event_alloc_failures;
	int event_alloc_success;

	
	wait_queue_head_t output_sleep;

	
	spinlock_t lock;
};

void snd_seq_cell_free(struct snd_seq_event_cell *cell);

int snd_seq_event_dup(struct snd_seq_pool *pool, struct snd_seq_event *event,
		      struct snd_seq_event_cell **cellp, int nonblock, struct file *file);

static inline int snd_seq_unused_cells(struct snd_seq_pool *pool)
{
	return pool ? pool->total_elements - atomic_read(&pool->counter) : 0;
}

static inline int snd_seq_total_cells(struct snd_seq_pool *pool)
{
	return pool ? pool->total_elements : 0;
}

int snd_seq_pool_init(struct snd_seq_pool *pool);

int snd_seq_pool_done(struct snd_seq_pool *pool);

struct snd_seq_pool *snd_seq_pool_new(int poolsize);

int snd_seq_pool_delete(struct snd_seq_pool **pool);

int snd_sequencer_memory_init(void);
            
void snd_sequencer_memory_done(void);

int snd_seq_pool_poll_wait(struct snd_seq_pool *pool, struct file *file, poll_table *wait);

void snd_seq_info_pool(struct snd_info_buffer *buffer,
		       struct snd_seq_pool *pool, char *space);

#endif
