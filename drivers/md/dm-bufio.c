/*
 * Copyright (C) 2009-2011 Red Hat, Inc.
 *
 * Author: Mikulas Patocka <mpatocka@redhat.com>
 *
 * This file is released under the GPL.
 */

#include "dm-bufio.h"

#include <linux/device-mapper.h>
#include <linux/dm-io.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/shrinker.h>
#include <linux/module.h>

#define DM_MSG_PREFIX "bufio"

#define DM_BUFIO_MIN_BUFFERS		8

#define DM_BUFIO_MEMORY_PERCENT		2
#define DM_BUFIO_VMALLOC_PERCENT	25
#define DM_BUFIO_WRITEBACK_PERCENT	75

#define DM_BUFIO_WORK_TIMER_SECS	10

#define DM_BUFIO_DEFAULT_AGE_SECS	60

#define DM_BUFIO_INLINE_VECS		16

#define DM_BUFIO_HASH_BITS	20
#define DM_BUFIO_HASH(block) \
	((((block) >> DM_BUFIO_HASH_BITS) ^ (block)) & \
	 ((1 << DM_BUFIO_HASH_BITS) - 1))

#define DM_BUFIO_BLOCK_SIZE_SLAB_LIMIT	(PAGE_SIZE >> 1)
#define DM_BUFIO_BLOCK_SIZE_GFP_LIMIT	(PAGE_SIZE << (MAX_ORDER - 1))

#define LIST_CLEAN	0
#define LIST_DIRTY	1
#define LIST_SIZE	2

/*
 * Linking of buffers:
 *	All buffers are linked to cache_hash with their hash_list field.
 *
 *	Clean buffers that are not being written (B_WRITING not set)
 *	are linked to lru[LIST_CLEAN] with their lru_list field.
 *
 *	Dirty and clean buffers that are being written are linked to
 *	lru[LIST_DIRTY] with their lru_list field. When the write
 *	finishes, the buffer cannot be relinked immediately (because we
 *	are in an interrupt context and relinking requires process
 *	context), so some clean-not-writing buffers can be held on
 *	dirty_lru too.  They are later added to lru in the process
 *	context.
 */
struct dm_bufio_client {
	struct mutex lock;

	struct list_head lru[LIST_SIZE];
	unsigned long n_buffers[LIST_SIZE];

	struct block_device *bdev;
	unsigned block_size;
	unsigned char sectors_per_block_bits;
	unsigned char pages_per_block_bits;
	unsigned char blocks_per_page_bits;
	unsigned aux_size;
	void (*alloc_callback)(struct dm_buffer *);
	void (*write_callback)(struct dm_buffer *);

	struct dm_io_client *dm_io;

	struct list_head reserved_buffers;
	unsigned need_reserved_buffers;

	struct hlist_head *cache_hash;
	wait_queue_head_t free_buffer_wait;

	int async_write_error;

	struct list_head client_list;
	struct shrinker shrinker;
};

#define B_READING	0
#define B_WRITING	1
#define B_DIRTY		2

enum data_mode {
	DATA_MODE_SLAB = 0,
	DATA_MODE_GET_FREE_PAGES = 1,
	DATA_MODE_VMALLOC = 2,
	DATA_MODE_LIMIT = 3
};

struct dm_buffer {
	struct hlist_node hash_list;
	struct list_head lru_list;
	sector_t block;
	void *data;
	enum data_mode data_mode;
	unsigned char list_mode;		
	unsigned hold_count;
	int read_error;
	int write_error;
	unsigned long state;
	unsigned long last_accessed;
	struct dm_bufio_client *c;
	struct bio bio;
	struct bio_vec bio_vec[DM_BUFIO_INLINE_VECS];
};


static struct kmem_cache *dm_bufio_caches[PAGE_SHIFT - SECTOR_SHIFT];
static char *dm_bufio_cache_names[PAGE_SHIFT - SECTOR_SHIFT];

static inline int dm_bufio_cache_index(struct dm_bufio_client *c)
{
	unsigned ret = c->blocks_per_page_bits - 1;

	BUG_ON(ret >= ARRAY_SIZE(dm_bufio_caches));

	return ret;
}

#define DM_BUFIO_CACHE(c)	(dm_bufio_caches[dm_bufio_cache_index(c)])
#define DM_BUFIO_CACHE_NAME(c)	(dm_bufio_cache_names[dm_bufio_cache_index(c)])

#define dm_bufio_in_request()	(!!current->bio_list)

static void dm_bufio_lock(struct dm_bufio_client *c)
{
	mutex_lock_nested(&c->lock, dm_bufio_in_request());
}

static int dm_bufio_trylock(struct dm_bufio_client *c)
{
	return mutex_trylock(&c->lock);
}

static void dm_bufio_unlock(struct dm_bufio_client *c)
{
	mutex_unlock(&c->lock);
}

#ifdef CONFIG_PREEMPT_VOLUNTARY
#  define dm_bufio_cond_resched()		\
do {						\
	if (unlikely(need_resched()))		\
		_cond_resched();		\
} while (0)
#else
#  define dm_bufio_cond_resched()                do { } while (0)
#endif


static unsigned long dm_bufio_default_cache_size;

static unsigned long dm_bufio_cache_size;

static unsigned long dm_bufio_cache_size_latch;

static DEFINE_SPINLOCK(param_spinlock);

static unsigned dm_bufio_max_age = DM_BUFIO_DEFAULT_AGE_SECS;

static unsigned long dm_bufio_peak_allocated;
static unsigned long dm_bufio_allocated_kmem_cache;
static unsigned long dm_bufio_allocated_get_free_pages;
static unsigned long dm_bufio_allocated_vmalloc;
static unsigned long dm_bufio_current_allocated;


