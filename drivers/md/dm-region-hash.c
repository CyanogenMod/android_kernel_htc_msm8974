/*
 * Copyright (C) 2003 Sistina Software Limited.
 * Copyright (C) 2004-2008 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the GPL.
 */

#include <linux/dm-dirty-log.h>
#include <linux/dm-region-hash.h>

#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "dm.h"

#define	DM_MSG_PREFIX	"region hash"

struct dm_region_hash {
	uint32_t region_size;
	unsigned region_shift;

	
	struct dm_dirty_log *log;

	
	rwlock_t hash_lock;
	mempool_t *region_pool;
	unsigned mask;
	unsigned nr_buckets;
	unsigned prime;
	unsigned shift;
	struct list_head *buckets;

	unsigned max_recovery; 

	spinlock_t region_lock;
	atomic_t recovery_in_flight;
	struct semaphore recovery_count;
	struct list_head clean_regions;
	struct list_head quiesced_regions;
	struct list_head recovered_regions;
	struct list_head failed_recovered_regions;

	int flush_failure;

	void *context;
	sector_t target_begin;

	
	void (*dispatch_bios)(void *context, struct bio_list *bios);

	
	void (*wakeup_workers)(void *context);

	
	void (*wakeup_all_recovery_waiters)(void *context);
};

struct dm_region {
	struct dm_region_hash *rh;	
	region_t key;
	int state;

	struct list_head hash_list;
	struct list_head list;

	atomic_t pending;
	struct bio_list delayed_bios;
};

static region_t dm_rh_sector_to_region(struct dm_region_hash *rh, sector_t sector)
{
	return sector >> rh->region_shift;
}

sector_t dm_rh_region_to_sector(struct dm_region_hash *rh, region_t region)
{
	return region << rh->region_shift;
}
EXPORT_SYMBOL_GPL(dm_rh_region_to_sector);

region_t dm_rh_bio_to_region(struct dm_region_hash *rh, struct bio *bio)
{
	return dm_rh_sector_to_region(rh, bio->bi_sector - rh->target_begin);
}
EXPORT_SYMBOL_GPL(dm_rh_bio_to_region);

void *dm_rh_region_context(struct dm_region *reg)
{
	return reg->rh->context;
}
EXPORT_SYMBOL_GPL(dm_rh_region_context);

region_t dm_rh_get_region_key(struct dm_region *reg)
{
	return reg->key;
}
EXPORT_SYMBOL_GPL(dm_rh_get_region_key);

sector_t dm_rh_get_region_size(struct dm_region_hash *rh)
{
	return rh->region_size;
}
EXPORT_SYMBOL_GPL(dm_rh_get_region_size);

#define RH_HASH_MULT 2654435387U
#define RH_HASH_SHIFT 12

#define MIN_REGIONS 64
struct dm_region_hash *dm_region_hash_create(
		void *context, void (*dispatch_bios)(void *context,
						     struct bio_list *bios),
		void (*wakeup_workers)(void *context),
		void (*wakeup_all_recovery_waiters)(void *context),
		sector_t target_begin, unsigned max_recovery,
		struct dm_dirty_log *log, uint32_t region_size,
		region_t nr_regions)
{
	struct dm_region_hash *rh;
	unsigned nr_buckets, max_buckets;
	size_t i;

	max_buckets = nr_regions >> 6;
	for (nr_buckets = 128u; nr_buckets < max_buckets; nr_buckets <<= 1)
		;
	nr_buckets >>= 1;

	rh = kmalloc(sizeof(*rh), GFP_KERNEL);
	if (!rh) {
		DMERR("unable to allocate region hash memory");
		return ERR_PTR(-ENOMEM);
	}

	rh->context = context;
	rh->dispatch_bios = dispatch_bios;
	rh->wakeup_workers = wakeup_workers;
	rh->wakeup_all_recovery_waiters = wakeup_all_recovery_waiters;
	rh->target_begin = target_begin;
	rh->max_recovery = max_recovery;
	rh->log = log;
	rh->region_size = region_size;
	rh->region_shift = ffs(region_size) - 1;
	rwlock_init(&rh->hash_lock);
	rh->mask = nr_buckets - 1;
	rh->nr_buckets = nr_buckets;

	rh->shift = RH_HASH_SHIFT;
	rh->prime = RH_HASH_MULT;

	rh->buckets = vmalloc(nr_buckets * sizeof(*rh->buckets));
	if (!rh->buckets) {
		DMERR("unable to allocate region hash bucket memory");
		kfree(rh);
		return ERR_PTR(-ENOMEM);
	}

	for (i = 0; i < nr_buckets; i++)
		INIT_LIST_HEAD(rh->buckets + i);

	spin_lock_init(&rh->region_lock);
	sema_init(&rh->recovery_count, 0);
	atomic_set(&rh->recovery_in_flight, 0);
	INIT_LIST_HEAD(&rh->clean_regions);
	INIT_LIST_HEAD(&rh->quiesced_regions);
	INIT_LIST_HEAD(&rh->recovered_regions);
	INIT_LIST_HEAD(&rh->failed_recovered_regions);
	rh->flush_failure = 0;

	rh->region_pool = mempool_create_kmalloc_pool(MIN_REGIONS,
						      sizeof(struct dm_region));
	if (!rh->region_pool) {
		vfree(rh->buckets);
		kfree(rh);
		rh = ERR_PTR(-ENOMEM);
	}

	return rh;
}
EXPORT_SYMBOL_GPL(dm_region_hash_create);

