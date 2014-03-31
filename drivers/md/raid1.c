/*
 * raid1.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1999, 2000, 2001 Ingo Molnar, Red Hat
 *
 * Copyright (C) 1996, 1997, 1998 Ingo Molnar, Miguel de Icaza, Gadi Oxman
 *
 * RAID-1 management functions.
 *
 * Better read-balancing code written by Mika Kuoppala <miku@iki.fi>, 2000
 *
 * Fixes to reconstruction by Jakob Østergaard" <jakob@ostenfeld.dk>
 * Various fixes by Neil Brown <neilb@cse.unsw.edu.au>
 *
 * Changes by Peter T. Breuer <ptb@it.uc3m.es> 31/1/2003 to support
 * bitmapped intelligence in resync:
 *
 *      - bitmap marked during normal i/o
 *      - bitmap used to skip nondirty blocks during sync
 *
 * Additions to bitmap code, (C) 2003-2004 Paul Clements, SteelEye Technology:
 * - persistent bitmap code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/ratelimit.h>
#include "md.h"
#include "raid1.h"
#include "bitmap.h"

#define	NR_RAID1_BIOS 256

/* When there are this many requests queue to be written by
 * the raid1 thread, we become 'congested' to provide back-pressure
 * for writeback.
 */
static int max_queued_requests = 1024;

static void allow_barrier(struct r1conf *conf);
static void lower_barrier(struct r1conf *conf);

static void * r1bio_pool_alloc(gfp_t gfp_flags, void *data)
{
	struct pool_info *pi = data;
	int size = offsetof(struct r1bio, bios[pi->raid_disks]);

	
	return kzalloc(size, gfp_flags);
}

static void r1bio_pool_free(void *r1_bio, void *data)
{
	kfree(r1_bio);
}

#define RESYNC_BLOCK_SIZE (64*1024)
#define RESYNC_SECTORS (RESYNC_BLOCK_SIZE >> 9)
#define RESYNC_PAGES ((RESYNC_BLOCK_SIZE + PAGE_SIZE-1) / PAGE_SIZE)
#define RESYNC_WINDOW (2048*1024)

static void * r1buf_pool_alloc(gfp_t gfp_flags, void *data)
{
	struct pool_info *pi = data;
	struct page *page;
	struct r1bio *r1_bio;
	struct bio *bio;
	int i, j;

	r1_bio = r1bio_pool_alloc(gfp_flags, pi);
	if (!r1_bio)
		return NULL;

	for (j = pi->raid_disks ; j-- ; ) {
		bio = bio_kmalloc(gfp_flags, RESYNC_PAGES);
		if (!bio)
			goto out_free_bio;
		r1_bio->bios[j] = bio;
	}
	if (test_bit(MD_RECOVERY_REQUESTED, &pi->mddev->recovery))
		j = pi->raid_disks;
	else
		j = 1;
	while(j--) {
		bio = r1_bio->bios[j];
		for (i = 0; i < RESYNC_PAGES; i++) {
			page = alloc_page(gfp_flags);
			if (unlikely(!page))
				goto out_free_pages;

			bio->bi_io_vec[i].bv_page = page;
			bio->bi_vcnt = i+1;
		}
	}
	
	if (!test_bit(MD_RECOVERY_REQUESTED, &pi->mddev->recovery)) {
		for (i=0; i<RESYNC_PAGES ; i++)
			for (j=1; j<pi->raid_disks; j++)
				r1_bio->bios[j]->bi_io_vec[i].bv_page =
					r1_bio->bios[0]->bi_io_vec[i].bv_page;
	}

	r1_bio->master_bio = NULL;

	return r1_bio;

out_free_pages:
	for (j=0 ; j < pi->raid_disks; j++)
		for (i=0; i < r1_bio->bios[j]->bi_vcnt ; i++)
			put_page(r1_bio->bios[j]->bi_io_vec[i].bv_page);
	j = -1;
out_free_bio:
	while (++j < pi->raid_disks)
		bio_put(r1_bio->bios[j]);
	r1bio_pool_free(r1_bio, data);
	return NULL;
}

static void r1buf_pool_free(void *__r1_bio, void *data)
{
	struct pool_info *pi = data;
	int i,j;
	struct r1bio *r1bio = __r1_bio;

	for (i = 0; i < RESYNC_PAGES; i++)
		for (j = pi->raid_disks; j-- ;) {
			if (j == 0 ||
			    r1bio->bios[j]->bi_io_vec[i].bv_page !=
			    r1bio->bios[0]->bi_io_vec[i].bv_page)
				safe_put_page(r1bio->bios[j]->bi_io_vec[i].bv_page);
		}
	for (i=0 ; i < pi->raid_disks; i++)
		bio_put(r1bio->bios[i]);

	r1bio_pool_free(r1bio, data);
}

static void put_all_bios(struct r1conf *conf, struct r1bio *r1_bio)
{
	int i;

	for (i = 0; i < conf->raid_disks * 2; i++) {
		struct bio **bio = r1_bio->bios + i;
		if (!BIO_SPECIAL(*bio))
			bio_put(*bio);
		*bio = NULL;
	}
}

static void free_r1bio(struct r1bio *r1_bio)
{
	struct r1conf *conf = r1_bio->mddev->private;

	put_all_bios(conf, r1_bio);
	mempool_free(r1_bio, conf->r1bio_pool);
}

static void put_buf(struct r1bio *r1_bio)
{
	struct r1conf *conf = r1_bio->mddev->private;
	int i;

	for (i = 0; i < conf->raid_disks * 2; i++) {
		struct bio *bio = r1_bio->bios[i];
		if (bio->bi_end_io)
			rdev_dec_pending(conf->mirrors[i].rdev, r1_bio->mddev);
	}

	mempool_free(r1_bio, conf->r1buf_pool);

	lower_barrier(conf);
}

static void reschedule_retry(struct r1bio *r1_bio)
{
	unsigned long flags;
	struct mddev *mddev = r1_bio->mddev;
	struct r1conf *conf = mddev->private;

	spin_lock_irqsave(&conf->device_lock, flags);
	list_add(&r1_bio->retry_list, &conf->retry_list);
	conf->nr_queued ++;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	wake_up(&conf->wait_barrier);
	md_wakeup_thread(mddev->thread);
}

static void call_bio_endio(struct r1bio *r1_bio)
{
	struct bio *bio = r1_bio->master_bio;
	int done;
	struct r1conf *conf = r1_bio->mddev->private;

	if (bio->bi_phys_segments) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		bio->bi_phys_segments--;
		done = (bio->bi_phys_segments == 0);
		spin_unlock_irqrestore(&conf->device_lock, flags);
	} else
		done = 1;

	if (!test_bit(R1BIO_Uptodate, &r1_bio->state))
		clear_bit(BIO_UPTODATE, &bio->bi_flags);
	if (done) {
		bio_endio(bio, 0);
		allow_barrier(conf);
	}
}

static void raid_end_bio_io(struct r1bio *r1_bio)
{
	struct bio *bio = r1_bio->master_bio;

	
	if (!test_and_set_bit(R1BIO_Returned, &r1_bio->state)) {
		pr_debug("raid1: sync end %s on sectors %llu-%llu\n",
			 (bio_data_dir(bio) == WRITE) ? "write" : "read",
			 (unsigned long long) bio->bi_sector,
			 (unsigned long long) bio->bi_sector +
			 (bio->bi_size >> 9) - 1);

		call_bio_endio(r1_bio);
	}
	free_r1bio(r1_bio);
}

static inline void update_head_pos(int disk, struct r1bio *r1_bio)
{
	struct r1conf *conf = r1_bio->mddev->private;

	conf->mirrors[disk].head_position =
		r1_bio->sector + (r1_bio->sectors);
}

static int find_bio_disk(struct r1bio *r1_bio, struct bio *bio)
{
	int mirror;
	struct r1conf *conf = r1_bio->mddev->private;
	int raid_disks = conf->raid_disks;

	for (mirror = 0; mirror < raid_disks * 2; mirror++)
		if (r1_bio->bios[mirror] == bio)
			break;

	BUG_ON(mirror == raid_disks * 2);
	update_head_pos(mirror, r1_bio);

	return mirror;
}

static void raid1_end_read_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r1bio *r1_bio = bio->bi_private;
	int mirror;
	struct r1conf *conf = r1_bio->mddev->private;

	mirror = r1_bio->read_disk;
	update_head_pos(mirror, r1_bio);

	if (uptodate)
		set_bit(R1BIO_Uptodate, &r1_bio->state);
	else {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		if (r1_bio->mddev->degraded == conf->raid_disks ||
		    (r1_bio->mddev->degraded == conf->raid_disks-1 &&
		     !test_bit(Faulty, &conf->mirrors[mirror].rdev->flags)))
			uptodate = 1;
		spin_unlock_irqrestore(&conf->device_lock, flags);
	}

	if (uptodate)
		raid_end_bio_io(r1_bio);
	else {
		char b[BDEVNAME_SIZE];
		printk_ratelimited(
			KERN_ERR "md/raid1:%s: %s: "
			"rescheduling sector %llu\n",
			mdname(conf->mddev),
			bdevname(conf->mirrors[mirror].rdev->bdev,
				 b),
			(unsigned long long)r1_bio->sector);
		set_bit(R1BIO_ReadError, &r1_bio->state);
		reschedule_retry(r1_bio);
	}

	rdev_dec_pending(conf->mirrors[mirror].rdev, conf->mddev);
}

static void close_write(struct r1bio *r1_bio)
{
	
	if (test_bit(R1BIO_BehindIO, &r1_bio->state)) {
		
		int i = r1_bio->behind_page_count;
		while (i--)
			safe_put_page(r1_bio->behind_bvecs[i].bv_page);
		kfree(r1_bio->behind_bvecs);
		r1_bio->behind_bvecs = NULL;
	}
	
	bitmap_endwrite(r1_bio->mddev->bitmap, r1_bio->sector,
			r1_bio->sectors,
			!test_bit(R1BIO_Degraded, &r1_bio->state),
			test_bit(R1BIO_BehindIO, &r1_bio->state));
	md_write_end(r1_bio->mddev);
}

