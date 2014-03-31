/*
   drbd_req.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2001-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 1999-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2002-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#include <linux/module.h>

#include <linux/slab.h>
#include <linux/drbd.h>
#include "drbd_int.h"
#include "drbd_req.h"


static void _drbd_start_io_acct(struct drbd_conf *mdev, struct drbd_request *req, struct bio *bio)
{
	const int rw = bio_data_dir(bio);
	int cpu;
	cpu = part_stat_lock();
	part_stat_inc(cpu, &mdev->vdisk->part0, ios[rw]);
	part_stat_add(cpu, &mdev->vdisk->part0, sectors[rw], bio_sectors(bio));
	part_inc_in_flight(&mdev->vdisk->part0, rw);
	part_stat_unlock();
}

static void _drbd_end_io_acct(struct drbd_conf *mdev, struct drbd_request *req)
{
	int rw = bio_data_dir(req->master_bio);
	unsigned long duration = jiffies - req->start_time;
	int cpu;
	cpu = part_stat_lock();
	part_stat_add(cpu, &mdev->vdisk->part0, ticks[rw], duration);
	part_round_stats(cpu, &mdev->vdisk->part0);
	part_dec_in_flight(&mdev->vdisk->part0, rw);
	part_stat_unlock();
}

static void _req_is_done(struct drbd_conf *mdev, struct drbd_request *req, const int rw)
{
	const unsigned long s = req->rq_state;

	list_del(&req->tl_requests);

	if (rw == WRITE) {
		if (!(s & RQ_NET_OK) || !(s & RQ_LOCAL_OK))
			drbd_set_out_of_sync(mdev, req->sector, req->size);

		if ((s & RQ_NET_OK) && (s & RQ_LOCAL_OK) && (s & RQ_NET_SIS))
			drbd_set_in_sync(mdev, req->sector, req->size);

		/* one might be tempted to move the drbd_al_complete_io
		 * to the local io completion callback drbd_endio_pri.
		 * but, if this was a mirror write, we may only
		 * drbd_al_complete_io after this is RQ_NET_DONE,
		 * otherwise the extent could be dropped from the al
		 * before it has actually been written on the peer.
		 * if we crash before our peer knows about the request,
		 * but after the extent has been dropped from the al,
		 * we would forget to resync the corresponding extent.
		 */
		if (s & RQ_LOCAL_MASK) {
			if (get_ldev_if_state(mdev, D_FAILED)) {
				if (s & RQ_IN_ACT_LOG)
					drbd_al_complete_io(mdev, req->sector);
				put_ldev(mdev);
			} else if (__ratelimit(&drbd_ratelimit_state)) {
				dev_warn(DEV, "Should have called drbd_al_complete_io(, %llu), "
				     "but my Disk seems to have failed :(\n",
				     (unsigned long long) req->sector);
			}
		}
	}

	drbd_req_free(req);
}

static void queue_barrier(struct drbd_conf *mdev)
{
	struct drbd_tl_epoch *b;

	if (test_bit(CREATE_BARRIER, &mdev->flags))
		return;

	b = mdev->newest_tle;
	b->w.cb = w_send_barrier;
	inc_ap_pending(mdev);
	drbd_queue_work(&mdev->data.work, &b->w);
	set_bit(CREATE_BARRIER, &mdev->flags);
}

static void _about_to_complete_local_write(struct drbd_conf *mdev,
	struct drbd_request *req)
{
	const unsigned long s = req->rq_state;
	struct drbd_request *i;
	struct drbd_epoch_entry *e;
	struct hlist_node *n;
	struct hlist_head *slot;

	if (mdev->state.conn >= C_CONNECTED &&
	    (s & RQ_NET_SENT) != 0 &&
	    req->epoch == mdev->newest_tle->br_number)
		queue_barrier(mdev);

	if ((s & RQ_NET_DONE) && mdev->ee_hash != NULL) {
		const sector_t sector = req->sector;
		const int size = req->size;

#define OVERLAPS overlaps(sector, size, i->sector, i->size)
		slot = tl_hash_slot(mdev, sector);
		hlist_for_each_entry(i, n, slot, collision) {
			if (OVERLAPS) {
				dev_alert(DEV, "LOGIC BUG: completed: %p %llus +%u; "
				      "other: %p %llus +%u\n",
				      req, (unsigned long long)sector, size,
				      i, (unsigned long long)i->sector, i->size);
			}
		}

#undef OVERLAPS
#define OVERLAPS overlaps(sector, size, e->sector, e->size)
		slot = ee_hash_slot(mdev, req->sector);
		hlist_for_each_entry(e, n, slot, collision) {
			if (OVERLAPS) {
				wake_up(&mdev->misc_wait);
				break;
			}
		}
	}
#undef OVERLAPS
}

