/*
 * @ubi: UBI device description object
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём), Thomas Gleixner
 */


#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include "ubi.h"

#define WL_RESERVED_PEBS 1

#define UBI_WL_THRESHOLD CONFIG_MTD_UBI_WL_THRESHOLD

/*
 * When a physical eraseblock is moved, the WL sub-system has to pick the target
 * physical eraseblock to move to. The simplest way would be just to pick the
 * one with the highest erase counter. But in certain workloads this could lead
 * to an unlimited wear of one or few physical eraseblock. Indeed, imagine a
 * situation when the picked physical eraseblock is constantly erased after the
 * data is written to it. So, we have a constant which limits the highest erase
 * counter of the free physical eraseblock to pick. Namely, the WL sub-system
 * does not pick eraseblocks with erase counter greater than the lowest erase
 * counter plus %WL_FREE_MAX_DIFF.
 */
#define WL_FREE_MAX_DIFF (2*UBI_WL_THRESHOLD)

#define WL_MAX_FAILURES 32

struct ubi_work {
	struct list_head list;
	int (*func)(struct ubi_device *ubi, struct ubi_work *wrk, int cancel);
	
	struct ubi_wl_entry *e;
	int torture;
};

#ifdef CONFIG_MTD_UBI_DEBUG
static int paranoid_check_ec(struct ubi_device *ubi, int pnum, int ec);
static int paranoid_check_in_wl_tree(const struct ubi_device *ubi,
				     struct ubi_wl_entry *e,
				     struct rb_root *root);
static int paranoid_check_in_pq(const struct ubi_device *ubi,
				struct ubi_wl_entry *e);
#else
#define paranoid_check_ec(ubi, pnum, ec) 0
#define paranoid_check_in_wl_tree(ubi, e, root)
#define paranoid_check_in_pq(ubi, e) 0
#endif

static void wl_tree_add(struct ubi_wl_entry *e, struct rb_root *root)
{
	struct rb_node **p, *parent = NULL;

	p = &root->rb_node;
	while (*p) {
		struct ubi_wl_entry *e1;

		parent = *p;
		e1 = rb_entry(parent, struct ubi_wl_entry, u.rb);

		if (e->ec < e1->ec)
			p = &(*p)->rb_left;
		else if (e->ec > e1->ec)
			p = &(*p)->rb_right;
		else {
			ubi_assert(e->pnum != e1->pnum);
			if (e->pnum < e1->pnum)
				p = &(*p)->rb_left;
			else
				p = &(*p)->rb_right;
		}
	}

	rb_link_node(&e->u.rb, parent, p);
	rb_insert_color(&e->u.rb, root);
}

static int do_work(struct ubi_device *ubi)
{
	int err;
	struct ubi_work *wrk;

	cond_resched();

	down_read(&ubi->work_sem);
	spin_lock(&ubi->wl_lock);
	if (list_empty(&ubi->works)) {
		spin_unlock(&ubi->wl_lock);
		up_read(&ubi->work_sem);
		return 0;
	}

	wrk = list_entry(ubi->works.next, struct ubi_work, list);
	list_del(&wrk->list);
	ubi->works_count -= 1;
	ubi_assert(ubi->works_count >= 0);
	spin_unlock(&ubi->wl_lock);

	err = wrk->func(ubi, wrk, 0);
	if (err)
		ubi_err("work failed with error code %d", err);
	up_read(&ubi->work_sem);

	return err;
}

static int produce_free_peb(struct ubi_device *ubi)
{
	int err;

	spin_lock(&ubi->wl_lock);
	while (!ubi->free.rb_node) {
		spin_unlock(&ubi->wl_lock);

		dbg_wl("do one work synchronously");
		err = do_work(ubi);
		if (err)
			return err;

		spin_lock(&ubi->wl_lock);
	}
	spin_unlock(&ubi->wl_lock);

	return 0;
}

static int in_wl_tree(struct ubi_wl_entry *e, struct rb_root *root)
{
	struct rb_node *p;

	p = root->rb_node;
	while (p) {
		struct ubi_wl_entry *e1;

		e1 = rb_entry(p, struct ubi_wl_entry, u.rb);

		if (e->pnum == e1->pnum) {
			ubi_assert(e == e1);
			return 1;
		}

		if (e->ec < e1->ec)
			p = p->rb_left;
		else if (e->ec > e1->ec)
			p = p->rb_right;
		else {
			ubi_assert(e->pnum != e1->pnum);
			if (e->pnum < e1->pnum)
				p = p->rb_left;
			else
				p = p->rb_right;
		}
	}

	return 0;
}

