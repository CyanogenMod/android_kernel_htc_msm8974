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

#ifndef __BTRFS_CTREE__
#define __BTRFS_CTREE__

#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/backing-dev.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <trace/events/btrfs.h>
#include <asm/kmap_types.h>
#include <linux/pagemap.h>
#include "extent_io.h"
#include "extent_map.h"
#include "async-thread.h"
#include "ioctl.h"

struct btrfs_trans_handle;
struct btrfs_transaction;
struct btrfs_pending_snapshot;
extern struct kmem_cache *btrfs_trans_handle_cachep;
extern struct kmem_cache *btrfs_transaction_cachep;
extern struct kmem_cache *btrfs_bit_radix_cachep;
extern struct kmem_cache *btrfs_path_cachep;
extern struct kmem_cache *btrfs_free_space_cachep;
struct btrfs_ordered_sum;

#define BTRFS_MAGIC "_BHRfS_M"

#define BTRFS_MAX_MIRRORS 2

#define BTRFS_MAX_LEVEL 8

#define BTRFS_COMPAT_EXTENT_TREE_V0

#define BTRFS_ORDERED_OPERATIONS_FLUSH_LIMIT (8 * 1024 * 1024)

#define BTRFS_ROOT_TREE_OBJECTID 1ULL

#define BTRFS_EXTENT_TREE_OBJECTID 2ULL

#define BTRFS_CHUNK_TREE_OBJECTID 3ULL

#define BTRFS_DEV_TREE_OBJECTID 4ULL

#define BTRFS_FS_TREE_OBJECTID 5ULL

#define BTRFS_ROOT_TREE_DIR_OBJECTID 6ULL

#define BTRFS_CSUM_TREE_OBJECTID 7ULL

#define BTRFS_BALANCE_OBJECTID -4ULL

#define BTRFS_ORPHAN_OBJECTID -5ULL

#define BTRFS_TREE_LOG_OBJECTID -6ULL
#define BTRFS_TREE_LOG_FIXUP_OBJECTID -7ULL

#define BTRFS_TREE_RELOC_OBJECTID -8ULL
#define BTRFS_DATA_RELOC_TREE_OBJECTID -9ULL

#define BTRFS_EXTENT_CSUM_OBJECTID -10ULL

#define BTRFS_FREE_SPACE_OBJECTID -11ULL

#define BTRFS_FREE_INO_OBJECTID -12ULL

#define BTRFS_MULTIPLE_OBJECTIDS -255ULL

#define BTRFS_FIRST_FREE_OBJECTID 256ULL
#define BTRFS_LAST_FREE_OBJECTID -256ULL
#define BTRFS_FIRST_CHUNK_TREE_OBJECTID 256ULL


#define BTRFS_DEV_ITEMS_OBJECTID 1ULL

#define BTRFS_BTREE_INODE_OBJECTID 1

#define BTRFS_EMPTY_SUBVOL_DIR_OBJECTID 2

#define BTRFS_MAX_METADATA_BLOCKSIZE 65536

#define BTRFS_NAME_LEN 255

#define BTRFS_CSUM_SIZE 32

#define BTRFS_CSUM_TYPE_CRC32	0

static int btrfs_csum_sizes[] = { 4, 0 };

#define BTRFS_EMPTY_DIR_SIZE 0

#define BTRFS_FT_UNKNOWN	0
#define BTRFS_FT_REG_FILE	1
#define BTRFS_FT_DIR		2
#define BTRFS_FT_CHRDEV		3
#define BTRFS_FT_BLKDEV		4
#define BTRFS_FT_FIFO		5
#define BTRFS_FT_SOCK		6
#define BTRFS_FT_SYMLINK	7
#define BTRFS_FT_XATTR		8
#define BTRFS_FT_MAX		9

struct btrfs_disk_key {
	__le64 objectid;
	u8 type;
	__le64 offset;
} __attribute__ ((__packed__));

struct btrfs_key {
	u64 objectid;
	u8 type;
	u64 offset;
} __attribute__ ((__packed__));

struct btrfs_mapping_tree {
	struct extent_map_tree map_tree;
};

struct btrfs_dev_item {
	
	__le64 devid;

	
	__le64 total_bytes;

	
	__le64 bytes_used;

	
	__le32 io_align;

	
	__le32 io_width;

	
	__le32 sector_size;

	
	__le64 type;

	
	__le64 generation;

	__le64 start_offset;

	
	__le32 dev_group;

	
	u8 seek_speed;

	
	u8 bandwidth;

	
	u8 uuid[BTRFS_UUID_SIZE];

	
	u8 fsid[BTRFS_UUID_SIZE];
} __attribute__ ((__packed__));

struct btrfs_stripe {
	__le64 devid;
	__le64 offset;
	u8 dev_uuid[BTRFS_UUID_SIZE];
} __attribute__ ((__packed__));

struct btrfs_chunk {
	
	__le64 length;

	
	__le64 owner;

	__le64 stripe_len;
	__le64 type;

	
	__le32 io_align;

	
	__le32 io_width;

	
	__le32 sector_size;

	__le16 num_stripes;

	
	__le16 sub_stripes;
	struct btrfs_stripe stripe;
	
} __attribute__ ((__packed__));

#define BTRFS_FREE_SPACE_EXTENT	1
#define BTRFS_FREE_SPACE_BITMAP	2

struct btrfs_free_space_entry {
	__le64 offset;
	__le64 bytes;
	u8 type;
} __attribute__ ((__packed__));

struct btrfs_free_space_header {
	struct btrfs_disk_key location;
	__le64 generation;
	__le64 num_entries;
	__le64 num_bitmaps;
} __attribute__ ((__packed__));

static inline unsigned long btrfs_chunk_item_size(int num_stripes)
{
	BUG_ON(num_stripes == 0);
	return sizeof(struct btrfs_chunk) +
		sizeof(struct btrfs_stripe) * (num_stripes - 1);
}

#define BTRFS_HEADER_FLAG_WRITTEN	(1ULL << 0)
#define BTRFS_HEADER_FLAG_RELOC		(1ULL << 1)


#define BTRFS_SUPER_FLAG_ERROR		(1ULL << 2)

#define BTRFS_SUPER_FLAG_SEEDING	(1ULL << 32)
#define BTRFS_SUPER_FLAG_METADUMP	(1ULL << 33)

#define BTRFS_BACKREF_REV_MAX		256
#define BTRFS_BACKREF_REV_SHIFT		56
#define BTRFS_BACKREF_REV_MASK		(((u64)BTRFS_BACKREF_REV_MAX - 1) << \
					 BTRFS_BACKREF_REV_SHIFT)

#define BTRFS_OLD_BACKREF_REV		0
#define BTRFS_MIXED_BACKREF_REV		1

struct btrfs_header {
	
	u8 csum[BTRFS_CSUM_SIZE];
	u8 fsid[BTRFS_FSID_SIZE]; 
	__le64 bytenr; 
	__le64 flags;

	
	u8 chunk_tree_uuid[BTRFS_UUID_SIZE];
	__le64 generation;
	__le64 owner;
	__le32 nritems;
	u8 level;
} __attribute__ ((__packed__));

#define BTRFS_NODEPTRS_PER_BLOCK(r) (((r)->nodesize - \
				      sizeof(struct btrfs_header)) / \
				     sizeof(struct btrfs_key_ptr))
#define __BTRFS_LEAF_DATA_SIZE(bs) ((bs) - sizeof(struct btrfs_header))
#define BTRFS_LEAF_DATA_SIZE(r) (__BTRFS_LEAF_DATA_SIZE(r->leafsize))
#define BTRFS_MAX_INLINE_DATA_SIZE(r) (BTRFS_LEAF_DATA_SIZE(r) - \
					sizeof(struct btrfs_item) - \
					sizeof(struct btrfs_file_extent_item))
#define BTRFS_MAX_XATTR_SIZE(r)	(BTRFS_LEAF_DATA_SIZE(r) - \
				 sizeof(struct btrfs_item) -\
				 sizeof(struct btrfs_dir_item))


#define BTRFS_SYSTEM_CHUNK_ARRAY_SIZE 2048
#define BTRFS_LABEL_SIZE 256

#define BTRFS_NUM_BACKUP_ROOTS 4
struct btrfs_root_backup {
	__le64 tree_root;
	__le64 tree_root_gen;

	__le64 chunk_root;
	__le64 chunk_root_gen;

	__le64 extent_root;
	__le64 extent_root_gen;

	__le64 fs_root;
	__le64 fs_root_gen;

	__le64 dev_root;
	__le64 dev_root_gen;

	__le64 csum_root;
	__le64 csum_root_gen;

	__le64 total_bytes;
	__le64 bytes_used;
	__le64 num_devices;
	
	__le64 unsed_64[4];

	u8 tree_root_level;
	u8 chunk_root_level;
	u8 extent_root_level;
	u8 fs_root_level;
	u8 dev_root_level;
	u8 csum_root_level;
	
	u8 unused_8[10];
} __attribute__ ((__packed__));

struct btrfs_super_block {
	u8 csum[BTRFS_CSUM_SIZE];
	
	u8 fsid[BTRFS_FSID_SIZE];    
	__le64 bytenr; 
	__le64 flags;

	
	__le64 magic;
	__le64 generation;
	__le64 root;
	__le64 chunk_root;
	__le64 log_root;

	
	__le64 log_root_transid;
	__le64 total_bytes;
	__le64 bytes_used;
	__le64 root_dir_objectid;
	__le64 num_devices;
	__le32 sectorsize;
	__le32 nodesize;
	__le32 leafsize;
	__le32 stripesize;
	__le32 sys_chunk_array_size;
	__le64 chunk_root_generation;
	__le64 compat_flags;
	__le64 compat_ro_flags;
	__le64 incompat_flags;
	__le16 csum_type;
	u8 root_level;
	u8 chunk_root_level;
	u8 log_root_level;
	struct btrfs_dev_item dev_item;

	char label[BTRFS_LABEL_SIZE];

	__le64 cache_generation;

	
	__le64 reserved[31];
	u8 sys_chunk_array[BTRFS_SYSTEM_CHUNK_ARRAY_SIZE];
	struct btrfs_root_backup super_roots[BTRFS_NUM_BACKUP_ROOTS];
} __attribute__ ((__packed__));

#define BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF	(1ULL << 0)
#define BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL	(1ULL << 1)
#define BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS	(1ULL << 2)
#define BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO	(1ULL << 3)
#define BTRFS_FEATURE_INCOMPAT_COMPRESS_LZOv2	(1ULL << 4)

#define BTRFS_FEATURE_INCOMPAT_BIG_METADATA	(1ULL << 5)

#define BTRFS_FEATURE_COMPAT_SUPP		0ULL
#define BTRFS_FEATURE_COMPAT_RO_SUPP		0ULL
#define BTRFS_FEATURE_INCOMPAT_SUPP			\
	(BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF |		\
	 BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL |	\
	 BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS |		\
	 BTRFS_FEATURE_INCOMPAT_BIG_METADATA |		\
	 BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO)

struct btrfs_item {
	struct btrfs_disk_key key;
	__le32 offset;
	__le32 size;
} __attribute__ ((__packed__));

struct btrfs_leaf {
	struct btrfs_header header;
	struct btrfs_item items[];
} __attribute__ ((__packed__));

struct btrfs_key_ptr {
	struct btrfs_disk_key key;
	__le64 blockptr;
	__le64 generation;
} __attribute__ ((__packed__));

struct btrfs_node {
	struct btrfs_header header;
	struct btrfs_key_ptr ptrs[];
} __attribute__ ((__packed__));

struct btrfs_path {
	struct extent_buffer *nodes[BTRFS_MAX_LEVEL];
	int slots[BTRFS_MAX_LEVEL];
	
	int locks[BTRFS_MAX_LEVEL];
	int reada;
	
	int lowest_level;

	unsigned int search_for_split:1;
	unsigned int keep_locks:1;
	unsigned int skip_locking:1;
	unsigned int leave_spinning:1;
	unsigned int search_commit_root:1;
};


struct btrfs_extent_item {
	__le64 refs;
	__le64 generation;
	__le64 flags;
} __attribute__ ((__packed__));

struct btrfs_extent_item_v0 {
	__le32 refs;
} __attribute__ ((__packed__));

#define BTRFS_MAX_EXTENT_ITEM_SIZE(r) ((BTRFS_LEAF_DATA_SIZE(r) >> 4) - \
					sizeof(struct btrfs_item))

#define BTRFS_EXTENT_FLAG_DATA		(1ULL << 0)
#define BTRFS_EXTENT_FLAG_TREE_BLOCK	(1ULL << 1)


#define BTRFS_BLOCK_FLAG_FULL_BACKREF	(1ULL << 8)

#define BTRFS_EXTENT_FLAG_SUPER		(1ULL << 48)

struct btrfs_tree_block_info {
	struct btrfs_disk_key key;
	u8 level;
} __attribute__ ((__packed__));

