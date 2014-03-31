/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * journal.h
 *
 * Defines journalling api and structures.
 *
 * Copyright (C) 2003, 2005 Oracle.  All rights reserved.
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

#ifndef OCFS2_JOURNAL_H
#define OCFS2_JOURNAL_H

#include <linux/fs.h>
#include <linux/jbd2.h>

enum ocfs2_journal_state {
	OCFS2_JOURNAL_FREE = 0,
	OCFS2_JOURNAL_LOADED,
	OCFS2_JOURNAL_IN_SHUTDOWN,
};

struct ocfs2_super;
struct ocfs2_dinode;


struct ocfs2_recovery_map {
	unsigned int rm_used;
	unsigned int *rm_entries;
};


struct ocfs2_journal {
	enum ocfs2_journal_state   j_state;    

	journal_t                 *j_journal; 
	struct inode              *j_inode;   
	struct ocfs2_super        *j_osb;     
	struct buffer_head        *j_bh;      
	atomic_t                  j_num_trans; 
	spinlock_t                j_lock;
	unsigned long             j_trans_id;
	struct rw_semaphore       j_trans_barrier;
	wait_queue_head_t         j_checkpointed;

	
	struct list_head          j_la_cleanups;
	struct work_struct        j_recovery_work;
};

extern spinlock_t trans_inc_lock;

static inline unsigned long ocfs2_inc_trans_id(struct ocfs2_journal *j)
{
	unsigned long old_id;
	spin_lock(&trans_inc_lock);
	old_id = j->j_trans_id++;
	if (unlikely(!j->j_trans_id))
		j->j_trans_id = 1;
	spin_unlock(&trans_inc_lock);
	return old_id;
}

static inline void ocfs2_set_ci_lock_trans(struct ocfs2_journal *journal,
					   struct ocfs2_caching_info *ci)
{
	spin_lock(&trans_inc_lock);
	ci->ci_last_trans = journal->j_trans_id;
	spin_unlock(&trans_inc_lock);
}

static inline int ocfs2_ci_fully_checkpointed(struct ocfs2_caching_info *ci)
{
	int ret;
	struct ocfs2_journal *journal =
		OCFS2_SB(ocfs2_metadata_cache_get_super(ci))->journal;

	spin_lock(&trans_inc_lock);
	ret = time_after(journal->j_trans_id, ci->ci_last_trans);
	spin_unlock(&trans_inc_lock);
	return ret;
}

static inline int ocfs2_ci_is_new(struct ocfs2_caching_info *ci)
{
	int ret;
	struct ocfs2_journal *journal =
		OCFS2_SB(ocfs2_metadata_cache_get_super(ci))->journal;

	spin_lock(&trans_inc_lock);
	ret = !(time_after(journal->j_trans_id, ci->ci_created_trans));
	if (!ret)
		ci->ci_created_trans = 0;
	spin_unlock(&trans_inc_lock);
	return ret;
}

static inline int ocfs2_inode_is_new(struct inode *inode)
{
	/* System files are never "new" as they're written out by
	 * mkfs. This helps us early during mount, before we have the
	 * journal open and j_trans_id could be junk. */
	if (OCFS2_I(inode)->ip_flags & OCFS2_INODE_SYSTEM_FILE)
		return 0;

	return ocfs2_ci_is_new(INODE_CACHE(inode));
}

static inline void ocfs2_ci_set_new(struct ocfs2_super *osb,
				    struct ocfs2_caching_info *ci)
{
	spin_lock(&trans_inc_lock);
	ci->ci_created_trans = osb->journal->j_trans_id;
	spin_unlock(&trans_inc_lock);
}

void ocfs2_orphan_scan_init(struct ocfs2_super *osb);
void ocfs2_orphan_scan_start(struct ocfs2_super *osb);
void ocfs2_orphan_scan_stop(struct ocfs2_super *osb);
void ocfs2_orphan_scan_exit(struct ocfs2_super *osb);

void ocfs2_complete_recovery(struct work_struct *work);
void ocfs2_wait_for_recovery(struct ocfs2_super *osb);

int ocfs2_recovery_init(struct ocfs2_super *osb);
void ocfs2_recovery_exit(struct ocfs2_super *osb);

int ocfs2_compute_replay_slots(struct ocfs2_super *osb);
void   ocfs2_set_journal_params(struct ocfs2_super *osb);
int    ocfs2_journal_init(struct ocfs2_journal *journal,
			  int *dirty);
void   ocfs2_journal_shutdown(struct ocfs2_super *osb);
int    ocfs2_journal_wipe(struct ocfs2_journal *journal,
			  int full);
int    ocfs2_journal_load(struct ocfs2_journal *journal, int local,
			  int replayed);
int    ocfs2_check_journals_nolocks(struct ocfs2_super *osb);
void   ocfs2_recovery_thread(struct ocfs2_super *osb,
			     int node_num);
int    ocfs2_mark_dead_nodes(struct ocfs2_super *osb);
void   ocfs2_complete_mount_recovery(struct ocfs2_super *osb);
void ocfs2_complete_quota_recovery(struct ocfs2_super *osb);

