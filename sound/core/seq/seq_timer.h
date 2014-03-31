/*
 *  ALSA sequencer Timer
 *  Copyright (c) 1998-1999 by Frank van de Pol <fvdpol@coil.demon.nl>
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
#ifndef __SND_SEQ_TIMER_H
#define __SND_SEQ_TIMER_H

#include <sound/timer.h>
#include <sound/seq_kernel.h>

struct snd_seq_timer_tick {
	snd_seq_tick_time_t	cur_tick;	
	unsigned long		resolution;	
	unsigned long		fraction;	
};

struct snd_seq_timer {
	

	unsigned int		running:1,		
				initialized:1;	

	unsigned int		tempo;		
	int			ppq;		

	snd_seq_real_time_t	cur_time;	
	struct snd_seq_timer_tick	tick;	
	int tick_updated;
	
	int			type;		
	struct snd_timer_id	alsa_id;	
	struct snd_timer_instance	*timeri;	
	unsigned int		ticks;
	unsigned long		preferred_resolution; 

	unsigned int skew;
	unsigned int skew_base;

	struct timeval 		last_update;	 

	spinlock_t lock;
};


struct snd_seq_timer *snd_seq_timer_new(void);

void snd_seq_timer_delete(struct snd_seq_timer **tmr);

static inline void snd_seq_timer_update_tick(struct snd_seq_timer_tick *tick,
					     unsigned long resolution)
{
	if (tick->resolution > 0) {
		tick->fraction += resolution;
		tick->cur_tick += (unsigned int)(tick->fraction / tick->resolution);
		tick->fraction %= tick->resolution;
	}
}


static inline int snd_seq_compare_tick_time(snd_seq_tick_time_t *a, snd_seq_tick_time_t *b)
{
	
	return (*a >= *b);
}

static inline int snd_seq_compare_real_time(snd_seq_real_time_t *a, snd_seq_real_time_t *b)
{
	
	if (a->tv_sec > b->tv_sec)
		return 1;
	if ((a->tv_sec == b->tv_sec) && (a->tv_nsec >= b->tv_nsec))
		return 1;
	return 0;
}


static inline void snd_seq_sanity_real_time(snd_seq_real_time_t *tm)
{
	while (tm->tv_nsec >= 1000000000) {
		
		tm->tv_nsec -= 1000000000;
                tm->tv_sec++;
        }
}


static inline void snd_seq_inc_real_time(snd_seq_real_time_t *tm, snd_seq_real_time_t *inc)
{
	tm->tv_sec  += inc->tv_sec;
	tm->tv_nsec += inc->tv_nsec;
	snd_seq_sanity_real_time(tm);
}

static inline void snd_seq_inc_time_nsec(snd_seq_real_time_t *tm, unsigned long nsec)
{
	tm->tv_nsec  += nsec;
	snd_seq_sanity_real_time(tm);
}

struct snd_seq_queue;
int snd_seq_timer_open(struct snd_seq_queue *q);
int snd_seq_timer_close(struct snd_seq_queue *q);
int snd_seq_timer_midi_open(struct snd_seq_queue *q);
int snd_seq_timer_midi_close(struct snd_seq_queue *q);
void snd_seq_timer_defaults(struct snd_seq_timer *tmr);
void snd_seq_timer_reset(struct snd_seq_timer *tmr);
int snd_seq_timer_stop(struct snd_seq_timer *tmr);
int snd_seq_timer_start(struct snd_seq_timer *tmr);
int snd_seq_timer_continue(struct snd_seq_timer *tmr);
int snd_seq_timer_set_tempo(struct snd_seq_timer *tmr, int tempo);
int snd_seq_timer_set_ppq(struct snd_seq_timer *tmr, int ppq);
int snd_seq_timer_set_position_tick(struct snd_seq_timer *tmr, snd_seq_tick_time_t position);
int snd_seq_timer_set_position_time(struct snd_seq_timer *tmr, snd_seq_real_time_t position);
int snd_seq_timer_set_skew(struct snd_seq_timer *tmr, unsigned int skew, unsigned int base);
snd_seq_real_time_t snd_seq_timer_get_cur_time(struct snd_seq_timer *tmr);
snd_seq_tick_time_t snd_seq_timer_get_cur_tick(struct snd_seq_timer *tmr);

extern int seq_default_timer_class;
extern int seq_default_timer_sclass;
extern int seq_default_timer_card;
extern int seq_default_timer_device;
extern int seq_default_timer_subdevice;
extern int seq_default_timer_resolution;

#endif
