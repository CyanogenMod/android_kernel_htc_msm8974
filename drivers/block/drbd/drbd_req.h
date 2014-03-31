/*
   drbd_req.h

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2006-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2006-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.
   Copyright (C) 2006-2008, Philipp Reisner <philipp.reisner@linbit.com>.

   DRBD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   DRBD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _DRBD_REQ_H
#define _DRBD_REQ_H

#include <linux/module.h>

#include <linux/slab.h>
#include <linux/drbd.h>
#include "drbd_int.h"
#include "drbd_wrappers.h"



enum drbd_req_event {
	created,
	to_be_send,
	to_be_submitted,

	queue_for_net_write,
	queue_for_net_read,
	queue_for_send_oos,

	send_canceled,
	send_failed,
	handed_over_to_network,
	oos_handed_to_network,
	connection_lost_while_pending,
	read_retry_remote_canceled,
	recv_acked_by_peer,
	write_acked_by_peer,
	write_acked_by_peer_and_sis, 
	conflict_discarded_by_peer,
	neg_acked,
	barrier_acked, 
	data_received, 

	read_completed_with_error,
	read_ahead_completed_with_error,
	write_completed_with_error,
	completed_ok,
	resend,
	fail_frozen_disk_io,
	restart_frozen_disk_io,
	nothing, 
};

enum drbd_req_state_bits {
	__RQ_LOCAL_PENDING,
	__RQ_LOCAL_COMPLETED,
	__RQ_LOCAL_OK,


	__RQ_NET_PENDING,

	__RQ_NET_QUEUED,

	__RQ_NET_SENT,

	__RQ_NET_DONE,

	/* whether or not we know (C) or pretend (B,A) that the write
	 * was successfully written on the peer.
	 */
	__RQ_NET_OK,

	
	__RQ_NET_SIS,

	
	__RQ_NET_MAX,

	
	__RQ_WRITE,

	
	__RQ_IN_ACT_LOG,
};

#define RQ_LOCAL_PENDING   (1UL << __RQ_LOCAL_PENDING)
#define RQ_LOCAL_COMPLETED (1UL << __RQ_LOCAL_COMPLETED)
#define RQ_LOCAL_OK        (1UL << __RQ_LOCAL_OK)

#define RQ_LOCAL_MASK      ((RQ_LOCAL_OK << 1)-1) 

#define RQ_NET_PENDING     (1UL << __RQ_NET_PENDING)
#define RQ_NET_QUEUED      (1UL << __RQ_NET_QUEUED)
#define RQ_NET_SENT        (1UL << __RQ_NET_SENT)
#define RQ_NET_DONE        (1UL << __RQ_NET_DONE)
#define RQ_NET_OK          (1UL << __RQ_NET_OK)
#define RQ_NET_SIS         (1UL << __RQ_NET_SIS)

#define RQ_NET_MASK        (((1UL << __RQ_NET_MAX)-1) & ~RQ_LOCAL_MASK)

#define RQ_WRITE           (1UL << __RQ_WRITE)
#define RQ_IN_ACT_LOG      (1UL << __RQ_IN_ACT_LOG)

#define MR_WRITE_SHIFT 0
#define MR_WRITE       (1 << MR_WRITE_SHIFT)
#define MR_READ_SHIFT  1
#define MR_READ        (1 << MR_READ_SHIFT)

static inline
struct hlist_head *ee_hash_slot(struct drbd_conf *mdev, sector_t sector)
{
	BUG_ON(mdev->ee_hash_s == 0);
	return mdev->ee_hash +
		((unsigned int)(sector>>HT_SHIFT) % mdev->ee_hash_s);
}

static inline
struct hlist_head *tl_hash_slot(struct drbd_conf *mdev, sector_t sector)
{
	BUG_ON(mdev->tl_hash_s == 0);
	return mdev->tl_hash +
		((unsigned int)(sector>>HT_SHIFT) % mdev->tl_hash_s);
}