static void prot_queue_add(struct ubi_device *ubi, struct ubi_wl_entry *e)
{
	int pq_tail = ubi->pq_head - 1;

	if (pq_tail < 0)
		pq_tail = UBI_PROT_QUEUE_LEN - 1;
	ubi_assert(pq_tail >= 0 && pq_tail < UBI_PROT_QUEUE_LEN);
	list_add_tail(&e->u.list, &ubi->pq[pq_tail]);
	dbg_wl("added PEB %d EC %d to the protection queue", e->pnum, e->ec);
}

static struct ubi_wl_entry *find_wl_entry(struct rb_root *root, int diff)
{
	struct rb_node *p;
	struct ubi_wl_entry *e;
	int max;

	e = rb_entry(rb_first(root), struct ubi_wl_entry, u.rb);
	max = e->ec + diff;

	p = root->rb_node;
	while (p) {
		struct ubi_wl_entry *e1;

		e1 = rb_entry(p, struct ubi_wl_entry, u.rb);
		if (e1->ec >= max)
			p = p->rb_left;
		else {
			p = p->rb_right;
			e = e1;
		}
	}

	return e;
}

int ubi_wl_get_peb(struct ubi_device *ubi, int dtype)
{
	int err;
	struct ubi_wl_entry *e, *first, *last;

	ubi_assert(dtype == UBI_LONGTERM || dtype == UBI_SHORTTERM ||
		   dtype == UBI_UNKNOWN);

retry:
	spin_lock(&ubi->wl_lock);
	if (!ubi->free.rb_node) {
		if (ubi->works_count == 0) {
			ubi_assert(list_empty(&ubi->works));
			ubi_err("no free eraseblocks");
			spin_unlock(&ubi->wl_lock);
			return -ENOSPC;
		}
		spin_unlock(&ubi->wl_lock);

		err = produce_free_peb(ubi);
		if (err < 0)
			return err;
		goto retry;
	}

	switch (dtype) {
	case UBI_LONGTERM:
		e = find_wl_entry(&ubi->free, WL_FREE_MAX_DIFF);
		break;
	case UBI_UNKNOWN:
		first = rb_entry(rb_first(&ubi->free), struct ubi_wl_entry,
					u.rb);
		last = rb_entry(rb_last(&ubi->free), struct ubi_wl_entry, u.rb);

		if (last->ec - first->ec < WL_FREE_MAX_DIFF)
			e = rb_entry(ubi->free.rb_node,
					struct ubi_wl_entry, u.rb);
		else
			e = find_wl_entry(&ubi->free, WL_FREE_MAX_DIFF/2);
		break;
	case UBI_SHORTTERM:
		e = rb_entry(rb_first(&ubi->free), struct ubi_wl_entry, u.rb);
		break;
	default:
		BUG();
	}

	paranoid_check_in_wl_tree(ubi, e, &ubi->free);

	rb_erase(&e->u.rb, &ubi->free);
	dbg_wl("PEB %d EC %d", e->pnum, e->ec);
	prot_queue_add(ubi, e);
	spin_unlock(&ubi->wl_lock);

	err = ubi_dbg_check_all_ff(ubi, e->pnum, ubi->vid_hdr_aloffset,
				   ubi->peb_size - ubi->vid_hdr_aloffset);
	if (err) {
		ubi_err("new PEB %d does not contain all 0xFF bytes", e->pnum);
		return err;
	}

	return e->pnum;
}

static int prot_queue_del(struct ubi_device *ubi, int pnum)
{
	struct ubi_wl_entry *e;

	e = ubi->lookuptbl[pnum];
	if (!e)
		return -ENODEV;

	if (paranoid_check_in_pq(ubi, e))
		return -ENODEV;

	list_del(&e->u.list);
	dbg_wl("deleted PEB %d from the protection queue", e->pnum);
	return 0;
}

static int sync_erase(struct ubi_device *ubi, struct ubi_wl_entry *e,
		      int torture)
{
	int err;
	struct ubi_ec_hdr *ec_hdr;
	unsigned long long ec = e->ec;

	dbg_wl("erase PEB %d, old EC %llu", e->pnum, ec);

	err = paranoid_check_ec(ubi, e->pnum, e->ec);
	if (err)
		return -EINVAL;

	ec_hdr = kzalloc(ubi->ec_hdr_alsize, GFP_NOFS);
	if (!ec_hdr)
		return -ENOMEM;

	err = ubi_io_sync_erase(ubi, e->pnum, torture);
	if (err < 0)
		goto out_free;

	ec += err;
	if (ec > UBI_MAX_ERASECOUNTER) {
		ubi_err("erase counter overflow at PEB %d, EC %llu",
			e->pnum, ec);
		err = -EINVAL;
		goto out_free;
	}

	dbg_wl("erased PEB %d, new EC %llu", e->pnum, ec);

	ec_hdr->ec = cpu_to_be64(ec);

	err = ubi_io_write_ec_hdr(ubi, e->pnum, ec_hdr);
	if (err)
		goto out_free;

	e->ec = ec;
	spin_lock(&ubi->wl_lock);
	if (e->ec > ubi->max_ec)
		ubi->max_ec = e->ec;
	spin_unlock(&ubi->wl_lock);

out_free:
	kfree(ec_hdr);
	return err;
}

