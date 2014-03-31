/*
 * raid10.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 2000-2004 Neil Brown
 *
 * RAID-10 support for md.
 *
 * Base on code in raid1.c.  See raid1.c for further copyright information.
 *
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
#include "raid10.h"
#include "raid0.h"
#include "bitmap.h"


#define	NR_RAID10_BIOS 256

/* When there are this many requests queue to be written by
 * the raid10 thread, we become 'congested' to provide back-pressure
 * for writeback.
 */
static int max_queued_requests = 1024;

static void allow_barrier(struct r10conf *conf);
static void lower_barrier(struct r10conf *conf);
static int enough(struct r10conf *conf, int ignore);

static void * r10bio_pool_alloc(gfp_t gfp_flags, void *data)
{
	struct r10conf *conf = data;
	int size = offsetof(struct r10bio, devs[conf->copies]);

	return kzalloc(size, gfp_flags);
}

static void r10bio_pool_free(void *r10_bio, void *data)
{
	kfree(r10_bio);
}

#define RESYNC_BLOCK_SIZE (64*1024)
#define RESYNC_PAGES ((RESYNC_BLOCK_SIZE + PAGE_SIZE-1) / PAGE_SIZE)
#define RESYNC_WINDOW (1024*1024)
#define RESYNC_DEPTH (32*1024*1024/RESYNC_BLOCK_SIZE)

static void * r10buf_pool_alloc(gfp_t gfp_flags, void *data)
{
	struct r10conf *conf = data;
	struct page *page;
	struct r10bio *r10_bio;
	struct bio *bio;
	int i, j;
	int nalloc;

	r10_bio = r10bio_pool_alloc(gfp_flags, conf);
	if (!r10_bio)
		return NULL;

	if (test_bit(MD_RECOVERY_SYNC, &conf->mddev->recovery))
		nalloc = conf->copies; 
	else
		nalloc = 2; 

	for (j = nalloc ; j-- ; ) {
		bio = bio_kmalloc(gfp_flags, RESYNC_PAGES);
		if (!bio)
			goto out_free_bio;
		r10_bio->devs[j].bio = bio;
		if (!conf->have_replacement)
			continue;
		bio = bio_kmalloc(gfp_flags, RESYNC_PAGES);
		if (!bio)
			goto out_free_bio;
		r10_bio->devs[j].repl_bio = bio;
	}
	for (j = 0 ; j < nalloc; j++) {
		struct bio *rbio = r10_bio->devs[j].repl_bio;
		bio = r10_bio->devs[j].bio;
		for (i = 0; i < RESYNC_PAGES; i++) {
			if (j == 1 && !test_bit(MD_RECOVERY_SYNC,
						&conf->mddev->recovery)) {
				
				struct bio *rbio = r10_bio->devs[0].bio;
				page = rbio->bi_io_vec[i].bv_page;
				get_page(page);
			} else
				page = alloc_page(gfp_flags);
			if (unlikely(!page))
				goto out_free_pages;

			bio->bi_io_vec[i].bv_page = page;
			if (rbio)
				rbio->bi_io_vec[i].bv_page = page;
		}
	}

	return r10_bio;

out_free_pages:
	for ( ; i > 0 ; i--)
		safe_put_page(bio->bi_io_vec[i-1].bv_page);
	while (j--)
		for (i = 0; i < RESYNC_PAGES ; i++)
			safe_put_page(r10_bio->devs[j].bio->bi_io_vec[i].bv_page);
	j = -1;
out_free_bio:
	while (++j < nalloc) {
		bio_put(r10_bio->devs[j].bio);
		if (r10_bio->devs[j].repl_bio)
			bio_put(r10_bio->devs[j].repl_bio);
	}
	r10bio_pool_free(r10_bio, conf);
	return NULL;
}

static void r10buf_pool_free(void *__r10_bio, void *data)
{
	int i;
	struct r10conf *conf = data;
	struct r10bio *r10bio = __r10_bio;
	int j;

	for (j=0; j < conf->copies; j++) {
		struct bio *bio = r10bio->devs[j].bio;
		if (bio) {
			for (i = 0; i < RESYNC_PAGES; i++) {
				safe_put_page(bio->bi_io_vec[i].bv_page);
				bio->bi_io_vec[i].bv_page = NULL;
			}
			bio_put(bio);
		}
		bio = r10bio->devs[j].repl_bio;
		if (bio)
			bio_put(bio);
	}
	r10bio_pool_free(r10bio, conf);
}

static void put_all_bios(struct r10conf *conf, struct r10bio *r10_bio)
{
	int i;

	for (i = 0; i < conf->copies; i++) {
		struct bio **bio = & r10_bio->devs[i].bio;
		if (!BIO_SPECIAL(*bio))
			bio_put(*bio);
		*bio = NULL;
		bio = &r10_bio->devs[i].repl_bio;
		if (r10_bio->read_slot < 0 && !BIO_SPECIAL(*bio))
			bio_put(*bio);
		*bio = NULL;
	}
}

static void free_r10bio(struct r10bio *r10_bio)
{
	struct r10conf *conf = r10_bio->mddev->private;

	put_all_bios(conf, r10_bio);
	mempool_free(r10_bio, conf->r10bio_pool);
}

static void put_buf(struct r10bio *r10_bio)
{
	struct r10conf *conf = r10_bio->mddev->private;

	mempool_free(r10_bio, conf->r10buf_pool);

	lower_barrier(conf);
}

static void reschedule_retry(struct r10bio *r10_bio)
{
	unsigned long flags;
	struct mddev *mddev = r10_bio->mddev;
	struct r10conf *conf = mddev->private;

	spin_lock_irqsave(&conf->device_lock, flags);
	list_add(&r10_bio->retry_list, &conf->retry_list);
	conf->nr_queued ++;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	
	wake_up(&conf->wait_barrier);

	md_wakeup_thread(mddev->thread);
}

static void raid_end_bio_io(struct r10bio *r10_bio)
{
	struct bio *bio = r10_bio->master_bio;
	int done;
	struct r10conf *conf = r10_bio->mddev->private;

	if (bio->bi_phys_segments) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		bio->bi_phys_segments--;
		done = (bio->bi_phys_segments == 0);
		spin_unlock_irqrestore(&conf->device_lock, flags);
	} else
		done = 1;
	if (!test_bit(R10BIO_Uptodate, &r10_bio->state))
		clear_bit(BIO_UPTODATE, &bio->bi_flags);
	if (done) {
		bio_endio(bio, 0);
		allow_barrier(conf);
	}
	free_r10bio(r10_bio);
}

static inline void update_head_pos(int slot, struct r10bio *r10_bio)
{
	struct r10conf *conf = r10_bio->mddev->private;

	conf->mirrors[r10_bio->devs[slot].devnum].head_position =
		r10_bio->devs[slot].addr + (r10_bio->sectors);
}

static int find_bio_disk(struct r10conf *conf, struct r10bio *r10_bio,
			 struct bio *bio, int *slotp, int *replp)
{
	int slot;
	int repl = 0;

	for (slot = 0; slot < conf->copies; slot++) {
		if (r10_bio->devs[slot].bio == bio)
			break;
		if (r10_bio->devs[slot].repl_bio == bio) {
			repl = 1;
			break;
		}
	}

	BUG_ON(slot == conf->copies);
	update_head_pos(slot, r10_bio);

	if (slotp)
		*slotp = slot;
	if (replp)
		*replp = repl;
	return r10_bio->devs[slot].devnum;
}

static void raid10_end_read_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r10bio *r10_bio = bio->bi_private;
	int slot, dev;
	struct md_rdev *rdev;
	struct r10conf *conf = r10_bio->mddev->private;


	slot = r10_bio->read_slot;
	dev = r10_bio->devs[slot].devnum;
	rdev = r10_bio->devs[slot].rdev;
	update_head_pos(slot, r10_bio);

	if (uptodate) {
		set_bit(R10BIO_Uptodate, &r10_bio->state);
	} else {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		if (!enough(conf, rdev->raid_disk))
			uptodate = 1;
		spin_unlock_irqrestore(&conf->device_lock, flags);
	}
	if (uptodate) {
		raid_end_bio_io(r10_bio);
		rdev_dec_pending(rdev, conf->mddev);
	} else {
		char b[BDEVNAME_SIZE];
		printk_ratelimited(KERN_ERR
				   "md/raid10:%s: %s: rescheduling sector %llu\n",
				   mdname(conf->mddev),
				   bdevname(rdev->bdev, b),
				   (unsigned long long)r10_bio->sector);
		set_bit(R10BIO_ReadError, &r10_bio->state);
		reschedule_retry(r10_bio);
	}
}

static void close_write(struct r10bio *r10_bio)
{
	
	bitmap_endwrite(r10_bio->mddev->bitmap, r10_bio->sector,
			r10_bio->sectors,
			!test_bit(R10BIO_Degraded, &r10_bio->state),
			0);
	md_write_end(r10_bio->mddev);
}

static void one_write_done(struct r10bio *r10_bio)
{
	if (atomic_dec_and_test(&r10_bio->remaining)) {
		if (test_bit(R10BIO_WriteError, &r10_bio->state))
			reschedule_retry(r10_bio);
		else {
			close_write(r10_bio);
			if (test_bit(R10BIO_MadeGood, &r10_bio->state))
				reschedule_retry(r10_bio);
			else
				raid_end_bio_io(r10_bio);
		}
	}
}

static void raid10_end_write_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r10bio *r10_bio = bio->bi_private;
	int dev;
	int dec_rdev = 1;
	struct r10conf *conf = r10_bio->mddev->private;
	int slot, repl;
	struct md_rdev *rdev = NULL;

	dev = find_bio_disk(conf, r10_bio, bio, &slot, &repl);

	if (repl)
		rdev = conf->mirrors[dev].replacement;
	if (!rdev) {
		smp_rmb();
		repl = 0;
		rdev = conf->mirrors[dev].rdev;
	}
	if (!uptodate) {
		if (repl)
			md_error(rdev->mddev, rdev);
		else {
			set_bit(WriteErrorSeen,	&rdev->flags);
			if (!test_and_set_bit(WantReplacement, &rdev->flags))
				set_bit(MD_RECOVERY_NEEDED,
					&rdev->mddev->recovery);
			set_bit(R10BIO_WriteError, &r10_bio->state);
			dec_rdev = 0;
		}
	} else {
		sector_t first_bad;
		int bad_sectors;

		set_bit(R10BIO_Uptodate, &r10_bio->state);

		
		if (is_badblock(rdev,
				r10_bio->devs[slot].addr,
				r10_bio->sectors,
				&first_bad, &bad_sectors)) {
			bio_put(bio);
			if (repl)
				r10_bio->devs[slot].repl_bio = IO_MADE_GOOD;
			else
				r10_bio->devs[slot].bio = IO_MADE_GOOD;
			dec_rdev = 0;
			set_bit(R10BIO_MadeGood, &r10_bio->state);
		}
	}

	one_write_done(r10_bio);
	if (dec_rdev)
		rdev_dec_pending(conf->mirrors[dev].rdev, conf->mddev);
}


