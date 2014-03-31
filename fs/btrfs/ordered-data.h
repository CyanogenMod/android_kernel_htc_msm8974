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

#ifndef __BTRFS_ORDERED_DATA__
#define __BTRFS_ORDERED_DATA__

struct btrfs_ordered_inode_tree {
	spinlock_t lock;
	struct rb_root tree;
	struct rb_node *last;
};

struct btrfs_sector_sum {
	
	u64 bytenr;
	u32 sum;
};

struct btrfs_ordered_sum {
	
	u64 bytenr;

	unsigned long len;
	struct list_head list;
	
	struct btrfs_sector_sum sums[];
};

/*
 * bits for the flags field:
 *
 * BTRFS_ORDERED_IO_DONE is set when all of the blocks are written.
 * It is used to make sure metadata is inserted into the tree only once
 * per extent.
 *
 * BTRFS_ORDERED_COMPLETE is set when the extent is removed from the
 * rbtree, just before waking any waiters.  It is used to indicate the
 * IO is done and any metadata is inserted into the tree.
 */
#define BTRFS_ORDERED_IO_DONE 0 /* set when all the pages are written */

#define BTRFS_ORDERED_COMPLETE 1 

#define BTRFS_ORDERED_NOCOW 2 

#define BTRFS_ORDERED_COMPRESSED 3 

#define BTRFS_ORDERED_PREALLOC 4 

#define BTRFS_ORDERED_DIRECT 5 

struct btrfs_ordered_extent {
	
	u64 file_offset;

	
	u64 start;

	
	u64 len;

	
	u64 disk_len;

	
	u64 bytes_left;

	
	unsigned long flags;

	
	int compress_type;

	
	atomic_t refs;

	
	struct inode *inode;

	
	struct list_head list;

	
	wait_queue_head_t wait;

	
	struct rb_node rb_node;

	
	struct list_head root_extent_list;
};


static inline int btrfs_ordered_sum_size(struct btrfs_root *root,
					 unsigned long bytes)
{
	unsigned long num_sectors = (bytes + root->sectorsize - 1) /
		root->sectorsize;
	num_sectors++;
	return sizeof(struct btrfs_ordered_sum) +
		num_sectors * sizeof(struct btrfs_sector_sum);
}

static inline void
btrfs_ordered_inode_tree_init(struct btrfs_ordered_inode_tree *t)
{
	spin_lock_init(&t->lock);
	t->tree = RB_ROOT;
	t->last = NULL;
}

void btrfs_put_ordered_extent(struct btrfs_ordered_extent *entry);
void btrfs_remove_ordered_extent(struct inode *inode,
				struct btrfs_ordered_extent *entry);
int btrfs_dec_test_ordered_pending(struct inode *inode,
				   struct btrfs_ordered_extent **cached,
				   u64 file_offset, u64 io_size);
int btrfs_dec_test_first_ordered_pending(struct inode *inode,
				   struct btrfs_ordered_extent **cached,
				   u64 *file_offset, u64 io_size);
int btrfs_add_ordered_extent(struct inode *inode, u64 file_offset,
			     u64 start, u64 len, u64 disk_len, int type);
int btrfs_add_ordered_extent_dio(struct inode *inode, u64 file_offset,
				 u64 start, u64 len, u64 disk_len, int type);
int btrfs_add_ordered_extent_compress(struct inode *inode, u64 file_offset,
				      u64 start, u64 len, u64 disk_len,
				      int type, int compress_type);
void btrfs_add_ordered_sum(struct inode *inode,
			   struct btrfs_ordered_extent *entry,
			   struct btrfs_ordered_sum *sum);
struct btrfs_ordered_extent *btrfs_lookup_ordered_extent(struct inode *inode,
							 u64 file_offset);
void btrfs_start_ordered_extent(struct inode *inode,
				struct btrfs_ordered_extent *entry, int wait);
void btrfs_wait_ordered_range(struct inode *inode, u64 start, u64 len);
struct btrfs_ordered_extent *
btrfs_lookup_first_ordered_extent(struct inode * inode, u64 file_offset);
struct btrfs_ordered_extent *btrfs_lookup_ordered_range(struct inode *inode,
							u64 file_offset,
							u64 len);
int btrfs_ordered_update_i_size(struct inode *inode, u64 offset,
				struct btrfs_ordered_extent *ordered);
int btrfs_find_ordered_sum(struct inode *inode, u64 offset, u64 disk_bytenr, u32 *sum);
void btrfs_run_ordered_operations(struct btrfs_root *root, int wait);
void btrfs_add_ordered_operation(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct inode *inode);
void btrfs_wait_ordered_extents(struct btrfs_root *root,
				int nocow_only, int delay_iput);
#endif