static void r1_bio_write_done(struct r1bio *r1_bio)
{
	if (!atomic_dec_and_test(&r1_bio->remaining))
		return;

	if (test_bit(R1BIO_WriteError, &r1_bio->state))
		reschedule_retry(r1_bio);
	else {
		close_write(r1_bio);
		if (test_bit(R1BIO_MadeGood, &r1_bio->state))
			reschedule_retry(r1_bio);
		else
			raid_end_bio_io(r1_bio);
	}
}

static void raid1_end_write_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r1bio *r1_bio = bio->bi_private;
	int mirror, behind = test_bit(R1BIO_BehindIO, &r1_bio->state);
	struct r1conf *conf = r1_bio->mddev->private;
	struct bio *to_put = NULL;

	mirror = find_bio_disk(r1_bio, bio);

	if (!uptodate) {
		set_bit(WriteErrorSeen,
			&conf->mirrors[mirror].rdev->flags);
		if (!test_and_set_bit(WantReplacement,
				      &conf->mirrors[mirror].rdev->flags))
			set_bit(MD_RECOVERY_NEEDED, &
				conf->mddev->recovery);

		set_bit(R1BIO_WriteError, &r1_bio->state);
	} else {
		sector_t first_bad;
		int bad_sectors;

		r1_bio->bios[mirror] = NULL;
		to_put = bio;
		set_bit(R1BIO_Uptodate, &r1_bio->state);

		
		if (is_badblock(conf->mirrors[mirror].rdev,
				r1_bio->sector, r1_bio->sectors,
				&first_bad, &bad_sectors)) {
			r1_bio->bios[mirror] = IO_MADE_GOOD;
			set_bit(R1BIO_MadeGood, &r1_bio->state);
		}
	}

	if (behind) {
		if (test_bit(WriteMostly, &conf->mirrors[mirror].rdev->flags))
			atomic_dec(&r1_bio->behind_remaining);

		if (atomic_read(&r1_bio->behind_remaining) >= (atomic_read(&r1_bio->remaining)-1) &&
		    test_bit(R1BIO_Uptodate, &r1_bio->state)) {
			
			if (!test_and_set_bit(R1BIO_Returned, &r1_bio->state)) {
				struct bio *mbio = r1_bio->master_bio;
				pr_debug("raid1: behind end write sectors"
					 " %llu-%llu\n",
					 (unsigned long long) mbio->bi_sector,
					 (unsigned long long) mbio->bi_sector +
					 (mbio->bi_size >> 9) - 1);
				call_bio_endio(r1_bio);
			}
		}
	}
	if (r1_bio->bios[mirror] == NULL)
		rdev_dec_pending(conf->mirrors[mirror].rdev,
				 conf->mddev);

	r1_bio_write_done(r1_bio);

	if (to_put)
		bio_put(to_put);
}


static int read_balance(struct r1conf *conf, struct r1bio *r1_bio, int *max_sectors)
{
	const sector_t this_sector = r1_bio->sector;
	int sectors;
	int best_good_sectors;
	int start_disk;
	int best_disk;
	int i;
	sector_t best_dist;
	struct md_rdev *rdev;
	int choose_first;

	rcu_read_lock();
 retry:
	sectors = r1_bio->sectors;
	best_disk = -1;
	best_dist = MaxSector;
	best_good_sectors = 0;

	if (conf->mddev->recovery_cp < MaxSector &&
	    (this_sector + sectors >= conf->next_resync)) {
		choose_first = 1;
		start_disk = 0;
	} else {
		choose_first = 0;
		start_disk = conf->last_used;
	}

	for (i = 0 ; i < conf->raid_disks * 2 ; i++) {
		sector_t dist;
		sector_t first_bad;
		int bad_sectors;

		int disk = start_disk + i;
		if (disk >= conf->raid_disks)
			disk -= conf->raid_disks;

		rdev = rcu_dereference(conf->mirrors[disk].rdev);
		if (r1_bio->bios[disk] == IO_BLOCKED
		    || rdev == NULL
		    || test_bit(Unmerged, &rdev->flags)
		    || test_bit(Faulty, &rdev->flags))
			continue;
		if (!test_bit(In_sync, &rdev->flags) &&
		    rdev->recovery_offset < this_sector + sectors)
			continue;
		if (test_bit(WriteMostly, &rdev->flags)) {
			if (best_disk < 0) {
				if (is_badblock(rdev, this_sector, sectors,
						&first_bad, &bad_sectors)) {
					if (first_bad < this_sector)
						
						continue;
					best_good_sectors = first_bad - this_sector;
				} else
					best_good_sectors = sectors;
				best_disk = disk;
			}
			continue;
		}
		if (is_badblock(rdev, this_sector, sectors,
				&first_bad, &bad_sectors)) {
			if (best_dist < MaxSector)
				
				continue;
			if (first_bad <= this_sector) {
				bad_sectors -= (this_sector - first_bad);
				if (choose_first && sectors > bad_sectors)
					sectors = bad_sectors;
				if (best_good_sectors > sectors)
					best_good_sectors = sectors;

			} else {
				sector_t good_sectors = first_bad - this_sector;
				if (good_sectors > best_good_sectors) {
					best_good_sectors = good_sectors;
					best_disk = disk;
				}
				if (choose_first)
					break;
			}
			continue;
		} else
			best_good_sectors = sectors;

		dist = abs(this_sector - conf->mirrors[disk].head_position);
		if (choose_first
		    
		    || conf->next_seq_sect == this_sector
		    || dist == 0
		    
		    || atomic_read(&rdev->nr_pending) == 0) {
			best_disk = disk;
			break;
		}
		if (dist < best_dist) {
			best_dist = dist;
			best_disk = disk;
		}
	}

	if (best_disk >= 0) {
		rdev = rcu_dereference(conf->mirrors[best_disk].rdev);
		if (!rdev)
			goto retry;
		atomic_inc(&rdev->nr_pending);
		if (test_bit(Faulty, &rdev->flags)) {
			rdev_dec_pending(rdev, conf->mddev);
			goto retry;
		}
		sectors = best_good_sectors;
		conf->next_seq_sect = this_sector + sectors;
		conf->last_used = best_disk;
	}
	rcu_read_unlock();
	*max_sectors = sectors;

	return best_disk;
}

static int raid1_mergeable_bvec(struct request_queue *q,
				struct bvec_merge_data *bvm,
				struct bio_vec *biovec)
{
	struct mddev *mddev = q->queuedata;
	struct r1conf *conf = mddev->private;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	int max = biovec->bv_len;

	if (mddev->merge_check_needed) {
		int disk;
		rcu_read_lock();
		for (disk = 0; disk < conf->raid_disks * 2; disk++) {
			struct md_rdev *rdev = rcu_dereference(
				conf->mirrors[disk].rdev);
			if (rdev && !test_bit(Faulty, &rdev->flags)) {
				struct request_queue *q =
					bdev_get_queue(rdev->bdev);
				if (q->merge_bvec_fn) {
					bvm->bi_sector = sector +
						rdev->data_offset;
					bvm->bi_bdev = rdev->bdev;
					max = min(max, q->merge_bvec_fn(
							  q, bvm, biovec));
				}
			}
		}
		rcu_read_unlock();
	}
	return max;

}

int md_raid1_congested(struct mddev *mddev, int bits)
{
	struct r1conf *conf = mddev->private;
	int i, ret = 0;

	if ((bits & (1 << BDI_async_congested)) &&
	    conf->pending_count >= max_queued_requests)
		return 1;

	rcu_read_lock();
	for (i = 0; i < conf->raid_disks * 2; i++) {
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev && !test_bit(Faulty, &rdev->flags)) {
			struct request_queue *q = bdev_get_queue(rdev->bdev);

			BUG_ON(!q);

			if ((bits & (1<<BDI_async_congested)) || 1)
				ret |= bdi_congested(&q->backing_dev_info, bits);
			else
				ret &= bdi_congested(&q->backing_dev_info, bits);
		}
	}
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(md_raid1_congested);

static int raid1_congested(void *data, int bits)
{
	struct mddev *mddev = data;

	return mddev_congested(mddev, bits) ||
		md_raid1_congested(mddev, bits);
}

static void flush_pending_writes(struct r1conf *conf)
{
	spin_lock_irq(&conf->device_lock);

	if (conf->pending_bio_list.head) {
		struct bio *bio;
		bio = bio_list_get(&conf->pending_bio_list);
		conf->pending_count = 0;
		spin_unlock_irq(&conf->device_lock);
		bitmap_unplug(conf->mddev->bitmap);
		wake_up(&conf->wait_barrier);

		while (bio) { 
			struct bio *next = bio->bi_next;
			bio->bi_next = NULL;
			generic_make_request(bio);
			bio = next;
		}
	} else
		spin_unlock_irq(&conf->device_lock);
}

#define RESYNC_DEPTH 32

static void raise_barrier(struct r1conf *conf)
{
	spin_lock_irq(&conf->resync_lock);

	
	wait_event_lock_irq(conf->wait_barrier, !conf->nr_waiting,
			    conf->resync_lock, );

	
	conf->barrier++;

	
	wait_event_lock_irq(conf->wait_barrier,
			    !conf->nr_pending && conf->barrier < RESYNC_DEPTH,
			    conf->resync_lock, );

	spin_unlock_irq(&conf->resync_lock);
}

