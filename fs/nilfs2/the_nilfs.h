/*
 * the_nilfs.h - the_nilfs shared structure.
 *
 * Copyright (C) 2005-2008 Nippon Telegraph and Telephone Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by Ryusuke Konishi <ryusuke@osrg.net>
 *
 */

#ifndef _THE_NILFS_H
#define _THE_NILFS_H

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/rbtree.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/slab.h>

struct nilfs_sc_info;

enum {
	THE_NILFS_INIT = 0,     
	THE_NILFS_DISCONTINUED,	
	THE_NILFS_GC_RUNNING,	
	THE_NILFS_SB_DIRTY,	
};

struct the_nilfs {
	unsigned long		ns_flags;

	struct block_device    *ns_bdev;
	struct rw_semaphore	ns_sem;

	struct buffer_head     *ns_sbh[2];
	struct nilfs_super_block *ns_sbp[2];
	time_t			ns_sbwtime;
	unsigned		ns_sbwcount;
	unsigned		ns_sbsize;
	unsigned		ns_mount_state;

	u64			ns_seg_seq;
	__u64			ns_segnum;
	__u64			ns_nextnum;
	unsigned long		ns_pseg_offset;
	__u64			ns_cno;
	time_t			ns_ctime;
	time_t			ns_nongc_ctime;
	atomic_t		ns_ndirtyblks;

	/*
	 * The following fields hold information on the latest partial segment
	 * written to disk with a super root.  These fields are protected by
	 * ns_last_segment_lock.
	 */
	spinlock_t		ns_last_segment_lock;
	sector_t		ns_last_pseg;
	u64			ns_last_seq;
	__u64			ns_last_cno;
	u64			ns_prot_seq;
	u64			ns_prev_seq;

	struct nilfs_sc_info   *ns_writer;
	struct rw_semaphore	ns_segctor_sem;

	struct inode	       *ns_dat;
	struct inode	       *ns_cpfile;
	struct inode	       *ns_sufile;

	
	struct rb_root		ns_cptree;
	spinlock_t		ns_cptree_lock;

	
	struct list_head	ns_dirty_files;
	spinlock_t		ns_inode_lock;

	
	struct list_head	ns_gc_inodes;

	
	u32			ns_next_generation;
	spinlock_t		ns_next_gen_lock;

	
	unsigned long		ns_mount_opt;

	uid_t			ns_resuid;
	gid_t			ns_resgid;
	unsigned long		ns_interval;
	unsigned long		ns_watermark;

	
	unsigned int		ns_blocksize_bits;
	unsigned int		ns_blocksize;
	unsigned long		ns_nsegments;
	unsigned long		ns_blocks_per_segment;
	unsigned long		ns_r_segments_percentage;
	unsigned long		ns_nrsvsegs;
	unsigned long		ns_first_data_block;
	int			ns_inode_size;
	int			ns_first_ino;
	u32			ns_crc_seed;
};

