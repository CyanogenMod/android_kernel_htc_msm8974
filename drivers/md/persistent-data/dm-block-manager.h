/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This file is released under the GPL.
 */

#ifndef _LINUX_DM_BLOCK_MANAGER_H
#define _LINUX_DM_BLOCK_MANAGER_H

#include <linux/types.h>
#include <linux/blkdev.h>


typedef uint64_t dm_block_t;
struct dm_block;

dm_block_t dm_block_location(struct dm_block *b);
void *dm_block_data(struct dm_block *b);


struct dm_block_manager;
struct dm_block_manager *dm_block_manager_create(
	struct block_device *bdev, unsigned block_size,
	unsigned cache_size, unsigned max_held_per_thread);
void dm_block_manager_destroy(struct dm_block_manager *bm);

unsigned dm_bm_block_size(struct dm_block_manager *bm);
dm_block_t dm_bm_nr_blocks(struct dm_block_manager *bm);


struct dm_block_validator {
	const char *name;
	void (*prepare_for_write)(struct dm_block_validator *v, struct dm_block *b, size_t block_size);

	int (*check)(struct dm_block_validator *v, struct dm_block *b, size_t block_size);
};



/*
 * dm_bm_lock() locks a block and returns through @result a pointer to
 * memory that holds a copy of that block.  If you have write-locked the
 * block then any changes you make to memory pointed to by @result will be
 * written back to the disk sometime after dm_bm_unlock is called.
 */
int dm_bm_read_lock(struct dm_block_manager *bm, dm_block_t b,
		    struct dm_block_validator *v,
		    struct dm_block **result);

int dm_bm_write_lock(struct dm_block_manager *bm, dm_block_t b,
		     struct dm_block_validator *v,
		     struct dm_block **result);

int dm_bm_read_try_lock(struct dm_block_manager *bm, dm_block_t b,
			struct dm_block_validator *v,
			struct dm_block **result);

int dm_bm_write_lock_zero(struct dm_block_manager *bm, dm_block_t b,
			  struct dm_block_validator *v,
			  struct dm_block **result);

int dm_bm_unlock(struct dm_block *b);

int dm_bm_unlock_move(struct dm_block *b, dm_block_t n);

/*
 * It's a common idiom to have a superblock that should be committed last.
 *
 * @superblock should be write-locked on entry. It will be unlocked during
 * this function.  All dirty blocks are guaranteed to be written and flushed
 * before the superblock.
 *
 * This method always blocks.
 */
int dm_bm_flush_and_unlock(struct dm_block_manager *bm,
			   struct dm_block *superblock);

u32 dm_bm_checksum(const void *data, size_t len, u32 init_xor);


#endif	