struct btrfs_extent_data_ref {
	__le64 root;
	__le64 objectid;
	__le64 offset;
	__le32 count;
} __attribute__ ((__packed__));

struct btrfs_shared_data_ref {
	__le32 count;
} __attribute__ ((__packed__));

struct btrfs_extent_inline_ref {
	u8 type;
	__le64 offset;
} __attribute__ ((__packed__));

struct btrfs_extent_ref_v0 {
	__le64 root;
	__le64 generation;
	__le64 objectid;
	__le32 count;
} __attribute__ ((__packed__));


struct btrfs_dev_extent {
	__le64 chunk_tree;
	__le64 chunk_objectid;
	__le64 chunk_offset;
	__le64 length;
	u8 chunk_tree_uuid[BTRFS_UUID_SIZE];
} __attribute__ ((__packed__));

struct btrfs_inode_ref {
	__le64 index;
	__le16 name_len;
	
} __attribute__ ((__packed__));

struct btrfs_timespec {
	__le64 sec;
	__le32 nsec;
} __attribute__ ((__packed__));

enum btrfs_compression_type {
	BTRFS_COMPRESS_NONE  = 0,
	BTRFS_COMPRESS_ZLIB  = 1,
	BTRFS_COMPRESS_LZO   = 2,
	BTRFS_COMPRESS_TYPES = 2,
	BTRFS_COMPRESS_LAST  = 3,
};

struct btrfs_inode_item {
	
	__le64 generation;
	
	__le64 transid;
	__le64 size;
	__le64 nbytes;
	__le64 block_group;
	__le32 nlink;
	__le32 uid;
	__le32 gid;
	__le32 mode;
	__le64 rdev;
	__le64 flags;

	
	__le64 sequence;

	__le64 reserved[4];
	struct btrfs_timespec atime;
	struct btrfs_timespec ctime;
	struct btrfs_timespec mtime;
	struct btrfs_timespec otime;
} __attribute__ ((__packed__));

struct btrfs_dir_log_item {
	__le64 end;
} __attribute__ ((__packed__));

struct btrfs_dir_item {
	struct btrfs_disk_key location;
	__le64 transid;
	__le16 data_len;
	__le16 name_len;
	u8 type;
} __attribute__ ((__packed__));

#define BTRFS_ROOT_SUBVOL_RDONLY	(1ULL << 0)

struct btrfs_root_item {
	struct btrfs_inode_item inode;
	__le64 generation;
	__le64 root_dirid;
	__le64 bytenr;
	__le64 byte_limit;
	__le64 bytes_used;
	__le64 last_snapshot;
	__le64 flags;
	__le32 refs;
	struct btrfs_disk_key drop_progress;
	u8 drop_level;
	u8 level;
} __attribute__ ((__packed__));

struct btrfs_root_ref {
	__le64 dirid;
	__le64 sequence;
	__le16 name_len;
} __attribute__ ((__packed__));

struct btrfs_disk_balance_args {
	__le64 profiles;

	
	__le64 usage;

	
	__le64 devid;

	
	__le64 pstart;
	__le64 pend;

	
	__le64 vstart;
	__le64 vend;

	__le64 target;

	
	__le64 flags;

	__le64 unused[8];
} __attribute__ ((__packed__));

struct btrfs_balance_item {
	
	__le64 flags;

	struct btrfs_disk_balance_args data;
	struct btrfs_disk_balance_args meta;
	struct btrfs_disk_balance_args sys;

	__le64 unused[4];
} __attribute__ ((__packed__));

#define BTRFS_FILE_EXTENT_INLINE 0
#define BTRFS_FILE_EXTENT_REG 1
#define BTRFS_FILE_EXTENT_PREALLOC 2

struct btrfs_file_extent_item {
	__le64 generation;
	__le64 ram_bytes;

	u8 compression;
	u8 encryption;
	__le16 other_encoding; 

	
	u8 type;

	__le64 disk_bytenr;
	__le64 disk_num_bytes;
	__le64 offset;
	__le64 num_bytes;

} __attribute__ ((__packed__));

struct btrfs_csum_item {
	u8 csum;
} __attribute__ ((__packed__));

#define BTRFS_BLOCK_GROUP_DATA		(1ULL << 0)
#define BTRFS_BLOCK_GROUP_SYSTEM	(1ULL << 1)
#define BTRFS_BLOCK_GROUP_METADATA	(1ULL << 2)
#define BTRFS_BLOCK_GROUP_RAID0		(1ULL << 3)
#define BTRFS_BLOCK_GROUP_RAID1		(1ULL << 4)
#define BTRFS_BLOCK_GROUP_DUP		(1ULL << 5)
#define BTRFS_BLOCK_GROUP_RAID10	(1ULL << 6)
#define BTRFS_BLOCK_GROUP_RESERVED	BTRFS_AVAIL_ALLOC_BIT_SINGLE
#define BTRFS_NR_RAID_TYPES		5

#define BTRFS_BLOCK_GROUP_TYPE_MASK	(BTRFS_BLOCK_GROUP_DATA |    \
					 BTRFS_BLOCK_GROUP_SYSTEM |  \
					 BTRFS_BLOCK_GROUP_METADATA)

#define BTRFS_BLOCK_GROUP_PROFILE_MASK	(BTRFS_BLOCK_GROUP_RAID0 |   \
					 BTRFS_BLOCK_GROUP_RAID1 |   \
					 BTRFS_BLOCK_GROUP_DUP |     \
					 BTRFS_BLOCK_GROUP_RAID10)
#define BTRFS_AVAIL_ALLOC_BIT_SINGLE	(1ULL << 48)

#define BTRFS_EXTENDED_PROFILE_MASK	(BTRFS_BLOCK_GROUP_PROFILE_MASK | \
					 BTRFS_AVAIL_ALLOC_BIT_SINGLE)

static inline u64 chunk_to_extended(u64 flags)
{
	if ((flags & BTRFS_BLOCK_GROUP_PROFILE_MASK) == 0)
		flags |= BTRFS_AVAIL_ALLOC_BIT_SINGLE;

	return flags;
}
static inline u64 extended_to_chunk(u64 flags)
{
	return flags & ~BTRFS_AVAIL_ALLOC_BIT_SINGLE;
}

struct btrfs_block_group_item {
	__le64 used;
	__le64 chunk_objectid;
	__le64 flags;
} __attribute__ ((__packed__));

struct btrfs_space_info {
	u64 flags;

	u64 total_bytes;	
	u64 bytes_used;		
	u64 bytes_pinned;	
	u64 bytes_reserved;	
	u64 bytes_readonly;	

	u64 bytes_may_use;	
	u64 disk_used;		
	u64 disk_total;		

	unsigned long reservation_progress;

	unsigned int full:1;	
	unsigned int chunk_alloc:1;	

	unsigned int flush:1;		

	unsigned int force_alloc;	

	struct list_head list;

	
	struct list_head block_groups[BTRFS_NR_RAID_TYPES];
	spinlock_t lock;
	struct rw_semaphore groups_sem;
	wait_queue_head_t wait;
};

struct btrfs_block_rsv {
	u64 size;
	u64 reserved;
	struct btrfs_space_info *space_info;
	spinlock_t lock;
	unsigned int full;
};

struct btrfs_free_cluster {
	spinlock_t lock;
	spinlock_t refill_lock;
	struct rb_root root;

	
	u64 max_size;

	
	u64 window_start;

	struct btrfs_block_group_cache *block_group;
	struct list_head block_group_list;
};

enum btrfs_caching_type {
	BTRFS_CACHE_NO		= 0,
	BTRFS_CACHE_STARTED	= 1,
	BTRFS_CACHE_FAST	= 2,
	BTRFS_CACHE_FINISHED	= 3,
};

enum btrfs_disk_cache_state {
	BTRFS_DC_WRITTEN	= 0,
	BTRFS_DC_ERROR		= 1,
	BTRFS_DC_CLEAR		= 2,
	BTRFS_DC_SETUP		= 3,
	BTRFS_DC_NEED_WRITE	= 4,
};

struct btrfs_caching_control {
	struct list_head list;
	struct mutex mutex;
	wait_queue_head_t wait;
	struct btrfs_work work;
	struct btrfs_block_group_cache *block_group;
	u64 progress;
	atomic_t count;
};

struct btrfs_block_group_cache {
	struct btrfs_key key;
	struct btrfs_block_group_item item;
	struct btrfs_fs_info *fs_info;
	struct inode *inode;
	spinlock_t lock;
	u64 pinned;
	u64 reserved;
	u64 bytes_super;
	u64 flags;
	u64 sectorsize;
	u64 cache_generation;
	unsigned int ro:1;
	unsigned int dirty:1;
	unsigned int iref:1;

	int disk_cache_state;

	
	int cached;
	struct btrfs_caching_control *caching_ctl;
	u64 last_byte_to_unpin;

	struct btrfs_space_info *space_info;

	
	struct btrfs_free_space_ctl *free_space_ctl;

	
	struct rb_node cache_node;

	
	struct list_head list;

	
	atomic_t count;

	struct list_head cluster_list;
};

struct reloc_control;
struct btrfs_device;
struct btrfs_fs_devices;
struct btrfs_balance_control;
struct btrfs_delayed_root;
struct btrfs_fs_info {
	u8 fsid[BTRFS_FSID_SIZE];
	u8 chunk_tree_uuid[BTRFS_UUID_SIZE];
	struct btrfs_root *extent_root;
	struct btrfs_root *tree_root;
	struct btrfs_root *chunk_root;
	struct btrfs_root *dev_root;
	struct btrfs_root *fs_root;
	struct btrfs_root *csum_root;

	
	struct btrfs_root *log_root_tree;

	spinlock_t fs_roots_radix_lock;
	struct radix_tree_root fs_roots_radix;

	
	spinlock_t block_group_cache_lock;
	struct rb_root block_group_cache_tree;

	
	spinlock_t free_chunk_lock;
	u64 free_chunk_space;

	struct extent_io_tree freed_extents[2];
	struct extent_io_tree *pinned_extents;

	
	struct btrfs_mapping_tree mapping_tree;

	struct btrfs_block_rsv global_block_rsv;
	
	struct btrfs_block_rsv delalloc_block_rsv;
	
	struct btrfs_block_rsv trans_block_rsv;
	
	struct btrfs_block_rsv chunk_block_rsv;
	
	struct btrfs_block_rsv delayed_block_rsv;

	struct btrfs_block_rsv empty_block_rsv;

	u64 generation;
	u64 last_trans_committed;

	u64 last_trans_log_full_commit;
	unsigned long mount_opt;
	unsigned long compress_type:4;
	u64 max_inline;
	u64 alloc_start;
	struct btrfs_transaction *running_transaction;
	wait_queue_head_t transaction_throttle;
	wait_queue_head_t transaction_wait;
	wait_queue_head_t transaction_blocked_wait;
	wait_queue_head_t async_submit_wait;

	struct btrfs_super_block *super_copy;
	struct btrfs_super_block *super_for_commit;
	struct block_device *__bdev;
	struct super_block *sb;
	struct inode *btree_inode;
	struct backing_dev_info bdi;
	struct mutex tree_log_mutex;
	struct mutex transaction_kthread_mutex;
	struct mutex cleaner_mutex;
	struct mutex chunk_mutex;
	struct mutex volume_mutex;
	struct mutex ordered_operations_mutex;
	struct rw_semaphore extent_commit_sem;

	struct rw_semaphore cleanup_work_sem;

	struct rw_semaphore subvol_sem;
	struct srcu_struct subvol_srcu;

	spinlock_t trans_lock;
	struct mutex reloc_mutex;

	struct list_head trans_list;
	struct list_head hashers;
	struct list_head dead_roots;
	struct list_head caching_block_groups;

	spinlock_t delayed_iput_lock;
	struct list_head delayed_iputs;

	atomic_t nr_async_submits;
	atomic_t async_submit_draining;
	atomic_t nr_async_bios;
	atomic_t async_delalloc_pages;
	atomic_t open_ioctl_trans;

	spinlock_t ordered_extent_lock;

	struct list_head ordered_extents;

	struct list_head delalloc_inodes;

	struct list_head ordered_operations;

	struct btrfs_workers generic_worker;
	struct btrfs_workers workers;
	struct btrfs_workers delalloc_workers;
	struct btrfs_workers endio_workers;
	struct btrfs_workers endio_meta_workers;
	struct btrfs_workers endio_meta_write_workers;
	struct btrfs_workers endio_write_workers;
	struct btrfs_workers endio_freespace_worker;
	struct btrfs_workers submit_workers;
	struct btrfs_workers caching_workers;
	struct btrfs_workers readahead_workers;

	struct btrfs_workers fixup_workers;
	struct btrfs_workers delayed_workers;
	struct task_struct *transaction_kthread;
	struct task_struct *cleaner_kthread;
	int thread_pool_size;

