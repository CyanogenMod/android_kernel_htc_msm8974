/*
 * bio-integrity.c - bio data integrity extensions
 *
 * Copyright (C) 2007, 2008, 2009 Oracle Corporation
 * Written by: Martin K. Petersen <martin.petersen@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
 */

#include <linux/blkdev.h>
#include <linux/mempool.h>
#include <linux/export.h>
#include <linux/bio.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

struct integrity_slab {
	struct kmem_cache *slab;
	unsigned short nr_vecs;
	char name[8];
};

#define IS(x) { .nr_vecs = x, .name = "bip-"__stringify(x) }
struct integrity_slab bip_slab[BIOVEC_NR_POOLS] __read_mostly = {
	IS(1), IS(4), IS(16), IS(64), IS(128), IS(BIO_MAX_PAGES),
};
#undef IS

static struct workqueue_struct *kintegrityd_wq;

static inline unsigned int vecs_to_idx(unsigned int nr)
{
	switch (nr) {
	case 1:
		return 0;
	case 2 ... 4:
		return 1;
	case 5 ... 16:
		return 2;
	case 17 ... 64:
		return 3;
	case 65 ... 128:
		return 4;
	case 129 ... BIO_MAX_PAGES:
		return 5;
	default:
		BUG();
	}
}

static inline int use_bip_pool(unsigned int idx)
{
	if (idx == BIOVEC_MAX_IDX)
		return 1;

	return 0;
}

struct bio_integrity_payload *bio_integrity_alloc_bioset(struct bio *bio,
							 gfp_t gfp_mask,
							 unsigned int nr_vecs,
							 struct bio_set *bs)
{
	struct bio_integrity_payload *bip;
	unsigned int idx = vecs_to_idx(nr_vecs);

	BUG_ON(bio == NULL);
	bip = NULL;

	
	if (!use_bip_pool(idx))
		bip = kmem_cache_alloc(bip_slab[idx].slab, gfp_mask);

	
	if (bip == NULL) {
		idx = BIOVEC_MAX_IDX;  
		bip = mempool_alloc(bs->bio_integrity_pool, gfp_mask);

		if (unlikely(bip == NULL)) {
			printk(KERN_ERR "%s: could not alloc bip\n", __func__);
			return NULL;
		}
	}

	memset(bip, 0, sizeof(*bip));

	bip->bip_slab = idx;
	bip->bip_bio = bio;
	bio->bi_integrity = bip;

	return bip;
}
EXPORT_SYMBOL(bio_integrity_alloc_bioset);

struct bio_integrity_payload *bio_integrity_alloc(struct bio *bio,
						  gfp_t gfp_mask,
						  unsigned int nr_vecs)
{
	return bio_integrity_alloc_bioset(bio, gfp_mask, nr_vecs, fs_bio_set);
}
EXPORT_SYMBOL(bio_integrity_alloc);

void bio_integrity_free(struct bio *bio, struct bio_set *bs)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;

	BUG_ON(bip == NULL);

	
	if (!bio_flagged(bio, BIO_CLONED) && !bio_flagged(bio, BIO_FS_INTEGRITY)
	    && bip->bip_buf != NULL)
		kfree(bip->bip_buf);

	if (use_bip_pool(bip->bip_slab))
		mempool_free(bip, bs->bio_integrity_pool);
	else
		kmem_cache_free(bip_slab[bip->bip_slab].slab, bip);

	bio->bi_integrity = NULL;
}
EXPORT_SYMBOL(bio_integrity_free);

int bio_integrity_add_page(struct bio *bio, struct page *page,
			   unsigned int len, unsigned int offset)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;
	struct bio_vec *iv;

	if (bip->bip_vcnt >= bvec_nr_vecs(bip->bip_slab)) {
		printk(KERN_ERR "%s: bip_vec full\n", __func__);
		return 0;
	}

	iv = bip_vec_idx(bip, bip->bip_vcnt);
	BUG_ON(iv == NULL);

	iv->bv_page = page;
	iv->bv_len = len;
	iv->bv_offset = offset;
	bip->bip_vcnt++;

	return len;
}
EXPORT_SYMBOL(bio_integrity_add_page);

static int bdev_integrity_enabled(struct block_device *bdev, int rw)
{
	struct blk_integrity *bi = bdev_get_integrity(bdev);

	if (bi == NULL)
		return 0;

	if (rw == READ && bi->verify_fn != NULL &&
	    (bi->flags & INTEGRITY_FLAG_READ))
		return 1;

	if (rw == WRITE && bi->generate_fn != NULL &&
	    (bi->flags & INTEGRITY_FLAG_WRITE))
		return 1;

	return 0;
}