void complete_master_bio(struct drbd_conf *mdev,
		struct bio_and_error *m)
{
	bio_endio(m->bio, m->error);
	dec_ap_bio(mdev);
}

void _req_may_be_done(struct drbd_request *req, struct bio_and_error *m)
{
	const unsigned long s = req->rq_state;
	struct drbd_conf *mdev = req->mdev;
	
	int rw = req->master_bio ? bio_data_dir(req->master_bio) : WRITE;

	if (s & RQ_NET_QUEUED)
		return;
	if (s & RQ_NET_PENDING)
		return;
	if (s & RQ_LOCAL_PENDING)
		return;

	if (req->master_bio) {

		int ok = (s & RQ_LOCAL_OK) || (s & RQ_NET_OK);
		int error = PTR_ERR(req->private_bio);

		if (!hlist_unhashed(&req->collision))
			hlist_del(&req->collision);
		else
			D_ASSERT((s & (RQ_NET_MASK & ~RQ_NET_DONE)) == 0);

		
		if (rw == WRITE)
			_about_to_complete_local_write(mdev, req);

		
		_drbd_end_io_acct(mdev, req);

		m->error = ok ? 0 : (error ?: -EIO);
		m->bio = req->master_bio;
		req->master_bio = NULL;
	}

	if ((s & RQ_NET_MASK) == 0 || (s & RQ_NET_DONE)) {
		_req_is_done(mdev, req, rw);
	}
}

static void _req_may_be_done_not_susp(struct drbd_request *req, struct bio_and_error *m)
{
	struct drbd_conf *mdev = req->mdev;

	if (!is_susp(mdev->state))
		_req_may_be_done(req, m);
}

static int _req_conflicts(struct drbd_request *req)
{
	struct drbd_conf *mdev = req->mdev;
	const sector_t sector = req->sector;
	const int size = req->size;
	struct drbd_request *i;
	struct drbd_epoch_entry *e;
	struct hlist_node *n;
	struct hlist_head *slot;

	D_ASSERT(hlist_unhashed(&req->collision));

	if (!get_net_conf(mdev))
		return 0;

	
	ERR_IF (mdev->tl_hash_s == 0)
		goto out_no_conflict;
	BUG_ON(mdev->tl_hash == NULL);

#define OVERLAPS overlaps(i->sector, i->size, sector, size)
	slot = tl_hash_slot(mdev, sector);
	hlist_for_each_entry(i, n, slot, collision) {
		if (OVERLAPS) {
			dev_alert(DEV, "%s[%u] Concurrent local write detected! "
			      "[DISCARD L] new: %llus +%u; "
			      "pending: %llus +%u\n",
			      current->comm, current->pid,
			      (unsigned long long)sector, size,
			      (unsigned long long)i->sector, i->size);
			goto out_conflict;
		}
	}

	if (mdev->ee_hash_s) {
		
		BUG_ON(mdev->ee_hash == NULL);
#undef OVERLAPS
#define OVERLAPS overlaps(e->sector, e->size, sector, size)
		slot = ee_hash_slot(mdev, sector);
		hlist_for_each_entry(e, n, slot, collision) {
			if (OVERLAPS) {
				dev_alert(DEV, "%s[%u] Concurrent remote write detected!"
				      " [DISCARD L] new: %llus +%u; "
				      "pending: %llus +%u\n",
				      current->comm, current->pid,
				      (unsigned long long)sector, size,
				      (unsigned long long)e->sector, e->size);
				goto out_conflict;
			}
		}
	}
#undef OVERLAPS

out_no_conflict:
	put_net_conf(mdev);
	return 0;

out_conflict:
	put_net_conf(mdev);
	return 1;
}

int __req_mod(struct drbd_request *req, enum drbd_req_event what,
		struct bio_and_error *m)
{
	struct drbd_conf *mdev = req->mdev;
	int rv = 0;
	m->bio = NULL;