void dm_region_hash_destroy(struct dm_region_hash *rh)
{
	unsigned h;
	struct dm_region *reg, *nreg;

	BUG_ON(!list_empty(&rh->quiesced_regions));
	for (h = 0; h < rh->nr_buckets; h++) {
		list_for_each_entry_safe(reg, nreg, rh->buckets + h,
					 hash_list) {
			BUG_ON(atomic_read(&reg->pending));
			mempool_free(reg, rh->region_pool);
		}
	}

	if (rh->log)
		dm_dirty_log_destroy(rh->log);

	if (rh->region_pool)
		mempool_destroy(rh->region_pool);

	vfree(rh->buckets);
	kfree(rh);
}
EXPORT_SYMBOL_GPL(dm_region_hash_destroy);

struct dm_dirty_log *dm_rh_dirty_log(struct dm_region_hash *rh)
{
	return rh->log;
}
EXPORT_SYMBOL_GPL(dm_rh_dirty_log);

static unsigned rh_hash(struct dm_region_hash *rh, region_t region)
{
	return (unsigned) ((region * rh->prime) >> rh->shift) & rh->mask;
}

static struct dm_region *__rh_lookup(struct dm_region_hash *rh, region_t region)
{
	struct dm_region *reg;
	struct list_head *bucket = rh->buckets + rh_hash(rh, region);

	list_for_each_entry(reg, bucket, hash_list)
		if (reg->key == region)
			return reg;

	return NULL;
}

static void __rh_insert(struct dm_region_hash *rh, struct dm_region *reg)
{
	list_add(&reg->hash_list, rh->buckets + rh_hash(rh, reg->key));
}

static struct dm_region *__rh_alloc(struct dm_region_hash *rh, region_t region)
{
	struct dm_region *reg, *nreg;

	nreg = mempool_alloc(rh->region_pool, GFP_ATOMIC);
	if (unlikely(!nreg))
		nreg = kmalloc(sizeof(*nreg), GFP_NOIO | __GFP_NOFAIL);

	nreg->state = rh->log->type->in_sync(rh->log, region, 1) ?
		      DM_RH_CLEAN : DM_RH_NOSYNC;
	nreg->rh = rh;
	nreg->key = region;
	INIT_LIST_HEAD(&nreg->list);
	atomic_set(&nreg->pending, 0);
	bio_list_init(&nreg->delayed_bios);

	write_lock_irq(&rh->hash_lock);
	reg = __rh_lookup(rh, region);
	if (reg)
		
		mempool_free(nreg, rh->region_pool);
	else {
		__rh_insert(rh, nreg);
		if (nreg->state == DM_RH_CLEAN) {
			spin_lock(&rh->region_lock);
			list_add(&nreg->list, &rh->clean_regions);
			spin_unlock(&rh->region_lock);
		}

		reg = nreg;
	}
	write_unlock_irq(&rh->hash_lock);

	return reg;
}

static struct dm_region *__rh_find(struct dm_region_hash *rh, region_t region)
{
	struct dm_region *reg;

	reg = __rh_lookup(rh, region);
	if (!reg) {
		read_unlock(&rh->hash_lock);
		reg = __rh_alloc(rh, region);
		read_lock(&rh->hash_lock);
	}

	return reg;
}

int dm_rh_get_state(struct dm_region_hash *rh, region_t region, int may_block)
{
	int r;
	struct dm_region *reg;

	read_lock(&rh->hash_lock);
	reg = __rh_lookup(rh, region);
	read_unlock(&rh->hash_lock);

	if (reg)
		return reg->state;

	r = rh->log->type->in_sync(rh->log, region, may_block);

	return r == 1 ? DM_RH_CLEAN : DM_RH_NOSYNC;
}
EXPORT_SYMBOL_GPL(dm_rh_get_state);

static void complete_resync_work(struct dm_region *reg, int success)
{
	struct dm_region_hash *rh = reg->rh;

	rh->log->type->set_region_sync(rh->log, reg->key, success);

	rh->dispatch_bios(rh->context, &reg->delayed_bios);
	if (atomic_dec_and_test(&rh->recovery_in_flight))
		rh->wakeup_all_recovery_waiters(rh->context);
	up(&rh->recovery_count);
}

