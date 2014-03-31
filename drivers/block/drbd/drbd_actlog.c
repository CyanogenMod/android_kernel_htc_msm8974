/*
   drbd_actlog.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2003-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2003-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2003-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

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

#include <linux/slab.h>
#include <linux/drbd.h>
#include "drbd_int.h"
#include "drbd_wrappers.h"

struct __packed al_transaction {
	u32       magic;
	u32       tr_number;
	struct __packed {
		u32 pos;
		u32 extent; } updates[1 + AL_EXTENTS_PT];
	u32       xor_sum;
};

struct update_odbm_work {
	struct drbd_work w;
	unsigned int enr;
};

struct update_al_work {
	struct drbd_work w;
	struct lc_element *al_ext;
	struct completion event;
	unsigned int enr;
	
	unsigned int old_enr;
};

struct drbd_atodb_wait {
	atomic_t           count;
	struct completion  io_done;
	struct drbd_conf   *mdev;
	int                error;
};


int w_al_write_transaction(struct drbd_conf *, struct drbd_work *, int);

static int _drbd_md_sync_page_io(struct drbd_conf *mdev,
				 struct drbd_backing_dev *bdev,
				 struct page *page, sector_t sector,
				 int rw, int size)
{
	struct bio *bio;
	struct drbd_md_io md_io;
	int ok;

	md_io.mdev = mdev;
	init_completion(&md_io.event);
	md_io.error = 0;

	if ((rw & WRITE) && !test_bit(MD_NO_FUA, &mdev->flags))
		rw |= REQ_FUA | REQ_FLUSH;
	rw |= REQ_SYNC;

	bio = bio_alloc(GFP_NOIO, 1);
	bio->bi_bdev = bdev->md_bdev;
	bio->bi_sector = sector;
	ok = (bio_add_page(bio, page, size, 0) == size);
	if (!ok)
		goto out;
	bio->bi_private = &md_io;
	bio->bi_end_io = drbd_md_io_complete;
	bio->bi_rw = rw;

	if (drbd_insert_fault(mdev, (rw & WRITE) ? DRBD_FAULT_MD_WR : DRBD_FAULT_MD_RD))
		bio_endio(bio, -EIO);
	else
		submit_bio(rw, bio);
	wait_for_completion(&md_io.event);
	ok = bio_flagged(bio, BIO_UPTODATE) && md_io.error == 0;

 out:
	bio_put(bio);
	return ok;
}

int drbd_md_sync_page_io(struct drbd_conf *mdev, struct drbd_backing_dev *bdev,
			 sector_t sector, int rw)
{
	int logical_block_size, mask, ok;
	int offset = 0;
	struct page *iop = mdev->md_io_page;

	D_ASSERT(mutex_is_locked(&mdev->md_io_mutex));

	BUG_ON(!bdev->md_bdev);

	logical_block_size = bdev_logical_block_size(bdev->md_bdev);
	if (logical_block_size == 0)
		logical_block_size = MD_SECTOR_SIZE;

	
	if (logical_block_size != MD_SECTOR_SIZE) {
		mask = (logical_block_size / MD_SECTOR_SIZE) - 1;
		D_ASSERT(mask == 1 || mask == 3 || mask == 7);
		D_ASSERT(logical_block_size == (mask+1) * MD_SECTOR_SIZE);
		offset = sector & mask;
		sector = sector & ~mask;
		iop = mdev->md_io_tmpp;

		if (rw & WRITE) {
			void *p = page_address(mdev->md_io_page);
			void *hp = page_address(mdev->md_io_tmpp);

			ok = _drbd_md_sync_page_io(mdev, bdev, iop, sector,
					READ, logical_block_size);

			if (unlikely(!ok)) {
				dev_err(DEV, "drbd_md_sync_page_io(,%llus,"
				    "READ [logical_block_size!=512]) failed!\n",
				    (unsigned long long)sector);
				return 0;
			}

			memcpy(hp + offset*MD_SECTOR_SIZE, p, MD_SECTOR_SIZE);
		}
	}

	if (sector < drbd_md_first_sector(bdev) ||
	    sector > drbd_md_last_sector(bdev))
		dev_alert(DEV, "%s [%d]:%s(,%llus,%s) out of range md access!\n",
		     current->comm, current->pid, __func__,
		     (unsigned long long)sector, (rw & WRITE) ? "WRITE" : "READ");

	ok = _drbd_md_sync_page_io(mdev, bdev, iop, sector, rw, logical_block_size);
	if (unlikely(!ok)) {
		dev_err(DEV, "drbd_md_sync_page_io(,%llus,%s) failed!\n",
		    (unsigned long long)sector, (rw & WRITE) ? "WRITE" : "READ");
		return 0;
	}

	if (logical_block_size != MD_SECTOR_SIZE && !(rw & WRITE)) {
		void *p = page_address(mdev->md_io_page);
		void *hp = page_address(mdev->md_io_tmpp);

		memcpy(p, hp + offset*MD_SECTOR_SIZE, MD_SECTOR_SIZE);
	}

	return ok;
}

static struct lc_element *_al_get(struct drbd_conf *mdev, unsigned int enr)
{
	struct lc_element *al_ext;
	struct lc_element *tmp;
	unsigned long     al_flags = 0;
	int wake;

	spin_lock_irq(&mdev->al_lock);
	tmp = lc_find(mdev->resync, enr/AL_EXT_PER_BM_SECT);
	if (unlikely(tmp != NULL)) {
		struct bm_extent  *bm_ext = lc_entry(tmp, struct bm_extent, lce);
		if (test_bit(BME_NO_WRITES, &bm_ext->flags)) {
			wake = !test_and_set_bit(BME_PRIORITY, &bm_ext->flags);
			spin_unlock_irq(&mdev->al_lock);
			if (wake)
				wake_up(&mdev->al_wait);
			return NULL;
		}
	}
	al_ext   = lc_get(mdev->act_log, enr);
	al_flags = mdev->act_log->flags;
	spin_unlock_irq(&mdev->al_lock);


	return al_ext;
}

void drbd_al_begin_io(struct drbd_conf *mdev, sector_t sector)
{
	unsigned int enr = (sector >> (AL_EXTENT_SHIFT-9));
	struct lc_element *al_ext;
	struct update_al_work al_work;

	D_ASSERT(atomic_read(&mdev->local_cnt) > 0);

	wait_event(mdev->al_wait, (al_ext = _al_get(mdev, enr)));

	if (al_ext->lc_number != enr) {
		init_completion(&al_work.event);
		al_work.al_ext = al_ext;
		al_work.enr = enr;
		al_work.old_enr = al_ext->lc_number;
		al_work.w.cb = w_al_write_transaction;
		drbd_queue_work_front(&mdev->data.work, &al_work.w);
		wait_for_completion(&al_work.event);

		mdev->al_writ_cnt++;

		spin_lock_irq(&mdev->al_lock);
		lc_changed(mdev->act_log, al_ext);
		spin_unlock_irq(&mdev->al_lock);
		wake_up(&mdev->al_wait);
	}
}

void drbd_al_complete_io(struct drbd_conf *mdev, sector_t sector)
{
	unsigned int enr = (sector >> (AL_EXTENT_SHIFT-9));
	struct lc_element *extent;
	unsigned long flags;

	spin_lock_irqsave(&mdev->al_lock, flags);

	extent = lc_find(mdev->act_log, enr);

	if (!extent) {
		spin_unlock_irqrestore(&mdev->al_lock, flags);
		dev_err(DEV, "al_complete_io() called on inactive extent %u\n", enr);
		return;
	}

	if (lc_put(mdev->act_log, extent) == 0)
		wake_up(&mdev->al_wait);

	spin_unlock_irqrestore(&mdev->al_lock, flags);
}

#if (PAGE_SHIFT + 3) < (AL_EXTENT_SHIFT - BM_BLOCK_SHIFT)
# error FIXME
#endif

static unsigned int al_extent_to_bm_page(unsigned int al_enr)
{
	return al_enr >>
		
		((PAGE_SHIFT + 3) -
		
		 (AL_EXTENT_SHIFT - BM_BLOCK_SHIFT));
}

static unsigned int rs_extent_to_bm_page(unsigned int rs_enr)
{
	return rs_enr >>
		
		((PAGE_SHIFT + 3) -
		
		 (BM_EXT_SHIFT - BM_BLOCK_SHIFT));
}

int
w_al_write_transaction(struct drbd_conf *mdev, struct drbd_work *w, int unused)
{
	struct update_al_work *aw = container_of(w, struct update_al_work, w);
	struct lc_element *updated = aw->al_ext;
	const unsigned int new_enr = aw->enr;
	const unsigned int evicted = aw->old_enr;
	struct al_transaction *buffer;
	sector_t sector;
	int i, n, mx;
	unsigned int extent_nr;
	u32 xor_sum = 0;

	if (!get_ldev(mdev)) {
		dev_err(DEV,
			"disk is %s, cannot start al transaction (-%d +%d)\n",
			drbd_disk_str(mdev->state.disk), evicted, new_enr);
		complete(&((struct update_al_work *)w)->event);
		return 1;
	}
	if (mdev->state.conn < C_CONNECTED && evicted != LC_FREE)
		drbd_bm_write_page(mdev, al_extent_to_bm_page(evicted));

	
	if (mdev->state.disk < D_INCONSISTENT) {
		dev_err(DEV,
			"disk is %s, cannot write al transaction (-%d +%d)\n",
			drbd_disk_str(mdev->state.disk), evicted, new_enr);
		complete(&((struct update_al_work *)w)->event);
		put_ldev(mdev);
		return 1;
	}

	mutex_lock(&mdev->md_io_mutex); 
	buffer = (struct al_transaction *)page_address(mdev->md_io_page);

	buffer->magic = __constant_cpu_to_be32(DRBD_MAGIC);
	buffer->tr_number = cpu_to_be32(mdev->al_tr_number);

	n = lc_index_of(mdev->act_log, updated);

	buffer->updates[0].pos = cpu_to_be32(n);
	buffer->updates[0].extent = cpu_to_be32(new_enr);

	xor_sum ^= new_enr;

	mx = min_t(int, AL_EXTENTS_PT,
		   mdev->act_log->nr_elements - mdev->al_tr_cycle);
	for (i = 0; i < mx; i++) {
		unsigned idx = mdev->al_tr_cycle + i;
		extent_nr = lc_element_by_index(mdev->act_log, idx)->lc_number;
		buffer->updates[i+1].pos = cpu_to_be32(idx);
		buffer->updates[i+1].extent = cpu_to_be32(extent_nr);
		xor_sum ^= extent_nr;
	}
	for (; i < AL_EXTENTS_PT; i++) {
		buffer->updates[i+1].pos = __constant_cpu_to_be32(-1);
		buffer->updates[i+1].extent = __constant_cpu_to_be32(LC_FREE);
		xor_sum ^= LC_FREE;
	}
	mdev->al_tr_cycle += AL_EXTENTS_PT;
	if (mdev->al_tr_cycle >= mdev->act_log->nr_elements)
		mdev->al_tr_cycle = 0;

	buffer->xor_sum = cpu_to_be32(xor_sum);

	sector =  mdev->ldev->md.md_offset
		+ mdev->ldev->md.al_offset + mdev->al_tr_pos;

	if (!drbd_md_sync_page_io(mdev, mdev->ldev, sector, WRITE))
		drbd_chk_io_error(mdev, 1, true);

	if (++mdev->al_tr_pos >
	    div_ceil(mdev->act_log->nr_elements, AL_EXTENTS_PT))
		mdev->al_tr_pos = 0;

	D_ASSERT(mdev->al_tr_pos < MD_AL_MAX_SIZE);
	mdev->al_tr_number++;

	mutex_unlock(&mdev->md_io_mutex);

	complete(&((struct update_al_work *)w)->event);
	put_ldev(mdev);

	return 1;
}

static int drbd_al_read_tr(struct drbd_conf *mdev,
			   struct drbd_backing_dev *bdev,
			   struct al_transaction *b,
			   int index)
{
	sector_t sector;
	int rv, i;
	u32 xor_sum = 0;

	sector = bdev->md.md_offset + bdev->md.al_offset + index;

	if (!drbd_md_sync_page_io(mdev, bdev, sector, READ))
		return -1;

	rv = (be32_to_cpu(b->magic) == DRBD_MAGIC);

	for (i = 0; i < AL_EXTENTS_PT + 1; i++)
		xor_sum ^= be32_to_cpu(b->updates[i].extent);
	rv &= (xor_sum == be32_to_cpu(b->xor_sum));

	return rv;
}

int drbd_al_read_log(struct drbd_conf *mdev, struct drbd_backing_dev *bdev)
{
	struct al_transaction *buffer;
	int i;
	int rv;
	int mx;
	int active_extents = 0;
	int transactions = 0;
	int found_valid = 0;
	int from = 0;
	int to = 0;
	u32 from_tnr = 0;
	u32 to_tnr = 0;
	u32 cnr;

	mx = div_ceil(mdev->act_log->nr_elements, AL_EXTENTS_PT);

	mutex_lock(&mdev->md_io_mutex);
	buffer = page_address(mdev->md_io_page);

	
	for (i = 0; i <= mx; i++) {
		rv = drbd_al_read_tr(mdev, bdev, buffer, i);
		if (rv == 0)
			continue;
		if (rv == -1) {
			mutex_unlock(&mdev->md_io_mutex);
			return 0;
		}
		cnr = be32_to_cpu(buffer->tr_number);

		if (++found_valid == 1) {
			from = i;
			to = i;
			from_tnr = cnr;
			to_tnr = cnr;
			continue;
		}
		if ((int)cnr - (int)from_tnr < 0) {
			D_ASSERT(from_tnr - cnr + i - from == mx+1);
			from = i;
			from_tnr = cnr;
		}
		if ((int)cnr - (int)to_tnr > 0) {
			D_ASSERT(cnr - to_tnr == i - to);
			to = i;
			to_tnr = cnr;
		}
	}

	if (!found_valid) {
		dev_warn(DEV, "No usable activity log found.\n");
		mutex_unlock(&mdev->md_io_mutex);
		return 1;
	}

	i = from;
	while (1) {
		int j, pos;
		unsigned int extent_nr;
		unsigned int trn;

		rv = drbd_al_read_tr(mdev, bdev, buffer, i);
		ERR_IF(rv == 0) goto cancel;
		if (rv == -1) {
			mutex_unlock(&mdev->md_io_mutex);
			return 0;
		}

		trn = be32_to_cpu(buffer->tr_number);

		spin_lock_irq(&mdev->al_lock);

		for (j = AL_EXTENTS_PT; j >= 0; j--) {
			pos = be32_to_cpu(buffer->updates[j].pos);
			extent_nr = be32_to_cpu(buffer->updates[j].extent);

			if (extent_nr == LC_FREE)
				continue;

			lc_set(mdev->act_log, extent_nr, pos);
			active_extents++;
		}
		spin_unlock_irq(&mdev->al_lock);

		transactions++;

cancel:
		if (i == to)
			break;
		i++;
		if (i > mx)
			i = 0;
	}

	mdev->al_tr_number = to_tnr+1;
	mdev->al_tr_pos = to;
	if (++mdev->al_tr_pos >
	    div_ceil(mdev->act_log->nr_elements, AL_EXTENTS_PT))
		mdev->al_tr_pos = 0;

	
	mutex_unlock(&mdev->md_io_mutex);

	dev_info(DEV, "Found %d transactions (%d active extents) in activity log.\n",
	     transactions, active_extents);

	return 1;
}

void drbd_al_apply_to_bm(struct drbd_conf *mdev)
{
	unsigned int enr;
	unsigned long add = 0;
	char ppb[10];
	int i, tmp;

	wait_event(mdev->al_wait, lc_try_lock(mdev->act_log));

	for (i = 0; i < mdev->act_log->nr_elements; i++) {
		enr = lc_element_by_index(mdev->act_log, i)->lc_number;
		if (enr == LC_FREE)
			continue;
		tmp = drbd_bm_ALe_set_all(mdev, enr);
		dynamic_dev_dbg(DEV, "AL: set %d bits in extent %u\n", tmp, enr);
		add += tmp;
	}

	lc_unlock(mdev->act_log);
	wake_up(&mdev->al_wait);

	dev_info(DEV, "Marked additional %s as out-of-sync based on AL.\n",
	     ppsize(ppb, Bit2KB(add)));
}

static int _try_lc_del(struct drbd_conf *mdev, struct lc_element *al_ext)
{
	int rv;

	spin_lock_irq(&mdev->al_lock);
	rv = (al_ext->refcnt == 0);
	if (likely(rv))
		lc_del(mdev->act_log, al_ext);
	spin_unlock_irq(&mdev->al_lock);

	return rv;
}

void drbd_al_shrink(struct drbd_conf *mdev)
{
	struct lc_element *al_ext;
	int i;

	D_ASSERT(test_bit(__LC_DIRTY, &mdev->act_log->flags));

	for (i = 0; i < mdev->act_log->nr_elements; i++) {
		al_ext = lc_element_by_index(mdev->act_log, i);
		if (al_ext->lc_number == LC_FREE)
			continue;
		wait_event(mdev->al_wait, _try_lc_del(mdev, al_ext));
	}

	wake_up(&mdev->al_wait);
}

static int w_update_odbm(struct drbd_conf *mdev, struct drbd_work *w, int unused)
{
	struct update_odbm_work *udw = container_of(w, struct update_odbm_work, w);

	if (!get_ldev(mdev)) {
		if (__ratelimit(&drbd_ratelimit_state))
			dev_warn(DEV, "Can not update on disk bitmap, local IO disabled.\n");
		kfree(udw);
		return 1;
	}

	drbd_bm_write_page(mdev, rs_extent_to_bm_page(udw->enr));
	put_ldev(mdev);

	kfree(udw);

	if (drbd_bm_total_weight(mdev) <= mdev->rs_failed) {
		switch (mdev->state.conn) {
		case C_SYNC_SOURCE:  case C_SYNC_TARGET:
		case C_PAUSED_SYNC_S: case C_PAUSED_SYNC_T:
			drbd_resync_finished(mdev);
		default:
			
			break;
		}
	}
	drbd_bcast_sync_progress(mdev);

	return 1;
}


static void drbd_try_clear_on_disk_bm(struct drbd_conf *mdev, sector_t sector,
				      int count, int success)
{
	struct lc_element *e;
	struct update_odbm_work *udw;

	unsigned int enr;

	D_ASSERT(atomic_read(&mdev->local_cnt));

	enr = BM_SECT_TO_EXT(sector);

	e = lc_get(mdev->resync, enr);
	if (e) {
		struct bm_extent *ext = lc_entry(e, struct bm_extent, lce);
		if (ext->lce.lc_number == enr) {
			if (success)
				ext->rs_left -= count;
			else
				ext->rs_failed += count;
			if (ext->rs_left < ext->rs_failed) {
				dev_err(DEV, "BAD! sector=%llus enr=%u rs_left=%d "
				    "rs_failed=%d count=%d\n",
				     (unsigned long long)sector,
				     ext->lce.lc_number, ext->rs_left,
				     ext->rs_failed, count);
				dump_stack();

				lc_put(mdev->resync, &ext->lce);
				drbd_force_state(mdev, NS(conn, C_DISCONNECTING));
				return;
			}
		} else {
			int rs_left = drbd_bm_e_weight(mdev, enr);
			if (ext->flags != 0) {
				dev_warn(DEV, "changing resync lce: %d[%u;%02lx]"
				     " -> %d[%u;00]\n",
				     ext->lce.lc_number, ext->rs_left,
				     ext->flags, enr, rs_left);
				ext->flags = 0;
			}
			if (ext->rs_failed) {
				dev_warn(DEV, "Kicking resync_lru element enr=%u "
				     "out with rs_failed=%d\n",
				     ext->lce.lc_number, ext->rs_failed);
			}
			ext->rs_left = rs_left;
			ext->rs_failed = success ? 0 : count;
			lc_changed(mdev->resync, &ext->lce);
		}
		lc_put(mdev->resync, &ext->lce);
		

		if (ext->rs_left == ext->rs_failed) {
			ext->rs_failed = 0;

			udw = kmalloc(sizeof(*udw), GFP_ATOMIC);
			if (udw) {
				udw->enr = ext->lce.lc_number;
				udw->w.cb = w_update_odbm;
				drbd_queue_work_front(&mdev->data.work, &udw->w);
			} else {
				dev_warn(DEV, "Could not kmalloc an udw\n");
			}
		}
	} else {
		dev_err(DEV, "lc_get() failed! locked=%d/%d flags=%lu\n",
		    mdev->resync_locked,
		    mdev->resync->nr_elements,
		    mdev->resync->flags);
	}
}

void drbd_advance_rs_marks(struct drbd_conf *mdev, unsigned long still_to_go)
{
	unsigned long now = jiffies;
	unsigned long last = mdev->rs_mark_time[mdev->rs_last_mark];
	int next = (mdev->rs_last_mark + 1) % DRBD_SYNC_MARKS;
	if (time_after_eq(now, last + DRBD_SYNC_MARK_STEP)) {
		if (mdev->rs_mark_left[mdev->rs_last_mark] != still_to_go &&
		    mdev->state.conn != C_PAUSED_SYNC_T &&
		    mdev->state.conn != C_PAUSED_SYNC_S) {
			mdev->rs_mark_time[next] = now;
			mdev->rs_mark_left[next] = still_to_go;
			mdev->rs_last_mark = next;
		}
	}
}

void __drbd_set_in_sync(struct drbd_conf *mdev, sector_t sector, int size,
		       const char *file, const unsigned int line)
{
	
	unsigned long sbnr, ebnr, lbnr;
	unsigned long count = 0;
	sector_t esector, nr_sectors;
	int wake_up = 0;
	unsigned long flags;

	if (size <= 0 || (size & 0x1ff) != 0 || size > DRBD_MAX_BIO_SIZE) {
		dev_err(DEV, "drbd_set_in_sync: sector=%llus size=%d nonsense!\n",
				(unsigned long long)sector, size);
		return;
	}
	nr_sectors = drbd_get_capacity(mdev->this_bdev);
	esector = sector + (size >> 9) - 1;

	ERR_IF(sector >= nr_sectors) return;
	ERR_IF(esector >= nr_sectors) esector = (nr_sectors-1);

	lbnr = BM_SECT_TO_BIT(nr_sectors-1);

	if (unlikely(esector < BM_SECT_PER_BIT-1))
		return;
	if (unlikely(esector == (nr_sectors-1)))
		ebnr = lbnr;
	else
		ebnr = BM_SECT_TO_BIT(esector - (BM_SECT_PER_BIT-1));
	sbnr = BM_SECT_TO_BIT(sector + BM_SECT_PER_BIT-1);

	if (sbnr > ebnr)
		return;

	count = drbd_bm_clear_bits(mdev, sbnr, ebnr);
	if (count && get_ldev(mdev)) {
		drbd_advance_rs_marks(mdev, drbd_bm_total_weight(mdev));
		spin_lock_irqsave(&mdev->al_lock, flags);
		drbd_try_clear_on_disk_bm(mdev, sector, count, true);
		spin_unlock_irqrestore(&mdev->al_lock, flags);

		wake_up = 1;
		put_ldev(mdev);
	}
	if (wake_up)
		wake_up(&mdev->al_wait);
}

int __drbd_set_out_of_sync(struct drbd_conf *mdev, sector_t sector, int size,
			    const char *file, const unsigned int line)
{
	unsigned long sbnr, ebnr, lbnr, flags;
	sector_t esector, nr_sectors;
	unsigned int enr, count = 0;
	struct lc_element *e;

	if (size <= 0 || (size & 0x1ff) != 0 || size > DRBD_MAX_BIO_SIZE) {
		dev_err(DEV, "sector: %llus, size: %d\n",
			(unsigned long long)sector, size);
		return 0;
	}

	if (!get_ldev(mdev))
		return 0; 

	nr_sectors = drbd_get_capacity(mdev->this_bdev);
	esector = sector + (size >> 9) - 1;

	ERR_IF(sector >= nr_sectors)
		goto out;
	ERR_IF(esector >= nr_sectors)
		esector = (nr_sectors-1);

	lbnr = BM_SECT_TO_BIT(nr_sectors-1);

	sbnr = BM_SECT_TO_BIT(sector);
	ebnr = BM_SECT_TO_BIT(esector);

	spin_lock_irqsave(&mdev->al_lock, flags);
	count = drbd_bm_set_bits(mdev, sbnr, ebnr);

	enr = BM_SECT_TO_EXT(sector);
	e = lc_find(mdev->resync, enr);
	if (e)
		lc_entry(e, struct bm_extent, lce)->rs_left += count;
	spin_unlock_irqrestore(&mdev->al_lock, flags);

out:
	put_ldev(mdev);

	return count;
}

static
struct bm_extent *_bme_get(struct drbd_conf *mdev, unsigned int enr)
{
	struct lc_element *e;
	struct bm_extent *bm_ext;
	int wakeup = 0;
	unsigned long rs_flags;

	spin_lock_irq(&mdev->al_lock);
	if (mdev->resync_locked > mdev->resync->nr_elements/2) {
		spin_unlock_irq(&mdev->al_lock);
		return NULL;
	}
	e = lc_get(mdev->resync, enr);
	bm_ext = e ? lc_entry(e, struct bm_extent, lce) : NULL;
	if (bm_ext) {
		if (bm_ext->lce.lc_number != enr) {
			bm_ext->rs_left = drbd_bm_e_weight(mdev, enr);
			bm_ext->rs_failed = 0;
			lc_changed(mdev->resync, &bm_ext->lce);
			wakeup = 1;
		}
		if (bm_ext->lce.refcnt == 1)
			mdev->resync_locked++;
		set_bit(BME_NO_WRITES, &bm_ext->flags);
	}
	rs_flags = mdev->resync->flags;
	spin_unlock_irq(&mdev->al_lock);
	if (wakeup)
		wake_up(&mdev->al_wait);

	if (!bm_ext) {
		if (rs_flags & LC_STARVING)
			dev_warn(DEV, "Have to wait for element"
			     " (resync LRU too small?)\n");
		BUG_ON(rs_flags & LC_DIRTY);
	}

	return bm_ext;
}

static int _is_in_al(struct drbd_conf *mdev, unsigned int enr)
{
	struct lc_element *al_ext;
	int rv = 0;

	spin_lock_irq(&mdev->al_lock);
	if (unlikely(enr == mdev->act_log->new_number))
		rv = 1;
	else {
		al_ext = lc_find(mdev->act_log, enr);
		if (al_ext) {
			if (al_ext->refcnt)
				rv = 1;
		}
	}
	spin_unlock_irq(&mdev->al_lock);

	return rv;
}

int drbd_rs_begin_io(struct drbd_conf *mdev, sector_t sector)
{
	unsigned int enr = BM_SECT_TO_EXT(sector);
	struct bm_extent *bm_ext;
	int i, sig;
	int sa = 200; 

retry:
	sig = wait_event_interruptible(mdev->al_wait,
			(bm_ext = _bme_get(mdev, enr)));
	if (sig)
		return -EINTR;

	if (test_bit(BME_LOCKED, &bm_ext->flags))
		return 0;

	for (i = 0; i < AL_EXT_PER_BM_SECT; i++) {
		sig = wait_event_interruptible(mdev->al_wait,
					       !_is_in_al(mdev, enr * AL_EXT_PER_BM_SECT + i) ||
					       test_bit(BME_PRIORITY, &bm_ext->flags));

		if (sig || (test_bit(BME_PRIORITY, &bm_ext->flags) && sa)) {
			spin_lock_irq(&mdev->al_lock);
			if (lc_put(mdev->resync, &bm_ext->lce) == 0) {
				bm_ext->flags = 0; 
				mdev->resync_locked--;
				wake_up(&mdev->al_wait);
			}
			spin_unlock_irq(&mdev->al_lock);
			if (sig)
				return -EINTR;
			if (schedule_timeout_interruptible(HZ/10))
				return -EINTR;
			if (sa && --sa == 0)
				dev_warn(DEV,"drbd_rs_begin_io() stepped aside for 20sec."
					 "Resync stalled?\n");
			goto retry;
		}
	}
	set_bit(BME_LOCKED, &bm_ext->flags);
	return 0;
}

int drbd_try_rs_begin_io(struct drbd_conf *mdev, sector_t sector)
{
	unsigned int enr = BM_SECT_TO_EXT(sector);
	const unsigned int al_enr = enr*AL_EXT_PER_BM_SECT;
	struct lc_element *e;
	struct bm_extent *bm_ext;
	int i;

	spin_lock_irq(&mdev->al_lock);
	if (mdev->resync_wenr != LC_FREE && mdev->resync_wenr != enr) {
		e = lc_find(mdev->resync, mdev->resync_wenr);
		bm_ext = e ? lc_entry(e, struct bm_extent, lce) : NULL;
		if (bm_ext) {
			D_ASSERT(!test_bit(BME_LOCKED, &bm_ext->flags));
			D_ASSERT(test_bit(BME_NO_WRITES, &bm_ext->flags));
			clear_bit(BME_NO_WRITES, &bm_ext->flags);
			mdev->resync_wenr = LC_FREE;
			if (lc_put(mdev->resync, &bm_ext->lce) == 0)
				mdev->resync_locked--;
			wake_up(&mdev->al_wait);
		} else {
			dev_alert(DEV, "LOGIC BUG\n");
		}
	}
	
	e = lc_try_get(mdev->resync, enr);
	bm_ext = e ? lc_entry(e, struct bm_extent, lce) : NULL;
	if (bm_ext) {
		if (test_bit(BME_LOCKED, &bm_ext->flags))
			goto proceed;
		if (!test_and_set_bit(BME_NO_WRITES, &bm_ext->flags)) {
			mdev->resync_locked++;
		} else {
			bm_ext->lce.refcnt--;
			D_ASSERT(bm_ext->lce.refcnt > 0);
		}
		goto check_al;
	} else {
		
		if (mdev->resync_locked > mdev->resync->nr_elements-3)
			goto try_again;
		
		e = lc_get(mdev->resync, enr);
		bm_ext = e ? lc_entry(e, struct bm_extent, lce) : NULL;
		if (!bm_ext) {
			const unsigned long rs_flags = mdev->resync->flags;
			if (rs_flags & LC_STARVING)
				dev_warn(DEV, "Have to wait for element"
				     " (resync LRU too small?)\n");
			BUG_ON(rs_flags & LC_DIRTY);
			goto try_again;
		}
		if (bm_ext->lce.lc_number != enr) {
			bm_ext->rs_left = drbd_bm_e_weight(mdev, enr);
			bm_ext->rs_failed = 0;
			lc_changed(mdev->resync, &bm_ext->lce);
			wake_up(&mdev->al_wait);
			D_ASSERT(test_bit(BME_LOCKED, &bm_ext->flags) == 0);
		}
		set_bit(BME_NO_WRITES, &bm_ext->flags);
		D_ASSERT(bm_ext->lce.refcnt == 1);
		mdev->resync_locked++;
		goto check_al;
	}
check_al:
	for (i = 0; i < AL_EXT_PER_BM_SECT; i++) {
		if (unlikely(al_enr+i == mdev->act_log->new_number))
			goto try_again;
		if (lc_is_used(mdev->act_log, al_enr+i))
			goto try_again;
	}
	set_bit(BME_LOCKED, &bm_ext->flags);
proceed:
	mdev->resync_wenr = LC_FREE;
	spin_unlock_irq(&mdev->al_lock);
	return 0;

try_again:
	if (bm_ext)
		mdev->resync_wenr = enr;
	spin_unlock_irq(&mdev->al_lock);
	return -EAGAIN;
}

void drbd_rs_complete_io(struct drbd_conf *mdev, sector_t sector)
{
	unsigned int enr = BM_SECT_TO_EXT(sector);
	struct lc_element *e;
	struct bm_extent *bm_ext;
	unsigned long flags;

	spin_lock_irqsave(&mdev->al_lock, flags);
	e = lc_find(mdev->resync, enr);
	bm_ext = e ? lc_entry(e, struct bm_extent, lce) : NULL;
	if (!bm_ext) {
		spin_unlock_irqrestore(&mdev->al_lock, flags);
		if (__ratelimit(&drbd_ratelimit_state))
			dev_err(DEV, "drbd_rs_complete_io() called, but extent not found\n");
		return;
	}

	if (bm_ext->lce.refcnt == 0) {
		spin_unlock_irqrestore(&mdev->al_lock, flags);
		dev_err(DEV, "drbd_rs_complete_io(,%llu [=%u]) called, "
		    "but refcnt is 0!?\n",
		    (unsigned long long)sector, enr);
		return;
	}

	if (lc_put(mdev->resync, &bm_ext->lce) == 0) {
		bm_ext->flags = 0; 
		mdev->resync_locked--;
		wake_up(&mdev->al_wait);
	}

	spin_unlock_irqrestore(&mdev->al_lock, flags);
}

void drbd_rs_cancel_all(struct drbd_conf *mdev)
{
	spin_lock_irq(&mdev->al_lock);

	if (get_ldev_if_state(mdev, D_FAILED)) { 
		lc_reset(mdev->resync);
		put_ldev(mdev);
	}
	mdev->resync_locked = 0;
	mdev->resync_wenr = LC_FREE;
	spin_unlock_irq(&mdev->al_lock);
	wake_up(&mdev->al_wait);
}

int drbd_rs_del_all(struct drbd_conf *mdev)
{
	struct lc_element *e;
	struct bm_extent *bm_ext;
	int i;

	spin_lock_irq(&mdev->al_lock);

	if (get_ldev_if_state(mdev, D_FAILED)) {
		
		for (i = 0; i < mdev->resync->nr_elements; i++) {
			e = lc_element_by_index(mdev->resync, i);
			bm_ext = lc_entry(e, struct bm_extent, lce);
			if (bm_ext->lce.lc_number == LC_FREE)
				continue;
			if (bm_ext->lce.lc_number == mdev->resync_wenr) {
				dev_info(DEV, "dropping %u in drbd_rs_del_all, apparently"
				     " got 'synced' by application io\n",
				     mdev->resync_wenr);
				D_ASSERT(!test_bit(BME_LOCKED, &bm_ext->flags));
				D_ASSERT(test_bit(BME_NO_WRITES, &bm_ext->flags));
				clear_bit(BME_NO_WRITES, &bm_ext->flags);
				mdev->resync_wenr = LC_FREE;
				lc_put(mdev->resync, &bm_ext->lce);
			}
			if (bm_ext->lce.refcnt != 0) {
				dev_info(DEV, "Retrying drbd_rs_del_all() later. "
				     "refcnt=%d\n", bm_ext->lce.refcnt);
				put_ldev(mdev);
				spin_unlock_irq(&mdev->al_lock);
				return -EAGAIN;
			}
			D_ASSERT(!test_bit(BME_LOCKED, &bm_ext->flags));
			D_ASSERT(!test_bit(BME_NO_WRITES, &bm_ext->flags));
			lc_del(mdev->resync, &bm_ext->lce);
		}
		D_ASSERT(mdev->resync->used == 0);
		put_ldev(mdev);
	}
	spin_unlock_irq(&mdev->al_lock);

	return 0;
}

void drbd_rs_failed_io(struct drbd_conf *mdev, sector_t sector, int size)
{
	
	unsigned long sbnr, ebnr, lbnr;
	unsigned long count;
	sector_t esector, nr_sectors;
	int wake_up = 0;

	if (size <= 0 || (size & 0x1ff) != 0 || size > DRBD_MAX_BIO_SIZE) {
		dev_err(DEV, "drbd_rs_failed_io: sector=%llus size=%d nonsense!\n",
				(unsigned long long)sector, size);
		return;
	}
	nr_sectors = drbd_get_capacity(mdev->this_bdev);
	esector = sector + (size >> 9) - 1;

	ERR_IF(sector >= nr_sectors) return;
	ERR_IF(esector >= nr_sectors) esector = (nr_sectors-1);

	lbnr = BM_SECT_TO_BIT(nr_sectors-1);

	if (unlikely(esector < BM_SECT_PER_BIT-1))
		return;
	if (unlikely(esector == (nr_sectors-1)))
		ebnr = lbnr;
	else
		ebnr = BM_SECT_TO_BIT(esector - (BM_SECT_PER_BIT-1));
	sbnr = BM_SECT_TO_BIT(sector + BM_SECT_PER_BIT-1);

	if (sbnr > ebnr)
		return;

	spin_lock_irq(&mdev->al_lock);
	count = drbd_bm_count_bits(mdev, sbnr, ebnr);
	if (count) {
		mdev->rs_failed += count;

		if (get_ldev(mdev)) {
			drbd_try_clear_on_disk_bm(mdev, sector, count, false);
			put_ldev(mdev);
		}

		wake_up = 1;
	}
	spin_unlock_irq(&mdev->al_lock);
	if (wake_up)
		wake_up(&mdev->al_wait);
}
