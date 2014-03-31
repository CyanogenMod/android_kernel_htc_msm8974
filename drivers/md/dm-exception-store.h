/*
 * Copyright (C) 2001-2002 Sistina Software (UK) Limited.
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 *
 * Device-mapper snapshot exception store.
 *
 * This file is released under the GPL.
 */

#ifndef _LINUX_DM_EXCEPTION_STORE
#define _LINUX_DM_EXCEPTION_STORE

#include <linux/blkdev.h>
#include <linux/device-mapper.h>

typedef sector_t chunk_t;

struct dm_exception {
	struct list_head hash_list;

	chunk_t old_chunk;
	chunk_t new_chunk;
};

struct dm_exception_store;
struct dm_exception_store_type {
	const char *name;
	struct module *module;

	int (*ctr) (struct dm_exception_store *store,
		    unsigned argc, char **argv);

	void (*dtr) (struct dm_exception_store *store);

	int (*read_metadata) (struct dm_exception_store *store,
			      int (*callback)(void *callback_context,
					      chunk_t old, chunk_t new),
			      void *callback_context);

	int (*prepare_exception) (struct dm_exception_store *store,
				  struct dm_exception *e);

	void (*commit_exception) (struct dm_exception_store *store,
				  struct dm_exception *e,
				  void (*callback) (void *, int success),
				  void *callback_context);

	int (*prepare_merge) (struct dm_exception_store *store,
			      chunk_t *last_old_chunk, chunk_t *last_new_chunk);

	int (*commit_merge) (struct dm_exception_store *store, int nr_merged);

	void (*drop_snapshot) (struct dm_exception_store *store);

	unsigned (*status) (struct dm_exception_store *store,
			    status_type_t status, char *result,
			    unsigned maxlen);

	void (*usage) (struct dm_exception_store *store,
		       sector_t *total_sectors, sector_t *sectors_allocated,
		       sector_t *metadata_sectors);

	
	struct list_head list;
};

struct dm_snapshot;

struct dm_exception_store {
	struct dm_exception_store_type *type;
	struct dm_snapshot *snap;

	
	unsigned chunk_size;
	unsigned chunk_mask;
	unsigned chunk_shift;

	void *context;
};

struct dm_dev *dm_snap_origin(struct dm_snapshot *snap);
struct dm_dev *dm_snap_cow(struct dm_snapshot *snap);

#  if defined(CONFIG_LBDAF) || (BITS_PER_LONG == 64)
#    define DM_CHUNK_CONSECUTIVE_BITS 8
#    define DM_CHUNK_NUMBER_BITS 56

static inline chunk_t dm_chunk_number(chunk_t chunk)
{
	return chunk & (chunk_t)((1ULL << DM_CHUNK_NUMBER_BITS) - 1ULL);
}

static inline unsigned dm_consecutive_chunk_count(struct dm_exception *e)
{
	return e->new_chunk >> DM_CHUNK_NUMBER_BITS;
}

static inline void dm_consecutive_chunk_count_inc(struct dm_exception *e)
{
	e->new_chunk += (1ULL << DM_CHUNK_NUMBER_BITS);

	BUG_ON(!dm_consecutive_chunk_count(e));
}

static inline void dm_consecutive_chunk_count_dec(struct dm_exception *e)
{
	BUG_ON(!dm_consecutive_chunk_count(e));

	e->new_chunk -= (1ULL << DM_CHUNK_NUMBER_BITS);
}

#  else
#    define DM_CHUNK_CONSECUTIVE_BITS 0

static inline chunk_t dm_chunk_number(chunk_t chunk)
{
	return chunk;
}

static inline unsigned dm_consecutive_chunk_count(struct dm_exception *e)
{
	return 0;
}

static inline void dm_consecutive_chunk_count_inc(struct dm_exception *e)
{
}

static inline void dm_consecutive_chunk_count_dec(struct dm_exception *e)
{
}

#  endif

static inline sector_t get_dev_size(struct block_device *bdev)
{
	return i_size_read(bdev->bd_inode) >> SECTOR_SHIFT;
}

static inline chunk_t sector_to_chunk(struct dm_exception_store *store,
				      sector_t sector)
{
	return sector >> store->chunk_shift;
}

int dm_exception_store_type_register(struct dm_exception_store_type *type);
int dm_exception_store_type_unregister(struct dm_exception_store_type *type);

int dm_exception_store_set_chunk_size(struct dm_exception_store *store,
				      unsigned chunk_size,
				      char **error);

int dm_exception_store_create(struct dm_target *ti, int argc, char **argv,
			      struct dm_snapshot *snap,
			      unsigned *args_used,
			      struct dm_exception_store **store);
void dm_exception_store_destroy(struct dm_exception_store *store);

int dm_exception_store_init(void);
void dm_exception_store_exit(void);

int dm_persistent_snapshot_init(void);
void dm_persistent_snapshot_exit(void);

int dm_transient_snapshot_init(void);
void dm_transient_snapshot_exit(void);

#endif 
