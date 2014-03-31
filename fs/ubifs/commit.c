/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Adrian Hunter
 *          Artem Bityutskiy (Битюцкий Артём)
 */

/*
 * This file implements functions that manage the running of the commit process.
 * Each affected module has its own functions to accomplish their part in the
 * commit and those functions are called here.
 *
 * The commit is the process whereby all updates to the index and LEB properties
 * are written out together and the journal becomes empty. This keeps the
 * file system consistent - at all times the state can be recreated by reading
 * the index and LEB properties and then replaying the journal.
 *
 * The commit is split into two parts named "commit start" and "commit end".
 * During commit start, the commit process has exclusive access to the journal
 * by holding the commit semaphore down for writing. As few I/O operations as
 * possible are performed during commit start, instead the nodes that are to be
 * written are merely identified. During commit end, the commit semaphore is no
 * longer held and the journal is again in operation, allowing users to continue
 * to use the file system while the bulk of the commit I/O is performed. The
 * purpose of this two-step approach is to prevent the commit from causing any
 * latency blips. Note that in any case, the commit does not prevent lookups
 * (as permitted by the TNC mutex), or access to VFS data structures e.g. page
 * cache.
 */

#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "ubifs.h"

static int nothing_to_commit(struct ubifs_info *c)
{
	if (c->mounting || c->remounting_rw)
		return 0;

	if (c->zroot.znode && ubifs_zn_dirty(c->zroot.znode))
		return 0;

	if (c->nroot && test_bit(DIRTY_CNODE, &c->nroot->flags))
		return 0;

	ubifs_assert(atomic_long_read(&c->dirty_zn_cnt) == 0);
	ubifs_assert(c->dirty_pn_cnt == 0);
	ubifs_assert(c->dirty_nn_cnt == 0);

	return 1;
}

static int do_commit(struct ubifs_info *c)
{
	int err, new_ltail_lnum, old_ltail_lnum, i;
	struct ubifs_zbranch zroot;
	struct ubifs_lp_stats lst;

	dbg_cmt("start");
	ubifs_assert(!c->ro_media && !c->ro_mount);

	if (c->ro_error) {
		err = -EROFS;
		goto out_up;
	}

	if (nothing_to_commit(c)) {
		up_write(&c->commit_sem);
		err = 0;
		goto out_cancel;
	}

	
	for (i = 0; i < c->jhead_cnt; i++) {
		err = ubifs_wbuf_sync(&c->jheads[i].wbuf);
		if (err)
			goto out_up;
	}

	c->cmt_no += 1;
	err = ubifs_gc_start_commit(c);
	if (err)
		goto out_up;
	err = dbg_check_lprops(c);
	if (err)
		goto out_up;
	err = ubifs_log_start_commit(c, &new_ltail_lnum);
	if (err)
		goto out_up;
	err = ubifs_tnc_start_commit(c, &zroot);
	if (err)
		goto out_up;
	err = ubifs_lpt_start_commit(c);
	if (err)
		goto out_up;
	err = ubifs_orphan_start_commit(c);
	if (err)
		goto out_up;

	ubifs_get_lp_stats(c, &lst);

	up_write(&c->commit_sem);

	err = ubifs_tnc_end_commit(c);
	if (err)
		goto out;
	err = ubifs_lpt_end_commit(c);
	if (err)
		goto out;
	err = ubifs_orphan_end_commit(c);
	if (err)
		goto out;
	old_ltail_lnum = c->ltail_lnum;
	err = ubifs_log_end_commit(c, new_ltail_lnum);
	if (err)
		goto out;
	err = dbg_check_old_index(c, &zroot);
	if (err)
		goto out;

	mutex_lock(&c->mst_mutex);
	c->mst_node->cmt_no      = cpu_to_le64(c->cmt_no);
	c->mst_node->log_lnum    = cpu_to_le32(new_ltail_lnum);
	c->mst_node->root_lnum   = cpu_to_le32(zroot.lnum);
	c->mst_node->root_offs   = cpu_to_le32(zroot.offs);
	c->mst_node->root_len    = cpu_to_le32(zroot.len);
	c->mst_node->ihead_lnum  = cpu_to_le32(c->ihead_lnum);
	c->mst_node->ihead_offs  = cpu_to_le32(c->ihead_offs);
	c->mst_node->index_size  = cpu_to_le64(c->bi.old_idx_sz);
	c->mst_node->lpt_lnum    = cpu_to_le32(c->lpt_lnum);
	c->mst_node->lpt_offs    = cpu_to_le32(c->lpt_offs);
	c->mst_node->nhead_lnum  = cpu_to_le32(c->nhead_lnum);
	c->mst_node->nhead_offs  = cpu_to_le32(c->nhead_offs);
	c->mst_node->ltab_lnum   = cpu_to_le32(c->ltab_lnum);
	c->mst_node->ltab_offs   = cpu_to_le32(c->ltab_offs);
	c->mst_node->lsave_lnum  = cpu_to_le32(c->lsave_lnum);
	c->mst_node->lsave_offs  = cpu_to_le32(c->lsave_offs);
	c->mst_node->lscan_lnum  = cpu_to_le32(c->lscan_lnum);
	c->mst_node->empty_lebs  = cpu_to_le32(lst.empty_lebs);
	c->mst_node->idx_lebs    = cpu_to_le32(lst.idx_lebs);
	c->mst_node->total_free  = cpu_to_le64(lst.total_free);
	c->mst_node->total_dirty = cpu_to_le64(lst.total_dirty);
	c->mst_node->total_used  = cpu_to_le64(lst.total_used);
	c->mst_node->total_dead  = cpu_to_le64(lst.total_dead);
	c->mst_node->total_dark  = cpu_to_le64(lst.total_dark);
	if (c->no_orphs)
		c->mst_node->flags |= cpu_to_le32(UBIFS_MST_NO_ORPHS);
	else
		c->mst_node->flags &= ~cpu_to_le32(UBIFS_MST_NO_ORPHS);
	err = ubifs_write_master(c);
	mutex_unlock(&c->mst_mutex);
	if (err)
		goto out;

	err = ubifs_log_post_commit(c, old_ltail_lnum);
	if (err)
		goto out;
	err = ubifs_gc_end_commit(c);
	if (err)
		goto out;
	err = ubifs_lpt_post_commit(c);
	if (err)
		goto out;

out_cancel:
	spin_lock(&c->cs_lock);
	c->cmt_state = COMMIT_RESTING;
	wake_up(&c->cmt_wq);
	dbg_cmt("commit end");
	spin_unlock(&c->cs_lock);
	return 0;

out_up:
	up_write(&c->commit_sem);
out:
	ubifs_err("commit failed, error %d", err);
	spin_lock(&c->cs_lock);
	c->cmt_state = COMMIT_BROKEN;
	wake_up(&c->cmt_wq);
	spin_unlock(&c->cs_lock);
	ubifs_ro_mode(c, err);
	return err;
}