	struct kobject super_kobj;
	struct completion kobj_unregister;
	int do_barriers;
	int closing;
	int log_root_recovering;
	int enospc_unlink;
	int trans_no_join;

	u64 total_pinned;

	u64 dirty_metadata_bytes;
	struct list_head dirty_cowonly_roots;

	struct btrfs_fs_devices *fs_devices;

	struct list_head space_info;

	struct reloc_control *reloc_ctl;

	spinlock_t delalloc_lock;
	u64 delalloc_bytes;

	
	struct btrfs_free_cluster data_alloc_cluster;

	
	struct btrfs_free_cluster meta_alloc_cluster;

	
	spinlock_t defrag_inodes_lock;
	struct rb_root defrag_inodes;
	atomic_t defrag_running;

	spinlock_t ref_cache_lock;
	u64 total_ref_cache_size;

	u64 avail_data_alloc_bits;
	u64 avail_metadata_alloc_bits;
	u64 avail_system_alloc_bits;

	
	spinlock_t balance_lock;
	struct mutex balance_mutex;
	atomic_t balance_running;
	atomic_t balance_pause_req;
	atomic_t balance_cancel_req;
	struct btrfs_balance_control *balance_ctl;
	wait_queue_head_t balance_wait_q;

	unsigned data_chunk_allocations;
	unsigned metadata_ratio;

	void *bdev_holder;

	
	struct mutex scrub_lock;
	atomic_t scrubs_running;
	atomic_t scrub_pause_req;
	atomic_t scrubs_paused;
	atomic_t scrub_cancel_req;
	wait_queue_head_t scrub_pause_wait;
	struct rw_semaphore scrub_super_lock;
	int scrub_workers_refcnt;
	struct btrfs_workers scrub_workers;

#ifdef CONFIG_BTRFS_FS_CHECK_INTEGRITY
	u32 check_integrity_print_mask;
#endif

	
	u64 fs_state;

	struct btrfs_delayed_root *delayed_root;

	
	spinlock_t reada_lock;
	struct radix_tree_root reada_tree;

	/* next backup root to be overwritten */
	int backup_root_index;
};

struct btrfs_root {
	struct extent_buffer *node;

	struct extent_buffer *commit_root;
	struct btrfs_root *log_root;
	struct btrfs_root *reloc_root;

	struct btrfs_root_item root_item;
	struct btrfs_key root_key;
	struct btrfs_fs_info *fs_info;
	struct extent_io_tree dirty_log_pages;

	struct kobject root_kobj;
	struct completion kobj_unregister;
	struct mutex objectid_mutex;

	spinlock_t accounting_lock;
	struct btrfs_block_rsv *block_rsv;

	
	struct mutex fs_commit_mutex;
	struct btrfs_free_space_ctl *free_ino_ctl;
	enum btrfs_caching_type cached;
	spinlock_t cache_lock;
	wait_queue_head_t cache_wait;
	struct btrfs_free_space_ctl *free_ino_pinned;
	u64 cache_progress;
	struct inode *cache_inode;

	struct mutex log_mutex;
	wait_queue_head_t log_writer_wait;
	wait_queue_head_t log_commit_wait[2];
	atomic_t log_writers;
	atomic_t log_commit[2];
	unsigned long log_transid;
	unsigned long last_log_commit;
	unsigned long log_batch;
	pid_t log_start_pid;
	bool log_multiple_pids;

	u64 objectid;
	u64 last_trans;

	
	u32 sectorsize;

	
	u32 nodesize;

	
	u32 leafsize;

	u32 stripesize;

	u32 type;

	u64 highest_objectid;

	unsigned long in_trans_setup;
	int ref_cows;
	int track_dirty;
	int in_radix;

	u64 defrag_trans_start;
	struct btrfs_key defrag_progress;
	struct btrfs_key defrag_max;
	int defrag_running;
	char *name;

	
	struct list_head dirty_list;

	struct list_head root_list;

	spinlock_t orphan_lock;
	struct list_head orphan_list;
	struct btrfs_block_rsv *orphan_block_rsv;
	int orphan_item_inserted;
	int orphan_cleanup_state;

	spinlock_t inode_lock;
	
	struct rb_root inode_tree;

	struct radix_tree_root delayed_nodes_tree;
	dev_t anon_dev;

	int force_cow;
};

struct btrfs_ioctl_defrag_range_args {
	
	__u64 start;

	
	__u64 len;

	__u64 flags;

	/*
	 * any extent bigger than this will be considered
	 * already defragged.  Use 0 to take the kernel default
	 * Use 1 to say every single extent must be rewritten
	 */
	__u32 extent_thresh;

	__u32 compress_type;

	
	__u32 unused[4];
};


#define BTRFS_INODE_ITEM_KEY		1
#define BTRFS_INODE_REF_KEY		12
#define BTRFS_XATTR_ITEM_KEY		24
#define BTRFS_ORPHAN_ITEM_KEY		48

#define BTRFS_DIR_LOG_ITEM_KEY  60
#define BTRFS_DIR_LOG_INDEX_KEY 72
#define BTRFS_DIR_ITEM_KEY	84
#define BTRFS_DIR_INDEX_KEY	96
#define BTRFS_EXTENT_DATA_KEY	108

#define BTRFS_EXTENT_CSUM_KEY	128

#define BTRFS_ROOT_ITEM_KEY	132

#define BTRFS_ROOT_BACKREF_KEY	144

#define BTRFS_ROOT_REF_KEY	156

#define BTRFS_EXTENT_ITEM_KEY	168

#define BTRFS_TREE_BLOCK_REF_KEY	176

#define BTRFS_EXTENT_DATA_REF_KEY	178

#define BTRFS_EXTENT_REF_V0_KEY		180

#define BTRFS_SHARED_BLOCK_REF_KEY	182

#define BTRFS_SHARED_DATA_REF_KEY	184

#define BTRFS_BLOCK_GROUP_ITEM_KEY 192

#define BTRFS_DEV_EXTENT_KEY	204
#define BTRFS_DEV_ITEM_KEY	216
#define BTRFS_CHUNK_ITEM_KEY	228

#define BTRFS_BALANCE_ITEM_KEY	248

#define BTRFS_STRING_ITEM_KEY	253

#define BTRFS_MOUNT_NODATASUM		(1 << 0)
#define BTRFS_MOUNT_NODATACOW		(1 << 1)
#define BTRFS_MOUNT_NOBARRIER		(1 << 2)
#define BTRFS_MOUNT_SSD			(1 << 3)
#define BTRFS_MOUNT_DEGRADED		(1 << 4)
#define BTRFS_MOUNT_COMPRESS		(1 << 5)
#define BTRFS_MOUNT_NOTREELOG           (1 << 6)
#define BTRFS_MOUNT_FLUSHONCOMMIT       (1 << 7)
#define BTRFS_MOUNT_SSD_SPREAD		(1 << 8)
#define BTRFS_MOUNT_NOSSD		(1 << 9)
#define BTRFS_MOUNT_DISCARD		(1 << 10)
#define BTRFS_MOUNT_FORCE_COMPRESS      (1 << 11)
#define BTRFS_MOUNT_SPACE_CACHE		(1 << 12)
#define BTRFS_MOUNT_CLEAR_CACHE		(1 << 13)
#define BTRFS_MOUNT_USER_SUBVOL_RM_ALLOWED (1 << 14)
#define BTRFS_MOUNT_ENOSPC_DEBUG	 (1 << 15)
#define BTRFS_MOUNT_AUTO_DEFRAG		(1 << 16)
#define BTRFS_MOUNT_INODE_MAP_CACHE	(1 << 17)
#define BTRFS_MOUNT_RECOVERY		(1 << 18)
#define BTRFS_MOUNT_SKIP_BALANCE	(1 << 19)
#define BTRFS_MOUNT_CHECK_INTEGRITY	(1 << 20)
#define BTRFS_MOUNT_CHECK_INTEGRITY_INCLUDING_EXTENT_DATA (1 << 21)
#define BTRFS_MOUNT_PANIC_ON_FATAL_ERROR	(1 << 22)

#define btrfs_clear_opt(o, opt)		((o) &= ~BTRFS_MOUNT_##opt)
#define btrfs_set_opt(o, opt)		((o) |= BTRFS_MOUNT_##opt)
#define btrfs_test_opt(root, opt)	((root)->fs_info->mount_opt & \
					 BTRFS_MOUNT_##opt)
#define BTRFS_INODE_NODATASUM		(1 << 0)
#define BTRFS_INODE_NODATACOW		(1 << 1)
#define BTRFS_INODE_READONLY		(1 << 2)
#define BTRFS_INODE_NOCOMPRESS		(1 << 3)
#define BTRFS_INODE_PREALLOC		(1 << 4)
#define BTRFS_INODE_SYNC		(1 << 5)
#define BTRFS_INODE_IMMUTABLE		(1 << 6)
#define BTRFS_INODE_APPEND		(1 << 7)
#define BTRFS_INODE_NODUMP		(1 << 8)
#define BTRFS_INODE_NOATIME		(1 << 9)
#define BTRFS_INODE_DIRSYNC		(1 << 10)
#define BTRFS_INODE_COMPRESS		(1 << 11)

#define BTRFS_INODE_ROOT_ITEM_INIT	(1 << 31)

struct btrfs_map_token {
	struct extent_buffer *eb;
	char *kaddr;
	unsigned long offset;
};

static inline void btrfs_init_map_token (struct btrfs_map_token *token)
{
	memset(token, 0, sizeof(*token));
}

#define le8_to_cpu(v) (v)
#define cpu_to_le8(v) (v)
#define __le8 u8

#define read_eb_member(eb, ptr, type, member, result) (			\
	read_extent_buffer(eb, (char *)(result),			\
			   ((unsigned long)(ptr)) +			\
			    offsetof(type, member),			\
			   sizeof(((type *)0)->member)))

#define write_eb_member(eb, ptr, type, member, result) (		\
	write_extent_buffer(eb, (char *)(result),			\
			   ((unsigned long)(ptr)) +			\
			    offsetof(type, member),			\
			   sizeof(((type *)0)->member)))

#ifndef BTRFS_SETGET_FUNCS
#define BTRFS_SETGET_FUNCS(name, type, member, bits)			\
u##bits btrfs_##name(struct extent_buffer *eb, type *s);		\
u##bits btrfs_token_##name(struct extent_buffer *eb, type *s, struct btrfs_map_token *token);		\
void btrfs_set_token_##name(struct extent_buffer *eb, type *s, u##bits val, struct btrfs_map_token *token);\
void btrfs_set_##name(struct extent_buffer *eb, type *s, u##bits val);
#endif

#define BTRFS_SETGET_HEADER_FUNCS(name, type, member, bits)		\
static inline u##bits btrfs_##name(struct extent_buffer *eb)		\
{									\
	type *p = page_address(eb->pages[0]);				\
	u##bits res = le##bits##_to_cpu(p->member);			\
	return res;							\
}									\
static inline void btrfs_set_##name(struct extent_buffer *eb,		\
				    u##bits val)			\
{									\
	type *p = page_address(eb->pages[0]);				\
	p->member = cpu_to_le##bits(val);				\
}

#define BTRFS_SETGET_STACK_FUNCS(name, type, member, bits)		\
static inline u##bits btrfs_##name(type *s)				\
{									\
	return le##bits##_to_cpu(s->member);				\
}									\
static inline void btrfs_set_##name(type *s, u##bits val)		\
{									\
	s->member = cpu_to_le##bits(val);				\
}

BTRFS_SETGET_FUNCS(device_type, struct btrfs_dev_item, type, 64);
BTRFS_SETGET_FUNCS(device_total_bytes, struct btrfs_dev_item, total_bytes, 64);
BTRFS_SETGET_FUNCS(device_bytes_used, struct btrfs_dev_item, bytes_used, 64);
BTRFS_SETGET_FUNCS(device_io_align, struct btrfs_dev_item, io_align, 32);
BTRFS_SETGET_FUNCS(device_io_width, struct btrfs_dev_item, io_width, 32);
BTRFS_SETGET_FUNCS(device_start_offset, struct btrfs_dev_item,
		   start_offset, 64);
BTRFS_SETGET_FUNCS(device_sector_size, struct btrfs_dev_item, sector_size, 32);
BTRFS_SETGET_FUNCS(device_id, struct btrfs_dev_item, devid, 64);
BTRFS_SETGET_FUNCS(device_group, struct btrfs_dev_item, dev_group, 32);
BTRFS_SETGET_FUNCS(device_seek_speed, struct btrfs_dev_item, seek_speed, 8);
BTRFS_SETGET_FUNCS(device_bandwidth, struct btrfs_dev_item, bandwidth, 8);
BTRFS_SETGET_FUNCS(device_generation, struct btrfs_dev_item, generation, 64);

