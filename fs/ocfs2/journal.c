/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * journal.c
 *
 * Defines functions of journalling api
 *
 * Copyright (C) 2003, 2004 Oracle.  All rights reserved.
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

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/random.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "blockcheck.h"
#include "dir.h"
#include "dlmglue.h"
#include "extent_map.h"
#include "heartbeat.h"
#include "inode.h"
#include "journal.h"
#include "localalloc.h"
#include "slot_map.h"
#include "super.h"
#include "sysfile.h"
#include "uptodate.h"
#include "quota.h"

#include "buffer_head_io.h"
#include "ocfs2_trace.h"

DEFINE_SPINLOCK(trans_inc_lock);

#define ORPHAN_SCAN_SCHEDULE_TIMEOUT 300000

static int ocfs2_force_read_journal(struct inode *inode);
static int ocfs2_recover_node(struct ocfs2_super *osb,
			      int node_num, int slot_num);
static int __ocfs2_recovery_thread(void *arg);
static int ocfs2_commit_cache(struct ocfs2_super *osb);
static int __ocfs2_wait_on_mount(struct ocfs2_super *osb, int quota);
static int ocfs2_journal_toggle_dirty(struct ocfs2_super *osb,
				      int dirty, int replayed);
static int ocfs2_trylock_journal(struct ocfs2_super *osb,
				 int slot_num);
static int ocfs2_recover_orphans(struct ocfs2_super *osb,
				 int slot);
static int ocfs2_commit_thread(void *arg);
static void ocfs2_queue_recovery_completion(struct ocfs2_journal *journal,
					    int slot_num,
					    struct ocfs2_dinode *la_dinode,
					    struct ocfs2_dinode *tl_dinode,
					    struct ocfs2_quota_recovery *qrec);

static inline int ocfs2_wait_on_mount(struct ocfs2_super *osb)
{
	return __ocfs2_wait_on_mount(osb, 0);
}

static inline int ocfs2_wait_on_quotas(struct ocfs2_super *osb)
{
	return __ocfs2_wait_on_mount(osb, 1);
}


enum ocfs2_replay_state {
	REPLAY_UNNEEDED = 0,	
	REPLAY_NEEDED, 		
	REPLAY_DONE 		
};

struct ocfs2_replay_map {
	unsigned int rm_slots;
	enum ocfs2_replay_state rm_state;
	unsigned char rm_replay_slots[0];
};

void ocfs2_replay_map_set_state(struct ocfs2_super *osb, int state)
{
	if (!osb->replay_map)
		return;

	
	if (osb->replay_map->rm_state == REPLAY_DONE)
		return;

	osb->replay_map->rm_state = state;
}

int ocfs2_compute_replay_slots(struct ocfs2_super *osb)
{
	struct ocfs2_replay_map *replay_map;
	int i, node_num;

	
	if (osb->replay_map)
		return 0;

	replay_map = kzalloc(sizeof(struct ocfs2_replay_map) +
			     (osb->max_slots * sizeof(char)), GFP_KERNEL);

	if (!replay_map) {
		mlog_errno(-ENOMEM);
		return -ENOMEM;
	}

	spin_lock(&osb->osb_lock);

	replay_map->rm_slots = osb->max_slots;
	replay_map->rm_state = REPLAY_UNNEEDED;

	
	for (i = 0; i < replay_map->rm_slots; i++) {
		if (ocfs2_slot_to_node_num_locked(osb, i, &node_num) == -ENOENT)
			replay_map->rm_replay_slots[i] = 1;
	}

	osb->replay_map = replay_map;
	spin_unlock(&osb->osb_lock);
	return 0;
}

void ocfs2_queue_replay_slots(struct ocfs2_super *osb)
{
	struct ocfs2_replay_map *replay_map = osb->replay_map;
	int i;

	if (!replay_map)
		return;

	if (replay_map->rm_state != REPLAY_NEEDED)
		return;

	for (i = 0; i < replay_map->rm_slots; i++)
		if (replay_map->rm_replay_slots[i])
			ocfs2_queue_recovery_completion(osb->journal, i, NULL,
							NULL, NULL);
	replay_map->rm_state = REPLAY_DONE;
}

void ocfs2_free_replay_slots(struct ocfs2_super *osb)
{
	struct ocfs2_replay_map *replay_map = osb->replay_map;

	if (!osb->replay_map)
		return;

	kfree(replay_map);
	osb->replay_map = NULL;
}

int ocfs2_recovery_init(struct ocfs2_super *osb)
{
	struct ocfs2_recovery_map *rm;

	mutex_init(&osb->recovery_lock);
	osb->disable_recovery = 0;
	osb->recovery_thread_task = NULL;
	init_waitqueue_head(&osb->recovery_event);

	rm = kzalloc(sizeof(struct ocfs2_recovery_map) +
		     osb->max_slots * sizeof(unsigned int),
		     GFP_KERNEL);
	if (!rm) {
		mlog_errno(-ENOMEM);
		return -ENOMEM;
	}

	rm->rm_entries = (unsigned int *)((char *)rm +
					  sizeof(struct ocfs2_recovery_map));
	osb->recovery_map = rm;

	return 0;
}

static int ocfs2_recovery_thread_running(struct ocfs2_super *osb)
{
	mb();
	return osb->recovery_thread_task != NULL;
}

void ocfs2_recovery_exit(struct ocfs2_super *osb)
{
	struct ocfs2_recovery_map *rm;

	mutex_lock(&osb->recovery_lock);
	osb->disable_recovery = 1;
	mutex_unlock(&osb->recovery_lock);
	wait_event(osb->recovery_event, !ocfs2_recovery_thread_running(osb));

	flush_workqueue(ocfs2_wq);

	rm = osb->recovery_map;
	

	kfree(rm);
}

static int __ocfs2_recovery_map_test(struct ocfs2_super *osb,
				     unsigned int node_num)
{
	int i;
	struct ocfs2_recovery_map *rm = osb->recovery_map;

	assert_spin_locked(&osb->osb_lock);

	for (i = 0; i < rm->rm_used; i++) {
		if (rm->rm_entries[i] == node_num)
			return 1;
	}

	return 0;
}

static int ocfs2_recovery_map_set(struct ocfs2_super *osb,
				  unsigned int node_num)
{
	struct ocfs2_recovery_map *rm = osb->recovery_map;

	spin_lock(&osb->osb_lock);
	if (__ocfs2_recovery_map_test(osb, node_num)) {
		spin_unlock(&osb->osb_lock);
		return 1;
	}

	
	BUG_ON(rm->rm_used >= osb->max_slots);

	rm->rm_entries[rm->rm_used] = node_num;
	rm->rm_used++;
	spin_unlock(&osb->osb_lock);

	return 0;
}