static unsigned long dm_bufio_cache_size_per_client;

static int dm_bufio_client_count;

static LIST_HEAD(dm_bufio_all_clients);

static DEFINE_MUTEX(dm_bufio_clients_lock);


static void adjust_total_allocated(enum data_mode data_mode, long diff)
{
	static unsigned long * const class_ptr[DATA_MODE_LIMIT] = {
		&dm_bufio_allocated_kmem_cache,
		&dm_bufio_allocated_get_free_pages,
		&dm_bufio_allocated_vmalloc,
	};

	spin_lock(&param_spinlock);

	*class_ptr[data_mode] += diff;

	dm_bufio_current_allocated += diff;

	if (dm_bufio_current_allocated > dm_bufio_peak_allocated)
		dm_bufio_peak_allocated = dm_bufio_current_allocated;

	spin_unlock(&param_spinlock);
}

static void __cache_size_refresh(void)
{
	BUG_ON(!mutex_is_locked(&dm_bufio_clients_lock));
	BUG_ON(dm_bufio_client_count < 0);

	dm_bufio_cache_size_latch = dm_bufio_cache_size;

	barrier();

	if (!dm_bufio_cache_size_latch) {
		(void)cmpxchg(&dm_bufio_cache_size, 0,
			      dm_bufio_default_cache_size);
		dm_bufio_cache_size_latch = dm_bufio_default_cache_size;
	}

	dm_bufio_cache_size_per_client = dm_bufio_cache_size_latch /
					 (dm_bufio_client_count ? : 1);
}

static void *alloc_buffer_data(struct dm_bufio_client *c, gfp_t gfp_mask,
			       enum data_mode *data_mode)
{
	if (c->block_size <= DM_BUFIO_BLOCK_SIZE_SLAB_LIMIT) {
		*data_mode = DATA_MODE_SLAB;
		return kmem_cache_alloc(DM_BUFIO_CACHE(c), gfp_mask);
	}

	if (c->block_size <= DM_BUFIO_BLOCK_SIZE_GFP_LIMIT &&
	    gfp_mask & __GFP_NORETRY) {
		*data_mode = DATA_MODE_GET_FREE_PAGES;
		return (void *)__get_free_pages(gfp_mask,
						c->pages_per_block_bits);
	}

	*data_mode = DATA_MODE_VMALLOC;
	return __vmalloc(c->block_size, gfp_mask, PAGE_KERNEL);
}

static void free_buffer_data(struct dm_bufio_client *c,
			     void *data, enum data_mode data_mode)
{
	switch (data_mode) {
	case DATA_MODE_SLAB:
		kmem_cache_free(DM_BUFIO_CACHE(c), data);
		break;

	case DATA_MODE_GET_FREE_PAGES:
		free_pages((unsigned long)data, c->pages_per_block_bits);
		break;

	case DATA_MODE_VMALLOC:
		vfree(data);
		break;

	default:
		DMCRIT("dm_bufio_free_buffer_data: bad data mode: %d",
		       data_mode);
		BUG();
	}
}

static struct dm_buffer *alloc_buffer(struct dm_bufio_client *c, gfp_t gfp_mask)
{
	struct dm_buffer *b = kmalloc(sizeof(struct dm_buffer) + c->aux_size,
				      gfp_mask);

	if (!b)
		return NULL;

	b->c = c;

	b->data = alloc_buffer_data(c, gfp_mask, &b->data_mode);
	if (!b->data) {
		kfree(b);
		return NULL;
	}

	adjust_total_allocated(b->data_mode, (long)c->block_size);

	return b;
}

static void free_buffer(struct dm_buffer *b)
{
	struct dm_bufio_client *c = b->c;

	adjust_total_allocated(b->data_mode, -(long)c->block_size);

	free_buffer_data(c, b->data, b->data_mode);
	kfree(b);
}

static void __link_buffer(struct dm_buffer *b, sector_t block, int dirty)
{
	struct dm_bufio_client *c = b->c;

	c->n_buffers[dirty]++;
	b->block = block;
	b->list_mode = dirty;
	list_add(&b->lru_list, &c->lru[dirty]);
	hlist_add_head(&b->hash_list, &c->cache_hash[DM_BUFIO_HASH(block)]);
	b->last_accessed = jiffies;
}

static void __unlink_buffer(struct dm_buffer *b)
{
	struct dm_bufio_client *c = b->c;

	BUG_ON(!c->n_buffers[b->list_mode]);

	c->n_buffers[b->list_mode]--;
	hlist_del(&b->hash_list);
	list_del(&b->lru_list);
}

static void __relink_lru(struct dm_buffer *b, int dirty)
{
	struct dm_bufio_client *c = b->c;

	BUG_ON(!c->n_buffers[b->list_mode]);

	c->n_buffers[b->list_mode]--;
	c->n_buffers[dirty]++;
	b->list_mode = dirty;
	list_del(&b->lru_list);
	list_add(&b->lru_list, &c->lru[dirty]);
}


static void dmio_complete(unsigned long error, void *context)
{
	struct dm_buffer *b = context;

	b->bio.bi_end_io(&b->bio, error ? -EIO : 0);
}

static void use_dmio(struct dm_buffer *b, int rw, sector_t block,
		     bio_end_io_t *end_io)
{
	int r;
	struct dm_io_request io_req = {
		.bi_rw = rw,
		.notify.fn = dmio_complete,
		.notify.context = b,
		.client = b->c->dm_io,
	};
	struct dm_io_region region = {
		.bdev = b->c->bdev,
		.sector = block << b->c->sectors_per_block_bits,
		.count = b->c->block_size >> SECTOR_SHIFT,
	};

	if (b->data_mode != DATA_MODE_VMALLOC) {
		io_req.mem.type = DM_IO_KMEM;
		io_req.mem.ptr.addr = b->data;
	} else {
		io_req.mem.type = DM_IO_VMA;
		io_req.mem.ptr.vma = b->data;
	}

	b->bio.bi_end_io = end_io;

	r = dm_io(&io_req, 1, &region, NULL);
	if (r)
		end_io(&b->bio, r);
}