	switch (what) {
	default:
		dev_err(DEV, "LOGIC BUG in %s:%u\n", __FILE__ , __LINE__);
		break;


	case to_be_send: 
		D_ASSERT(!(req->rq_state & RQ_NET_MASK));
		req->rq_state |= RQ_NET_PENDING;
		inc_ap_pending(mdev);
		break;

	case to_be_submitted: 
		
		D_ASSERT(!(req->rq_state & RQ_LOCAL_MASK));
		req->rq_state |= RQ_LOCAL_PENDING;
		break;

	case completed_ok:
		if (bio_data_dir(req->master_bio) == WRITE)
			mdev->writ_cnt += req->size>>9;
		else
			mdev->read_cnt += req->size>>9;

		req->rq_state |= (RQ_LOCAL_COMPLETED|RQ_LOCAL_OK);
		req->rq_state &= ~RQ_LOCAL_PENDING;

		_req_may_be_done_not_susp(req, m);
		put_ldev(mdev);
		break;

	case write_completed_with_error:
		req->rq_state |= RQ_LOCAL_COMPLETED;
		req->rq_state &= ~RQ_LOCAL_PENDING;

		__drbd_chk_io_error(mdev, false);
		_req_may_be_done_not_susp(req, m);
		put_ldev(mdev);
		break;

	case read_ahead_completed_with_error:
		
		req->rq_state |= RQ_LOCAL_COMPLETED;
		req->rq_state &= ~RQ_LOCAL_PENDING;
		_req_may_be_done_not_susp(req, m);
		put_ldev(mdev);
		break;

	case read_completed_with_error:
		drbd_set_out_of_sync(mdev, req->sector, req->size);

		req->rq_state |= RQ_LOCAL_COMPLETED;
		req->rq_state &= ~RQ_LOCAL_PENDING;

		D_ASSERT(!(req->rq_state & RQ_NET_MASK));

		__drbd_chk_io_error(mdev, false);
		put_ldev(mdev);

		if (mdev->state.pdsk != D_UP_TO_DATE) {
			_req_may_be_done_not_susp(req, m);
			break;
		}

		
		req->rq_state |= RQ_NET_PENDING;
		inc_ap_pending(mdev);
		

	case queue_for_net_read:

		hlist_add_head(&req->collision, ar_hash_slot(mdev, req->sector));

		set_bit(UNPLUG_REMOTE, &mdev->flags);

		D_ASSERT(req->rq_state & RQ_NET_PENDING);
		req->rq_state |= RQ_NET_QUEUED;
		req->w.cb = (req->rq_state & RQ_LOCAL_MASK)
			? w_read_retry_remote
			: w_send_read_req;
		drbd_queue_work(&mdev->data.work, &req->w);
		break;

	case queue_for_net_write:
		
		

		hlist_add_head(&req->collision, tl_hash_slot(mdev, req->sector));
		


		set_bit(UNPLUG_REMOTE, &mdev->flags);

		D_ASSERT(test_bit(CREATE_BARRIER, &mdev->flags) == 0);

		req->epoch = mdev->newest_tle->br_number;

		
		mdev->newest_tle->n_writes++;

		
		D_ASSERT(req->rq_state & RQ_NET_PENDING);
		req->rq_state |= RQ_NET_QUEUED;
		req->w.cb =  w_send_dblock;
		drbd_queue_work(&mdev->data.work, &req->w);

		
		if (mdev->newest_tle->n_writes >= mdev->net_conf->max_epoch_size)
			queue_barrier(mdev);

		break;

	case queue_for_send_oos:
		req->rq_state |= RQ_NET_QUEUED;
		req->w.cb =  w_send_oos;
		drbd_queue_work(&mdev->data.work, &req->w);
		break;

	case oos_handed_to_network:
		
	case send_canceled:
		
	case send_failed:
		req->rq_state &= ~RQ_NET_QUEUED;
		_req_may_be_done_not_susp(req, m);
		break;

	case handed_over_to_network:
		
		if (bio_data_dir(req->master_bio) == WRITE)
			atomic_add(req->size>>9, &mdev->ap_in_flight);

		if (bio_data_dir(req->master_bio) == WRITE &&
		    mdev->net_conf->wire_protocol == DRBD_PROT_A) {
			/* this is what is dangerous about protocol A:
			 * pretend it was successfully written on the peer. */
			if (req->rq_state & RQ_NET_PENDING) {
				dec_ap_pending(mdev);
				req->rq_state &= ~RQ_NET_PENDING;
				req->rq_state |= RQ_NET_OK;
			} 
		}
		req->rq_state &= ~RQ_NET_QUEUED;
		req->rq_state |= RQ_NET_SENT;
		_req_may_be_done_not_susp(req, m);
		break;

	case read_retry_remote_canceled:
		req->rq_state &= ~RQ_NET_QUEUED;
		
	case connection_lost_while_pending:
		
		
		if (req->rq_state & RQ_NET_PENDING)
			dec_ap_pending(mdev);
		req->rq_state &= ~(RQ_NET_OK|RQ_NET_PENDING);
		req->rq_state |= RQ_NET_DONE;
		if (req->rq_state & RQ_NET_SENT && req->rq_state & RQ_WRITE)
			atomic_sub(req->size>>9, &mdev->ap_in_flight);

		if (!(req->rq_state & RQ_NET_QUEUED))
			_req_may_be_done(req, m); 
		break;

	case write_acked_by_peer_and_sis:
		req->rq_state |= RQ_NET_SIS;
	case conflict_discarded_by_peer:
		if (what == conflict_discarded_by_peer)
			dev_alert(DEV, "Got DiscardAck packet %llus +%u!"
			      " DRBD is not a random data generator!\n",
			      (unsigned long long)req->sector, req->size);
		req->rq_state |= RQ_NET_DONE;
		
	case write_acked_by_peer:
		/* protocol C; successfully written on peer.
		 * Nothing to do here.
		 * We want to keep the tl in place for all protocols, to cater
		 * for volatile write-back caches on lower level devices.
		 *
		 * A barrier request is expected to have forced all prior
		 * requests onto stable storage, so completion of a barrier
		 * request could set NET_DONE right here, and not wait for the
		 * P_BARRIER_ACK, but that is an unnecessary optimization. */

		
	case recv_acked_by_peer:
		/* protocol B; pretends to be successfully written on peer.
		 * see also notes above in handed_over_to_network about
		 * protocol != C */
		req->rq_state |= RQ_NET_OK;
		D_ASSERT(req->rq_state & RQ_NET_PENDING);
		dec_ap_pending(mdev);
		atomic_sub(req->size>>9, &mdev->ap_in_flight);
		req->rq_state &= ~RQ_NET_PENDING;
		_req_may_be_done_not_susp(req, m);
		break;

	case neg_acked:
		
		if (req->rq_state & RQ_NET_PENDING) {
			dec_ap_pending(mdev);
			atomic_sub(req->size>>9, &mdev->ap_in_flight);
		}
		req->rq_state &= ~(RQ_NET_OK|RQ_NET_PENDING);

		req->rq_state |= RQ_NET_DONE;
		_req_may_be_done_not_susp(req, m);
		
		break;

	case fail_frozen_disk_io:
		if (!(req->rq_state & RQ_LOCAL_COMPLETED))
			break;

		_req_may_be_done(req, m); 
		break;

	case restart_frozen_disk_io:
		if (!(req->rq_state & RQ_LOCAL_COMPLETED))
			break;

		req->rq_state &= ~RQ_LOCAL_COMPLETED;

		rv = MR_READ;
		if (bio_data_dir(req->master_bio) == WRITE)
			rv = MR_WRITE;

		get_ldev(mdev);
		req->w.cb = w_restart_disk_io;
		drbd_queue_work(&mdev->data.work, &req->w);
		break;

	case resend:
		if (!(req->rq_state & RQ_NET_OK)) {
			if (req->w.cb) {
				drbd_queue_work(&mdev->data.work, &req->w);
				rv = req->rq_state & RQ_WRITE ? MR_WRITE : MR_READ;
			}
			break;
		}
		

	case barrier_acked:
		if (!(req->rq_state & RQ_WRITE))
			break;

		if (req->rq_state & RQ_NET_PENDING) {
			dev_err(DEV, "FIXME (barrier_acked but pending)\n");
			list_move(&req->tl_requests, &mdev->out_of_sequence_requests);
		}
		if ((req->rq_state & RQ_NET_MASK) != 0) {
			req->rq_state |= RQ_NET_DONE;
			if (mdev->net_conf->wire_protocol == DRBD_PROT_A)
				atomic_sub(req->size>>9, &mdev->ap_in_flight);
		}
		_req_may_be_done(req, m); 
		break;

	case data_received:
		D_ASSERT(req->rq_state & RQ_NET_PENDING);
		dec_ap_pending(mdev);
		req->rq_state &= ~RQ_NET_PENDING;
		req->rq_state |= (RQ_NET_OK|RQ_NET_DONE);
		_req_may_be_done_not_susp(req, m);
		break;
	};