static void ocfs2_recovery_map_clear(struct ocfs2_super *osb,
				     unsigned int node_num)
{
	int i;
	struct ocfs2_recovery_map *rm = osb->recovery_map;

	spin_lock(&osb->osb_lock);

	for (i = 0; i < rm->rm_used; i++) {
		if (rm->rm_entries[i] == node_num)
			break;
	}

	if (i < rm->rm_used) {
		
		memmove(&(rm->rm_entries[i]), &(rm->rm_entries[i + 1]),
			(rm->rm_used - i - 1) * sizeof(unsigned int));
		rm->rm_used--;
	}

	spin_unlock(&osb->osb_lock);
}

static int ocfs2_commit_cache(struct ocfs2_super *osb)
{
	int status = 0;
	unsigned int flushed;
	struct ocfs2_journal *journal = NULL;

	journal = osb->journal;

	
	down_write(&journal->j_trans_barrier);

	flushed = atomic_read(&journal->j_num_trans);
	trace_ocfs2_commit_cache_begin(flushed);
	if (flushed == 0) {
		up_write(&journal->j_trans_barrier);
		goto finally;
	}

	jbd2_journal_lock_updates(journal->j_journal);
	status = jbd2_journal_flush(journal->j_journal);
	jbd2_journal_unlock_updates(journal->j_journal);
	if (status < 0) {
		up_write(&journal->j_trans_barrier);
		mlog_errno(status);
		goto finally;
	}

	ocfs2_inc_trans_id(journal);

	flushed = atomic_read(&journal->j_num_trans);
	atomic_set(&journal->j_num_trans, 0);
	up_write(&journal->j_trans_barrier);

	trace_ocfs2_commit_cache_end(journal->j_trans_id, flushed);

	ocfs2_wake_downconvert_thread(osb);
	wake_up(&journal->j_checkpointed);
finally:
	return status;
}

handle_t *ocfs2_start_trans(struct ocfs2_super *osb, int max_buffs)
{
	journal_t *journal = osb->journal->j_journal;
	handle_t *handle;

	BUG_ON(!osb || !osb->journal->j_journal);

	if (ocfs2_is_hard_readonly(osb))
		return ERR_PTR(-EROFS);

	BUG_ON(osb->journal->j_state == OCFS2_JOURNAL_FREE);
	BUG_ON(max_buffs <= 0);

	
	if (journal_current_handle())
		return jbd2_journal_start(journal, max_buffs);

	down_read(&osb->journal->j_trans_barrier);

	handle = jbd2_journal_start(journal, max_buffs);
	if (IS_ERR(handle)) {
		up_read(&osb->journal->j_trans_barrier);

		mlog_errno(PTR_ERR(handle));

		if (is_journal_aborted(journal)) {
			ocfs2_abort(osb->sb, "Detected aborted journal");
			handle = ERR_PTR(-EROFS);
		}
	} else {
		if (!ocfs2_mount_local(osb))
			atomic_inc(&(osb->journal->j_num_trans));
	}

	return handle;
}

int ocfs2_commit_trans(struct ocfs2_super *osb,
		       handle_t *handle)
{
	int ret, nested;
	struct ocfs2_journal *journal = osb->journal;

	BUG_ON(!handle);

	nested = handle->h_ref > 1;
	ret = jbd2_journal_stop(handle);
	if (ret < 0)
		mlog_errno(ret);

	if (!nested)
		up_read(&journal->j_trans_barrier);

	return ret;
}

int ocfs2_extend_trans(handle_t *handle, int nblocks)
{
	int status, old_nblocks;

	BUG_ON(!handle);
	BUG_ON(nblocks < 0);

	if (!nblocks)
		return 0;

	old_nblocks = handle->h_buffer_credits;

	trace_ocfs2_extend_trans(old_nblocks, nblocks);

#ifdef CONFIG_OCFS2_DEBUG_FS
	status = 1;
#else
	status = jbd2_journal_extend(handle, nblocks);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
#endif

	if (status > 0) {
		trace_ocfs2_extend_trans_restart(old_nblocks + nblocks);
		status = jbd2_journal_restart(handle,
					      old_nblocks + nblocks);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	status = 0;
bail:
	return status;
}

struct ocfs2_triggers {
	struct jbd2_buffer_trigger_type	ot_triggers;
	int				ot_offset;
};

static inline struct ocfs2_triggers *to_ocfs2_trigger(struct jbd2_buffer_trigger_type *triggers)
{
	return container_of(triggers, struct ocfs2_triggers, ot_triggers);
}

static void ocfs2_frozen_trigger(struct jbd2_buffer_trigger_type *triggers,
				 struct buffer_head *bh,
				 void *data, size_t size)
{
	struct ocfs2_triggers *ot = to_ocfs2_trigger(triggers);

	ocfs2_block_check_compute(data, size, data + ot->ot_offset);
}

static void ocfs2_dq_frozen_trigger(struct jbd2_buffer_trigger_type *triggers,
				 struct buffer_head *bh,
				 void *data, size_t size)
{
	struct ocfs2_disk_dqtrailer *dqt =
		ocfs2_block_dqtrailer(size, data);

	ocfs2_block_check_compute(data, size, &dqt->dq_check);
}

static void ocfs2_db_frozen_trigger(struct jbd2_buffer_trigger_type *triggers,
				 struct buffer_head *bh,
				 void *data, size_t size)
{
	struct ocfs2_dir_block_trailer *trailer =
		ocfs2_dir_trailer_from_size(size, data);

	ocfs2_block_check_compute(data, size, &trailer->db_check);
}

static void ocfs2_abort_trigger(struct jbd2_buffer_trigger_type *triggers,
				struct buffer_head *bh)
{
	mlog(ML_ERROR,
	     "ocfs2_abort_trigger called by JBD2.  bh = 0x%lx, "
	     "bh->b_blocknr = %llu\n",
	     (unsigned long)bh,
	     (unsigned long long)bh->b_blocknr);

	ocfs2_error(bh->b_assoc_map->host->i_sb,
		    "JBD2 has aborted our journal, ocfs2 cannot continue\n");
}

static struct ocfs2_triggers di_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_dinode, i_check),
};

static struct ocfs2_triggers eb_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_extent_block, h_check),
};

static struct ocfs2_triggers rb_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_refcount_block, rf_check),
};

static struct ocfs2_triggers gd_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_group_desc, bg_check),
};

static struct ocfs2_triggers db_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_db_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
};

static struct ocfs2_triggers xb_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_xattr_block, xb_check),
};

static struct ocfs2_triggers dq_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_dq_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
};

static struct ocfs2_triggers dr_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_dx_root_block, dr_check),
};

static struct ocfs2_triggers dl_triggers = {
	.ot_triggers = {
		.t_frozen = ocfs2_frozen_trigger,
		.t_abort = ocfs2_abort_trigger,
	},
	.ot_offset	= offsetof(struct ocfs2_dx_leaf, dl_check),
};