static int run_bg_commit(struct ubifs_info *c)
{
	spin_lock(&c->cs_lock);
	if (c->cmt_state != COMMIT_BACKGROUND &&
	    c->cmt_state != COMMIT_REQUIRED)
		goto out;
	spin_unlock(&c->cs_lock);

	down_write(&c->commit_sem);
	spin_lock(&c->cs_lock);
	if (c->cmt_state == COMMIT_REQUIRED)
		c->cmt_state = COMMIT_RUNNING_REQUIRED;
	else if (c->cmt_state == COMMIT_BACKGROUND)
		c->cmt_state = COMMIT_RUNNING_BACKGROUND;
	else
		goto out_cmt_unlock;
	spin_unlock(&c->cs_lock);

	return do_commit(c);

out_cmt_unlock:
	up_write(&c->commit_sem);
out:
	spin_unlock(&c->cs_lock);
	return 0;
}

int ubifs_bg_thread(void *info)
{
	int err;
	struct ubifs_info *c = info;

	dbg_msg("background thread \"%s\" started, PID %d",
		c->bgt_name, current->pid);
	set_freezable();

	while (1) {
		if (kthread_should_stop())
			break;

		if (try_to_freeze())
			continue;

		set_current_state(TASK_INTERRUPTIBLE);
		
		if (!c->need_bgt) {
			if (kthread_should_stop())
				break;
			schedule();
			continue;
		} else
			__set_current_state(TASK_RUNNING);

		c->need_bgt = 0;
		err = ubifs_bg_wbufs_sync(c);
		if (err)
			ubifs_ro_mode(c, err);

		run_bg_commit(c);
		cond_resched();
	}

	dbg_msg("background thread \"%s\" stops", c->bgt_name);
	return 0;
}

void ubifs_commit_required(struct ubifs_info *c)
{
	spin_lock(&c->cs_lock);
	switch (c->cmt_state) {
	case COMMIT_RESTING:
	case COMMIT_BACKGROUND:
		dbg_cmt("old: %s, new: %s", dbg_cstate(c->cmt_state),
			dbg_cstate(COMMIT_REQUIRED));
		c->cmt_state = COMMIT_REQUIRED;
		break;
	case COMMIT_RUNNING_BACKGROUND:
		dbg_cmt("old: %s, new: %s", dbg_cstate(c->cmt_state),
			dbg_cstate(COMMIT_RUNNING_REQUIRED));
		c->cmt_state = COMMIT_RUNNING_REQUIRED;
		break;
	case COMMIT_REQUIRED:
	case COMMIT_RUNNING_REQUIRED:
	case COMMIT_BROKEN:
		break;
	}
	spin_unlock(&c->cs_lock);
}