	return rv;
}

static int drbd_may_do_local_read(struct drbd_conf *mdev, sector_t sector, int size)
{
	unsigned long sbnr, ebnr;
	sector_t esector, nr_sectors;

	if (mdev->state.disk == D_UP_TO_DATE)
		return 1;
	if (mdev->state.disk >= D_OUTDATED)
		return 0;
	if (mdev->state.disk <  D_INCONSISTENT)
		return 0;
	
	nr_sectors = drbd_get_capacity(mdev->this_bdev);
	esector = sector + (size >> 9) - 1;

	D_ASSERT(sector  < nr_sectors);
	D_ASSERT(esector < nr_sectors);

	sbnr = BM_SECT_TO_BIT(sector);
	ebnr = BM_SECT_TO_BIT(esector);

	return 0 == drbd_bm_count_bits(mdev, sbnr, ebnr);
}

static int drbd_make_request_common(struct drbd_conf *mdev, struct bio *bio, unsigned long start_time)
{
	const int rw = bio_rw(bio);
	const int size = bio->bi_size;
	const sector_t sector = bio->bi_sector;
	struct drbd_tl_epoch *b = NULL;
	struct drbd_request *req;
	int local, remote, send_oos = 0;
	int err = -EIO;
	int ret = 0;

	
	req = drbd_req_new(mdev, bio);
	if (!req) {
		dec_ap_bio(mdev);
		dev_err(DEV, "could not kmalloc() req\n");
		bio_endio(bio, -ENOMEM);
		return 0;
	}
	req->start_time = start_time;

	local = get_ldev(mdev);
	if (!local) {
		bio_put(req->private_bio); 
		req->private_bio = NULL;
	}
	if (rw == WRITE) {
		remote = 1;
	} else {
		
		if (local) {
			if (!drbd_may_do_local_read(mdev, sector, size)) {
				local = 0;
				bio_put(req->private_bio);
				req->private_bio = NULL;
				put_ldev(mdev);
			}
		}
		remote = !local && mdev->state.pdsk >= D_UP_TO_DATE;
	}

	if (rw == READA && mdev->state.disk >= D_INCONSISTENT && !local) {
		err = -EWOULDBLOCK;
		goto fail_and_free_req;
	}

	if (rw == WRITE && local && !test_bit(AL_SUSPENDED, &mdev->flags)) {
		req->rq_state |= RQ_IN_ACT_LOG;
		drbd_al_begin_io(mdev, sector);
	}

	remote = remote && drbd_should_do_remote(mdev->state);
	send_oos = rw == WRITE && drbd_should_send_oos(mdev->state);
	D_ASSERT(!(remote && send_oos));

	if (!(local || remote) && !is_susp(mdev->state)) {
		if (__ratelimit(&drbd_ratelimit_state))
			dev_err(DEV, "IO ERROR: neither local nor remote disk\n");
		goto fail_free_complete;
	}

	if (rw == WRITE && (remote || send_oos) &&
	    mdev->unused_spare_tle == NULL &&
	    test_bit(CREATE_BARRIER, &mdev->flags)) {
allocate_barrier:
		b = kmalloc(sizeof(struct drbd_tl_epoch), GFP_NOIO);
		if (!b) {
			dev_err(DEV, "Failed to alloc barrier.\n");
			err = -ENOMEM;
			goto fail_free_complete;
		}
	}

	
	spin_lock_irq(&mdev->req_lock);

	if (is_susp(mdev->state)) {
		ret = 1;
		spin_unlock_irq(&mdev->req_lock);
		goto fail_free_complete;
	}

	if (remote || send_oos) {
		remote = drbd_should_do_remote(mdev->state);
		send_oos = rw == WRITE && drbd_should_send_oos(mdev->state);
		D_ASSERT(!(remote && send_oos));

		if (!(remote || send_oos))
			dev_warn(DEV, "lost connection while grabbing the req_lock!\n");
		if (!(local || remote)) {
			dev_err(DEV, "IO ERROR: neither local nor remote disk\n");
			spin_unlock_irq(&mdev->req_lock);
			goto fail_free_complete;
		}
	}

	if (b && mdev->unused_spare_tle == NULL) {
		mdev->unused_spare_tle = b;
		b = NULL;
	}
	if (rw == WRITE && (remote || send_oos) &&
	    mdev->unused_spare_tle == NULL &&
	    test_bit(CREATE_BARRIER, &mdev->flags)) {
		spin_unlock_irq(&mdev->req_lock);
		goto allocate_barrier;
	}


	
	_drbd_start_io_acct(mdev, req, bio);

	if ((remote || send_oos) && mdev->unused_spare_tle &&
	    test_and_clear_bit(CREATE_BARRIER, &mdev->flags)) {
		_tl_add_barrier(mdev, mdev->unused_spare_tle);
		mdev->unused_spare_tle = NULL;
	} else {
		D_ASSERT(!(remote && rw == WRITE &&
			   test_bit(CREATE_BARRIER, &mdev->flags)));
	}


	if (remote)
		_req_mod(req, to_be_send);
	if (local)
		_req_mod(req, to_be_submitted);

	if (rw == WRITE && _req_conflicts(req))
		goto fail_conflicting;

	list_add_tail(&req->tl_requests, &mdev->newest_tle->requests);

	if (remote) {
		_req_mod(req, (rw == WRITE)
				? queue_for_net_write
				: queue_for_net_read);
	}
	if (send_oos && drbd_set_out_of_sync(mdev, sector, size))
		_req_mod(req, queue_for_send_oos);

	if (remote &&
	    mdev->net_conf->on_congestion != OC_BLOCK && mdev->agreed_pro_version >= 96) {
		int congested = 0;

		if (mdev->net_conf->cong_fill &&
		    atomic_read(&mdev->ap_in_flight) >= mdev->net_conf->cong_fill) {
			dev_info(DEV, "Congestion-fill threshold reached\n");
			congested = 1;
		}

		if (mdev->act_log->used >= mdev->net_conf->cong_extents) {
			dev_info(DEV, "Congestion-extents threshold reached\n");
			congested = 1;
		}

		if (congested) {
			queue_barrier(mdev); 

			if (mdev->net_conf->on_congestion == OC_PULL_AHEAD)
				_drbd_set_state(_NS(mdev, conn, C_AHEAD), 0, NULL);
			else  
				_drbd_set_state(_NS(mdev, conn, C_DISCONNECTING), 0, NULL);
		}
	}

	spin_unlock_irq(&mdev->req_lock);
	kfree(b); 

	if (local) {
		req->private_bio->bi_bdev = mdev->ldev->backing_bdev;

		if (get_ldev(mdev)) {
			if (drbd_insert_fault(mdev,   rw == WRITE ? DRBD_FAULT_DT_WR
						    : rw == READ  ? DRBD_FAULT_DT_RD
						    :               DRBD_FAULT_DT_RA))
				bio_endio(req->private_bio, -EIO);
			else
				generic_make_request(req->private_bio);
			put_ldev(mdev);
		} else
			bio_endio(req->private_bio, -EIO);
	}

	return 0;

fail_conflicting:
	_drbd_end_io_acct(mdev, req);
	spin_unlock_irq(&mdev->req_lock);
	if (remote)
		dec_ap_pending(mdev);
	err = 0;

fail_free_complete:
	if (req->rq_state & RQ_IN_ACT_LOG)
		drbd_al_complete_io(mdev, sector);
fail_and_free_req:
	if (local) {
		bio_put(req->private_bio);
		req->private_bio = NULL;
		put_ldev(mdev);
	}
	if (!ret)
		bio_endio(bio, err);

	drbd_req_free(req);
	dec_ap_bio(mdev);
	kfree(b);

	return ret;
}