static int __ocfs2_journal_access(handle_t *handle,
				  struct ocfs2_caching_info *ci,
				  struct buffer_head *bh,
				  struct ocfs2_triggers *triggers,
				  int type)
{
	int status;
	struct ocfs2_super *osb =
		OCFS2_SB(ocfs2_metadata_cache_get_super(ci));

	BUG_ON(!ci || !ci->ci_ops);
	BUG_ON(!handle);
	BUG_ON(!bh);

	trace_ocfs2_journal_access(
		(unsigned long long)ocfs2_metadata_cache_owner(ci),
		(unsigned long long)bh->b_blocknr, type, bh->b_size);

	
	if (!buffer_uptodate(bh)) {
		mlog(ML_ERROR, "giving me a buffer that's not uptodate!\n");
		mlog(ML_ERROR, "b_blocknr=%llu\n",
		     (unsigned long long)bh->b_blocknr);
		BUG();
	}

	ocfs2_set_ci_lock_trans(osb->journal, ci);

	ocfs2_metadata_cache_io_lock(ci);
	switch (type) {
	case OCFS2_JOURNAL_ACCESS_CREATE:
	case OCFS2_JOURNAL_ACCESS_WRITE:
		status = jbd2_journal_get_write_access(handle, bh);
		break;

	case OCFS2_JOURNAL_ACCESS_UNDO:
		status = jbd2_journal_get_undo_access(handle, bh);
		break;

	default:
		status = -EINVAL;
		mlog(ML_ERROR, "Unknown access type!\n");
	}
	if (!status && ocfs2_meta_ecc(osb) && triggers)
		jbd2_journal_set_triggers(bh, &triggers->ot_triggers);
	ocfs2_metadata_cache_io_unlock(ci);

	if (status < 0)
		mlog(ML_ERROR, "Error %d getting %d access to buffer!\n",
		     status, type);

	return status;
}

int ocfs2_journal_access_di(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &di_triggers, type);
}

int ocfs2_journal_access_eb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &eb_triggers, type);
}

int ocfs2_journal_access_rb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &rb_triggers,
				      type);
}

int ocfs2_journal_access_gd(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &gd_triggers, type);
}

int ocfs2_journal_access_db(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &db_triggers, type);
}

int ocfs2_journal_access_xb(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &xb_triggers, type);
}

int ocfs2_journal_access_dq(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &dq_triggers, type);
}

int ocfs2_journal_access_dr(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &dr_triggers, type);
}

int ocfs2_journal_access_dl(handle_t *handle, struct ocfs2_caching_info *ci,
			    struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, &dl_triggers, type);
}

int ocfs2_journal_access(handle_t *handle, struct ocfs2_caching_info *ci,
			 struct buffer_head *bh, int type)
{
	return __ocfs2_journal_access(handle, ci, bh, NULL, type);
}

void ocfs2_journal_dirty(handle_t *handle, struct buffer_head *bh)
{
	int status;

	trace_ocfs2_journal_dirty((unsigned long long)bh->b_blocknr);

	status = jbd2_journal_dirty_metadata(handle, bh);
	BUG_ON(status);
}

#define OCFS2_DEFAULT_COMMIT_INTERVAL	(HZ * JBD2_DEFAULT_MAX_COMMIT_AGE)

void ocfs2_set_journal_params(struct ocfs2_super *osb)
{
	journal_t *journal = osb->journal->j_journal;
	unsigned long commit_interval = OCFS2_DEFAULT_COMMIT_INTERVAL;

	if (osb->osb_commit_interval)
		commit_interval = osb->osb_commit_interval;

	write_lock(&journal->j_state_lock);
	journal->j_commit_interval = commit_interval;
	if (osb->s_mount_opt & OCFS2_MOUNT_BARRIER)
		journal->j_flags |= JBD2_BARRIER;
	else
		journal->j_flags &= ~JBD2_BARRIER;
	write_unlock(&journal->j_state_lock);
}

int ocfs2_journal_init(struct ocfs2_journal *journal, int *dirty)
{
	int status = -1;
	struct inode *inode = NULL; 
	journal_t *j_journal = NULL;
	struct ocfs2_dinode *di = NULL;
	struct buffer_head *bh = NULL;
	struct ocfs2_super *osb;
	int inode_lock = 0;

	BUG_ON(!journal);

	osb = journal->j_osb;

	
	inode = ocfs2_get_system_file_inode(osb, JOURNAL_SYSTEM_INODE,
					    osb->slot_num);
	if (inode == NULL) {
		status = -EACCES;
		mlog_errno(status);
		goto done;
	}
	if (is_bad_inode(inode)) {
		mlog(ML_ERROR, "access error (bad inode)\n");
		iput(inode);
		inode = NULL;
		status = -EACCES;
		goto done;
	}

	SET_INODE_JOURNAL(inode);
	OCFS2_I(inode)->ip_open_count++;

	status = ocfs2_inode_lock_full(inode, &bh, 1, OCFS2_META_LOCK_RECOVERY);
	if (status < 0) {
		if (status != -ERESTARTSYS)
			mlog(ML_ERROR, "Could not get lock on journal!\n");
		goto done;
	}

	inode_lock = 1;
	di = (struct ocfs2_dinode *)bh->b_data;

	if (inode->i_size <  OCFS2_MIN_JOURNAL_SIZE) {
		mlog(ML_ERROR, "Journal file size (%lld) is too small!\n",
		     inode->i_size);
		status = -EINVAL;
		goto done;
	}

	trace_ocfs2_journal_init(inode->i_size,
				 (unsigned long long)inode->i_blocks,
				 OCFS2_I(inode)->ip_clusters);

	
	j_journal = jbd2_journal_init_inode(inode);
	if (j_journal == NULL) {
		mlog(ML_ERROR, "Linux journal layer error\n");
		status = -EINVAL;
		goto done;
	}

	trace_ocfs2_journal_init_maxlen(j_journal->j_maxlen);

	*dirty = (le32_to_cpu(di->id1.journal1.ij_flags) &
		  OCFS2_JOURNAL_DIRTY_FL);

	journal->j_journal = j_journal;
	journal->j_inode = inode;
	journal->j_bh = bh;

	ocfs2_set_journal_params(osb);

	journal->j_state = OCFS2_JOURNAL_LOADED;

	status = 0;
done:
	if (status < 0) {
		if (inode_lock)
			ocfs2_inode_unlock(inode, 1);
		brelse(bh);
		if (inode) {
			OCFS2_I(inode)->ip_open_count--;
			iput(inode);
		}
	}

	return status;
}

static void ocfs2_bump_recovery_generation(struct ocfs2_dinode *di)
{
	le32_add_cpu(&(di->id1.journal1.ij_recovery_generation), 1);
}

static u32 ocfs2_get_recovery_generation(struct ocfs2_dinode *di)
{
	return le32_to_cpu(di->id1.journal1.ij_recovery_generation);
}