#define THE_NILFS_FNS(bit, name)					\
static inline void set_nilfs_##name(struct the_nilfs *nilfs)		\
{									\
	set_bit(THE_NILFS_##bit, &(nilfs)->ns_flags);			\
}									\
static inline void clear_nilfs_##name(struct the_nilfs *nilfs)		\
{									\
	clear_bit(THE_NILFS_##bit, &(nilfs)->ns_flags);			\
}									\
static inline int nilfs_##name(struct the_nilfs *nilfs)			\
{									\
	return test_bit(THE_NILFS_##bit, &(nilfs)->ns_flags);		\
}

THE_NILFS_FNS(INIT, init)
THE_NILFS_FNS(DISCONTINUED, discontinued)
THE_NILFS_FNS(GC_RUNNING, gc_running)
THE_NILFS_FNS(SB_DIRTY, sb_dirty)

#define nilfs_clear_opt(nilfs, opt)  \
	do { (nilfs)->ns_mount_opt &= ~NILFS_MOUNT_##opt; } while (0)
#define nilfs_set_opt(nilfs, opt)  \
	do { (nilfs)->ns_mount_opt |= NILFS_MOUNT_##opt; } while (0)
#define nilfs_test_opt(nilfs, opt) ((nilfs)->ns_mount_opt & NILFS_MOUNT_##opt)
#define nilfs_write_opt(nilfs, mask, opt)				\
	do { (nilfs)->ns_mount_opt =					\
		(((nilfs)->ns_mount_opt & ~NILFS_MOUNT_##mask) |	\
		 NILFS_MOUNT_##opt);					\
	} while (0)

struct nilfs_root {
	__u64 cno;
	struct rb_node rb_node;

	atomic_t count;
	struct the_nilfs *nilfs;
	struct inode *ifile;

	atomic_t inodes_count;
	atomic_t blocks_count;
};

#define NILFS_CPTREE_CURRENT_CNO	0

#define NILFS_SB_FREQ		10

static inline int nilfs_sb_need_update(struct the_nilfs *nilfs)
{
	u64 t = get_seconds();
	return t < nilfs->ns_sbwtime || t > nilfs->ns_sbwtime + NILFS_SB_FREQ;
}

static inline int nilfs_sb_will_flip(struct the_nilfs *nilfs)
{
	int flip_bits = nilfs->ns_sbwcount & 0x0FL;
	return (flip_bits != 0x08 && flip_bits != 0x0F);
}

void nilfs_set_last_segment(struct the_nilfs *, sector_t, u64, __u64);
struct the_nilfs *alloc_nilfs(struct block_device *bdev);
void destroy_nilfs(struct the_nilfs *nilfs);
int init_nilfs(struct the_nilfs *nilfs, struct super_block *sb, char *data);
int load_nilfs(struct the_nilfs *nilfs, struct super_block *sb);
unsigned long nilfs_nrsvsegs(struct the_nilfs *nilfs, unsigned long nsegs);
void nilfs_set_nsegments(struct the_nilfs *nilfs, unsigned long nsegs);
int nilfs_discard_segments(struct the_nilfs *, __u64 *, size_t);
int nilfs_count_free_blocks(struct the_nilfs *, sector_t *);
struct nilfs_root *nilfs_lookup_root(struct the_nilfs *nilfs, __u64 cno);
struct nilfs_root *nilfs_find_or_create_root(struct the_nilfs *nilfs,
					     __u64 cno);
void nilfs_put_root(struct nilfs_root *root);
int nilfs_near_disk_full(struct the_nilfs *);
void nilfs_fall_back_super_block(struct the_nilfs *);
void nilfs_swap_super_block(struct the_nilfs *);


static inline void nilfs_get_root(struct nilfs_root *root)
{
	atomic_inc(&root->count);
}

static inline int nilfs_valid_fs(struct the_nilfs *nilfs)
{
	unsigned valid_fs;

	down_read(&nilfs->ns_sem);
	valid_fs = (nilfs->ns_mount_state & NILFS_VALID_FS);
	up_read(&nilfs->ns_sem);
	return valid_fs;
}

static inline void
nilfs_get_segment_range(struct the_nilfs *nilfs, __u64 segnum,
			sector_t *seg_start, sector_t *seg_end)
{
	*seg_start = (sector_t)nilfs->ns_blocks_per_segment * segnum;
	*seg_end = *seg_start + nilfs->ns_blocks_per_segment - 1;
	if (segnum == 0)
		*seg_start = nilfs->ns_first_data_block;
}

static inline sector_t
nilfs_get_segment_start_blocknr(struct the_nilfs *nilfs, __u64 segnum)
{
	return (segnum == 0) ? nilfs->ns_first_data_block :
		(sector_t)nilfs->ns_blocks_per_segment * segnum;
}

static inline __u64
nilfs_get_segnum_of_block(struct the_nilfs *nilfs, sector_t blocknr)
{
	sector_t segnum = blocknr;

	sector_div(segnum, nilfs->ns_blocks_per_segment);
	return segnum;
}

static inline void
nilfs_terminate_segment(struct the_nilfs *nilfs, sector_t seg_start,
			sector_t seg_end)
{
	
	nilfs->ns_pseg_offset = seg_end - seg_start + 1;
}

static inline void nilfs_shift_to_next_segment(struct the_nilfs *nilfs)
{
	
	nilfs->ns_segnum = nilfs->ns_nextnum;
	nilfs->ns_pseg_offset = 0;
	nilfs->ns_seg_seq++;
}

static inline __u64 nilfs_last_cno(struct the_nilfs *nilfs)
{
	__u64 cno;

	spin_lock(&nilfs->ns_last_segment_lock);
	cno = nilfs->ns_last_cno;
	spin_unlock(&nilfs->ns_last_segment_lock);
	return cno;
}

static inline int nilfs_segment_is_active(struct the_nilfs *nilfs, __u64 n)
{
	return n == nilfs->ns_segnum || n == nilfs->ns_nextnum;
}

#endif 