static void use_inline_bio(struct dm_buffer *b, int rw, sector_t block,
			   bio_end_io_t *end_io)
{
	char *ptr;
	int len;

	bio_init(&b->bio);
	b->bio.bi_io_vec = b->bio_vec;
	b->bio.bi_max_vecs = DM_BUFIO_INLINE_VECS;
	b->bio.bi_sector = block << b->c->sectors_per_block_bits;
	b->bio.bi_bdev = b->c->bdev;
	b->bio.bi_end_io = end_io;

	ptr = b->data;
	len = b->c->block_size;

	if (len >= PAGE_SIZE)
		BUG_ON((unsigned long)ptr & (PAGE_SIZE - 1));
	else
		BUG_ON((unsigned long)ptr & (len - 1));

	do {
		if (!bio_add_page(&b->bio, virt_to_page(ptr),
				  len < PAGE_SIZE ? len : PAGE_SIZE,
				  virt_to_phys(ptr) & (PAGE_SIZE - 1))) {
			BUG_ON(b->c->block_size <= PAGE_SIZE);
			use_dmio(b, rw, block, end_io);
			return;
		}

		len -= PAGE_SIZE;
		ptr += PAGE_SIZE;
	} while (len > 0);

	submit_bio(rw, &b->bio);
}

static void submit_io(struct dm_buffer *b, int rw, sector_t block,
		      bio_end_io_t *end_io)
{
	if (rw == WRITE && b->c->write_callback)
		b->c->write_callback(b);

	if (b->c->block_size <= DM_BUFIO_INLINE_VECS * PAGE_SIZE &&
	    b->data_mode != DATA_MODE_VMALLOC)
		use_inline_bio(b, rw, block, end_io);
	else
		use_dmio(b, rw, block, end_io);
}


static void write_endio(struct bio *bio, int error)
{
	struct dm_buffer *b = container_of(bio, struct dm_buffer, bio);

	b->write_error = error;
	if (unlikely(error)) {
		struct dm_bufio_client *c = b->c;
		(void)cmpxchg(&c->async_write_error, 0, error);
	}

	BUG_ON(!test_bit(B_WRITING, &b->state));

	smp_mb__before_clear_bit();
	clear_bit(B_WRITING, &b->state);
	smp_mb__after_clear_bit();

	wake_up_bit(&b->state, B_WRITING);
}

static int do_io_schedule(void *word)
{
	io_schedule();

	return 0;
}

static void __write_dirty_buffer(struct dm_buffer *b)
{
	if (!test_bit(B_DIRTY, &b->state))
		return;

	clear_bit(B_DIRTY, &b->state);
	wait_on_bit_lock(&b->state, B_WRITING,
			 do_io_schedule, TASK_UNINTERRUPTIBLE);

	submit_io(b, WRITE, b->block, write_endio);
}

static void __make_buffer_clean(struct dm_buffer *b)
{
	BUG_ON(b->hold_count);

	if (!b->state)	
		return;

	wait_on_bit(&b->state, B_READING, do_io_schedule, TASK_UNINTERRUPTIBLE);
	__write_dirty_buffer(b);
	wait_on_bit(&b->state, B_WRITING, do_io_schedule, TASK_UNINTERRUPTIBLE);
}

static struct dm_buffer *__get_unclaimed_buffer(struct dm_bufio_client *c)
{
	struct dm_buffer *b;

	list_for_each_entry_reverse(b, &c->lru[LIST_CLEAN], lru_list) {
		BUG_ON(test_bit(B_WRITING, &b->state));
		BUG_ON(test_bit(B_DIRTY, &b->state));

		if (!b->hold_count) {
			__make_buffer_clean(b);
			__unlink_buffer(b);
			return b;
		}
		dm_bufio_cond_resched();
	}

	list_for_each_entry_reverse(b, &c->lru[LIST_DIRTY], lru_list) {
		BUG_ON(test_bit(B_READING, &b->state));

		if (!b->hold_count) {
			__make_buffer_clean(b);
			__unlink_buffer(b);
			return b;
		}
		dm_bufio_cond_resched();
	}

	return NULL;
}

static void __wait_for_free_buffer(struct dm_bufio_client *c)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&c->free_buffer_wait, &wait);
	set_task_state(current, TASK_UNINTERRUPTIBLE);
	dm_bufio_unlock(c);

	io_schedule();

	set_task_state(current, TASK_RUNNING);
	remove_wait_queue(&c->free_buffer_wait, &wait);

	dm_bufio_lock(c);
}

enum new_flag {
	NF_FRESH = 0,
	NF_READ = 1,
	NF_GET = 2,
	NF_PREFETCH = 3
};

static struct dm_buffer *__alloc_buffer_wait_no_callback(struct dm_bufio_client *c, enum new_flag nf)
{
	struct dm_buffer *b;

	while (1) {
		if (dm_bufio_cache_size_latch != 1) {
			b = alloc_buffer(c, GFP_NOIO | __GFP_NORETRY | __GFP_NOMEMALLOC | __GFP_NOWARN);
			if (b)
				return b;
		}

		if (nf == NF_PREFETCH)
			return NULL;

		if (!list_empty(&c->reserved_buffers)) {
			b = list_entry(c->reserved_buffers.next,
				       struct dm_buffer, lru_list);
			list_del(&b->lru_list);
			c->need_reserved_buffers++;

			return b;
		}

		b = __get_unclaimed_buffer(c);
		if (b)
			return b;

		__wait_for_free_buffer(c);
	}
}