static void serve_prot_queue(struct ubi_device *ubi)
{
	struct ubi_wl_entry *e, *tmp;
	int count;

repeat:
	count = 0;
	spin_lock(&ubi->wl_lock);
	list_for_each_entry_safe(e, tmp, &ubi->pq[ubi->pq_head], u.list) {
		dbg_wl("PEB %d EC %d protection over, move to used tree",
			e->pnum, e->ec);

		list_del(&e->u.list);
		wl_tree_add(e, &ubi->used);
		if (count++ > 32) {
			spin_unlock(&ubi->wl_lock);
			cond_resched();
			goto repeat;
		}
	}

	ubi->pq_head += 1;
	if (ubi->pq_head == UBI_PROT_QUEUE_LEN)
		ubi->pq_head = 0;
	ubi_assert(ubi->pq_head >= 0 && ubi->pq_head < UBI_PROT_QUEUE_LEN);
	spin_unlock(&ubi->wl_lock);
}

static void schedule_ubi_work(struct ubi_device *ubi, struct ubi_work *wrk)
{
	spin_lock(&ubi->wl_lock);
	list_add_tail(&wrk->list, &ubi->works);
	ubi_assert(ubi->works_count >= 0);
	ubi->works_count += 1;
	if (ubi->thread_enabled && !ubi_dbg_is_bgt_disabled(ubi))
		wake_up_process(ubi->bgt_thread);
	spin_unlock(&ubi->wl_lock);
}

static int erase_worker(struct ubi_device *ubi, struct ubi_work *wl_wrk,
			int cancel);

static int schedule_erase(struct ubi_device *ubi, struct ubi_wl_entry *e,
			  int torture)
{
	struct ubi_work *wl_wrk;

	dbg_wl("schedule erasure of PEB %d, EC %d, torture %d",
	       e->pnum, e->ec, torture);

	wl_wrk = kmalloc(sizeof(struct ubi_work), GFP_NOFS);
	if (!wl_wrk)
		return -ENOMEM;

	wl_wrk->func = &erase_worker;
	wl_wrk->e = e;
	wl_wrk->torture = torture;

	schedule_ubi_work(ubi, wl_wrk);
	return 0;
}