BTRFS_SETGET_STACK_FUNCS(stack_device_type, struct btrfs_dev_item, type, 64);
BTRFS_SETGET_STACK_FUNCS(stack_device_total_bytes, struct btrfs_dev_item,
			 total_bytes, 64);
BTRFS_SETGET_STACK_FUNCS(stack_device_bytes_used, struct btrfs_dev_item,
			 bytes_used, 64);
BTRFS_SETGET_STACK_FUNCS(stack_device_io_align, struct btrfs_dev_item,
			 io_align, 32);
BTRFS_SETGET_STACK_FUNCS(stack_device_io_width, struct btrfs_dev_item,
			 io_width, 32);
BTRFS_SETGET_STACK_FUNCS(stack_device_sector_size, struct btrfs_dev_item,
			 sector_size, 32);
BTRFS_SETGET_STACK_FUNCS(stack_device_id, struct btrfs_dev_item, devid, 64);
BTRFS_SETGET_STACK_FUNCS(stack_device_group, struct btrfs_dev_item,
			 dev_group, 32);
BTRFS_SETGET_STACK_FUNCS(stack_device_seek_speed, struct btrfs_dev_item,
			 seek_speed, 8);
BTRFS_SETGET_STACK_FUNCS(stack_device_bandwidth, struct btrfs_dev_item,
			 bandwidth, 8);
BTRFS_SETGET_STACK_FUNCS(stack_device_generation, struct btrfs_dev_item,
			 generation, 64);

static inline char *btrfs_device_uuid(struct btrfs_dev_item *d)
{
	return (char *)d + offsetof(struct btrfs_dev_item, uuid);
}

static inline char *btrfs_device_fsid(struct btrfs_dev_item *d)
{
	return (char *)d + offsetof(struct btrfs_dev_item, fsid);
}

BTRFS_SETGET_FUNCS(chunk_length, struct btrfs_chunk, length, 64);
BTRFS_SETGET_FUNCS(chunk_owner, struct btrfs_chunk, owner, 64);
BTRFS_SETGET_FUNCS(chunk_stripe_len, struct btrfs_chunk, stripe_len, 64);
BTRFS_SETGET_FUNCS(chunk_io_align, struct btrfs_chunk, io_align, 32);
BTRFS_SETGET_FUNCS(chunk_io_width, struct btrfs_chunk, io_width, 32);
BTRFS_SETGET_FUNCS(chunk_sector_size, struct btrfs_chunk, sector_size, 32);
BTRFS_SETGET_FUNCS(chunk_type, struct btrfs_chunk, type, 64);
BTRFS_SETGET_FUNCS(chunk_num_stripes, struct btrfs_chunk, num_stripes, 16);
BTRFS_SETGET_FUNCS(chunk_sub_stripes, struct btrfs_chunk, sub_stripes, 16);
BTRFS_SETGET_FUNCS(stripe_devid, struct btrfs_stripe, devid, 64);
BTRFS_SETGET_FUNCS(stripe_offset, struct btrfs_stripe, offset, 64);

static inline char *btrfs_stripe_dev_uuid(struct btrfs_stripe *s)
{
	return (char *)s + offsetof(struct btrfs_stripe, dev_uuid);
}

BTRFS_SETGET_STACK_FUNCS(stack_chunk_length, struct btrfs_chunk, length, 64);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_owner, struct btrfs_chunk, owner, 64);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_stripe_len, struct btrfs_chunk,
			 stripe_len, 64);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_io_align, struct btrfs_chunk,
			 io_align, 32);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_io_width, struct btrfs_chunk,
			 io_width, 32);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_sector_size, struct btrfs_chunk,
			 sector_size, 32);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_type, struct btrfs_chunk, type, 64);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_num_stripes, struct btrfs_chunk,
			 num_stripes, 16);
BTRFS_SETGET_STACK_FUNCS(stack_chunk_sub_stripes, struct btrfs_chunk,
			 sub_stripes, 16);
BTRFS_SETGET_STACK_FUNCS(stack_stripe_devid, struct btrfs_stripe, devid, 64);
BTRFS_SETGET_STACK_FUNCS(stack_stripe_offset, struct btrfs_stripe, offset, 64);

static inline struct btrfs_stripe *btrfs_stripe_nr(struct btrfs_chunk *c,
						   int nr)
{
	unsigned long offset = (unsigned long)c;
	offset += offsetof(struct btrfs_chunk, stripe);
	offset += nr * sizeof(struct btrfs_stripe);
	return (struct btrfs_stripe *)offset;
}

static inline char *btrfs_stripe_dev_uuid_nr(struct btrfs_chunk *c, int nr)
{
	return btrfs_stripe_dev_uuid(btrfs_stripe_nr(c, nr));
}

static inline u64 btrfs_stripe_offset_nr(struct extent_buffer *eb,
					 struct btrfs_chunk *c, int nr)
{
	return btrfs_stripe_offset(eb, btrfs_stripe_nr(c, nr));
}

static inline u64 btrfs_stripe_devid_nr(struct extent_buffer *eb,
					 struct btrfs_chunk *c, int nr)
{
	return btrfs_stripe_devid(eb, btrfs_stripe_nr(c, nr));
}

BTRFS_SETGET_STACK_FUNCS(block_group_used, struct btrfs_block_group_item,
			 used, 64);
BTRFS_SETGET_FUNCS(disk_block_group_used, struct btrfs_block_group_item,
			 used, 64);
BTRFS_SETGET_STACK_FUNCS(block_group_chunk_objectid,
			struct btrfs_block_group_item, chunk_objectid, 64);

BTRFS_SETGET_FUNCS(disk_block_group_chunk_objectid,
		   struct btrfs_block_group_item, chunk_objectid, 64);
BTRFS_SETGET_FUNCS(disk_block_group_flags,
		   struct btrfs_block_group_item, flags, 64);
BTRFS_SETGET_STACK_FUNCS(block_group_flags,
			struct btrfs_block_group_item, flags, 64);

BTRFS_SETGET_FUNCS(inode_ref_name_len, struct btrfs_inode_ref, name_len, 16);
BTRFS_SETGET_FUNCS(inode_ref_index, struct btrfs_inode_ref, index, 64);

BTRFS_SETGET_FUNCS(inode_generation, struct btrfs_inode_item, generation, 64);
BTRFS_SETGET_FUNCS(inode_sequence, struct btrfs_inode_item, sequence, 64);
BTRFS_SETGET_FUNCS(inode_transid, struct btrfs_inode_item, transid, 64);
BTRFS_SETGET_FUNCS(inode_size, struct btrfs_inode_item, size, 64);
BTRFS_SETGET_FUNCS(inode_nbytes, struct btrfs_inode_item, nbytes, 64);
BTRFS_SETGET_FUNCS(inode_block_group, struct btrfs_inode_item, block_group, 64);
BTRFS_SETGET_FUNCS(inode_nlink, struct btrfs_inode_item, nlink, 32);
BTRFS_SETGET_FUNCS(inode_uid, struct btrfs_inode_item, uid, 32);
BTRFS_SETGET_FUNCS(inode_gid, struct btrfs_inode_item, gid, 32);
BTRFS_SETGET_FUNCS(inode_mode, struct btrfs_inode_item, mode, 32);
BTRFS_SETGET_FUNCS(inode_rdev, struct btrfs_inode_item, rdev, 64);
BTRFS_SETGET_FUNCS(inode_flags, struct btrfs_inode_item, flags, 64);

static inline struct btrfs_timespec *
btrfs_inode_atime(struct btrfs_inode_item *inode_item)
{
	unsigned long ptr = (unsigned long)inode_item;
	ptr += offsetof(struct btrfs_inode_item, atime);
	return (struct btrfs_timespec *)ptr;
}

static inline struct btrfs_timespec *
btrfs_inode_mtime(struct btrfs_inode_item *inode_item)
{
	unsigned long ptr = (unsigned long)inode_item;
	ptr += offsetof(struct btrfs_inode_item, mtime);
	return (struct btrfs_timespec *)ptr;
}

static inline struct btrfs_timespec *
btrfs_inode_ctime(struct btrfs_inode_item *inode_item)
{
	unsigned long ptr = (unsigned long)inode_item;
	ptr += offsetof(struct btrfs_inode_item, ctime);
	return (struct btrfs_timespec *)ptr;
}

BTRFS_SETGET_FUNCS(timespec_sec, struct btrfs_timespec, sec, 64);
BTRFS_SETGET_FUNCS(timespec_nsec, struct btrfs_timespec, nsec, 32);

BTRFS_SETGET_FUNCS(dev_extent_chunk_tree, struct btrfs_dev_extent,
		   chunk_tree, 64);
BTRFS_SETGET_FUNCS(dev_extent_chunk_objectid, struct btrfs_dev_extent,
		   chunk_objectid, 64);
BTRFS_SETGET_FUNCS(dev_extent_chunk_offset, struct btrfs_dev_extent,
		   chunk_offset, 64);
BTRFS_SETGET_FUNCS(dev_extent_length, struct btrfs_dev_extent, length, 64);

static inline u8 *btrfs_dev_extent_chunk_tree_uuid(struct btrfs_dev_extent *dev)
{
	unsigned long ptr = offsetof(struct btrfs_dev_extent, chunk_tree_uuid);
	return (u8 *)((unsigned long)dev + ptr);
}

BTRFS_SETGET_FUNCS(extent_refs, struct btrfs_extent_item, refs, 64);
BTRFS_SETGET_FUNCS(extent_generation, struct btrfs_extent_item,
		   generation, 64);
BTRFS_SETGET_FUNCS(extent_flags, struct btrfs_extent_item, flags, 64);

BTRFS_SETGET_FUNCS(extent_refs_v0, struct btrfs_extent_item_v0, refs, 32);


BTRFS_SETGET_FUNCS(tree_block_level, struct btrfs_tree_block_info, level, 8);

static inline void btrfs_tree_block_key(struct extent_buffer *eb,
					struct btrfs_tree_block_info *item,
					struct btrfs_disk_key *key)
{
	read_eb_member(eb, item, struct btrfs_tree_block_info, key, key);
}

static inline void btrfs_set_tree_block_key(struct extent_buffer *eb,
					    struct btrfs_tree_block_info *item,
					    struct btrfs_disk_key *key)
{
	write_eb_member(eb, item, struct btrfs_tree_block_info, key, key);
}

BTRFS_SETGET_FUNCS(extent_data_ref_root, struct btrfs_extent_data_ref,
		   root, 64);
BTRFS_SETGET_FUNCS(extent_data_ref_objectid, struct btrfs_extent_data_ref,
		   objectid, 64);
BTRFS_SETGET_FUNCS(extent_data_ref_offset, struct btrfs_extent_data_ref,
		   offset, 64);
BTRFS_SETGET_FUNCS(extent_data_ref_count, struct btrfs_extent_data_ref,
		   count, 32);

BTRFS_SETGET_FUNCS(shared_data_ref_count, struct btrfs_shared_data_ref,
		   count, 32);

BTRFS_SETGET_FUNCS(extent_inline_ref_type, struct btrfs_extent_inline_ref,
		   type, 8);
BTRFS_SETGET_FUNCS(extent_inline_ref_offset, struct btrfs_extent_inline_ref,
		   offset, 64);

static inline u32 btrfs_extent_inline_ref_size(int type)
{
	if (type == BTRFS_TREE_BLOCK_REF_KEY ||
	    type == BTRFS_SHARED_BLOCK_REF_KEY)
		return sizeof(struct btrfs_extent_inline_ref);
	if (type == BTRFS_SHARED_DATA_REF_KEY)
		return sizeof(struct btrfs_shared_data_ref) +
		       sizeof(struct btrfs_extent_inline_ref);
	if (type == BTRFS_EXTENT_DATA_REF_KEY)
		return sizeof(struct btrfs_extent_data_ref) +
		       offsetof(struct btrfs_extent_inline_ref, offset);
	BUG();
	return 0;
}

BTRFS_SETGET_FUNCS(ref_root_v0, struct btrfs_extent_ref_v0, root, 64);
BTRFS_SETGET_FUNCS(ref_generation_v0, struct btrfs_extent_ref_v0,
		   generation, 64);
BTRFS_SETGET_FUNCS(ref_objectid_v0, struct btrfs_extent_ref_v0, objectid, 64);
BTRFS_SETGET_FUNCS(ref_count_v0, struct btrfs_extent_ref_v0, count, 32);

BTRFS_SETGET_FUNCS(key_blockptr, struct btrfs_key_ptr, blockptr, 64);
BTRFS_SETGET_FUNCS(key_generation, struct btrfs_key_ptr, generation, 64);

static inline u64 btrfs_node_blockptr(struct extent_buffer *eb, int nr)
{
	unsigned long ptr;
	ptr = offsetof(struct btrfs_node, ptrs) +
		sizeof(struct btrfs_key_ptr) * nr;
	return btrfs_key_blockptr(eb, (struct btrfs_key_ptr *)ptr);
}

