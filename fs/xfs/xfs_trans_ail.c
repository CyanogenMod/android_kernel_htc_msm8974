/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
 * Copyright (c) 2008 Dave Chinner
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_trans_priv.h"
#include "xfs_trace.h"
#include "xfs_error.h"

#ifdef DEBUG
STATIC void
xfs_ail_check(
	struct xfs_ail	*ailp,
	xfs_log_item_t	*lip)
{
	xfs_log_item_t	*prev_lip;

	if (list_empty(&ailp->xa_ail))
		return;

	ASSERT((lip->li_flags & XFS_LI_IN_AIL) != 0);
	prev_lip = list_entry(lip->li_ail.prev, xfs_log_item_t, li_ail);
	if (&prev_lip->li_ail != &ailp->xa_ail)
		ASSERT(XFS_LSN_CMP(prev_lip->li_lsn, lip->li_lsn) <= 0);

	prev_lip = list_entry(lip->li_ail.next, xfs_log_item_t, li_ail);
	if (&prev_lip->li_ail != &ailp->xa_ail)
		ASSERT(XFS_LSN_CMP(prev_lip->li_lsn, lip->li_lsn) >= 0);


#ifdef XFS_TRANS_DEBUG
	prev_lip = list_entry(&ailp->xa_ail, xfs_log_item_t, li_ail);
	list_for_each_entry(lip, &ailp->xa_ail, li_ail) {
		if (&prev_lip->li_ail != &ailp->xa_ail)
			ASSERT(XFS_LSN_CMP(prev_lip->li_lsn, lip->li_lsn) <= 0);
		ASSERT((lip->li_flags & XFS_LI_IN_AIL) != 0);
		prev_lip = lip;
	}
#endif 
}
#else 
#define	xfs_ail_check(a,l)
#endif 

static xfs_log_item_t *
xfs_ail_min(
	struct xfs_ail  *ailp)
{
	if (list_empty(&ailp->xa_ail))
		return NULL;

	return list_first_entry(&ailp->xa_ail, xfs_log_item_t, li_ail);
}

static xfs_log_item_t *
xfs_ail_max(
	struct xfs_ail  *ailp)
{
	if (list_empty(&ailp->xa_ail))
		return NULL;

	return list_entry(ailp->xa_ail.prev, xfs_log_item_t, li_ail);
}

static xfs_log_item_t *
xfs_ail_next(
	struct xfs_ail  *ailp,
	xfs_log_item_t  *lip)
{
	if (lip->li_ail.next == &ailp->xa_ail)
		return NULL;

	return list_first_entry(&lip->li_ail, xfs_log_item_t, li_ail);
}

xfs_lsn_t
xfs_ail_min_lsn(
	struct xfs_ail	*ailp)
{
	xfs_lsn_t	lsn = 0;
	xfs_log_item_t	*lip;

	spin_lock(&ailp->xa_lock);
	lip = xfs_ail_min(ailp);
	if (lip)
		lsn = lip->li_lsn;
	spin_unlock(&ailp->xa_lock);

	return lsn;
}

static xfs_lsn_t
xfs_ail_max_lsn(
	struct xfs_ail  *ailp)
{
	xfs_lsn_t       lsn = 0;
	xfs_log_item_t  *lip;

	spin_lock(&ailp->xa_lock);
	lip = xfs_ail_max(ailp);
	if (lip)
		lsn = lip->li_lsn;
	spin_unlock(&ailp->xa_lock);

	return lsn;
}

STATIC void
xfs_trans_ail_cursor_init(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur)
{
	cur->item = NULL;
	list_add_tail(&cur->list, &ailp->xa_cursors);
}

struct xfs_log_item *
xfs_trans_ail_cursor_next(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur)
{
	struct xfs_log_item	*lip = cur->item;

	if ((__psint_t)lip & 1)
		lip = xfs_ail_min(ailp);
	if (lip)
		cur->item = xfs_ail_next(ailp, lip);
	return lip;
}

