/*
 * Copyright (C) 2003 Sistina Software Limited.
 * Copyright (C) 2005-2008 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the GPL.
 */

#include "dm-bio-record.h"

#include <linux/init.h>
#include <linux/mempool.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/device-mapper.h>
#include <linux/dm-io.h>
#include <linux/dm-dirty-log.h>
#include <linux/dm-kcopyd.h>
#include <linux/dm-region-hash.h>

#define DM_MSG_PREFIX "raid1"

#define MAX_RECOVERY 1	

#define DM_RAID1_HANDLE_ERRORS 0x01
#define errors_handled(p)	((p)->features & DM_RAID1_HANDLE_ERRORS)

static DECLARE_WAIT_QUEUE_HEAD(_kmirrord_recovery_stopped);

enum dm_raid1_error {
	DM_RAID1_WRITE_ERROR,
	DM_RAID1_FLUSH_ERROR,
	DM_RAID1_SYNC_ERROR,
	DM_RAID1_READ_ERROR
};

struct mirror {
	struct mirror_set *ms;
	atomic_t error_count;
	unsigned long error_type;
	struct dm_dev *dev;
	sector_t offset;
};

struct mirror_set {
	struct dm_target *ti;
	struct list_head list;

	uint64_t features;

	spinlock_t lock;	
	struct bio_list reads;
	struct bio_list writes;
	struct bio_list failures;
	struct bio_list holds;	

	struct dm_region_hash *rh;
	struct dm_kcopyd_client *kcopyd_client;
	struct dm_io_client *io_client;
	mempool_t *read_record_pool;

	
	region_t nr_regions;
	int in_sync;
	int log_failure;
	int leg_failure;
	atomic_t suspend;

	atomic_t default_mirror;	

	struct workqueue_struct *kmirrord_wq;
	struct work_struct kmirrord_work;
	struct timer_list timer;
	unsigned long timer_pending;

	struct work_struct trigger_event;

	unsigned nr_mirrors;
	struct mirror mirror[0];
};

static void wakeup_mirrord(void *context)
{
	struct mirror_set *ms = context;

	queue_work(ms->kmirrord_wq, &ms->kmirrord_work);
}

static void delayed_wake_fn(unsigned long data)
{
	struct mirror_set *ms = (struct mirror_set *) data;

	clear_bit(0, &ms->timer_pending);
	wakeup_mirrord(ms);
}

static void delayed_wake(struct mirror_set *ms)
{
	if (test_and_set_bit(0, &ms->timer_pending))
		return;

	ms->timer.expires = jiffies + HZ / 5;
	ms->timer.data = (unsigned long) ms;
	ms->timer.function = delayed_wake_fn;
	add_timer(&ms->timer);
}

static void wakeup_all_recovery_waiters(void *context)
{
	wake_up_all(&_kmirrord_recovery_stopped);
}

static void queue_bio(struct mirror_set *ms, struct bio *bio, int rw)
{
	unsigned long flags;
	int should_wake = 0;
	struct bio_list *bl;

	bl = (rw == WRITE) ? &ms->writes : &ms->reads;
	spin_lock_irqsave(&ms->lock, flags);
	should_wake = !(bl->head);
	bio_list_add(bl, bio);
	spin_unlock_irqrestore(&ms->lock, flags);

	if (should_wake)
		wakeup_mirrord(ms);
}

static void dispatch_bios(void *context, struct bio_list *bio_list)
{
	struct mirror_set *ms = context;
	struct bio *bio;

	while ((bio = bio_list_pop(bio_list)))
		queue_bio(ms, bio, WRITE);
}

#define MIN_READ_RECORDS 20
struct dm_raid1_read_record {
	struct mirror *m;
	struct dm_bio_details details;
};

static struct kmem_cache *_dm_raid1_read_record_cache;

#define DEFAULT_MIRROR 0

static struct mirror *bio_get_m(struct bio *bio)
{
	return (struct mirror *) bio->bi_next;
}

static void bio_set_m(struct bio *bio, struct mirror *m)
{
	bio->bi_next = (struct bio *) m;
}

static struct mirror *get_default_mirror(struct mirror_set *ms)
{
	return &ms->mirror[atomic_read(&ms->default_mirror)];
}

static void set_default_mirror(struct mirror *m)
{
	struct mirror_set *ms = m->ms;
	struct mirror *m0 = &(ms->mirror[0]);

	atomic_set(&ms->default_mirror, m - m0);
}

static struct mirror *get_valid_mirror(struct mirror_set *ms)
{
	struct mirror *m;

