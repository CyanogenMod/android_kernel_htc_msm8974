/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This file is released under the GPL.
 */
#ifndef _LINUX_DM_BTREE_H
#define _LINUX_DM_BTREE_H

#include "dm-block-manager.h"

struct dm_transaction_manager;


#ifdef __CHECKER__
#  define __dm_written_to_disk(x) __releases(x)
#  define __dm_reads_from_disk(x) __acquires(x)
#  define __dm_bless_for_disk(x) __acquire(x)
#  define __dm_unbless_for_disk(x) __release(x)
#else
#  define __dm_written_to_disk(x)
#  define __dm_reads_from_disk(x)
#  define __dm_bless_for_disk(x)
#  define __dm_unbless_for_disk(x)
#endif



struct dm_btree_value_type {
	void *context;

	uint32_t size;


	void (*inc)(void *context, void *value);

	void (*dec)(void *context, void *value);

	/*
	 * A test for equality between two values.  When a value is
	 * overwritten with a new one, the old one has the dec method
	 * called _unless_ the new and old value are deemed equal.
	 */
	int (*equal)(void *context, void *value1, void *value2);
};

struct dm_btree_info {
	struct dm_transaction_manager *tm;

	unsigned levels;
	struct dm_btree_value_type value_type;
};

int dm_btree_empty(struct dm_btree_info *info, dm_block_t *root);

int dm_btree_del(struct dm_btree_info *info, dm_block_t root);


int dm_btree_lookup(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, void *value_le);

int dm_btree_insert(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, void *value, dm_block_t *new_root)
		    __dm_written_to_disk(value);

int dm_btree_insert_notify(struct dm_btree_info *info, dm_block_t root,
			   uint64_t *keys, void *value, dm_block_t *new_root,
			   int *inserted)
			   __dm_written_to_disk(value);

int dm_btree_remove(struct dm_btree_info *info, dm_block_t root,
		    uint64_t *keys, dm_block_t *new_root);

int dm_btree_find_highest_key(struct dm_btree_info *info, dm_block_t root,
			      uint64_t *result_keys);

#endif	