static inline void btrfs_set_node_blockptr(struct extent_buffer *eb,
					   int nr, u64 val)
{
	unsigned long ptr;
	ptr = offsetof(struct btrfs_node, ptrs) +
		sizeof(struct btrfs_key_ptr) * nr;
	btrfs_set_key_blockptr(eb, (struct btrfs_key_ptr *)ptr, val);
}

static inline u64 btrfs_node_ptr_generation(struct extent_buffer *eb, int nr)
{
	unsigned long ptr;
	ptr = offsetof(struct btrfs_node, ptrs) +
		sizeof(struct btrfs_key_ptr) * nr;
	return btrfs_key_generation(eb, (struct btrfs_key_ptr *)ptr);
}

static inline void btrfs_set_node_ptr_generation(struct extent_buffer *eb,
						 int nr, u64 val)
{
	unsigned long ptr;
	ptr = offsetof(struct btrfs_node, ptrs) +
		sizeof(struct btrfs_key_ptr) * nr;
	btrfs_set_key_generation(eb, (struct btrfs_key_ptr *)ptr, val);
}

static inline unsigned long btrfs_node_key_ptr_offset(int nr)
{
	return offsetof(struct btrfs_node, ptrs) +
		sizeof(struct btrfs_key_ptr) * nr;
}

void btrfs_node_key(struct extent_buffer *eb,
		    struct btrfs_disk_key *disk_key, int nr);

static inline void btrfs_set_node_key(struct extent_buffer *eb,
				      struct btrfs_disk_key *disk_key, int nr)
{
	unsigned long ptr;
	ptr = btrfs_node_key_ptr_offset(nr);
	write_eb_member(eb, (struct btrfs_key_ptr *)ptr,
		       struct btrfs_key_ptr, key, disk_key);
}

BTRFS_SETGET_FUNCS(item_offset, struct btrfs_item, offset, 32);
BTRFS_SETGET_FUNCS(item_size, struct btrfs_item, size, 32);

static inline unsigned long btrfs_item_nr_offset(int nr)
{
	return offsetof(struct btrfs_leaf, items) +
		sizeof(struct btrfs_item) * nr;
}

static inline struct btrfs_item *btrfs_item_nr(struct extent_buffer *eb,
					       int nr)
{
	return (struct btrfs_item *)btrfs_item_nr_offset(nr);
}

static inline u32 btrfs_item_end(struct extent_buffer *eb,
				 struct btrfs_item *item)
{
	return btrfs_item_offset(eb, item) + btrfs_item_size(eb, item);
}

static inline u32 btrfs_item_end_nr(struct extent_buffer *eb, int nr)
{
	return btrfs_item_end(eb, btrfs_item_nr(eb, nr));
}

static inline u32 btrfs_item_offset_nr(struct extent_buffer *eb, int nr)
{
	return btrfs_item_offset(eb, btrfs_item_nr(eb, nr));
}

static inline u32 btrfs_item_size_nr(struct extent_buffer *eb, int nr)
{
	return btrfs_item_size(eb, btrfs_item_nr(eb, nr));
}

static inline void btrfs_item_key(struct extent_buffer *eb,
			   struct btrfs_disk_key *disk_key, int nr)
{
	struct btrfs_item *item = btrfs_item_nr(eb, nr);
	read_eb_member(eb, item, struct btrfs_item, key, disk_key);
}

static inline void btrfs_set_item_key(struct extent_buffer *eb,
			       struct btrfs_disk_key *disk_key, int nr)
{
	struct btrfs_item *item = btrfs_item_nr(eb, nr);
	write_eb_member(eb, item, struct btrfs_item, key, disk_key);
}

BTRFS_SETGET_FUNCS(dir_log_end, struct btrfs_dir_log_item, end, 64);

BTRFS_SETGET_FUNCS(root_ref_dirid, struct btrfs_root_ref, dirid, 64);
BTRFS_SETGET_FUNCS(root_ref_sequence, struct btrfs_root_ref, sequence, 64);
BTRFS_SETGET_FUNCS(root_ref_name_len, struct btrfs_root_ref, name_len, 16);

BTRFS_SETGET_FUNCS(dir_data_len, struct btrfs_dir_item, data_len, 16);
BTRFS_SETGET_FUNCS(dir_type, struct btrfs_dir_item, type, 8);
BTRFS_SETGET_FUNCS(dir_name_len, struct btrfs_dir_item, name_len, 16);
BTRFS_SETGET_FUNCS(dir_transid, struct btrfs_dir_item, transid, 64);

static inline void btrfs_dir_item_key(struct extent_buffer *eb,
				      struct btrfs_dir_item *item,
				      struct btrfs_disk_key *key)
{
	read_eb_member(eb, item, struct btrfs_dir_item, location, key);
}

static inline void btrfs_set_dir_item_key(struct extent_buffer *eb,
					  struct btrfs_dir_item *item,
					  struct btrfs_disk_key *key)
{
	write_eb_member(eb, item, struct btrfs_dir_item, location, key);
}

BTRFS_SETGET_FUNCS(free_space_entries, struct btrfs_free_space_header,
		   num_entries, 64);
BTRFS_SETGET_FUNCS(free_space_bitmaps, struct btrfs_free_space_header,
		   num_bitmaps, 64);
BTRFS_SETGET_FUNCS(free_space_generation, struct btrfs_free_space_header,
		   generation, 64);

static inline void btrfs_free_space_key(struct extent_buffer *eb,
					struct btrfs_free_space_header *h,
					struct btrfs_disk_key *key)
{
	read_eb_member(eb, h, struct btrfs_free_space_header, location, key);
}

static inline void btrfs_set_free_space_key(struct extent_buffer *eb,
					    struct btrfs_free_space_header *h,
					    struct btrfs_disk_key *key)
{
	write_eb_member(eb, h, struct btrfs_free_space_header, location, key);
}

BTRFS_SETGET_STACK_FUNCS(disk_key_objectid, struct btrfs_disk_key,
			 objectid, 64);
BTRFS_SETGET_STACK_FUNCS(disk_key_offset, struct btrfs_disk_key, offset, 64);
BTRFS_SETGET_STACK_FUNCS(disk_key_type, struct btrfs_disk_key, type, 8);

static inline void btrfs_disk_key_to_cpu(struct btrfs_key *cpu,
					 struct btrfs_disk_key *disk)
{
	cpu->offset = le64_to_cpu(disk->offset);
	cpu->type = disk->type;
	cpu->objectid = le64_to_cpu(disk->objectid);
}

static inline void btrfs_cpu_key_to_disk(struct btrfs_disk_key *disk,
					 struct btrfs_key *cpu)
{
	disk->offset = cpu_to_le64(cpu->offset);
	disk->type = cpu->type;
	disk->objectid = cpu_to_le64(cpu->objectid);
}

static inline void btrfs_node_key_to_cpu(struct extent_buffer *eb,
				  struct btrfs_key *key, int nr)
{
	struct btrfs_disk_key disk_key;
	btrfs_node_key(eb, &disk_key, nr);
	btrfs_disk_key_to_cpu(key, &disk_key);
}

static inline void btrfs_item_key_to_cpu(struct extent_buffer *eb,
				  struct btrfs_key *key, int nr)
{
	struct btrfs_disk_key disk_key;
	btrfs_item_key(eb, &disk_key, nr);
	btrfs_disk_key_to_cpu(key, &disk_key);
}

static inline void btrfs_dir_item_key_to_cpu(struct extent_buffer *eb,
				      struct btrfs_dir_item *item,
				      struct btrfs_key *key)
{
	struct btrfs_disk_key disk_key;
	btrfs_dir_item_key(eb, item, &disk_key);
	btrfs_disk_key_to_cpu(key, &disk_key);
}


static inline u8 btrfs_key_type(struct btrfs_key *key)
{
	return key->type;
}

static inline void btrfs_set_key_type(struct btrfs_key *key, u8 val)
{
	key->type = val;
}

BTRFS_SETGET_HEADER_FUNCS(header_bytenr, struct btrfs_header, bytenr, 64);
BTRFS_SETGET_HEADER_FUNCS(header_generation, struct btrfs_header,
			  generation, 64);
BTRFS_SETGET_HEADER_FUNCS(header_owner, struct btrfs_header, owner, 64);
BTRFS_SETGET_HEADER_FUNCS(header_nritems, struct btrfs_header, nritems, 32);
BTRFS_SETGET_HEADER_FUNCS(header_flags, struct btrfs_header, flags, 64);
BTRFS_SETGET_HEADER_FUNCS(header_level, struct btrfs_header, level, 8);

static inline int btrfs_header_flag(struct extent_buffer *eb, u64 flag)
{
	return (btrfs_header_flags(eb) & flag) == flag;
}

static inline int btrfs_set_header_flag(struct extent_buffer *eb, u64 flag)
{
	u64 flags = btrfs_header_flags(eb);
	btrfs_set_header_flags(eb, flags | flag);
	return (flags & flag) == flag;
}

static inline int btrfs_clear_header_flag(struct extent_buffer *eb, u64 flag)
{
	u64 flags = btrfs_header_flags(eb);
	btrfs_set_header_flags(eb, flags & ~flag);
	return (flags & flag) == flag;
}

static inline int btrfs_header_backref_rev(struct extent_buffer *eb)
{
	u64 flags = btrfs_header_flags(eb);
	return flags >> BTRFS_BACKREF_REV_SHIFT;
}

static inline void btrfs_set_header_backref_rev(struct extent_buffer *eb,
						int rev)
{
	u64 flags = btrfs_header_flags(eb);
	flags &= ~BTRFS_BACKREF_REV_MASK;
	flags |= (u64)rev << BTRFS_BACKREF_REV_SHIFT;
	btrfs_set_header_flags(eb, flags);
}

static inline u8 *btrfs_header_fsid(struct extent_buffer *eb)
{
	unsigned long ptr = offsetof(struct btrfs_header, fsid);
	return (u8 *)ptr;
}

static inline u8 *btrfs_header_chunk_tree_uuid(struct extent_buffer *eb)
{
	unsigned long ptr = offsetof(struct btrfs_header, chunk_tree_uuid);
	return (u8 *)ptr;
}

static inline int btrfs_is_leaf(struct extent_buffer *eb)
{
	return btrfs_header_level(eb) == 0;
}

BTRFS_SETGET_FUNCS(disk_root_generation, struct btrfs_root_item,
		   generation, 64);
BTRFS_SETGET_FUNCS(disk_root_refs, struct btrfs_root_item, refs, 32);
BTRFS_SETGET_FUNCS(disk_root_bytenr, struct btrfs_root_item, bytenr, 64);
BTRFS_SETGET_FUNCS(disk_root_level, struct btrfs_root_item, level, 8);

BTRFS_SETGET_STACK_FUNCS(root_generation, struct btrfs_root_item,
			 generation, 64);
BTRFS_SETGET_STACK_FUNCS(root_bytenr, struct btrfs_root_item, bytenr, 64);
BTRFS_SETGET_STACK_FUNCS(root_level, struct btrfs_root_item, level, 8);
BTRFS_SETGET_STACK_FUNCS(root_dirid, struct btrfs_root_item, root_dirid, 64);
BTRFS_SETGET_STACK_FUNCS(root_refs, struct btrfs_root_item, refs, 32);
BTRFS_SETGET_STACK_FUNCS(root_flags, struct btrfs_root_item, flags, 64);
BTRFS_SETGET_STACK_FUNCS(root_used, struct btrfs_root_item, bytes_used, 64);
BTRFS_SETGET_STACK_FUNCS(root_limit, struct btrfs_root_item, byte_limit, 64);
BTRFS_SETGET_STACK_FUNCS(root_last_snapshot, struct btrfs_root_item,
			 last_snapshot, 64);

static inline bool btrfs_root_readonly(struct btrfs_root *root)
{
	return (root->root_item.flags & cpu_to_le64(BTRFS_ROOT_SUBVOL_RDONLY)) != 0;
}

BTRFS_SETGET_STACK_FUNCS(backup_tree_root, struct btrfs_root_backup,
		   tree_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_tree_root_gen, struct btrfs_root_backup,
		   tree_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_tree_root_level, struct btrfs_root_backup,
		   tree_root_level, 8);

BTRFS_SETGET_STACK_FUNCS(backup_chunk_root, struct btrfs_root_backup,
		   chunk_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_chunk_root_gen, struct btrfs_root_backup,
		   chunk_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_chunk_root_level, struct btrfs_root_backup,
		   chunk_root_level, 8);

BTRFS_SETGET_STACK_FUNCS(backup_extent_root, struct btrfs_root_backup,
		   extent_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_extent_root_gen, struct btrfs_root_backup,
		   extent_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_extent_root_level, struct btrfs_root_backup,
		   extent_root_level, 8);

