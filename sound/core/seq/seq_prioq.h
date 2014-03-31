/*
 *   ALSA sequencer Priority Queue
 *   Copyright (c) 1998 by Frank van de Pol <fvdpol@coil.demon.nl>
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
#ifndef __SND_SEQ_PRIOQ_H
#define __SND_SEQ_PRIOQ_H

#include "seq_memory.h"



struct snd_seq_prioq {
	struct snd_seq_event_cell *head;      
	struct snd_seq_event_cell *tail;      
	int cells;
	spinlock_t lock;
};


struct snd_seq_prioq *snd_seq_prioq_new(void);

void snd_seq_prioq_delete(struct snd_seq_prioq **fifo);

int snd_seq_prioq_cell_in(struct snd_seq_prioq *f, struct snd_seq_event_cell *cell);

 
struct snd_seq_event_cell *snd_seq_prioq_cell_out(struct snd_seq_prioq *f);

int snd_seq_prioq_avail(struct snd_seq_prioq *f);

struct snd_seq_event_cell *snd_seq_prioq_cell_peek(struct snd_seq_prioq *f);

void snd_seq_prioq_leave(struct snd_seq_prioq *f, int client, int timestamp);        

void snd_seq_prioq_remove_events(struct snd_seq_prioq *f, int client,
				 struct snd_seq_remove_events *info);

#endif