static struct dm_buffer *__alloc_buffer_wait(struct dm_bufio_client *c, enum new_flag nf)
{
	struct dm_buffer *b = __alloc_buffer_wait_no_callback(c, nf);

	if (!b)
		return NULL;

	if (c->alloc_callback)
		c->alloc_callback(b);

	return b;
}

static void __free_buffer_wake(struct dm_buffer *b)
{
	struct dm_bufio_client *c = b->c;

	if (!c->need_reserved_buffers)
		free_buffer(b);
	else {
		list_add(&b->lru_list, &c->reserved_buffers);
		c->need_reserved_buffers--;
	}

	wake_up(&c->free_buffer_wait);
}

static void __write_dirty_buffers_async(struct dm_bufio_client *c, int no_wait)
{
	struct dm_buffer *b, *tmp;

	list_for_each_entry_safe_reverse(b, tmp, &c->lru[LIST_DIRTY], lru_list) {
		BUG_ON(test_bit(B_READING, &b->state));

		if (!test_bit(B_DIRTY, &b->state) &&
		    !test_bit(B_WRITING, &b->state)) {
			__relink_lru(b, LIST_CLEAN);
			continue;
		}

		if (no_wait && test_bit(B_WRITING, &b->state))
			return;

		__write_dirty_buffer(b);
		dm_bufio_cond_resched();
	}
}

static void __get_memory_limit(struct dm_bufio_client *c,
			       unsigned long *threshold_buffers,
			       unsigned long *limit_buffers)
{
	unsigned long buffers;

	if (dm_bufio_cache_size != dm_bufio_cache_size_latch) {
		mutex_lock(&dm_bufio_clients_lock);
		__cache_size_refresh();
		mutex_unlock(&dm_bufio_clients_lock);
	}

	buffers = dm_bufio_cache_size_per_client >>
		  (c->sectors_per_block_bits + SECTOR_SHIFT);

	if (buffers < DM_BUFIO_MIN_BUFFERS)
		buffers = DM_BUFIO_MIN_BUFFERS;

	*limit_buffers = buffers;
	*threshold_buffers = buffers * DM_BUFIO_WRITEBACK_PERCENT / 100;
}

static void __check_watermark(struct dm_bufio_client *c)
{
	unsigned long threshold_buffers, limit_buffers;

	__get_memory_limit(c, &threshold_buffers, &limit_buffers);

	while (c->n_buffers[LIST_CLEAN] + c->n_buffers[LIST_DIRTY] >
	       limit_buffers) {

		struct dm_buffer *b = __get_unclaimed_buffer(c);

		if (!b)
			return;

		__free_buffer_wake(b);
		dm_bufio_cond_resched();
	}

	if (c->n_buffers[LIST_DIRTY] > threshold_buffers)
		__write_dirty_buffers_async(c, 1);
}

static struct dm_buffer *__find(struct dm_bufio_client *c, sector_t block)
{
	struct dm_buffer *b;
	struct hlist_node *hn;

	hlist_for_each_entry(b, hn, &c->cache_hash[DM_BUFIO_HASH(block)],
			     hash_list) {
		dm_bufio_cond_resched();
		if (b->block == block)
			return b;
	}

	return NULL;
}


static struct dm_buffer *__bufio_new(struct dm_bufio_client *c, sector_t block,
				     enum new_flag nf, int *need_submit)
{
	struct dm_buffer *b, *new_b = NULL;

	*need_submit = 0;

	b = __find(c, block);
	if (b)
		goto found_buffer;

	if (nf == NF_GET)
		return NULL;

	new_b = __alloc_buffer_wait(c, nf);
	if (!new_b)
		return NULL;

	b = __find(c, block);
	if (b) {
		__free_buffer_wake(new_b);
		goto found_buffer;
	}

	__check_watermark(c);

	b = new_b;
	b->hold_count = 1;
	b->read_error = 0;
	b->write_error = 0;
	__link_buffer(b, block, LIST_CLEAN);

	if (nf == NF_FRESH) {
		b->state = 0;
		return b;
	}

	b->state = 1 << B_READING;
	*need_submit = 1;

	return b;

found_buffer:
	if (nf == NF_PREFETCH)
		return NULL;
	if (nf == NF_GET && unlikely(test_bit(B_READING, &b->state)))
		return NULL;

	b->hold_count++;
	__relink_lru(b, test_bit(B_DIRTY, &b->state) ||
		     test_bit(B_WRITING, &b->state));
	return b;
}

static void read_endio(struct bio *bio, int error)
{
	struct dm_buffer *b = container_of(bio, struct dm_buffer, bio);

	b->read_error = error;

	BUG_ON(!test_bit(B_READING, &b->state));

	smp_mb__before_clear_bit();
	clear_bit(B_READING, &b->state);
	smp_mb__after_clear_bit();

	wake_up_bit(&b->state, B_READING);
}

static void *new_read(struct dm_bufio_client *c, sector_t block,
		      enum new_flag nf, struct dm_buffer **bp)
{
	int need_submit;
	struct dm_buffer *b;

	dm_bufio_lock(c);
	b = __bufio_new(c, block, nf, &need_submit);
	dm_bufio_unlock(c);

	if (!b)
		return b;

	if (need_submit)
		submit_io(b, READ, b->block, read_endio);

	wait_on_bit(&b->state, B_READING, do_io_schedule, TASK_UNINTERRUPTIBLE);

	if (b->read_error) {
		int error = b->read_error;

		dm_bufio_release(b);

		return ERR_PTR(error);
	}

	*bp = b;

	return b->data;
}