/* dm_rh_mark_nosync
 * @ms
 * @bio
 *
 * The bio was written on some mirror(s) but failed on other mirror(s).
 * We can successfully endio the bio but should avoid the region being
 * marked clean by setting the state DM_RH_NOSYNC.
 *
 * This function is _not_ safe in interrupt context!
 */
void dm_rh_mark_nosync(struct dm_region_hash *rh, struct bio *bio)
{
	unsigned long flags;
	struct dm_dirty_log *log = rh->log;
	struct dm_region *reg;
	region_t region = dm_rh_bio_to_region(rh, bio);
	int recovering = 0;

	if (bio->bi_rw & REQ_FLUSH) {
		rh->flush_failure = 1;
		return;
	}

	
	log->type->set_region_sync(log, region, 0);

	read_lock(&rh->hash_lock);
	reg = __rh_find(rh, region);
	read_unlock(&rh->hash_lock);

	
	BUG_ON(!reg);
	BUG_ON(!list_empty(&reg->list));

	spin_lock_irqsave(&rh->region_lock, flags);
	recovering = (reg->state == DM_RH_RECOVERING);
	reg->state = DM_RH_NOSYNC;
	BUG_ON(!list_empty(&reg->list));
	spin_unlock_irqrestore(&rh->region_lock, flags);

	if (recovering)
		complete_resync_work(reg, 0);
}
EXPORT_SYMBOL_GPL(dm_rh_mark_nosync);

void dm_rh_update_states(struct dm_region_hash *rh, int errors_handled)
{
	struct dm_region *reg, *next;

	LIST_HEAD(clean);
	LIST_HEAD(recovered);
	LIST_HEAD(failed_recovered);

	write_lock_irq(&rh->hash_lock);
	spin_lock(&rh->region_lock);
	if (!list_empty(&rh->clean_regions)) {
		list_splice_init(&rh->clean_regions, &clean);

		list_for_each_entry(reg, &clean, list)
			list_del(&reg->hash_list);
	}

	if (!list_empty(&rh->recovered_regions)) {
		list_splice_init(&rh->recovered_regions, &recovered);

		list_for_each_entry(reg, &recovered, list)
			list_del(&reg->hash_list);
	}

	if (!list_empty(&rh->failed_recovered_regions)) {
		list_splice_init(&rh->failed_recovered_regions,
				 &failed_recovered);

		list_for_each_entry(reg, &failed_recovered, list)
			list_del(&reg->hash_list);
	}

	spin_unlock(&rh->region_lock);
	write_unlock_irq(&rh->hash_lock);

	list_for_each_entry_safe(reg, next, &recovered, list) {
		rh->log->type->clear_region(rh->log, reg->key);
		complete_resync_work(reg, 1);
		mempool_free(reg, rh->region_pool);
	}

	list_for_each_entry_safe(reg, next, &failed_recovered, list) {
		complete_resync_work(reg, errors_handled ? 0 : 1);
		mempool_free(reg, rh->region_pool);
	}

	list_for_each_entry_safe(reg, next, &clean, list) {
		rh->log->type->clear_region(rh->log, reg->key);
		mempool_free(reg, rh->region_pool);
	}

	rh->log->type->flush(rh->log);
}
EXPORT_SYMBOL_GPL(dm_rh_update_states);

static void rh_inc(struct dm_region_hash *rh, region_t region)
{
	struct dm_region *reg;

	read_lock(&rh->hash_lock);
	reg = __rh_find(rh, region);

	spin_lock_irq(&rh->region_lock);
	atomic_inc(&reg->pending);

	if (reg->state == DM_RH_CLEAN) {
		reg->state = DM_RH_DIRTY;
		list_del_init(&reg->list);	
		spin_unlock_irq(&rh->region_lock);

		rh->log->type->mark_region(rh->log, reg->key);
	} else
		spin_unlock_irq(&rh->region_lock);


	read_unlock(&rh->hash_lock);
}

void dm_rh_inc_pending(struct dm_region_hash *rh, struct bio_list *bios)
{
	struct bio *bio;

	for (bio = bios->head; bio; bio = bio->bi_next) {
		if (bio->bi_rw & REQ_FLUSH)
			continue;
		rh_inc(rh, dm_rh_bio_to_region(rh, bio));
	}
}
EXPORT_SYMBOL_GPL(dm_rh_inc_pending);