static int ocfs2_journal_toggle_dirty(struct ocfs2_super *osb,
				      int dirty, int replayed)
{
	int status;
	unsigned int flags;
	struct ocfs2_journal *journal = osb->journal;
	struct buffer_head *bh = journal->j_bh;
	struct ocfs2_dinode *fe;

	fe = (struct ocfs2_dinode *)bh->b_data;

	BUG_ON(!OCFS2_IS_VALID_DINODE(fe));

	flags = le32_to_cpu(fe->id1.journal1.ij_flags);
	if (dirty)
		flags |= OCFS2_JOURNAL_DIRTY_FL;
	else
		flags &= ~OCFS2_JOURNAL_DIRTY_FL;
	fe->id1.journal1.ij_flags = cpu_to_le32(flags);

	if (replayed)
		ocfs2_bump_recovery_generation(fe);

	ocfs2_compute_meta_ecc(osb->sb, bh->b_data, &fe->i_check);
	status = ocfs2_write_block(osb, bh, INODE_CACHE(journal->j_inode));
	if (status < 0)
		mlog_errno(status);

	return status;
}

void ocfs2_journal_shutdown(struct ocfs2_super *osb)
{
	struct ocfs2_journal *journal = NULL;
	int status = 0;
	struct inode *inode = NULL;
	int num_running_trans = 0;

	BUG_ON(!osb);

	journal = osb->journal;
	if (!journal)
		goto done;

	inode = journal->j_inode;

	if (journal->j_state != OCFS2_JOURNAL_LOADED)
		goto done;

	
	if (!igrab(inode))
		BUG();

	num_running_trans = atomic_read(&(osb->journal->j_num_trans));
	trace_ocfs2_journal_shutdown(num_running_trans);

	journal->j_state = OCFS2_JOURNAL_IN_SHUTDOWN;

	if (osb->commit_task) {
		
		trace_ocfs2_journal_shutdown_wait(osb->commit_task);
		kthread_stop(osb->commit_task);
		osb->commit_task = NULL;
	}

	BUG_ON(atomic_read(&(osb->journal->j_num_trans)) != 0);

	if (ocfs2_mount_local(osb)) {
		jbd2_journal_lock_updates(journal->j_journal);
		status = jbd2_journal_flush(journal->j_journal);
		jbd2_journal_unlock_updates(journal->j_journal);
		if (status < 0)
			mlog_errno(status);
	}

	if (status == 0) {
		status = ocfs2_journal_toggle_dirty(osb, 0, 0);
		if (status < 0)
			mlog_errno(status);
	}

	
	jbd2_journal_destroy(journal->j_journal);
	journal->j_journal = NULL;

	OCFS2_I(inode)->ip_open_count--;

	
	ocfs2_inode_unlock(inode, 1);

	brelse(journal->j_bh);
	journal->j_bh = NULL;

	journal->j_state = OCFS2_JOURNAL_FREE;

done:
	if (inode)
		iput(inode);
}

static void ocfs2_clear_journal_error(struct super_block *sb,
				      journal_t *journal,
				      int slot)
{
	int olderr;

	olderr = jbd2_journal_errno(journal);
	if (olderr) {
		mlog(ML_ERROR, "File system error %d recorded in "
		     "journal %u.\n", olderr, slot);
		mlog(ML_ERROR, "File system on device %s needs checking.\n",
		     sb->s_id);

		jbd2_journal_ack_err(journal);
		jbd2_journal_clear_err(journal);
	}
}

int ocfs2_journal_load(struct ocfs2_journal *journal, int local, int replayed)
{
	int status = 0;
	struct ocfs2_super *osb;

	BUG_ON(!journal);

	osb = journal->j_osb;

	status = jbd2_journal_load(journal->j_journal);
	if (status < 0) {
		mlog(ML_ERROR, "Failed to load journal!\n");
		goto done;
	}

	ocfs2_clear_journal_error(osb->sb, journal->j_journal, osb->slot_num);

	status = ocfs2_journal_toggle_dirty(osb, 1, replayed);
	if (status < 0) {
		mlog_errno(status);
		goto done;
	}

	
	if (!local) {
		osb->commit_task = kthread_run(ocfs2_commit_thread, osb,
					       "ocfs2cmt");
		if (IS_ERR(osb->commit_task)) {
			status = PTR_ERR(osb->commit_task);
			osb->commit_task = NULL;
			mlog(ML_ERROR, "unable to launch ocfs2commit thread, "
			     "error=%d", status);
			goto done;
		}
	} else
		osb->commit_task = NULL;

done:
	return status;
}


int ocfs2_journal_wipe(struct ocfs2_journal *journal, int full)
{
	int status;

	BUG_ON(!journal);

	status = jbd2_journal_wipe(journal->j_journal, full);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_journal_toggle_dirty(journal->j_osb, 0, 0);
	if (status < 0)
		mlog_errno(status);

bail:
	return status;
}

static int ocfs2_recovery_completed(struct ocfs2_super *osb)
{
	int empty;
	struct ocfs2_recovery_map *rm = osb->recovery_map;

	spin_lock(&osb->osb_lock);
	empty = (rm->rm_used == 0);
	spin_unlock(&osb->osb_lock);

	return empty;
}

void ocfs2_wait_for_recovery(struct ocfs2_super *osb)
{
	wait_event(osb->recovery_event, ocfs2_recovery_completed(osb));
}

static int ocfs2_force_read_journal(struct inode *inode)
{
	int status = 0;
	int i;
	u64 v_blkno, p_blkno, p_blocks, num_blocks;
#define CONCURRENT_JOURNAL_FILL 32ULL
	struct buffer_head *bhs[CONCURRENT_JOURNAL_FILL];

	memset(bhs, 0, sizeof(struct buffer_head *) * CONCURRENT_JOURNAL_FILL);

	num_blocks = ocfs2_blocks_for_bytes(inode->i_sb, inode->i_size);
	v_blkno = 0;
	while (v_blkno < num_blocks) {
		status = ocfs2_extent_map_get_blocks(inode, v_blkno,
						     &p_blkno, &p_blocks, NULL);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		if (p_blocks > CONCURRENT_JOURNAL_FILL)
			p_blocks = CONCURRENT_JOURNAL_FILL;

		status = ocfs2_read_blocks_sync(OCFS2_SB(inode->i_sb),
						p_blkno, p_blocks, bhs);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		for(i = 0; i < p_blocks; i++) {
			brelse(bhs[i]);
			bhs[i] = NULL;
		}

		v_blkno += p_blocks;
	}

bail:
	for(i = 0; i < CONCURRENT_JOURNAL_FILL; i++)
		brelse(bhs[i]);
	return status;
}

struct ocfs2_la_recovery_item {
	struct list_head	lri_list;
	int			lri_slot;
	struct ocfs2_dinode	*lri_la_dinode;
	struct ocfs2_dinode	*lri_tl_dinode;
	struct ocfs2_quota_recovery *lri_qrec;
};