static void lower_barrier(struct r1conf *conf)
{
	unsigned long flags;
	BUG_ON(conf->barrier <= 0);
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->barrier--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void wait_barrier(struct r1conf *conf)
{
	spin_lock_irq(&conf->resync_lock);
	if (conf->barrier) {
		conf->nr_waiting++;
		wait_event_lock_irq(conf->wait_barrier,
				    !conf->barrier ||
				    (conf->nr_pending &&
				     current->bio_list &&
				     !bio_list_empty(current->bio_list)),
				    conf->resync_lock,
			);
		conf->nr_waiting--;
	}
	conf->nr_pending++;
	spin_unlock_irq(&conf->resync_lock);
}

static void allow_barrier(struct r1conf *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->nr_pending--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void freeze_array(struct r1conf *conf)
{
	spin_lock_irq(&conf->resync_lock);
	conf->barrier++;
	conf->nr_waiting++;
	wait_event_lock_irq(conf->wait_barrier,
			    conf->nr_pending == conf->nr_queued+1,
			    conf->resync_lock,
			    flush_pending_writes(conf));
	spin_unlock_irq(&conf->resync_lock);
}
static void unfreeze_array(struct r1conf *conf)
{
	
	spin_lock_irq(&conf->resync_lock);
	conf->barrier--;
	conf->nr_waiting--;
	wake_up(&conf->wait_barrier);
	spin_unlock_irq(&conf->resync_lock);
}


static void alloc_behind_pages(struct bio *bio, struct r1bio *r1_bio)
{
	int i;
	struct bio_vec *bvec;
	struct bio_vec *bvecs = kzalloc(bio->bi_vcnt * sizeof(struct bio_vec),
					GFP_NOIO);
	if (unlikely(!bvecs))
		return;

	bio_for_each_segment(bvec, bio, i) {
		bvecs[i] = *bvec;
		bvecs[i].bv_page = alloc_page(GFP_NOIO);
		if (unlikely(!bvecs[i].bv_page))
			goto do_sync_io;
		memcpy(kmap(bvecs[i].bv_page) + bvec->bv_offset,
		       kmap(bvec->bv_page) + bvec->bv_offset, bvec->bv_len);
		kunmap(bvecs[i].bv_page);
		kunmap(bvec->bv_page);
	}
	r1_bio->behind_bvecs = bvecs;
	r1_bio->behind_page_count = bio->bi_vcnt;
	set_bit(R1BIO_BehindIO, &r1_bio->state);
	return;

do_sync_io:
	for (i = 0; i < bio->bi_vcnt; i++)
		if (bvecs[i].bv_page)
			put_page(bvecs[i].bv_page);
	kfree(bvecs);
	pr_debug("%dB behind alloc failed, doing sync I/O\n", bio->bi_size);
}

static void make_request(struct mddev *mddev, struct bio * bio)
{
	struct r1conf *conf = mddev->private;
	struct mirror_info *mirror;
	struct r1bio *r1_bio;
	struct bio *read_bio;
	int i, disks;
	struct bitmap *bitmap;
	unsigned long flags;
	const int rw = bio_data_dir(bio);
	const unsigned long do_sync = (bio->bi_rw & REQ_SYNC);
	const unsigned long do_flush_fua = (bio->bi_rw & (REQ_FLUSH | REQ_FUA));
	struct md_rdev *blocked_rdev;
	int plugged;
	int first_clone;
	int sectors_handled;
	int max_sectors;


	md_write_start(mddev, bio); 

	if (bio_data_dir(bio) == WRITE &&
	    bio->bi_sector + bio->bi_size/512 > mddev->suspend_lo &&
	    bio->bi_sector < mddev->suspend_hi) {
		DEFINE_WAIT(w);
		for (;;) {
			flush_signals(current);
			prepare_to_wait(&conf->wait_barrier,
					&w, TASK_INTERRUPTIBLE);
			if (bio->bi_sector + bio->bi_size/512 <= mddev->suspend_lo ||
			    bio->bi_sector >= mddev->suspend_hi)
				break;
			schedule();
		}
		finish_wait(&conf->wait_barrier, &w);
	}

	wait_barrier(conf);

	bitmap = mddev->bitmap;

	r1_bio = mempool_alloc(conf->r1bio_pool, GFP_NOIO);

	r1_bio->master_bio = bio;
	r1_bio->sectors = bio->bi_size >> 9;
	r1_bio->state = 0;
	r1_bio->mddev = mddev;
	r1_bio->sector = bio->bi_sector;

	bio->bi_phys_segments = 0;
	clear_bit(BIO_SEG_VALID, &bio->bi_flags);

	if (rw == READ) {
		int rdisk;

read_again:
		rdisk = read_balance(conf, r1_bio, &max_sectors);

		if (rdisk < 0) {
			
			raid_end_bio_io(r1_bio);
			return;
		}
		mirror = conf->mirrors + rdisk;

		if (test_bit(WriteMostly, &mirror->rdev->flags) &&
		    bitmap) {
			wait_event(bitmap->behind_wait,
				   atomic_read(&bitmap->behind_writes) == 0);
		}
		r1_bio->read_disk = rdisk;

		read_bio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(read_bio, r1_bio->sector - bio->bi_sector,
			    max_sectors);

		r1_bio->bios[rdisk] = read_bio;

		read_bio->bi_sector = r1_bio->sector + mirror->rdev->data_offset;
		read_bio->bi_bdev = mirror->rdev->bdev;
		read_bio->bi_end_io = raid1_end_read_request;
		read_bio->bi_rw = READ | do_sync;
		read_bio->bi_private = r1_bio;

		if (max_sectors < r1_bio->sectors) {

			sectors_handled = (r1_bio->sector + max_sectors
					   - bio->bi_sector);
			r1_bio->sectors = max_sectors;
			spin_lock_irq(&conf->device_lock);
			if (bio->bi_phys_segments == 0)
				bio->bi_phys_segments = 2;
			else
				bio->bi_phys_segments++;
			spin_unlock_irq(&conf->device_lock);
			reschedule_retry(r1_bio);

			r1_bio = mempool_alloc(conf->r1bio_pool, GFP_NOIO);

			r1_bio->master_bio = bio;
			r1_bio->sectors = (bio->bi_size >> 9) - sectors_handled;
			r1_bio->state = 0;
			r1_bio->mddev = mddev;
			r1_bio->sector = bio->bi_sector + sectors_handled;
			goto read_again;
		} else
			generic_make_request(read_bio);
		return;
	}

	if (conf->pending_count >= max_queued_requests) {
		md_wakeup_thread(mddev->thread);
		wait_event(conf->wait_barrier,
			   conf->pending_count < max_queued_requests);
	}
	plugged = mddev_check_plugged(mddev);

	disks = conf->raid_disks * 2;
 retry_write:
	blocked_rdev = NULL;
	rcu_read_lock();
	max_sectors = r1_bio->sectors;
	for (i = 0;  i < disks; i++) {
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev && unlikely(test_bit(Blocked, &rdev->flags))) {
			atomic_inc(&rdev->nr_pending);
			blocked_rdev = rdev;
			break;
		}
		r1_bio->bios[i] = NULL;
		if (!rdev || test_bit(Faulty, &rdev->flags)
		    || test_bit(Unmerged, &rdev->flags)) {
			if (i < conf->raid_disks)
				set_bit(R1BIO_Degraded, &r1_bio->state);
			continue;
		}

		atomic_inc(&rdev->nr_pending);
		if (test_bit(WriteErrorSeen, &rdev->flags)) {
			sector_t first_bad;
			int bad_sectors;
			int is_bad;

			is_bad = is_badblock(rdev, r1_bio->sector,
					     max_sectors,
					     &first_bad, &bad_sectors);
			if (is_bad < 0) {
				set_bit(BlockedBadBlocks, &rdev->flags);
				blocked_rdev = rdev;
				break;
			}
			if (is_bad && first_bad <= r1_bio->sector) {
				
				bad_sectors -= (r1_bio->sector - first_bad);
				if (bad_sectors < max_sectors)
					max_sectors = bad_sectors;
				rdev_dec_pending(rdev, mddev);
				continue;
			}
			if (is_bad) {
				int good_sectors = first_bad - r1_bio->sector;
				if (good_sectors < max_sectors)
					max_sectors = good_sectors;
			}
		}
		r1_bio->bios[i] = bio;
	}
	rcu_read_unlock();

	if (unlikely(blocked_rdev)) {
		
		int j;

		for (j = 0; j < i; j++)
			if (r1_bio->bios[j])
				rdev_dec_pending(conf->mirrors[j].rdev, mddev);
		r1_bio->state = 0;
		allow_barrier(conf);
		md_wait_for_blocked_rdev(blocked_rdev, mddev);
		wait_barrier(conf);
		goto retry_write;
	}

	if (max_sectors < r1_bio->sectors) {
		r1_bio->sectors = max_sectors;
		spin_lock_irq(&conf->device_lock);
		if (bio->bi_phys_segments == 0)
			bio->bi_phys_segments = 2;
		else
			bio->bi_phys_segments++;
		spin_unlock_irq(&conf->device_lock);
	}
	sectors_handled = r1_bio->sector + max_sectors - bio->bi_sector;

	atomic_set(&r1_bio->remaining, 1);
	atomic_set(&r1_bio->behind_remaining, 0);

	first_clone = 1;
	for (i = 0; i < disks; i++) {
		struct bio *mbio;
		if (!r1_bio->bios[i])
			continue;

		mbio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(mbio, r1_bio->sector - bio->bi_sector, max_sectors);

		if (first_clone) {
			if (bitmap &&
			    (atomic_read(&bitmap->behind_writes)
			     < mddev->bitmap_info.max_write_behind) &&
			    !waitqueue_active(&bitmap->behind_wait))
				alloc_behind_pages(mbio, r1_bio);

			bitmap_startwrite(bitmap, r1_bio->sector,
					  r1_bio->sectors,
					  test_bit(R1BIO_BehindIO,
						   &r1_bio->state));
			first_clone = 0;
		}
		if (r1_bio->behind_bvecs) {
			struct bio_vec *bvec;
			int j;

			__bio_for_each_segment(bvec, mbio, j, 0)
				bvec->bv_page = r1_bio->behind_bvecs[j].bv_page;
			if (test_bit(WriteMostly, &conf->mirrors[i].rdev->flags))
				atomic_inc(&r1_bio->behind_remaining);
		}

		r1_bio->bios[i] = mbio;

		mbio->bi_sector	= (r1_bio->sector +
				   conf->mirrors[i].rdev->data_offset);
		mbio->bi_bdev = conf->mirrors[i].rdev->bdev;
		mbio->bi_end_io	= raid1_end_write_request;
		mbio->bi_rw = WRITE | do_flush_fua | do_sync;
		mbio->bi_private = r1_bio;

		atomic_inc(&r1_bio->remaining);
		spin_lock_irqsave(&conf->device_lock, flags);
		bio_list_add(&conf->pending_bio_list, mbio);
		conf->pending_count++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
	}
	if (sectors_handled < (bio->bi_size >> 9)) {
		r1_bio_write_done(r1_bio);
		r1_bio = mempool_alloc(conf->r1bio_pool, GFP_NOIO);
		r1_bio->master_bio = bio;
		r1_bio->sectors = (bio->bi_size >> 9) - sectors_handled;
		r1_bio->state = 0;
		r1_bio->mddev = mddev;
		r1_bio->sector = bio->bi_sector + sectors_handled;
		goto retry_write;
	}

	r1_bio_write_done(r1_bio);

	
	wake_up(&conf->wait_barrier);

	if (do_sync || !bitmap || !plugged)
		md_wakeup_thread(mddev->thread);
}

static void status(struct seq_file *seq, struct mddev *mddev)
{
	struct r1conf *conf = mddev->private;
	int i;

	seq_printf(seq, " [%d/%d] [", conf->raid_disks,
		   conf->raid_disks - mddev->degraded);
	rcu_read_lock();
	for (i = 0; i < conf->raid_disks; i++) {
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[i].rdev);
		seq_printf(seq, "%s",
			   rdev && test_bit(In_sync, &rdev->flags) ? "U" : "_");
	}
	rcu_read_unlock();
	seq_printf(seq, "]");
}