int bio_integrity_enabled(struct bio *bio)
{
	
	if (bio_integrity(bio))
		return 0;

	return bdev_integrity_enabled(bio->bi_bdev, bio_data_dir(bio));
}
EXPORT_SYMBOL(bio_integrity_enabled);

static inline unsigned int bio_integrity_hw_sectors(struct blk_integrity *bi,
						    unsigned int sectors)
{
	
	if (bi->sector_size == 4096)
		return sectors >>= 3;

	return sectors;
}

unsigned int bio_integrity_tag_size(struct bio *bio)
{
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);

	BUG_ON(bio->bi_size == 0);

	return bi->tag_size * (bio->bi_size / bi->sector_size);
}
EXPORT_SYMBOL(bio_integrity_tag_size);

int bio_integrity_tag(struct bio *bio, void *tag_buf, unsigned int len, int set)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);
	unsigned int nr_sectors;

	BUG_ON(bip->bip_buf == NULL);

	if (bi->tag_size == 0)
		return -1;

	nr_sectors = bio_integrity_hw_sectors(bi,
					DIV_ROUND_UP(len, bi->tag_size));

	if (nr_sectors * bi->tuple_size > bip->bip_size) {
		printk(KERN_ERR "%s: tag too big for bio: %u > %u\n",
		       __func__, nr_sectors * bi->tuple_size, bip->bip_size);
		return -1;
	}

	if (set)
		bi->set_tag_fn(bip->bip_buf, tag_buf, nr_sectors);
	else
		bi->get_tag_fn(bip->bip_buf, tag_buf, nr_sectors);

	return 0;
}

int bio_integrity_set_tag(struct bio *bio, void *tag_buf, unsigned int len)
{
	BUG_ON(bio_data_dir(bio) != WRITE);

	return bio_integrity_tag(bio, tag_buf, len, 1);
}
EXPORT_SYMBOL(bio_integrity_set_tag);

int bio_integrity_get_tag(struct bio *bio, void *tag_buf, unsigned int len)
{
	BUG_ON(bio_data_dir(bio) != READ);

	return bio_integrity_tag(bio, tag_buf, len, 0);
}
EXPORT_SYMBOL(bio_integrity_get_tag);

static void bio_integrity_generate(struct bio *bio)
{
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);
	struct blk_integrity_exchg bix;
	struct bio_vec *bv;
	sector_t sector = bio->bi_sector;
	unsigned int i, sectors, total;
	void *prot_buf = bio->bi_integrity->bip_buf;

	total = 0;
	bix.disk_name = bio->bi_bdev->bd_disk->disk_name;
	bix.sector_size = bi->sector_size;

	bio_for_each_segment(bv, bio, i) {
		void *kaddr = kmap_atomic(bv->bv_page);
		bix.data_buf = kaddr + bv->bv_offset;
		bix.data_size = bv->bv_len;
		bix.prot_buf = prot_buf;
		bix.sector = sector;

		bi->generate_fn(&bix);

		sectors = bv->bv_len / bi->sector_size;
		sector += sectors;
		prot_buf += sectors * bi->tuple_size;
		total += sectors * bi->tuple_size;
		BUG_ON(total > bio->bi_integrity->bip_size);

		kunmap_atomic(kaddr);
	}
}

static inline unsigned short blk_integrity_tuple_size(struct blk_integrity *bi)
{
	if (bi)
		return bi->tuple_size;

	return 0;
}