	for (m = ms->mirror; m < ms->mirror + ms->nr_mirrors; m++)
		if (!atomic_read(&m->error_count))
			return m;

	return NULL;
}

static void fail_mirror(struct mirror *m, enum dm_raid1_error error_type)
{
	struct mirror_set *ms = m->ms;
	struct mirror *new;

	ms->leg_failure = 1;

	atomic_inc(&m->error_count);

	if (test_and_set_bit(error_type, &m->error_type))
		return;

	if (!errors_handled(ms))
		return;

	if (m != get_default_mirror(ms))
		goto out;

	if (!ms->in_sync) {
		DMERR("Primary mirror (%s) failed while out-of-sync: "
		      "Reads may fail.", m->dev->name);
		goto out;
	}

	new = get_valid_mirror(ms);
	if (new)
		set_default_mirror(new);
	else
		DMWARN("All sides of mirror have failed.");

out:
	schedule_work(&ms->trigger_event);
}

static int mirror_flush(struct dm_target *ti)
{
	struct mirror_set *ms = ti->private;
	unsigned long error_bits;

	unsigned int i;
	struct dm_io_region io[ms->nr_mirrors];
	struct mirror *m;
	struct dm_io_request io_req = {
		.bi_rw = WRITE_FLUSH,
		.mem.type = DM_IO_KMEM,
		.mem.ptr.addr = NULL,
		.client = ms->io_client,
	};

	for (i = 0, m = ms->mirror; i < ms->nr_mirrors; i++, m++) {
		io[i].bdev = m->dev->bdev;
		io[i].sector = 0;
		io[i].count = 0;
	}

	error_bits = -1;
	dm_io(&io_req, ms->nr_mirrors, io, &error_bits);
	if (unlikely(error_bits != 0)) {
		for (i = 0; i < ms->nr_mirrors; i++)
			if (test_bit(i, &error_bits))
				fail_mirror(ms->mirror + i,
					    DM_RAID1_FLUSH_ERROR);
		return -EIO;
	}

	return 0;
}

static void recovery_complete(int read_err, unsigned long write_err,
			      void *context)
{
	struct dm_region *reg = context;
	struct mirror_set *ms = dm_rh_region_context(reg);
	int m, bit = 0;

	if (read_err) {
		
		DMERR_LIMIT("Unable to read primary mirror during recovery");
		fail_mirror(get_default_mirror(ms), DM_RAID1_SYNC_ERROR);
	}

	if (write_err) {
		DMERR_LIMIT("Write error during recovery (error = 0x%lx)",
			    write_err);
		for (m = 0; m < ms->nr_mirrors; m++) {
			if (&ms->mirror[m] == get_default_mirror(ms))
				continue;
			if (test_bit(bit, &write_err))
				fail_mirror(ms->mirror + m,
					    DM_RAID1_SYNC_ERROR);
			bit++;
		}
	}

	dm_rh_recovery_end(reg, !(read_err || write_err));
}

static int recover(struct mirror_set *ms, struct dm_region *reg)
{
	int r;
	unsigned i;
	struct dm_io_region from, to[DM_KCOPYD_MAX_REGIONS], *dest;
	struct mirror *m;
	unsigned long flags = 0;
	region_t key = dm_rh_get_region_key(reg);
	sector_t region_size = dm_rh_get_region_size(ms->rh);

	
	m = get_default_mirror(ms);
	from.bdev = m->dev->bdev;
	from.sector = m->offset + dm_rh_region_to_sector(ms->rh, key);
	if (key == (ms->nr_regions - 1)) {
		from.count = ms->ti->len & (region_size - 1);
		if (!from.count)
			from.count = region_size;
	} else
		from.count = region_size;

	
	for (i = 0, dest = to; i < ms->nr_mirrors; i++) {
		if (&ms->mirror[i] == get_default_mirror(ms))
			continue;

		m = ms->mirror + i;
		dest->bdev = m->dev->bdev;
		dest->sector = m->offset + dm_rh_region_to_sector(ms->rh, key);
		dest->count = from.count;
		dest++;
	}

	
	if (!errors_handled(ms))
		set_bit(DM_KCOPYD_IGNORE_ERROR, &flags);

	r = dm_kcopyd_copy(ms->kcopyd_client, &from, ms->nr_mirrors - 1, to,
			   flags, recovery_complete, reg);

	return r;
}

