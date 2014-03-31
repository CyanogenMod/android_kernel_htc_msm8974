/*
 * zfcp device driver
 *
 * Header file for zfcp qdio interface
 *
 * Copyright IBM Corporation 2010
 */

#ifndef ZFCP_QDIO_H
#define ZFCP_QDIO_H

#include <asm/qdio.h>

#define ZFCP_QDIO_SBALE_LEN	PAGE_SIZE

#define ZFCP_QDIO_MAX_SBALS_PER_REQ	36

struct zfcp_qdio {
	struct qdio_buffer	*res_q[QDIO_MAX_BUFFERS_PER_Q];
	struct qdio_buffer	*req_q[QDIO_MAX_BUFFERS_PER_Q];
	u8			req_q_idx;
	atomic_t		req_q_free;
	spinlock_t		stat_lock;
	spinlock_t		req_q_lock;
	unsigned long long	req_q_time;
	u64			req_q_util;
	atomic_t		req_q_full;
	wait_queue_head_t	req_q_wq;
	struct zfcp_adapter	*adapter;
	u16			max_sbale_per_sbal;
	u16			max_sbale_per_req;
};

struct zfcp_qdio_req {
	u8	sbtype;
	u8	sbal_number;
	u8	sbal_first;
	u8	sbal_last;
	u8	sbal_limit;
	u8	sbale_curr;
	u8	sbal_response;
	u16	qdio_outb_usage;
};

static inline struct qdio_buffer_element *
zfcp_qdio_sbale_req(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	return &qdio->req_q[q_req->sbal_last]->element[0];
}

static inline struct qdio_buffer_element *
zfcp_qdio_sbale_curr(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	return &qdio->req_q[q_req->sbal_last]->element[q_req->sbale_curr];
}

static inline
void zfcp_qdio_req_init(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req,
			unsigned long req_id, u8 sbtype, void *data, u32 len)
{
	struct qdio_buffer_element *sbale;
	int count = min(atomic_read(&qdio->req_q_free),
			ZFCP_QDIO_MAX_SBALS_PER_REQ);

	q_req->sbal_first = q_req->sbal_last = qdio->req_q_idx;
	q_req->sbal_number = 1;
	q_req->sbtype = sbtype;
	q_req->sbale_curr = 1;
	q_req->sbal_limit = (q_req->sbal_first + count - 1)
					% QDIO_MAX_BUFFERS_PER_Q;

	sbale = zfcp_qdio_sbale_req(qdio, q_req);
	sbale->addr = (void *) req_id;
	sbale->eflags = 0;
	sbale->sflags = SBAL_SFLAGS0_COMMAND | sbtype;

	if (unlikely(!data))
		return;
	sbale++;
	sbale->addr = data;
	sbale->length = len;
}

static inline
void zfcp_qdio_fill_next(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req,
			 void *data, u32 len)
{
	struct qdio_buffer_element *sbale;

	BUG_ON(q_req->sbale_curr == qdio->max_sbale_per_sbal - 1);
	q_req->sbale_curr++;
	sbale = zfcp_qdio_sbale_curr(qdio, q_req);
	sbale->addr = data;
	sbale->length = len;
}

static inline
void zfcp_qdio_set_sbale_last(struct zfcp_qdio *qdio,
			      struct zfcp_qdio_req *q_req)
{
	struct qdio_buffer_element *sbale;

	sbale = zfcp_qdio_sbale_curr(qdio, q_req);
	sbale->eflags |= SBAL_EFLAGS_LAST_ENTRY;
}

static inline
int zfcp_qdio_sg_one_sbale(struct scatterlist *sg)
{
	return sg_is_last(sg) && sg->length <= ZFCP_QDIO_SBALE_LEN;
}

static inline
void zfcp_qdio_skip_to_last_sbale(struct zfcp_qdio *qdio,
				  struct zfcp_qdio_req *q_req)
{
	q_req->sbale_curr = qdio->max_sbale_per_sbal - 1;
}

static inline
void zfcp_qdio_sbal_limit(struct zfcp_qdio *qdio,
			  struct zfcp_qdio_req *q_req, int max_sbals)
{
	int count = min(atomic_read(&qdio->req_q_free), max_sbals);

	q_req->sbal_limit = (q_req->sbal_first + count - 1) %
				QDIO_MAX_BUFFERS_PER_Q;
}

static inline
void zfcp_qdio_set_data_div(struct zfcp_qdio *qdio,
			    struct zfcp_qdio_req *q_req, u32 count)
{
	struct qdio_buffer_element *sbale;

	sbale = qdio->req_q[q_req->sbal_first]->element;
	sbale->length = count;
}

static inline
unsigned int zfcp_qdio_sbale_count(struct scatterlist *sg)
{
	unsigned int count = 0;

	for (; sg; sg = sg_next(sg))
		count++;

	return count;
}

static inline
unsigned int zfcp_qdio_real_bytes(struct scatterlist *sg)
{
	unsigned int real_bytes = 0;

	for (; sg; sg = sg_next(sg))
		real_bytes += sg->length;

	return real_bytes;
}

static inline
void zfcp_qdio_set_scount(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	struct qdio_buffer_element *sbale;

	sbale = qdio->req_q[q_req->sbal_first]->element;
	sbale->scount = q_req->sbal_number - 1;
}

#endif 