void *dm_bufio_get(struct dm_bufio_client *c, sector_t block,
		   struct dm_buffer **bp)
{
	return new_read(c, block, NF_GET, bp);
}
EXPORT_SYMBOL_GPL(dm_bufio_get);

void *dm_bufio_read(struct dm_bufio_client *c, sector_t block,
		    struct dm_buffer **bp)
{
	BUG_ON(dm_bufio_in_request());

	return new_read(c, block, NF_READ, bp);
}
EXPORT_SYMBOL_GPL(dm_bufio_read);

void *dm_bufio_new(struct dm_bufio_client *c, sector_t block,
		   struct dm_buffer **bp)
{
	BUG_ON(dm_bufio_in_request());

	return new_read(c, block, NF_FRESH, bp);
}
EXPORT_SYMBOL_GPL(dm_bufio_new);

void dm_bufio_prefetch(struct dm_bufio_client *c,
		       sector_t block, unsigned n_blocks)
{
	struct blk_plug plug;

	blk_start_plug(&plug);
	dm_bufio_lock(c);

	for (; n_blocks--; block++) {
		int need_submit;
		struct dm_buffer *b;
		b = __bufio_new(c, block, NF_PREFETCH, &need_submit);
		if (unlikely(b != NULL)) {
			dm_bufio_unlock(c);

			if (need_submit)
				submit_io(b, READ, b->block, read_endio);
			dm_bufio_release(b);

			dm_bufio_cond_resched();

			if (!n_blocks)
				goto flush_plug;
			dm_bufio_lock(c);
		}

	}

	dm_bufio_unlock(c);

flush_plug:
	blk_finish_plug(&plug);
}
EXPORT_SYMBOL_GPL(dm_bufio_prefetch);

void dm_bufio_release(struct dm_buffer *b)
{
	struct dm_bufio_client *c = b->c;

	dm_bufio_lock(c);

	BUG_ON(!b->hold_count);

	b->hold_count--;
	if (!b->hold_count) {
		wake_up(&c->free_buffer_wait);

		/*
		 * If there were errors on the buffer, and the buffer is not
		 * to be written, free the buffer. There is no point in caching
		 * invalid buffer.
		 */
		if ((b->read_error || b->write_error) &&
		    !test_bit(B_READING, &b->state) &&
		    !test_bit(B_WRITING, &b->state) &&
		    !test_bit(B_DIRTY, &b->state)) {
			__unlink_buffer(b);
			__free_buffer_wake(b);
		}
	}

	dm_bufio_unlock(c);
}
EXPORT_SYMBOL_GPL(dm_bufio_release);

void dm_bufio_mark_buffer_dirty(struct dm_buffer *b)
{
	struct dm_bufio_client *c = b->c;

	dm_bufio_lock(c);

	BUG_ON(test_bit(B_READING, &b->state));

	if (!test_and_set_bit(B_DIRTY, &b->state))
		__relink_lru(b, LIST_DIRTY);

	dm_bufio_unlock(c);
}
EXPORT_SYMBOL_GPL(dm_bufio_mark_buffer_dirty);

void dm_bufio_write_dirty_buffers_async(struct dm_bufio_client *c)
{
	BUG_ON(dm_bufio_in_request());

	dm_bufio_lock(c);
	__write_dirty_buffers_async(c, 0);
	dm_bufio_unlock(c);
}
EXPORT_SYMBOL_GPL(dm_bufio_write_dirty_buffers_async);

/*
 * For performance, it is essential that the buffers are written asynchronously
 * and simultaneously (so that the block layer can merge the writes) and then
 * waited upon.
 *
 * Finally, we flush hardware disk cache.
 */
int dm_bufio_write_dirty_buffers(struct dm_bufio_client *c)
{
	int a, f;
	unsigned long buffers_processed = 0;
	struct dm_buffer *b, *tmp;

	dm_bufio_lock(c);
	__write_dirty_buffers_async(c, 0);

again:
	list_for_each_entry_safe_reverse(b, tmp, &c->lru[LIST_DIRTY], lru_list) {
		int dropped_lock = 0;

		if (buffers_processed < c->n_buffers[LIST_DIRTY])
			buffers_processed++;

		BUG_ON(test_bit(B_READING, &b->state));

		if (test_bit(B_WRITING, &b->state)) {
			if (buffers_processed < c->n_buffers[LIST_DIRTY]) {
				dropped_lock = 1;
				b->hold_count++;
				dm_bufio_unlock(c);
				wait_on_bit(&b->state, B_WRITING,
					    do_io_schedule,
					    TASK_UNINTERRUPTIBLE);
				dm_bufio_lock(c);
				b->hold_count--;
			} else
				wait_on_bit(&b->state, B_WRITING,
					    do_io_schedule,
					    TASK_UNINTERRUPTIBLE);
		}

		if (!test_bit(B_DIRTY, &b->state) &&
		    !test_bit(B_WRITING, &b->state))
			__relink_lru(b, LIST_CLEAN);

		dm_bufio_cond_resched();

		if (dropped_lock)
			goto again;
	}
	wake_up(&c->free_buffer_wait);
	dm_bufio_unlock(c);

	a = xchg(&c->async_write_error, 0);
	f = dm_bufio_issue_flush(c);
	if (a)
		return a;

	return f;
}
EXPORT_SYMBOL_GPL(dm_bufio_write_dirty_buffers);

int dm_bufio_issue_flush(struct dm_bufio_client *c)
{
	struct dm_io_request io_req = {
		.bi_rw = REQ_FLUSH,
		.mem.type = DM_IO_KMEM,
		.mem.ptr.addr = NULL,
		.client = c->dm_io,
	};
	struct dm_io_region io_reg = {
		.bdev = c->bdev,
		.sector = 0,
		.count = 0,
	};

	BUG_ON(dm_bufio_in_request());

	return dm_io(&io_req, 1, &io_reg, NULL);
}
EXPORT_SYMBOL_GPL(dm_bufio_issue_flush);