int bio_integrity_prep(struct bio *bio)
{
	struct bio_integrity_payload *bip;
	struct blk_integrity *bi;
	struct request_queue *q;
	void *buf;
	unsigned long start, end;
	unsigned int len, nr_pages;
	unsigned int bytes, offset, i;
	unsigned int sectors;

	bi = bdev_get_integrity(bio->bi_bdev);
	q = bdev_get_queue(bio->bi_bdev);
	BUG_ON(bi == NULL);
	BUG_ON(bio_integrity(bio));

	sectors = bio_integrity_hw_sectors(bi, bio_sectors(bio));

	
	len = sectors * blk_integrity_tuple_size(bi);
	buf = kmalloc(len, GFP_NOIO | q->bounce_gfp);
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "could not allocate integrity buffer\n");
		return -ENOMEM;
	}

	end = (((unsigned long) buf) + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	start = ((unsigned long) buf) >> PAGE_SHIFT;
	nr_pages = end - start;

	
	bip = bio_integrity_alloc(bio, GFP_NOIO, nr_pages);
	if (unlikely(bip == NULL)) {
		printk(KERN_ERR "could not allocate data integrity bioset\n");
		kfree(buf);
		return -EIO;
	}

	bip->bip_buf = buf;
	bip->bip_size = len;
	bip->bip_sector = bio->bi_sector;

	
	offset = offset_in_page(buf);
	for (i = 0 ; i < nr_pages ; i++) {
		int ret;
		bytes = PAGE_SIZE - offset;

		if (len <= 0)
			break;

		if (bytes > len)
			bytes = len;

		ret = bio_integrity_add_page(bio, virt_to_page(buf),
					     bytes, offset);

		if (ret == 0)
			return 0;

		if (ret < bytes)
			break;

		buf += bytes;
		len -= bytes;
		offset = 0;
	}

	
	if (bio_data_dir(bio) == READ) {
		bip->bip_end_io = bio->bi_end_io;
		bio->bi_end_io = bio_integrity_endio;
	}

	
	if (bio_data_dir(bio) == WRITE)
		bio_integrity_generate(bio);

	return 0;
}
EXPORT_SYMBOL(bio_integrity_prep);

static int bio_integrity_verify(struct bio *bio)
{
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);
	struct blk_integrity_exchg bix;
	struct bio_vec *bv;
	sector_t sector = bio->bi_integrity->bip_sector;
	unsigned int i, sectors, total, ret;
	void *prot_buf = bio->bi_integrity->bip_buf;

	ret = total = 0;
	bix.disk_name = bio->bi_bdev->bd_disk->disk_name;
	bix.sector_size = bi->sector_size;

	bio_for_each_segment(bv, bio, i) {
		void *kaddr = kmap_atomic(bv->bv_page);
		bix.data_buf = kaddr + bv->bv_offset;
		bix.data_size = bv->bv_len;
		bix.prot_buf = prot_buf;
		bix.sector = sector;

		ret = bi->verify_fn(&bix);

		if (ret) {
			kunmap_atomic(kaddr);
			return ret;
		}

		sectors = bv->bv_len / bi->sector_size;
		sector += sectors;
		prot_buf += sectors * bi->tuple_size;
		total += sectors * bi->tuple_size;
		BUG_ON(total > bio->bi_integrity->bip_size);

		kunmap_atomic(kaddr);
	}

	return ret;
}

static void bio_integrity_verify_fn(struct work_struct *work)
{
	struct bio_integrity_payload *bip =
		container_of(work, struct bio_integrity_payload, bip_work);
	struct bio *bio = bip->bip_bio;
	int error;

	error = bio_integrity_verify(bio);

	
	bio->bi_end_io = bip->bip_end_io;
	bio_endio(bio, error);
}

void bio_integrity_endio(struct bio *bio, int error)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;

	BUG_ON(bip->bip_bio != bio);

	if (error) {
		bio->bi_end_io = bip->bip_end_io;
		bio_endio(bio, error);

		return;
	}

	INIT_WORK(&bip->bip_work, bio_integrity_verify_fn);
	queue_work(kintegrityd_wq, &bip->bip_work);
}
EXPORT_SYMBOL(bio_integrity_endio);

void bio_integrity_mark_head(struct bio_integrity_payload *bip,
			     unsigned int skip)
{
	struct bio_vec *iv;
	unsigned int i;

	bip_for_each_vec(iv, bip, i) {
		if (skip == 0) {
			bip->bip_idx = i;
			return;
		} else if (skip >= iv->bv_len) {
			skip -= iv->bv_len;
		} else { 
			iv->bv_offset += skip;
			iv->bv_len -= skip;
			bip->bip_idx = i;
			return;
		}
	}
}

void bio_integrity_mark_tail(struct bio_integrity_payload *bip,
			     unsigned int len)
{
	struct bio_vec *iv;
	unsigned int i;

	bip_for_each_vec(iv, bip, i) {
		if (len == 0) {
			bip->bip_vcnt = i;
			return;
		} else if (len >= iv->bv_len) {
			len -= iv->bv_len;
		} else { 
			iv->bv_len = len;
			len = 0;
		}
	}
}

void bio_integrity_advance(struct bio *bio, unsigned int bytes_done)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);
	unsigned int nr_sectors;

	BUG_ON(bip == NULL);
	BUG_ON(bi == NULL);

	nr_sectors = bio_integrity_hw_sectors(bi, bytes_done >> 9);
	bio_integrity_mark_head(bip, nr_sectors * bi->tuple_size);
}
EXPORT_SYMBOL(bio_integrity_advance);