static void error(struct mddev *mddev, struct md_rdev *rdev)
{
	char b[BDEVNAME_SIZE];
	struct r1conf *conf = mddev->private;

	if (test_bit(In_sync, &rdev->flags)
	    && (conf->raid_disks - mddev->degraded) == 1) {
		conf->recovery_disabled = mddev->recovery_disabled;
		return;
	}
	set_bit(Blocked, &rdev->flags);
	if (test_and_clear_bit(In_sync, &rdev->flags)) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		mddev->degraded++;
		set_bit(Faulty, &rdev->flags);
		spin_unlock_irqrestore(&conf->device_lock, flags);
		set_bit(MD_RECOVERY_INTR, &mddev->recovery);
	} else
		set_bit(Faulty, &rdev->flags);
	set_bit(MD_CHANGE_DEVS, &mddev->flags);
	printk(KERN_ALERT
	       "md/raid1:%s: Disk failure on %s, disabling device.\n"
	       "md/raid1:%s: Operation continuing on %d devices.\n",
	       mdname(mddev), bdevname(rdev->bdev, b),
	       mdname(mddev), conf->raid_disks - mddev->degraded);
}

static void print_conf(struct r1conf *conf)
{
	int i;

	printk(KERN_DEBUG "RAID1 conf printout:\n");
	if (!conf) {
		printk(KERN_DEBUG "(!conf)\n");
		return;
	}
	printk(KERN_DEBUG " --- wd:%d rd:%d\n", conf->raid_disks - conf->mddev->degraded,
		conf->raid_disks);

	rcu_read_lock();
	for (i = 0; i < conf->raid_disks; i++) {
		char b[BDEVNAME_SIZE];
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev)
			printk(KERN_DEBUG " disk %d, wo:%d, o:%d, dev:%s\n",
			       i, !test_bit(In_sync, &rdev->flags),
			       !test_bit(Faulty, &rdev->flags),
			       bdevname(rdev->bdev,b));
	}
	rcu_read_unlock();
}

static void close_sync(struct r1conf *conf)
{
	wait_barrier(conf);
	allow_barrier(conf);

	mempool_destroy(conf->r1buf_pool);
	conf->r1buf_pool = NULL;
}

static int raid1_spare_active(struct mddev *mddev)
{
	int i;
	struct r1conf *conf = mddev->private;
	int count = 0;
	unsigned long flags;

	for (i = 0; i < conf->raid_disks; i++) {
		struct md_rdev *rdev = conf->mirrors[i].rdev;
		struct md_rdev *repl = conf->mirrors[conf->raid_disks + i].rdev;
		if (repl
		    && repl->recovery_offset == MaxSector
		    && !test_bit(Faulty, &repl->flags)
		    && !test_and_set_bit(In_sync, &repl->flags)) {
			
			if (!rdev ||
			    !test_and_clear_bit(In_sync, &rdev->flags))
				count++;
			if (rdev) {
				set_bit(Faulty, &rdev->flags);
				sysfs_notify_dirent_safe(
					rdev->sysfs_state);
			}
		}
		if (rdev
		    && !test_bit(Faulty, &rdev->flags)
		    && !test_and_set_bit(In_sync, &rdev->flags)) {
			count++;
			sysfs_notify_dirent_safe(rdev->sysfs_state);
		}
	}
	spin_lock_irqsave(&conf->device_lock, flags);
	mddev->degraded -= count;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	print_conf(conf);
	return count;
}


static int raid1_add_disk(struct mddev *mddev, struct md_rdev *rdev)
{
	struct r1conf *conf = mddev->private;
	int err = -EEXIST;
	int mirror = 0;
	struct mirror_info *p;
	int first = 0;
	int last = conf->raid_disks - 1;
	struct request_queue *q = bdev_get_queue(rdev->bdev);

	if (mddev->recovery_disabled == conf->recovery_disabled)
		return -EBUSY;

	if (rdev->raid_disk >= 0)
		first = last = rdev->raid_disk;

	if (q->merge_bvec_fn) {
		set_bit(Unmerged, &rdev->flags);
		mddev->merge_check_needed = 1;
	}

	for (mirror = first; mirror <= last; mirror++) {
		p = conf->mirrors+mirror;
		if (!p->rdev) {

			disk_stack_limits(mddev->gendisk, rdev->bdev,
					  rdev->data_offset << 9);

			p->head_position = 0;
			rdev->raid_disk = mirror;
			err = 0;
			if (rdev->saved_raid_disk < 0)
				conf->fullsync = 1;
			rcu_assign_pointer(p->rdev, rdev);
			break;
		}
		if (test_bit(WantReplacement, &p->rdev->flags) &&
		    p[conf->raid_disks].rdev == NULL) {
			
			clear_bit(In_sync, &rdev->flags);
			set_bit(Replacement, &rdev->flags);
			rdev->raid_disk = mirror;
			err = 0;
			conf->fullsync = 1;
			rcu_assign_pointer(p[conf->raid_disks].rdev, rdev);
			break;
		}
	}
	if (err == 0 && test_bit(Unmerged, &rdev->flags)) {
		synchronize_sched();
		raise_barrier(conf);
		lower_barrier(conf);
		clear_bit(Unmerged, &rdev->flags);
	}
	md_integrity_add_rdev(rdev, mddev);
	print_conf(conf);
	return err;
}

static int raid1_remove_disk(struct mddev *mddev, struct md_rdev *rdev)
{
	struct r1conf *conf = mddev->private;
	int err = 0;
	int number = rdev->raid_disk;
	struct mirror_info *p = conf->mirrors+ number;

	if (rdev != p->rdev)
		p = conf->mirrors + conf->raid_disks + number;

	print_conf(conf);
	if (rdev == p->rdev) {
		if (test_bit(In_sync, &rdev->flags) ||
		    atomic_read(&rdev->nr_pending)) {
			err = -EBUSY;
			goto abort;
		}
		if (!test_bit(Faulty, &rdev->flags) &&
		    mddev->recovery_disabled != conf->recovery_disabled &&
		    mddev->degraded < conf->raid_disks) {
			err = -EBUSY;
			goto abort;
		}
		p->rdev = NULL;
		synchronize_rcu();
		if (atomic_read(&rdev->nr_pending)) {
			
			err = -EBUSY;
			p->rdev = rdev;
			goto abort;
		} else if (conf->mirrors[conf->raid_disks + number].rdev) {
			struct md_rdev *repl =
				conf->mirrors[conf->raid_disks + number].rdev;
			raise_barrier(conf);
			clear_bit(Replacement, &repl->flags);
			p->rdev = repl;
			conf->mirrors[conf->raid_disks + number].rdev = NULL;
			lower_barrier(conf);
			clear_bit(WantReplacement, &rdev->flags);
		} else
			clear_bit(WantReplacement, &rdev->flags);
		err = md_integrity_register(mddev);
	}
abort:

	print_conf(conf);
	return err;
}


static void end_sync_read(struct bio *bio, int error)
{
	struct r1bio *r1_bio = bio->bi_private;

	update_head_pos(r1_bio->read_disk, r1_bio);

	/*
	 * we have read a block, now it needs to be re-written,
	 * or re-read if the read failed.
	 * We don't do much here, just schedule handling by raid1d
	 */
	if (test_bit(BIO_UPTODATE, &bio->bi_flags))
		set_bit(R1BIO_Uptodate, &r1_bio->state);

	if (atomic_dec_and_test(&r1_bio->remaining))
		reschedule_retry(r1_bio);
}

static void end_sync_write(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r1bio *r1_bio = bio->bi_private;
	struct mddev *mddev = r1_bio->mddev;
	struct r1conf *conf = mddev->private;
	int mirror=0;
	sector_t first_bad;
	int bad_sectors;

	mirror = find_bio_disk(r1_bio, bio);

	if (!uptodate) {
		sector_t sync_blocks = 0;
		sector_t s = r1_bio->sector;
		long sectors_to_go = r1_bio->sectors;
		
		do {
			bitmap_end_sync(mddev->bitmap, s,
					&sync_blocks, 1);
			s += sync_blocks;
			sectors_to_go -= sync_blocks;
		} while (sectors_to_go > 0);
		set_bit(WriteErrorSeen,
			&conf->mirrors[mirror].rdev->flags);
		if (!test_and_set_bit(WantReplacement,
				      &conf->mirrors[mirror].rdev->flags))
			set_bit(MD_RECOVERY_NEEDED, &
				mddev->recovery);
		set_bit(R1BIO_WriteError, &r1_bio->state);
	} else if (is_badblock(conf->mirrors[mirror].rdev,
			       r1_bio->sector,
			       r1_bio->sectors,
			       &first_bad, &bad_sectors) &&
		   !is_badblock(conf->mirrors[r1_bio->read_disk].rdev,
				r1_bio->sector,
				r1_bio->sectors,
				&first_bad, &bad_sectors)
		)
		set_bit(R1BIO_MadeGood, &r1_bio->state);

	if (atomic_dec_and_test(&r1_bio->remaining)) {
		int s = r1_bio->sectors;
		if (test_bit(R1BIO_MadeGood, &r1_bio->state) ||
		    test_bit(R1BIO_WriteError, &r1_bio->state))
			reschedule_retry(r1_bio);
		else {
			put_buf(r1_bio);
			md_done_sync(mddev, s, uptodate);
		}
	}
}