static void do_recovery(struct mirror_set *ms)
{
	struct dm_region *reg;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);
	int r;

	dm_rh_recovery_prepare(ms->rh);

	while ((reg = dm_rh_recovery_start(ms->rh))) {
		r = recover(ms, reg);
		if (r)
			dm_rh_recovery_end(reg, 0);
	}

	if (!ms->in_sync &&
	    (log->type->get_sync_count(log) == ms->nr_regions)) {
		
		dm_table_event(ms->ti->table);
		ms->in_sync = 1;
	}
}

static struct mirror *choose_mirror(struct mirror_set *ms, sector_t sector)
{
	struct mirror *m = get_default_mirror(ms);

	do {
		if (likely(!atomic_read(&m->error_count)))
			return m;

		if (m-- == ms->mirror)
			m += ms->nr_mirrors;
	} while (m != get_default_mirror(ms));

	return NULL;
}

static int default_ok(struct mirror *m)
{
	struct mirror *default_mirror = get_default_mirror(m->ms);

	return !atomic_read(&default_mirror->error_count);
}

static int mirror_available(struct mirror_set *ms, struct bio *bio)
{
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);
	region_t region = dm_rh_bio_to_region(ms->rh, bio);

	if (log->type->in_sync(log, region, 0))
		return choose_mirror(ms,  bio->bi_sector) ? 1 : 0;

	return 0;
}

static sector_t map_sector(struct mirror *m, struct bio *bio)
{
	if (unlikely(!bio->bi_size))
		return 0;
	return m->offset + dm_target_offset(m->ms->ti, bio->bi_sector);
}

static void map_bio(struct mirror *m, struct bio *bio)
{
	bio->bi_bdev = m->dev->bdev;
	bio->bi_sector = map_sector(m, bio);
}

static void map_region(struct dm_io_region *io, struct mirror *m,
		       struct bio *bio)
{
	io->bdev = m->dev->bdev;
	io->sector = map_sector(m, bio);
	io->count = bio->bi_size >> 9;
}

static void hold_bio(struct mirror_set *ms, struct bio *bio)
{
	spin_lock_irq(&ms->lock);

	if (atomic_read(&ms->suspend)) {
		spin_unlock_irq(&ms->lock);

		if (dm_noflush_suspending(ms->ti))
			bio_endio(bio, DM_ENDIO_REQUEUE);
		else
			bio_endio(bio, -EIO);
		return;
	}

	bio_list_add(&ms->holds, bio);
	spin_unlock_irq(&ms->lock);
}

static void read_callback(unsigned long error, void *context)
{
	struct bio *bio = context;
	struct mirror *m;

	m = bio_get_m(bio);
	bio_set_m(bio, NULL);

	if (likely(!error)) {
		bio_endio(bio, 0);
		return;
	}

	fail_mirror(m, DM_RAID1_READ_ERROR);

	if (likely(default_ok(m)) || mirror_available(m->ms, bio)) {
		DMWARN_LIMIT("Read failure on mirror device %s.  "
			     "Trying alternative device.",
			     m->dev->name);
		queue_bio(m->ms, bio, bio_rw(bio));
		return;
	}

	DMERR_LIMIT("Read failure on mirror device %s.  Failing I/O.",
		    m->dev->name);
	bio_endio(bio, -EIO);
}

static void read_async_bio(struct mirror *m, struct bio *bio)
{
	struct dm_io_region io;
	struct dm_io_request io_req = {
		.bi_rw = READ,
		.mem.type = DM_IO_BVEC,
		.mem.ptr.bvec = bio->bi_io_vec + bio->bi_idx,
		.notify.fn = read_callback,
		.notify.context = bio,
		.client = m->ms->io_client,
	};

	map_region(&io, m, bio);
	bio_set_m(bio, m);
	BUG_ON(dm_io(&io_req, 1, &io, NULL));
}

static inline int region_in_sync(struct mirror_set *ms, region_t region,
				 int may_block)
{
	int state = dm_rh_get_state(ms->rh, region, may_block);
	return state == DM_RH_CLEAN || state == DM_RH_DIRTY;
}

static void do_reads(struct mirror_set *ms, struct bio_list *reads)
{
	region_t region;
	struct bio *bio;
	struct mirror *m;

	while ((bio = bio_list_pop(reads))) {
		region = dm_rh_bio_to_region(ms->rh, bio);
		m = get_default_mirror(ms);

		if (likely(region_in_sync(ms, region, 1)))
			m = choose_mirror(ms, bio->bi_sector);
		else if (m && atomic_read(&m->error_count))
			m = NULL;

		if (likely(m))
			read_async_bio(m, bio);
		else
			bio_endio(bio, -EIO);
	}
}