void dm_rh_dec(struct dm_region_hash *rh, region_t region)
{
	unsigned long flags;
	struct dm_region *reg;
	int should_wake = 0;

	read_lock(&rh->hash_lock);
	reg = __rh_lookup(rh, region);
	read_unlock(&rh->hash_lock);

	spin_lock_irqsave(&rh->region_lock, flags);
	if (atomic_dec_and_test(&reg->pending)) {

		
		if (unlikely(rh->flush_failure)) {
			reg->state = DM_RH_NOSYNC;
		} else if (reg->state == DM_RH_RECOVERING) {
			list_add_tail(&reg->list, &rh->quiesced_regions);
		} else if (reg->state == DM_RH_DIRTY) {
			reg->state = DM_RH_CLEAN;
			list_add(&reg->list, &rh->clean_regions);
		}
		should_wake = 1;
	}
	spin_unlock_irqrestore(&rh->region_lock, flags);

	if (should_wake)
		rh->wakeup_workers(rh->context);
}
EXPORT_SYMBOL_GPL(dm_rh_dec);

static int __rh_recovery_prepare(struct dm_region_hash *rh)
{
	int r;
	region_t region;
	struct dm_region *reg;

	r = rh->log->type->get_resync_work(rh->log, &region);
	if (r <= 0)
		return r;

	read_lock(&rh->hash_lock);
	reg = __rh_find(rh, region);
	read_unlock(&rh->hash_lock);

	spin_lock_irq(&rh->region_lock);
	reg->state = DM_RH_RECOVERING;

	
	if (atomic_read(&reg->pending))
		list_del_init(&reg->list);
	else
		list_move(&reg->list, &rh->quiesced_regions);

	spin_unlock_irq(&rh->region_lock);

	return 1;
}

void dm_rh_recovery_prepare(struct dm_region_hash *rh)
{
	
	atomic_inc(&rh->recovery_in_flight);

	while (!down_trylock(&rh->recovery_count)) {
		atomic_inc(&rh->recovery_in_flight);
		if (__rh_recovery_prepare(rh) <= 0) {
			atomic_dec(&rh->recovery_in_flight);
			up(&rh->recovery_count);
			break;
		}
	}

	
	if (atomic_dec_and_test(&rh->recovery_in_flight))
		rh->wakeup_all_recovery_waiters(rh->context);
}
EXPORT_SYMBOL_GPL(dm_rh_recovery_prepare);

struct dm_region *dm_rh_recovery_start(struct dm_region_hash *rh)
{
	struct dm_region *reg = NULL;

	spin_lock_irq(&rh->region_lock);
	if (!list_empty(&rh->quiesced_regions)) {
		reg = list_entry(rh->quiesced_regions.next,
				 struct dm_region, list);
		list_del_init(&reg->list);  
	}
	spin_unlock_irq(&rh->region_lock);

	return reg;
}
EXPORT_SYMBOL_GPL(dm_rh_recovery_start);

void dm_rh_recovery_end(struct dm_region *reg, int success)
{
	struct dm_region_hash *rh = reg->rh;

	spin_lock_irq(&rh->region_lock);
	if (success)
		list_add(&reg->list, &reg->rh->recovered_regions);
	else
		list_add(&reg->list, &reg->rh->failed_recovered_regions);

	spin_unlock_irq(&rh->region_lock);

	rh->wakeup_workers(rh->context);
}
EXPORT_SYMBOL_GPL(dm_rh_recovery_end);

int dm_rh_recovery_in_flight(struct dm_region_hash *rh)
{
	return atomic_read(&rh->recovery_in_flight);
}
EXPORT_SYMBOL_GPL(dm_rh_recovery_in_flight);

int dm_rh_flush(struct dm_region_hash *rh)
{
	return rh->log->type->flush(rh->log);
}
EXPORT_SYMBOL_GPL(dm_rh_flush);

void dm_rh_delay(struct dm_region_hash *rh, struct bio *bio)
{
	struct dm_region *reg;

	read_lock(&rh->hash_lock);
	reg = __rh_find(rh, dm_rh_bio_to_region(rh, bio));
	bio_list_add(&reg->delayed_bios, bio);
	read_unlock(&rh->hash_lock);
}
EXPORT_SYMBOL_GPL(dm_rh_delay);

void dm_rh_stop_recovery(struct dm_region_hash *rh)
{
	int i;

	
	for (i = 0; i < rh->max_recovery; i++)
		down(&rh->recovery_count);
}
EXPORT_SYMBOL_GPL(dm_rh_stop_recovery);

void dm_rh_start_recovery(struct dm_region_hash *rh)
{
	int i;

	for (i = 0; i < rh->max_recovery; i++)
		up(&rh->recovery_count);

	rh->wakeup_workers(rh->context);
}
EXPORT_SYMBOL_GPL(dm_rh_start_recovery);

MODULE_DESCRIPTION(DM_NAME " region hash");
MODULE_AUTHOR("Joe Thornber/Heinz Mauelshagen <dm-devel@redhat.com>");
MODULE_LICENSE("GPL");