static int r1_sync_page_io(struct md_rdev *rdev, sector_t sector,
			    int sectors, struct page *page, int rw)
{
	if (sync_page_io(rdev, sector, sectors << 9, page, rw, false))
		
		return 1;
	if (rw == WRITE) {
		set_bit(WriteErrorSeen, &rdev->flags);
		if (!test_and_set_bit(WantReplacement,
				      &rdev->flags))
			set_bit(MD_RECOVERY_NEEDED, &
				rdev->mddev->recovery);
	}
	
	if (!rdev_set_badblocks(rdev, sector, sectors, 0))
		md_error(rdev->mddev, rdev);
	return 0;
}

static int fix_sync_read_error(struct r1bio *r1_bio)
{
	struct mddev *mddev = r1_bio->mddev;
	struct r1conf *conf = mddev->private;
	struct bio *bio = r1_bio->bios[r1_bio->read_disk];
	sector_t sect = r1_bio->sector;
	int sectors = r1_bio->sectors;
	int idx = 0;

	while(sectors) {
		int s = sectors;
		int d = r1_bio->read_disk;
		int success = 0;
		struct md_rdev *rdev;
		int start;

		if (s > (PAGE_SIZE>>9))
			s = PAGE_SIZE >> 9;
		do {
			if (r1_bio->bios[d]->bi_end_io == end_sync_read) {
				rdev = conf->mirrors[d].rdev;
				if (sync_page_io(rdev, sect, s<<9,
						 bio->bi_io_vec[idx].bv_page,
						 READ, false)) {
					success = 1;
					break;
				}
			}
			d++;
			if (d == conf->raid_disks * 2)
				d = 0;
		} while (!success && d != r1_bio->read_disk);

		if (!success) {
			char b[BDEVNAME_SIZE];
			int abort = 0;
			printk(KERN_ALERT "md/raid1:%s: %s: unrecoverable I/O read error"
			       " for block %llu\n",
			       mdname(mddev),
			       bdevname(bio->bi_bdev, b),
			       (unsigned long long)r1_bio->sector);
			for (d = 0; d < conf->raid_disks * 2; d++) {
				rdev = conf->mirrors[d].rdev;
				if (!rdev || test_bit(Faulty, &rdev->flags))
					continue;
				if (!rdev_set_badblocks(rdev, sect, s, 0))
					abort = 1;
			}
			if (abort) {
				conf->recovery_disabled =
					mddev->recovery_disabled;
				set_bit(MD_RECOVERY_INTR, &mddev->recovery);
				md_done_sync(mddev, r1_bio->sectors, 0);
				put_buf(r1_bio);
				return 0;
			}
			
			sectors -= s;
			sect += s;
			idx++;
			continue;
		}

		start = d;
		
		while (d != r1_bio->read_disk) {
			if (d == 0)
				d = conf->raid_disks * 2;
			d--;
			if (r1_bio->bios[d]->bi_end_io != end_sync_read)
				continue;
			rdev = conf->mirrors[d].rdev;
			if (r1_sync_page_io(rdev, sect, s,
					    bio->bi_io_vec[idx].bv_page,
					    WRITE) == 0) {
				r1_bio->bios[d]->bi_end_io = NULL;
				rdev_dec_pending(rdev, mddev);
			}
		}
		d = start;
		while (d != r1_bio->read_disk) {
			if (d == 0)
				d = conf->raid_disks * 2;
			d--;
			if (r1_bio->bios[d]->bi_end_io != end_sync_read)
				continue;
			rdev = conf->mirrors[d].rdev;
			if (r1_sync_page_io(rdev, sect, s,
					    bio->bi_io_vec[idx].bv_page,
					    READ) != 0)
				atomic_add(s, &rdev->corrected_errors);
		}
		sectors -= s;
		sect += s;
		idx ++;
	}
	set_bit(R1BIO_Uptodate, &r1_bio->state);
	set_bit(BIO_UPTODATE, &bio->bi_flags);
	return 1;
}

static int process_checks(struct r1bio *r1_bio)
{
	struct mddev *mddev = r1_bio->mddev;
	struct r1conf *conf = mddev->private;
	int primary;
	int i;
	int vcnt;

	for (primary = 0; primary < conf->raid_disks * 2; primary++)
		if (r1_bio->bios[primary]->bi_end_io == end_sync_read &&
		    test_bit(BIO_UPTODATE, &r1_bio->bios[primary]->bi_flags)) {
			r1_bio->bios[primary]->bi_end_io = NULL;
			rdev_dec_pending(conf->mirrors[primary].rdev, mddev);
			break;
		}
	r1_bio->read_disk = primary;
	vcnt = (r1_bio->sectors + PAGE_SIZE / 512 - 1) >> (PAGE_SHIFT - 9);
	for (i = 0; i < conf->raid_disks * 2; i++) {
		int j;
		struct bio *pbio = r1_bio->bios[primary];
		struct bio *sbio = r1_bio->bios[i];
		int size;

		if (r1_bio->bios[i]->bi_end_io != end_sync_read)
			continue;

		if (test_bit(BIO_UPTODATE, &sbio->bi_flags)) {
			for (j = vcnt; j-- ; ) {
				struct page *p, *s;
				p = pbio->bi_io_vec[j].bv_page;
				s = sbio->bi_io_vec[j].bv_page;
				if (memcmp(page_address(p),
					   page_address(s),
					   sbio->bi_io_vec[j].bv_len))
					break;
			}
		} else
			j = 0;
		if (j >= 0)
			mddev->resync_mismatches += r1_bio->sectors;
		if (j < 0 || (test_bit(MD_RECOVERY_CHECK, &mddev->recovery)
			      && test_bit(BIO_UPTODATE, &sbio->bi_flags))) {
			
			sbio->bi_end_io = NULL;
			rdev_dec_pending(conf->mirrors[i].rdev, mddev);
			continue;
		}
		
		sbio->bi_vcnt = vcnt;
		sbio->bi_size = r1_bio->sectors << 9;
		sbio->bi_idx = 0;
		sbio->bi_phys_segments = 0;
		sbio->bi_flags &= ~(BIO_POOL_MASK - 1);
		sbio->bi_flags |= 1 << BIO_UPTODATE;
		sbio->bi_next = NULL;
		sbio->bi_sector = r1_bio->sector +
			conf->mirrors[i].rdev->data_offset;
		sbio->bi_bdev = conf->mirrors[i].rdev->bdev;
		size = sbio->bi_size;
		for (j = 0; j < vcnt ; j++) {
			struct bio_vec *bi;
			bi = &sbio->bi_io_vec[j];
			bi->bv_offset = 0;
			if (size > PAGE_SIZE)
				bi->bv_len = PAGE_SIZE;
			else
				bi->bv_len = size;
			size -= PAGE_SIZE;
			memcpy(page_address(bi->bv_page),
			       page_address(pbio->bi_io_vec[j].bv_page),
			       PAGE_SIZE);
		}
	}
	return 0;
}

static void sync_request_write(struct mddev *mddev, struct r1bio *r1_bio)
{
	struct r1conf *conf = mddev->private;
	int i;
	int disks = conf->raid_disks * 2;
	struct bio *bio, *wbio;

	bio = r1_bio->bios[r1_bio->read_disk];

	if (!test_bit(R1BIO_Uptodate, &r1_bio->state))
		
		if (!fix_sync_read_error(r1_bio))
			return;

	if (test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery))
		if (process_checks(r1_bio) < 0)
			return;
	atomic_set(&r1_bio->remaining, 1);
	for (i = 0; i < disks ; i++) {
		wbio = r1_bio->bios[i];
		if (wbio->bi_end_io == NULL ||
		    (wbio->bi_end_io == end_sync_read &&
		     (i == r1_bio->read_disk ||
		      !test_bit(MD_RECOVERY_SYNC, &mddev->recovery))))
			continue;

		wbio->bi_rw = WRITE;
		wbio->bi_end_io = end_sync_write;
		atomic_inc(&r1_bio->remaining);
		md_sync_acct(conf->mirrors[i].rdev->bdev, wbio->bi_size >> 9);

		generic_make_request(wbio);
	}

	if (atomic_dec_and_test(&r1_bio->remaining)) {
		
		md_done_sync(mddev, r1_bio->sectors, 1);
		put_buf(r1_bio);
	}
}


static void fix_read_error(struct r1conf *conf, int read_disk,
			   sector_t sect, int sectors)
{
	struct mddev *mddev = conf->mddev;
	while(sectors) {
		int s = sectors;
		int d = read_disk;
		int success = 0;
		int start;
		struct md_rdev *rdev;

		if (s > (PAGE_SIZE>>9))
			s = PAGE_SIZE >> 9;

		do {
			sector_t first_bad;
			int bad_sectors;

			rdev = conf->mirrors[d].rdev;
			if (rdev &&
			    test_bit(In_sync, &rdev->flags) &&
			    is_badblock(rdev, sect, s,
					&first_bad, &bad_sectors) == 0 &&
			    sync_page_io(rdev, sect, s<<9,
					 conf->tmppage, READ, false))
				success = 1;
			else {
				d++;
				if (d == conf->raid_disks * 2)
					d = 0;
			}
		} while (!success && d != read_disk);

		if (!success) {
			
			struct md_rdev *rdev = conf->mirrors[read_disk].rdev;
			if (!rdev_set_badblocks(rdev, sect, s, 0))
				md_error(mddev, rdev);
			break;
		}
		
		start = d;
		while (d != read_disk) {
			if (d==0)
				d = conf->raid_disks * 2;
			d--;
			rdev = conf->mirrors[d].rdev;
			if (rdev &&
			    test_bit(In_sync, &rdev->flags))
				r1_sync_page_io(rdev, sect, s,
						conf->tmppage, WRITE);
		}
		d = start;
		while (d != read_disk) {
			char b[BDEVNAME_SIZE];
			if (d==0)
				d = conf->raid_disks * 2;
			d--;
			rdev = conf->mirrors[d].rdev;
			if (rdev &&
			    test_bit(In_sync, &rdev->flags)) {
				if (r1_sync_page_io(rdev, sect, s,
						    conf->tmppage, READ)) {
					atomic_add(s, &rdev->corrected_errors);
					printk(KERN_INFO
					       "md/raid1:%s: read error corrected "
					       "(%d sectors at %llu on %s)\n",
					       mdname(mddev), s,
					       (unsigned long long)(sect +
					           rdev->data_offset),
					       bdevname(rdev->bdev, b));
				}
			}
		}
		sectors -= s;
		sect += s;
	}
}