BTRFS_SETGET_STACK_FUNCS(backup_fs_root, struct btrfs_root_backup,
		   fs_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_fs_root_gen, struct btrfs_root_backup,
		   fs_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_fs_root_level, struct btrfs_root_backup,
		   fs_root_level, 8);

BTRFS_SETGET_STACK_FUNCS(backup_dev_root, struct btrfs_root_backup,
		   dev_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_dev_root_gen, struct btrfs_root_backup,
		   dev_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_dev_root_level, struct btrfs_root_backup,
		   dev_root_level, 8);

BTRFS_SETGET_STACK_FUNCS(backup_csum_root, struct btrfs_root_backup,
		   csum_root, 64);
BTRFS_SETGET_STACK_FUNCS(backup_csum_root_gen, struct btrfs_root_backup,
		   csum_root_gen, 64);
BTRFS_SETGET_STACK_FUNCS(backup_csum_root_level, struct btrfs_root_backup,
		   csum_root_level, 8);
BTRFS_SETGET_STACK_FUNCS(backup_total_bytes, struct btrfs_root_backup,
		   total_bytes, 64);
BTRFS_SETGET_STACK_FUNCS(backup_bytes_used, struct btrfs_root_backup,
		   bytes_used, 64);
BTRFS_SETGET_STACK_FUNCS(backup_num_devices, struct btrfs_root_backup,
		   num_devices, 64);

BTRFS_SETGET_FUNCS(balance_flags, struct btrfs_balance_item, flags, 64);

static inline void btrfs_balance_data(struct extent_buffer *eb,
				      struct btrfs_balance_item *bi,
				      struct btrfs_disk_balance_args *ba)
{
	read_eb_member(eb, bi, struct btrfs_balance_item, data, ba);
}

static inline void btrfs_set_balance_data(struct extent_buffer *eb,
					  struct btrfs_balance_item *bi,
					  struct btrfs_disk_balance_args *ba)
{
	write_eb_member(eb, bi, struct btrfs_balance_item, data, ba);
}

static inline void btrfs_balance_meta(struct extent_buffer *eb,
				      struct btrfs_balance_item *bi,
				      struct btrfs_disk_balance_args *ba)
{
	read_eb_member(eb, bi, struct btrfs_balance_item, meta, ba);
}

static inline void btrfs_set_balance_meta(struct extent_buffer *eb,
					  struct btrfs_balance_item *bi,
					  struct btrfs_disk_balance_args *ba)
{
	write_eb_member(eb, bi, struct btrfs_balance_item, meta, ba);
}

static inline void btrfs_balance_sys(struct extent_buffer *eb,
				     struct btrfs_balance_item *bi,
				     struct btrfs_disk_balance_args *ba)
{
	read_eb_member(eb, bi, struct btrfs_balance_item, sys, ba);
}

static inline void btrfs_set_balance_sys(struct extent_buffer *eb,
					 struct btrfs_balance_item *bi,
					 struct btrfs_disk_balance_args *ba)
{
	write_eb_member(eb, bi, struct btrfs_balance_item, sys, ba);
}

static inline void
btrfs_disk_balance_args_to_cpu(struct btrfs_balance_args *cpu,
			       struct btrfs_disk_balance_args *disk)
{
	memset(cpu, 0, sizeof(*cpu));

	cpu->profiles = le64_to_cpu(disk->profiles);
	cpu->usage = le64_to_cpu(disk->usage);
	cpu->devid = le64_to_cpu(disk->devid);
	cpu->pstart = le64_to_cpu(disk->pstart);
	cpu->pend = le64_to_cpu(disk->pend);
	cpu->vstart = le64_to_cpu(disk->vstart);
	cpu->vend = le64_to_cpu(disk->vend);
	cpu->target = le64_to_cpu(disk->target);
	cpu->flags = le64_to_cpu(disk->flags);
}

static inline void
btrfs_cpu_balance_args_to_disk(struct btrfs_disk_balance_args *disk,
			       struct btrfs_balance_args *cpu)
{
	memset(disk, 0, sizeof(*disk));

	disk->profiles = cpu_to_le64(cpu->profiles);
	disk->usage = cpu_to_le64(cpu->usage);
	disk->devid = cpu_to_le64(cpu->devid);
	disk->pstart = cpu_to_le64(cpu->pstart);
	disk->pend = cpu_to_le64(cpu->pend);
	disk->vstart = cpu_to_le64(cpu->vstart);
	disk->vend = cpu_to_le64(cpu->vend);
	disk->target = cpu_to_le64(cpu->target);
	disk->flags = cpu_to_le64(cpu->flags);
}

BTRFS_SETGET_STACK_FUNCS(super_bytenr, struct btrfs_super_block, bytenr, 64);
BTRFS_SETGET_STACK_FUNCS(super_flags, struct btrfs_super_block, flags, 64);
BTRFS_SETGET_STACK_FUNCS(super_generation, struct btrfs_super_block,
			 generation, 64);
BTRFS_SETGET_STACK_FUNCS(super_root, struct btrfs_super_block, root, 64);
BTRFS_SETGET_STACK_FUNCS(super_sys_array_size,
			 struct btrfs_super_block, sys_chunk_array_size, 32);
BTRFS_SETGET_STACK_FUNCS(super_chunk_root_generation,
			 struct btrfs_super_block, chunk_root_generation, 64);
BTRFS_SETGET_STACK_FUNCS(super_root_level, struct btrfs_super_block,
			 root_level, 8);
BTRFS_SETGET_STACK_FUNCS(super_chunk_root, struct btrfs_super_block,
			 chunk_root, 64);
BTRFS_SETGET_STACK_FUNCS(super_chunk_root_level, struct btrfs_super_block,
			 chunk_root_level, 8);
BTRFS_SETGET_STACK_FUNCS(super_log_root, struct btrfs_super_block,
			 log_root, 64);
BTRFS_SETGET_STACK_FUNCS(super_log_root_transid, struct btrfs_super_block,
			 log_root_transid, 64);
BTRFS_SETGET_STACK_FUNCS(super_log_root_level, struct btrfs_super_block,
			 log_root_level, 8);
BTRFS_SETGET_STACK_FUNCS(super_total_bytes, struct btrfs_super_block,
			 total_bytes, 64);
BTRFS_SETGET_STACK_FUNCS(super_bytes_used, struct btrfs_super_block,
			 bytes_used, 64);
BTRFS_SETGET_STACK_FUNCS(super_sectorsize, struct btrfs_super_block,
			 sectorsize, 32);
BTRFS_SETGET_STACK_FUNCS(super_nodesize, struct btrfs_super_block,
			 nodesize, 32);
BTRFS_SETGET_STACK_FUNCS(super_leafsize, struct btrfs_super_block,
			 leafsize, 32);
BTRFS_SETGET_STACK_FUNCS(super_stripesize, struct btrfs_super_block,
			 stripesize, 32);
BTRFS_SETGET_STACK_FUNCS(super_root_dir, struct btrfs_super_block,
			 root_dir_objectid, 64);
BTRFS_SETGET_STACK_FUNCS(super_num_devices, struct btrfs_super_block,
			 num_devices, 64);
BTRFS_SETGET_STACK_FUNCS(super_compat_flags, struct btrfs_super_block,
			 compat_flags, 64);
BTRFS_SETGET_STACK_FUNCS(super_compat_ro_flags, struct btrfs_super_block,
			 compat_ro_flags, 64);
BTRFS_SETGET_STACK_FUNCS(super_incompat_flags, struct btrfs_super_block,
			 incompat_flags, 64);
BTRFS_SETGET_STACK_FUNCS(super_csum_type, struct btrfs_super_block,
			 csum_type, 16);
BTRFS_SETGET_STACK_FUNCS(super_cache_generation, struct btrfs_super_block,
			 cache_generation, 64);

static inline int btrfs_super_csum_size(struct btrfs_super_block *s)
{
	int t = btrfs_super_csum_type(s);
	BUG_ON(t >= ARRAY_SIZE(btrfs_csum_sizes));
	return btrfs_csum_sizes[t];
}

static inline unsigned long btrfs_leaf_data(struct extent_buffer *l)
{
	return offsetof(struct btrfs_leaf, items);
}

BTRFS_SETGET_FUNCS(file_extent_type, struct btrfs_file_extent_item, type, 8);

static inline unsigned long
btrfs_file_extent_inline_start(struct btrfs_file_extent_item *e)
{
	unsigned long offset = (unsigned long)e;
	offset += offsetof(struct btrfs_file_extent_item, disk_bytenr);
	return offset;
}

static inline u32 btrfs_file_extent_calc_inline_size(u32 datasize)
{
	return offsetof(struct btrfs_file_extent_item, disk_bytenr) + datasize;
}

BTRFS_SETGET_FUNCS(file_extent_disk_bytenr, struct btrfs_file_extent_item,
		   disk_bytenr, 64);
BTRFS_SETGET_FUNCS(file_extent_generation, struct btrfs_file_extent_item,
		   generation, 64);
BTRFS_SETGET_FUNCS(file_extent_disk_num_bytes, struct btrfs_file_extent_item,
		   disk_num_bytes, 64);
BTRFS_SETGET_FUNCS(file_extent_offset, struct btrfs_file_extent_item,
		  offset, 64);
BTRFS_SETGET_FUNCS(file_extent_num_bytes, struct btrfs_file_extent_item,
		   num_bytes, 64);
BTRFS_SETGET_FUNCS(file_extent_ram_bytes, struct btrfs_file_extent_item,
		   ram_bytes, 64);
BTRFS_SETGET_FUNCS(file_extent_compression, struct btrfs_file_extent_item,
		   compression, 8);
BTRFS_SETGET_FUNCS(file_extent_encryption, struct btrfs_file_extent_item,
		   encryption, 8);
BTRFS_SETGET_FUNCS(file_extent_other_encoding, struct btrfs_file_extent_item,
		   other_encoding, 16);

static inline u32 btrfs_file_extent_inline_len(struct extent_buffer *eb,
					       struct btrfs_file_extent_item *e)
{
	return btrfs_file_extent_ram_bytes(eb, e);
}

static inline u32 btrfs_file_extent_inline_item_len(struct extent_buffer *eb,
						    struct btrfs_item *e)
{
	unsigned long offset;
	offset = offsetof(struct btrfs_file_extent_item, disk_bytenr);
	return btrfs_item_size(eb, e) - offset;
}

static inline struct btrfs_fs_info *btrfs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline u32 btrfs_level_size(struct btrfs_root *root, int level)
{
	if (level == 0)
		return root->leafsize;
	return root->nodesize;
}

#define btrfs_item_ptr(leaf, slot, type) \
	((type *)(btrfs_leaf_data(leaf) + \
	btrfs_item_offset_nr(leaf, slot)))

#define btrfs_item_ptr_offset(leaf, slot) \
	((unsigned long)(btrfs_leaf_data(leaf) + \
	btrfs_item_offset_nr(leaf, slot)))

static inline struct dentry *fdentry(struct file *file)
{
	return file->f_path.dentry;
}

static inline bool btrfs_mixed_space_info(struct btrfs_space_info *space_info)
{
	return ((space_info->flags & BTRFS_BLOCK_GROUP_METADATA) &&
		(space_info->flags & BTRFS_BLOCK_GROUP_DATA));
}

static inline gfp_t btrfs_alloc_write_mask(struct address_space *mapping)
{
	return mapping_gfp_mask(mapping) & ~__GFP_FS;
}

static inline u64 btrfs_calc_trans_metadata_size(struct btrfs_root *root,
						 unsigned num_items)
{
	return (root->leafsize + root->nodesize * (BTRFS_MAX_LEVEL - 1)) *
		3 * num_items;
}

static inline u64 btrfs_calc_trunc_metadata_size(struct btrfs_root *root,
						 unsigned num_items)
{
	return (root->leafsize + root->nodesize * (BTRFS_MAX_LEVEL - 1)) *
		num_items;
}

void btrfs_put_block_group(struct btrfs_block_group_cache *cache);
int btrfs_run_delayed_refs(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, unsigned long count);
int btrfs_lookup_extent(struct btrfs_root *root, u64 start, u64 len);
int btrfs_lookup_extent_info(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, u64 bytenr,
			     u64 num_bytes, u64 *refs, u64 *flags);
int btrfs_pin_extent(struct btrfs_root *root,
		     u64 bytenr, u64 num, int reserved);
int btrfs_pin_extent_for_log_replay(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    u64 bytenr, u64 num_bytes);
int btrfs_cross_ref_exist(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  u64 objectid, u64 offset, u64 bytenr);
struct btrfs_block_group_cache *btrfs_lookup_block_group(
						 struct btrfs_fs_info *info,
						 u64 bytenr);
void btrfs_put_block_group(struct btrfs_block_group_cache *cache);
u64 btrfs_find_block_group(struct btrfs_root *root,
			   u64 search_start, u64 search_hint, int owner);