void ocfs2_complete_recovery(struct work_struct *work)
{
	int ret = 0;
	struct ocfs2_journal *journal =
		container_of(work, struct ocfs2_journal, j_recovery_work);
	struct ocfs2_super *osb = journal->j_osb;
	struct ocfs2_dinode *la_dinode, *tl_dinode;
	struct ocfs2_la_recovery_item *item, *n;
	struct ocfs2_quota_recovery *qrec;
	LIST_HEAD(tmp_la_list);

	trace_ocfs2_complete_recovery(
		(unsigned long long)OCFS2_I(journal->j_inode)->ip_blkno);

	spin_lock(&journal->j_lock);
	list_splice_init(&journal->j_la_cleanups, &tmp_la_list);
	spin_unlock(&journal->j_lock);

	list_for_each_entry_safe(item, n, &tmp_la_list, lri_list) {
		list_del_init(&item->lri_list);

		ocfs2_wait_on_quotas(osb);

		la_dinode = item->lri_la_dinode;
		tl_dinode = item->lri_tl_dinode;
		qrec = item->lri_qrec;

		trace_ocfs2_complete_recovery_slot(item->lri_slot,
			la_dinode ? le64_to_cpu(la_dinode->i_blkno) : 0,
			tl_dinode ? le64_to_cpu(tl_dinode->i_blkno) : 0,
			qrec);

		if (la_dinode) {
			ret = ocfs2_complete_local_alloc_recovery(osb,
								  la_dinode);
			if (ret < 0)
				mlog_errno(ret);

			kfree(la_dinode);
		}

		if (tl_dinode) {
			ret = ocfs2_complete_truncate_log_recovery(osb,
								   tl_dinode);
			if (ret < 0)
				mlog_errno(ret);

			kfree(tl_dinode);
		}

		ret = ocfs2_recover_orphans(osb, item->lri_slot);
		if (ret < 0)
			mlog_errno(ret);

		if (qrec) {
			ret = ocfs2_finish_quota_recovery(osb, qrec,
							  item->lri_slot);
			if (ret < 0)
				mlog_errno(ret);
			
		}

		kfree(item);
	}

	trace_ocfs2_complete_recovery_end(ret);
}

static void ocfs2_queue_recovery_completion(struct ocfs2_journal *journal,
					    int slot_num,
					    struct ocfs2_dinode *la_dinode,
					    struct ocfs2_dinode *tl_dinode,
					    struct ocfs2_quota_recovery *qrec)
{
	struct ocfs2_la_recovery_item *item;

	item = kmalloc(sizeof(struct ocfs2_la_recovery_item), GFP_NOFS);
	if (!item) {
		if (la_dinode)
			kfree(la_dinode);

		if (tl_dinode)
			kfree(tl_dinode);

		if (qrec)
			ocfs2_free_quota_recovery(qrec);

		mlog_errno(-ENOMEM);
		return;
	}

	INIT_LIST_HEAD(&item->lri_list);
	item->lri_la_dinode = la_dinode;
	item->lri_slot = slot_num;
	item->lri_tl_dinode = tl_dinode;
	item->lri_qrec = qrec;

	spin_lock(&journal->j_lock);
	list_add_tail(&item->lri_list, &journal->j_la_cleanups);
	queue_work(ocfs2_wq, &journal->j_recovery_work);
	spin_unlock(&journal->j_lock);
}

void ocfs2_complete_mount_recovery(struct ocfs2_super *osb)
{
	struct ocfs2_journal *journal = osb->journal;

	if (ocfs2_is_hard_readonly(osb))
		return;

	ocfs2_queue_recovery_completion(journal, osb->slot_num,
					osb->local_alloc_copy, NULL, NULL);
	ocfs2_schedule_truncate_log_flush(osb, 0);

	osb->local_alloc_copy = NULL;
	osb->dirty = 0;

	
	ocfs2_replay_map_set_state(osb, REPLAY_NEEDED);
	ocfs2_queue_replay_slots(osb);
	ocfs2_free_replay_slots(osb);
}

void ocfs2_complete_quota_recovery(struct ocfs2_super *osb)
{
	if (osb->quota_rec) {
		ocfs2_queue_recovery_completion(osb->journal,
						osb->slot_num,
						NULL,
						NULL,
						osb->quota_rec);
		osb->quota_rec = NULL;
	}
}

static int __ocfs2_recovery_thread(void *arg)
{
	int status, node_num, slot_num;
	struct ocfs2_super *osb = arg;
	struct ocfs2_recovery_map *rm = osb->recovery_map;
	int *rm_quota = NULL;
	int rm_quota_used = 0, i;
	struct ocfs2_quota_recovery *qrec;

	status = ocfs2_wait_on_mount(osb);
	if (status < 0) {
		goto bail;
	}

	rm_quota = kzalloc(osb->max_slots * sizeof(int), GFP_NOFS);
	if (!rm_quota) {
		status = -ENOMEM;
		goto bail;
	}
restart:
	status = ocfs2_super_lock(osb, 1);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_compute_replay_slots(osb);
	if (status < 0)
		mlog_errno(status);

	
	ocfs2_queue_recovery_completion(osb->journal, osb->slot_num, NULL,
					NULL, NULL);

	spin_lock(&osb->osb_lock);
	while (rm->rm_used) {
		node_num = rm->rm_entries[0];
		spin_unlock(&osb->osb_lock);
		slot_num = ocfs2_node_num_to_slot(osb, node_num);
		trace_ocfs2_recovery_thread_node(node_num, slot_num);
		if (slot_num == -ENOENT) {
			status = 0;
			goto skip_recovery;
		}

		for (i = 0; i < rm_quota_used && rm_quota[i] != slot_num; i++);
		if (i == rm_quota_used)
			rm_quota[rm_quota_used++] = slot_num;

		status = ocfs2_recover_node(osb, node_num, slot_num);
skip_recovery:
		if (!status) {
			ocfs2_recovery_map_clear(osb, node_num);
		} else {
			mlog(ML_ERROR,
			     "Error %d recovering node %d on device (%u,%u)!\n",
			     status, node_num,
			     MAJOR(osb->sb->s_dev), MINOR(osb->sb->s_dev));
			mlog(ML_ERROR, "Volume requires unmount.\n");
		}

		spin_lock(&osb->osb_lock);
	}
	spin_unlock(&osb->osb_lock);
	trace_ocfs2_recovery_thread_end(status);

	
	status = ocfs2_check_journals_nolocks(osb);
	status = (status == -EROFS) ? 0 : status;
	if (status < 0)
		mlog_errno(status);

	for (i = 0; i < rm_quota_used; i++) {
		qrec = ocfs2_begin_quota_recovery(osb, rm_quota[i]);
		if (IS_ERR(qrec)) {
			status = PTR_ERR(qrec);
			mlog_errno(status);
			continue;
		}
		ocfs2_queue_recovery_completion(osb->journal, rm_quota[i],
						NULL, NULL, qrec);
	}

	ocfs2_super_unlock(osb, 1);

	
	ocfs2_queue_replay_slots(osb);

bail:
	mutex_lock(&osb->recovery_lock);
	if (!status && !ocfs2_recovery_completed(osb)) {
		mutex_unlock(&osb->recovery_lock);
		goto restart;
	}

	ocfs2_free_replay_slots(osb);
	osb->recovery_thread_task = NULL;
	mb(); 
	wake_up(&osb->recovery_event);

	mutex_unlock(&osb->recovery_lock);

	if (rm_quota)
		kfree(rm_quota);

	complete_and_exit(NULL, status);
	return status;
}