void bio_integrity_trim(struct bio *bio, unsigned int offset,
			unsigned int sectors)
{
	struct bio_integrity_payload *bip = bio->bi_integrity;
	struct blk_integrity *bi = bdev_get_integrity(bio->bi_bdev);
	unsigned int nr_sectors;

	BUG_ON(bip == NULL);
	BUG_ON(bi == NULL);
	BUG_ON(!bio_flagged(bio, BIO_CLONED));

	nr_sectors = bio_integrity_hw_sectors(bi, sectors);
	bip->bip_sector = bip->bip_sector + offset;
	bio_integrity_mark_head(bip, offset * bi->tuple_size);
	bio_integrity_mark_tail(bip, sectors * bi->tuple_size);
}
EXPORT_SYMBOL(bio_integrity_trim);

void bio_integrity_split(struct bio *bio, struct bio_pair *bp, int sectors)
{
	struct blk_integrity *bi;
	struct bio_integrity_payload *bip = bio->bi_integrity;
	unsigned int nr_sectors;

	if (bio_integrity(bio) == 0)
		return;

	bi = bdev_get_integrity(bio->bi_bdev);
	BUG_ON(bi == NULL);
	BUG_ON(bip->bip_vcnt != 1);

	nr_sectors = bio_integrity_hw_sectors(bi, sectors);

	bp->bio1.bi_integrity = &bp->bip1;
	bp->bio2.bi_integrity = &bp->bip2;

	bp->iv1 = bip->bip_vec[0];
	bp->iv2 = bip->bip_vec[0];

	bp->bip1.bip_vec[0] = bp->iv1;
	bp->bip2.bip_vec[0] = bp->iv2;

	bp->iv1.bv_len = sectors * bi->tuple_size;
	bp->iv2.bv_offset += sectors * bi->tuple_size;
	bp->iv2.bv_len -= sectors * bi->tuple_size;

	bp->bip1.bip_sector = bio->bi_integrity->bip_sector;
	bp->bip2.bip_sector = bio->bi_integrity->bip_sector + nr_sectors;

	bp->bip1.bip_vcnt = bp->bip2.bip_vcnt = 1;
	bp->bip1.bip_idx = bp->bip2.bip_idx = 0;
}
EXPORT_SYMBOL(bio_integrity_split);

int bio_integrity_clone(struct bio *bio, struct bio *bio_src,
			gfp_t gfp_mask, struct bio_set *bs)
{
	struct bio_integrity_payload *bip_src = bio_src->bi_integrity;
	struct bio_integrity_payload *bip;

	BUG_ON(bip_src == NULL);

	bip = bio_integrity_alloc_bioset(bio, gfp_mask, bip_src->bip_vcnt, bs);

	if (bip == NULL)
		return -EIO;

	memcpy(bip->bip_vec, bip_src->bip_vec,
	       bip_src->bip_vcnt * sizeof(struct bio_vec));

	bip->bip_sector = bip_src->bip_sector;
	bip->bip_vcnt = bip_src->bip_vcnt;
	bip->bip_idx = bip_src->bip_idx;

	return 0;
}
EXPORT_SYMBOL(bio_integrity_clone);

int bioset_integrity_create(struct bio_set *bs, int pool_size)
{
	unsigned int max_slab = vecs_to_idx(BIO_MAX_PAGES);

	if (bs->bio_integrity_pool)
		return 0;

	bs->bio_integrity_pool =
		mempool_create_slab_pool(pool_size, bip_slab[max_slab].slab);

	if (!bs->bio_integrity_pool)
		return -1;

	return 0;
}
EXPORT_SYMBOL(bioset_integrity_create);

void bioset_integrity_free(struct bio_set *bs)
{
	if (bs->bio_integrity_pool)
		mempool_destroy(bs->bio_integrity_pool);
}
EXPORT_SYMBOL(bioset_integrity_free);

void __init bio_integrity_init(void)
{
	unsigned int i;

	kintegrityd_wq = alloc_workqueue("kintegrityd", WQ_MEM_RECLAIM |
					 WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!kintegrityd_wq)
		panic("Failed to create kintegrityd\n");

	for (i = 0 ; i < BIOVEC_NR_POOLS ; i++) {
		unsigned int size;

		size = sizeof(struct bio_integrity_payload)
			+ bip_slab[i].nr_vecs * sizeof(struct bio_vec);

		bip_slab[i].slab =
			kmem_cache_create(bip_slab[i].name, size, 0,
					  SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	}
}