static void write_callback(unsigned long error, void *context)
{
	unsigned i, ret = 0;
	struct bio *bio = (struct bio *) context;
	struct mirror_set *ms;
	int should_wake = 0;
	unsigned long flags;

	ms = bio_get_m(bio)->ms;
	bio_set_m(bio, NULL);

	if (likely(!error)) {
		bio_endio(bio, ret);
		return;
	}

	for (i = 0; i < ms->nr_mirrors; i++)
		if (test_bit(i, &error))
			fail_mirror(ms->mirror + i, DM_RAID1_WRITE_ERROR);

	spin_lock_irqsave(&ms->lock, flags);
	if (!ms->failures.head)
		should_wake = 1;
	bio_list_add(&ms->failures, bio);
	spin_unlock_irqrestore(&ms->lock, flags);
	if (should_wake)
		wakeup_mirrord(ms);
}

static void do_write(struct mirror_set *ms, struct bio *bio)
{
	unsigned int i;
	struct dm_io_region io[ms->nr_mirrors], *dest = io;
	struct mirror *m;
	struct dm_io_request io_req = {
		.bi_rw = WRITE | (bio->bi_rw & WRITE_FLUSH_FUA),
		.mem.type = DM_IO_BVEC,
		.mem.ptr.bvec = bio->bi_io_vec + bio->bi_idx,
		.notify.fn = write_callback,
		.notify.context = bio,
		.client = ms->io_client,
	};

	if (bio->bi_rw & REQ_DISCARD) {
		io_req.bi_rw |= REQ_DISCARD;
		io_req.mem.type = DM_IO_KMEM;
		io_req.mem.ptr.addr = NULL;
	}

	for (i = 0, m = ms->mirror; i < ms->nr_mirrors; i++, m++)
		map_region(dest++, m, bio);

	bio_set_m(bio, get_default_mirror(ms));

	BUG_ON(dm_io(&io_req, ms->nr_mirrors, io, NULL));
}

static void do_writes(struct mirror_set *ms, struct bio_list *writes)
{
	int state;
	struct bio *bio;
	struct bio_list sync, nosync, recover, *this_list = NULL;
	struct bio_list requeue;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);
	region_t region;

	if (!writes->head)
		return;

	bio_list_init(&sync);
	bio_list_init(&nosync);
	bio_list_init(&recover);
	bio_list_init(&requeue);

	while ((bio = bio_list_pop(writes))) {
		if ((bio->bi_rw & REQ_FLUSH) ||
		    (bio->bi_rw & REQ_DISCARD)) {
			bio_list_add(&sync, bio);
			continue;
		}

		region = dm_rh_bio_to_region(ms->rh, bio);

		if (log->type->is_remote_recovering &&
		    log->type->is_remote_recovering(log, region)) {
			bio_list_add(&requeue, bio);
			continue;
		}

		state = dm_rh_get_state(ms->rh, region, 1);
		switch (state) {
		case DM_RH_CLEAN:
		case DM_RH_DIRTY:
			this_list = &sync;
			break;

		case DM_RH_NOSYNC:
			this_list = &nosync;
			break;

		case DM_RH_RECOVERING:
			this_list = &recover;
			break;
		}

		bio_list_add(this_list, bio);
	}

	if (unlikely(requeue.head)) {
		spin_lock_irq(&ms->lock);
		bio_list_merge(&ms->writes, &requeue);
		spin_unlock_irq(&ms->lock);
		delayed_wake(ms);
	}

	/*
	 * Increment the pending counts for any regions that will
	 * be written to (writes to recover regions are going to
	 * be delayed).
	 */
	dm_rh_inc_pending(ms->rh, &sync);
	dm_rh_inc_pending(ms->rh, &nosync);

	ms->log_failure = dm_rh_flush(ms->rh) ? 1 : ms->log_failure;

	if (unlikely(ms->log_failure) && errors_handled(ms)) {
		spin_lock_irq(&ms->lock);
		bio_list_merge(&ms->failures, &sync);
		spin_unlock_irq(&ms->lock);
		wakeup_mirrord(ms);
	} else
		while ((bio = bio_list_pop(&sync)))
			do_write(ms, bio);

	while ((bio = bio_list_pop(&recover)))
		dm_rh_delay(ms->rh, bio);

	while ((bio = bio_list_pop(&nosync))) {
		if (unlikely(ms->leg_failure) && errors_handled(ms)) {
			spin_lock_irq(&ms->lock);
			bio_list_add(&ms->failures, bio);
			spin_unlock_irq(&ms->lock);
			wakeup_mirrord(ms);
		} else {
			map_bio(get_default_mirror(ms), bio);
			generic_make_request(bio);
		}
	}
}