void ocfs2_recovery_thread(struct ocfs2_super *osb, int node_num)
{
	mutex_lock(&osb->recovery_lock);

	trace_ocfs2_recovery_thread(node_num, osb->node_num,
		osb->disable_recovery, osb->recovery_thread_task,
		osb->disable_recovery ?
		-1 : ocfs2_recovery_map_set(osb, node_num));

	if (osb->disable_recovery)
		goto out;

	if (osb->recovery_thread_task)
		goto out;

	osb->recovery_thread_task =  kthread_run(__ocfs2_recovery_thread, osb,
						 "ocfs2rec");
	if (IS_ERR(osb->recovery_thread_task)) {
		mlog_errno((int)PTR_ERR(osb->recovery_thread_task));
		osb->recovery_thread_task = NULL;
	}

out:
	mutex_unlock(&osb->recovery_lock);
	wake_up(&osb->recovery_event);
}

static int ocfs2_read_journal_inode(struct ocfs2_super *osb,
				    int slot_num,
				    struct buffer_head **bh,
				    struct inode **ret_inode)
{
	int status = -EACCES;
	struct inode *inode = NULL;

	BUG_ON(slot_num >= osb->max_slots);

	inode = ocfs2_get_system_file_inode(osb, JOURNAL_SYSTEM_INODE,
					    slot_num);
	if (!inode || is_bad_inode(inode)) {
		mlog_errno(status);
		goto bail;
	}
	SET_INODE_JOURNAL(inode);

	status = ocfs2_read_inode_block_full(inode, bh, OCFS2_BH_IGNORE_CACHE);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = 0;

bail:
	if (inode) {
		if (status || !ret_inode)
			iput(inode);
		else
			*ret_inode = inode;
	}
	return status;
}

static int ocfs2_replay_journal(struct ocfs2_super *osb,
				int node_num,
				int slot_num)
{
	int status;
	int got_lock = 0;
	unsigned int flags;
	struct inode *inode = NULL;
	struct ocfs2_dinode *fe;
	journal_t *journal = NULL;
	struct buffer_head *bh = NULL;
	u32 slot_reco_gen;

	status = ocfs2_read_journal_inode(osb, slot_num, &bh, &inode);
	if (status) {
		mlog_errno(status);
		goto done;
	}

	fe = (struct ocfs2_dinode *)bh->b_data;
	slot_reco_gen = ocfs2_get_recovery_generation(fe);
	brelse(bh);
	bh = NULL;

	if (osb->slot_recovery_generations[slot_num] != slot_reco_gen) {
		trace_ocfs2_replay_journal_recovered(slot_num,
		     osb->slot_recovery_generations[slot_num], slot_reco_gen);
		osb->slot_recovery_generations[slot_num] = slot_reco_gen;
		status = -EBUSY;
		goto done;
	}

	

	status = ocfs2_inode_lock_full(inode, &bh, 1, OCFS2_META_LOCK_RECOVERY);
	if (status < 0) {
		trace_ocfs2_replay_journal_lock_err(status);
		if (status != -ERESTARTSYS)
			mlog(ML_ERROR, "Could not lock journal!\n");
		goto done;
	}
	got_lock = 1;

	fe = (struct ocfs2_dinode *) bh->b_data;

	flags = le32_to_cpu(fe->id1.journal1.ij_flags);
	slot_reco_gen = ocfs2_get_recovery_generation(fe);

	if (!(flags & OCFS2_JOURNAL_DIRTY_FL)) {
		trace_ocfs2_replay_journal_skip(node_num);
		
		osb->slot_recovery_generations[slot_num] = slot_reco_gen;
		goto done;
	}

	
	ocfs2_replay_map_set_state(osb, REPLAY_NEEDED);

	printk(KERN_NOTICE "ocfs2: Begin replay journal (node %d, slot %d) on "\
	       "device (%u,%u)\n", node_num, slot_num, MAJOR(osb->sb->s_dev),
	       MINOR(osb->sb->s_dev));

	OCFS2_I(inode)->ip_clusters = le32_to_cpu(fe->i_clusters);

	status = ocfs2_force_read_journal(inode);
	if (status < 0) {
		mlog_errno(status);
		goto done;
	}

	journal = jbd2_journal_init_inode(inode);
	if (journal == NULL) {
		mlog(ML_ERROR, "Linux journal layer error\n");
		status = -EIO;
		goto done;
	}

	status = jbd2_journal_load(journal);
	if (status < 0) {
		mlog_errno(status);
		if (!igrab(inode))
			BUG();
		jbd2_journal_destroy(journal);
		goto done;
	}

	ocfs2_clear_journal_error(osb->sb, journal, slot_num);

	
	jbd2_journal_lock_updates(journal);
	status = jbd2_journal_flush(journal);
	jbd2_journal_unlock_updates(journal);
	if (status < 0)
		mlog_errno(status);

	
	flags = le32_to_cpu(fe->id1.journal1.ij_flags);
	flags &= ~OCFS2_JOURNAL_DIRTY_FL;
	fe->id1.journal1.ij_flags = cpu_to_le32(flags);

	
	ocfs2_bump_recovery_generation(fe);
	osb->slot_recovery_generations[slot_num] =
					ocfs2_get_recovery_generation(fe);

	ocfs2_compute_meta_ecc(osb->sb, bh->b_data, &fe->i_check);
	status = ocfs2_write_block(osb, bh, INODE_CACHE(inode));
	if (status < 0)
		mlog_errno(status);

	if (!igrab(inode))
		BUG();

	jbd2_journal_destroy(journal);

	printk(KERN_NOTICE "ocfs2: End replay journal (node %d, slot %d) on "\
	       "device (%u,%u)\n", node_num, slot_num, MAJOR(osb->sb->s_dev),
	       MINOR(osb->sb->s_dev));
done:
	
	if (got_lock)
		ocfs2_inode_unlock(inode, 1);

	if (inode)
		iput(inode);

	brelse(bh);

	return status;
}

static int ocfs2_recover_node(struct ocfs2_super *osb,
			      int node_num, int slot_num)
{
	int status = 0;
	struct ocfs2_dinode *la_copy = NULL;
	struct ocfs2_dinode *tl_copy = NULL;

	trace_ocfs2_recover_node(node_num, slot_num, osb->node_num);

	BUG_ON(osb->node_num == node_num);

	status = ocfs2_replay_journal(osb, node_num, slot_num);
	if (status < 0) {
		if (status == -EBUSY) {
			trace_ocfs2_recover_node_skip(slot_num, node_num);
			status = 0;
			goto done;
		}
		mlog_errno(status);
		goto done;
	}

	
	status = ocfs2_begin_local_alloc_recovery(osb, slot_num, &la_copy);
	if (status < 0) {
		mlog_errno(status);
		goto done;
	}

	status = ocfs2_begin_truncate_log_recovery(osb, slot_num, &tl_copy);
	if (status < 0)
		mlog_errno(status);

	status = ocfs2_clear_slot(osb, slot_num);
	if (status < 0)
		mlog_errno(status);

	
	ocfs2_queue_recovery_completion(osb->journal, slot_num, la_copy,
					tl_copy, NULL);

	status = 0;
done:

	return status;
}

