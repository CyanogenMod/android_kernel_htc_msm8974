/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This file is released under the GPL.
 */

#ifndef DM_SPACE_MAP_METADATA_H
#define DM_SPACE_MAP_METADATA_H

#include "dm-transaction-manager.h"

struct dm_space_map *dm_sm_metadata_init(void);

int dm_sm_metadata_create(struct dm_space_map *sm,
			  struct dm_transaction_manager *tm,
			  dm_block_t nr_blocks,
			  dm_block_t superblock);

int dm_sm_metadata_open(struct dm_space_map *sm,
			struct dm_transaction_manager *tm,
			void *root_le, size_t len);

#endif	