static void do_failures(struct mirror_set *ms, struct bio_list *failures)
{
	struct bio *bio;

	if (likely(!failures->head))
		return;

	while ((bio = bio_list_pop(failures))) {
		if (!ms->log_failure) {
			ms->in_sync = 0;
			dm_rh_mark_nosync(ms->rh, bio);
		}

		if (!get_valid_mirror(ms))
			bio_endio(bio, -EIO);
		else if (errors_handled(ms))
			hold_bio(ms, bio);
		else
			bio_endio(bio, 0);
	}
}

static void trigger_event(struct work_struct *work)
{
	struct mirror_set *ms =
		container_of(work, struct mirror_set, trigger_event);

	dm_table_event(ms->ti->table);
}

static void do_mirror(struct work_struct *work)
{
	struct mirror_set *ms = container_of(work, struct mirror_set,
					     kmirrord_work);
	struct bio_list reads, writes, failures;
	unsigned long flags;

	spin_lock_irqsave(&ms->lock, flags);
	reads = ms->reads;
	writes = ms->writes;
	failures = ms->failures;
	bio_list_init(&ms->reads);
	bio_list_init(&ms->writes);
	bio_list_init(&ms->failures);
	spin_unlock_irqrestore(&ms->lock, flags);

	dm_rh_update_states(ms->rh, errors_handled(ms));
	do_recovery(ms);
	do_reads(ms, &reads);
	do_writes(ms, &writes);
	do_failures(ms, &failures);
}

static struct mirror_set *alloc_context(unsigned int nr_mirrors,
					uint32_t region_size,
					struct dm_target *ti,
					struct dm_dirty_log *dl)
{
	size_t len;
	struct mirror_set *ms = NULL;

	len = sizeof(*ms) + (sizeof(ms->mirror[0]) * nr_mirrors);

	ms = kzalloc(len, GFP_KERNEL);
	if (!ms) {
		ti->error = "Cannot allocate mirror context";
		return NULL;
	}

	spin_lock_init(&ms->lock);
	bio_list_init(&ms->reads);
	bio_list_init(&ms->writes);
	bio_list_init(&ms->failures);
	bio_list_init(&ms->holds);

	ms->ti = ti;
	ms->nr_mirrors = nr_mirrors;
	ms->nr_regions = dm_sector_div_up(ti->len, region_size);
	ms->in_sync = 0;
	ms->log_failure = 0;
	ms->leg_failure = 0;
	atomic_set(&ms->suspend, 0);
	atomic_set(&ms->default_mirror, DEFAULT_MIRROR);

	ms->read_record_pool = mempool_create_slab_pool(MIN_READ_RECORDS,
						_dm_raid1_read_record_cache);

	if (!ms->read_record_pool) {
		ti->error = "Error creating mirror read_record_pool";
		kfree(ms);
		return NULL;
	}

	ms->io_client = dm_io_client_create();
	if (IS_ERR(ms->io_client)) {
		ti->error = "Error creating dm_io client";
		mempool_destroy(ms->read_record_pool);
		kfree(ms);
 		return NULL;
	}

	ms->rh = dm_region_hash_create(ms, dispatch_bios, wakeup_mirrord,
				       wakeup_all_recovery_waiters,
				       ms->ti->begin, MAX_RECOVERY,
				       dl, region_size, ms->nr_regions);
	if (IS_ERR(ms->rh)) {
		ti->error = "Error creating dirty region hash";
		dm_io_client_destroy(ms->io_client);
		mempool_destroy(ms->read_record_pool);
		kfree(ms);
		return NULL;
	}

	return ms;
}

static void free_context(struct mirror_set *ms, struct dm_target *ti,
			 unsigned int m)
{
	while (m--)
		dm_put_device(ti, ms->mirror[m].dev);

	dm_io_client_destroy(ms->io_client);
	dm_region_hash_destroy(ms->rh);
	mempool_destroy(ms->read_record_pool);
	kfree(ms);
}

