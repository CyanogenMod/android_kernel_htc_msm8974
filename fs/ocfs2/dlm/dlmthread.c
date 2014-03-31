/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dlmthread.c
 *
 * standalone DLM module
 *
 * Copyright (C) 2004 Oracle.  All rights reserved.
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
 *
 */


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/random.h>
#include <linux/blkdev.h>
#include <linux/socket.h>
#include <linux/inet.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/delay.h>


#include "cluster/heartbeat.h"
#include "cluster/nodemanager.h"
#include "cluster/tcp.h"

#include "dlmapi.h"
#include "dlmcommon.h"
#include "dlmdomain.h"

#define MLOG_MASK_PREFIX (ML_DLM|ML_DLM_THREAD)
#include "cluster/masklog.h"

static int dlm_thread(void *data);
static void dlm_flush_asts(struct dlm_ctxt *dlm);

#define dlm_lock_is_remote(dlm, lock)     ((lock)->ml.node != (dlm)->node_num)

void __dlm_wait_on_lockres_flags(struct dlm_lock_resource *res, int flags)
{
	DECLARE_WAITQUEUE(wait, current);

	assert_spin_locked(&res->spinlock);

	add_wait_queue(&res->wq, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (res->state & flags) {
		spin_unlock(&res->spinlock);
		schedule();
		spin_lock(&res->spinlock);
		goto repeat;
	}
	remove_wait_queue(&res->wq, &wait);
	__set_current_state(TASK_RUNNING);
}

int __dlm_lockres_has_locks(struct dlm_lock_resource *res)
{
	if (list_empty(&res->granted) &&
	    list_empty(&res->converting) &&
	    list_empty(&res->blocked))
		return 0;
	return 1;
}

int __dlm_lockres_unused(struct dlm_lock_resource *res)
{
	int bit;

	assert_spin_locked(&res->spinlock);

	if (__dlm_lockres_has_locks(res))
		return 0;

	
	if (res->inflight_locks)
		return 0;

	if (!list_empty(&res->dirty) || res->state & DLM_LOCK_RES_DIRTY)
		return 0;

	if (res->state & DLM_LOCK_RES_RECOVERING)
		return 0;

	
	bit = find_next_bit(res->refmap, O2NM_MAX_NODES, 0);
	if (bit < O2NM_MAX_NODES)
		return 0;

	return 1;
}


void __dlm_lockres_calc_usage(struct dlm_ctxt *dlm,
			      struct dlm_lock_resource *res)
{
	assert_spin_locked(&dlm->spinlock);
	assert_spin_locked(&res->spinlock);

	if (__dlm_lockres_unused(res)){
		if (list_empty(&res->purge)) {
			mlog(0, "%s: Adding res %.*s to purge list\n",
			     dlm->name, res->lockname.len, res->lockname.name);

			res->last_used = jiffies;
			dlm_lockres_get(res);
			list_add_tail(&res->purge, &dlm->purge_list);
			dlm->purge_count++;
		}
	} else if (!list_empty(&res->purge)) {
		mlog(0, "%s: Removing res %.*s from purge list\n",
		     dlm->name, res->lockname.len, res->lockname.name);

		list_del_init(&res->purge);
		dlm_lockres_put(res);
		dlm->purge_count--;
	}
}

void dlm_lockres_calc_usage(struct dlm_ctxt *dlm,
			    struct dlm_lock_resource *res)
{
	spin_lock(&dlm->spinlock);
	spin_lock(&res->spinlock);

	__dlm_lockres_calc_usage(dlm, res);

	spin_unlock(&res->spinlock);
	spin_unlock(&dlm->spinlock);
}

static void dlm_purge_lockres(struct dlm_ctxt *dlm,
			     struct dlm_lock_resource *res)
{
	int master;
	int ret = 0;

	assert_spin_locked(&dlm->spinlock);
	assert_spin_locked(&res->spinlock);

	master = (res->owner == dlm->node_num);

	mlog(0, "%s: Purging res %.*s, master %d\n", dlm->name,
	     res->lockname.len, res->lockname.name, master);

	if (!master) {
		res->state |= DLM_LOCK_RES_DROPPING_REF;
		
		spin_unlock(&res->spinlock);
		spin_unlock(&dlm->spinlock);

		spin_lock(&res->spinlock);
		
		__dlm_wait_on_lockres_flags(res, DLM_LOCK_RES_SETREF_INPROG);
		spin_unlock(&res->spinlock);

		
		ret = dlm_drop_lockres_ref(dlm, res);
		if (ret < 0) {
			if (!dlm_is_host_down(ret))
				BUG();
		}
		spin_lock(&dlm->spinlock);
		spin_lock(&res->spinlock);
	}

	if (!list_empty(&res->purge)) {
		mlog(0, "%s: Removing res %.*s from purgelist, master %d\n",
		     dlm->name, res->lockname.len, res->lockname.name, master);
		list_del_init(&res->purge);
		dlm_lockres_put(res);
		dlm->purge_count--;
	}

	if (!__dlm_lockres_unused(res)) {
		mlog(ML_ERROR, "%s: res %.*s in use after deref\n",
		     dlm->name, res->lockname.len, res->lockname.name);
		__dlm_print_one_lock_resource(res);
		BUG();
	}

	__dlm_unhash_lockres(dlm, res);

	if (!master) {
		res->state &= ~DLM_LOCK_RES_DROPPING_REF;
		spin_unlock(&res->spinlock);
		wake_up(&res->wq);
	} else
		spin_unlock(&res->spinlock);
}

static void dlm_run_purge_list(struct dlm_ctxt *dlm,
			       int purge_now)
{
	unsigned int run_max, unused;
	unsigned long purge_jiffies;
	struct dlm_lock_resource *lockres;

	spin_lock(&dlm->spinlock);
	run_max = dlm->purge_count;

	while(run_max && !list_empty(&dlm->purge_list)) {
		run_max--;

		lockres = list_entry(dlm->purge_list.next,
				     struct dlm_lock_resource, purge);

		spin_lock(&lockres->spinlock);

		purge_jiffies = lockres->last_used +
			msecs_to_jiffies(DLM_PURGE_INTERVAL_MS);

		if (!purge_now && time_after(purge_jiffies, jiffies)) {
			spin_unlock(&lockres->spinlock);
			break;
		}

		unused = __dlm_lockres_unused(lockres);
		if (!unused ||
		    (lockres->state & DLM_LOCK_RES_MIGRATING)) {
			mlog(0, "%s: res %.*s is in use or being remastered, "
			     "used %d, state %d\n", dlm->name,
			     lockres->lockname.len, lockres->lockname.name,
			     !unused, lockres->state);
			list_move_tail(&dlm->purge_list, &lockres->purge);
			spin_unlock(&lockres->spinlock);
			continue;
		}

		dlm_lockres_get(lockres);

		dlm_purge_lockres(dlm, lockres);

		dlm_lockres_put(lockres);

		
		cond_resched_lock(&dlm->spinlock);
	}

	spin_unlock(&dlm->spinlock);
}

static void dlm_shuffle_lists(struct dlm_ctxt *dlm,
			      struct dlm_lock_resource *res)
{
	struct dlm_lock *lock, *target;
	struct list_head *iter;
	struct list_head *head;
	int can_grant = 1;

	assert_spin_locked(&dlm->ast_lock);
	assert_spin_locked(&res->spinlock);
	BUG_ON((res->state & (DLM_LOCK_RES_MIGRATING|
			      DLM_LOCK_RES_RECOVERING|
			      DLM_LOCK_RES_IN_PROGRESS)));

converting:
	if (list_empty(&res->converting))
		goto blocked;
	mlog(0, "%s: res %.*s has locks on the convert queue\n", dlm->name,
	     res->lockname.len, res->lockname.name);

	target = list_entry(res->converting.next, struct dlm_lock, list);
	if (target->ml.convert_type == LKM_IVMODE) {
		mlog(ML_ERROR, "%s: res %.*s converting lock to invalid mode\n",
		     dlm->name, res->lockname.len, res->lockname.name);
		BUG();
	}
	head = &res->granted;
	list_for_each(iter, head) {
		lock = list_entry(iter, struct dlm_lock, list);
		if (lock==target)
			continue;
		if (!dlm_lock_compatible(lock->ml.type,
					 target->ml.convert_type)) {
			can_grant = 0;
			
			if (lock->ml.highest_blocked == LKM_IVMODE) {
				__dlm_lockres_reserve_ast(res);
				__dlm_queue_bast(dlm, lock);
			}
			
			if (lock->ml.highest_blocked < target->ml.convert_type)
				lock->ml.highest_blocked =
					target->ml.convert_type;
		}
	}
	head = &res->converting;
	list_for_each(iter, head) {
		lock = list_entry(iter, struct dlm_lock, list);
		if (lock==target)
			continue;
		if (!dlm_lock_compatible(lock->ml.type,
					 target->ml.convert_type)) {
			can_grant = 0;
			if (lock->ml.highest_blocked == LKM_IVMODE) {
				__dlm_lockres_reserve_ast(res);
				__dlm_queue_bast(dlm, lock);
			}
			if (lock->ml.highest_blocked < target->ml.convert_type)
				lock->ml.highest_blocked =
					target->ml.convert_type;
		}
	}

	
	if (can_grant) {
		spin_lock(&target->spinlock);
		BUG_ON(target->ml.highest_blocked != LKM_IVMODE);

		mlog(0, "%s: res %.*s, AST for Converting lock %u:%llu, type "
		     "%d => %d, node %u\n", dlm->name, res->lockname.len,
		     res->lockname.name,
		     dlm_get_lock_cookie_node(be64_to_cpu(target->ml.cookie)),
		     dlm_get_lock_cookie_seq(be64_to_cpu(target->ml.cookie)),
		     target->ml.type,
		     target->ml.convert_type, target->ml.node);

		target->ml.type = target->ml.convert_type;
		target->ml.convert_type = LKM_IVMODE;
		list_move_tail(&target->list, &res->granted);

		BUG_ON(!target->lksb);
		target->lksb->status = DLM_NORMAL;

		spin_unlock(&target->spinlock);

		__dlm_lockres_reserve_ast(res);
		__dlm_queue_ast(dlm, target);
		
		goto converting;
	}

blocked:
	if (list_empty(&res->blocked))
		goto leave;
	target = list_entry(res->blocked.next, struct dlm_lock, list);

	head = &res->granted;
	list_for_each(iter, head) {
		lock = list_entry(iter, struct dlm_lock, list);
		if (lock==target)
			continue;
		if (!dlm_lock_compatible(lock->ml.type, target->ml.type)) {
			can_grant = 0;
			if (lock->ml.highest_blocked == LKM_IVMODE) {
				__dlm_lockres_reserve_ast(res);
				__dlm_queue_bast(dlm, lock);
			}
			if (lock->ml.highest_blocked < target->ml.type)
				lock->ml.highest_blocked = target->ml.type;
		}
	}

	head = &res->converting;
	list_for_each(iter, head) {
		lock = list_entry(iter, struct dlm_lock, list);
		if (lock==target)
			continue;
		if (!dlm_lock_compatible(lock->ml.type, target->ml.type)) {
			can_grant = 0;
			if (lock->ml.highest_blocked == LKM_IVMODE) {
				__dlm_lockres_reserve_ast(res);
				__dlm_queue_bast(dlm, lock);
			}
			if (lock->ml.highest_blocked < target->ml.type)
				lock->ml.highest_blocked = target->ml.type;
		}
	}

	if (can_grant) {
		spin_lock(&target->spinlock);
		BUG_ON(target->ml.highest_blocked != LKM_IVMODE);

		mlog(0, "%s: res %.*s, AST for Blocked lock %u:%llu, type %d, "
		     "node %u\n", dlm->name, res->lockname.len,
		     res->lockname.name,
		     dlm_get_lock_cookie_node(be64_to_cpu(target->ml.cookie)),
		     dlm_get_lock_cookie_seq(be64_to_cpu(target->ml.cookie)),
		     target->ml.type, target->ml.node);

		
		list_move_tail(&target->list, &res->granted);

		BUG_ON(!target->lksb);
		target->lksb->status = DLM_NORMAL;

		spin_unlock(&target->spinlock);

		__dlm_lockres_reserve_ast(res);
		__dlm_queue_ast(dlm, target);
		
		goto converting;
	}

leave:
	return;
}

void dlm_kick_thread(struct dlm_ctxt *dlm, struct dlm_lock_resource *res)
{
	if (res) {
		spin_lock(&dlm->spinlock);
		spin_lock(&res->spinlock);
		__dlm_dirty_lockres(dlm, res);
		spin_unlock(&res->spinlock);
		spin_unlock(&dlm->spinlock);
	}
	wake_up(&dlm->dlm_thread_wq);
}

void __dlm_dirty_lockres(struct dlm_ctxt *dlm, struct dlm_lock_resource *res)
{
	assert_spin_locked(&dlm->spinlock);
	assert_spin_locked(&res->spinlock);

	
	if ((res->owner == dlm->node_num)) {
		if (res->state & (DLM_LOCK_RES_MIGRATING |
				  DLM_LOCK_RES_BLOCK_DIRTY))
		    return;

		if (list_empty(&res->dirty)) {
			
			dlm_lockres_get(res);
			list_add_tail(&res->dirty, &dlm->dirty_list);
			res->state |= DLM_LOCK_RES_DIRTY;
		}
	}

	mlog(0, "%s: res %.*s\n", dlm->name, res->lockname.len,
	     res->lockname.name);
}


int dlm_launch_thread(struct dlm_ctxt *dlm)
{
	mlog(0, "Starting dlm_thread...\n");

	dlm->dlm_thread_task = kthread_run(dlm_thread, dlm, "dlm_thread");
	if (IS_ERR(dlm->dlm_thread_task)) {
		mlog_errno(PTR_ERR(dlm->dlm_thread_task));
		dlm->dlm_thread_task = NULL;
		return -EINVAL;
	}

	return 0;
}

void dlm_complete_thread(struct dlm_ctxt *dlm)
{
	if (dlm->dlm_thread_task) {
		mlog(ML_KTHREAD, "Waiting for dlm thread to exit\n");
		kthread_stop(dlm->dlm_thread_task);
		dlm->dlm_thread_task = NULL;
	}
}

static int dlm_dirty_list_empty(struct dlm_ctxt *dlm)
{
	int empty;

	spin_lock(&dlm->spinlock);
	empty = list_empty(&dlm->dirty_list);
	spin_unlock(&dlm->spinlock);

	return empty;
}

static void dlm_flush_asts(struct dlm_ctxt *dlm)
{
	int ret;
	struct dlm_lock *lock;
	struct dlm_lock_resource *res;
	u8 hi;

	spin_lock(&dlm->ast_lock);
	while (!list_empty(&dlm->pending_asts)) {
		lock = list_entry(dlm->pending_asts.next,
				  struct dlm_lock, ast_list);
		
		dlm_lock_get(lock);
		res = lock->lockres;
		mlog(0, "%s: res %.*s, Flush AST for lock %u:%llu, type %d, "
		     "node %u\n", dlm->name, res->lockname.len,
		     res->lockname.name,
		     dlm_get_lock_cookie_node(be64_to_cpu(lock->ml.cookie)),
		     dlm_get_lock_cookie_seq(be64_to_cpu(lock->ml.cookie)),
		     lock->ml.type, lock->ml.node);

		BUG_ON(!lock->ast_pending);

		
		list_del_init(&lock->ast_list);
		dlm_lock_put(lock);
		spin_unlock(&dlm->ast_lock);

		if (lock->ml.node != dlm->node_num) {
			ret = dlm_do_remote_ast(dlm, res, lock);
			if (ret < 0)
				mlog_errno(ret);
		} else
			dlm_do_local_ast(dlm, res, lock);

		spin_lock(&dlm->ast_lock);

		if (!list_empty(&lock->ast_list)) {
			mlog(0, "%s: res %.*s, AST queued while flushing last "
			     "one\n", dlm->name, res->lockname.len,
			     res->lockname.name);
		} else
			lock->ast_pending = 0;

		dlm_lock_put(lock);
		dlm_lockres_release_ast(dlm, res);
	}

	while (!list_empty(&dlm->pending_basts)) {
		lock = list_entry(dlm->pending_basts.next,
				  struct dlm_lock, bast_list);
		
		dlm_lock_get(lock);
		res = lock->lockres;

		BUG_ON(!lock->bast_pending);

		
		spin_lock(&lock->spinlock);
		BUG_ON(lock->ml.highest_blocked <= LKM_IVMODE);
		hi = lock->ml.highest_blocked;
		lock->ml.highest_blocked = LKM_IVMODE;
		spin_unlock(&lock->spinlock);

		
		list_del_init(&lock->bast_list);
		dlm_lock_put(lock);
		spin_unlock(&dlm->ast_lock);

		mlog(0, "%s: res %.*s, Flush BAST for lock %u:%llu, "
		     "blocked %d, node %u\n",
		     dlm->name, res->lockname.len, res->lockname.name,
		     dlm_get_lock_cookie_node(be64_to_cpu(lock->ml.cookie)),
		     dlm_get_lock_cookie_seq(be64_to_cpu(lock->ml.cookie)),
		     hi, lock->ml.node);

		if (lock->ml.node != dlm->node_num) {
			ret = dlm_send_proxy_bast(dlm, res, lock, hi);
			if (ret < 0)
				mlog_errno(ret);
		} else
			dlm_do_local_bast(dlm, res, lock, hi);

		spin_lock(&dlm->ast_lock);

		if (!list_empty(&lock->bast_list)) {
			mlog(0, "%s: res %.*s, BAST queued while flushing last "
			     "one\n", dlm->name, res->lockname.len,
			     res->lockname.name);
		} else
			lock->bast_pending = 0;

		dlm_lock_put(lock);
		dlm_lockres_release_ast(dlm, res);
	}
	wake_up(&dlm->ast_wq);
	spin_unlock(&dlm->ast_lock);
}


#define DLM_THREAD_TIMEOUT_MS (4 * 1000)
#define DLM_THREAD_MAX_DIRTY  100
#define DLM_THREAD_MAX_ASTS   10

static int dlm_thread(void *data)
{
	struct dlm_lock_resource *res;
	struct dlm_ctxt *dlm = data;
	unsigned long timeout = msecs_to_jiffies(DLM_THREAD_TIMEOUT_MS);

	mlog(0, "dlm thread running for %s...\n", dlm->name);

	while (!kthread_should_stop()) {
		int n = DLM_THREAD_MAX_DIRTY;

		dlm_run_purge_list(dlm, dlm_shutting_down(dlm));

		spin_lock(&dlm->spinlock);
		while (!list_empty(&dlm->dirty_list)) {
			int delay = 0;
			res = list_entry(dlm->dirty_list.next,
					 struct dlm_lock_resource, dirty);

			BUG_ON(!res);
			dlm_lockres_get(res);

			spin_lock(&res->spinlock);
			
			list_del_init(&res->dirty);
			spin_unlock(&res->spinlock);
			spin_unlock(&dlm->spinlock);
			
			dlm_lockres_put(res);


			spin_lock(&dlm->ast_lock);
			spin_lock(&res->spinlock);
			if (res->owner != dlm->node_num) {
				__dlm_print_one_lock_resource(res);
				mlog(ML_ERROR, "%s: inprog %d, mig %d, reco %d,"
				     " dirty %d\n", dlm->name,
				     !!(res->state & DLM_LOCK_RES_IN_PROGRESS),
				     !!(res->state & DLM_LOCK_RES_MIGRATING),
				     !!(res->state & DLM_LOCK_RES_RECOVERING),
				     !!(res->state & DLM_LOCK_RES_DIRTY));
			}
			BUG_ON(res->owner != dlm->node_num);

			BUG_ON(res->state & DLM_LOCK_RES_MIGRATING);
			if (res->state & (DLM_LOCK_RES_IN_PROGRESS |
					  DLM_LOCK_RES_RECOVERING)) {
				
				res->state &= ~DLM_LOCK_RES_DIRTY;
				spin_unlock(&res->spinlock);
				spin_unlock(&dlm->ast_lock);
				mlog(0, "%s: res %.*s, inprogress, delay list "
				     "shuffle, state %d\n", dlm->name,
				     res->lockname.len, res->lockname.name,
				     res->state);
				delay = 1;
				goto in_progress;
			}


			
			dlm_shuffle_lists(dlm, res);
			res->state &= ~DLM_LOCK_RES_DIRTY;
			spin_unlock(&res->spinlock);
			spin_unlock(&dlm->ast_lock);

			dlm_lockres_calc_usage(dlm, res);

in_progress:

			spin_lock(&dlm->spinlock);
			if (delay) {
				spin_lock(&res->spinlock);
				__dlm_dirty_lockres(dlm, res);
				spin_unlock(&res->spinlock);
			}
			dlm_lockres_put(res);

			if (!--n) {
				mlog(0, "%s: Throttling dlm thread\n",
				     dlm->name);
				break;
			}
		}

		spin_unlock(&dlm->spinlock);
		dlm_flush_asts(dlm);

		
		if (!n) {
			cond_resched();
			continue;
		}

		wait_event_interruptible_timeout(dlm->dlm_thread_wq,
						 !dlm_dirty_list_empty(dlm) ||
						 kthread_should_stop(),
						 timeout);
	}

	mlog(0, "quitting DLM thread\n");
	return 0;
}