static void bi_complete(struct bio *bio, int error)
{
	complete((struct completion *)bio->bi_private);
}

static int submit_bio_wait(int rw, struct bio *bio)
{
	struct completion event;
	rw |= REQ_SYNC;

	init_completion(&event);
	bio->bi_private = &event;
	bio->bi_end_io = bi_complete;
	submit_bio(rw, bio);
	wait_for_completion(&event);

	return test_bit(BIO_UPTODATE, &bio->bi_flags);
}

static int narrow_write_error(struct r1bio *r1_bio, int i)
{
	struct mddev *mddev = r1_bio->mddev;
	struct r1conf *conf = mddev->private;
	struct md_rdev *rdev = conf->mirrors[i].rdev;
	int vcnt, idx;
	struct bio_vec *vec;

	/* bio has the data to be written to device 'i' where
	 * we just recently had a write error.
	 * We repeatedly clone the bio and trim down to one block,
	 * then try the write.  Where the write fails we record
	 * a bad block.
	 * It is conceivable that the bio doesn't exactly align with
	 * blocks.  We must handle this somehow.
	 *
	 * We currently own a reference on the rdev.
	 */

	int block_sectors;
	sector_t sector;
	int sectors;
	int sect_to_write = r1_bio->sectors;
	int ok = 1;

	if (rdev->badblocks.shift < 0)
		return 0;

	block_sectors = 1 << rdev->badblocks.shift;
	sector = r1_bio->sector;
	sectors = ((sector + block_sectors)
		   & ~(sector_t)(block_sectors - 1))
		- sector;

	if (test_bit(R1BIO_BehindIO, &r1_bio->state)) {
		vcnt = r1_bio->behind_page_count;
		vec = r1_bio->behind_bvecs;
		idx = 0;
		while (vec[idx].bv_page == NULL)
			idx++;
	} else {
		vcnt = r1_bio->master_bio->bi_vcnt;
		vec = r1_bio->master_bio->bi_io_vec;
		idx = r1_bio->master_bio->bi_idx;
	}
	while (sect_to_write) {
		struct bio *wbio;
		if (sectors > sect_to_write)
			sectors = sect_to_write;
		

		wbio = bio_alloc_mddev(GFP_NOIO, vcnt, mddev);
		memcpy(wbio->bi_io_vec, vec, vcnt * sizeof(struct bio_vec));
		wbio->bi_sector = r1_bio->sector;
		wbio->bi_rw = WRITE;
		wbio->bi_vcnt = vcnt;
		wbio->bi_size = r1_bio->sectors << 9;
		wbio->bi_idx = idx;

		md_trim_bio(wbio, sector - r1_bio->sector, sectors);
		wbio->bi_sector += rdev->data_offset;
		wbio->bi_bdev = rdev->bdev;
		if (submit_bio_wait(WRITE, wbio) == 0)
			
			ok = rdev_set_badblocks(rdev, sector,
						sectors, 0)
				&& ok;

		bio_put(wbio);
		sect_to_write -= sectors;
		sector += sectors;
		sectors = block_sectors;
	}
	return ok;
}

static void handle_sync_write_finished(struct r1conf *conf, struct r1bio *r1_bio)
{
	int m;
	int s = r1_bio->sectors;
	for (m = 0; m < conf->raid_disks * 2 ; m++) {
		struct md_rdev *rdev = conf->mirrors[m].rdev;
		struct bio *bio = r1_bio->bios[m];
		if (bio->bi_end_io == NULL)
			continue;
		if (test_bit(BIO_UPTODATE, &bio->bi_flags) &&
		    test_bit(R1BIO_MadeGood, &r1_bio->state)) {
			rdev_clear_badblocks(rdev, r1_bio->sector, s);
		}
		if (!test_bit(BIO_UPTODATE, &bio->bi_flags) &&
		    test_bit(R1BIO_WriteError, &r1_bio->state)) {
			if (!rdev_set_badblocks(rdev, r1_bio->sector, s, 0))
				md_error(conf->mddev, rdev);
		}
	}
	put_buf(r1_bio);
	md_done_sync(conf->mddev, s, 1);
}

static void handle_write_finished(struct r1conf *conf, struct r1bio *r1_bio)
{
	int m;
	for (m = 0; m < conf->raid_disks * 2 ; m++)
		if (r1_bio->bios[m] == IO_MADE_GOOD) {
			struct md_rdev *rdev = conf->mirrors[m].rdev;
			rdev_clear_badblocks(rdev,
					     r1_bio->sector,
					     r1_bio->sectors);
			rdev_dec_pending(rdev, conf->mddev);
		} else if (r1_bio->bios[m] != NULL) {
			if (!narrow_write_error(r1_bio, m)) {
				md_error(conf->mddev,
					 conf->mirrors[m].rdev);
				
				set_bit(R1BIO_Degraded, &r1_bio->state);
			}
			rdev_dec_pending(conf->mirrors[m].rdev,
					 conf->mddev);
		}
	if (test_bit(R1BIO_WriteError, &r1_bio->state))
		close_write(r1_bio);
	raid_end_bio_io(r1_bio);
}

static void handle_read_error(struct r1conf *conf, struct r1bio *r1_bio)
{
	int disk;
	int max_sectors;
	struct mddev *mddev = conf->mddev;
	struct bio *bio;
	char b[BDEVNAME_SIZE];
	struct md_rdev *rdev;

	clear_bit(R1BIO_ReadError, &r1_bio->state);
	if (mddev->ro == 0) {
		freeze_array(conf);
		fix_read_error(conf, r1_bio->read_disk,
			       r1_bio->sector, r1_bio->sectors);
		unfreeze_array(conf);
	} else
		md_error(mddev, conf->mirrors[r1_bio->read_disk].rdev);

	bio = r1_bio->bios[r1_bio->read_disk];
	bdevname(bio->bi_bdev, b);
read_more:
	disk = read_balance(conf, r1_bio, &max_sectors);
	if (disk == -1) {
		printk(KERN_ALERT "md/raid1:%s: %s: unrecoverable I/O"
		       " read error for block %llu\n",
		       mdname(mddev), b, (unsigned long long)r1_bio->sector);
		raid_end_bio_io(r1_bio);
	} else {
		const unsigned long do_sync
			= r1_bio->master_bio->bi_rw & REQ_SYNC;
		if (bio) {
			r1_bio->bios[r1_bio->read_disk] =
				mddev->ro ? IO_BLOCKED : NULL;
			bio_put(bio);
		}
		r1_bio->read_disk = disk;
		bio = bio_clone_mddev(r1_bio->master_bio, GFP_NOIO, mddev);
		md_trim_bio(bio, r1_bio->sector - bio->bi_sector, max_sectors);
		r1_bio->bios[r1_bio->read_disk] = bio;
		rdev = conf->mirrors[disk].rdev;
		printk_ratelimited(KERN_ERR
				   "md/raid1:%s: redirecting sector %llu"
				   " to other mirror: %s\n",
				   mdname(mddev),
				   (unsigned long long)r1_bio->sector,
				   bdevname(rdev->bdev, b));
		bio->bi_sector = r1_bio->sector + rdev->data_offset;
		bio->bi_bdev = rdev->bdev;
		bio->bi_end_io = raid1_end_read_request;
		bio->bi_rw = READ | do_sync;
		bio->bi_private = r1_bio;
		if (max_sectors < r1_bio->sectors) {
			
			struct bio *mbio = r1_bio->master_bio;
			int sectors_handled = (r1_bio->sector + max_sectors
					       - mbio->bi_sector);
			r1_bio->sectors = max_sectors;
			spin_lock_irq(&conf->device_lock);
			if (mbio->bi_phys_segments == 0)
				mbio->bi_phys_segments = 2;
			else
				mbio->bi_phys_segments++;
			spin_unlock_irq(&conf->device_lock);
			generic_make_request(bio);
			bio = NULL;

			r1_bio = mempool_alloc(conf->r1bio_pool, GFP_NOIO);

			r1_bio->master_bio = mbio;
			r1_bio->sectors = (mbio->bi_size >> 9)
					  - sectors_handled;
			r1_bio->state = 0;
			set_bit(R1BIO_ReadError, &r1_bio->state);
			r1_bio->mddev = mddev;
			r1_bio->sector = mbio->bi_sector + sectors_handled;

			goto read_more;
		} else
			generic_make_request(bio);
	}
}

static void raid1d(struct mddev *mddev)
{
	struct r1bio *r1_bio;
	unsigned long flags;
	struct r1conf *conf = mddev->private;
	struct list_head *head = &conf->retry_list;
	struct blk_plug plug;

	md_check_recovery(mddev);

	blk_start_plug(&plug);
	for (;;) {

		if (atomic_read(&mddev->plug_cnt) == 0)
			flush_pending_writes(conf);

		spin_lock_irqsave(&conf->device_lock, flags);
		if (list_empty(head)) {
			spin_unlock_irqrestore(&conf->device_lock, flags);
			break;
		}
		r1_bio = list_entry(head->prev, struct r1bio, retry_list);
		list_del(head->prev);
		conf->nr_queued--;
		spin_unlock_irqrestore(&conf->device_lock, flags);

		mddev = r1_bio->mddev;
		conf = mddev->private;
		if (test_bit(R1BIO_IsSync, &r1_bio->state)) {
			if (test_bit(R1BIO_MadeGood, &r1_bio->state) ||
			    test_bit(R1BIO_WriteError, &r1_bio->state))
				handle_sync_write_finished(conf, r1_bio);
			else
				sync_request_write(mddev, r1_bio);
		} else if (test_bit(R1BIO_MadeGood, &r1_bio->state) ||
			   test_bit(R1BIO_WriteError, &r1_bio->state))
			handle_write_finished(conf, r1_bio);
		else if (test_bit(R1BIO_ReadError, &r1_bio->state))
			handle_read_error(conf, r1_bio);
		else
			generic_make_request(r1_bio->bios[r1_bio->read_disk]);

		cond_resched();
		if (mddev->flags & ~(1<<MD_CHANGE_PENDING))
			md_check_recovery(mddev);
	}
	blk_finish_plug(&plug);
}


