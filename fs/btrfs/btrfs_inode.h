/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef __BTRFS_I__
#define __BTRFS_I__

#include "extent_map.h"
#include "extent_io.h"
#include "ordered-data.h"
#include "delayed-inode.h"

struct btrfs_inode {
	
	struct btrfs_root *root;

	struct btrfs_key location;

	
	spinlock_t lock;

	
	struct extent_map_tree extent_tree;

	
	struct extent_io_tree io_tree;

	struct extent_io_tree io_failure_tree;

	
	struct mutex log_mutex;

	
	struct mutex delalloc_mutex;

	
	struct btrfs_ordered_inode_tree ordered_tree;

	
	struct list_head i_orphan;

	struct list_head delalloc_inodes;

	struct list_head ordered_operations;

	
	struct rb_node rb_node;

	
	struct btrfs_space_info *space_info;

	u64 generation;

	
	u64 sequence;

	u64 last_trans;

	u64 last_sub_trans;

	u64 logged_trans;

	u64 delalloc_bytes;

	/*
	 * the size of the file stored in the metadata on disk.  data=ordered
	 * means the in-memory i_size might be larger than the size on disk
	 * because not all the blocks are written yet.
	 */
	u64 disk_i_size;

	u64 index_cnt;

	u64 last_unlink_trans;

	u64 csum_bytes;

	
	u32 flags;

	unsigned outstanding_extents;
	unsigned reserved_extents;

	/*
	 * ordered_data_close is set by truncate when a file that used
	 * to have good data has been truncated to zero.  When it is set
	 * the btrfs file release call will add this inode to the
	 * ordered operations list so that we make sure to flush out any
	 * new data the application may have written before commit.
	 */
	unsigned ordered_data_close:1;
	unsigned orphan_meta_reserved:1;
	unsigned dummy_inode:1;
	unsigned in_defrag:1;
	unsigned delalloc_meta_reserved:1;

	unsigned force_compress:4;

	struct btrfs_delayed_node *delayed_node;

	struct inode vfs_inode;
};

extern unsigned char btrfs_filetype_table[];

static inline struct btrfs_inode *BTRFS_I(struct inode *inode)
{
	return container_of(inode, struct btrfs_inode, vfs_inode);
}

static inline u64 btrfs_ino(struct inode *inode)
{
	u64 ino = BTRFS_I(inode)->location.objectid;

	if (!ino || BTRFS_I(inode)->location.type == BTRFS_ROOT_ITEM_KEY)
		ino = inode->i_ino;
	return ino;
}

static inline void btrfs_i_size_write(struct inode *inode, u64 size)
{
	i_size_write(inode, size);
	BTRFS_I(inode)->disk_i_size = size;
}

static inline bool btrfs_is_free_space_inode(struct btrfs_root *root,
				       struct inode *inode)
{
	if (root == root->fs_info->tree_root ||
	    BTRFS_I(inode)->location.objectid == BTRFS_FREE_INO_OBJECTID)
		return true;
	return false;
}

#endif