static struct hlist_head *ar_hash_slot(struct drbd_conf *mdev, sector_t sector)
{
	return mdev->app_reads_hash
		+ ((unsigned int)(sector) % APP_R_HSIZE);
}

static inline struct drbd_request *_ar_id_to_req(struct drbd_conf *mdev,
	u64 id, sector_t sector)
{
	struct hlist_head *slot = ar_hash_slot(mdev, sector);
	struct hlist_node *n;
	struct drbd_request *req;

	hlist_for_each_entry(req, n, slot, collision) {
		if ((unsigned long)req == (unsigned long)id) {
			D_ASSERT(req->sector == sector);
			return req;
		}
	}
	return NULL;
}

static inline void drbd_req_make_private_bio(struct drbd_request *req, struct bio *bio_src)
{
	struct bio *bio;
	bio = bio_clone(bio_src, GFP_NOIO); 

	req->private_bio = bio;

	bio->bi_private  = req;
	bio->bi_end_io   = drbd_endio_pri;
	bio->bi_next     = NULL;
}

static inline struct drbd_request *drbd_req_new(struct drbd_conf *mdev,
	struct bio *bio_src)
{
	struct drbd_request *req =
		mempool_alloc(drbd_request_mempool, GFP_NOIO);
	if (likely(req)) {
		drbd_req_make_private_bio(req, bio_src);

		req->rq_state    = bio_data_dir(bio_src) == WRITE ? RQ_WRITE : 0;
		req->mdev        = mdev;
		req->master_bio  = bio_src;
		req->epoch       = 0;
		req->sector      = bio_src->bi_sector;
		req->size        = bio_src->bi_size;
		INIT_HLIST_NODE(&req->collision);
		INIT_LIST_HEAD(&req->tl_requests);
		INIT_LIST_HEAD(&req->w.list);
	}
	return req;
}

static inline void drbd_req_free(struct drbd_request *req)
{
	mempool_free(req, drbd_request_mempool);
}

static inline int overlaps(sector_t s1, int l1, sector_t s2, int l2)
{
	return !((s1 + (l1>>9) <= s2) || (s1 >= s2 + (l2>>9)));
}

struct bio_and_error {
	struct bio *bio;
	int error;
};

extern void _req_may_be_done(struct drbd_request *req,
		struct bio_and_error *m);
extern int __req_mod(struct drbd_request *req, enum drbd_req_event what,
		struct bio_and_error *m);
extern void complete_master_bio(struct drbd_conf *mdev,
		struct bio_and_error *m);
extern void request_timer_fn(unsigned long data);
extern void tl_restart(struct drbd_conf *mdev, enum drbd_req_event what);

static inline int _req_mod(struct drbd_request *req, enum drbd_req_event what)
{
	struct drbd_conf *mdev = req->mdev;
	struct bio_and_error m;
	int rv;

	
	rv = __req_mod(req, what, &m);
	if (m.bio)
		complete_master_bio(mdev, &m);

	return rv;
}

static inline int req_mod(struct drbd_request *req,
		enum drbd_req_event what)
{
	unsigned long flags;
	struct drbd_conf *mdev = req->mdev;
	struct bio_and_error m;
	int rv;

	spin_lock_irqsave(&mdev->req_lock, flags);
	rv = __req_mod(req, what, &m);
	spin_unlock_irqrestore(&mdev->req_lock, flags);

	if (m.bio)
		complete_master_bio(mdev, &m);

	return rv;
}

static inline bool drbd_should_do_remote(union drbd_state s)
{
	return s.pdsk == D_UP_TO_DATE ||
		(s.pdsk >= D_INCONSISTENT &&
		 s.conn >= C_WF_BITMAP_T &&
		 s.conn < C_AHEAD);
}
static inline bool drbd_should_send_oos(union drbd_state s)
{
	return s.conn == C_AHEAD || s.conn == C_WF_BITMAP_S;
}

#endif