static int wear_leveling_worker(struct ubi_device *ubi, struct ubi_work *wrk,
				int cancel)
{
	int err, scrubbing = 0, torture = 0, protect = 0, erroneous = 0;
	int vol_id = -1, uninitialized_var(lnum);
	struct ubi_wl_entry *e1, *e2;
	struct ubi_vid_hdr *vid_hdr;

	kfree(wrk);
	if (cancel)
		return 0;

	vid_hdr = ubi_zalloc_vid_hdr(ubi, GFP_NOFS);
	if (!vid_hdr)
		return -ENOMEM;

	mutex_lock(&ubi->move_mutex);
	spin_lock(&ubi->wl_lock);
	ubi_assert(!ubi->move_from && !ubi->move_to);
	ubi_assert(!ubi->move_to_put);

	if (!ubi->free.rb_node ||
	    (!ubi->used.rb_node && !ubi->scrub.rb_node)) {
		dbg_wl("cancel WL, a list is empty: free %d, used %d",
		       !ubi->free.rb_node, !ubi->used.rb_node);
		goto out_cancel;
	}

	if (!ubi->scrub.rb_node) {
		e1 = rb_entry(rb_first(&ubi->used), struct ubi_wl_entry, u.rb);
		e2 = find_wl_entry(&ubi->free, WL_FREE_MAX_DIFF);

		if (!(e2->ec - e1->ec >= UBI_WL_THRESHOLD)) {
			dbg_wl("no WL needed: min used EC %d, max free EC %d",
			       e1->ec, e2->ec);
			goto out_cancel;
		}
		paranoid_check_in_wl_tree(ubi, e1, &ubi->used);
		rb_erase(&e1->u.rb, &ubi->used);
		dbg_wl("move PEB %d EC %d to PEB %d EC %d",
		       e1->pnum, e1->ec, e2->pnum, e2->ec);
	} else {
		
		scrubbing = 1;
		e1 = rb_entry(rb_first(&ubi->scrub), struct ubi_wl_entry, u.rb);
		e2 = find_wl_entry(&ubi->free, WL_FREE_MAX_DIFF);
		paranoid_check_in_wl_tree(ubi, e1, &ubi->scrub);
		rb_erase(&e1->u.rb, &ubi->scrub);
		dbg_wl("scrub PEB %d to PEB %d", e1->pnum, e2->pnum);
	}

	paranoid_check_in_wl_tree(ubi, e2, &ubi->free);
	rb_erase(&e2->u.rb, &ubi->free);
	ubi->move_from = e1;
	ubi->move_to = e2;
	spin_unlock(&ubi->wl_lock);


	err = ubi_io_read_vid_hdr(ubi, e1->pnum, vid_hdr, 0);
	if (err && err != UBI_IO_BITFLIPS) {
		if (err == UBI_IO_FF) {
			/*
			 * We are trying to move PEB without a VID header. UBI
			 * always write VID headers shortly after the PEB was
			 * given, so we have a situation when it has not yet
			 * had a chance to write it, because it was preempted.
			 * So add this PEB to the protection queue so far,
			 * because presumably more data will be written there
			 * (including the missing VID header), and then we'll
			 * move it.
			 */
			dbg_wl("PEB %d has no VID header", e1->pnum);
			protect = 1;
			goto out_not_moved;
		} else if (err == UBI_IO_FF_BITFLIPS) {
			dbg_wl("PEB %d has no VID header but has bit-flips",
			       e1->pnum);
			scrubbing = 1;
			goto out_not_moved;
		}

		ubi_err("error %d while reading VID header from PEB %d",
			err, e1->pnum);
		goto out_error;
	}

	vol_id = be32_to_cpu(vid_hdr->vol_id);
	lnum = be32_to_cpu(vid_hdr->lnum);

	err = ubi_eba_copy_leb(ubi, e1->pnum, e2->pnum, vid_hdr);
	if (err) {
		if (err == MOVE_CANCEL_RACE) {
			protect = 1;
			goto out_not_moved;
		}
		if (err == MOVE_RETRY) {
			scrubbing = 1;
			goto out_not_moved;
		}
		if (err == MOVE_TARGET_BITFLIPS || err == MOVE_TARGET_WR_ERR ||
		    err == MOVE_TARGET_RD_ERR) {
			torture = 1;
			goto out_not_moved;
		}

		if (err == MOVE_SOURCE_RD_ERR) {
			if (ubi->erroneous_peb_count > ubi->max_erroneous) {
				ubi_err("too many erroneous eraseblocks (%d)",
					ubi->erroneous_peb_count);
				goto out_error;
			}
			erroneous = 1;
			goto out_not_moved;
		}

		if (err < 0)
			goto out_error;

		ubi_assert(0);
	}

	
	if (scrubbing)
		ubi_msg("scrubbed PEB %d (LEB %d:%d), data moved to PEB %d",
			e1->pnum, vol_id, lnum, e2->pnum);
	ubi_free_vid_hdr(ubi, vid_hdr);

	spin_lock(&ubi->wl_lock);
	if (!ubi->move_to_put) {
		wl_tree_add(e2, &ubi->used);
		e2 = NULL;
	}
	ubi->move_from = ubi->move_to = NULL;
	ubi->move_to_put = ubi->wl_scheduled = 0;
	spin_unlock(&ubi->wl_lock);

	err = schedule_erase(ubi, e1, 0);
	if (err) {
		kmem_cache_free(ubi_wl_entry_slab, e1);
		if (e2)
			kmem_cache_free(ubi_wl_entry_slab, e2);
		goto out_ro;
	}

	if (e2) {
		dbg_wl("PEB %d (LEB %d:%d) was put meanwhile, erase",
		       e2->pnum, vol_id, lnum);
		err = schedule_erase(ubi, e2, 0);
		if (err) {
			kmem_cache_free(ubi_wl_entry_slab, e2);
			goto out_ro;
		}
	}

	dbg_wl("done");
	mutex_unlock(&ubi->move_mutex);
	return 0;

out_not_moved:
	if (vol_id != -1)
		dbg_wl("cancel moving PEB %d (LEB %d:%d) to PEB %d (%d)",
		       e1->pnum, vol_id, lnum, e2->pnum, err);
	else
		dbg_wl("cancel moving PEB %d to PEB %d (%d)",
		       e1->pnum, e2->pnum, err);
	spin_lock(&ubi->wl_lock);
	if (protect)
		prot_queue_add(ubi, e1);
	else if (erroneous) {
		wl_tree_add(e1, &ubi->erroneous);
		ubi->erroneous_peb_count += 1;
	} else if (scrubbing)
		wl_tree_add(e1, &ubi->scrub);
	else
		wl_tree_add(e1, &ubi->used);
	ubi_assert(!ubi->move_to_put);
	ubi->move_from = ubi->move_to = NULL;
	ubi->wl_scheduled = 0;
	spin_unlock(&ubi->wl_lock);

	ubi_free_vid_hdr(ubi, vid_hdr);
	err = schedule_erase(ubi, e2, torture);
	if (err) {
		kmem_cache_free(ubi_wl_entry_slab, e2);
		goto out_ro;
	}
	mutex_unlock(&ubi->move_mutex);
	return 0;

out_error:
	if (vol_id != -1)
		ubi_err("error %d while moving PEB %d to PEB %d",
			err, e1->pnum, e2->pnum);
	else
		ubi_err("error %d while moving PEB %d (LEB %d:%d) to PEB %d",
			err, e1->pnum, vol_id, lnum, e2->pnum);
	spin_lock(&ubi->wl_lock);
	ubi->move_from = ubi->move_to = NULL;
	ubi->move_to_put = ubi->wl_scheduled = 0;
	spin_unlock(&ubi->wl_lock);

	ubi_free_vid_hdr(ubi, vid_hdr);
	kmem_cache_free(ubi_wl_entry_slab, e1);
	kmem_cache_free(ubi_wl_entry_slab, e2);

out_ro:
	ubi_ro_mode(ubi);
	mutex_unlock(&ubi->move_mutex);
	ubi_assert(err != 0);
	return err < 0 ? err : -EIO;

out_cancel:
	ubi->wl_scheduled = 0;
	spin_unlock(&ubi->wl_lock);
	mutex_unlock(&ubi->move_mutex);
	ubi_free_vid_hdr(ubi, vid_hdr);
	return 0;
}