void dm_bufio_release_move(struct dm_buffer *b, sector_t new_block)
{
	struct dm_bufio_client *c = b->c;
	struct dm_buffer *new;

	BUG_ON(dm_bufio_in_request());

	dm_bufio_lock(c);

retry:
	new = __find(c, new_block);
	if (new) {
		if (new->hold_count) {
			__wait_for_free_buffer(c);
			goto retry;
		}

		/*
		 * FIXME: Is there any point waiting for a write that's going
		 * to be overwritten in a bit?
		 */
		__make_buffer_clean(new);
		__unlink_buffer(new);
		__free_buffer_wake(new);
	}

	BUG_ON(!b->hold_count);
	BUG_ON(test_bit(B_READING, &b->state));

	__write_dirty_buffer(b);
	if (b->hold_count == 1) {
		wait_on_bit(&b->state, B_WRITING,
			    do_io_schedule, TASK_UNINTERRUPTIBLE);
		set_bit(B_DIRTY, &b->state);
		__unlink_buffer(b);
		__link_buffer(b, new_block, LIST_DIRTY);
	} else {
		sector_t old_block;
		wait_on_bit_lock(&b->state, B_WRITING,
				 do_io_schedule, TASK_UNINTERRUPTIBLE);
		old_block = b->block;
		__unlink_buffer(b);
		__link_buffer(b, new_block, b->list_mode);
		submit_io(b, WRITE, new_block, write_endio);
		wait_on_bit(&b->state, B_WRITING,
			    do_io_schedule, TASK_UNINTERRUPTIBLE);
		__unlink_buffer(b);
		__link_buffer(b, old_block, b->list_mode);
	}

	dm_bufio_unlock(c);
	dm_bufio_release(b);
}
EXPORT_SYMBOL_GPL(dm_bufio_release_move);

unsigned dm_bufio_get_block_size(struct dm_bufio_client *c)
{
	return c->block_size;
}
EXPORT_SYMBOL_GPL(dm_bufio_get_block_size);

sector_t dm_bufio_get_device_size(struct dm_bufio_client *c)
{
	return i_size_read(c->bdev->bd_inode) >>
			   (SECTOR_SHIFT + c->sectors_per_block_bits);
}
EXPORT_SYMBOL_GPL(dm_bufio_get_device_size);

sector_t dm_bufio_get_block_number(struct dm_buffer *b)
{
	return b->block;
}
EXPORT_SYMBOL_GPL(dm_bufio_get_block_number);

void *dm_bufio_get_block_data(struct dm_buffer *b)
{
	return b->data;
}
EXPORT_SYMBOL_GPL(dm_bufio_get_block_data);

void *dm_bufio_get_aux_data(struct dm_buffer *b)
{
	return b + 1;
}
EXPORT_SYMBOL_GPL(dm_bufio_get_aux_data);

struct dm_bufio_client *dm_bufio_get_client(struct dm_buffer *b)
{
	return b->c;
}
EXPORT_SYMBOL_GPL(dm_bufio_get_client);

static void drop_buffers(struct dm_bufio_client *c)
{
	struct dm_buffer *b;
	int i;

	BUG_ON(dm_bufio_in_request());

	/*
	 * An optimization so that the buffers are not written one-by-one.
	 */
	dm_bufio_write_dirty_buffers_async(c);

	dm_bufio_lock(c);

	while ((b = __get_unclaimed_buffer(c)))
		__free_buffer_wake(b);

	for (i = 0; i < LIST_SIZE; i++)
		list_for_each_entry(b, &c->lru[i], lru_list)
			DMERR("leaked buffer %llx, hold count %u, list %d",
			      (unsigned long long)b->block, b->hold_count, i);

	for (i = 0; i < LIST_SIZE; i++)
		BUG_ON(!list_empty(&c->lru[i]));

	dm_bufio_unlock(c);
}

static int __cleanup_old_buffer(struct dm_buffer *b, gfp_t gfp,
				unsigned long max_jiffies)
{
	if (jiffies - b->last_accessed < max_jiffies)
		return 1;

	if (!(gfp & __GFP_IO)) {
		if (test_bit(B_READING, &b->state) ||
		    test_bit(B_WRITING, &b->state) ||
		    test_bit(B_DIRTY, &b->state))
			return 1;
	}

	if (b->hold_count)
		return 1;

	__make_buffer_clean(b);
	__unlink_buffer(b);
	__free_buffer_wake(b);

	return 0;
}

static void __scan(struct dm_bufio_client *c, unsigned long nr_to_scan,
		   struct shrink_control *sc)
{
	int l;
	struct dm_buffer *b, *tmp;

	for (l = 0; l < LIST_SIZE; l++) {
		list_for_each_entry_safe_reverse(b, tmp, &c->lru[l], lru_list)
			if (!__cleanup_old_buffer(b, sc->gfp_mask, 0) &&
			    !--nr_to_scan)
				return;
		dm_bufio_cond_resched();
	}
}

static int shrink(struct shrinker *shrinker, struct shrink_control *sc)
{
	struct dm_bufio_client *c =
	    container_of(shrinker, struct dm_bufio_client, shrinker);
	unsigned long r;
	unsigned long nr_to_scan = sc->nr_to_scan;

	if (sc->gfp_mask & __GFP_IO)
		dm_bufio_lock(c);
	else if (!dm_bufio_trylock(c))
		return !nr_to_scan ? 0 : -1;

	if (nr_to_scan)
		__scan(c, nr_to_scan, sc);

	r = c->n_buffers[LIST_CLEAN] + c->n_buffers[LIST_DIRTY];
	if (r > INT_MAX)
		r = INT_MAX;

	dm_bufio_unlock(c);

	return r;
}