void
xfs_trans_ail_cursor_done(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur)
{
	cur->item = NULL;
	list_del_init(&cur->list);
}

STATIC void
xfs_trans_ail_cursor_clear(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	struct xfs_ail_cursor	*cur;

	list_for_each_entry(cur, &ailp->xa_cursors, list) {
		if (cur->item == lip)
			cur->item = (struct xfs_log_item *)
					((__psint_t)cur->item | 1);
	}
}

xfs_log_item_t *
xfs_trans_ail_cursor_first(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	xfs_lsn_t		lsn)
{
	xfs_log_item_t		*lip;

	xfs_trans_ail_cursor_init(ailp, cur);

	if (lsn == 0) {
		lip = xfs_ail_min(ailp);
		goto out;
	}

	list_for_each_entry(lip, &ailp->xa_ail, li_ail) {
		if (XFS_LSN_CMP(lip->li_lsn, lsn) >= 0)
			goto out;
	}
	return NULL;

out:
	if (lip)
		cur->item = xfs_ail_next(ailp, lip);
	return lip;
}

static struct xfs_log_item *
__xfs_trans_ail_cursor_last(
	struct xfs_ail		*ailp,
	xfs_lsn_t		lsn)
{
	xfs_log_item_t		*lip;

	list_for_each_entry_reverse(lip, &ailp->xa_ail, li_ail) {
		if (XFS_LSN_CMP(lip->li_lsn, lsn) <= 0)
			return lip;
	}
	return NULL;
}

struct xfs_log_item *
xfs_trans_ail_cursor_last(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	xfs_lsn_t		lsn)
{
	xfs_trans_ail_cursor_init(ailp, cur);
	cur->item = __xfs_trans_ail_cursor_last(ailp, lsn);
	return cur->item;
}

static void
xfs_ail_splice(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct list_head	*list,
	xfs_lsn_t		lsn)
{
	struct xfs_log_item	*lip;

	ASSERT(!list_empty(list));

	lip = cur ? cur->item : NULL;
	if (!lip || (__psint_t) lip & 1)
		lip = __xfs_trans_ail_cursor_last(ailp, lsn);

	if (cur)
		cur->item = list_entry(list->prev, struct xfs_log_item, li_ail);

	if (lip)
		list_splice(list, &lip->li_ail);
	else
		list_splice(list, &ailp->xa_ail);
}

static void
xfs_ail_delete(
	struct xfs_ail  *ailp,
	xfs_log_item_t  *lip)
{
	xfs_ail_check(ailp, lip);
	list_del(&lip->li_ail);
	xfs_trans_ail_cursor_clear(ailp, lip);
}

static long
xfsaild_push(
	struct xfs_ail		*ailp)
{
	xfs_mount_t		*mp = ailp->xa_mount;
	struct xfs_ail_cursor	cur;
	xfs_log_item_t		*lip;
	xfs_lsn_t		lsn;
	xfs_lsn_t		target;
	long			tout = 10;
	int			stuck = 0;
	int			count = 0;
	int			push_xfsbufd = 0;

	spin_lock(&ailp->xa_lock);
	if (ailp->xa_last_pushed_lsn == 0 && ailp->xa_log_flush &&
	    !list_empty(&ailp->xa_ail)) {
		ailp->xa_log_flush = 0;
		spin_unlock(&ailp->xa_lock);
		XFS_STATS_INC(xs_push_ail_flush);
		xfs_log_force(mp, XFS_LOG_SYNC);
		spin_lock(&ailp->xa_lock);
	}

	target = ailp->xa_target;
	lip = xfs_trans_ail_cursor_first(ailp, &cur, ailp->xa_last_pushed_lsn);
	if (!lip || XFS_FORCED_SHUTDOWN(mp)) {
		xfs_trans_ail_cursor_done(ailp, &cur);
		spin_unlock(&ailp->xa_lock);
		goto out_done;
	}

