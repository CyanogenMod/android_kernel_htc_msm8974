/*
 *  IBM eServer eHCA Infiniband device driver for Linux on POWER
 *
 *  internal queue handling
 *
 *  Authors: Waleri Fomin <fomin@de.ibm.com>
 *           Reinhard Ernst <rernst@de.ibm.com>
 *           Christoph Raisch <raisch@de.ibm.com>
 *
 *  Copyright (c) 2005 IBM Corporation
 *
 *  All rights reserved.
 *
 *  This source code is distributed under a dual license of GPL v2.0 and OpenIB
 *  BSD.
 *
 * OpenIB BSD License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials
 * provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __IPZ_PT_FN_H__
#define __IPZ_PT_FN_H__

#define EHCA_PAGESHIFT   12
#define EHCA_PAGESIZE   4096UL
#define EHCA_PAGEMASK   (~(EHCA_PAGESIZE-1))
#define EHCA_PT_ENTRIES 512UL

#include "ehca_tools.h"
#include "ehca_qes.h"

struct ehca_pd;
struct ipz_small_queue_page;

extern struct kmem_cache *small_qp_cache;

struct ipz_page {
	u8 entries[EHCA_PAGESIZE];
};

#define IPZ_SPAGE_PER_KPAGE (PAGE_SIZE / 512)

struct ipz_small_queue_page {
	unsigned long page;
	unsigned long bitmap[IPZ_SPAGE_PER_KPAGE / BITS_PER_LONG];
	int fill;
	void *mapped_addr;
	u32 mmap_count;
	struct list_head list;
};

struct ipz_queue {
	u64 current_q_offset;	

	struct ipz_page **queue_pages;	
	u32 qe_size;		
	u32 act_nr_of_sg;
	u32 queue_length;	
	u32 pagesize;
	u32 toggle_state;	
	u32 offset; 
	struct ipz_small_queue_page *small_page;
};

static inline void *ipz_qeit_calc(struct ipz_queue *queue, u64 q_offset)
{
	struct ipz_page *current_page;
	if (q_offset >= queue->queue_length)
		return NULL;
	current_page = (queue->queue_pages)[q_offset >> EHCA_PAGESHIFT];
	return &current_page->entries[q_offset & (EHCA_PAGESIZE - 1)];
}

static inline void *ipz_qeit_get(struct ipz_queue *queue)
{
	return ipz_qeit_calc(queue, queue->current_q_offset);
}

void *ipz_qpageit_get_inc(struct ipz_queue *queue);

static inline void *ipz_qeit_get_inc(struct ipz_queue *queue)
{
	void *ret = ipz_qeit_get(queue);
	queue->current_q_offset += queue->qe_size;
	if (queue->current_q_offset >= queue->queue_length) {
		queue->current_q_offset = 0;
		
		queue->toggle_state = (~queue->toggle_state) & 1;
	}

	return ret;
}

static inline int ipz_qeit_is_valid(struct ipz_queue *queue)
{
	struct ehca_cqe *cqe = ipz_qeit_get(queue);
	return ((cqe->cqe_flags >> 7) == (queue->toggle_state & 1));
}

static inline void *ipz_qeit_get_inc_valid(struct ipz_queue *queue)
{
	return ipz_qeit_is_valid(queue) ? ipz_qeit_get_inc(queue) : NULL;
}

static inline void *ipz_qeit_reset(struct ipz_queue *queue)
{
	queue->current_q_offset = 0;
	return ipz_qeit_get(queue);
}

int ipz_queue_abs_to_offset(struct ipz_queue *queue, u64 addr, u64 *q_offset);

static inline u64 ipz_queue_advance_offset(struct ipz_queue *queue, u64 offset)
{
	offset += queue->qe_size;
	if (offset >= queue->queue_length) offset = 0;
	return offset;
}

struct ipz_pt {
	u64 entries[EHCA_PT_ENTRIES];
};

struct ipz_qpt {
	
	u64 *qpts;
	u32 n_qpts;
	u32 n_ptes;       
	u64 *current_pte_addr;
};

int ipz_queue_ctor(struct ehca_pd *pd, struct ipz_queue *queue,
		   const u32 nr_of_pages, const u32 pagesize,
		   const u32 qe_size, const u32 nr_of_sg,
		   int is_small);

int ipz_queue_dtor(struct ehca_pd *pd, struct ipz_queue *queue);

void ipz_qpt_ctor(struct ipz_qpt *qpt,
		  const u32 nr_of_qes,
		  const u32 pagesize,
		  const u32 qe_size,
		  const u8 lowbyte, const u8 toggle,
		  u32 * act_nr_of_QEs, u32 * act_nr_of_pages);

void *ipz_qeit_eq_get_inc(struct ipz_queue *queue);

static inline void *ipz_eqit_eq_get_inc_valid(struct ipz_queue *queue)
{
	void *ret = ipz_qeit_get(queue);
	u32 qe = *(u8 *)ret;
	if ((qe >> 7) != (queue->toggle_state & 1))
		return NULL;
	ipz_qeit_eq_get_inc(queue); 
	return ret;
}

static inline void *ipz_eqit_eq_peek_valid(struct ipz_queue *queue)
{
	void *ret = ipz_qeit_get(queue);
	u32 qe = *(u8 *)ret;
	if ((qe >> 7) != (queue->toggle_state & 1))
		return NULL;
	return ret;
}

static inline u64 ipz_qpt_get_firstpage(struct ipz_qpt *qpt)
{
	return be64_to_cpu(qpt->qpts[0]);
}

static inline void *ipz_qpt_get_qpt(struct ipz_qpt *qpt)
{
	return qpt->qpts;
}

#endif				
