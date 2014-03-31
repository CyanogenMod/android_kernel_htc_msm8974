/*
 * Copyright (C) 2001-2002 Sistina Software (UK) Limited.
 * Copyright (C) 2006-2008 Red Hat GmbH
 *
 * This file is released under the GPL.
 */

#include "dm-exception-store.h"

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/dm-io.h>

#define DM_MSG_PREFIX "persistent snapshot"
#define DM_CHUNK_SIZE_DEFAULT_SECTORS 32	



#define SNAP_MAGIC 0x70416e53

#define SNAPSHOT_DISK_VERSION 1

#define NUM_SNAPSHOT_HDR_CHUNKS 1

struct disk_header {
	__le32 magic;

	__le32 valid;

	__le32 version;

	
	__le32 chunk_size;
} __packed;

struct disk_exception {
	__le64 old_chunk;
	__le64 new_chunk;
} __packed;

struct core_exception {
	uint64_t old_chunk;
	uint64_t new_chunk;
};

struct commit_callback {
	void (*callback)(void *, int success);
	void *context;
};

struct pstore {
	struct dm_exception_store *store;
	int version;
	int valid;
	uint32_t exceptions_per_area;

	void *area;

	void *zero_area;

	/*
	 * An area used for header. The header can be written
	 * concurrently with metadata (when invalidating the snapshot),
	 * so it needs a separate buffer.
	 */
	void *header_area;

	chunk_t current_area;


	chunk_t next_free;

	uint32_t current_committed;

	atomic_t pending_count;
	uint32_t callback_count;
	struct commit_callback *callbacks;
	struct dm_io_client *io_client;

	struct workqueue_struct *metadata_wq;
};

static int alloc_area(struct pstore *ps)
{
	int r = -ENOMEM;
	size_t len;

	len = ps->store->chunk_size << SECTOR_SHIFT;

	ps->area = vmalloc(len);
	if (!ps->area)
		goto err_area;

	ps->zero_area = vzalloc(len);
	if (!ps->zero_area)
		goto err_zero_area;

	ps->header_area = vmalloc(len);
	if (!ps->header_area)
		goto err_header_area;

	return 0;

err_header_area:
	vfree(ps->zero_area);

err_zero_area:
	vfree(ps->area);

err_area:
	return r;
}

static void free_area(struct pstore *ps)
{
	if (ps->area)
		vfree(ps->area);
	ps->area = NULL;

	if (ps->zero_area)
		vfree(ps->zero_area);
	ps->zero_area = NULL;

	if (ps->header_area)
		vfree(ps->header_area);
	ps->header_area = NULL;
}

struct mdata_req {
	struct dm_io_region *where;
	struct dm_io_request *io_req;
	struct work_struct work;
	int result;
};

static void do_metadata(struct work_struct *work)
{
	struct mdata_req *req = container_of(work, struct mdata_req, work);

	req->result = dm_io(req->io_req, 1, req->where, NULL);
}

static int chunk_io(struct pstore *ps, void *area, chunk_t chunk, int rw,
		    int metadata)
{
	struct dm_io_region where = {
		.bdev = dm_snap_cow(ps->store->snap)->bdev,
		.sector = ps->store->chunk_size * chunk,
		.count = ps->store->chunk_size,
	};
	struct dm_io_request io_req = {
		.bi_rw = rw,
		.mem.type = DM_IO_VMA,
		.mem.ptr.vma = area,
		.client = ps->io_client,
		.notify.fn = NULL,
	};
	struct mdata_req req;

	if (!metadata)
		return dm_io(&io_req, 1, &where, NULL);

	req.where = &where;
	req.io_req = &io_req;

	INIT_WORK_ONSTACK(&req.work, do_metadata);
	queue_work(ps->metadata_wq, &req.work);
	flush_work(&req.work);

	return req.result;
}

static chunk_t area_location(struct pstore *ps, chunk_t area)
{
	return NUM_SNAPSHOT_HDR_CHUNKS + ((ps->exceptions_per_area + 1) * area);
}

static int area_io(struct pstore *ps, int rw)
{
	int r;
	chunk_t chunk;

	chunk = area_location(ps, ps->current_area);

	r = chunk_io(ps, ps->area, chunk, rw, 0);
	if (r)
		return r;

	return 0;
}

static void zero_memory_area(struct pstore *ps)
{
	memset(ps->area, 0, ps->store->chunk_size << SECTOR_SHIFT);
}

static int zero_disk_area(struct pstore *ps, chunk_t area)
{
	return chunk_io(ps, ps->zero_area, area_location(ps, area), WRITE, 0);
}