struct extent_buffer *btrfs_alloc_free_block(struct btrfs_trans_handle *trans,
					struct btrfs_root *root, u32 blocksize,
					u64 parent, u64 root_objectid,
					struct btrfs_disk_key *key, int level,
					u64 hint, u64 empty_size, int for_cow);
void btrfs_free_tree_block(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct extent_buffer *buf,
			   u64 parent, int last_ref, int for_cow);
struct extent_buffer *btrfs_init_new_buffer(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    u64 bytenr, u32 blocksize,
					    int level);
int btrfs_alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 root_objectid, u64 owner,
				     u64 offset, struct btrfs_key *ins);
int btrfs_alloc_logged_file_extent(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   u64 root_objectid, u64 owner, u64 offset,
				   struct btrfs_key *ins);
int btrfs_reserve_extent(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  u64 num_bytes, u64 min_alloc_size,
				  u64 empty_size, u64 hint_byte,
				  struct btrfs_key *ins, u64 data);
int btrfs_inc_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref, int for_cow);
int btrfs_dec_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref, int for_cow);
int btrfs_set_disk_extent_flags(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 flags,
				int is_data);
int btrfs_free_extent(struct btrfs_trans_handle *trans,
		      struct btrfs_root *root,
		      u64 bytenr, u64 num_bytes, u64 parent, u64 root_objectid,
		      u64 owner, u64 offset, int for_cow);

int btrfs_free_reserved_extent(struct btrfs_root *root, u64 start, u64 len);
int btrfs_free_and_pin_reserved_extent(struct btrfs_root *root,
				       u64 start, u64 len);
void btrfs_prepare_extent_commit(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root);
int btrfs_finish_extent_commit(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root);
int btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 u64 bytenr, u64 num_bytes, u64 parent,
			 u64 root_objectid, u64 owner, u64 offset, int for_cow);

int btrfs_write_dirty_block_groups(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root);
int btrfs_extent_readonly(struct btrfs_root *root, u64 bytenr);
int btrfs_free_block_groups(struct btrfs_fs_info *info);
int btrfs_read_block_groups(struct btrfs_root *root);
int btrfs_can_relocate(struct btrfs_root *root, u64 bytenr);
int btrfs_make_block_group(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, u64 bytes_used,
			   u64 type, u64 chunk_objectid, u64 chunk_offset,
			   u64 size);
int btrfs_remove_block_group(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, u64 group_start);
u64 btrfs_reduce_alloc_profile(struct btrfs_root *root, u64 flags);
u64 btrfs_get_alloc_profile(struct btrfs_root *root, int data);
void btrfs_set_inode_space_info(struct btrfs_root *root, struct inode *ionde);
void btrfs_clear_space_info_full(struct btrfs_fs_info *info);
int btrfs_check_data_free_space(struct inode *inode, u64 bytes);
void btrfs_free_reserved_data_space(struct inode *inode, u64 bytes);
void btrfs_trans_release_metadata(struct btrfs_trans_handle *trans,
				struct btrfs_root *root);
int btrfs_orphan_reserve_metadata(struct btrfs_trans_handle *trans,
				  struct inode *inode);
void btrfs_orphan_release_metadata(struct inode *inode);
int btrfs_snap_reserve_metadata(struct btrfs_trans_handle *trans,
				struct btrfs_pending_snapshot *pending);
int btrfs_delalloc_reserve_metadata(struct inode *inode, u64 num_bytes);
void btrfs_delalloc_release_metadata(struct inode *inode, u64 num_bytes);
int btrfs_delalloc_reserve_space(struct inode *inode, u64 num_bytes);
void btrfs_delalloc_release_space(struct inode *inode, u64 num_bytes);
void btrfs_init_block_rsv(struct btrfs_block_rsv *rsv);
struct btrfs_block_rsv *btrfs_alloc_block_rsv(struct btrfs_root *root);
void btrfs_free_block_rsv(struct btrfs_root *root,
			  struct btrfs_block_rsv *rsv);
int btrfs_block_rsv_add(struct btrfs_root *root,
			struct btrfs_block_rsv *block_rsv,
			u64 num_bytes);
int btrfs_block_rsv_add_noflush(struct btrfs_root *root,
				struct btrfs_block_rsv *block_rsv,
				u64 num_bytes);
int btrfs_block_rsv_check(struct btrfs_root *root,
			  struct btrfs_block_rsv *block_rsv, int min_factor);
int btrfs_block_rsv_refill(struct btrfs_root *root,
			  struct btrfs_block_rsv *block_rsv,
			  u64 min_reserved);
int btrfs_block_rsv_refill_noflush(struct btrfs_root *root,
				   struct btrfs_block_rsv *block_rsv,
				   u64 min_reserved);
int btrfs_block_rsv_migrate(struct btrfs_block_rsv *src_rsv,
			    struct btrfs_block_rsv *dst_rsv,
			    u64 num_bytes);
void btrfs_block_rsv_release(struct btrfs_root *root,
			     struct btrfs_block_rsv *block_rsv,
			     u64 num_bytes);
int btrfs_set_block_group_ro(struct btrfs_root *root,
			     struct btrfs_block_group_cache *cache);
void btrfs_set_block_group_rw(struct btrfs_root *root,
			      struct btrfs_block_group_cache *cache);
void btrfs_put_block_group_cache(struct btrfs_fs_info *info);
u64 btrfs_account_ro_block_groups_free_space(struct btrfs_space_info *sinfo);
int btrfs_error_unpin_extent_range(struct btrfs_root *root,
				   u64 start, u64 end);
int btrfs_error_discard_extent(struct btrfs_root *root, u64 bytenr,
			       u64 num_bytes, u64 *actual_bytes);
int btrfs_force_chunk_alloc(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root, u64 type);
int btrfs_trim_fs(struct btrfs_root *root, struct fstrim_range *range);

int btrfs_init_space_info(struct btrfs_fs_info *fs_info);
int btrfs_bin_search(struct extent_buffer *eb, struct btrfs_key *key,
		     int level, int *slot);
int btrfs_comp_cpu_keys(struct btrfs_key *k1, struct btrfs_key *k2);
int btrfs_previous_item(struct btrfs_root *root,
			struct btrfs_path *path, u64 min_objectid,
			int type);
void btrfs_set_item_key_safe(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, struct btrfs_path *path,
			     struct btrfs_key *new_key);
struct extent_buffer *btrfs_root_node(struct btrfs_root *root);
struct extent_buffer *btrfs_lock_root_node(struct btrfs_root *root);
int btrfs_find_next_key(struct btrfs_root *root, struct btrfs_path *path,
			struct btrfs_key *key, int lowest_level,
			int cache_only, u64 min_trans);
int btrfs_search_forward(struct btrfs_root *root, struct btrfs_key *min_key,
			 struct btrfs_key *max_key,
			 struct btrfs_path *path, int cache_only,
			 u64 min_trans);
int btrfs_cow_block(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, struct extent_buffer *buf,
		    struct extent_buffer *parent, int parent_slot,
		    struct extent_buffer **cow_ret);
int btrfs_copy_root(struct btrfs_trans_handle *trans,
		      struct btrfs_root *root,
		      struct extent_buffer *buf,
		      struct extent_buffer **cow_ret, u64 new_root_objectid);
int btrfs_block_can_be_shared(struct btrfs_root *root,
			      struct extent_buffer *buf);
void btrfs_extend_item(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct btrfs_path *path,
		       u32 data_size);
void btrfs_truncate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 struct btrfs_path *path,
			 u32 new_size, int from_end);
int btrfs_split_item(struct btrfs_trans_handle *trans,
		     struct btrfs_root *root,
		     struct btrfs_path *path,
		     struct btrfs_key *new_key,
		     unsigned long split_offset);
int btrfs_duplicate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 struct btrfs_path *path,
			 struct btrfs_key *new_key);
int btrfs_search_slot(struct btrfs_trans_handle *trans, struct btrfs_root
		      *root, struct btrfs_key *key, struct btrfs_path *p, int
		      ins_len, int cow);
int btrfs_realloc_node(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct extent_buffer *parent,
		       int start_slot, int cache_only, u64 *last_ret,
		       struct btrfs_key *progress);
void btrfs_release_path(struct btrfs_path *p);
struct btrfs_path *btrfs_alloc_path(void);
void btrfs_free_path(struct btrfs_path *p);
void btrfs_set_path_blocking(struct btrfs_path *p);
void btrfs_clear_path_blocking(struct btrfs_path *p,
			       struct extent_buffer *held, int held_rw);
void btrfs_unlock_up_safe(struct btrfs_path *p, int level);

int btrfs_del_items(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		   struct btrfs_path *path, int slot, int nr);
static inline int btrfs_del_item(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path)
{
	return btrfs_del_items(trans, root, path, path->slots[0], 1);
}

void setup_items_for_insert(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root, struct btrfs_path *path,
			    struct btrfs_key *cpu_key, u32 *data_size,
			    u32 total_data, u32 total_size, int nr);
int btrfs_insert_item(struct btrfs_trans_handle *trans, struct btrfs_root
		      *root, struct btrfs_key *key, void *data, u32 data_size);
int btrfs_insert_empty_items(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path,
			     struct btrfs_key *cpu_key, u32 *data_size, int nr);

static inline int btrfs_insert_empty_item(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  struct btrfs_key *key,
					  u32 data_size)
{
	return btrfs_insert_empty_items(trans, root, path, key, &data_size, 1);
}

int btrfs_next_leaf(struct btrfs_root *root, struct btrfs_path *path);
static inline int btrfs_next_item(struct btrfs_root *root, struct btrfs_path *p)
{
	++p->slots[0];
	if (p->slots[0] >= btrfs_header_nritems(p->nodes[0]))
		return btrfs_next_leaf(root, p);
	return 0;
}
int btrfs_prev_leaf(struct btrfs_root *root, struct btrfs_path *path);
int btrfs_leaf_free_space(struct btrfs_root *root, struct extent_buffer *leaf);
int __must_check btrfs_drop_snapshot(struct btrfs_root *root,
				     struct btrfs_block_rsv *block_rsv,
				     int update_ref, int for_reloc);
int btrfs_drop_subtree(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct extent_buffer *node,
			struct extent_buffer *parent);
static inline int btrfs_fs_closing(struct btrfs_fs_info *fs_info)
{
	smp_mb();
	return fs_info->closing;
}
static inline void free_fs_info(struct btrfs_fs_info *fs_info)
{
	kfree(fs_info->balance_ctl);
	kfree(fs_info->delayed_root);
	kfree(fs_info->extent_root);
	kfree(fs_info->tree_root);
	kfree(fs_info->chunk_root);
	kfree(fs_info->dev_root);
	kfree(fs_info->csum_root);
	kfree(fs_info->super_copy);
	kfree(fs_info->super_for_commit);
	kfree(fs_info);
}

int btrfs_find_root_ref(struct btrfs_root *tree_root,
			struct btrfs_path *path,
			u64 root_id, u64 ref_id);
int btrfs_add_root_ref(struct btrfs_trans_handle *trans,
		       struct btrfs_root *tree_root,
		       u64 root_id, u64 ref_id, u64 dirid, u64 sequence,
		       const char *name, int name_len);
int btrfs_del_root_ref(struct btrfs_trans_handle *trans,
		       struct btrfs_root *tree_root,
		       u64 root_id, u64 ref_id, u64 dirid, u64 *sequence,
		       const char *name, int name_len);
int btrfs_del_root(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		   struct btrfs_key *key);
int btrfs_insert_root(struct btrfs_trans_handle *trans, struct btrfs_root
		      *root, struct btrfs_key *key, struct btrfs_root_item
		      *item);
int __must_check btrfs_update_root(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_key *key,
				   struct btrfs_root_item *item);
int btrfs_find_last_root(struct btrfs_root *root, u64 objectid, struct
			 btrfs_root_item *item, struct btrfs_key *key);
int btrfs_find_dead_roots(struct btrfs_root *root, u64 objectid);
int btrfs_find_orphan_roots(struct btrfs_root *tree_root);
void btrfs_set_root_node(struct btrfs_root_item *item,
			 struct extent_buffer *node);
void btrfs_check_and_init_root_item(struct btrfs_root_item *item);

int btrfs_insert_dir_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root, const char *name,
			  int name_len, struct inode *dir,
			  struct btrfs_key *location, u8 type, u64 index);
struct btrfs_dir_item *btrfs_lookup_dir_item(struct btrfs_trans_handle *trans,
					     struct btrfs_root *root,
					     struct btrfs_path *path, u64 dir,
					     const char *name, int name_len,
					     int mod);
struct btrfs_dir_item *
btrfs_lookup_dir_index_item(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root,
			    struct btrfs_path *path, u64 dir,
			    u64 objectid, const char *name, int name_len,
			    int mod);
struct btrfs_dir_item *
btrfs_search_dir_index_item(struct btrfs_root *root,
			    struct btrfs_path *path, u64 dirid,
			    const char *name, int name_len);