static int ocfs2_trylock_journal(struct ocfs2_super *osb,
				 int slot_num)
{
	int status, flags;
	struct inode *inode = NULL;

	inode = ocfs2_get_system_file_inode(osb, JOURNAL_SYSTEM_INODE,
					    slot_num);
	if (inode == NULL) {
		mlog(ML_ERROR, "access error\n");
		status = -EACCES;
		goto bail;
	}
	if (is_bad_inode(inode)) {
		mlog(ML_ERROR, "access error (bad inode)\n");
		iput(inode);
		inode = NULL;
		status = -EACCES;
		goto bail;
	}
	SET_INODE_JOURNAL(inode);

	flags = OCFS2_META_LOCK_RECOVERY | OCFS2_META_LOCK_NOQUEUE;
	status = ocfs2_inode_lock_full(inode, NULL, 1, flags);
	if (status < 0) {
		if (status != -EAGAIN)
			mlog_errno(status);
		goto bail;
	}

	ocfs2_inode_unlock(inode, 1);
bail:
	if (inode)
		iput(inode);

	return status;
}

int ocfs2_mark_dead_nodes(struct ocfs2_super *osb)
{
	unsigned int node_num;
	int status, i;
	u32 gen;
	struct buffer_head *bh = NULL;
	struct ocfs2_dinode *di;


	for (i = 0; i < osb->max_slots; i++) {
		
		status = ocfs2_read_journal_inode(osb, i, &bh, NULL);
		if (status) {
			mlog_errno(status);
			goto bail;
		}
		di = (struct ocfs2_dinode *)bh->b_data;
		gen = ocfs2_get_recovery_generation(di);
		brelse(bh);
		bh = NULL;

		spin_lock(&osb->osb_lock);
		osb->slot_recovery_generations[i] = gen;

		trace_ocfs2_mark_dead_nodes(i,
					    osb->slot_recovery_generations[i]);

		if (i == osb->slot_num) {
			spin_unlock(&osb->osb_lock);
			continue;
		}

		status = ocfs2_slot_to_node_num_locked(osb, i, &node_num);
		if (status == -ENOENT) {
			spin_unlock(&osb->osb_lock);
			continue;
		}

		if (__ocfs2_recovery_map_test(osb, node_num)) {
			spin_unlock(&osb->osb_lock);
			continue;
		}
		spin_unlock(&osb->osb_lock);

		status = ocfs2_trylock_journal(osb, i);
		if (!status) {
			ocfs2_recovery_thread(osb, node_num);
		} else if ((status < 0) && (status != -EAGAIN)) {
			mlog_errno(status);
			goto bail;
		}
	}

	status = 0;
bail:
	return status;
}

static inline unsigned long ocfs2_orphan_scan_timeout(void)
{
	unsigned long time;

	get_random_bytes(&time, sizeof(time));
	time = ORPHAN_SCAN_SCHEDULE_TIMEOUT + (time % 5000);
	return msecs_to_jiffies(time);
}

void ocfs2_queue_orphan_scan(struct ocfs2_super *osb)
{
	struct ocfs2_orphan_scan *os;
	int status, i;
	u32 seqno = 0;

	os = &osb->osb_orphan_scan;

	if (atomic_read(&os->os_state) == ORPHAN_SCAN_INACTIVE)
		goto out;

	trace_ocfs2_queue_orphan_scan_begin(os->os_count, os->os_seqno,
					    atomic_read(&os->os_state));

	status = ocfs2_orphan_scan_lock(osb, &seqno);
	if (status < 0) {
		if (status != -EAGAIN)
			mlog_errno(status);
		goto out;
	}

	
	if (atomic_read(&os->os_state) == ORPHAN_SCAN_INACTIVE)
		goto unlock;

	if (os->os_seqno != seqno) {
		os->os_seqno = seqno;
		goto unlock;
	}

	for (i = 0; i < osb->max_slots; i++)
		ocfs2_queue_recovery_completion(osb->journal, i, NULL, NULL,
						NULL);
	seqno++;
	os->os_count++;
	os->os_scantime = CURRENT_TIME;
unlock:
	ocfs2_orphan_scan_unlock(osb, seqno);
out:
	trace_ocfs2_queue_orphan_scan_end(os->os_count, os->os_seqno,
					  atomic_read(&os->os_state));
	return;
}

void ocfs2_orphan_scan_work(struct work_struct *work)
{
	struct ocfs2_orphan_scan *os;
	struct ocfs2_super *osb;

	os = container_of(work, struct ocfs2_orphan_scan,
			  os_orphan_scan_work.work);
	osb = os->os_osb;

	mutex_lock(&os->os_lock);
	ocfs2_queue_orphan_scan(osb);
	if (atomic_read(&os->os_state) == ORPHAN_SCAN_ACTIVE)
		queue_delayed_work(ocfs2_wq, &os->os_orphan_scan_work,
				      ocfs2_orphan_scan_timeout());
	mutex_unlock(&os->os_lock);
}

void ocfs2_orphan_scan_stop(struct ocfs2_super *osb)
{
	struct ocfs2_orphan_scan *os;

	os = &osb->osb_orphan_scan;
	if (atomic_read(&os->os_state) == ORPHAN_SCAN_ACTIVE) {
		atomic_set(&os->os_state, ORPHAN_SCAN_INACTIVE);
		mutex_lock(&os->os_lock);
		cancel_delayed_work(&os->os_orphan_scan_work);
		mutex_unlock(&os->os_lock);
	}
}

void ocfs2_orphan_scan_init(struct ocfs2_super *osb)
{
	struct ocfs2_orphan_scan *os;

	os = &osb->osb_orphan_scan;
	os->os_osb = osb;
	os->os_count = 0;
	os->os_seqno = 0;
	mutex_init(&os->os_lock);
	INIT_DELAYED_WORK(&os->os_orphan_scan_work, ocfs2_orphan_scan_work);
}

void ocfs2_orphan_scan_start(struct ocfs2_super *osb)
{
	struct ocfs2_orphan_scan *os;

	os = &osb->osb_orphan_scan;
	os->os_scantime = CURRENT_TIME;
	if (ocfs2_is_hard_readonly(osb) || ocfs2_mount_local(osb))
		atomic_set(&os->os_state, ORPHAN_SCAN_INACTIVE);
	else {
		atomic_set(&os->os_state, ORPHAN_SCAN_ACTIVE);
		queue_delayed_work(ocfs2_wq, &os->os_orphan_scan_work,
				   ocfs2_orphan_scan_timeout());
	}
}