static int get_mirror(struct mirror_set *ms, struct dm_target *ti,
		      unsigned int mirror, char **argv)
{
	unsigned long long offset;
	char dummy;

	if (sscanf(argv[1], "%llu%c", &offset, &dummy) != 1) {
		ti->error = "Invalid offset";
		return -EINVAL;
	}

	if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table),
			  &ms->mirror[mirror].dev)) {
		ti->error = "Device lookup failure";
		return -ENXIO;
	}

	ms->mirror[mirror].ms = ms;
	atomic_set(&(ms->mirror[mirror].error_count), 0);
	ms->mirror[mirror].error_type = 0;
	ms->mirror[mirror].offset = offset;

	return 0;
}

static struct dm_dirty_log *create_dirty_log(struct dm_target *ti,
					     unsigned argc, char **argv,
					     unsigned *args_used)
{
	unsigned param_count;
	struct dm_dirty_log *dl;
	char dummy;

	if (argc < 2) {
		ti->error = "Insufficient mirror log arguments";
		return NULL;
	}

	if (sscanf(argv[1], "%u%c", &param_count, &dummy) != 1) {
		ti->error = "Invalid mirror log argument count";
		return NULL;
	}

	*args_used = 2 + param_count;

	if (argc < *args_used) {
		ti->error = "Insufficient mirror log arguments";
		return NULL;
	}

	dl = dm_dirty_log_create(argv[0], ti, mirror_flush, param_count,
				 argv + 2);
	if (!dl) {
		ti->error = "Error creating mirror dirty log";
		return NULL;
	}

	return dl;
}

static int parse_features(struct mirror_set *ms, unsigned argc, char **argv,
			  unsigned *args_used)
{
	unsigned num_features;
	struct dm_target *ti = ms->ti;
	char dummy;

	*args_used = 0;

	if (!argc)
		return 0;

	if (sscanf(argv[0], "%u%c", &num_features, &dummy) != 1) {
		ti->error = "Invalid number of features";
		return -EINVAL;
	}

	argc--;
	argv++;
	(*args_used)++;

	if (num_features > argc) {
		ti->error = "Not enough arguments to support feature count";
		return -EINVAL;
	}

	if (!strcmp("handle_errors", argv[0]))
		ms->features |= DM_RAID1_HANDLE_ERRORS;
	else {
		ti->error = "Unrecognised feature requested";
		return -EINVAL;
	}

	(*args_used)++;

	return 0;
}

static int mirror_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	int r;
	unsigned int nr_mirrors, m, args_used;
	struct mirror_set *ms;
	struct dm_dirty_log *dl;
	char dummy;

	dl = create_dirty_log(ti, argc, argv, &args_used);
	if (!dl)
		return -EINVAL;

	argv += args_used;
	argc -= args_used;

	if (!argc || sscanf(argv[0], "%u%c", &nr_mirrors, &dummy) != 1 ||
	    nr_mirrors < 2 || nr_mirrors > DM_KCOPYD_MAX_REGIONS + 1) {
		ti->error = "Invalid number of mirrors";
		dm_dirty_log_destroy(dl);
		return -EINVAL;
	}

	argv++, argc--;

	if (argc < nr_mirrors * 2) {
		ti->error = "Too few mirror arguments";
		dm_dirty_log_destroy(dl);
		return -EINVAL;
	}

	ms = alloc_context(nr_mirrors, dl->type->get_region_size(dl), ti, dl);
	if (!ms) {
		dm_dirty_log_destroy(dl);
		return -ENOMEM;
	}

	
	for (m = 0; m < nr_mirrors; m++) {
		r = get_mirror(ms, ti, m, argv);
		if (r) {
			free_context(ms, ti, m);
			return r;
		}
		argv += 2;
		argc -= 2;
	}

	ti->private = ms;
	ti->split_io = dm_rh_get_region_size(ms->rh);
	ti->num_flush_requests = 1;
	ti->num_discard_requests = 1;

	ms->kmirrord_wq = alloc_workqueue("kmirrord",
					  WQ_NON_REENTRANT | WQ_MEM_RECLAIM, 0);
	if (!ms->kmirrord_wq) {
		DMERR("couldn't start kmirrord");
		r = -ENOMEM;
		goto err_free_context;
	}
	INIT_WORK(&ms->kmirrord_work, do_mirror);
	init_timer(&ms->timer);
	ms->timer_pending = 0;
	INIT_WORK(&ms->trigger_event, trigger_event);

	r = parse_features(ms, argc, argv, &args_used);
	if (r)
		goto err_destroy_wq;

	argv += args_used;
	argc -= args_used;


	if (argc) {
		ti->error = "Too many mirror arguments";
		r = -EINVAL;
		goto err_destroy_wq;
	}

	ms->kcopyd_client = dm_kcopyd_client_create();
	if (IS_ERR(ms->kcopyd_client)) {
		r = PTR_ERR(ms->kcopyd_client);
		goto err_destroy_wq;
	}

	wakeup_mirrord(ms);
	return 0;