static void raid10_find_phys(struct r10conf *conf, struct r10bio *r10bio)
{
	int n,f;
	sector_t sector;
	sector_t chunk;
	sector_t stripe;
	int dev;

	int slot = 0;

	
	chunk = r10bio->sector >> conf->chunk_shift;
	sector = r10bio->sector & conf->chunk_mask;

	chunk *= conf->near_copies;
	stripe = chunk;
	dev = sector_div(stripe, conf->raid_disks);
	if (conf->far_offset)
		stripe *= conf->far_copies;

	sector += stripe << conf->chunk_shift;

	
	for (n=0; n < conf->near_copies; n++) {
		int d = dev;
		sector_t s = sector;
		r10bio->devs[slot].addr = sector;
		r10bio->devs[slot].devnum = d;
		slot++;

		for (f = 1; f < conf->far_copies; f++) {
			d += conf->near_copies;
			if (d >= conf->raid_disks)
				d -= conf->raid_disks;
			s += conf->stride;
			r10bio->devs[slot].devnum = d;
			r10bio->devs[slot].addr = s;
			slot++;
		}
		dev++;
		if (dev >= conf->raid_disks) {
			dev = 0;
			sector += (conf->chunk_mask + 1);
		}
	}
	BUG_ON(slot != conf->copies);
}

static sector_t raid10_find_virt(struct r10conf *conf, sector_t sector, int dev)
{
	sector_t offset, chunk, vchunk;

	offset = sector & conf->chunk_mask;
	if (conf->far_offset) {
		int fc;
		chunk = sector >> conf->chunk_shift;
		fc = sector_div(chunk, conf->far_copies);
		dev -= fc * conf->near_copies;
		if (dev < 0)
			dev += conf->raid_disks;
	} else {
		while (sector >= conf->stride) {
			sector -= conf->stride;
			if (dev < conf->near_copies)
				dev += conf->raid_disks - conf->near_copies;
			else
				dev -= conf->near_copies;
		}
		chunk = sector >> conf->chunk_shift;
	}
	vchunk = chunk * conf->raid_disks + dev;
	sector_div(vchunk, conf->near_copies);
	return (vchunk << conf->chunk_shift) + offset;
}

static int raid10_mergeable_bvec(struct request_queue *q,
				 struct bvec_merge_data *bvm,
				 struct bio_vec *biovec)
{
	struct mddev *mddev = q->queuedata;
	struct r10conf *conf = mddev->private;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	int max;
	unsigned int chunk_sectors = mddev->chunk_sectors;
	unsigned int bio_sectors = bvm->bi_size >> 9;

	if (conf->near_copies < conf->raid_disks) {
		max = (chunk_sectors - ((sector & (chunk_sectors - 1))
					+ bio_sectors)) << 9;
		if (max < 0)
			
			max = 0;
		if (max <= biovec->bv_len && bio_sectors == 0)
			return biovec->bv_len;
	} else
		max = biovec->bv_len;