static int read_header(struct pstore *ps, int *new_snapshot)
{
	int r;
	struct disk_header *dh;
	unsigned chunk_size;
	int chunk_size_supplied = 1;
	char *chunk_err;

	if (!ps->store->chunk_size) {
		ps->store->chunk_size = max(DM_CHUNK_SIZE_DEFAULT_SECTORS,
		    bdev_logical_block_size(dm_snap_cow(ps->store->snap)->
					    bdev) >> 9);
		ps->store->chunk_mask = ps->store->chunk_size - 1;
		ps->store->chunk_shift = ffs(ps->store->chunk_size) - 1;
		chunk_size_supplied = 0;
	}

	ps->io_client = dm_io_client_create();
	if (IS_ERR(ps->io_client))
		return PTR_ERR(ps->io_client);

	r = alloc_area(ps);
	if (r)
		return r;

	r = chunk_io(ps, ps->header_area, 0, READ, 1);
	if (r)
		goto bad;

	dh = ps->header_area;

	if (le32_to_cpu(dh->magic) == 0) {
		*new_snapshot = 1;
		return 0;
	}

	if (le32_to_cpu(dh->magic) != SNAP_MAGIC) {
		DMWARN("Invalid or corrupt snapshot");
		r = -ENXIO;
		goto bad;
	}

	*new_snapshot = 0;
	ps->valid = le32_to_cpu(dh->valid);
	ps->version = le32_to_cpu(dh->version);
	chunk_size = le32_to_cpu(dh->chunk_size);

	if (ps->store->chunk_size == chunk_size)
		return 0;

	if (chunk_size_supplied)
		DMWARN("chunk size %u in device metadata overrides "
		       "table chunk size of %u.",
		       chunk_size, ps->store->chunk_size);

	
	free_area(ps);

	r = dm_exception_store_set_chunk_size(ps->store, chunk_size,
					      &chunk_err);
	if (r) {
		DMERR("invalid on-disk chunk size %u: %s.",
		      chunk_size, chunk_err);
		return r;
	}

	r = alloc_area(ps);
	return r;

bad:
	free_area(ps);
	return r;
}

static int write_header(struct pstore *ps)
{
	struct disk_header *dh;

	memset(ps->header_area, 0, ps->store->chunk_size << SECTOR_SHIFT);

	dh = ps->header_area;
	dh->magic = cpu_to_le32(SNAP_MAGIC);
	dh->valid = cpu_to_le32(ps->valid);
	dh->version = cpu_to_le32(ps->version);
	dh->chunk_size = cpu_to_le32(ps->store->chunk_size);

	return chunk_io(ps, ps->header_area, 0, WRITE, 1);
}

static struct disk_exception *get_exception(struct pstore *ps, uint32_t index)
{
	BUG_ON(index >= ps->exceptions_per_area);

	return ((struct disk_exception *) ps->area) + index;
}

static void read_exception(struct pstore *ps,
			   uint32_t index, struct core_exception *result)
{
	struct disk_exception *de = get_exception(ps, index);

	
	result->old_chunk = le64_to_cpu(de->old_chunk);
	result->new_chunk = le64_to_cpu(de->new_chunk);
}

static void write_exception(struct pstore *ps,
			    uint32_t index, struct core_exception *e)
{
	struct disk_exception *de = get_exception(ps, index);

	
	de->old_chunk = cpu_to_le64(e->old_chunk);
	de->new_chunk = cpu_to_le64(e->new_chunk);
}

static void clear_exception(struct pstore *ps, uint32_t index)
{
	struct disk_exception *de = get_exception(ps, index);

	
	de->old_chunk = 0;
	de->new_chunk = 0;
}

static int insert_exceptions(struct pstore *ps,
			     int (*callback)(void *callback_context,
					     chunk_t old, chunk_t new),
			     void *callback_context,
			     int *full)
{
	int r;
	unsigned int i;
	struct core_exception e;

	
	*full = 1;

	for (i = 0; i < ps->exceptions_per_area; i++) {
		read_exception(ps, i, &e);

		if (e.new_chunk == 0LL) {
			ps->current_committed = i;
			*full = 0;
			break;
		}

		if (ps->next_free <= e.new_chunk)
			ps->next_free = e.new_chunk + 1;

		r = callback(callback_context, e.old_chunk, e.new_chunk);
		if (r)
			return r;
	}

	return 0;
}

static int read_exceptions(struct pstore *ps,
			   int (*callback)(void *callback_context, chunk_t old,
					   chunk_t new),
			   void *callback_context)
{
	int r, full = 1;

	for (ps->current_area = 0; full; ps->current_area++) {
		r = area_io(ps, READ);
		if (r)
			return r;

		r = insert_exceptions(ps, callback, callback_context, &full);
		if (r)
			return r;
	}

	ps->current_area--;

	return 0;
}