err_destroy_wq:
	destroy_workqueue(ms->kmirrord_wq);
err_free_context:
	free_context(ms, ti, ms->nr_mirrors);
	return r;
}

static void mirror_dtr(struct dm_target *ti)
{
	struct mirror_set *ms = (struct mirror_set *) ti->private;

	del_timer_sync(&ms->timer);
	flush_workqueue(ms->kmirrord_wq);
	flush_work_sync(&ms->trigger_event);
	dm_kcopyd_client_destroy(ms->kcopyd_client);
	destroy_workqueue(ms->kmirrord_wq);
	free_context(ms, ti, ms->nr_mirrors);
}

static int mirror_map(struct dm_target *ti, struct bio *bio,
		      union map_info *map_context)
{
	int r, rw = bio_rw(bio);
	struct mirror *m;
	struct mirror_set *ms = ti->private;
	struct dm_raid1_read_record *read_record = NULL;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);

	if (rw == WRITE) {
		
		map_context->ll = dm_rh_bio_to_region(ms->rh, bio);
		queue_bio(ms, bio, rw);
		return DM_MAPIO_SUBMITTED;
	}

	r = log->type->in_sync(log, dm_rh_bio_to_region(ms->rh, bio), 0);
	if (r < 0 && r != -EWOULDBLOCK)
		return r;

	if (!r || (r == -EWOULDBLOCK)) {
		if (rw == READA)
			return -EWOULDBLOCK;

		queue_bio(ms, bio, rw);
		return DM_MAPIO_SUBMITTED;
	}

	m = choose_mirror(ms, bio->bi_sector);
	if (unlikely(!m))
		return -EIO;

	read_record = mempool_alloc(ms->read_record_pool, GFP_NOIO);
	if (likely(read_record)) {
		dm_bio_record(&read_record->details, bio);
		map_context->ptr = read_record;
		read_record->m = m;
	}

	map_bio(m, bio);

	return DM_MAPIO_REMAPPED;
}

static int mirror_end_io(struct dm_target *ti, struct bio *bio,
			 int error, union map_info *map_context)
{
	int rw = bio_rw(bio);
	struct mirror_set *ms = (struct mirror_set *) ti->private;
	struct mirror *m = NULL;
	struct dm_bio_details *bd = NULL;
	struct dm_raid1_read_record *read_record = map_context->ptr;

	if (rw == WRITE) {
		if (!(bio->bi_rw & REQ_FLUSH))
			dm_rh_dec(ms->rh, map_context->ll);
		return error;
	}

	if (error == -EOPNOTSUPP)
		goto out;

	if ((error == -EWOULDBLOCK) && (bio->bi_rw & REQ_RAHEAD))
		goto out;

	if (unlikely(error)) {
		if (!read_record) {
			DMERR_LIMIT("Mirror read failed.");
			return -EIO;
		}

		m = read_record->m;

		DMERR("Mirror read failed from %s. Trying alternative device.",
		      m->dev->name);

		fail_mirror(m, DM_RAID1_READ_ERROR);

		if (default_ok(m) || mirror_available(ms, bio)) {
			bd = &read_record->details;

			dm_bio_restore(bd, bio);
			mempool_free(read_record, ms->read_record_pool);
			map_context->ptr = NULL;
			queue_bio(ms, bio, rw);
			return 1;
		}
		DMERR("All replicated volumes dead, failing I/O");
	}

out:
	if (read_record) {
		mempool_free(read_record, ms->read_record_pool);
		map_context->ptr = NULL;
	}

	return error;
}

static void mirror_presuspend(struct dm_target *ti)
{
	struct mirror_set *ms = (struct mirror_set *) ti->private;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);

	struct bio_list holds;
	struct bio *bio;

	atomic_set(&ms->suspend, 1);

	spin_lock_irq(&ms->lock);
	holds = ms->holds;
	bio_list_init(&ms->holds);
	spin_unlock_irq(&ms->lock);

	while ((bio = bio_list_pop(&holds)))
		hold_bio(ms, bio);

	dm_rh_stop_recovery(ms->rh);

	wait_event(_kmirrord_recovery_stopped,
		   !dm_rh_recovery_in_flight(ms->rh));

	if (log->type->presuspend && log->type->presuspend(log))
		
		DMWARN("log presuspend failed");

	flush_workqueue(ms->kmirrord_wq);
}