static int drbd_fail_request_early(struct drbd_conf *mdev, int is_write)
{
	if (mdev->state.role != R_PRIMARY &&
		(!allow_oos || is_write)) {
		if (__ratelimit(&drbd_ratelimit_state)) {
			dev_err(DEV, "Process %s[%u] tried to %s; "
			    "since we are not in Primary state, "
			    "we cannot allow this\n",
			    current->comm, current->pid,
			    is_write ? "WRITE" : "READ");
		}
		return 1;
	}

	return 0;
}

void drbd_make_request(struct request_queue *q, struct bio *bio)
{
	unsigned int s_enr, e_enr;
	struct drbd_conf *mdev = (struct drbd_conf *) q->queuedata;
	unsigned long start_time;

	if (drbd_fail_request_early(mdev, bio_data_dir(bio) & WRITE)) {
		bio_endio(bio, -EPERM);
		return;
	}

	start_time = jiffies;

	D_ASSERT(bio->bi_size > 0);
	D_ASSERT((bio->bi_size & 0x1ff) == 0);
	D_ASSERT(bio->bi_idx == 0);

	s_enr = bio->bi_sector >> HT_SHIFT;
	e_enr = (bio->bi_sector+(bio->bi_size>>9)-1) >> HT_SHIFT;

	if (likely(s_enr == e_enr)) {
		inc_ap_bio(mdev, 1);
		drbd_make_request_common(mdev, bio, start_time);
		return;
	}

	if (bio->bi_vcnt != 1 || bio->bi_idx != 0 || bio->bi_size > DRBD_MAX_BIO_SIZE) {
		
		dev_err(DEV, "bio would need to, but cannot, be split: "
		    "(vcnt=%u,idx=%u,size=%u,sector=%llu)\n",
		    bio->bi_vcnt, bio->bi_idx, bio->bi_size,
		    (unsigned long long)bio->bi_sector);
		bio_endio(bio, -EINVAL);
	} else {
		
		struct bio_pair *bp;
		const sector_t sect = bio->bi_sector;
		const int sps = 1 << HT_SHIFT; 
		const int mask = sps - 1;
		const sector_t first_sectors = sps - (sect & mask);
		bp = bio_split(bio, first_sectors);

		inc_ap_bio(mdev, 3);

		D_ASSERT(e_enr == s_enr + 1);

		while (drbd_make_request_common(mdev, &bp->bio1, start_time))
			inc_ap_bio(mdev, 1);

		while (drbd_make_request_common(mdev, &bp->bio2, start_time))
			inc_ap_bio(mdev, 1);

		dec_ap_bio(mdev);

		bio_pair_release(bp);
	}
}