static int init_resync(struct r1conf *conf)
{
	int buffs;

	buffs = RESYNC_WINDOW / RESYNC_BLOCK_SIZE;
	BUG_ON(conf->r1buf_pool);
	conf->r1buf_pool = mempool_create(buffs, r1buf_pool_alloc, r1buf_pool_free,
					  conf->poolinfo);
	if (!conf->r1buf_pool)
		return -ENOMEM;
	conf->next_resync = 0;
	return 0;
}


static sector_t sync_request(struct mddev *mddev, sector_t sector_nr, int *skipped, int go_faster)
{
	struct r1conf *conf = mddev->private;
	struct r1bio *r1_bio;
	struct bio *bio;
	sector_t max_sector, nr_sectors;
	int disk = -1;
	int i;
	int wonly = -1;
	int write_targets = 0, read_targets = 0;
	sector_t sync_blocks;
	int still_degraded = 0;
	int good_sectors = RESYNC_SECTORS;
	int min_bad = 0; 

	if (!conf->r1buf_pool)
		if (init_resync(conf))
			return 0;

	max_sector = mddev->dev_sectors;
	if (sector_nr >= max_sector) {
		if (mddev->curr_resync < max_sector) 
			bitmap_end_sync(mddev->bitmap, mddev->curr_resync,
						&sync_blocks, 1);
		else 
			conf->fullsync = 0;

		bitmap_close_sync(mddev->bitmap);
		close_sync(conf);
		return 0;
	}

	if (mddev->bitmap == NULL &&
	    mddev->recovery_cp == MaxSector &&
	    !test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery) &&
	    conf->fullsync == 0) {
		*skipped = 1;
		return max_sector - sector_nr;
	}
	if (!bitmap_start_sync(mddev->bitmap, sector_nr, &sync_blocks, 1) &&
	    !conf->fullsync && !test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery)) {
		
		*skipped = 1;
		return sync_blocks;
	}
	if (!go_faster && conf->nr_waiting)
		msleep_interruptible(1000);

	bitmap_cond_end_sync(mddev->bitmap, sector_nr);
	r1_bio = mempool_alloc(conf->r1buf_pool, GFP_NOIO);
	raise_barrier(conf);

	conf->next_resync = sector_nr;

	rcu_read_lock();

	r1_bio->mddev = mddev;
	r1_bio->sector = sector_nr;
	r1_bio->state = 0;
	set_bit(R1BIO_IsSync, &r1_bio->state);

	for (i = 0; i < conf->raid_disks * 2; i++) {
		struct md_rdev *rdev;
		bio = r1_bio->bios[i];

		
		bio->bi_next = NULL;
		bio->bi_flags &= ~(BIO_POOL_MASK-1);
		bio->bi_flags |= 1 << BIO_UPTODATE;
		bio->bi_rw = READ;
		bio->bi_vcnt = 0;
		bio->bi_idx = 0;
		bio->bi_phys_segments = 0;
		bio->bi_size = 0;
		bio->bi_end_io = NULL;
		bio->bi_private = NULL;

		rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev == NULL ||
		    test_bit(Faulty, &rdev->flags)) {
			if (i < conf->raid_disks)
				still_degraded = 1;
		} else if (!test_bit(In_sync, &rdev->flags)) {
			bio->bi_rw = WRITE;
			bio->bi_end_io = end_sync_write;
			write_targets ++;
		} else {
			
			sector_t first_bad = MaxSector;
			int bad_sectors;

			if (is_badblock(rdev, sector_nr, good_sectors,
					&first_bad, &bad_sectors)) {
				if (first_bad > sector_nr)
					good_sectors = first_bad - sector_nr;
				else {
					bad_sectors -= (sector_nr - first_bad);
					if (min_bad == 0 ||
					    min_bad > bad_sectors)
						min_bad = bad_sectors;
				}
			}
			if (sector_nr < first_bad) {
				if (test_bit(WriteMostly, &rdev->flags)) {
					if (wonly < 0)
						wonly = i;
				} else {
					if (disk < 0)
						disk = i;
				}
				bio->bi_rw = READ;
				bio->bi_end_io = end_sync_read;
				read_targets++;
			}
		}
		if (bio->bi_end_io) {
			atomic_inc(&rdev->nr_pending);
			bio->bi_sector = sector_nr + rdev->data_offset;
			bio->bi_bdev = rdev->bdev;
			bio->bi_private = r1_bio;
		}
	}
	rcu_read_unlock();
	if (disk < 0)
		disk = wonly;
	r1_bio->read_disk = disk;

	if (read_targets == 0 && min_bad > 0) {
		int ok = 1;
		for (i = 0 ; i < conf->raid_disks * 2 ; i++)
			if (r1_bio->bios[i]->bi_end_io == end_sync_write) {
				struct md_rdev *rdev = conf->mirrors[i].rdev;
				ok = rdev_set_badblocks(rdev, sector_nr,
							min_bad, 0
					) && ok;
			}
		set_bit(MD_CHANGE_DEVS, &mddev->flags);
		*skipped = 1;
		put_buf(r1_bio);

		if (!ok) {
			conf->recovery_disabled = mddev->recovery_disabled;
			set_bit(MD_RECOVERY_INTR, &mddev->recovery);
			return 0;
		} else
			return min_bad;

	}
	if (min_bad > 0 && min_bad < good_sectors) {
		good_sectors = min_bad;
	}

	if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery) && read_targets > 0)
		
		write_targets += read_targets-1;

	if (write_targets == 0 || read_targets == 0) {
		sector_t rv = max_sector - sector_nr;
		*skipped = 1;
		put_buf(r1_bio);
		return rv;
	}

	if (max_sector > mddev->resync_max)
		max_sector = mddev->resync_max; 
	if (max_sector > sector_nr + good_sectors)
		max_sector = sector_nr + good_sectors;
	nr_sectors = 0;
	sync_blocks = 0;
	do {
		struct page *page;
		int len = PAGE_SIZE;
		if (sector_nr + (len>>9) > max_sector)
			len = (max_sector - sector_nr) << 9;
		if (len == 0)
			break;
		if (sync_blocks == 0) {
			if (!bitmap_start_sync(mddev->bitmap, sector_nr,
					       &sync_blocks, still_degraded) &&
			    !conf->fullsync &&
			    !test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery))
				break;
			BUG_ON(sync_blocks < (PAGE_SIZE>>9));
			if ((len >> 9) > sync_blocks)
				len = sync_blocks<<9;
		}

		for (i = 0 ; i < conf->raid_disks * 2; i++) {
			bio = r1_bio->bios[i];
			if (bio->bi_end_io) {
				page = bio->bi_io_vec[bio->bi_vcnt].bv_page;
				if (bio_add_page(bio, page, len, 0) == 0) {
					
					bio->bi_io_vec[bio->bi_vcnt].bv_page = page;
					while (i > 0) {
						i--;
						bio = r1_bio->bios[i];
						if (bio->bi_end_io==NULL)
							continue;
						
						bio->bi_vcnt--;
						bio->bi_size -= len;
						bio->bi_flags &= ~(1<< BIO_SEG_VALID);
					}
					goto bio_full;
				}
			}
		}
		nr_sectors += len>>9;
		sector_nr += len>>9;
		sync_blocks -= (len>>9);
	} while (r1_bio->bios[disk]->bi_vcnt < RESYNC_PAGES);
 bio_full:
	r1_bio->sectors = nr_sectors;

	if (test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery)) {
		atomic_set(&r1_bio->remaining, read_targets);
		for (i = 0; i < conf->raid_disks * 2; i++) {
			bio = r1_bio->bios[i];
			if (bio->bi_end_io == end_sync_read) {
				md_sync_acct(bio->bi_bdev, nr_sectors);
				generic_make_request(bio);
			}
		}
	} else {
		atomic_set(&r1_bio->remaining, 1);
		bio = r1_bio->bios[r1_bio->read_disk];
		md_sync_acct(bio->bi_bdev, nr_sectors);
		generic_make_request(bio);

	}
	return nr_sectors;
}

static sector_t raid1_size(struct mddev *mddev, sector_t sectors, int raid_disks)
{
	if (sectors)
		return sectors;

	return mddev->dev_sectors;
}

static struct r1conf *setup_conf(struct mddev *mddev)
{
	struct r1conf *conf;
	int i;
	struct mirror_info *disk;
	struct md_rdev *rdev;
	int err = -ENOMEM;

	conf = kzalloc(sizeof(struct r1conf), GFP_KERNEL);
	if (!conf)
		goto abort;

	conf->mirrors = kzalloc(sizeof(struct mirror_info)
				* mddev->raid_disks * 2,
				 GFP_KERNEL);
	if (!conf->mirrors)
		goto abort;

	conf->tmppage = alloc_page(GFP_KERNEL);
	if (!conf->tmppage)
		goto abort;

	conf->poolinfo = kzalloc(sizeof(*conf->poolinfo), GFP_KERNEL);
	if (!conf->poolinfo)
		goto abort;
	conf->poolinfo->raid_disks = mddev->raid_disks * 2;
	conf->r1bio_pool = mempool_create(NR_RAID1_BIOS, r1bio_pool_alloc,
					  r1bio_pool_free,
					  conf->poolinfo);
	if (!conf->r1bio_pool)
		goto abort;

	conf->poolinfo->mddev = mddev;

	err = -EINVAL;
	spin_lock_init(&conf->device_lock);
	rdev_for_each(rdev, mddev) {
		int disk_idx = rdev->raid_disk;
		if (disk_idx >= mddev->raid_disks
		    || disk_idx < 0)
			continue;
		if (test_bit(Replacement, &rdev->flags))
			disk = conf->mirrors + conf->raid_disks + disk_idx;
		else
			disk = conf->mirrors + disk_idx;

		if (disk->rdev)
			goto abort;
		disk->rdev = rdev;

		disk->head_position = 0;
	}
	conf->raid_disks = mddev->raid_disks;
	conf->mddev = mddev;
	INIT_LIST_HEAD(&conf->retry_list);

	spin_lock_init(&conf->resync_lock);
	init_waitqueue_head(&conf->wait_barrier);

	bio_list_init(&conf->pending_bio_list);
	conf->pending_count = 0;
	conf->recovery_disabled = mddev->recovery_disabled - 1;