static int ensure_wear_leveling(struct ubi_device *ubi)
{
	int err = 0;
	struct ubi_wl_entry *e1;
	struct ubi_wl_entry *e2;
	struct ubi_work *wrk;

	spin_lock(&ubi->wl_lock);
	if (ubi->wl_scheduled)
		
		goto out_unlock;

	if (!ubi->scrub.rb_node) {
		if (!ubi->used.rb_node || !ubi->free.rb_node)
			
			goto out_unlock;

		e1 = rb_entry(rb_first(&ubi->used), struct ubi_wl_entry, u.rb);
		e2 = find_wl_entry(&ubi->free, WL_FREE_MAX_DIFF);

		if (!(e2->ec - e1->ec >= UBI_WL_THRESHOLD))
			goto out_unlock;
		dbg_wl("schedule wear-leveling");
	} else
		dbg_wl("schedule scrubbing");

	ubi->wl_scheduled = 1;
	spin_unlock(&ubi->wl_lock);

	wrk = kmalloc(sizeof(struct ubi_work), GFP_NOFS);
	if (!wrk) {
		err = -ENOMEM;
		goto out_cancel;
	}

	wrk->func = &wear_leveling_worker;
	schedule_ubi_work(ubi, wrk);
	return err;

out_cancel:
	spin_lock(&ubi->wl_lock);
	ubi->wl_scheduled = 0;
out_unlock:
	spin_unlock(&ubi->wl_lock);
	return err;
}

static int erase_worker(struct ubi_device *ubi, struct ubi_work *wl_wrk,
			int cancel)
{
	struct ubi_wl_entry *e = wl_wrk->e;
	int pnum = e->pnum, err, need;

	if (cancel) {
		dbg_wl("cancel erasure of PEB %d EC %d", pnum, e->ec);
		kfree(wl_wrk);
		kmem_cache_free(ubi_wl_entry_slab, e);
		return 0;
	}

	dbg_wl("erase PEB %d EC %d", pnum, e->ec);

	err = sync_erase(ubi, e, wl_wrk->torture);
	if (!err) {
		
		kfree(wl_wrk);

		spin_lock(&ubi->wl_lock);
		wl_tree_add(e, &ubi->free);
		spin_unlock(&ubi->wl_lock);

		serve_prot_queue(ubi);

		
		err = ensure_wear_leveling(ubi);
		return err;
	}

	ubi_err("failed to erase PEB %d, error %d", pnum, err);
	kfree(wl_wrk);

	if (err == -EINTR || err == -ENOMEM || err == -EAGAIN ||
	    err == -EBUSY) {
		int err1;

		
		err1 = schedule_erase(ubi, e, 0);
		if (err1) {
			err = err1;
			goto out_ro;
		}
		return err;
	}

	kmem_cache_free(ubi_wl_entry_slab, e);
	if (err != -EIO)
		goto out_ro;

	

	if (!ubi->bad_allowed) {
		ubi_err("bad physical eraseblock %d detected", pnum);
		goto out_ro;
	}

	spin_lock(&ubi->volumes_lock);
	need = ubi->beb_rsvd_level - ubi->beb_rsvd_pebs + 1;
	if (need > 0) {
		need = ubi->avail_pebs >= need ? need : ubi->avail_pebs;
		ubi->avail_pebs -= need;
		ubi->rsvd_pebs += need;
		ubi->beb_rsvd_pebs += need;
		if (need > 0)
			ubi_msg("reserve more %d PEBs", need);
	}

	if (ubi->beb_rsvd_pebs == 0) {
		spin_unlock(&ubi->volumes_lock);
		ubi_err("no reserved physical eraseblocks");
		goto out_ro;
	}
	spin_unlock(&ubi->volumes_lock);

	ubi_msg("mark PEB %d as bad", pnum);
	err = ubi_io_mark_bad(ubi, pnum);
	if (err)
		goto out_ro;

	spin_lock(&ubi->volumes_lock);
	ubi->beb_rsvd_pebs -= 1;
	ubi->bad_peb_count += 1;
	ubi->good_peb_count -= 1;
	ubi_calculate_reserved(ubi);
	if (ubi->beb_rsvd_pebs)
		ubi_msg("%d PEBs left in the reserve", ubi->beb_rsvd_pebs);
	else
		ubi_warn("last PEB from the reserved pool was used");
	spin_unlock(&ubi->volumes_lock);

	return err;

out_ro:
	ubi_ro_mode(ubi);
	return err;
}