static inline void ocfs2_start_checkpoint(struct ocfs2_super *osb)
{
	atomic_set(&osb->needs_checkpoint, 1);
	wake_up(&osb->checkpoint_event);
}

static inline void ocfs2_checkpoint_inode(struct inode *inode)
{
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);

	if (ocfs2_mount_local(osb))
		return;

	if (!ocfs2_ci_fully_checkpointed(INODE_CACHE(inode))) {
		ocfs2_start_checkpoint(osb);

		wait_event(osb->journal->j_checkpointed,
			   ocfs2_ci_fully_checkpointed(INODE_CACHE(inode)));
	}
}


handle_t		    *ocfs2_start_trans(struct ocfs2_super *osb,
					       int max_buffs);
int			     ocfs2_commit_trans(struct ocfs2_super *osb,
						handle_t *handle);
int			     ocfs2_extend_trans(handle_t *handle, int nblocks);

#define OCFS2_JOURNAL_ACCESS_CREATE 0
#define OCFS2_JOURNAL_ACCESS_WRITE  1
#define OCFS2_JOURNAL_ACCESS_UNDO   2


int ocfs2_journal_access_di(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_eb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_rb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_gd(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_xb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_dq(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_db(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_dr(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access_dl(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type);
int ocfs2_journal_access(handle_t *handle, struct ocfs2_caching_info *ci,
			 struct buffer_head *bh, int type);

void ocfs2_journal_dirty(handle_t *handle, struct buffer_head *bh);


#define OCFS2_INODE_UPDATE_CREDITS 1

#define OCFS2_XATTR_BLOCK_UPDATE_CREDITS 1

#define OCFS2_QUOTA_BLOCK_UPDATE_CREDITS 1

#define OCFS2_QINFO_WRITE_CREDITS (OCFS2_INODE_UPDATE_CREDITS + \
				   OCFS2_QUOTA_BLOCK_UPDATE_CREDITS)

#define OCFS2_LOCAL_QINFO_WRITE_CREDITS OCFS2_QUOTA_BLOCK_UPDATE_CREDITS
#define OCFS2_QWRITE_CREDITS (OCFS2_QINFO_WRITE_CREDITS + \
			      OCFS2_QUOTA_BLOCK_UPDATE_CREDITS)

#define OCFS2_QSYNC_CREDITS (OCFS2_QINFO_WRITE_CREDITS + \
			     2 * OCFS2_QUOTA_BLOCK_UPDATE_CREDITS)

static inline int ocfs2_quota_trans_credits(struct super_block *sb)
{
	int credits = 0;

	if (OCFS2_HAS_RO_COMPAT_FEATURE(sb, OCFS2_FEATURE_RO_COMPAT_USRQUOTA))
		credits += OCFS2_QWRITE_CREDITS;
	if (OCFS2_HAS_RO_COMPAT_FEATURE(sb, OCFS2_FEATURE_RO_COMPAT_GRPQUOTA))
		credits += OCFS2_QWRITE_CREDITS;
	return credits;
}

#define OCFS2_GROUP_EXTEND_CREDITS	(OCFS2_INODE_UPDATE_CREDITS + 1)

#define OCFS2_GROUP_ADD_CREDITS	(OCFS2_INODE_UPDATE_CREDITS + 1)

#define OCFS2_SUBALLOC_ALLOC (3)

static inline int ocfs2_inline_to_extents_credits(struct super_block *sb)
{
	return OCFS2_SUBALLOC_ALLOC + OCFS2_INODE_UPDATE_CREDITS +
	       ocfs2_quota_trans_credits(sb);
}

#define OCFS2_SUBALLOC_FREE  (2)

#define OCFS2_TRUNCATE_LOG_UPDATE OCFS2_INODE_UPDATE_CREDITS
#define OCFS2_TRUNCATE_LOG_FLUSH_ONE_REC (OCFS2_SUBALLOC_FREE 		      \
					 + OCFS2_TRUNCATE_LOG_UPDATE)

static inline int ocfs2_remove_extent_credits(struct super_block *sb)
{
	return OCFS2_TRUNCATE_LOG_UPDATE + OCFS2_INODE_UPDATE_CREDITS +
	       ocfs2_quota_trans_credits(sb);
}

#define OCFS2_DIR_LINK_ADDITIONAL_CREDITS (1 + OCFS2_SUBALLOC_ALLOC + 1)

static inline int ocfs2_add_dir_index_credits(struct super_block *sb)
{
	return 1 + 2 * OCFS2_SUBALLOC_ALLOC +
		ocfs2_clusters_to_blocks(sb, 1);
}

static inline int ocfs2_mknod_credits(struct super_block *sb, int is_dir,
				      int xattr_credits)
{
	int dir_credits = OCFS2_DIR_LINK_ADDITIONAL_CREDITS;

	if (is_dir)
		dir_credits += ocfs2_add_dir_index_credits(sb);

	return 4 + OCFS2_SUBALLOC_ALLOC + dir_credits + xattr_credits +
	       ocfs2_quota_trans_credits(sb);
}

#define OCFS2_WINDOW_MOVE_CREDITS (OCFS2_INODE_UPDATE_CREDITS                 \
				  + OCFS2_SUBALLOC_ALLOC + OCFS2_SUBALLOC_FREE)