static struct pstore *get_info(struct dm_exception_store *store)
{
	return (struct pstore *) store->context;
}

static void persistent_usage(struct dm_exception_store *store,
			     sector_t *total_sectors,
			     sector_t *sectors_allocated,
			     sector_t *metadata_sectors)
{
	struct pstore *ps = get_info(store);

	*sectors_allocated = ps->next_free * store->chunk_size;
	*total_sectors = get_dev_size(dm_snap_cow(store->snap)->bdev);

	*metadata_sectors = (ps->current_area + 1 + NUM_SNAPSHOT_HDR_CHUNKS) *
			    store->chunk_size;
}

static void persistent_dtr(struct dm_exception_store *store)
{
	struct pstore *ps = get_info(store);

	destroy_workqueue(ps->metadata_wq);

	
	if (ps->io_client)
		dm_io_client_destroy(ps->io_client);
	free_area(ps);

	
	if (ps->callbacks)
		vfree(ps->callbacks);

	kfree(ps);
}

static int persistent_read_metadata(struct dm_exception_store *store,
				    int (*callback)(void *callback_context,
						    chunk_t old, chunk_t new),
				    void *callback_context)
{
	int r, uninitialized_var(new_snapshot);
	struct pstore *ps = get_info(store);

	r = read_header(ps, &new_snapshot);
	if (r)
		return r;

	ps->exceptions_per_area = (ps->store->chunk_size << SECTOR_SHIFT) /
				  sizeof(struct disk_exception);
	ps->callbacks = dm_vcalloc(ps->exceptions_per_area,
				   sizeof(*ps->callbacks));
	if (!ps->callbacks)
		return -ENOMEM;

	if (new_snapshot) {
		r = write_header(ps);
		if (r) {
			DMWARN("write_header failed");
			return r;
		}

		ps->current_area = 0;
		zero_memory_area(ps);
		r = zero_disk_area(ps, 0);
		if (r)
			DMWARN("zero_disk_area(0) failed");
		return r;
	}
	if (ps->version != SNAPSHOT_DISK_VERSION) {
		DMWARN("unable to handle snapshot disk version %d",
		       ps->version);
		return -EINVAL;
	}

	if (!ps->valid)
		return 1;

	r = read_exceptions(ps, callback, callback_context);

	return r;
}

static int persistent_prepare_exception(struct dm_exception_store *store,
					struct dm_exception *e)
{
	struct pstore *ps = get_info(store);
	uint32_t stride;
	chunk_t next_free;
	sector_t size = get_dev_size(dm_snap_cow(store->snap)->bdev);

	
	if (size < ((ps->next_free + 1) * store->chunk_size))
		return -ENOSPC;

	e->new_chunk = ps->next_free;

	stride = (ps->exceptions_per_area + 1);
	next_free = ++ps->next_free;
	if (sector_div(next_free, stride) == 1)
		ps->next_free++;

	atomic_inc(&ps->pending_count);
	return 0;
}

static void persistent_commit_exception(struct dm_exception_store *store,
					struct dm_exception *e,
					void (*callback) (void *, int success),
					void *callback_context)
{
	unsigned int i;
	struct pstore *ps = get_info(store);
	struct core_exception ce;
	struct commit_callback *cb;

	ce.old_chunk = e->old_chunk;
	ce.new_chunk = e->new_chunk;
	write_exception(ps, ps->current_committed++, &ce);

	cb = ps->callbacks + ps->callback_count++;
	cb->callback = callback;
	cb->context = callback_context;

	if (!atomic_dec_and_test(&ps->pending_count) &&
	    (ps->current_committed != ps->exceptions_per_area))
		return;

	if ((ps->current_committed == ps->exceptions_per_area) &&
	    zero_disk_area(ps, ps->current_area + 1))
		ps->valid = 0;

	if (ps->valid && area_io(ps, WRITE_FLUSH_FUA))
		ps->valid = 0;

	if (ps->current_committed == ps->exceptions_per_area) {
		ps->current_committed = 0;
		ps->current_area++;
		zero_memory_area(ps);
	}

	for (i = 0; i < ps->callback_count; i++) {
		cb = ps->callbacks + i;
		cb->callback(cb->context, ps->valid);
	}

	ps->callback_count = 0;
}