int ubi_wl_put_peb(struct ubi_device *ubi, int pnum, int torture)
{
	int err;
	struct ubi_wl_entry *e;

	dbg_wl("PEB %d", pnum);
	ubi_assert(pnum >= 0);
	ubi_assert(pnum < ubi->peb_count);

retry:
	spin_lock(&ubi->wl_lock);
	e = ubi->lookuptbl[pnum];
	if (e == ubi->move_from) {
		dbg_wl("PEB %d is being moved, wait", pnum);
		spin_unlock(&ubi->wl_lock);

		
		mutex_lock(&ubi->move_mutex);
		mutex_unlock(&ubi->move_mutex);
		goto retry;
	} else if (e == ubi->move_to) {
		dbg_wl("PEB %d is the target of data moving", pnum);
		ubi_assert(!ubi->move_to_put);
		ubi->move_to_put = 1;
		spin_unlock(&ubi->wl_lock);
		return 0;
	} else {
		if (in_wl_tree(e, &ubi->used)) {
			paranoid_check_in_wl_tree(ubi, e, &ubi->used);
			rb_erase(&e->u.rb, &ubi->used);
		} else if (in_wl_tree(e, &ubi->scrub)) {
			paranoid_check_in_wl_tree(ubi, e, &ubi->scrub);
			rb_erase(&e->u.rb, &ubi->scrub);
		} else if (in_wl_tree(e, &ubi->erroneous)) {
			paranoid_check_in_wl_tree(ubi, e, &ubi->erroneous);
			rb_erase(&e->u.rb, &ubi->erroneous);
			ubi->erroneous_peb_count -= 1;
			ubi_assert(ubi->erroneous_peb_count >= 0);
			
			torture = 1;
		} else {
			err = prot_queue_del(ubi, e->pnum);
			if (err) {
				ubi_err("PEB %d not found", pnum);
				ubi_ro_mode(ubi);
				spin_unlock(&ubi->wl_lock);
				return err;
			}
		}
	}
	spin_unlock(&ubi->wl_lock);

	err = schedule_erase(ubi, e, torture);
	if (err) {
		spin_lock(&ubi->wl_lock);
		wl_tree_add(e, &ubi->used);
		spin_unlock(&ubi->wl_lock);
	}

	return err;
}

int ubi_wl_scrub_peb(struct ubi_device *ubi, int pnum)
{
	struct ubi_wl_entry *e;

	dbg_msg("schedule PEB %d for scrubbing", pnum);

retry:
	spin_lock(&ubi->wl_lock);
	e = ubi->lookuptbl[pnum];
	if (e == ubi->move_from || in_wl_tree(e, &ubi->scrub) ||
				   in_wl_tree(e, &ubi->erroneous)) {
		spin_unlock(&ubi->wl_lock);
		return 0;
	}

	if (e == ubi->move_to) {
		spin_unlock(&ubi->wl_lock);
		dbg_wl("the PEB %d is not in proper tree, retry", pnum);
		yield();
		goto retry;
	}

	if (in_wl_tree(e, &ubi->used)) {
		paranoid_check_in_wl_tree(ubi, e, &ubi->used);
		rb_erase(&e->u.rb, &ubi->used);
	} else {
		int err;

		err = prot_queue_del(ubi, e->pnum);
		if (err) {
			ubi_err("PEB %d not found", pnum);
			ubi_ro_mode(ubi);
			spin_unlock(&ubi->wl_lock);
			return err;
		}
	}

	wl_tree_add(e, &ubi->scrub);
	spin_unlock(&ubi->wl_lock);

	return ensure_wear_leveling(ubi);
}

