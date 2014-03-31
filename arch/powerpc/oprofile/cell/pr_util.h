 /*
 * Cell Broadband Engine OProfile Support
 *
 * (C) Copyright IBM Corporation 2006
 *
 * Author: Maynard Johnson <maynardj@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef PR_UTIL_H
#define PR_UTIL_H

#include <linux/cpumask.h>
#include <linux/oprofile.h>
#include <asm/cell-pmu.h>
#include <asm/cell-regs.h>
#include <asm/spu.h>

#define SKIP_GENERIC_SYNC 0
#define SYNC_START_ERROR -1
#define DO_GENERIC_SYNC 1
#define SPUS_PER_NODE   8
#define DEFAULT_TIMER_EXPIRE  (HZ / 10)

extern struct delayed_work spu_work;
extern int spu_prof_running;

#define TRACE_ARRAY_SIZE 1024

extern spinlock_t oprof_spu_smpl_arry_lck;

struct spu_overlay_info {	
	unsigned int vma;	
	unsigned int size;	
	unsigned int offset;	
	unsigned int buf;
};

struct vma_to_fileoffset_map {	
	struct vma_to_fileoffset_map *next;	
	unsigned int vma;	
	unsigned int size;	
	unsigned int offset;	
	unsigned int guard_ptr;
	unsigned int guard_val;

};

struct spu_buffer {
	int last_guard_val;
	int ctx_sw_seen;
	unsigned long *buff;
	unsigned int head, tail;
};


struct vma_to_fileoffset_map *create_vma_map(const struct spu *spu,
					     unsigned long objectid);
unsigned int vma_map_lookup(struct vma_to_fileoffset_map *map,
			    unsigned int vma, const struct spu *aSpu,
			    int *grd_val);
void vma_map_free(struct vma_to_fileoffset_map *map);

int start_spu_profiling_cycles(unsigned int cycles_reset);
void start_spu_profiling_events(void);

void stop_spu_profiling_cycles(void);
void stop_spu_profiling_events(void);

int spu_sync_start(void);

int spu_sync_stop(void);

void spu_sync_buffer(int spu_num, unsigned int *samples,
		     int num_samples);

void set_spu_profiling_frequency(unsigned int freq_khz, unsigned int cycles_reset);

#endif	  