	XFS_STATS_INC(xs_push_ail);

	lsn = lip->li_lsn;
	while ((XFS_LSN_CMP(lip->li_lsn, target) <= 0)) {
		int	lock_result;
		lock_result = IOP_TRYLOCK(lip);
		spin_unlock(&ailp->xa_lock);
		switch (lock_result) {
		case XFS_ITEM_SUCCESS:
			XFS_STATS_INC(xs_push_ail_success);
			trace_xfs_ail_push(lip);

			IOP_PUSH(lip);
			ailp->xa_last_pushed_lsn = lsn;
			break;

		case XFS_ITEM_PUSHBUF:
			XFS_STATS_INC(xs_push_ail_pushbuf);
			trace_xfs_ail_pushbuf(lip);

			if (!IOP_PUSHBUF(lip)) {
				trace_xfs_ail_pushbuf_pinned(lip);
				stuck++;
				ailp->xa_log_flush++;
			} else {
				ailp->xa_last_pushed_lsn = lsn;
			}
			push_xfsbufd = 1;
			break;

		case XFS_ITEM_PINNED:
			XFS_STATS_INC(xs_push_ail_pinned);
			trace_xfs_ail_pinned(lip);

			stuck++;
			ailp->xa_log_flush++;
			break;

		case XFS_ITEM_LOCKED:
			XFS_STATS_INC(xs_push_ail_locked);
			trace_xfs_ail_locked(lip);
			stuck++;
			break;

		default:
			ASSERT(0);
			break;
		}

		spin_lock(&ailp->xa_lock);
		
		if (XFS_FORCED_SHUTDOWN(mp))
			break;
		ASSERT(mp->m_log);

		count++;

		if (stuck > 100)
			break;

		lip = xfs_trans_ail_cursor_next(ailp, &cur);
		if (lip == NULL)
			break;
		lsn = lip->li_lsn;
	}
	xfs_trans_ail_cursor_done(ailp, &cur);
	spin_unlock(&ailp->xa_lock);

	if (push_xfsbufd) {
		
		wake_up_process(mp->m_ddev_targp->bt_task);
	}

	
out_done:
	if (!count) {
		
		ailp->xa_last_pushed_lsn = 0;
		ailp->xa_log_flush = 0;

		tout = 50;
	} else if (XFS_LSN_CMP(lsn, target) >= 0) {
		tout = 50;
		ailp->xa_last_pushed_lsn = 0;
	} else if ((stuck * 100) / count > 90) {
		tout = 20;
		ailp->xa_last_pushed_lsn = 0;
	}

	return tout;
}

static int
xfsaild(
	void		*data)
{
	struct xfs_ail	*ailp = data;
	long		tout = 0;	

	while (!kthread_should_stop()) {
		if (tout && tout <= 20)
			__set_current_state(TASK_KILLABLE);
		else
			__set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(tout ?
				 msecs_to_jiffies(tout) : MAX_SCHEDULE_TIMEOUT);

		try_to_freeze();

		tout = xfsaild_push(ailp);
	}

	return 0;
}

void
xfs_ail_push(
	struct xfs_ail	*ailp,
	xfs_lsn_t	threshold_lsn)
{
	xfs_log_item_t	*lip;

	lip = xfs_ail_min(ailp);
	if (!lip || XFS_FORCED_SHUTDOWN(ailp->xa_mount) ||
	    XFS_LSN_CMP(threshold_lsn, ailp->xa_target) <= 0)
		return;

	smp_wmb();
	xfs_trans_ail_copy_lsn(ailp, &ailp->xa_target, &threshold_lsn);
	smp_wmb();

	wake_up_process(ailp->xa_task);
}

void
xfs_ail_push_all(
	struct xfs_ail  *ailp)
{
	xfs_lsn_t       threshold_lsn = xfs_ail_max_lsn(ailp);