int ubi_wl_flush(struct ubi_device *ubi)
{
	int err;

	dbg_wl("flush (%d pending works)", ubi->works_count);
	while (ubi->works_count) {
		err = do_work(ubi);
		if (err)
			return err;
	}

	down_write(&ubi->work_sem);
	up_write(&ubi->work_sem);

	while (ubi->works_count) {
		dbg_wl("flush more (%d pending works)", ubi->works_count);
		err = do_work(ubi);
		if (err)
			return err;
	}

	return 0;
}

static void tree_destroy(struct rb_root *root)
{
	struct rb_node *rb;
	struct ubi_wl_entry *e;

	rb = root->rb_node;
	while (rb) {
		if (rb->rb_left)
			rb = rb->rb_left;
		else if (rb->rb_right)
			rb = rb->rb_right;
		else {
			e = rb_entry(rb, struct ubi_wl_entry, u.rb);

			rb = rb_parent(rb);
			if (rb) {
				if (rb->rb_left == &e->u.rb)
					rb->rb_left = NULL;
				else
					rb->rb_right = NULL;
			}

			kmem_cache_free(ubi_wl_entry_slab, e);
		}
	}
}

int ubi_thread(void *u)
{
	int failures = 0;
	struct ubi_device *ubi = u;

	ubi_msg("background thread \"%s\" started, PID %d",
		ubi->bgt_name, task_pid_nr(current));

	set_freezable();
	for (;;) {
		int err;

		if (kthread_should_stop())
			break;

		if (try_to_freeze())
			continue;

		spin_lock(&ubi->wl_lock);
		if (list_empty(&ubi->works) || ubi->ro_mode ||
		    !ubi->thread_enabled || ubi_dbg_is_bgt_disabled(ubi)) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock(&ubi->wl_lock);
			schedule();
			continue;
		}
		spin_unlock(&ubi->wl_lock);

		err = do_work(ubi);
		if (err) {
			ubi_err("%s: work failed with error code %d",
				ubi->bgt_name, err);
			if (failures++ > WL_MAX_FAILURES) {
				ubi_msg("%s: %d consecutive failures",
					ubi->bgt_name, WL_MAX_FAILURES);
				ubi_ro_mode(ubi);
				ubi->thread_enabled = 0;
				continue;
			}
		} else
			failures = 0;

		cond_resched();
	}

	dbg_wl("background thread \"%s\" is killed", ubi->bgt_name);
	return 0;
}

static void cancel_pending(struct ubi_device *ubi)
{
	while (!list_empty(&ubi->works)) {
		struct ubi_work *wrk;

		wrk = list_entry(ubi->works.next, struct ubi_work, list);
		list_del(&wrk->list);
		wrk->func(ubi, wrk, 1);
		ubi->works_count -= 1;
		ubi_assert(ubi->works_count >= 0);
	}
}

int ubi_wl_init_scan(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	int err, i;
	struct rb_node *rb1, *rb2;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb, *tmp;
	struct ubi_wl_entry *e;

	ubi->used = ubi->erroneous = ubi->free = ubi->scrub = RB_ROOT;
	spin_lock_init(&ubi->wl_lock);
	mutex_init(&ubi->move_mutex);
	init_rwsem(&ubi->work_sem);
	ubi->max_ec = si->max_ec;
	INIT_LIST_HEAD(&ubi->works);

	sprintf(ubi->bgt_name, UBI_BGT_NAME_PATTERN, ubi->ubi_num);

	err = -ENOMEM;
	ubi->lookuptbl = kzalloc(ubi->peb_count * sizeof(void *), GFP_KERNEL);
	if (!ubi->lookuptbl)
		return err;

	for (i = 0; i < UBI_PROT_QUEUE_LEN; i++)
		INIT_LIST_HEAD(&ubi->pq[i]);
	ubi->pq_head = 0;

	list_for_each_entry_safe(seb, tmp, &si->erase, u.list) {
		cond_resched();

		e = kmem_cache_alloc(ubi_wl_entry_slab, GFP_KERNEL);
		if (!e)
			goto out_free;

		e->pnum = seb->pnum;
		e->ec = seb->ec;
		ubi->lookuptbl[e->pnum] = e;
		if (schedule_erase(ubi, e, 0)) {
			kmem_cache_free(ubi_wl_entry_slab, e);
			goto out_free;
		}
	}

	list_for_each_entry(seb, &si->free, u.list) {
		cond_resched();

		e = kmem_cache_alloc(ubi_wl_entry_slab, GFP_KERNEL);
		if (!e)
			goto out_free;

		e->pnum = seb->pnum;
		e->ec = seb->ec;
		ubi_assert(e->ec >= 0);
		wl_tree_add(e, &ubi->free);
		ubi->lookuptbl[e->pnum] = e;
	}

	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb) {
			cond_resched();

			e = kmem_cache_alloc(ubi_wl_entry_slab, GFP_KERNEL);
			if (!e)
				goto out_free;

			e->pnum = seb->pnum;
			e->ec = seb->ec;
			ubi->lookuptbl[e->pnum] = e;
			if (!seb->scrub) {
				dbg_wl("add PEB %d EC %d to the used tree",
				       e->pnum, e->ec);
				wl_tree_add(e, &ubi->used);
			} else {
				dbg_wl("add PEB %d EC %d to the scrub tree",
				       e->pnum, e->ec);
				wl_tree_add(e, &ubi->scrub);
			}
		}
	}

	if (ubi->avail_pebs < WL_RESERVED_PEBS) {
		ubi_err("no enough physical eraseblocks (%d, need %d)",
			ubi->avail_pebs, WL_RESERVED_PEBS);
		if (ubi->corr_peb_count)
			ubi_err("%d PEBs are corrupted and not used",
				ubi->corr_peb_count);
		goto out_free;
	}
	ubi->avail_pebs -= WL_RESERVED_PEBS;
	ubi->rsvd_pebs += WL_RESERVED_PEBS;

	
	err = ensure_wear_leveling(ubi);
	if (err)
		goto out_free;

	return 0;