struct dm_bufio_client *dm_bufio_client_create(struct block_device *bdev, unsigned block_size,
					       unsigned reserved_buffers, unsigned aux_size,
					       void (*alloc_callback)(struct dm_buffer *),
					       void (*write_callback)(struct dm_buffer *))
{
	int r;
	struct dm_bufio_client *c;
	unsigned i;

	BUG_ON(block_size < 1 << SECTOR_SHIFT ||
	       (block_size & (block_size - 1)));

	c = kmalloc(sizeof(*c), GFP_KERNEL);
	if (!c) {
		r = -ENOMEM;
		goto bad_client;
	}
	c->cache_hash = vmalloc(sizeof(struct hlist_head) << DM_BUFIO_HASH_BITS);
	if (!c->cache_hash) {
		r = -ENOMEM;
		goto bad_hash;
	}

	c->bdev = bdev;
	c->block_size = block_size;
	c->sectors_per_block_bits = ffs(block_size) - 1 - SECTOR_SHIFT;
	c->pages_per_block_bits = (ffs(block_size) - 1 >= PAGE_SHIFT) ?
				  ffs(block_size) - 1 - PAGE_SHIFT : 0;
	c->blocks_per_page_bits = (ffs(block_size) - 1 < PAGE_SHIFT ?
				  PAGE_SHIFT - (ffs(block_size) - 1) : 0);

	c->aux_size = aux_size;
	c->alloc_callback = alloc_callback;
	c->write_callback = write_callback;

	for (i = 0; i < LIST_SIZE; i++) {
		INIT_LIST_HEAD(&c->lru[i]);
		c->n_buffers[i] = 0;
	}

	for (i = 0; i < 1 << DM_BUFIO_HASH_BITS; i++)
		INIT_HLIST_HEAD(&c->cache_hash[i]);

	mutex_init(&c->lock);
	INIT_LIST_HEAD(&c->reserved_buffers);
	c->need_reserved_buffers = reserved_buffers;

	init_waitqueue_head(&c->free_buffer_wait);
	c->async_write_error = 0;

	c->dm_io = dm_io_client_create();
	if (IS_ERR(c->dm_io)) {
		r = PTR_ERR(c->dm_io);
		goto bad_dm_io;
	}

	mutex_lock(&dm_bufio_clients_lock);
	if (c->blocks_per_page_bits) {
		if (!DM_BUFIO_CACHE_NAME(c)) {
			DM_BUFIO_CACHE_NAME(c) = kasprintf(GFP_KERNEL, "dm_bufio_cache-%u", c->block_size);
			if (!DM_BUFIO_CACHE_NAME(c)) {
				r = -ENOMEM;
				mutex_unlock(&dm_bufio_clients_lock);
				goto bad_cache;
			}
		}

		if (!DM_BUFIO_CACHE(c)) {
			DM_BUFIO_CACHE(c) = kmem_cache_create(DM_BUFIO_CACHE_NAME(c),
							      c->block_size,
							      c->block_size, 0, NULL);
			if (!DM_BUFIO_CACHE(c)) {
				r = -ENOMEM;
				mutex_unlock(&dm_bufio_clients_lock);
				goto bad_cache;
			}
		}
	}
	mutex_unlock(&dm_bufio_clients_lock);

	while (c->need_reserved_buffers) {
		struct dm_buffer *b = alloc_buffer(c, GFP_KERNEL);

		if (!b) {
			r = -ENOMEM;
			goto bad_buffer;
		}
		__free_buffer_wake(b);
	}

	mutex_lock(&dm_bufio_clients_lock);
	dm_bufio_client_count++;
	list_add(&c->client_list, &dm_bufio_all_clients);
	__cache_size_refresh();
	mutex_unlock(&dm_bufio_clients_lock);

	c->shrinker.shrink = shrink;
	c->shrinker.seeks = 1;
	c->shrinker.batch = 0;
	register_shrinker(&c->shrinker);

	return c;

bad_buffer:
bad_cache:
	while (!list_empty(&c->reserved_buffers)) {
		struct dm_buffer *b = list_entry(c->reserved_buffers.next,
						 struct dm_buffer, lru_list);
		list_del(&b->lru_list);
		free_buffer(b);
	}
	dm_io_client_destroy(c->dm_io);
bad_dm_io:
	vfree(c->cache_hash);
bad_hash:
	kfree(c);
bad_client:
	return ERR_PTR(r);
}
EXPORT_SYMBOL_GPL(dm_bufio_client_create);

void dm_bufio_client_destroy(struct dm_bufio_client *c)
{
	unsigned i;

	drop_buffers(c);

	unregister_shrinker(&c->shrinker);

	mutex_lock(&dm_bufio_clients_lock);

	list_del(&c->client_list);
	dm_bufio_client_count--;
	__cache_size_refresh();

	mutex_unlock(&dm_bufio_clients_lock);

	for (i = 0; i < 1 << DM_BUFIO_HASH_BITS; i++)
		BUG_ON(!hlist_empty(&c->cache_hash[i]));

	BUG_ON(c->need_reserved_buffers);

	while (!list_empty(&c->reserved_buffers)) {
		struct dm_buffer *b = list_entry(c->reserved_buffers.next,
						 struct dm_buffer, lru_list);
		list_del(&b->lru_list);
		free_buffer(b);
	}

	for (i = 0; i < LIST_SIZE; i++)
		if (c->n_buffers[i])
			DMERR("leaked buffer count %d: %ld", i, c->n_buffers[i]);

	for (i = 0; i < LIST_SIZE; i++)
		BUG_ON(c->n_buffers[i]);

	dm_io_client_destroy(c->dm_io);
	vfree(c->cache_hash);
	kfree(c);
}
EXPORT_SYMBOL_GPL(dm_bufio_client_destroy);