void ubifs_request_bg_commit(struct ubifs_info *c)
{
	spin_lock(&c->cs_lock);
	if (c->cmt_state == COMMIT_RESTING) {
		dbg_cmt("old: %s, new: %s", dbg_cstate(c->cmt_state),
			dbg_cstate(COMMIT_BACKGROUND));
		c->cmt_state = COMMIT_BACKGROUND;
		spin_unlock(&c->cs_lock);
		ubifs_wake_up_bgt(c);
	} else
		spin_unlock(&c->cs_lock);
}

static int wait_for_commit(struct ubifs_info *c)
{
	dbg_cmt("pid %d goes sleep", current->pid);

	wait_event(c->cmt_wq, c->cmt_state != COMMIT_RUNNING_BACKGROUND &&
			      c->cmt_state != COMMIT_RUNNING_REQUIRED);
	dbg_cmt("commit finished, pid %d woke up", current->pid);
	return 0;
}

int ubifs_run_commit(struct ubifs_info *c)
{
	int err = 0;

	spin_lock(&c->cs_lock);
	if (c->cmt_state == COMMIT_BROKEN) {
		err = -EROFS;
		goto out;
	}

	if (c->cmt_state == COMMIT_RUNNING_BACKGROUND)
		c->cmt_state = COMMIT_RUNNING_REQUIRED;

	if (c->cmt_state == COMMIT_RUNNING_REQUIRED) {
		spin_unlock(&c->cs_lock);
		return wait_for_commit(c);
	}
	spin_unlock(&c->cs_lock);

	

	down_write(&c->commit_sem);
	spin_lock(&c->cs_lock);
	if (c->cmt_state == COMMIT_BROKEN) {
		err = -EROFS;
		goto out_cmt_unlock;
	}

	if (c->cmt_state == COMMIT_RUNNING_BACKGROUND)
		c->cmt_state = COMMIT_RUNNING_REQUIRED;

	if (c->cmt_state == COMMIT_RUNNING_REQUIRED) {
		up_write(&c->commit_sem);
		spin_unlock(&c->cs_lock);
		return wait_for_commit(c);
	}
	c->cmt_state = COMMIT_RUNNING_REQUIRED;
	spin_unlock(&c->cs_lock);

	err = do_commit(c);
	return err;

out_cmt_unlock:
	up_write(&c->commit_sem);
out:
	spin_unlock(&c->cs_lock);
	return err;
}

int ubifs_gc_should_commit(struct ubifs_info *c)
{
	int ret = 0;

	spin_lock(&c->cs_lock);
	if (c->cmt_state == COMMIT_BACKGROUND) {
		dbg_cmt("commit required now");
		c->cmt_state = COMMIT_REQUIRED;
	} else
		dbg_cmt("commit not requested");
	if (c->cmt_state == COMMIT_REQUIRED)
		ret = 1;
	spin_unlock(&c->cs_lock);
	return ret;
}

#ifdef CONFIG_UBIFS_FS_DEBUG

struct idx_node {
	struct list_head list;
	int iip;
	union ubifs_key upper_key;
	struct ubifs_idx_node idx __attribute__((aligned(8)));
};

int dbg_old_index_check_init(struct ubifs_info *c, struct ubifs_zbranch *zroot)
{
	struct ubifs_idx_node *idx;
	int lnum, offs, len, err = 0;
	struct ubifs_debug_info *d = c->dbg;

	d->old_zroot = *zroot;
	lnum = d->old_zroot.lnum;
	offs = d->old_zroot.offs;
	len = d->old_zroot.len;

	idx = kmalloc(c->max_idx_node_sz, GFP_NOFS);
	if (!idx)
		return -ENOMEM;

	err = ubifs_read_node(c, idx, UBIFS_IDX_NODE, len, lnum, offs);
	if (err)
		goto out;

	d->old_zroot_level = le16_to_cpu(idx->level);
	d->old_zroot_sqnum = le64_to_cpu(idx->ch.sqnum);
out:
	kfree(idx);
	return err;
}