static int persistent_prepare_merge(struct dm_exception_store *store,
				    chunk_t *last_old_chunk,
				    chunk_t *last_new_chunk)
{
	struct pstore *ps = get_info(store);
	struct core_exception ce;
	int nr_consecutive;
	int r;

	if (!ps->current_committed) {
		if (!ps->current_area)
			return 0;

		ps->current_area--;
		r = area_io(ps, READ);
		if (r < 0)
			return r;
		ps->current_committed = ps->exceptions_per_area;
	}

	read_exception(ps, ps->current_committed - 1, &ce);
	*last_old_chunk = ce.old_chunk;
	*last_new_chunk = ce.new_chunk;

	for (nr_consecutive = 1; nr_consecutive < ps->current_committed;
	     nr_consecutive++) {
		read_exception(ps, ps->current_committed - 1 - nr_consecutive,
			       &ce);
		if (ce.old_chunk != *last_old_chunk - nr_consecutive ||
		    ce.new_chunk != *last_new_chunk - nr_consecutive)
			break;
	}

	return nr_consecutive;
}

static int persistent_commit_merge(struct dm_exception_store *store,
				   int nr_merged)
{
	int r, i;
	struct pstore *ps = get_info(store);

	BUG_ON(nr_merged > ps->current_committed);

	for (i = 0; i < nr_merged; i++)
		clear_exception(ps, ps->current_committed - 1 - i);

	r = area_io(ps, WRITE_FLUSH_FUA);
	if (r < 0)
		return r;

	ps->current_committed -= nr_merged;

	ps->next_free = area_location(ps, ps->current_area) +
			ps->current_committed + 1;

	return 0;
}

static void persistent_drop_snapshot(struct dm_exception_store *store)
{
	struct pstore *ps = get_info(store);

	ps->valid = 0;
	if (write_header(ps))
		DMWARN("write header failed");
}

static int persistent_ctr(struct dm_exception_store *store,
			  unsigned argc, char **argv)
{
	struct pstore *ps;

	
	ps = kzalloc(sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;

	ps->store = store;
	ps->valid = 1;
	ps->version = SNAPSHOT_DISK_VERSION;
	ps->area = NULL;
	ps->zero_area = NULL;
	ps->header_area = NULL;
	ps->next_free = NUM_SNAPSHOT_HDR_CHUNKS + 1; 
	ps->current_committed = 0;

	ps->callback_count = 0;
	atomic_set(&ps->pending_count, 0);
	ps->callbacks = NULL;

	ps->metadata_wq = alloc_workqueue("ksnaphd", WQ_MEM_RECLAIM, 0);
	if (!ps->metadata_wq) {
		kfree(ps);
		DMERR("couldn't start header metadata update thread");
		return -ENOMEM;
	}

	store->context = ps;

	return 0;
}

static unsigned persistent_status(struct dm_exception_store *store,
				  status_type_t status, char *result,
				  unsigned maxlen)
{
	unsigned sz = 0;

	switch (status) {
	case STATUSTYPE_INFO:
		break;
	case STATUSTYPE_TABLE:
		DMEMIT(" P %llu", (unsigned long long)store->chunk_size);
	}

	return sz;
}

static struct dm_exception_store_type _persistent_type = {
	.name = "persistent",
	.module = THIS_MODULE,
	.ctr = persistent_ctr,
	.dtr = persistent_dtr,
	.read_metadata = persistent_read_metadata,
	.prepare_exception = persistent_prepare_exception,
	.commit_exception = persistent_commit_exception,
	.prepare_merge = persistent_prepare_merge,
	.commit_merge = persistent_commit_merge,
	.drop_snapshot = persistent_drop_snapshot,
	.usage = persistent_usage,
	.status = persistent_status,
};

static struct dm_exception_store_type _persistent_compat_type = {
	.name = "P",
	.module = THIS_MODULE,
	.ctr = persistent_ctr,
	.dtr = persistent_dtr,
	.read_metadata = persistent_read_metadata,
	.prepare_exception = persistent_prepare_exception,
	.commit_exception = persistent_commit_exception,
	.prepare_merge = persistent_prepare_merge,
	.commit_merge = persistent_commit_merge,
	.drop_snapshot = persistent_drop_snapshot,
	.usage = persistent_usage,
	.status = persistent_status,
};

int dm_persistent_snapshot_init(void)
{
	int r;

	r = dm_exception_store_type_register(&_persistent_type);
	if (r) {
		DMERR("Unable to register persistent exception store type");
		return r;
	}

	r = dm_exception_store_type_register(&_persistent_compat_type);
	if (r) {
		DMERR("Unable to register old-style persistent exception "
		      "store type");
		dm_exception_store_type_unregister(&_persistent_type);
		return r;
	}

	return r;
}

void dm_persistent_snapshot_exit(void)
{
	dm_exception_store_type_unregister(&_persistent_type);
	dm_exception_store_type_unregister(&_persistent_compat_type);
}
