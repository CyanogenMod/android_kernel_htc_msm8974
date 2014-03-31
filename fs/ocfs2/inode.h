/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * inode.h
 *
 * Function prototypes
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifndef OCFS2_INODE_H
#define OCFS2_INODE_H

#include "extent_map.h"

struct ocfs2_inode_info
{
	u64			ip_blkno;

	struct ocfs2_lock_res		ip_rw_lockres;
	struct ocfs2_lock_res		ip_inode_lockres;
	struct ocfs2_lock_res		ip_open_lockres;

	
	struct rw_semaphore		ip_alloc_sem;

	
	struct rw_semaphore		ip_xattr_sem;

	
	atomic_t			ip_unaligned_aio;

	
	spinlock_t			ip_lock;
	u32				ip_open_count;
	struct list_head		ip_io_markers;
	u32				ip_clusters;

	u16				ip_dyn_features;
	struct mutex			ip_io_mutex;
	u32				ip_flags; 
	u32				ip_attr; 

	
	struct inode			*ip_next_orphan;

	struct ocfs2_caching_info	ip_metadata_cache;
	struct ocfs2_extent_map		ip_extent_map;
	struct inode			vfs_inode;
	struct jbd2_inode		ip_jinode;

	u32				ip_dir_start_lookup;

	
	u32				ip_last_used_slot;
	u64				ip_last_used_group;
	u32				ip_dir_lock_gen;

	struct ocfs2_alloc_reservation	ip_la_data_resv;
};

#define OCFS2_INODE_SYSTEM_FILE		0x00000001
#define OCFS2_INODE_JOURNAL		0x00000002
#define OCFS2_INODE_BITMAP		0x00000004
#define OCFS2_INODE_DELETED		0x00000008
#define OCFS2_INODE_SKIP_DELETE		0x00000010
#define OCFS2_INODE_MAYBE_ORPHANED	0x00000020
#define OCFS2_INODE_OPEN_DIRECT		0x00000040
#define OCFS2_INODE_SKIP_ORPHAN_DIR     0x00000080

static inline struct ocfs2_inode_info *OCFS2_I(struct inode *inode)
{
	return container_of(inode, struct ocfs2_inode_info, vfs_inode);
}

#define INODE_JOURNAL(i) (OCFS2_I(i)->ip_flags & OCFS2_INODE_JOURNAL)
#define SET_INODE_JOURNAL(i) (OCFS2_I(i)->ip_flags |= OCFS2_INODE_JOURNAL)

extern struct kmem_cache *ocfs2_inode_cache;

extern const struct address_space_operations ocfs2_aops;
extern const struct ocfs2_caching_operations ocfs2_inode_caching_ops;

static inline struct ocfs2_caching_info *INODE_CACHE(struct inode *inode)
{
	return &OCFS2_I(inode)->ip_metadata_cache;
}

void ocfs2_evict_inode(struct inode *inode);
int ocfs2_drop_inode(struct inode *inode);

#define OCFS2_FI_FLAG_SYSFILE		0x1
#define OCFS2_FI_FLAG_ORPHAN_RECOVERY	0x2
struct inode *ocfs2_ilookup(struct super_block *sb, u64 feoff);
struct inode *ocfs2_iget(struct ocfs2_super *osb, u64 feoff, unsigned flags,
			 int sysfile_type);
int ocfs2_inode_init_private(struct inode *inode);
int ocfs2_inode_revalidate(struct dentry *dentry);
void ocfs2_populate_inode(struct inode *inode, struct ocfs2_dinode *fe,
			  int create_ino);
void ocfs2_read_inode(struct inode *inode);
void ocfs2_read_inode2(struct inode *inode, void *opaque);
ssize_t ocfs2_rw_direct(int rw, struct file *filp, char *buf,
			size_t size, loff_t *offp);
void ocfs2_sync_blockdev(struct super_block *sb);
void ocfs2_refresh_inode(struct inode *inode,
			 struct ocfs2_dinode *fe);
int ocfs2_mark_inode_dirty(handle_t *handle,
			   struct inode *inode,
			   struct buffer_head *bh);
int ocfs2_aio_read(struct file *file, struct kiocb *req, struct iocb *iocb);
int ocfs2_aio_write(struct file *file, struct kiocb *req, struct iocb *iocb);
struct buffer_head *ocfs2_bread(struct inode *inode,
				int block, int *err, int reada);

void ocfs2_set_inode_flags(struct inode *inode);
void ocfs2_get_inode_flags(struct ocfs2_inode_info *oi);

static inline blkcnt_t ocfs2_inode_sector_count(struct inode *inode)
{
	int c_to_s_bits = OCFS2_SB(inode->i_sb)->s_clustersize_bits - 9;

	return (blkcnt_t)(OCFS2_I(inode)->ip_clusters << c_to_s_bits);
}

int ocfs2_validate_inode_block(struct super_block *sb,
			       struct buffer_head *bh);
int ocfs2_read_inode_block(struct inode *inode, struct buffer_head **bh);
int ocfs2_read_inode_block_full(struct inode *inode, struct buffer_head **bh,
				int flags);

static inline struct ocfs2_inode_info *cache_info_to_inode(struct ocfs2_caching_info *ci)
{
	return container_of(ci, struct ocfs2_inode_info, ip_metadata_cache);
}

#endif 