out_free:
	cancel_pending(ubi);
	tree_destroy(&ubi->used);
	tree_destroy(&ubi->free);
	tree_destroy(&ubi->scrub);
	kfree(ubi->lookuptbl);
	return err;
}

static void protection_queue_destroy(struct ubi_device *ubi)
{
	int i;
	struct ubi_wl_entry *e, *tmp;

	for (i = 0; i < UBI_PROT_QUEUE_LEN; ++i) {
		list_for_each_entry_safe(e, tmp, &ubi->pq[i], u.list) {
			list_del(&e->u.list);
			kmem_cache_free(ubi_wl_entry_slab, e);
		}
	}
}

void ubi_wl_close(struct ubi_device *ubi)
{
	dbg_wl("close the WL sub-system");
	cancel_pending(ubi);
	protection_queue_destroy(ubi);
	tree_destroy(&ubi->used);
	tree_destroy(&ubi->erroneous);
	tree_destroy(&ubi->free);
	tree_destroy(&ubi->scrub);
	kfree(ubi->lookuptbl);
}

#ifdef CONFIG_MTD_UBI_DEBUG

static int paranoid_check_ec(struct ubi_device *ubi, int pnum, int ec)
{
	int err;
	long long read_ec;
	struct ubi_ec_hdr *ec_hdr;

	if (!ubi->dbg->chk_gen)
		return 0;

	ec_hdr = kzalloc(ubi->ec_hdr_alsize, GFP_NOFS);
	if (!ec_hdr)
		return -ENOMEM;

	err = ubi_io_read_ec_hdr(ubi, pnum, ec_hdr, 0);
	if (err && err != UBI_IO_BITFLIPS) {
		
		err = 0;
		goto out_free;
	}

	read_ec = be64_to_cpu(ec_hdr->ec);
	if (ec != read_ec) {
		ubi_err("paranoid check failed for PEB %d", pnum);
		ubi_err("read EC is %lld, should be %d", read_ec, ec);
		ubi_dbg_dump_stack();
		err = 1;
	} else
		err = 0;

out_free:
	kfree(ec_hdr);
	return err;
}

static int paranoid_check_in_wl_tree(const struct ubi_device *ubi,
				     struct ubi_wl_entry *e,
				     struct rb_root *root)
{
	if (!ubi->dbg->chk_gen)
		return 0;

	if (in_wl_tree(e, root))
		return 0;

	ubi_err("paranoid check failed for PEB %d, EC %d, RB-tree %p ",
		e->pnum, e->ec, root);
	ubi_dbg_dump_stack();
	return -EINVAL;
}

static int paranoid_check_in_pq(const struct ubi_device *ubi,
				struct ubi_wl_entry *e)
{
	struct ubi_wl_entry *p;
	int i;

	if (!ubi->dbg->chk_gen)
		return 0;

	for (i = 0; i < UBI_PROT_QUEUE_LEN; ++i)
		list_for_each_entry(p, &ubi->pq[i], u.list)
			if (p == e)
				return 0;

	ubi_err("paranoid check failed for PEB %d, EC %d, Protect queue",
		e->pnum, e->ec);
	ubi_dbg_dump_stack();
	return -EINVAL;
}

#endif 
