/*
 * Copyright (C) 2010-2011 Red Hat, Inc.
 *
 * This file is released under the GPL.
 */

#ifndef DM_THIN_METADATA_H
#define DM_THIN_METADATA_H

#include "persistent-data/dm-block-manager.h"

#define THIN_METADATA_BLOCK_SIZE 4096

#define THIN_METADATA_MAX_SECTORS (255 * (1 << 14) * (THIN_METADATA_BLOCK_SIZE / (1 << SECTOR_SHIFT)))

#define THIN_METADATA_MAX_SECTORS_WARNING (16 * (1024 * 1024 * 1024 >> SECTOR_SHIFT))


struct dm_pool_metadata;
struct dm_thin_device;

typedef uint64_t dm_thin_id;

struct dm_pool_metadata *dm_pool_metadata_open(struct block_device *bdev,
					       sector_t data_block_size);

int dm_pool_metadata_close(struct dm_pool_metadata *pmd);

#define THIN_FEATURE_COMPAT_SUPP	  0UL
#define THIN_FEATURE_COMPAT_RO_SUPP	  0UL
#define THIN_FEATURE_INCOMPAT_SUPP	  0UL

int dm_pool_create_thin(struct dm_pool_metadata *pmd, dm_thin_id dev);

int dm_pool_create_snap(struct dm_pool_metadata *pmd, dm_thin_id dev,
			dm_thin_id origin);

int dm_pool_delete_thin_device(struct dm_pool_metadata *pmd,
			       dm_thin_id dev);

int dm_pool_commit_metadata(struct dm_pool_metadata *pmd);

int dm_pool_set_metadata_transaction_id(struct dm_pool_metadata *pmd,
					uint64_t current_id,
					uint64_t new_id);

int dm_pool_get_metadata_transaction_id(struct dm_pool_metadata *pmd,
					uint64_t *result);

int dm_pool_hold_metadata_root(struct dm_pool_metadata *pmd);

int dm_pool_get_held_metadata_root(struct dm_pool_metadata *pmd,
				   dm_block_t *result);


int dm_pool_open_thin_device(struct dm_pool_metadata *pmd, dm_thin_id dev,
			     struct dm_thin_device **td);

int dm_pool_close_thin_device(struct dm_thin_device *td);

dm_thin_id dm_thin_dev_id(struct dm_thin_device *td);

struct dm_thin_lookup_result {
	dm_block_t block;
	int shared;
};

int dm_thin_find_block(struct dm_thin_device *td, dm_block_t block,
		       int can_block, struct dm_thin_lookup_result *result);

int dm_pool_alloc_data_block(struct dm_pool_metadata *pmd, dm_block_t *result);

int dm_thin_insert_block(struct dm_thin_device *td, dm_block_t block,
			 dm_block_t data_block);

int dm_thin_remove_block(struct dm_thin_device *td, dm_block_t block);

int dm_thin_get_highest_mapped_block(struct dm_thin_device *td,
				     dm_block_t *highest_mapped);

int dm_thin_get_mapped_count(struct dm_thin_device *td, dm_block_t *result);

int dm_pool_get_free_block_count(struct dm_pool_metadata *pmd,
				 dm_block_t *result);

int dm_pool_get_free_metadata_block_count(struct dm_pool_metadata *pmd,
					  dm_block_t *result);

int dm_pool_get_metadata_dev_size(struct dm_pool_metadata *pmd,
				  dm_block_t *result);

int dm_pool_get_data_block_size(struct dm_pool_metadata *pmd, sector_t *result);

int dm_pool_get_data_dev_size(struct dm_pool_metadata *pmd, dm_block_t *result);

int dm_pool_resize_data_dev(struct dm_pool_metadata *pmd, dm_block_t new_size);


#endif