struct ocfs2_orphan_filldir_priv {
	struct inode		*head;
	struct ocfs2_super	*osb;
};

static int ocfs2_orphan_filldir(void *priv, const char *name, int name_len,
				loff_t pos, u64 ino, unsigned type)
{
	struct ocfs2_orphan_filldir_priv *p = priv;
	struct inode *iter;

	if (name_len == 1 && !strncmp(".", name, 1))
		return 0;
	if (name_len == 2 && !strncmp("..", name, 2))
		return 0;

	
	iter = ocfs2_iget(p->osb, ino,
			  OCFS2_FI_FLAG_ORPHAN_RECOVERY, 0);
	if (IS_ERR(iter))
		return 0;

	trace_ocfs2_orphan_filldir((unsigned long long)OCFS2_I(iter)->ip_blkno);
	OCFS2_I(iter)->ip_next_orphan = p->head;
	p->head = iter;

	return 0;
}

static int ocfs2_queue_orphans(struct ocfs2_super *osb,
			       int slot,
			       struct inode **head)
{
	int status;
	struct inode *orphan_dir_inode = NULL;
	struct ocfs2_orphan_filldir_priv priv;
	loff_t pos = 0;

	priv.osb = osb;
	priv.head = *head;

	orphan_dir_inode = ocfs2_get_system_file_inode(osb,
						       ORPHAN_DIR_SYSTEM_INODE,
						       slot);
	if  (!orphan_dir_inode) {
		status = -ENOENT;
		mlog_errno(status);
		return status;
	}

	mutex_lock(&orphan_dir_inode->i_mutex);
	status = ocfs2_inode_lock(orphan_dir_inode, NULL, 0);
	if (status < 0) {
		mlog_errno(status);
		goto out;
	}

	status = ocfs2_dir_foreach(orphan_dir_inode, &pos, &priv,
				   ocfs2_orphan_filldir);
	if (status) {
		mlog_errno(status);
		goto out_cluster;
	}

	*head = priv.head;

out_cluster:
	ocfs2_inode_unlock(orphan_dir_inode, 0);
out:
	mutex_unlock(&orphan_dir_inode->i_mutex);
	iput(orphan_dir_inode);
	return status;
}

static int ocfs2_orphan_recovery_can_continue(struct ocfs2_super *osb,
					      int slot)
{
	int ret;

	spin_lock(&osb->osb_lock);
	ret = !osb->osb_orphan_wipes[slot];
	spin_unlock(&osb->osb_lock);
	return ret;
}

static void ocfs2_mark_recovering_orphan_dir(struct ocfs2_super *osb,
					     int slot)
{
	spin_lock(&osb->osb_lock);
	ocfs2_node_map_set_bit(osb, &osb->osb_recovering_orphan_dirs, slot);
	while (osb->osb_orphan_wipes[slot]) {
		spin_unlock(&osb->osb_lock);
		wait_event_interruptible(osb->osb_wipe_event,
					 ocfs2_orphan_recovery_can_continue(osb, slot));
		spin_lock(&osb->osb_lock);
	}
	spin_unlock(&osb->osb_lock);
}

static void ocfs2_clear_recovering_orphan_dir(struct ocfs2_super *osb,
					      int slot)
{
	ocfs2_node_map_clear_bit(osb, &osb->osb_recovering_orphan_dirs, slot);
}

static int ocfs2_recover_orphans(struct ocfs2_super *osb,
				 int slot)
{
	int ret = 0;
	struct inode *inode = NULL;
	struct inode *iter;
	struct ocfs2_inode_info *oi;

	trace_ocfs2_recover_orphans(slot);

	ocfs2_mark_recovering_orphan_dir(osb, slot);
	ret = ocfs2_queue_orphans(osb, slot, &inode);
	ocfs2_clear_recovering_orphan_dir(osb, slot);

	if (ret)
		mlog_errno(ret);

	while (inode) {
		oi = OCFS2_I(inode);
		trace_ocfs2_recover_orphans_iput(
					(unsigned long long)oi->ip_blkno);

		iter = oi->ip_next_orphan;

		spin_lock(&oi->ip_lock);
		oi->ip_flags &= ~(OCFS2_INODE_DELETED|OCFS2_INODE_SKIP_DELETE);

		oi->ip_flags |= OCFS2_INODE_MAYBE_ORPHANED;
		spin_unlock(&oi->ip_lock);

		iput(inode);

		inode = iter;
	}

	return ret;
}

static int __ocfs2_wait_on_mount(struct ocfs2_super *osb, int quota)
{
	wait_event(osb->osb_mount_event,
		  (!quota && atomic_read(&osb->vol_state) == VOLUME_MOUNTED) ||
		   atomic_read(&osb->vol_state) == VOLUME_MOUNTED_QUOTAS ||
		   atomic_read(&osb->vol_state) == VOLUME_DISABLED);

	if (atomic_read(&osb->vol_state) == VOLUME_DISABLED) {
		trace_ocfs2_wait_on_mount(VOLUME_DISABLED);
		mlog(0, "mount error, exiting!\n");
		return -EBUSY;
	}

	return 0;
}

static int ocfs2_commit_thread(void *arg)
{
	int status;
	struct ocfs2_super *osb = arg;
	struct ocfs2_journal *journal = osb->journal;

	while (!(kthread_should_stop() &&
		 atomic_read(&journal->j_num_trans) == 0)) {

		wait_event_interruptible(osb->checkpoint_event,
					 atomic_read(&journal->j_num_trans)
					 || kthread_should_stop());

		status = ocfs2_commit_cache(osb);
		if (status < 0)
			mlog_errno(status);

		if (kthread_should_stop() && atomic_read(&journal->j_num_trans)){
			mlog(ML_KTHREAD,
			     "commit_thread: %u transactions pending on "
			     "shutdown\n",
			     atomic_read(&journal->j_num_trans));
		}
	}

	return 0;
}

int ocfs2_check_journals_nolocks(struct ocfs2_super *osb)
{
	int ret = 0;
	unsigned int slot;
	struct buffer_head *di_bh = NULL;
	struct ocfs2_dinode *di;
	int journal_dirty = 0;

	for(slot = 0; slot < osb->max_slots; slot++) {
		ret = ocfs2_read_journal_inode(osb, slot, &di_bh, NULL);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		di = (struct ocfs2_dinode *) di_bh->b_data;

		osb->slot_recovery_generations[slot] =
					ocfs2_get_recovery_generation(di);

		if (le32_to_cpu(di->id1.journal1.ij_flags) &
		    OCFS2_JOURNAL_DIRTY_FL)
			journal_dirty = 1;

		brelse(di_bh);
		di_bh = NULL;
	}

out:
	if (journal_dirty)
		ret = -EROFS;
	return ret;
}