	err = -EIO;
	conf->last_used = -1;
	for (i = 0; i < conf->raid_disks * 2; i++) {

		disk = conf->mirrors + i;

		if (i < conf->raid_disks &&
		    disk[conf->raid_disks].rdev) {
			
			if (!disk->rdev) {
				disk->rdev =
					disk[conf->raid_disks].rdev;
				disk[conf->raid_disks].rdev = NULL;
			} else if (!test_bit(In_sync, &disk->rdev->flags))
				
				goto abort;
		}

		if (!disk->rdev ||
		    !test_bit(In_sync, &disk->rdev->flags)) {
			disk->head_position = 0;
			if (disk->rdev)
				conf->fullsync = 1;
		} else if (conf->last_used < 0)
			conf->last_used = i;
	}

	if (conf->last_used < 0) {
		printk(KERN_ERR "md/raid1:%s: no operational mirrors\n",
		       mdname(mddev));
		goto abort;
	}
	err = -ENOMEM;
	conf->thread = md_register_thread(raid1d, mddev, NULL);
	if (!conf->thread) {
		printk(KERN_ERR
		       "md/raid1:%s: couldn't allocate thread\n",
		       mdname(mddev));
		goto abort;
	}

	return conf;

 abort:
	if (conf) {
		if (conf->r1bio_pool)
			mempool_destroy(conf->r1bio_pool);
		kfree(conf->mirrors);
		safe_put_page(conf->tmppage);
		kfree(conf->poolinfo);
		kfree(conf);
	}
	return ERR_PTR(err);
}

static int stop(struct mddev *mddev);
static int run(struct mddev *mddev)
{
	struct r1conf *conf;
	int i;
	struct md_rdev *rdev;
	int ret;

	if (mddev->level != 1) {
		printk(KERN_ERR "md/raid1:%s: raid level not set to mirroring (%d)\n",
		       mdname(mddev), mddev->level);
		return -EIO;
	}
	if (mddev->reshape_position != MaxSector) {
		printk(KERN_ERR "md/raid1:%s: reshape_position set but not supported\n",
		       mdname(mddev));
		return -EIO;
	}
	if (mddev->private == NULL)
		conf = setup_conf(mddev);
	else
		conf = mddev->private;

	if (IS_ERR(conf))
		return PTR_ERR(conf);

	rdev_for_each(rdev, mddev) {
		if (!mddev->gendisk)
			continue;
		disk_stack_limits(mddev->gendisk, rdev->bdev,
				  rdev->data_offset << 9);
	}

	mddev->degraded = 0;
	for (i=0; i < conf->raid_disks; i++)
		if (conf->mirrors[i].rdev == NULL ||
		    !test_bit(In_sync, &conf->mirrors[i].rdev->flags) ||
		    test_bit(Faulty, &conf->mirrors[i].rdev->flags))
			mddev->degraded++;

	if (conf->raid_disks - mddev->degraded == 1)
		mddev->recovery_cp = MaxSector;

	if (mddev->recovery_cp != MaxSector)
		printk(KERN_NOTICE "md/raid1:%s: not clean"
		       " -- starting background reconstruction\n",
		       mdname(mddev));
	printk(KERN_INFO 
		"md/raid1:%s: active with %d out of %d mirrors\n",
		mdname(mddev), mddev->raid_disks - mddev->degraded, 
		mddev->raid_disks);

	mddev->thread = conf->thread;
	conf->thread = NULL;
	mddev->private = conf;

	md_set_array_sectors(mddev, raid1_size(mddev, 0, 0));

	if (mddev->queue) {
		mddev->queue->backing_dev_info.congested_fn = raid1_congested;
		mddev->queue->backing_dev_info.congested_data = mddev;
		blk_queue_merge_bvec(mddev->queue, raid1_mergeable_bvec);
	}

	ret =  md_integrity_register(mddev);
	if (ret)
		stop(mddev);
	return ret;
}

static int stop(struct mddev *mddev)
{
	struct r1conf *conf = mddev->private;
	struct bitmap *bitmap = mddev->bitmap;

	
	if (bitmap && atomic_read(&bitmap->behind_writes) > 0) {
		printk(KERN_INFO "md/raid1:%s: behind writes in progress - waiting to stop.\n",
		       mdname(mddev));
		
		wait_event(bitmap->behind_wait,
			   atomic_read(&bitmap->behind_writes) == 0);
	}

	raise_barrier(conf);
	lower_barrier(conf);

	md_unregister_thread(&mddev->thread);
	if (conf->r1bio_pool)
		mempool_destroy(conf->r1bio_pool);
	kfree(conf->mirrors);
	kfree(conf->poolinfo);
	kfree(conf);
	mddev->private = NULL;
	return 0;
}

static int raid1_resize(struct mddev *mddev, sector_t sectors)
{
	md_set_array_sectors(mddev, raid1_size(mddev, sectors, 0));
	if (mddev->array_sectors > raid1_size(mddev, sectors, 0))
		return -EINVAL;
	set_capacity(mddev->gendisk, mddev->array_sectors);
	revalidate_disk(mddev->gendisk);
	if (sectors > mddev->dev_sectors &&
	    mddev->recovery_cp > mddev->dev_sectors) {
		mddev->recovery_cp = mddev->dev_sectors;
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	}
	mddev->dev_sectors = sectors;
	mddev->resync_max_sectors = sectors;
	return 0;
}

static int raid1_reshape(struct mddev *mddev)
{
	mempool_t *newpool, *oldpool;
	struct pool_info *newpoolinfo;
	struct mirror_info *newmirrors;
	struct r1conf *conf = mddev->private;
	int cnt, raid_disks;
	unsigned long flags;
	int d, d2, err;

	
	if (mddev->chunk_sectors != mddev->new_chunk_sectors ||
	    mddev->layout != mddev->new_layout ||
	    mddev->level != mddev->new_level) {
		mddev->new_chunk_sectors = mddev->chunk_sectors;
		mddev->new_layout = mddev->layout;
		mddev->new_level = mddev->level;
		return -EINVAL;
	}

	err = md_allow_write(mddev);
	if (err)
		return err;

	raid_disks = mddev->raid_disks + mddev->delta_disks;

	if (raid_disks < conf->raid_disks) {
		cnt=0;
		for (d= 0; d < conf->raid_disks; d++)
			if (conf->mirrors[d].rdev)
				cnt++;
		if (cnt > raid_disks)
			return -EBUSY;
	}

	newpoolinfo = kmalloc(sizeof(*newpoolinfo), GFP_KERNEL);
	if (!newpoolinfo)
		return -ENOMEM;
	newpoolinfo->mddev = mddev;
	newpoolinfo->raid_disks = raid_disks * 2;

	newpool = mempool_create(NR_RAID1_BIOS, r1bio_pool_alloc,
				 r1bio_pool_free, newpoolinfo);
	if (!newpool) {
		kfree(newpoolinfo);
		return -ENOMEM;
	}
	newmirrors = kzalloc(sizeof(struct mirror_info) * raid_disks * 2,
			     GFP_KERNEL);
	if (!newmirrors) {
		kfree(newpoolinfo);
		mempool_destroy(newpool);
		return -ENOMEM;
	}

	raise_barrier(conf);

	
	oldpool = conf->r1bio_pool;
	conf->r1bio_pool = newpool;

	for (d = d2 = 0; d < conf->raid_disks; d++) {
		struct md_rdev *rdev = conf->mirrors[d].rdev;
		if (rdev && rdev->raid_disk != d2) {
			sysfs_unlink_rdev(mddev, rdev);
			rdev->raid_disk = d2;
			sysfs_unlink_rdev(mddev, rdev);
			if (sysfs_link_rdev(mddev, rdev))
				printk(KERN_WARNING
				       "md/raid1:%s: cannot register rd%d\n",
				       mdname(mddev), rdev->raid_disk);
		}
		if (rdev)
			newmirrors[d2++].rdev = rdev;
	}
	kfree(conf->mirrors);
	conf->mirrors = newmirrors;
	kfree(conf->poolinfo);
	conf->poolinfo = newpoolinfo;

	spin_lock_irqsave(&conf->device_lock, flags);
	mddev->degraded += (raid_disks - conf->raid_disks);
	spin_unlock_irqrestore(&conf->device_lock, flags);
	conf->raid_disks = mddev->raid_disks = raid_disks;
	mddev->delta_disks = 0;

	conf->last_used = 0; 
	lower_barrier(conf);

	set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	md_wakeup_thread(mddev->thread);

	mempool_destroy(oldpool);
	return 0;
}

static void raid1_quiesce(struct mddev *mddev, int state)
{
	struct r1conf *conf = mddev->private;

	switch(state) {
	case 2: 
		wake_up(&conf->wait_barrier);
		break;
	case 1:
		raise_barrier(conf);
		break;
	case 0:
		lower_barrier(conf);
		break;
	}
}

static void *raid1_takeover(struct mddev *mddev)
{
	if (mddev->level == 5 && mddev->raid_disks == 2) {
		struct r1conf *conf;
		mddev->new_level = 1;
		mddev->new_layout = 0;
		mddev->new_chunk_sectors = 0;
		conf = setup_conf(mddev);
		if (!IS_ERR(conf))
			conf->barrier = 1;
		return conf;
	}
	return ERR_PTR(-EINVAL);
}

static struct md_personality raid1_personality =
{
	.name		= "raid1",
	.level		= 1,
	.owner		= THIS_MODULE,
	.make_request	= make_request,
	.run		= run,
	.stop		= stop,
	.status		= status,
	.error_handler	= error,
	.hot_add_disk	= raid1_add_disk,
	.hot_remove_disk= raid1_remove_disk,
	.spare_active	= raid1_spare_active,
	.sync_request	= sync_request,
	.resize		= raid1_resize,
	.size		= raid1_size,
	.check_reshape	= raid1_reshape,
	.quiesce	= raid1_quiesce,
	.takeover	= raid1_takeover,
};

static int __init raid_init(void)
{
	return register_md_personality(&raid1_personality);
}

static void raid_exit(void)
{
	unregister_md_personality(&raid1_personality);
}

module_init(raid_init);
module_exit(raid_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RAID1 (mirroring) personality for MD");
MODULE_ALIAS("md-personality-3"); 
MODULE_ALIAS("md-raid1");
MODULE_ALIAS("md-level-1");

module_param(max_queued_requests, int, S_IRUGO|S_IWUSR);
