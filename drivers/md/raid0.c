/*
   raid0.c : Multiple Devices driver for Linux
             Copyright (C) 1994-96 Marc ZYNGIER
	     <zyngier@ufr-info-p7.ibp.fr> or
	     <maz@gloups.fdn.fr>
             Copyright (C) 1999, 2000 Ingo Molnar, Red Hat


   RAID-0 management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/blkdev.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "md.h"
#include "raid0.h"
#include "raid5.h"

static int raid0_congested(void *data, int bits)
{
	struct mddev *mddev = data;
	struct r0conf *conf = mddev->private;
	struct md_rdev **devlist = conf->devlist;
	int raid_disks = conf->strip_zone[0].nb_dev;
	int i, ret = 0;

	if (mddev_congested(mddev, bits))
		return 1;

	for (i = 0; i < raid_disks && !ret ; i++) {
		struct request_queue *q = bdev_get_queue(devlist[i]->bdev);

		ret |= bdi_congested(&q->backing_dev_info, bits);
	}
	return ret;
}

static void dump_zones(struct mddev *mddev)
{
	int j, k;
	sector_t zone_size = 0;
	sector_t zone_start = 0;
	char b[BDEVNAME_SIZE];
	struct r0conf *conf = mddev->private;
	int raid_disks = conf->strip_zone[0].nb_dev;
	printk(KERN_INFO "md: RAID0 configuration for %s - %d zone%s\n",
	       mdname(mddev),
	       conf->nr_strip_zones, conf->nr_strip_zones==1?"":"s");
	for (j = 0; j < conf->nr_strip_zones; j++) {
		printk(KERN_INFO "md: zone%d=[", j);
		for (k = 0; k < conf->strip_zone[j].nb_dev; k++)
			printk(KERN_CONT "%s%s", k?"/":"",
			bdevname(conf->devlist[j*raid_disks
						+ k]->bdev, b));
		printk(KERN_CONT "]\n");

		zone_size  = conf->strip_zone[j].zone_end - zone_start;
		printk(KERN_INFO "      zone-offset=%10lluKB, "
				"device-offset=%10lluKB, size=%10lluKB\n",
			(unsigned long long)zone_start>>1,
			(unsigned long long)conf->strip_zone[j].dev_start>>1,
			(unsigned long long)zone_size>>1);
		zone_start = conf->strip_zone[j].zone_end;
	}
	printk(KERN_INFO "\n");
}

static int create_strip_zones(struct mddev *mddev, struct r0conf **private_conf)
{
	int i, c, err;
	sector_t curr_zone_end, sectors;
	struct md_rdev *smallest, *rdev1, *rdev2, *rdev, **dev;
	struct strip_zone *zone;
	int cnt;
	char b[BDEVNAME_SIZE];
	char b2[BDEVNAME_SIZE];
	struct r0conf *conf = kzalloc(sizeof(*conf), GFP_KERNEL);

	if (!conf)
		return -ENOMEM;
	rdev_for_each(rdev1, mddev) {
		pr_debug("md/raid0:%s: looking at %s\n",
			 mdname(mddev),
			 bdevname(rdev1->bdev, b));
		c = 0;

		
		sectors = rdev1->sectors;
		sector_div(sectors, mddev->chunk_sectors);
		rdev1->sectors = sectors * mddev->chunk_sectors;

		rdev_for_each(rdev2, mddev) {
			pr_debug("md/raid0:%s:   comparing %s(%llu)"
				 " with %s(%llu)\n",
				 mdname(mddev),
				 bdevname(rdev1->bdev,b),
				 (unsigned long long)rdev1->sectors,
				 bdevname(rdev2->bdev,b2),
				 (unsigned long long)rdev2->sectors);
			if (rdev2 == rdev1) {
				pr_debug("md/raid0:%s:   END\n",
					 mdname(mddev));
				break;
			}
			if (rdev2->sectors == rdev1->sectors) {
				pr_debug("md/raid0:%s:   EQUAL\n",
					 mdname(mddev));
				c = 1;
				break;
			}
			pr_debug("md/raid0:%s:   NOT EQUAL\n",
				 mdname(mddev));
		}
		if (!c) {
			pr_debug("md/raid0:%s:   ==> UNIQUE\n",
				 mdname(mddev));
			conf->nr_strip_zones++;
			pr_debug("md/raid0:%s: %d zones\n",
				 mdname(mddev), conf->nr_strip_zones);
		}
	}
	pr_debug("md/raid0:%s: FINAL %d zones\n",
		 mdname(mddev), conf->nr_strip_zones);
	err = -ENOMEM;
	conf->strip_zone = kzalloc(sizeof(struct strip_zone)*
				conf->nr_strip_zones, GFP_KERNEL);
	if (!conf->strip_zone)
		goto abort;
	conf->devlist = kzalloc(sizeof(struct md_rdev*)*
				conf->nr_strip_zones*mddev->raid_disks,
				GFP_KERNEL);
	if (!conf->devlist)
		goto abort;

	zone = &conf->strip_zone[0];
	cnt = 0;
	smallest = NULL;
	dev = conf->devlist;
	err = -EINVAL;
	rdev_for_each(rdev1, mddev) {
		int j = rdev1->raid_disk;

		if (mddev->level == 10) {
			
			j /= 2;
			rdev1->new_raid_disk = j;
		}

		if (mddev->level == 1) {
			j = 0;
			rdev1->new_raid_disk = j;
		}

		if (j < 0 || j >= mddev->raid_disks) {
			printk(KERN_ERR "md/raid0:%s: bad disk number %d - "
			       "aborting!\n", mdname(mddev), j);
			goto abort;
		}
		if (dev[j]) {
			printk(KERN_ERR "md/raid0:%s: multiple devices for %d - "
			       "aborting!\n", mdname(mddev), j);
			goto abort;
		}
		dev[j] = rdev1;

		disk_stack_limits(mddev->gendisk, rdev1->bdev,
				  rdev1->data_offset << 9);

		if (rdev1->bdev->bd_disk->queue->merge_bvec_fn)
			conf->has_merge_bvec = 1;

		if (!smallest || (rdev1->sectors < smallest->sectors))
			smallest = rdev1;
		cnt++;
	}
	if (cnt != mddev->raid_disks) {
		printk(KERN_ERR "md/raid0:%s: too few disks (%d of %d) - "
		       "aborting!\n", mdname(mddev), cnt, mddev->raid_disks);
		goto abort;
	}
	zone->nb_dev = cnt;
	zone->zone_end = smallest->sectors * cnt;

	curr_zone_end = zone->zone_end;

	
	for (i = 1; i < conf->nr_strip_zones; i++)
	{
		int j;

		zone = conf->strip_zone + i;
		dev = conf->devlist + i * mddev->raid_disks;

		pr_debug("md/raid0:%s: zone %d\n", mdname(mddev), i);
		zone->dev_start = smallest->sectors;
		smallest = NULL;
		c = 0;

		for (j=0; j<cnt; j++) {
			rdev = conf->devlist[j];
			if (rdev->sectors <= zone->dev_start) {
				pr_debug("md/raid0:%s: checking %s ... nope\n",
					 mdname(mddev),
					 bdevname(rdev->bdev, b));
				continue;
			}
			pr_debug("md/raid0:%s: checking %s ..."
				 " contained as device %d\n",
				 mdname(mddev),
				 bdevname(rdev->bdev, b), c);
			dev[c] = rdev;
			c++;
			if (!smallest || rdev->sectors < smallest->sectors) {
				smallest = rdev;
				pr_debug("md/raid0:%s:  (%llu) is smallest!.\n",
					 mdname(mddev),
					 (unsigned long long)rdev->sectors);
			}
		}

		zone->nb_dev = c;
		sectors = (smallest->sectors - zone->dev_start) * c;
		pr_debug("md/raid0:%s: zone->nb_dev: %d, sectors: %llu\n",
			 mdname(mddev),
			 zone->nb_dev, (unsigned long long)sectors);

		curr_zone_end += sectors;
		zone->zone_end = curr_zone_end;

		pr_debug("md/raid0:%s: current zone start: %llu\n",
			 mdname(mddev),
			 (unsigned long long)smallest->sectors);
	}
	mddev->queue->backing_dev_info.congested_fn = raid0_congested;
	mddev->queue->backing_dev_info.congested_data = mddev;

	if ((mddev->chunk_sectors << 9) % queue_logical_block_size(mddev->queue)) {
		printk(KERN_ERR "md/raid0:%s: chunk_size of %d not valid\n",
		       mdname(mddev),
		       mddev->chunk_sectors << 9);
		goto abort;
	}

	blk_queue_io_min(mddev->queue, mddev->chunk_sectors << 9);
	blk_queue_io_opt(mddev->queue,
			 (mddev->chunk_sectors << 9) * mddev->raid_disks);

	pr_debug("md/raid0:%s: done.\n", mdname(mddev));
	*private_conf = conf;

	return 0;
abort:
	kfree(conf->strip_zone);
	kfree(conf->devlist);
	kfree(conf);
	*private_conf = NULL;
	return err;
}

static struct strip_zone *find_zone(struct r0conf *conf,
				    sector_t *sectorp)
{
	int i;
	struct strip_zone *z = conf->strip_zone;
	sector_t sector = *sectorp;

	for (i = 0; i < conf->nr_strip_zones; i++)
		if (sector < z[i].zone_end) {
			if (i)
				*sectorp = sector - z[i-1].zone_end;
			return z + i;
		}
	BUG();
}

static struct md_rdev *map_sector(struct mddev *mddev, struct strip_zone *zone,
				sector_t sector, sector_t *sector_offset)
{
	unsigned int sect_in_chunk;
	sector_t chunk;
	struct r0conf *conf = mddev->private;
	int raid_disks = conf->strip_zone[0].nb_dev;
	unsigned int chunk_sects = mddev->chunk_sectors;

	if (is_power_of_2(chunk_sects)) {
		int chunksect_bits = ffz(~chunk_sects);
		
		sect_in_chunk  = sector & (chunk_sects - 1);
		sector >>= chunksect_bits;
		
		chunk = *sector_offset;
		
		sector_div(chunk, zone->nb_dev << chunksect_bits);
	} else{
		sect_in_chunk = sector_div(sector, chunk_sects);
		chunk = *sector_offset;
		sector_div(chunk, chunk_sects * zone->nb_dev);
	}
	*sector_offset = (chunk * chunk_sects) + sect_in_chunk;
	return conf->devlist[(zone - conf->strip_zone)*raid_disks
			     + sector_div(sector, zone->nb_dev)];
}

static int raid0_mergeable_bvec(struct request_queue *q,
				struct bvec_merge_data *bvm,
				struct bio_vec *biovec)
{
	struct mddev *mddev = q->queuedata;
	struct r0conf *conf = mddev->private;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	sector_t sector_offset = sector;
	int max;
	unsigned int chunk_sectors = mddev->chunk_sectors;
	unsigned int bio_sectors = bvm->bi_size >> 9;
	struct strip_zone *zone;
	struct md_rdev *rdev;
	struct request_queue *subq;

	if (is_power_of_2(chunk_sectors))
		max =  (chunk_sectors - ((sector & (chunk_sectors-1))
						+ bio_sectors)) << 9;
	else
		max =  (chunk_sectors - (sector_div(sector, chunk_sectors)
						+ bio_sectors)) << 9;
	if (max < 0)
		max = 0; 
	if (max <= biovec->bv_len && bio_sectors == 0)
		return biovec->bv_len;
	if (max < biovec->bv_len)
		
		return max;
	if (!conf->has_merge_bvec)
		return max;

	
	sector = sector_offset;
	zone = find_zone(mddev->private, &sector_offset);
	rdev = map_sector(mddev, zone, sector, &sector_offset);
	subq = bdev_get_queue(rdev->bdev);
	if (subq->merge_bvec_fn) {
		bvm->bi_bdev = rdev->bdev;
		bvm->bi_sector = sector_offset + zone->dev_start +
			rdev->data_offset;
		return min(max, subq->merge_bvec_fn(subq, bvm, biovec));
	} else
		return max;
}

static sector_t raid0_size(struct mddev *mddev, sector_t sectors, int raid_disks)
{
	sector_t array_sectors = 0;
	struct md_rdev *rdev;

	WARN_ONCE(sectors || raid_disks,
		  "%s does not support generic reshape\n", __func__);

	rdev_for_each(rdev, mddev)
		array_sectors += rdev->sectors;

	return array_sectors;
}

static int raid0_stop(struct mddev *mddev);

static int raid0_run(struct mddev *mddev)
{
	struct r0conf *conf;
	int ret;

	if (mddev->chunk_sectors == 0) {
		printk(KERN_ERR "md/raid0:%s: chunk size must be set.\n",
		       mdname(mddev));
		return -EINVAL;
	}
	if (md_check_no_bitmap(mddev))
		return -EINVAL;
	blk_queue_max_hw_sectors(mddev->queue, mddev->chunk_sectors);

	
	if (mddev->private == NULL) {
		ret = create_strip_zones(mddev, &conf);
		if (ret < 0)
			return ret;
		mddev->private = conf;
	}
	conf = mddev->private;

	
	md_set_array_sectors(mddev, raid0_size(mddev, 0, 0));

	printk(KERN_INFO "md/raid0:%s: md_size is %llu sectors.\n",
	       mdname(mddev),
	       (unsigned long long)mddev->array_sectors);
	{
		int stripe = mddev->raid_disks *
			(mddev->chunk_sectors << 9) / PAGE_SIZE;
		if (mddev->queue->backing_dev_info.ra_pages < 2* stripe)
			mddev->queue->backing_dev_info.ra_pages = 2* stripe;
	}

	blk_queue_merge_bvec(mddev->queue, raid0_mergeable_bvec);
	dump_zones(mddev);

	ret = md_integrity_register(mddev);
	if (ret)
		raid0_stop(mddev);

	return ret;
}

static int raid0_stop(struct mddev *mddev)
{
	struct r0conf *conf = mddev->private;

	blk_sync_queue(mddev->queue); 
	kfree(conf->strip_zone);
	kfree(conf->devlist);
	kfree(conf);
	mddev->private = NULL;
	return 0;
}

static inline int is_io_in_chunk_boundary(struct mddev *mddev,
			unsigned int chunk_sects, struct bio *bio)
{
	if (likely(is_power_of_2(chunk_sects))) {
		return chunk_sects >= ((bio->bi_sector & (chunk_sects-1))
					+ (bio->bi_size >> 9));
	} else{
		sector_t sector = bio->bi_sector;
		return chunk_sects >= (sector_div(sector, chunk_sects)
						+ (bio->bi_size >> 9));
	}
}

static void raid0_make_request(struct mddev *mddev, struct bio *bio)
{
	unsigned int chunk_sects;
	sector_t sector_offset;
	struct strip_zone *zone;
	struct md_rdev *tmp_dev;

	if (unlikely(bio->bi_rw & REQ_FLUSH)) {
		md_flush_request(mddev, bio);
		return;
	}

	chunk_sects = mddev->chunk_sectors;
	if (unlikely(!is_io_in_chunk_boundary(mddev, chunk_sects, bio))) {
		sector_t sector = bio->bi_sector;
		struct bio_pair *bp;
		
		if (bio->bi_vcnt != 1 ||
		    bio->bi_idx != 0)
			goto bad_map;
		if (likely(is_power_of_2(chunk_sects)))
			bp = bio_split(bio, chunk_sects - (sector &
							   (chunk_sects-1)));
		else
			bp = bio_split(bio, chunk_sects -
				       sector_div(sector, chunk_sects));
		raid0_make_request(mddev, &bp->bio1);
		raid0_make_request(mddev, &bp->bio2);
		bio_pair_release(bp);
		return;
	}

	sector_offset = bio->bi_sector;
	zone = find_zone(mddev->private, &sector_offset);
	tmp_dev = map_sector(mddev, zone, bio->bi_sector,
			     &sector_offset);
	bio->bi_bdev = tmp_dev->bdev;
	bio->bi_sector = sector_offset + zone->dev_start +
		tmp_dev->data_offset;

	generic_make_request(bio);
	return;

bad_map:
	printk("md/raid0:%s: make_request bug: can't convert block across chunks"
	       " or bigger than %dk %llu %d\n",
	       mdname(mddev), chunk_sects / 2,
	       (unsigned long long)bio->bi_sector, bio->bi_size >> 10);

	bio_io_error(bio);
	return;
}

static void raid0_status(struct seq_file *seq, struct mddev *mddev)
{
	seq_printf(seq, " %dk chunks", mddev->chunk_sectors / 2);
	return;
}

static void *raid0_takeover_raid45(struct mddev *mddev)
{
	struct md_rdev *rdev;
	struct r0conf *priv_conf;

	if (mddev->degraded != 1) {
		printk(KERN_ERR "md/raid0:%s: raid5 must be degraded! Degraded disks: %d\n",
		       mdname(mddev),
		       mddev->degraded);
		return ERR_PTR(-EINVAL);
	}

	rdev_for_each(rdev, mddev) {
		
		if (rdev->raid_disk == mddev->raid_disks-1) {
			printk(KERN_ERR "md/raid0:%s: raid5 must have missing parity disk!\n",
			       mdname(mddev));
			return ERR_PTR(-EINVAL);
		}
	}

	
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->raid_disks--;
	mddev->delta_disks = -1;
	
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover_raid10(struct mddev *mddev)
{
	struct r0conf *priv_conf;

	if (mddev->layout != ((1 << 8) + 2)) {
		printk(KERN_ERR "md/raid0:%s:: Raid0 cannot takover layout: 0x%x\n",
		       mdname(mddev),
		       mddev->layout);
		return ERR_PTR(-EINVAL);
	}
	if (mddev->raid_disks & 1) {
		printk(KERN_ERR "md/raid0:%s: Raid0 cannot takover Raid10 with odd disk number.\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}
	if (mddev->degraded != (mddev->raid_disks>>1)) {
		printk(KERN_ERR "md/raid0:%s: All mirrors must be already degraded!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->delta_disks = - mddev->raid_disks / 2;
	mddev->raid_disks += mddev->delta_disks;
	mddev->degraded = 0;
	
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover_raid1(struct mddev *mddev)
{
	struct r0conf *priv_conf;
	int chunksect;

	if ((mddev->raid_disks - 1) != mddev->degraded) {
		printk(KERN_ERR "md/raid0:%s: (N - 1) mirrors drives must be already faulty!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	chunksect = 64 * 2; 

	
	while (chunksect && (mddev->array_sectors & (chunksect - 1)))
		chunksect >>= 1;

	if ((chunksect << 9) < PAGE_SIZE)
		
		return ERR_PTR(-EINVAL);

	
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = chunksect;
	mddev->chunk_sectors = chunksect;
	mddev->delta_disks = 1 - mddev->raid_disks;
	mddev->raid_disks = 1;
	
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover(struct mddev *mddev)
{
	if (mddev->level == 4)
		return raid0_takeover_raid45(mddev);

	if (mddev->level == 5) {
		if (mddev->layout == ALGORITHM_PARITY_N)
			return raid0_takeover_raid45(mddev);

		printk(KERN_ERR "md/raid0:%s: Raid can only takeover Raid5 with layout: %d\n",
		       mdname(mddev), ALGORITHM_PARITY_N);
	}

	if (mddev->level == 10)
		return raid0_takeover_raid10(mddev);

	if (mddev->level == 1)
		return raid0_takeover_raid1(mddev);

	printk(KERN_ERR "Takeover from raid%i to raid0 not supported\n",
		mddev->level);

	return ERR_PTR(-EINVAL);
}

static void raid0_quiesce(struct mddev *mddev, int state)
{
}

static struct md_personality raid0_personality=
{
	.name		= "raid0",
	.level		= 0,
	.owner		= THIS_MODULE,
	.make_request	= raid0_make_request,
	.run		= raid0_run,
	.stop		= raid0_stop,
	.status		= raid0_status,
	.size		= raid0_size,
	.takeover	= raid0_takeover,
	.quiesce	= raid0_quiesce,
};

static int __init raid0_init (void)
{
	return register_md_personality (&raid0_personality);
}

static void raid0_exit (void)
{
	unregister_md_personality (&raid0_personality);
}

module_init(raid0_init);
module_exit(raid0_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RAID0 (striping) personality for MD");
MODULE_ALIAS("md-personality-2"); 
MODULE_ALIAS("md-raid0");
MODULE_ALIAS("md-level-0");