int dbg_check_old_index(struct ubifs_info *c, struct ubifs_zbranch *zroot)
{
	int lnum, offs, len, err = 0, uninitialized_var(last_level), child_cnt;
	int first = 1, iip;
	struct ubifs_debug_info *d = c->dbg;
	union ubifs_key uninitialized_var(lower_key), upper_key, l_key, u_key;
	unsigned long long uninitialized_var(last_sqnum);
	struct ubifs_idx_node *idx;
	struct list_head list;
	struct idx_node *i;
	size_t sz;

	if (!dbg_is_chk_index(c))
		return 0;

	INIT_LIST_HEAD(&list);

	sz = sizeof(struct idx_node) + ubifs_idx_node_sz(c, c->fanout) -
	     UBIFS_IDX_NODE_SZ;

	
	lnum = d->old_zroot.lnum;
	offs = d->old_zroot.offs;
	len = d->old_zroot.len;
	iip = 0;

	while (1) {
		struct ubifs_branch *br;

		
		i = kmalloc(sz, GFP_NOFS);
		if (!i) {
			err = -ENOMEM;
			goto out_free;
		}
		i->iip = iip;
		
		list_add_tail(&i->list, &list);
		
		idx = &i->idx;
		err = ubifs_read_node(c, idx, UBIFS_IDX_NODE, len, lnum, offs);
		if (err)
			goto out_free;
		
		child_cnt = le16_to_cpu(idx->child_cnt);
		if (child_cnt < 1 || child_cnt > c->fanout) {
			err = 1;
			goto out_dump;
		}
		if (first) {
			first = 0;
			
			if (le16_to_cpu(idx->level) != d->old_zroot_level) {
				err = 2;
				goto out_dump;
			}
			if (le64_to_cpu(idx->ch.sqnum) != d->old_zroot_sqnum) {
				err = 3;
				goto out_dump;
			}
			
			last_level = le16_to_cpu(idx->level) + 1;
			last_sqnum = le64_to_cpu(idx->ch.sqnum) + 1;
			key_read(c, ubifs_idx_key(c, idx), &lower_key);
			highest_ino_key(c, &upper_key, INUM_WATERMARK);
		}
		key_copy(c, &upper_key, &i->upper_key);
		if (le16_to_cpu(idx->level) != last_level - 1) {
			err = 3;
			goto out_dump;
		}
		/*
		 * The index is always written bottom up hence a child's sqnum
		 * is always less than the parents.
		 */
		if (le64_to_cpu(idx->ch.sqnum) >= last_sqnum) {
			err = 4;
			goto out_dump;
		}
		
		key_read(c, ubifs_idx_key(c, idx), &l_key);
		br = ubifs_idx_branch(c, idx, child_cnt - 1);
		key_read(c, &br->key, &u_key);
		if (keys_cmp(c, &lower_key, &l_key) > 0) {
			err = 5;
			goto out_dump;
		}
		if (keys_cmp(c, &upper_key, &u_key) < 0) {
			err = 6;
			goto out_dump;
		}
		if (keys_cmp(c, &upper_key, &u_key) == 0)
			if (!is_hash_key(c, &u_key)) {
				err = 7;
				goto out_dump;
			}
		
		if (le16_to_cpu(idx->level) == 0) {
			
			while (1) {
				
				list_del(&i->list);
				kfree(i);
				
				if (list_empty(&list))
					goto out;
				
				i = list_entry(list.prev, struct idx_node,
					       list);
				idx = &i->idx;
				
				if (iip + 1 < le16_to_cpu(idx->child_cnt)) {
					iip = iip + 1;
					break;
				} else
					
					iip = i->iip;
			}
		} else
			
			iip = 0;
		last_level = le16_to_cpu(idx->level);
		last_sqnum = le64_to_cpu(idx->ch.sqnum);
		br = ubifs_idx_branch(c, idx, iip);
		lnum = le32_to_cpu(br->lnum);
		offs = le32_to_cpu(br->offs);
		len = le32_to_cpu(br->len);
		key_read(c, &br->key, &lower_key);
		if (iip + 1 < le16_to_cpu(idx->child_cnt)) {
			br = ubifs_idx_branch(c, idx, iip + 1);
			key_read(c, &br->key, &upper_key);
		} else
			key_copy(c, &i->upper_key, &upper_key);
	}
out:
	err = dbg_old_index_check_init(c, zroot);
	if (err)
		goto out_free;

	return 0;

out_dump:
	dbg_err("dumping index node (iip=%d)", i->iip);
	dbg_dump_node(c, idx);
	list_del(&i->list);
	kfree(i);
	if (!list_empty(&list)) {
		i = list_entry(list.prev, struct idx_node, list);
		dbg_err("dumping parent index node");
		dbg_dump_node(c, &i->idx);
	}
out_free:
	while (!list_empty(&list)) {
		i = list_entry(list.next, struct idx_node, list);
		list_del(&i->list);
		kfree(i);
	}
	ubifs_err("failed, error %d", err);
	if (err > 0)
		err = -EINVAL;
	return err;
}

#endif 