#define OCFS2_SIMPLE_DIR_EXTEND_CREDITS (2)

static inline int ocfs2_link_credits(struct super_block *sb)
{
	return 2*OCFS2_INODE_UPDATE_CREDITS + 4 +
	       ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_unlink_credits(struct super_block *sb)
{
	
	return 2 * OCFS2_INODE_UPDATE_CREDITS + 3 + ocfs2_link_credits(sb);
}

#define OCFS2_DELETE_INODE_CREDITS (3 * OCFS2_INODE_UPDATE_CREDITS + 4)

static inline int ocfs2_rename_credits(struct super_block *sb)
{
	return 3 * OCFS2_INODE_UPDATE_CREDITS + 6 + ocfs2_unlink_credits(sb);
}

#define OCFS2_XATTR_BLOCK_CREATE_CREDITS (OCFS2_SUBALLOC_ALLOC * 2 + \
					  + OCFS2_INODE_UPDATE_CREDITS \
					  + OCFS2_XATTR_BLOCK_UPDATE_CREDITS)

#define OCFS2_DX_ROOT_REMOVE_CREDITS (OCFS2_INODE_UPDATE_CREDITS +	\
				      OCFS2_SUBALLOC_FREE)

static inline int ocfs2_calc_dxi_expand_credits(struct super_block *sb)
{
	int credits = 1 + OCFS2_SUBALLOC_ALLOC;

	credits += ocfs2_clusters_to_blocks(sb, 1);
	credits += ocfs2_quota_trans_credits(sb);

	return credits;
}

#define OCFS2_REFCOUNT_TREE_CREATE_CREDITS (OCFS2_INODE_UPDATE_CREDITS + 1 \
					    + OCFS2_SUBALLOC_ALLOC)

#define OCFS2_REFCOUNT_TREE_SET_CREDITS (OCFS2_INODE_UPDATE_CREDITS + 1)

#define OCFS2_REFCOUNT_TREE_REMOVE_CREDITS (OCFS2_INODE_UPDATE_CREDITS + 1)

#define OCFS2_EXPAND_REFCOUNT_TREE_CREDITS (OCFS2_SUBALLOC_ALLOC * 2 + 3)

static inline int ocfs2_calc_extend_credits(struct super_block *sb,
					    struct ocfs2_extent_list *root_el,
					    u32 bits_wanted)
{
	int bitmap_blocks, sysfile_bitmap_blocks, extent_blocks;

	
	bitmap_blocks = OCFS2_SUBALLOC_ALLOC;

	sysfile_bitmap_blocks = 1 +
		(OCFS2_SUBALLOC_ALLOC - 1) * ocfs2_extend_meta_needed(root_el);

	extent_blocks = 1 + 1 + le16_to_cpu(root_el->l_tree_depth);

	return bitmap_blocks + sysfile_bitmap_blocks + extent_blocks +
	       ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_calc_symlink_credits(struct super_block *sb)
{
	int blocks = ocfs2_mknod_credits(sb, 0, 0);

	blocks += ocfs2_clusters_to_blocks(sb, 1);

	return blocks + ocfs2_quota_trans_credits(sb);
}

static inline int ocfs2_calc_group_alloc_credits(struct super_block *sb,
						 unsigned int cpg)
{
	int blocks;
	int bitmap_blocks = OCFS2_SUBALLOC_ALLOC + 1;
	blocks = 1 + 1 + 1 + bitmap_blocks;
	return blocks;
}

static inline int ocfs2_calc_bg_discontig_credits(struct super_block *sb)
{
	return ocfs2_extent_recs_per_gd(sb);
}

static inline int ocfs2_calc_tree_trunc_credits(struct super_block *sb,
						unsigned int clusters_to_del,
						struct ocfs2_dinode *fe,
						struct ocfs2_extent_list *last_el)
{
 	
	u16 next_free = le16_to_cpu(last_el->l_next_free_rec);
	u16 tree_depth = le16_to_cpu(fe->id2.i_list.l_tree_depth);
	int credits = 1 + tree_depth + 1;
	int i;

	i = next_free - 1;
	BUG_ON(i < 0);

	if (tree_depth && next_free == 1 &&
	    ocfs2_rec_clusters(last_el, &last_el->l_recs[i]) == clusters_to_del)
		credits += 1 + tree_depth;

	
	credits += OCFS2_TRUNCATE_LOG_UPDATE;

	credits += ocfs2_quota_trans_credits(sb);

	return credits;
}

static inline int ocfs2_jbd2_file_inode(handle_t *handle, struct inode *inode)
{
	return jbd2_journal_file_inode(handle, &OCFS2_I(inode)->ip_jinode);
}

static inline int ocfs2_begin_ordered_truncate(struct inode *inode,
					       loff_t new_size)
{
	return jbd2_journal_begin_ordered_truncate(
				OCFS2_SB(inode->i_sb)->journal->j_journal,
				&OCFS2_I(inode)->ip_jinode,
				new_size);
}

#endif 