struct btrfs_dir_item *btrfs_match_dir_item_name(struct btrfs_root *root,
			      struct btrfs_path *path,
			      const char *name, int name_len);
int btrfs_delete_one_dir_name(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct btrfs_path *path,
			      struct btrfs_dir_item *di);
int btrfs_insert_xattr_item(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root,
			    struct btrfs_path *path, u64 objectid,
			    const char *name, u16 name_len,
			    const void *data, u16 data_len);
struct btrfs_dir_item *btrfs_lookup_xattr(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path, u64 dir,
					  const char *name, u16 name_len,
					  int mod);
int verify_dir_item(struct btrfs_root *root,
		    struct extent_buffer *leaf,
		    struct btrfs_dir_item *dir_item);

int btrfs_insert_orphan_item(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root, u64 offset);
int btrfs_del_orphan_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root, u64 offset);
int btrfs_find_orphan_item(struct btrfs_root *root, u64 offset);

int btrfs_insert_inode_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   const char *name, int name_len,
			   u64 inode_objectid, u64 ref_objectid, u64 index);
int btrfs_del_inode_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   const char *name, int name_len,
			   u64 inode_objectid, u64 ref_objectid, u64 *index);
struct btrfs_inode_ref *
btrfs_lookup_inode_ref(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct btrfs_path *path,
			const char *name, int name_len,
			u64 inode_objectid, u64 ref_objectid, int mod);
int btrfs_insert_empty_inode(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid);
int btrfs_lookup_inode(struct btrfs_trans_handle *trans, struct btrfs_root
		       *root, struct btrfs_path *path,
		       struct btrfs_key *location, int mod);

int btrfs_del_csums(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, u64 bytenr, u64 len);
int btrfs_lookup_bio_sums(struct btrfs_root *root, struct inode *inode,
			  struct bio *bio, u32 *dst);
int btrfs_lookup_bio_sums_dio(struct btrfs_root *root, struct inode *inode,
			      struct bio *bio, u64 logical_offset, u32 *dst);
int btrfs_insert_file_extent(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     u64 objectid, u64 pos,
			     u64 disk_offset, u64 disk_num_bytes,
			     u64 num_bytes, u64 offset, u64 ram_bytes,
			     u8 compression, u8 encryption, u16 other_encoding);
int btrfs_lookup_file_extent(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid,
			     u64 bytenr, int mod);
int btrfs_csum_file_blocks(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct btrfs_ordered_sum *sums);
int btrfs_csum_one_bio(struct btrfs_root *root, struct inode *inode,
		       struct bio *bio, u64 file_start, int contig);
struct btrfs_csum_item *btrfs_lookup_csum(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  u64 bytenr, int cow);
int btrfs_csum_truncate(struct btrfs_trans_handle *trans,
			struct btrfs_root *root, struct btrfs_path *path,
			u64 isize);
int btrfs_lookup_csums_range(struct btrfs_root *root, u64 start, u64 end,
			     struct list_head *list, int search_commit);
struct extent_map *btrfs_get_extent_fiemap(struct inode *inode, struct page *page,
					   size_t pg_offset, u64 start, u64 len,
					   int create);

#if defined(ClearPageFsMisc) && !defined(ClearPageChecked)
#define ClearPageChecked ClearPageFsMisc
#define SetPageChecked SetPageFsMisc
#define PageChecked PageFsMisc
#endif

static inline void btrfs_force_ra(struct address_space *mapping,
				  struct file_ra_state *ra, struct file *file,
				  pgoff_t offset, unsigned long req_size)
{
	page_cache_sync_readahead(mapping, ra, file, offset, req_size);
}

struct inode *btrfs_lookup_dentry(struct inode *dir, struct dentry *dentry);
int btrfs_set_inode_index(struct inode *dir, u64 *index);
int btrfs_unlink_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root,
		       struct inode *dir, struct inode *inode,
		       const char *name, int name_len);
int btrfs_add_link(struct btrfs_trans_handle *trans,
		   struct inode *parent_inode, struct inode *inode,
		   const char *name, int name_len, int add_backref, u64 index);
int btrfs_unlink_subvol(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct inode *dir, u64 objectid,
			const char *name, int name_len);
int btrfs_truncate_inode_items(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct inode *inode, u64 new_size,
			       u32 min_type);

int btrfs_start_delalloc_inodes(struct btrfs_root *root, int delay_iput);
int btrfs_set_extent_delalloc(struct inode *inode, u64 start, u64 end,
			      struct extent_state **cached_state);
int btrfs_writepages(struct address_space *mapping,
		     struct writeback_control *wbc);
int btrfs_create_subvol_root(struct btrfs_trans_handle *trans,
			     struct btrfs_root *new_root, u64 new_dirid);
int btrfs_merge_bio_hook(struct page *page, unsigned long offset,
			 size_t size, struct bio *bio, unsigned long bio_flags);

int btrfs_page_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf);
int btrfs_readpage(struct file *file, struct page *page);
void btrfs_evict_inode(struct inode *inode);
int btrfs_write_inode(struct inode *inode, struct writeback_control *wbc);
int btrfs_dirty_inode(struct inode *inode);
int btrfs_update_time(struct file *file);
struct inode *btrfs_alloc_inode(struct super_block *sb);
void btrfs_destroy_inode(struct inode *inode);
int btrfs_drop_inode(struct inode *inode);
int btrfs_init_cachep(void);
void btrfs_destroy_cachep(void);
long btrfs_ioctl_trans_end(struct file *file);
struct inode *btrfs_iget(struct super_block *s, struct btrfs_key *location,
			 struct btrfs_root *root, int *was_new);
struct extent_map *btrfs_get_extent(struct inode *inode, struct page *page,
				    size_t pg_offset, u64 start, u64 end,
				    int create);
int btrfs_update_inode(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct inode *inode);
int btrfs_orphan_add(struct btrfs_trans_handle *trans, struct inode *inode);
int btrfs_orphan_del(struct btrfs_trans_handle *trans, struct inode *inode);
int btrfs_orphan_cleanup(struct btrfs_root *root);
void btrfs_orphan_commit_root(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root);
int btrfs_cont_expand(struct inode *inode, loff_t oldsize, loff_t size);
void btrfs_invalidate_inodes(struct btrfs_root *root);
void btrfs_add_delayed_iput(struct inode *inode);
void btrfs_run_delayed_iputs(struct btrfs_root *root);
int btrfs_prealloc_file_range(struct inode *inode, int mode,
			      u64 start, u64 num_bytes, u64 min_size,
			      loff_t actual_len, u64 *alloc_hint);
int btrfs_prealloc_file_range_trans(struct inode *inode,
				    struct btrfs_trans_handle *trans, int mode,
				    u64 start, u64 num_bytes, u64 min_size,
				    loff_t actual_len, u64 *alloc_hint);
extern const struct dentry_operations btrfs_dentry_operations;

long btrfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
void btrfs_update_iflags(struct inode *inode);
void btrfs_inherit_iflags(struct inode *inode, struct inode *dir);
int btrfs_defrag_file(struct inode *inode, struct file *file,
		      struct btrfs_ioctl_defrag_range_args *range,
		      u64 newer_than, unsigned long max_pages);
int btrfs_add_inode_defrag(struct btrfs_trans_handle *trans,
			   struct inode *inode);
int btrfs_run_defrag_inodes(struct btrfs_fs_info *fs_info);
int btrfs_sync_file(struct file *file, loff_t start, loff_t end, int datasync);
int btrfs_drop_extent_cache(struct inode *inode, u64 start, u64 end,
			    int skip_pinned);
extern const struct file_operations btrfs_file_operations;
int btrfs_drop_extents(struct btrfs_trans_handle *trans, struct inode *inode,
		       u64 start, u64 end, u64 *hint_byte, int drop_cache);
int btrfs_mark_extent_written(struct btrfs_trans_handle *trans,
			      struct inode *inode, u64 start, u64 end);
int btrfs_release_file(struct inode *inode, struct file *file);
void btrfs_drop_pages(struct page **pages, size_t num_pages);
int btrfs_dirty_pages(struct btrfs_root *root, struct inode *inode,
		      struct page **pages, size_t num_pages,
		      loff_t pos, size_t write_bytes,
		      struct extent_state **cached);

int btrfs_defrag_leaves(struct btrfs_trans_handle *trans,
			struct btrfs_root *root, int cache_only);

int btrfs_init_sysfs(void);
void btrfs_exit_sysfs(void);

ssize_t btrfs_listxattr(struct dentry *dentry, char *buffer, size_t size);

int btrfs_parse_options(struct btrfs_root *root, char *options);
int btrfs_sync_fs(struct super_block *sb, int wait);
void btrfs_printk(struct btrfs_fs_info *fs_info, const char *fmt, ...);
void __btrfs_std_error(struct btrfs_fs_info *fs_info, const char *function,
		     unsigned int line, int errno, const char *fmt, ...);

void __btrfs_abort_transaction(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root, const char *function,
			       unsigned int line, int errno);

#define btrfs_abort_transaction(trans, root, errno)		\
do {								\
	__btrfs_abort_transaction(trans, root, __func__,	\
				  __LINE__, errno);		\
} while (0)

#define btrfs_std_error(fs_info, errno)				\
do {								\
	if ((errno))						\
		__btrfs_std_error((fs_info), __func__,		\
				   __LINE__, (errno), NULL);	\
} while (0)

#define btrfs_error(fs_info, errno, fmt, args...)		\
do {								\
	__btrfs_std_error((fs_info), __func__, __LINE__,	\
			  (errno), fmt, ##args);		\
} while (0)

void __btrfs_panic(struct btrfs_fs_info *fs_info, const char *function,
		   unsigned int line, int errno, const char *fmt, ...);

#define btrfs_panic(fs_info, errno, fmt, args...)			\
do {									\
	struct btrfs_fs_info *_i = (fs_info);				\
	__btrfs_panic(_i, __func__, __LINE__, errno, fmt, ##args);	\
	BUG_ON(!(_i->mount_opt & BTRFS_MOUNT_PANIC_ON_FATAL_ERROR));	\
} while (0)

#ifdef CONFIG_BTRFS_FS_POSIX_ACL
struct posix_acl *btrfs_get_acl(struct inode *inode, int type);
int btrfs_init_acl(struct btrfs_trans_handle *trans,
		   struct inode *inode, struct inode *dir);
int btrfs_acl_chmod(struct inode *inode);
#else
#define btrfs_get_acl NULL
static inline int btrfs_init_acl(struct btrfs_trans_handle *trans,
				 struct inode *inode, struct inode *dir)
{
	return 0;
}
static inline int btrfs_acl_chmod(struct inode *inode)
{
	return 0;
}
#endif

int btrfs_relocate_block_group(struct btrfs_root *root, u64 group_start);
int btrfs_init_reloc_root(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root);
int btrfs_update_reloc_root(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root);
int btrfs_recover_relocation(struct btrfs_root *root);
int btrfs_reloc_clone_csums(struct inode *inode, u64 file_pos, u64 len);
void btrfs_reloc_cow_block(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, struct extent_buffer *buf,
			   struct extent_buffer *cow);
void btrfs_reloc_pre_snapshot(struct btrfs_trans_handle *trans,
			      struct btrfs_pending_snapshot *pending,
			      u64 *bytes_to_reserve);
int btrfs_reloc_post_snapshot(struct btrfs_trans_handle *trans,
			      struct btrfs_pending_snapshot *pending);

int btrfs_scrub_dev(struct btrfs_root *root, u64 devid, u64 start, u64 end,
		    struct btrfs_scrub_progress *progress, int readonly);
void btrfs_scrub_pause(struct btrfs_root *root);
void btrfs_scrub_pause_super(struct btrfs_root *root);
void btrfs_scrub_continue(struct btrfs_root *root);
void btrfs_scrub_continue_super(struct btrfs_root *root);
int __btrfs_scrub_cancel(struct btrfs_fs_info *info);
int btrfs_scrub_cancel(struct btrfs_root *root);
int btrfs_scrub_cancel_dev(struct btrfs_root *root, struct btrfs_device *dev);
int btrfs_scrub_cancel_devid(struct btrfs_root *root, u64 devid);
int btrfs_scrub_progress(struct btrfs_root *root, u64 devid,
			 struct btrfs_scrub_progress *progress);

struct reada_control {
	struct btrfs_root	*root;		
	struct btrfs_key	key_start;
	struct btrfs_key	key_end;	
	atomic_t		elems;
	struct kref		refcnt;
	wait_queue_head_t	wait;
};
struct reada_control *btrfs_reada_add(struct btrfs_root *root,
			      struct btrfs_key *start, struct btrfs_key *end);
int btrfs_reada_wait(void *handle);
void btrfs_reada_detach(void *handle);
int btree_readahead_hook(struct btrfs_root *root, struct extent_buffer *eb,
			 u64 start, int err);

#endif