static void cleanup_old_buffers(void)
{
	unsigned long max_age = dm_bufio_max_age;
	struct dm_bufio_client *c;

	barrier();

	if (max_age > ULONG_MAX / HZ)
		max_age = ULONG_MAX / HZ;

	mutex_lock(&dm_bufio_clients_lock);
	list_for_each_entry(c, &dm_bufio_all_clients, client_list) {
		if (!dm_bufio_trylock(c))
			continue;

		while (!list_empty(&c->lru[LIST_CLEAN])) {
			struct dm_buffer *b;
			b = list_entry(c->lru[LIST_CLEAN].prev,
				       struct dm_buffer, lru_list);
			if (__cleanup_old_buffer(b, 0, max_age * HZ))
				break;
			dm_bufio_cond_resched();
		}

		dm_bufio_unlock(c);
		dm_bufio_cond_resched();
	}
	mutex_unlock(&dm_bufio_clients_lock);
}

static struct workqueue_struct *dm_bufio_wq;
static struct delayed_work dm_bufio_work;

static void work_fn(struct work_struct *w)
{
	cleanup_old_buffers();

	queue_delayed_work(dm_bufio_wq, &dm_bufio_work,
			   DM_BUFIO_WORK_TIMER_SECS * HZ);
}


static int __init dm_bufio_init(void)
{
	__u64 mem;

	memset(&dm_bufio_caches, 0, sizeof dm_bufio_caches);
	memset(&dm_bufio_cache_names, 0, sizeof dm_bufio_cache_names);

	mem = (__u64)((totalram_pages - totalhigh_pages) *
		      DM_BUFIO_MEMORY_PERCENT / 100) << PAGE_SHIFT;

	if (mem > ULONG_MAX)
		mem = ULONG_MAX;

#ifdef CONFIG_MMU
	if (mem > (VMALLOC_END - VMALLOC_START) * DM_BUFIO_VMALLOC_PERCENT / 100)
		mem = (VMALLOC_END - VMALLOC_START) * DM_BUFIO_VMALLOC_PERCENT / 100;
#endif

	dm_bufio_default_cache_size = mem;

	mutex_lock(&dm_bufio_clients_lock);
	__cache_size_refresh();
	mutex_unlock(&dm_bufio_clients_lock);

	dm_bufio_wq = create_singlethread_workqueue("dm_bufio_cache");
	if (!dm_bufio_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&dm_bufio_work, work_fn);
	queue_delayed_work(dm_bufio_wq, &dm_bufio_work,
			   DM_BUFIO_WORK_TIMER_SECS * HZ);

	return 0;
}

static void __exit dm_bufio_exit(void)
{
	int bug = 0;
	int i;

	cancel_delayed_work_sync(&dm_bufio_work);
	destroy_workqueue(dm_bufio_wq);

	for (i = 0; i < ARRAY_SIZE(dm_bufio_caches); i++) {
		struct kmem_cache *kc = dm_bufio_caches[i];

		if (kc)
			kmem_cache_destroy(kc);
	}

	for (i = 0; i < ARRAY_SIZE(dm_bufio_cache_names); i++)
		kfree(dm_bufio_cache_names[i]);

	if (dm_bufio_client_count) {
		DMCRIT("%s: dm_bufio_client_count leaked: %d",
			__func__, dm_bufio_client_count);
		bug = 1;
	}

	if (dm_bufio_current_allocated) {
		DMCRIT("%s: dm_bufio_current_allocated leaked: %lu",
			__func__, dm_bufio_current_allocated);
		bug = 1;
	}

	if (dm_bufio_allocated_get_free_pages) {
		DMCRIT("%s: dm_bufio_allocated_get_free_pages leaked: %lu",
		       __func__, dm_bufio_allocated_get_free_pages);
		bug = 1;
	}

	if (dm_bufio_allocated_vmalloc) {
		DMCRIT("%s: dm_bufio_vmalloc leaked: %lu",
		       __func__, dm_bufio_allocated_vmalloc);
		bug = 1;
	}

	if (bug)
		BUG();
}

module_init(dm_bufio_init)
module_exit(dm_bufio_exit)

module_param_named(max_cache_size_bytes, dm_bufio_cache_size, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_cache_size_bytes, "Size of metadata cache");

module_param_named(max_age_seconds, dm_bufio_max_age, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_age_seconds, "Max age of a buffer in seconds");

module_param_named(peak_allocated_bytes, dm_bufio_peak_allocated, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(peak_allocated_bytes, "Tracks the maximum allocated memory");

module_param_named(allocated_kmem_cache_bytes, dm_bufio_allocated_kmem_cache, ulong, S_IRUGO);
MODULE_PARM_DESC(allocated_kmem_cache_bytes, "Memory allocated with kmem_cache_alloc");

module_param_named(allocated_get_free_pages_bytes, dm_bufio_allocated_get_free_pages, ulong, S_IRUGO);
MODULE_PARM_DESC(allocated_get_free_pages_bytes, "Memory allocated with get_free_pages");

module_param_named(allocated_vmalloc_bytes, dm_bufio_allocated_vmalloc, ulong, S_IRUGO);
MODULE_PARM_DESC(allocated_vmalloc_bytes, "Memory allocated with vmalloc");

module_param_named(current_allocated_bytes, dm_bufio_current_allocated, ulong, S_IRUGO);
MODULE_PARM_DESC(current_allocated_bytes, "Memory currently used by the cache");

MODULE_AUTHOR("Mikulas Patocka <dm-devel@redhat.com>");
MODULE_DESCRIPTION(DM_NAME " buffered I/O library");
MODULE_LICENSE("GPL");