static void mirror_postsuspend(struct dm_target *ti)
{
	struct mirror_set *ms = ti->private;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);

	if (log->type->postsuspend && log->type->postsuspend(log))
		
		DMWARN("log postsuspend failed");
}

static void mirror_resume(struct dm_target *ti)
{
	struct mirror_set *ms = ti->private;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);

	atomic_set(&ms->suspend, 0);
	if (log->type->resume && log->type->resume(log))
		
		DMWARN("log resume failed");
	dm_rh_start_recovery(ms->rh);
}

static char device_status_char(struct mirror *m)
{
	if (!atomic_read(&(m->error_count)))
		return 'A';

	return (test_bit(DM_RAID1_FLUSH_ERROR, &(m->error_type))) ? 'F' :
		(test_bit(DM_RAID1_WRITE_ERROR, &(m->error_type))) ? 'D' :
		(test_bit(DM_RAID1_SYNC_ERROR, &(m->error_type))) ? 'S' :
		(test_bit(DM_RAID1_READ_ERROR, &(m->error_type))) ? 'R' : 'U';
}


static int mirror_status(struct dm_target *ti, status_type_t type,
			 char *result, unsigned int maxlen)
{
	unsigned int m, sz = 0;
	struct mirror_set *ms = (struct mirror_set *) ti->private;
	struct dm_dirty_log *log = dm_rh_dirty_log(ms->rh);
	char buffer[ms->nr_mirrors + 1];

	switch (type) {
	case STATUSTYPE_INFO:
		DMEMIT("%d ", ms->nr_mirrors);
		for (m = 0; m < ms->nr_mirrors; m++) {
			DMEMIT("%s ", ms->mirror[m].dev->name);
			buffer[m] = device_status_char(&(ms->mirror[m]));
		}
		buffer[m] = '\0';

		DMEMIT("%llu/%llu 1 %s ",
		      (unsigned long long)log->type->get_sync_count(log),
		      (unsigned long long)ms->nr_regions, buffer);

		sz += log->type->status(log, type, result+sz, maxlen-sz);

		break;

	case STATUSTYPE_TABLE:
		sz = log->type->status(log, type, result, maxlen);

		DMEMIT("%d", ms->nr_mirrors);
		for (m = 0; m < ms->nr_mirrors; m++)
			DMEMIT(" %s %llu", ms->mirror[m].dev->name,
			       (unsigned long long)ms->mirror[m].offset);

		if (ms->features & DM_RAID1_HANDLE_ERRORS)
			DMEMIT(" 1 handle_errors");
	}

	return 0;
}

static int mirror_iterate_devices(struct dm_target *ti,
				  iterate_devices_callout_fn fn, void *data)
{
	struct mirror_set *ms = ti->private;
	int ret = 0;
	unsigned i;

	for (i = 0; !ret && i < ms->nr_mirrors; i++)
		ret = fn(ti, ms->mirror[i].dev,
			 ms->mirror[i].offset, ti->len, data);

	return ret;
}

static struct target_type mirror_target = {
	.name	 = "mirror",
	.version = {1, 12, 1},
	.module	 = THIS_MODULE,
	.ctr	 = mirror_ctr,
	.dtr	 = mirror_dtr,
	.map	 = mirror_map,
	.end_io	 = mirror_end_io,
	.presuspend = mirror_presuspend,
	.postsuspend = mirror_postsuspend,
	.resume	 = mirror_resume,
	.status	 = mirror_status,
	.iterate_devices = mirror_iterate_devices,
};

static int __init dm_mirror_init(void)
{
	int r;

	_dm_raid1_read_record_cache = KMEM_CACHE(dm_raid1_read_record, 0);
	if (!_dm_raid1_read_record_cache) {
		DMERR("Can't allocate dm_raid1_read_record cache");
		r = -ENOMEM;
		goto bad_cache;
	}

	r = dm_register_target(&mirror_target);
	if (r < 0) {
		DMERR("Failed to register mirror target");
		goto bad_target;
	}

	return 0;

bad_target:
	kmem_cache_destroy(_dm_raid1_read_record_cache);
bad_cache:
	return r;
}

static void __exit dm_mirror_exit(void)
{
	dm_unregister_target(&mirror_target);
	kmem_cache_destroy(_dm_raid1_read_record_cache);
}

module_init(dm_mirror_init);
module_exit(dm_mirror_exit);

MODULE_DESCRIPTION(DM_NAME " mirror target");
MODULE_AUTHOR("Joe Thornber");
MODULE_LICENSE("GPL");