	if (mddev->merge_check_needed) {
		struct r10bio r10_bio;
		int s;
		r10_bio.sector = sector;
		raid10_find_phys(conf, &r10_bio);
		rcu_read_lock();
		for (s = 0; s < conf->copies; s++) {
			int disk = r10_bio.devs[s].devnum;
			struct md_rdev *rdev = rcu_dereference(
				conf->mirrors[disk].rdev);
			if (rdev && !test_bit(Faulty, &rdev->flags)) {
				struct request_queue *q =
					bdev_get_queue(rdev->bdev);
				if (q->merge_bvec_fn) {
					bvm->bi_sector = r10_bio.devs[s].addr
						+ rdev->data_offset;
					bvm->bi_bdev = rdev->bdev;
					max = min(max, q->merge_bvec_fn(
							  q, bvm, biovec));
				}
			}
			rdev = rcu_dereference(conf->mirrors[disk].replacement);
			if (rdev && !test_bit(Faulty, &rdev->flags)) {
				struct request_queue *q =
					bdev_get_queue(rdev->bdev);
				if (q->merge_bvec_fn) {
					bvm->bi_sector = r10_bio.devs[s].addr
						+ rdev->data_offset;
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


static struct md_rdev *read_balance(struct r10conf *conf,
				    struct r10bio *r10_bio,
				    int *max_sectors)
{
	const sector_t this_sector = r10_bio->sector;
	int disk, slot;
	int sectors = r10_bio->sectors;
	int best_good_sectors;
	sector_t new_distance, best_dist;
	struct md_rdev *rdev, *best_rdev;
	int do_balance;
	int best_slot;

	raid10_find_phys(conf, r10_bio);
	rcu_read_lock();
retry:
	sectors = r10_bio->sectors;
	best_slot = -1;
	best_rdev = NULL;
	best_dist = MaxSector;
	best_good_sectors = 0;
	do_balance = 1;
	if (conf->mddev->recovery_cp < MaxSector
	    && (this_sector + sectors >= conf->next_resync))
		do_balance = 0;

	for (slot = 0; slot < conf->copies ; slot++) {
		sector_t first_bad;
		int bad_sectors;
		sector_t dev_sector;

		if (r10_bio->devs[slot].bio == IO_BLOCKED)
			continue;
		disk = r10_bio->devs[slot].devnum;
		rdev = rcu_dereference(conf->mirrors[disk].replacement);
		if (rdev == NULL || test_bit(Faulty, &rdev->flags) ||
		    test_bit(Unmerged, &rdev->flags) ||
		    r10_bio->devs[slot].addr + sectors > rdev->recovery_offset)
			rdev = rcu_dereference(conf->mirrors[disk].rdev);
		if (rdev == NULL ||
		    test_bit(Faulty, &rdev->flags) ||
		    test_bit(Unmerged, &rdev->flags))
			continue;
		if (!test_bit(In_sync, &rdev->flags) &&
		    r10_bio->devs[slot].addr + sectors > rdev->recovery_offset)
			continue;

		dev_sector = r10_bio->devs[slot].addr;
		if (is_badblock(rdev, dev_sector, sectors,
				&first_bad, &bad_sectors)) {
			if (best_dist < MaxSector)
				
				continue;
			if (first_bad <= dev_sector) {
				bad_sectors -= (dev_sector - first_bad);
				if (!do_balance && sectors > bad_sectors)
					sectors = bad_sectors;
				if (best_good_sectors > sectors)
					best_good_sectors = sectors;
			} else {
				sector_t good_sectors =
					first_bad - dev_sector;
				if (good_sectors > best_good_sectors) {
					best_good_sectors = good_sectors;
					best_slot = slot;
					best_rdev = rdev;
				}
				if (!do_balance)
					
					break;
			}
			continue;
		} else
			best_good_sectors = sectors;

		if (!do_balance)
			break;

		if (conf->near_copies > 1 && !atomic_read(&rdev->nr_pending))
			break;

		
		if (conf->far_copies > 1)
			new_distance = r10_bio->devs[slot].addr;
		else
			new_distance = abs(r10_bio->devs[slot].addr -
					   conf->mirrors[disk].head_position);
		if (new_distance < best_dist) {
			best_dist = new_distance;
			best_slot = slot;
			best_rdev = rdev;
		}
	}
	if (slot >= conf->copies) {
		slot = best_slot;
		rdev = best_rdev;
	}

	if (slot >= 0) {
		atomic_inc(&rdev->nr_pending);
		if (test_bit(Faulty, &rdev->flags)) {
			rdev_dec_pending(rdev, conf->mddev);
			goto retry;
		}
		r10_bio->read_slot = slot;
	} else
		rdev = NULL;
	rcu_read_unlock();
	*max_sectors = best_good_sectors;

	return rdev;
}

static int raid10_congested(void *data, int bits)
{
	struct mddev *mddev = data;
	struct r10conf *conf = mddev->private;
	int i, ret = 0;

	if ((bits & (1 << BDI_async_congested)) &&
	    conf->pending_count >= max_queued_requests)
		return 1;

	if (mddev_congested(mddev, bits))
		return 1;
	rcu_read_lock();
	for (i = 0; i < conf->raid_disks && ret == 0; i++) {
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev && !test_bit(Faulty, &rdev->flags)) {
			struct request_queue *q = bdev_get_queue(rdev->bdev);

			ret |= bdi_congested(&q->backing_dev_info, bits);
		}
	}
	rcu_read_unlock();
	return ret;
}

static void flush_pending_writes(struct r10conf *conf)
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


static void raise_barrier(struct r10conf *conf, int force)
{
	BUG_ON(force && !conf->barrier);
	spin_lock_irq(&conf->resync_lock);

	
	wait_event_lock_irq(conf->wait_barrier, force || !conf->nr_waiting,
			    conf->resync_lock, );

	
	conf->barrier++;

	
	wait_event_lock_irq(conf->wait_barrier,
			    !conf->nr_pending && conf->barrier < RESYNC_DEPTH,
			    conf->resync_lock, );

	spin_unlock_irq(&conf->resync_lock);
}

static void lower_barrier(struct r10conf *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->barrier--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void wait_barrier(struct r10conf *conf)
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

static void allow_barrier(struct r10conf *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->nr_pending--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void freeze_array(struct r10conf *conf)
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

static void unfreeze_array(struct r10conf *conf)
{
	
	spin_lock_irq(&conf->resync_lock);
	conf->barrier--;
	conf->nr_waiting--;
	wake_up(&conf->wait_barrier);
	spin_unlock_irq(&conf->resync_lock);
}

static void make_request(struct mddev *mddev, struct bio * bio)
{
	struct r10conf *conf = mddev->private;
	struct r10bio *r10_bio;
	struct bio *read_bio;
	int i;
	int chunk_sects = conf->chunk_mask + 1;
	const int rw = bio_data_dir(bio);
	const unsigned long do_sync = (bio->bi_rw & REQ_SYNC);
	const unsigned long do_fua = (bio->bi_rw & REQ_FUA);
	unsigned long flags;
	struct md_rdev *blocked_rdev;
	int plugged;
	int sectors_handled;
	int max_sectors;

	if (unlikely(bio->bi_rw & REQ_FLUSH)) {
		md_flush_request(mddev, bio);
		return;
	}

	if (unlikely( (bio->bi_sector & conf->chunk_mask) + (bio->bi_size >> 9)
		      > chunk_sects &&
		    conf->near_copies < conf->raid_disks)) {
		struct bio_pair *bp;
		
		if (bio->bi_vcnt != 1 ||
		    bio->bi_idx != 0)
			goto bad_map;
		bp = bio_split(bio,
			       chunk_sects - (bio->bi_sector & (chunk_sects - 1)) );

		spin_lock_irq(&conf->resync_lock);
		conf->nr_waiting++;
		spin_unlock_irq(&conf->resync_lock);

		make_request(mddev, &bp->bio1);
		make_request(mddev, &bp->bio2);

		spin_lock_irq(&conf->resync_lock);
		conf->nr_waiting--;
		wake_up(&conf->wait_barrier);
		spin_unlock_irq(&conf->resync_lock);

		bio_pair_release(bp);
		return;
	bad_map:
		printk("md/raid10:%s: make_request bug: can't convert block across chunks"
		       " or bigger than %dk %llu %d\n", mdname(mddev), chunk_sects/2,
		       (unsigned long long)bio->bi_sector, bio->bi_size >> 10);

		bio_io_error(bio);
		return;
	}

	md_write_start(mddev, bio);

	wait_barrier(conf);

	r10_bio = mempool_alloc(conf->r10bio_pool, GFP_NOIO);

	r10_bio->master_bio = bio;
	r10_bio->sectors = bio->bi_size >> 9;

	r10_bio->mddev = mddev;
	r10_bio->sector = bio->bi_sector;
	r10_bio->state = 0;

	bio->bi_phys_segments = 0;
	clear_bit(BIO_SEG_VALID, &bio->bi_flags);

	if (rw == READ) {
		struct md_rdev *rdev;
		int slot;

read_again:
		rdev = read_balance(conf, r10_bio, &max_sectors);
		if (!rdev) {
			raid_end_bio_io(r10_bio);
			return;
		}
		slot = r10_bio->read_slot;

		read_bio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(read_bio, r10_bio->sector - bio->bi_sector,
			    max_sectors);

		r10_bio->devs[slot].bio = read_bio;
		r10_bio->devs[slot].rdev = rdev;

		read_bio->bi_sector = r10_bio->devs[slot].addr +
			rdev->data_offset;
		read_bio->bi_bdev = rdev->bdev;
		read_bio->bi_end_io = raid10_end_read_request;
		read_bio->bi_rw = READ | do_sync;
		read_bio->bi_private = r10_bio;

		if (max_sectors < r10_bio->sectors) {
			sectors_handled = (r10_bio->sectors + max_sectors
					   - bio->bi_sector);
			r10_bio->sectors = max_sectors;
			spin_lock_irq(&conf->device_lock);
			if (bio->bi_phys_segments == 0)
				bio->bi_phys_segments = 2;
			else
				bio->bi_phys_segments++;
			spin_unlock(&conf->device_lock);
			reschedule_retry(r10_bio);

			r10_bio = mempool_alloc(conf->r10bio_pool, GFP_NOIO);

			r10_bio->master_bio = bio;
			r10_bio->sectors = ((bio->bi_size >> 9)
					    - sectors_handled);
			r10_bio->state = 0;
			r10_bio->mddev = mddev;
			r10_bio->sector = bio->bi_sector + sectors_handled;
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

	r10_bio->read_slot = -1; 
	raid10_find_phys(conf, r10_bio);
retry_write:
	blocked_rdev = NULL;
	rcu_read_lock();
	max_sectors = r10_bio->sectors;

	for (i = 0;  i < conf->copies; i++) {
		int d = r10_bio->devs[i].devnum;
		struct md_rdev *rdev = rcu_dereference(conf->mirrors[d].rdev);
		struct md_rdev *rrdev = rcu_dereference(
			conf->mirrors[d].replacement);
		if (rdev == rrdev)
			rrdev = NULL;
		if (rdev && unlikely(test_bit(Blocked, &rdev->flags))) {
			atomic_inc(&rdev->nr_pending);
			blocked_rdev = rdev;
			break;
		}
		if (rrdev && unlikely(test_bit(Blocked, &rrdev->flags))) {
			atomic_inc(&rrdev->nr_pending);
			blocked_rdev = rrdev;
			break;
		}
		if (rrdev && (test_bit(Faulty, &rrdev->flags)
			      || test_bit(Unmerged, &rrdev->flags)))
			rrdev = NULL;

		r10_bio->devs[i].bio = NULL;
		r10_bio->devs[i].repl_bio = NULL;
		if (!rdev || test_bit(Faulty, &rdev->flags) ||
		    test_bit(Unmerged, &rdev->flags)) {
			set_bit(R10BIO_Degraded, &r10_bio->state);
			continue;
		}
		if (test_bit(WriteErrorSeen, &rdev->flags)) {
			sector_t first_bad;
			sector_t dev_sector = r10_bio->devs[i].addr;
			int bad_sectors;
			int is_bad;

			is_bad = is_badblock(rdev, dev_sector,
					     max_sectors,
					     &first_bad, &bad_sectors);
			if (is_bad < 0) {
				atomic_inc(&rdev->nr_pending);
				set_bit(BlockedBadBlocks, &rdev->flags);
				blocked_rdev = rdev;
				break;
			}
			if (is_bad && first_bad <= dev_sector) {
				
				bad_sectors -= (dev_sector - first_bad);
				if (bad_sectors < max_sectors)
					max_sectors = bad_sectors;
				continue;
			}
			if (is_bad) {
				int good_sectors = first_bad - dev_sector;
				if (good_sectors < max_sectors)
					max_sectors = good_sectors;
			}
		}
		r10_bio->devs[i].bio = bio;
		atomic_inc(&rdev->nr_pending);
		if (rrdev) {
			r10_bio->devs[i].repl_bio = bio;
			atomic_inc(&rrdev->nr_pending);
		}
	}
	rcu_read_unlock();

	if (unlikely(blocked_rdev)) {
		
		int j;
		int d;

		for (j = 0; j < i; j++) {
			if (r10_bio->devs[j].bio) {
				d = r10_bio->devs[j].devnum;
				rdev_dec_pending(conf->mirrors[d].rdev, mddev);
			}
			if (r10_bio->devs[j].repl_bio) {
				struct md_rdev *rdev;
				d = r10_bio->devs[j].devnum;
				rdev = conf->mirrors[d].replacement;
				if (!rdev) {
					
					smp_mb();
					rdev = conf->mirrors[d].rdev;
				}
				rdev_dec_pending(rdev, mddev);
			}
		}
		allow_barrier(conf);
		md_wait_for_blocked_rdev(blocked_rdev, mddev);
		wait_barrier(conf);
		goto retry_write;
	}

	if (max_sectors < r10_bio->sectors) {
		r10_bio->sectors = max_sectors;
		spin_lock_irq(&conf->device_lock);
		if (bio->bi_phys_segments == 0)
			bio->bi_phys_segments = 2;
		else
			bio->bi_phys_segments++;
		spin_unlock_irq(&conf->device_lock);
	}
	sectors_handled = r10_bio->sector + max_sectors - bio->bi_sector;

	atomic_set(&r10_bio->remaining, 1);
	bitmap_startwrite(mddev->bitmap, r10_bio->sector, r10_bio->sectors, 0);

	for (i = 0; i < conf->copies; i++) {
		struct bio *mbio;
		int d = r10_bio->devs[i].devnum;
		if (!r10_bio->devs[i].bio)
			continue;

		mbio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(mbio, r10_bio->sector - bio->bi_sector,
			    max_sectors);
		r10_bio->devs[i].bio = mbio;

		mbio->bi_sector	= (r10_bio->devs[i].addr+
				   conf->mirrors[d].rdev->data_offset);
		mbio->bi_bdev = conf->mirrors[d].rdev->bdev;
		mbio->bi_end_io	= raid10_end_write_request;
		mbio->bi_rw = WRITE | do_sync | do_fua;
		mbio->bi_private = r10_bio;

		atomic_inc(&r10_bio->remaining);
		spin_lock_irqsave(&conf->device_lock, flags);
		bio_list_add(&conf->pending_bio_list, mbio);
		conf->pending_count++;
		spin_unlock_irqrestore(&conf->device_lock, flags);

		if (!r10_bio->devs[i].repl_bio)
			continue;

		mbio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(mbio, r10_bio->sector - bio->bi_sector,
			    max_sectors);
		r10_bio->devs[i].repl_bio = mbio;

		mbio->bi_sector	= (r10_bio->devs[i].addr+
				   conf->mirrors[d].replacement->data_offset);
		mbio->bi_bdev = conf->mirrors[d].replacement->bdev;
		mbio->bi_end_io	= raid10_end_write_request;
		mbio->bi_rw = WRITE | do_sync | do_fua;
		mbio->bi_private = r10_bio;

		atomic_inc(&r10_bio->remaining);
		spin_lock_irqsave(&conf->device_lock, flags);
		bio_list_add(&conf->pending_bio_list, mbio);
		conf->pending_count++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
	}


	if (sectors_handled < (bio->bi_size >> 9)) {
		one_write_done(r10_bio);
		r10_bio = mempool_alloc(conf->r10bio_pool, GFP_NOIO);

		r10_bio->master_bio = bio;
		r10_bio->sectors = (bio->bi_size >> 9) - sectors_handled;

		r10_bio->mddev = mddev;
		r10_bio->sector = bio->bi_sector + sectors_handled;
		r10_bio->state = 0;
		goto retry_write;
	}
	one_write_done(r10_bio);

	
	wake_up(&conf->wait_barrier);

	if (do_sync || !mddev->bitmap || !plugged)
		md_wakeup_thread(mddev->thread);
}

static void status(struct seq_file *seq, struct mddev *mddev)
{
	struct r10conf *conf = mddev->private;
	int i;

	if (conf->near_copies < conf->raid_disks)
		seq_printf(seq, " %dK chunks", mddev->chunk_sectors / 2);
	if (conf->near_copies > 1)
		seq_printf(seq, " %d near-copies", conf->near_copies);
	if (conf->far_copies > 1) {
		if (conf->far_offset)
			seq_printf(seq, " %d offset-copies", conf->far_copies);
		else
			seq_printf(seq, " %d far-copies", conf->far_copies);
	}
	seq_printf(seq, " [%d/%d] [", conf->raid_disks,
					conf->raid_disks - mddev->degraded);
	for (i = 0; i < conf->raid_disks; i++)
		seq_printf(seq, "%s",
			      conf->mirrors[i].rdev &&
			      test_bit(In_sync, &conf->mirrors[i].rdev->flags) ? "U" : "_");
	seq_printf(seq, "]");
}

static int enough(struct r10conf *conf, int ignore)
{
	int first = 0;

	do {
		int n = conf->copies;
		int cnt = 0;
		while (n--) {
			if (conf->mirrors[first].rdev &&
			    first != ignore)
				cnt++;
			first = (first+1) % conf->raid_disks;
		}
		if (cnt == 0)
			return 0;
	} while (first != 0);
	return 1;
}

static void error(struct mddev *mddev, struct md_rdev *rdev)
{
	char b[BDEVNAME_SIZE];
	struct r10conf *conf = mddev->private;

	if (test_bit(In_sync, &rdev->flags)
	    && !enough(conf, rdev->raid_disk))
		return;
	if (test_and_clear_bit(In_sync, &rdev->flags)) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		mddev->degraded++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
		set_bit(MD_RECOVERY_INTR, &mddev->recovery);
	}
	set_bit(Blocked, &rdev->flags);
	set_bit(Faulty, &rdev->flags);
	set_bit(MD_CHANGE_DEVS, &mddev->flags);
	printk(KERN_ALERT
	       "md/raid10:%s: Disk failure on %s, disabling device.\n"
	       "md/raid10:%s: Operation continuing on %d devices.\n",
	       mdname(mddev), bdevname(rdev->bdev, b),
	       mdname(mddev), conf->raid_disks - mddev->degraded);
}

static void print_conf(struct r10conf *conf)
{
	int i;
	struct mirror_info *tmp;

	printk(KERN_DEBUG "RAID10 conf printout:\n");
	if (!conf) {
		printk(KERN_DEBUG "(!conf)\n");
		return;
	}
	printk(KERN_DEBUG " --- wd:%d rd:%d\n", conf->raid_disks - conf->mddev->degraded,
		conf->raid_disks);

	for (i = 0; i < conf->raid_disks; i++) {
		char b[BDEVNAME_SIZE];
		tmp = conf->mirrors + i;
		if (tmp->rdev)
			printk(KERN_DEBUG " disk %d, wo:%d, o:%d, dev:%s\n",
				i, !test_bit(In_sync, &tmp->rdev->flags),
			        !test_bit(Faulty, &tmp->rdev->flags),
				bdevname(tmp->rdev->bdev,b));
	}
}

static void close_sync(struct r10conf *conf)
{
	wait_barrier(conf);
	allow_barrier(conf);

	mempool_destroy(conf->r10buf_pool);
	conf->r10buf_pool = NULL;
}

static int raid10_spare_active(struct mddev *mddev)
{
	int i;
	struct r10conf *conf = mddev->private;
	struct mirror_info *tmp;
	int count = 0;
	unsigned long flags;

	for (i = 0; i < conf->raid_disks; i++) {
		tmp = conf->mirrors + i;
		if (tmp->replacement
		    && tmp->replacement->recovery_offset == MaxSector
		    && !test_bit(Faulty, &tmp->replacement->flags)
		    && !test_and_set_bit(In_sync, &tmp->replacement->flags)) {
			
			if (!tmp->rdev
			    || !test_and_clear_bit(In_sync, &tmp->rdev->flags))
				count++;
			if (tmp->rdev) {
				set_bit(Faulty, &tmp->rdev->flags);
				sysfs_notify_dirent_safe(
					tmp->rdev->sysfs_state);
			}
			sysfs_notify_dirent_safe(tmp->replacement->sysfs_state);
		} else if (tmp->rdev
			   && !test_bit(Faulty, &tmp->rdev->flags)
			   && !test_and_set_bit(In_sync, &tmp->rdev->flags)) {
			count++;
			sysfs_notify_dirent(tmp->rdev->sysfs_state);
		}
	}
	spin_lock_irqsave(&conf->device_lock, flags);
	mddev->degraded -= count;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	print_conf(conf);
	return count;
}


static int raid10_add_disk(struct mddev *mddev, struct md_rdev *rdev)
{
	struct r10conf *conf = mddev->private;
	int err = -EEXIST;
	int mirror;
	int first = 0;
	int last = conf->raid_disks - 1;
	struct request_queue *q = bdev_get_queue(rdev->bdev);

	if (mddev->recovery_cp < MaxSector)
		return -EBUSY;
	if (rdev->saved_raid_disk < 0 && !enough(conf, -1))
		return -EINVAL;

	if (rdev->raid_disk >= 0)
		first = last = rdev->raid_disk;

	if (q->merge_bvec_fn) {
		set_bit(Unmerged, &rdev->flags);
		mddev->merge_check_needed = 1;
	}

	if (rdev->saved_raid_disk >= first &&
	    conf->mirrors[rdev->saved_raid_disk].rdev == NULL)
		mirror = rdev->saved_raid_disk;
	else
		mirror = first;
	for ( ; mirror <= last ; mirror++) {
		struct mirror_info *p = &conf->mirrors[mirror];
		if (p->recovery_disabled == mddev->recovery_disabled)
			continue;
		if (p->rdev) {
			if (!test_bit(WantReplacement, &p->rdev->flags) ||
			    p->replacement != NULL)
				continue;
			clear_bit(In_sync, &rdev->flags);
			set_bit(Replacement, &rdev->flags);
			rdev->raid_disk = mirror;
			err = 0;
			disk_stack_limits(mddev->gendisk, rdev->bdev,
					  rdev->data_offset << 9);
			conf->fullsync = 1;
			rcu_assign_pointer(p->replacement, rdev);
			break;
		}

		disk_stack_limits(mddev->gendisk, rdev->bdev,
				  rdev->data_offset << 9);

		p->head_position = 0;
		p->recovery_disabled = mddev->recovery_disabled - 1;
		rdev->raid_disk = mirror;
		err = 0;
		if (rdev->saved_raid_disk != mirror)
			conf->fullsync = 1;
		rcu_assign_pointer(p->rdev, rdev);
		break;
	}
	if (err == 0 && test_bit(Unmerged, &rdev->flags)) {
		synchronize_sched();
		raise_barrier(conf, 0);
		lower_barrier(conf);
		clear_bit(Unmerged, &rdev->flags);
	}
	md_integrity_add_rdev(rdev, mddev);
	print_conf(conf);
	return err;
}

static int raid10_remove_disk(struct mddev *mddev, struct md_rdev *rdev)
{
	struct r10conf *conf = mddev->private;
	int err = 0;
	int number = rdev->raid_disk;
	struct md_rdev **rdevp;
	struct mirror_info *p = conf->mirrors + number;

	print_conf(conf);
	if (rdev == p->rdev)
		rdevp = &p->rdev;
	else if (rdev == p->replacement)
		rdevp = &p->replacement;
	else
		return 0;

	if (test_bit(In_sync, &rdev->flags) ||
	    atomic_read(&rdev->nr_pending)) {
		err = -EBUSY;
		goto abort;
	}
	if (!test_bit(Faulty, &rdev->flags) &&
	    mddev->recovery_disabled != p->recovery_disabled &&
	    (!p->replacement || p->replacement == rdev) &&
	    enough(conf, -1)) {
		err = -EBUSY;
		goto abort;
	}
	*rdevp = NULL;
	synchronize_rcu();
	if (atomic_read(&rdev->nr_pending)) {
		
		err = -EBUSY;
		*rdevp = rdev;
		goto abort;
	} else if (p->replacement) {
		
		p->rdev = p->replacement;
		clear_bit(Replacement, &p->replacement->flags);
		smp_mb(); 
		p->replacement = NULL;
		clear_bit(WantReplacement, &rdev->flags);
	} else
		clear_bit(WantReplacement, &rdev->flags);

	err = md_integrity_register(mddev);

abort:

	print_conf(conf);
	return err;
}


static void end_sync_read(struct bio *bio, int error)
{
	struct r10bio *r10_bio = bio->bi_private;
	struct r10conf *conf = r10_bio->mddev->private;
	int d;

	d = find_bio_disk(conf, r10_bio, bio, NULL, NULL);

	if (test_bit(BIO_UPTODATE, &bio->bi_flags))
		set_bit(R10BIO_Uptodate, &r10_bio->state);
	else
		atomic_add(r10_bio->sectors,
			   &conf->mirrors[d].rdev->corrected_errors);

	rdev_dec_pending(conf->mirrors[d].rdev, conf->mddev);
	if (test_bit(R10BIO_IsRecover, &r10_bio->state) ||
	    atomic_dec_and_test(&r10_bio->remaining)) {
		reschedule_retry(r10_bio);
	}
}

static void end_sync_request(struct r10bio *r10_bio)
{
	struct mddev *mddev = r10_bio->mddev;

	while (atomic_dec_and_test(&r10_bio->remaining)) {
		if (r10_bio->master_bio == NULL) {
			
			sector_t s = r10_bio->sectors;
			if (test_bit(R10BIO_MadeGood, &r10_bio->state) ||
			    test_bit(R10BIO_WriteError, &r10_bio->state))
				reschedule_retry(r10_bio);
			else
				put_buf(r10_bio);
			md_done_sync(mddev, s, 1);
			break;
		} else {
			struct r10bio *r10_bio2 = (struct r10bio *)r10_bio->master_bio;
			if (test_bit(R10BIO_MadeGood, &r10_bio->state) ||
			    test_bit(R10BIO_WriteError, &r10_bio->state))
				reschedule_retry(r10_bio);
			else
				put_buf(r10_bio);
			r10_bio = r10_bio2;
		}
	}
}

static void end_sync_write(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct r10bio *r10_bio = bio->bi_private;
	struct mddev *mddev = r10_bio->mddev;
	struct r10conf *conf = mddev->private;
	int d;
	sector_t first_bad;
	int bad_sectors;
	int slot;
	int repl;
	struct md_rdev *rdev = NULL;

	d = find_bio_disk(conf, r10_bio, bio, &slot, &repl);
	if (repl)
		rdev = conf->mirrors[d].replacement;
	else
		rdev = conf->mirrors[d].rdev;

	if (!uptodate) {
		if (repl)
			md_error(mddev, rdev);
		else {
			set_bit(WriteErrorSeen, &rdev->flags);
			if (!test_and_set_bit(WantReplacement, &rdev->flags))
				set_bit(MD_RECOVERY_NEEDED,
					&rdev->mddev->recovery);
			set_bit(R10BIO_WriteError, &r10_bio->state);
		}
	} else if (is_badblock(rdev,
			     r10_bio->devs[slot].addr,
			     r10_bio->sectors,
			     &first_bad, &bad_sectors))
		set_bit(R10BIO_MadeGood, &r10_bio->state);

	rdev_dec_pending(rdev, mddev);

	end_sync_request(r10_bio);
}

static void sync_request_write(struct mddev *mddev, struct r10bio *r10_bio)
{
	struct r10conf *conf = mddev->private;
	int i, first;
	struct bio *tbio, *fbio;
	int vcnt;

	atomic_set(&r10_bio->remaining, 1);

	
	for (i=0; i<conf->copies; i++)
		if (test_bit(BIO_UPTODATE, &r10_bio->devs[i].bio->bi_flags))
			break;

	if (i == conf->copies)
		goto done;

	first = i;
	fbio = r10_bio->devs[i].bio;

	vcnt = (r10_bio->sectors + (PAGE_SIZE >> 9) - 1) >> (PAGE_SHIFT - 9);
	
	for (i=0 ; i < conf->copies ; i++) {
		int  j, d;

		tbio = r10_bio->devs[i].bio;

		if (tbio->bi_end_io != end_sync_read)
			continue;
		if (i == first)
			continue;
		if (test_bit(BIO_UPTODATE, &r10_bio->devs[i].bio->bi_flags)) {
			for (j = 0; j < vcnt; j++)
				if (memcmp(page_address(fbio->bi_io_vec[j].bv_page),
					   page_address(tbio->bi_io_vec[j].bv_page),
					   fbio->bi_io_vec[j].bv_len))
					break;
			if (j == vcnt)
				continue;
			mddev->resync_mismatches += r10_bio->sectors;
			if (test_bit(MD_RECOVERY_CHECK, &mddev->recovery))
				
				continue;
		}
		tbio->bi_vcnt = vcnt;
		tbio->bi_size = r10_bio->sectors << 9;
		tbio->bi_idx = 0;
		tbio->bi_phys_segments = 0;
		tbio->bi_flags &= ~(BIO_POOL_MASK - 1);
		tbio->bi_flags |= 1 << BIO_UPTODATE;
		tbio->bi_next = NULL;
		tbio->bi_rw = WRITE;
		tbio->bi_private = r10_bio;
		tbio->bi_sector = r10_bio->devs[i].addr;

		for (j=0; j < vcnt ; j++) {
			tbio->bi_io_vec[j].bv_offset = 0;
			tbio->bi_io_vec[j].bv_len = PAGE_SIZE;

			memcpy(page_address(tbio->bi_io_vec[j].bv_page),
			       page_address(fbio->bi_io_vec[j].bv_page),
			       PAGE_SIZE);
		}
		tbio->bi_end_io = end_sync_write;

		d = r10_bio->devs[i].devnum;
		atomic_inc(&conf->mirrors[d].rdev->nr_pending);
		atomic_inc(&r10_bio->remaining);
		md_sync_acct(conf->mirrors[d].rdev->bdev, tbio->bi_size >> 9);

		tbio->bi_sector += conf->mirrors[d].rdev->data_offset;
		tbio->bi_bdev = conf->mirrors[d].rdev->bdev;
		generic_make_request(tbio);
	}

	for (i = 0; i < conf->copies; i++) {
		int j, d;

		tbio = r10_bio->devs[i].repl_bio;
		if (!tbio || !tbio->bi_end_io)
			continue;
		if (r10_bio->devs[i].bio->bi_end_io != end_sync_write
		    && r10_bio->devs[i].bio != fbio)
			for (j = 0; j < vcnt; j++)
				memcpy(page_address(tbio->bi_io_vec[j].bv_page),
				       page_address(fbio->bi_io_vec[j].bv_page),
				       PAGE_SIZE);
		d = r10_bio->devs[i].devnum;
		atomic_inc(&r10_bio->remaining);
		md_sync_acct(conf->mirrors[d].replacement->bdev,
			     tbio->bi_size >> 9);
		generic_make_request(tbio);
	}

done:
	if (atomic_dec_and_test(&r10_bio->remaining)) {
		md_done_sync(mddev, r10_bio->sectors, 1);
		put_buf(r10_bio);
	}
}

static void fix_recovery_read_error(struct r10bio *r10_bio)
{
	struct mddev *mddev = r10_bio->mddev;
	struct r10conf *conf = mddev->private;
	struct bio *bio = r10_bio->devs[0].bio;
	sector_t sect = 0;
	int sectors = r10_bio->sectors;
	int idx = 0;
	int dr = r10_bio->devs[0].devnum;
	int dw = r10_bio->devs[1].devnum;

	while (sectors) {
		int s = sectors;
		struct md_rdev *rdev;
		sector_t addr;
		int ok;

		if (s > (PAGE_SIZE>>9))
			s = PAGE_SIZE >> 9;

		rdev = conf->mirrors[dr].rdev;
		addr = r10_bio->devs[0].addr + sect,
		ok = sync_page_io(rdev,
				  addr,
				  s << 9,
				  bio->bi_io_vec[idx].bv_page,
				  READ, false);
		if (ok) {
			rdev = conf->mirrors[dw].rdev;
			addr = r10_bio->devs[1].addr + sect;
			ok = sync_page_io(rdev,
					  addr,
					  s << 9,
					  bio->bi_io_vec[idx].bv_page,
					  WRITE, false);
			if (!ok) {
				set_bit(WriteErrorSeen, &rdev->flags);
				if (!test_and_set_bit(WantReplacement,
						      &rdev->flags))
					set_bit(MD_RECOVERY_NEEDED,
						&rdev->mddev->recovery);
			}
		}
		if (!ok) {
			rdev_set_badblocks(rdev, addr, s, 0);

			if (rdev != conf->mirrors[dw].rdev) {
				
				struct md_rdev *rdev2 = conf->mirrors[dw].rdev;
				addr = r10_bio->devs[1].addr + sect;
				ok = rdev_set_badblocks(rdev2, addr, s, 0);
				if (!ok) {
					
					printk(KERN_NOTICE
					       "md/raid10:%s: recovery aborted"
					       " due to read error\n",
					       mdname(mddev));

					conf->mirrors[dw].recovery_disabled
						= mddev->recovery_disabled;
					set_bit(MD_RECOVERY_INTR,
						&mddev->recovery);
					break;
				}
			}
		}

		sectors -= s;
		sect += s;
		idx++;
	}
}

static void recovery_request_write(struct mddev *mddev, struct r10bio *r10_bio)
{
	struct r10conf *conf = mddev->private;
	int d;
	struct bio *wbio, *wbio2;

	if (!test_bit(R10BIO_Uptodate, &r10_bio->state)) {
		fix_recovery_read_error(r10_bio);
		end_sync_request(r10_bio);
		return;
	}

	d = r10_bio->devs[1].devnum;
	wbio = r10_bio->devs[1].bio;
	wbio2 = r10_bio->devs[1].repl_bio;
	if (wbio->bi_end_io) {
		atomic_inc(&conf->mirrors[d].rdev->nr_pending);
		md_sync_acct(conf->mirrors[d].rdev->bdev, wbio->bi_size >> 9);
		generic_make_request(wbio);
	}
	if (wbio2 && wbio2->bi_end_io) {
		atomic_inc(&conf->mirrors[d].replacement->nr_pending);
		md_sync_acct(conf->mirrors[d].replacement->bdev,
			     wbio2->bi_size >> 9);
		generic_make_request(wbio2);
	}
}


static void check_decay_read_errors(struct mddev *mddev, struct md_rdev *rdev)
{
	struct timespec cur_time_mon;
	unsigned long hours_since_last;
	unsigned int read_errors = atomic_read(&rdev->read_errors);

	ktime_get_ts(&cur_time_mon);

	if (rdev->last_read_error.tv_sec == 0 &&
	    rdev->last_read_error.tv_nsec == 0) {
		
		rdev->last_read_error = cur_time_mon;
		return;
	}

	hours_since_last = (cur_time_mon.tv_sec -
			    rdev->last_read_error.tv_sec) / 3600;

	rdev->last_read_error = cur_time_mon;

	if (hours_since_last >= 8 * sizeof(read_errors))
		atomic_set(&rdev->read_errors, 0);
	else
		atomic_set(&rdev->read_errors, read_errors >> hours_since_last);
}

static int r10_sync_page_io(struct md_rdev *rdev, sector_t sector,
			    int sectors, struct page *page, int rw)
{
	sector_t first_bad;
	int bad_sectors;

	if (is_badblock(rdev, sector, sectors, &first_bad, &bad_sectors)
	    && (rw == READ || test_bit(WriteErrorSeen, &rdev->flags)))
		return -1;
	if (sync_page_io(rdev, sector, sectors << 9, page, rw, false))
		
		return 1;
	if (rw == WRITE) {
		set_bit(WriteErrorSeen, &rdev->flags);
		if (!test_and_set_bit(WantReplacement, &rdev->flags))
			set_bit(MD_RECOVERY_NEEDED,
				&rdev->mddev->recovery);
	}
	
	if (!rdev_set_badblocks(rdev, sector, sectors, 0))
		md_error(rdev->mddev, rdev);
	return 0;
}


static void fix_read_error(struct r10conf *conf, struct mddev *mddev, struct r10bio *r10_bio)
{
	int sect = 0; 
	int sectors = r10_bio->sectors;
	struct md_rdev*rdev;
	int max_read_errors = atomic_read(&mddev->max_corr_read_errors);
	int d = r10_bio->devs[r10_bio->read_slot].devnum;

	rdev = conf->mirrors[d].rdev;

	if (test_bit(Faulty, &rdev->flags))
		return;

	check_decay_read_errors(mddev, rdev);
	atomic_inc(&rdev->read_errors);
	if (atomic_read(&rdev->read_errors) > max_read_errors) {
		char b[BDEVNAME_SIZE];
		bdevname(rdev->bdev, b);

		printk(KERN_NOTICE
		       "md/raid10:%s: %s: Raid device exceeded "
		       "read_error threshold [cur %d:max %d]\n",
		       mdname(mddev), b,
		       atomic_read(&rdev->read_errors), max_read_errors);
		printk(KERN_NOTICE
		       "md/raid10:%s: %s: Failing raid device\n",
		       mdname(mddev), b);
		md_error(mddev, conf->mirrors[d].rdev);
		r10_bio->devs[r10_bio->read_slot].bio = IO_BLOCKED;
		return;
	}

	while(sectors) {
		int s = sectors;
		int sl = r10_bio->read_slot;
		int success = 0;
		int start;

		if (s > (PAGE_SIZE>>9))
			s = PAGE_SIZE >> 9;

		rcu_read_lock();
		do {
			sector_t first_bad;
			int bad_sectors;

			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (rdev &&
			    !test_bit(Unmerged, &rdev->flags) &&
			    test_bit(In_sync, &rdev->flags) &&
			    is_badblock(rdev, r10_bio->devs[sl].addr + sect, s,
					&first_bad, &bad_sectors) == 0) {
				atomic_inc(&rdev->nr_pending);
				rcu_read_unlock();
				success = sync_page_io(rdev,
						       r10_bio->devs[sl].addr +
						       sect,
						       s<<9,
						       conf->tmppage, READ, false);
				rdev_dec_pending(rdev, mddev);
				rcu_read_lock();
				if (success)
					break;
			}
			sl++;
			if (sl == conf->copies)
				sl = 0;
		} while (!success && sl != r10_bio->read_slot);
		rcu_read_unlock();

		if (!success) {
			int dn = r10_bio->devs[r10_bio->read_slot].devnum;
			rdev = conf->mirrors[dn].rdev;

			if (!rdev_set_badblocks(
				    rdev,
				    r10_bio->devs[r10_bio->read_slot].addr
				    + sect,
				    s, 0)) {
				md_error(mddev, rdev);
				r10_bio->devs[r10_bio->read_slot].bio
					= IO_BLOCKED;
			}
			break;
		}

		start = sl;
		
		rcu_read_lock();
		while (sl != r10_bio->read_slot) {
			char b[BDEVNAME_SIZE];

			if (sl==0)
				sl = conf->copies;
			sl--;
			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (!rdev ||
			    test_bit(Unmerged, &rdev->flags) ||
			    !test_bit(In_sync, &rdev->flags))
				continue;

			atomic_inc(&rdev->nr_pending);
			rcu_read_unlock();
			if (r10_sync_page_io(rdev,
					     r10_bio->devs[sl].addr +
					     sect,
					     s<<9, conf->tmppage, WRITE)
			    == 0) {
				
				printk(KERN_NOTICE
				       "md/raid10:%s: read correction "
				       "write failed"
				       " (%d sectors at %llu on %s)\n",
				       mdname(mddev), s,
				       (unsigned long long)(
					       sect + rdev->data_offset),
				       bdevname(rdev->bdev, b));
				printk(KERN_NOTICE "md/raid10:%s: %s: failing "
				       "drive\n",
				       mdname(mddev),
				       bdevname(rdev->bdev, b));
			}
			rdev_dec_pending(rdev, mddev);
			rcu_read_lock();
		}
		sl = start;
		while (sl != r10_bio->read_slot) {
			char b[BDEVNAME_SIZE];

			if (sl==0)
				sl = conf->copies;
			sl--;
			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (!rdev ||
			    !test_bit(In_sync, &rdev->flags))
				continue;

			atomic_inc(&rdev->nr_pending);
			rcu_read_unlock();
			switch (r10_sync_page_io(rdev,
					     r10_bio->devs[sl].addr +
					     sect,
					     s<<9, conf->tmppage,
						 READ)) {
			case 0:
				
				printk(KERN_NOTICE
				       "md/raid10:%s: unable to read back "
				       "corrected sectors"
				       " (%d sectors at %llu on %s)\n",
				       mdname(mddev), s,
				       (unsigned long long)(
					       sect + rdev->data_offset),
				       bdevname(rdev->bdev, b));
				printk(KERN_NOTICE "md/raid10:%s: %s: failing "
				       "drive\n",
				       mdname(mddev),
				       bdevname(rdev->bdev, b));
				break;
			case 1:
				printk(KERN_INFO
				       "md/raid10:%s: read error corrected"
				       " (%d sectors at %llu on %s)\n",
				       mdname(mddev), s,
				       (unsigned long long)(
					       sect + rdev->data_offset),
				       bdevname(rdev->bdev, b));
				atomic_add(s, &rdev->corrected_errors);
			}

			rdev_dec_pending(rdev, mddev);
			rcu_read_lock();
		}
		rcu_read_unlock();

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

static int narrow_write_error(struct r10bio *r10_bio, int i)
{
	struct bio *bio = r10_bio->master_bio;
	struct mddev *mddev = r10_bio->mddev;
	struct r10conf *conf = mddev->private;
	struct md_rdev *rdev = conf->mirrors[r10_bio->devs[i].devnum].rdev;
	/* bio has the data to be written to slot 'i' where
	 * we just recently had a write error.
	 * We repeatedly clone the bio and trim down to one block,
	 * then try the write.  Where the write fails we record
	 * a bad block.
	 * It is conceivable that the bio doesn't exactly align with
	 * blocks.  We must handle this.
	 *
	 * We currently own a reference to the rdev.
	 */

	int block_sectors;
	sector_t sector;
	int sectors;
	int sect_to_write = r10_bio->sectors;
	int ok = 1;

	if (rdev->badblocks.shift < 0)
		return 0;

	block_sectors = 1 << rdev->badblocks.shift;
	sector = r10_bio->sector;
	sectors = ((r10_bio->sector + block_sectors)
		   & ~(sector_t)(block_sectors - 1))
		- sector;

	while (sect_to_write) {
		struct bio *wbio;
		if (sectors > sect_to_write)
			sectors = sect_to_write;
		
		wbio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		md_trim_bio(wbio, sector - bio->bi_sector, sectors);
		wbio->bi_sector = (r10_bio->devs[i].addr+
				   rdev->data_offset+
				   (sector - r10_bio->sector));
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

static void handle_read_error(struct mddev *mddev, struct r10bio *r10_bio)
{
	int slot = r10_bio->read_slot;
	struct bio *bio;
	struct r10conf *conf = mddev->private;
	struct md_rdev *rdev = r10_bio->devs[slot].rdev;
	char b[BDEVNAME_SIZE];
	unsigned long do_sync;
	int max_sectors;

	bio = r10_bio->devs[slot].bio;
	bdevname(bio->bi_bdev, b);
	bio_put(bio);
	r10_bio->devs[slot].bio = NULL;

	if (mddev->ro == 0) {
		freeze_array(conf);
		fix_read_error(conf, mddev, r10_bio);
		unfreeze_array(conf);
	} else
		r10_bio->devs[slot].bio = IO_BLOCKED;

	rdev_dec_pending(rdev, mddev);

read_more:
	rdev = read_balance(conf, r10_bio, &max_sectors);
	if (rdev == NULL) {
		printk(KERN_ALERT "md/raid10:%s: %s: unrecoverable I/O"
		       " read error for block %llu\n",
		       mdname(mddev), b,
		       (unsigned long long)r10_bio->sector);
		raid_end_bio_io(r10_bio);
		return;
	}

	do_sync = (r10_bio->master_bio->bi_rw & REQ_SYNC);
	slot = r10_bio->read_slot;
	printk_ratelimited(
		KERN_ERR
		"md/raid10:%s: %s: redirecting"
		"sector %llu to another mirror\n",
		mdname(mddev),
		bdevname(rdev->bdev, b),
		(unsigned long long)r10_bio->sector);
	bio = bio_clone_mddev(r10_bio->master_bio,
			      GFP_NOIO, mddev);
	md_trim_bio(bio,
		    r10_bio->sector - bio->bi_sector,
		    max_sectors);
	r10_bio->devs[slot].bio = bio;
	r10_bio->devs[slot].rdev = rdev;
	bio->bi_sector = r10_bio->devs[slot].addr
		+ rdev->data_offset;
	bio->bi_bdev = rdev->bdev;
	bio->bi_rw = READ | do_sync;
	bio->bi_private = r10_bio;
	bio->bi_end_io = raid10_end_read_request;
	if (max_sectors < r10_bio->sectors) {
		
		struct bio *mbio = r10_bio->master_bio;
		int sectors_handled =
			r10_bio->sector + max_sectors
			- mbio->bi_sector;
		r10_bio->sectors = max_sectors;
		spin_lock_irq(&conf->device_lock);
		if (mbio->bi_phys_segments == 0)
			mbio->bi_phys_segments = 2;
		else
			mbio->bi_phys_segments++;
		spin_unlock_irq(&conf->device_lock);
		generic_make_request(bio);

		r10_bio = mempool_alloc(conf->r10bio_pool,
					GFP_NOIO);
		r10_bio->master_bio = mbio;
		r10_bio->sectors = (mbio->bi_size >> 9)
			- sectors_handled;
		r10_bio->state = 0;
		set_bit(R10BIO_ReadError,
			&r10_bio->state);
		r10_bio->mddev = mddev;
		r10_bio->sector = mbio->bi_sector
			+ sectors_handled;

		goto read_more;
	} else
		generic_make_request(bio);
}

static void handle_write_completed(struct r10conf *conf, struct r10bio *r10_bio)
{
	int m;
	struct md_rdev *rdev;

	if (test_bit(R10BIO_IsSync, &r10_bio->state) ||
	    test_bit(R10BIO_IsRecover, &r10_bio->state)) {
		for (m = 0; m < conf->copies; m++) {
			int dev = r10_bio->devs[m].devnum;
			rdev = conf->mirrors[dev].rdev;
			if (r10_bio->devs[m].bio == NULL)
				continue;
			if (test_bit(BIO_UPTODATE,
				     &r10_bio->devs[m].bio->bi_flags)) {
				rdev_clear_badblocks(
					rdev,
					r10_bio->devs[m].addr,
					r10_bio->sectors);
			} else {
				if (!rdev_set_badblocks(
					    rdev,
					    r10_bio->devs[m].addr,
					    r10_bio->sectors, 0))
					md_error(conf->mddev, rdev);
			}
			rdev = conf->mirrors[dev].replacement;
			if (r10_bio->devs[m].repl_bio == NULL)
				continue;
			if (test_bit(BIO_UPTODATE,
				     &r10_bio->devs[m].repl_bio->bi_flags)) {
				rdev_clear_badblocks(
					rdev,
					r10_bio->devs[m].addr,
					r10_bio->sectors);
			} else {
				if (!rdev_set_badblocks(
					    rdev,
					    r10_bio->devs[m].addr,
					    r10_bio->sectors, 0))
					md_error(conf->mddev, rdev);
			}
		}
		put_buf(r10_bio);
	} else {
		for (m = 0; m < conf->copies; m++) {
			int dev = r10_bio->devs[m].devnum;
			struct bio *bio = r10_bio->devs[m].bio;
			rdev = conf->mirrors[dev].rdev;
			if (bio == IO_MADE_GOOD) {
				rdev_clear_badblocks(
					rdev,
					r10_bio->devs[m].addr,
					r10_bio->sectors);
				rdev_dec_pending(rdev, conf->mddev);
			} else if (bio != NULL &&
				   !test_bit(BIO_UPTODATE, &bio->bi_flags)) {
				if (!narrow_write_error(r10_bio, m)) {
					md_error(conf->mddev, rdev);
					set_bit(R10BIO_Degraded,
						&r10_bio->state);
				}
				rdev_dec_pending(rdev, conf->mddev);
			}
			bio = r10_bio->devs[m].repl_bio;
			rdev = conf->mirrors[dev].replacement;
			if (rdev && bio == IO_MADE_GOOD) {
				rdev_clear_badblocks(
					rdev,
					r10_bio->devs[m].addr,
					r10_bio->sectors);
				rdev_dec_pending(rdev, conf->mddev);
			}
		}
		if (test_bit(R10BIO_WriteError,
			     &r10_bio->state))
			close_write(r10_bio);
		raid_end_bio_io(r10_bio);
	}
}

static void raid10d(struct mddev *mddev)
{
	struct r10bio *r10_bio;
	unsigned long flags;
	struct r10conf *conf = mddev->private;
	struct list_head *head = &conf->retry_list;
	struct blk_plug plug;

	md_check_recovery(mddev);

	blk_start_plug(&plug);
	for (;;) {

		flush_pending_writes(conf);

		spin_lock_irqsave(&conf->device_lock, flags);
		if (list_empty(head)) {
			spin_unlock_irqrestore(&conf->device_lock, flags);
			break;
		}
		r10_bio = list_entry(head->prev, struct r10bio, retry_list);
		list_del(head->prev);
		conf->nr_queued--;
		spin_unlock_irqrestore(&conf->device_lock, flags);

		mddev = r10_bio->mddev;
		conf = mddev->private;
		if (test_bit(R10BIO_MadeGood, &r10_bio->state) ||
		    test_bit(R10BIO_WriteError, &r10_bio->state))
			handle_write_completed(conf, r10_bio);
		else if (test_bit(R10BIO_IsSync, &r10_bio->state))
			sync_request_write(mddev, r10_bio);
		else if (test_bit(R10BIO_IsRecover, &r10_bio->state))
			recovery_request_write(mddev, r10_bio);
		else if (test_bit(R10BIO_ReadError, &r10_bio->state))
			handle_read_error(mddev, r10_bio);
		else {
			int slot = r10_bio->read_slot;
			generic_make_request(r10_bio->devs[slot].bio);
		}

		cond_resched();
		if (mddev->flags & ~(1<<MD_CHANGE_PENDING))
			md_check_recovery(mddev);
	}
	blk_finish_plug(&plug);
}


static int init_resync(struct r10conf *conf)
{
	int buffs;
	int i;

	buffs = RESYNC_WINDOW / RESYNC_BLOCK_SIZE;
	BUG_ON(conf->r10buf_pool);
	conf->have_replacement = 0;
	for (i = 0; i < conf->raid_disks; i++)
		if (conf->mirrors[i].replacement)
			conf->have_replacement = 1;
	conf->r10buf_pool = mempool_create(buffs, r10buf_pool_alloc, r10buf_pool_free, conf);
	if (!conf->r10buf_pool)
		return -ENOMEM;
	conf->next_resync = 0;
	return 0;
}


static sector_t sync_request(struct mddev *mddev, sector_t sector_nr,
			     int *skipped, int go_faster)
{
	struct r10conf *conf = mddev->private;
	struct r10bio *r10_bio;
	struct bio *biolist = NULL, *bio;
	sector_t max_sector, nr_sectors;
	int i;
	int max_sync;
	sector_t sync_blocks;
	sector_t sectors_skipped = 0;
	int chunks_skipped = 0;

	if (!conf->r10buf_pool)
		if (init_resync(conf))
			return 0;

 skipped:
	max_sector = mddev->dev_sectors;
	if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery))
		max_sector = mddev->resync_max_sectors;
	if (sector_nr >= max_sector) {
		if (mddev->curr_resync < max_sector) { 
			if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery))
				bitmap_end_sync(mddev->bitmap, mddev->curr_resync,
						&sync_blocks, 1);
			else for (i=0; i<conf->raid_disks; i++) {
				sector_t sect =
					raid10_find_virt(conf, mddev->curr_resync, i);
				bitmap_end_sync(mddev->bitmap, sect,
						&sync_blocks, 1);
			}
		} else {
			
			if ((!mddev->bitmap || conf->fullsync)
			    && conf->have_replacement
			    && test_bit(MD_RECOVERY_SYNC, &mddev->recovery)) {
				for (i = 0; i < conf->raid_disks; i++)
					if (conf->mirrors[i].replacement)
						conf->mirrors[i].replacement
							->recovery_offset
							= MaxSector;
			}
			conf->fullsync = 0;
		}
		bitmap_close_sync(mddev->bitmap);
		close_sync(conf);
		*skipped = 1;
		return sectors_skipped;
	}
	if (chunks_skipped >= conf->raid_disks) {
		*skipped = 1;
		return (max_sector - sector_nr) + sectors_skipped;
	}

	if (max_sector > mddev->resync_max)
		max_sector = mddev->resync_max; 

	if (conf->near_copies < conf->raid_disks &&
	    max_sector > (sector_nr | conf->chunk_mask))
		max_sector = (sector_nr | conf->chunk_mask) + 1;
	if (!go_faster && conf->nr_waiting)
		msleep_interruptible(1000);


	max_sync = RESYNC_PAGES << (PAGE_SHIFT-9);
	if (!test_bit(MD_RECOVERY_SYNC, &mddev->recovery)) {
		
		int j;
		r10_bio = NULL;

		for (i=0 ; i<conf->raid_disks; i++) {
			int still_degraded;
			struct r10bio *rb2;
			sector_t sect;
			int must_sync;
			int any_working;
			struct mirror_info *mirror = &conf->mirrors[i];

			if ((mirror->rdev == NULL ||
			     test_bit(In_sync, &mirror->rdev->flags))
			    &&
			    (mirror->replacement == NULL ||
			     test_bit(Faulty,
				      &mirror->replacement->flags)))
				continue;

			still_degraded = 0;
			
			rb2 = r10_bio;
			sect = raid10_find_virt(conf, sector_nr, i);
			must_sync = bitmap_start_sync(mddev->bitmap, sect,
						      &sync_blocks, 1);
			if (sync_blocks < max_sync)
				max_sync = sync_blocks;
			if (!must_sync &&
			    mirror->replacement == NULL &&
			    !conf->fullsync) {
				chunks_skipped = -1;
				continue;
			}

			r10_bio = mempool_alloc(conf->r10buf_pool, GFP_NOIO);
			raise_barrier(conf, rb2 != NULL);
			atomic_set(&r10_bio->remaining, 0);

			r10_bio->master_bio = (struct bio*)rb2;
			if (rb2)
				atomic_inc(&rb2->remaining);
			r10_bio->mddev = mddev;
			set_bit(R10BIO_IsRecover, &r10_bio->state);
			r10_bio->sector = sect;

			raid10_find_phys(conf, r10_bio);

			for (j=0; j<conf->raid_disks; j++)
				if (conf->mirrors[j].rdev == NULL ||
				    test_bit(Faulty, &conf->mirrors[j].rdev->flags)) {
					still_degraded = 1;
					break;
				}

			must_sync = bitmap_start_sync(mddev->bitmap, sect,
						      &sync_blocks, still_degraded);

			any_working = 0;
			for (j=0; j<conf->copies;j++) {
				int k;
				int d = r10_bio->devs[j].devnum;
				sector_t from_addr, to_addr;
				struct md_rdev *rdev;
				sector_t sector, first_bad;
				int bad_sectors;
				if (!conf->mirrors[d].rdev ||
				    !test_bit(In_sync, &conf->mirrors[d].rdev->flags))
					continue;
				
				any_working = 1;
				rdev = conf->mirrors[d].rdev;
				sector = r10_bio->devs[j].addr;

				if (is_badblock(rdev, sector, max_sync,
						&first_bad, &bad_sectors)) {
					if (first_bad > sector)
						max_sync = first_bad - sector;
					else {
						bad_sectors -= (sector
								- first_bad);
						if (max_sync > bad_sectors)
							max_sync = bad_sectors;
						continue;
					}
				}
				bio = r10_bio->devs[0].bio;
				bio->bi_next = biolist;
				biolist = bio;
				bio->bi_private = r10_bio;
				bio->bi_end_io = end_sync_read;
				bio->bi_rw = READ;
				from_addr = r10_bio->devs[j].addr;
				bio->bi_sector = from_addr + rdev->data_offset;
				bio->bi_bdev = rdev->bdev;
				atomic_inc(&rdev->nr_pending);
				

				for (k=0; k<conf->copies; k++)
					if (r10_bio->devs[k].devnum == i)
						break;
				BUG_ON(k == conf->copies);
				to_addr = r10_bio->devs[k].addr;
				r10_bio->devs[0].devnum = d;
				r10_bio->devs[0].addr = from_addr;
				r10_bio->devs[1].devnum = i;
				r10_bio->devs[1].addr = to_addr;

				rdev = mirror->rdev;
				if (!test_bit(In_sync, &rdev->flags)) {
					bio = r10_bio->devs[1].bio;
					bio->bi_next = biolist;
					biolist = bio;
					bio->bi_private = r10_bio;
					bio->bi_end_io = end_sync_write;
					bio->bi_rw = WRITE;
					bio->bi_sector = to_addr
						+ rdev->data_offset;
					bio->bi_bdev = rdev->bdev;
					atomic_inc(&r10_bio->remaining);
				} else
					r10_bio->devs[1].bio->bi_end_io = NULL;

				
				bio = r10_bio->devs[1].repl_bio;
				if (bio)
					bio->bi_end_io = NULL;
				rdev = mirror->replacement;
				if (rdev == NULL || bio == NULL ||
				    test_bit(Faulty, &rdev->flags))
					break;
				bio->bi_next = biolist;
				biolist = bio;
				bio->bi_private = r10_bio;
				bio->bi_end_io = end_sync_write;
				bio->bi_rw = WRITE;
				bio->bi_sector = to_addr + rdev->data_offset;
				bio->bi_bdev = rdev->bdev;
				atomic_inc(&r10_bio->remaining);
				break;
			}
			if (j == conf->copies) {
				put_buf(r10_bio);
				if (rb2)
					atomic_dec(&rb2->remaining);
				r10_bio = rb2;
				if (any_working) {
					int k;
					for (k = 0; k < conf->copies; k++)
						if (r10_bio->devs[k].devnum == i)
							break;
					if (!test_bit(In_sync,
						      &mirror->rdev->flags)
					    && !rdev_set_badblocks(
						    mirror->rdev,
						    r10_bio->devs[k].addr,
						    max_sync, 0))
						any_working = 0;
					if (mirror->replacement &&
					    !rdev_set_badblocks(
						    mirror->replacement,
						    r10_bio->devs[k].addr,
						    max_sync, 0))
						any_working = 0;
				}
				if (!any_working)  {
					if (!test_and_set_bit(MD_RECOVERY_INTR,
							      &mddev->recovery))
						printk(KERN_INFO "md/raid10:%s: insufficient "
						       "working devices for recovery.\n",
						       mdname(mddev));
					mirror->recovery_disabled
						= mddev->recovery_disabled;
				}
				break;
			}
		}
		if (biolist == NULL) {
			while (r10_bio) {
				struct r10bio *rb2 = r10_bio;
				r10_bio = (struct r10bio*) rb2->master_bio;
				rb2->master_bio = NULL;
				put_buf(rb2);
			}
			goto giveup;
		}
	} else {
		
		int count = 0;

		bitmap_cond_end_sync(mddev->bitmap, sector_nr);

		if (!bitmap_start_sync(mddev->bitmap, sector_nr,
				       &sync_blocks, mddev->degraded) &&
		    !conf->fullsync && !test_bit(MD_RECOVERY_REQUESTED,
						 &mddev->recovery)) {
			
			*skipped = 1;
			return sync_blocks + sectors_skipped;
		}
		if (sync_blocks < max_sync)
			max_sync = sync_blocks;
		r10_bio = mempool_alloc(conf->r10buf_pool, GFP_NOIO);

		r10_bio->mddev = mddev;
		atomic_set(&r10_bio->remaining, 0);
		raise_barrier(conf, 0);
		conf->next_resync = sector_nr;

		r10_bio->master_bio = NULL;
		r10_bio->sector = sector_nr;
		set_bit(R10BIO_IsSync, &r10_bio->state);
		raid10_find_phys(conf, r10_bio);
		r10_bio->sectors = (sector_nr | conf->chunk_mask) - sector_nr +1;

		for (i=0; i<conf->copies; i++) {
			int d = r10_bio->devs[i].devnum;
			sector_t first_bad, sector;
			int bad_sectors;

			if (r10_bio->devs[i].repl_bio)
				r10_bio->devs[i].repl_bio->bi_end_io = NULL;

			bio = r10_bio->devs[i].bio;
			bio->bi_end_io = NULL;
			clear_bit(BIO_UPTODATE, &bio->bi_flags);
			if (conf->mirrors[d].rdev == NULL ||
			    test_bit(Faulty, &conf->mirrors[d].rdev->flags))
				continue;
			sector = r10_bio->devs[i].addr;
			if (is_badblock(conf->mirrors[d].rdev,
					sector, max_sync,
					&first_bad, &bad_sectors)) {
				if (first_bad > sector)
					max_sync = first_bad - sector;
				else {
					bad_sectors -= (sector - first_bad);
					if (max_sync > bad_sectors)
						max_sync = max_sync;
					continue;
				}
			}
			atomic_inc(&conf->mirrors[d].rdev->nr_pending);
			atomic_inc(&r10_bio->remaining);
			bio->bi_next = biolist;
			biolist = bio;
			bio->bi_private = r10_bio;
			bio->bi_end_io = end_sync_read;
			bio->bi_rw = READ;
			bio->bi_sector = sector +
				conf->mirrors[d].rdev->data_offset;
			bio->bi_bdev = conf->mirrors[d].rdev->bdev;
			count++;

			if (conf->mirrors[d].replacement == NULL ||
			    test_bit(Faulty,
				     &conf->mirrors[d].replacement->flags))
				continue;

			
			bio = r10_bio->devs[i].repl_bio;
			clear_bit(BIO_UPTODATE, &bio->bi_flags);

			sector = r10_bio->devs[i].addr;
			atomic_inc(&conf->mirrors[d].rdev->nr_pending);
			bio->bi_next = biolist;
			biolist = bio;
			bio->bi_private = r10_bio;
			bio->bi_end_io = end_sync_write;
			bio->bi_rw = WRITE;
			bio->bi_sector = sector +
				conf->mirrors[d].replacement->data_offset;
			bio->bi_bdev = conf->mirrors[d].replacement->bdev;
			count++;
		}

		if (count < 2) {
			for (i=0; i<conf->copies; i++) {
				int d = r10_bio->devs[i].devnum;
				if (r10_bio->devs[i].bio->bi_end_io)
					rdev_dec_pending(conf->mirrors[d].rdev,
							 mddev);
				if (r10_bio->devs[i].repl_bio &&
				    r10_bio->devs[i].repl_bio->bi_end_io)
					rdev_dec_pending(
						conf->mirrors[d].replacement,
						mddev);
			}
			put_buf(r10_bio);
			biolist = NULL;
			goto giveup;
		}
	}

	for (bio = biolist; bio ; bio=bio->bi_next) {

		bio->bi_flags &= ~(BIO_POOL_MASK - 1);
		if (bio->bi_end_io)
			bio->bi_flags |= 1 << BIO_UPTODATE;
		bio->bi_vcnt = 0;
		bio->bi_idx = 0;
		bio->bi_phys_segments = 0;
		bio->bi_size = 0;
	}

	nr_sectors = 0;
	if (sector_nr + max_sync < max_sector)
		max_sector = sector_nr + max_sync;
	do {
		struct page *page;
		int len = PAGE_SIZE;
		if (sector_nr + (len>>9) > max_sector)
			len = (max_sector - sector_nr) << 9;
		if (len == 0)
			break;
		for (bio= biolist ; bio ; bio=bio->bi_next) {
			struct bio *bio2;
			page = bio->bi_io_vec[bio->bi_vcnt].bv_page;
			if (bio_add_page(bio, page, len, 0))
				continue;

			
			bio->bi_io_vec[bio->bi_vcnt].bv_page = page;
			for (bio2 = biolist;
			     bio2 && bio2 != bio;
			     bio2 = bio2->bi_next) {
				
				bio2->bi_vcnt--;
				bio2->bi_size -= len;
				bio2->bi_flags &= ~(1<< BIO_SEG_VALID);
			}
			goto bio_full;
		}
		nr_sectors += len>>9;
		sector_nr += len>>9;
	} while (biolist->bi_vcnt < RESYNC_PAGES);
 bio_full:
	r10_bio->sectors = nr_sectors;

	while (biolist) {
		bio = biolist;
		biolist = biolist->bi_next;

		bio->bi_next = NULL;
		r10_bio = bio->bi_private;
		r10_bio->sectors = nr_sectors;

		if (bio->bi_end_io == end_sync_read) {
			md_sync_acct(bio->bi_bdev, nr_sectors);
			generic_make_request(bio);
		}
	}

	if (sectors_skipped)
		md_done_sync(mddev, sectors_skipped, 1);

	return sectors_skipped + nr_sectors;
 giveup:
	if (sector_nr + max_sync < max_sector)
		max_sector = sector_nr + max_sync;

	sectors_skipped += (max_sector - sector_nr);
	chunks_skipped ++;
	sector_nr = max_sector;
	goto skipped;
}

static sector_t
raid10_size(struct mddev *mddev, sector_t sectors, int raid_disks)
{
	sector_t size;
	struct r10conf *conf = mddev->private;

	if (!raid_disks)
		raid_disks = conf->raid_disks;
	if (!sectors)
		sectors = conf->dev_sectors;

	size = sectors >> conf->chunk_shift;
	sector_div(size, conf->far_copies);
	size = size * raid_disks;
	sector_div(size, conf->near_copies);

	return size << conf->chunk_shift;
}

static void calc_sectors(struct r10conf *conf, sector_t size)
{

	size = size >> conf->chunk_shift;
	sector_div(size, conf->far_copies);
	size = size * conf->raid_disks;
	sector_div(size, conf->near_copies);
	
	
	size = size * conf->copies;

	size = DIV_ROUND_UP_SECTOR_T(size, conf->raid_disks);

	conf->dev_sectors = size << conf->chunk_shift;

	if (conf->far_offset)
		conf->stride = 1 << conf->chunk_shift;
	else {
		sector_div(size, conf->far_copies);
		conf->stride = size << conf->chunk_shift;
	}
}

static struct r10conf *setup_conf(struct mddev *mddev)
{
	struct r10conf *conf = NULL;
	int nc, fc, fo;
	int err = -EINVAL;

	if (mddev->new_chunk_sectors < (PAGE_SIZE >> 9) ||
	    !is_power_of_2(mddev->new_chunk_sectors)) {
		printk(KERN_ERR "md/raid10:%s: chunk size must be "
		       "at least PAGE_SIZE(%ld) and be a power of 2.\n",
		       mdname(mddev), PAGE_SIZE);
		goto out;
	}

	nc = mddev->new_layout & 255;
	fc = (mddev->new_layout >> 8) & 255;
	fo = mddev->new_layout & (1<<16);

	if ((nc*fc) <2 || (nc*fc) > mddev->raid_disks ||
	    (mddev->new_layout >> 17)) {
		printk(KERN_ERR "md/raid10:%s: unsupported raid10 layout: 0x%8x\n",
		       mdname(mddev), mddev->new_layout);
		goto out;
	}

	err = -ENOMEM;
	conf = kzalloc(sizeof(struct r10conf), GFP_KERNEL);
	if (!conf)
		goto out;

	conf->mirrors = kzalloc(sizeof(struct mirror_info)*mddev->raid_disks,
				GFP_KERNEL);
	if (!conf->mirrors)
		goto out;

	conf->tmppage = alloc_page(GFP_KERNEL);
	if (!conf->tmppage)
		goto out;


	conf->raid_disks = mddev->raid_disks;
	conf->near_copies = nc;
	conf->far_copies = fc;
	conf->copies = nc*fc;
	conf->far_offset = fo;
	conf->chunk_mask = mddev->new_chunk_sectors - 1;
	conf->chunk_shift = ffz(~mddev->new_chunk_sectors);

	conf->r10bio_pool = mempool_create(NR_RAID10_BIOS, r10bio_pool_alloc,
					   r10bio_pool_free, conf);
	if (!conf->r10bio_pool)
		goto out;

	calc_sectors(conf, mddev->dev_sectors);

	spin_lock_init(&conf->device_lock);
	INIT_LIST_HEAD(&conf->retry_list);

	spin_lock_init(&conf->resync_lock);
	init_waitqueue_head(&conf->wait_barrier);

	conf->thread = md_register_thread(raid10d, mddev, NULL);
	if (!conf->thread)
		goto out;

	conf->mddev = mddev;
	return conf;

 out:
	printk(KERN_ERR "md/raid10:%s: couldn't allocate memory.\n",
	       mdname(mddev));
	if (conf) {
		if (conf->r10bio_pool)
			mempool_destroy(conf->r10bio_pool);
		kfree(conf->mirrors);
		safe_put_page(conf->tmppage);
		kfree(conf);
	}
	return ERR_PTR(err);
}

static int run(struct mddev *mddev)
{
	struct r10conf *conf;
	int i, disk_idx, chunk_size;
	struct mirror_info *disk;
	struct md_rdev *rdev;
	sector_t size;


	if (mddev->private == NULL) {
		conf = setup_conf(mddev);
		if (IS_ERR(conf))
			return PTR_ERR(conf);
		mddev->private = conf;
	}
	conf = mddev->private;
	if (!conf)
		goto out;

	mddev->thread = conf->thread;
	conf->thread = NULL;

	chunk_size = mddev->chunk_sectors << 9;
	blk_queue_io_min(mddev->queue, chunk_size);
	if (conf->raid_disks % conf->near_copies)
		blk_queue_io_opt(mddev->queue, chunk_size * conf->raid_disks);
	else
		blk_queue_io_opt(mddev->queue, chunk_size *
				 (conf->raid_disks / conf->near_copies));

	rdev_for_each(rdev, mddev) {

		disk_idx = rdev->raid_disk;
		if (disk_idx >= conf->raid_disks
		    || disk_idx < 0)
			continue;
		disk = conf->mirrors + disk_idx;

		if (test_bit(Replacement, &rdev->flags)) {
			if (disk->replacement)
				goto out_free_conf;
			disk->replacement = rdev;
		} else {
			if (disk->rdev)
				goto out_free_conf;
			disk->rdev = rdev;
		}

		disk_stack_limits(mddev->gendisk, rdev->bdev,
				  rdev->data_offset << 9);

		disk->head_position = 0;
	}
	
	if (!enough(conf, -1)) {
		printk(KERN_ERR "md/raid10:%s: not enough operational mirrors.\n",
		       mdname(mddev));
		goto out_free_conf;
	}

	mddev->degraded = 0;
	for (i = 0; i < conf->raid_disks; i++) {

		disk = conf->mirrors + i;

		if (!disk->rdev && disk->replacement) {
			
			disk->rdev = disk->replacement;
			disk->replacement = NULL;
			clear_bit(Replacement, &disk->rdev->flags);
		}

		if (!disk->rdev ||
		    !test_bit(In_sync, &disk->rdev->flags)) {
			disk->head_position = 0;
			mddev->degraded++;
			if (disk->rdev)
				conf->fullsync = 1;
		}
		disk->recovery_disabled = mddev->recovery_disabled - 1;
	}

	if (mddev->recovery_cp != MaxSector)
		printk(KERN_NOTICE "md/raid10:%s: not clean"
		       " -- starting background reconstruction\n",
		       mdname(mddev));
	printk(KERN_INFO
		"md/raid10:%s: active with %d out of %d devices\n",
		mdname(mddev), conf->raid_disks - mddev->degraded,
		conf->raid_disks);
	mddev->dev_sectors = conf->dev_sectors;
	size = raid10_size(mddev, 0, 0);
	md_set_array_sectors(mddev, size);
	mddev->resync_max_sectors = size;

	mddev->queue->backing_dev_info.congested_fn = raid10_congested;
	mddev->queue->backing_dev_info.congested_data = mddev;

	{
		int stripe = conf->raid_disks *
			((mddev->chunk_sectors << 9) / PAGE_SIZE);
		stripe /= conf->near_copies;
		if (mddev->queue->backing_dev_info.ra_pages < 2* stripe)
			mddev->queue->backing_dev_info.ra_pages = 2* stripe;
	}

	blk_queue_merge_bvec(mddev->queue, raid10_mergeable_bvec);

	if (md_integrity_register(mddev))
		goto out_free_conf;

	return 0;

out_free_conf:
	md_unregister_thread(&mddev->thread);
	if (conf->r10bio_pool)
		mempool_destroy(conf->r10bio_pool);
	safe_put_page(conf->tmppage);
	kfree(conf->mirrors);
	kfree(conf);
	mddev->private = NULL;
out:
	return -EIO;
}

static int stop(struct mddev *mddev)
{
	struct r10conf *conf = mddev->private;

	raise_barrier(conf, 0);
	lower_barrier(conf);

	md_unregister_thread(&mddev->thread);
	blk_sync_queue(mddev->queue); 
	if (conf->r10bio_pool)
		mempool_destroy(conf->r10bio_pool);
	kfree(conf->mirrors);
	kfree(conf);
	mddev->private = NULL;
	return 0;
}

static void raid10_quiesce(struct mddev *mddev, int state)
{
	struct r10conf *conf = mddev->private;

	switch(state) {
	case 1:
		raise_barrier(conf, 0);
		break;
	case 0:
		lower_barrier(conf);
		break;
	}
}

static int raid10_resize(struct mddev *mddev, sector_t sectors)
{
	struct r10conf *conf = mddev->private;
	sector_t oldsize, size;

	if (conf->far_copies > 1 && !conf->far_offset)
		return -EINVAL;

	oldsize = raid10_size(mddev, 0, 0);
	size = raid10_size(mddev, sectors, 0);
	md_set_array_sectors(mddev, size);
	if (mddev->array_sectors > size)
		return -EINVAL;
	set_capacity(mddev->gendisk, mddev->array_sectors);
	revalidate_disk(mddev->gendisk);
	if (sectors > mddev->dev_sectors &&
	    mddev->recovery_cp > oldsize) {
		mddev->recovery_cp = oldsize;
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	}
	calc_sectors(conf, sectors);
	mddev->dev_sectors = conf->dev_sectors;
	mddev->resync_max_sectors = size;
	return 0;
}

static void *raid10_takeover_raid0(struct mddev *mddev)
{
	struct md_rdev *rdev;
	struct r10conf *conf;

	if (mddev->degraded > 0) {
		printk(KERN_ERR "md/raid10:%s: Error: degraded raid0!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	
	mddev->new_level = 10;
	
	mddev->new_layout = (1<<8) + 2;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->delta_disks = mddev->raid_disks;
	mddev->raid_disks *= 2;
	
	mddev->recovery_cp = MaxSector;

	conf = setup_conf(mddev);
	if (!IS_ERR(conf)) {
		rdev_for_each(rdev, mddev)
			if (rdev->raid_disk >= 0)
				rdev->new_raid_disk = rdev->raid_disk * 2;
		conf->barrier = 1;
	}

	return conf;
}

static void *raid10_takeover(struct mddev *mddev)
{
	struct r0conf *raid0_conf;

	if (mddev->level == 0) {
		
		raid0_conf = mddev->private;
		if (raid0_conf->nr_strip_zones > 1) {
			printk(KERN_ERR "md/raid10:%s: cannot takeover raid 0"
			       " with more than one zone.\n",
			       mdname(mddev));
			return ERR_PTR(-EINVAL);
		}
		return raid10_takeover_raid0(mddev);
	}
	return ERR_PTR(-EINVAL);
}

static struct md_personality raid10_personality =
{
	.name		= "raid10",
	.level		= 10,
	.owner		= THIS_MODULE,
	.make_request	= make_request,
	.run		= run,
	.stop		= stop,
	.status		= status,
	.error_handler	= error,
	.hot_add_disk	= raid10_add_disk,
	.hot_remove_disk= raid10_remove_disk,
	.spare_active	= raid10_spare_active,
	.sync_request	= sync_request,
	.quiesce	= raid10_quiesce,
	.size		= raid10_size,
	.resize		= raid10_resize,
	.takeover	= raid10_takeover,
};

static int __init raid_init(void)
{
	return register_md_personality(&raid10_personality);
}

static void raid_exit(void)
{
	unregister_md_personality(&raid10_personality);
}

module_init(raid_init);
module_exit(raid_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RAID10 (striped mirror) personality for MD");
MODULE_ALIAS("md-personality-9"); 
MODULE_ALIAS("md-raid10");
MODULE_ALIAS("md-level-10");

module_param(max_queued_requests, int, S_IRUGO|S_IWUSR);