	if (threshold_lsn)
		xfs_ail_push(ailp, threshold_lsn);
}

void
xfs_trans_ail_update_bulk(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct xfs_log_item	**log_items,
	int			nr_items,
	xfs_lsn_t		lsn) __releases(ailp->xa_lock)
{
	xfs_log_item_t		*mlip;
	int			mlip_changed = 0;
	int			i;
	LIST_HEAD(tmp);

	ASSERT(nr_items > 0);		
	mlip = xfs_ail_min(ailp);

	for (i = 0; i < nr_items; i++) {
		struct xfs_log_item *lip = log_items[i];
		if (lip->li_flags & XFS_LI_IN_AIL) {
			
			if (XFS_LSN_CMP(lsn, lip->li_lsn) <= 0)
				continue;

			xfs_ail_delete(ailp, lip);
			if (mlip == lip)
				mlip_changed = 1;
		} else {
			lip->li_flags |= XFS_LI_IN_AIL;
		}
		lip->li_lsn = lsn;
		list_add(&lip->li_ail, &tmp);
	}

	if (!list_empty(&tmp))
		xfs_ail_splice(ailp, cur, &tmp, lsn);
	spin_unlock(&ailp->xa_lock);

	if (mlip_changed && !XFS_FORCED_SHUTDOWN(ailp->xa_mount)) {
		xlog_assign_tail_lsn(ailp->xa_mount);
		xfs_log_space_wake(ailp->xa_mount);
	}
}

void
xfs_trans_ail_delete_bulk(
	struct xfs_ail		*ailp,
	struct xfs_log_item	**log_items,
	int			nr_items) __releases(ailp->xa_lock)
{
	xfs_log_item_t		*mlip;
	int			mlip_changed = 0;
	int			i;

	mlip = xfs_ail_min(ailp);

	for (i = 0; i < nr_items; i++) {
		struct xfs_log_item *lip = log_items[i];
		if (!(lip->li_flags & XFS_LI_IN_AIL)) {
			struct xfs_mount	*mp = ailp->xa_mount;

			spin_unlock(&ailp->xa_lock);
			if (!XFS_FORCED_SHUTDOWN(mp)) {
				xfs_alert_tag(mp, XFS_PTAG_AILDELETE,
		"%s: attempting to delete a log item that is not in the AIL",
						__func__);
				xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
			}
			return;
		}

		xfs_ail_delete(ailp, lip);
		lip->li_flags &= ~XFS_LI_IN_AIL;
		lip->li_lsn = 0;
		if (mlip == lip)
			mlip_changed = 1;
	}
	spin_unlock(&ailp->xa_lock);

	if (mlip_changed && !XFS_FORCED_SHUTDOWN(ailp->xa_mount)) {
		xlog_assign_tail_lsn(ailp->xa_mount);
		xfs_log_space_wake(ailp->xa_mount);
	}
}


int
xfs_trans_ail_init(
	xfs_mount_t	*mp)
{
	struct xfs_ail	*ailp;

	ailp = kmem_zalloc(sizeof(struct xfs_ail), KM_MAYFAIL);
	if (!ailp)
		return ENOMEM;

	ailp->xa_mount = mp;
	INIT_LIST_HEAD(&ailp->xa_ail);
	INIT_LIST_HEAD(&ailp->xa_cursors);
	spin_lock_init(&ailp->xa_lock);

	ailp->xa_task = kthread_run(xfsaild, ailp, "xfsaild/%s",
			ailp->xa_mount->m_fsname);
	if (IS_ERR(ailp->xa_task))
		goto out_free_ailp;

	mp->m_ail = ailp;
	return 0;

out_free_ailp:
	kmem_free(ailp);
	return ENOMEM;
}

void
xfs_trans_ail_destroy(
	xfs_mount_t	*mp)
{
	struct xfs_ail	*ailp = mp->m_ail;

	kthread_stop(ailp->xa_task);
	kmem_free(ailp);
}