int drbd_merge_bvec(struct request_queue *q, struct bvec_merge_data *bvm, struct bio_vec *bvec)
{
	struct drbd_conf *mdev = (struct drbd_conf *) q->queuedata;
	unsigned int bio_offset =
		(unsigned int)bvm->bi_sector << 9; 
	unsigned int bio_size = bvm->bi_size;
	int limit, backing_limit;

	limit = DRBD_MAX_BIO_SIZE
	      - ((bio_offset & (DRBD_MAX_BIO_SIZE-1)) + bio_size);
	if (limit < 0)
		limit = 0;
	if (bio_size == 0) {
		if (limit <= bvec->bv_len)
			limit = bvec->bv_len;
	} else if (limit && get_ldev(mdev)) {
		struct request_queue * const b =
			mdev->ldev->backing_bdev->bd_disk->queue;
		if (b->merge_bvec_fn) {
			backing_limit = b->merge_bvec_fn(b, bvm, bvec);
			limit = min(limit, backing_limit);
		}
		put_ldev(mdev);
	}
	return limit;
}

void request_timer_fn(unsigned long data)
{
	struct drbd_conf *mdev = (struct drbd_conf *) data;
	struct drbd_request *req; 
	struct list_head *le;
	unsigned long et = 0; 

	if (get_net_conf(mdev)) {
		et = mdev->net_conf->timeout*HZ/10 * mdev->net_conf->ko_count;
		put_net_conf(mdev);
	}
	if (!et || mdev->state.conn < C_WF_REPORT_PARAMS)
		return; 

	spin_lock_irq(&mdev->req_lock);
	le = &mdev->oldest_tle->requests;
	if (list_empty(le)) {
		spin_unlock_irq(&mdev->req_lock);
		mod_timer(&mdev->request_timer, jiffies + et);
		return;
	}

	le = le->prev;
	req = list_entry(le, struct drbd_request, tl_requests);
	if (time_is_before_eq_jiffies(req->start_time + et)) {
		if (req->rq_state & RQ_NET_PENDING) {
			dev_warn(DEV, "Remote failed to finish a request within ko-count * timeout\n");
			_drbd_set_state(_NS(mdev, conn, C_TIMEOUT), CS_VERBOSE, NULL);
		} else {
			dev_warn(DEV, "Local backing block device frozen?\n");
			mod_timer(&mdev->request_timer, jiffies + et);
		}
	} else {
		mod_timer(&mdev->request_timer, req->start_time + et);
	}

	spin_unlock_irq(&mdev->req_lock);
}
