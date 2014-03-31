/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_inode.h"
#include "xfs_dinode.h"
#include "xfs_error.h"
#include "xfs_filestream.h"
#include "xfs_vnodeops.h"
#include "xfs_inode_item.h"
#include "xfs_quota.h"
#include "xfs_trace.h"
#include "xfs_fsops.h"

#include <linux/kthread.h>
#include <linux/freezer.h>

struct workqueue_struct	*xfs_syncd_wq;	

#define XFS_LOOKUP_BATCH	32

STATIC int
xfs_inode_ag_walk_grab(
	struct xfs_inode	*ip)
{
	struct inode		*inode = VFS_I(ip);

	ASSERT(rcu_read_lock_held());

	spin_lock(&ip->i_flags_lock);
	if (!ip->i_ino)
		goto out_unlock_noent;

	
	if (__xfs_iflags_test(ip, XFS_INEW | XFS_IRECLAIMABLE | XFS_IRECLAIM))
		goto out_unlock_noent;
	spin_unlock(&ip->i_flags_lock);

	
	if (XFS_FORCED_SHUTDOWN(ip->i_mount))
		return EFSCORRUPTED;

	
	if (!igrab(inode))
		return ENOENT;

	if (is_bad_inode(inode)) {
		IRELE(ip);
		return ENOENT;
	}

	
	return 0;

out_unlock_noent:
	spin_unlock(&ip->i_flags_lock);
	return ENOENT;
}

STATIC int
xfs_inode_ag_walk(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	int			(*execute)(struct xfs_inode *ip,
					   struct xfs_perag *pag, int flags),
	int			flags)
{
	uint32_t		first_index;
	int			last_error = 0;
	int			skipped;
	int			done;
	int			nr_found;

restart:
	done = 0;
	skipped = 0;
	first_index = 0;
	nr_found = 0;
	do {
		struct xfs_inode *batch[XFS_LOOKUP_BATCH];
		int		error = 0;
		int		i;

		rcu_read_lock();
		nr_found = radix_tree_gang_lookup(&pag->pag_ici_root,
					(void **)batch, first_index,
					XFS_LOOKUP_BATCH);
		if (!nr_found) {
			rcu_read_unlock();
			break;
		}

		for (i = 0; i < nr_found; i++) {
			struct xfs_inode *ip = batch[i];

			if (done || xfs_inode_ag_walk_grab(ip))
				batch[i] = NULL;

			if (XFS_INO_TO_AGNO(mp, ip->i_ino) != pag->pag_agno)
				continue;
			first_index = XFS_INO_TO_AGINO(mp, ip->i_ino + 1);
			if (first_index < XFS_INO_TO_AGINO(mp, ip->i_ino))
				done = 1;
		}

		
		rcu_read_unlock();

		for (i = 0; i < nr_found; i++) {
			if (!batch[i])
				continue;
			error = execute(batch[i], pag, flags);
			IRELE(batch[i]);
			if (error == EAGAIN) {
				skipped++;
				continue;
			}
			if (error && last_error != EFSCORRUPTED)
				last_error = error;
		}

		
		if (error == EFSCORRUPTED)
			break;

		cond_resched();

	} while (nr_found && !done);

	if (skipped) {
		delay(1);
		goto restart;
	}
	return last_error;
}

int
xfs_inode_ag_iterator(
	struct xfs_mount	*mp,
	int			(*execute)(struct xfs_inode *ip,
					   struct xfs_perag *pag, int flags),
	int			flags)
{
	struct xfs_perag	*pag;
	int			error = 0;
	int			last_error = 0;
	xfs_agnumber_t		ag;

	ag = 0;
	while ((pag = xfs_perag_get(mp, ag))) {
		ag = pag->pag_agno + 1;
		error = xfs_inode_ag_walk(mp, pag, execute, flags);
		xfs_perag_put(pag);
		if (error) {
			last_error = error;
			if (error == EFSCORRUPTED)
				break;
		}
	}
	return XFS_ERROR(last_error);
}

STATIC int
xfs_sync_inode_data(
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	int			flags)
{
	struct inode		*inode = VFS_I(ip);
	struct address_space *mapping = inode->i_mapping;
	int			error = 0;

	if (!mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		return 0;

	if (!xfs_ilock_nowait(ip, XFS_IOLOCK_SHARED)) {
		if (flags & SYNC_TRYLOCK)
			return 0;
		xfs_ilock(ip, XFS_IOLOCK_SHARED);
	}

	error = xfs_flush_pages(ip, 0, -1, (flags & SYNC_WAIT) ?
				0 : XBF_ASYNC, FI_NONE);
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);
	return error;
}

STATIC int
xfs_sync_inode_attr(
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	int			flags)
{
	int			error = 0;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	if (xfs_inode_clean(ip))
		goto out_unlock;
	if (!xfs_iflock_nowait(ip)) {
		if (!(flags & SYNC_WAIT))
			goto out_unlock;
		xfs_iflock(ip);
	}

	if (xfs_inode_clean(ip)) {
		xfs_ifunlock(ip);
		goto out_unlock;
	}

	error = xfs_iflush(ip, flags);

	/*
	 * We don't want to try again on non-blocking flushes that can't run
	 * again immediately. If an inode really must be written, then that's
	 * what the SYNC_WAIT flag is for.
	 */
	if (error == EAGAIN) {
		ASSERT(!(flags & SYNC_WAIT));
		error = 0;
	}

 out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return error;
}

STATIC int
xfs_sync_data(
	struct xfs_mount	*mp,
	int			flags)
{
	int			error;

	ASSERT((flags & ~(SYNC_TRYLOCK|SYNC_WAIT)) == 0);

	error = xfs_inode_ag_iterator(mp, xfs_sync_inode_data, flags);
	if (error)
		return XFS_ERROR(error);

	xfs_log_force(mp, (flags & SYNC_WAIT) ? XFS_LOG_SYNC : 0);
	return 0;
}

STATIC int
xfs_sync_attr(
	struct xfs_mount	*mp,
	int			flags)
{
	ASSERT((flags & ~SYNC_WAIT) == 0);

	return xfs_inode_ag_iterator(mp, xfs_sync_inode_attr, flags);
}

STATIC int
xfs_sync_fsdata(
	struct xfs_mount	*mp)
{
	struct xfs_buf		*bp;
	int			error;

	bp = xfs_getsb(mp, 0);
	if (xfs_buf_ispinned(bp))
		xfs_log_force(mp, 0);
	error = xfs_bwrite(bp);
	xfs_buf_relse(bp);
	return error;
}

/*
 * When remounting a filesystem read-only or freezing the filesystem, we have
 * two phases to execute. This first phase is syncing the data before we
 * quiesce the filesystem, and the second is flushing all the inodes out after
 * we've waited for all the transactions created by the first phase to
 * complete. The second phase ensures that the inodes are written to their
 * location on disk rather than just existing in transactions in the log. This
 * means after a quiesce there is no log replay required to write the inodes to
 * disk (this is the main difference between a sync and a quiesce).
 */
int
xfs_quiesce_data(
	struct xfs_mount	*mp)
{
	int			error, error2 = 0;

	
	xfs_log_force(mp, XFS_LOG_SYNC);

	
	error = xfs_sync_fsdata(mp);

	/* make sure all delwri buffers are written out */
	xfs_flush_buftarg(mp->m_ddev_targp, 1);

	
	if (xfs_log_need_covered(mp))
		error2 = xfs_fs_log_dummy(mp);

	
	if (mp->m_rtdev_targp)
		xfs_flush_buftarg(mp->m_rtdev_targp, 1);

	return error ? error : error2;
}

STATIC void
xfs_quiesce_fs(
	struct xfs_mount	*mp)
{
	int	count = 0, pincount;

	xfs_reclaim_inodes(mp, 0);
	xfs_flush_buftarg(mp->m_ddev_targp, 0);

	do {
		xfs_reclaim_inodes(mp, SYNC_WAIT);
		xfs_sync_attr(mp, SYNC_WAIT);
		pincount = xfs_flush_buftarg(mp->m_ddev_targp, 1);
		if (!pincount) {
			delay(50);
			count++;
		}
	} while (count < 2);
}

void
xfs_quiesce_attr(
	struct xfs_mount	*mp)
{
	int	error = 0;

	
	while (atomic_read(&mp->m_active_trans) > 0)
		delay(100);

	
	xfs_quiesce_fs(mp);

	WARN_ON(atomic_read(&mp->m_active_trans) != 0);

	
	error = xfs_log_sbcount(mp);
	if (error)
		xfs_warn(mp, "xfs_attr_quiesce: failed to log sb changes. "
				"Frozen image may not be consistent.");
	xfs_log_unmount_write(mp);
	xfs_unmountfs_writesb(mp);
}

static void
xfs_syncd_queue_sync(
	struct xfs_mount        *mp)
{
	queue_delayed_work(xfs_syncd_wq, &mp->m_sync_work,
				msecs_to_jiffies(xfs_syncd_centisecs * 10));
}

STATIC void
xfs_sync_worker(
	struct work_struct *work)
{
	struct xfs_mount *mp = container_of(to_delayed_work(work),
					struct xfs_mount, m_sync_work);
	int		error;

	if (!(mp->m_flags & XFS_MOUNT_RDONLY)) {
		
		if (mp->m_super->s_frozen == SB_UNFROZEN &&
		    xfs_log_need_covered(mp))
			error = xfs_fs_log_dummy(mp);
		else
			xfs_log_force(mp, 0);

		
		xfs_ail_push_all(mp->m_ail);
	}

	
	xfs_syncd_queue_sync(mp);
}

static void
xfs_syncd_queue_reclaim(
	struct xfs_mount        *mp)
{

	if (!(mp->m_super->s_flags & MS_ACTIVE))
		return;

	rcu_read_lock();
	if (radix_tree_tagged(&mp->m_perag_tree, XFS_ICI_RECLAIM_TAG)) {
		queue_delayed_work(xfs_syncd_wq, &mp->m_reclaim_work,
			msecs_to_jiffies(xfs_syncd_centisecs / 6 * 10));
	}
	rcu_read_unlock();
}

STATIC void
xfs_reclaim_worker(
	struct work_struct *work)
{
	struct xfs_mount *mp = container_of(to_delayed_work(work),
					struct xfs_mount, m_reclaim_work);

	xfs_reclaim_inodes(mp, SYNC_TRYLOCK);
	xfs_syncd_queue_reclaim(mp);
}

void
xfs_flush_inodes(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;

	queue_work(xfs_syncd_wq, &mp->m_flush_work);
	flush_work_sync(&mp->m_flush_work);
}

STATIC void
xfs_flush_worker(
	struct work_struct *work)
{
	struct xfs_mount *mp = container_of(work,
					struct xfs_mount, m_flush_work);

	xfs_sync_data(mp, SYNC_TRYLOCK);
	xfs_sync_data(mp, SYNC_TRYLOCK | SYNC_WAIT);
}

int
xfs_syncd_init(
	struct xfs_mount	*mp)
{
	INIT_WORK(&mp->m_flush_work, xfs_flush_worker);
	INIT_DELAYED_WORK(&mp->m_sync_work, xfs_sync_worker);
	INIT_DELAYED_WORK(&mp->m_reclaim_work, xfs_reclaim_worker);

	xfs_syncd_queue_sync(mp);
	xfs_syncd_queue_reclaim(mp);

	return 0;
}

void
xfs_syncd_stop(
	struct xfs_mount	*mp)
{
	cancel_delayed_work_sync(&mp->m_sync_work);
	cancel_delayed_work_sync(&mp->m_reclaim_work);
	cancel_work_sync(&mp->m_flush_work);
}

void
__xfs_inode_set_reclaim_tag(
	struct xfs_perag	*pag,
	struct xfs_inode	*ip)
{
	radix_tree_tag_set(&pag->pag_ici_root,
			   XFS_INO_TO_AGINO(ip->i_mount, ip->i_ino),
			   XFS_ICI_RECLAIM_TAG);

	if (!pag->pag_ici_reclaimable) {
		
		spin_lock(&ip->i_mount->m_perag_lock);
		radix_tree_tag_set(&ip->i_mount->m_perag_tree,
				XFS_INO_TO_AGNO(ip->i_mount, ip->i_ino),
				XFS_ICI_RECLAIM_TAG);
		spin_unlock(&ip->i_mount->m_perag_lock);

		
		xfs_syncd_queue_reclaim(ip->i_mount);

		trace_xfs_perag_set_reclaim(ip->i_mount, pag->pag_agno,
							-1, _RET_IP_);
	}
	pag->pag_ici_reclaimable++;
}

void
xfs_inode_set_reclaim_tag(
	xfs_inode_t	*ip)
{
	struct xfs_mount *mp = ip->i_mount;
	struct xfs_perag *pag;

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));
	spin_lock(&pag->pag_ici_lock);
	spin_lock(&ip->i_flags_lock);
	__xfs_inode_set_reclaim_tag(pag, ip);
	__xfs_iflags_set(ip, XFS_IRECLAIMABLE);
	spin_unlock(&ip->i_flags_lock);
	spin_unlock(&pag->pag_ici_lock);
	xfs_perag_put(pag);
}

STATIC void
__xfs_inode_clear_reclaim(
	xfs_perag_t	*pag,
	xfs_inode_t	*ip)
{
	pag->pag_ici_reclaimable--;
	if (!pag->pag_ici_reclaimable) {
		
		spin_lock(&ip->i_mount->m_perag_lock);
		radix_tree_tag_clear(&ip->i_mount->m_perag_tree,
				XFS_INO_TO_AGNO(ip->i_mount, ip->i_ino),
				XFS_ICI_RECLAIM_TAG);
		spin_unlock(&ip->i_mount->m_perag_lock);
		trace_xfs_perag_clear_reclaim(ip->i_mount, pag->pag_agno,
							-1, _RET_IP_);
	}
}

void
__xfs_inode_clear_reclaim_tag(
	xfs_mount_t	*mp,
	xfs_perag_t	*pag,
	xfs_inode_t	*ip)
{
	radix_tree_tag_clear(&pag->pag_ici_root,
			XFS_INO_TO_AGINO(mp, ip->i_ino), XFS_ICI_RECLAIM_TAG);
	__xfs_inode_clear_reclaim(pag, ip);
}

STATIC int
xfs_reclaim_inode_grab(
	struct xfs_inode	*ip,
	int			flags)
{
	ASSERT(rcu_read_lock_held());

	
	if (!ip->i_ino)
		return 1;

	if ((flags & SYNC_TRYLOCK) &&
	    __xfs_iflags_test(ip, XFS_IFLOCK | XFS_IRECLAIM))
		return 1;

	spin_lock(&ip->i_flags_lock);
	if (!__xfs_iflags_test(ip, XFS_IRECLAIMABLE) ||
	    __xfs_iflags_test(ip, XFS_IRECLAIM)) {
		
		spin_unlock(&ip->i_flags_lock);
		return 1;
	}
	__xfs_iflags_set(ip, XFS_IRECLAIM);
	spin_unlock(&ip->i_flags_lock);
	return 0;
}

STATIC int
xfs_reclaim_inode(
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	int			sync_mode)
{
	int	error;

restart:
	error = 0;
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if (!xfs_iflock_nowait(ip)) {
		if (!(sync_mode & SYNC_WAIT))
			goto out;

		xfs_promote_inode(ip);
		xfs_iflock(ip);
	}

	if (is_bad_inode(VFS_I(ip)))
		goto reclaim;
	if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		xfs_iunpin_wait(ip);
		goto reclaim;
	}
	if (xfs_ipincount(ip)) {
		if (!(sync_mode & SYNC_WAIT)) {
			xfs_ifunlock(ip);
			goto out;
		}
		xfs_iunpin_wait(ip);
	}
	if (xfs_iflags_test(ip, XFS_ISTALE))
		goto reclaim;
	if (xfs_inode_clean(ip))
		goto reclaim;

	error = xfs_iflush(ip, SYNC_TRYLOCK | sync_mode);
	if (sync_mode & SYNC_WAIT) {
		if (error == EAGAIN) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			
			delay(2);
			goto restart;
		}
		xfs_iflock(ip);
		goto reclaim;
	}

	if (error && error != EAGAIN && !XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		xfs_warn(ip->i_mount,
			"inode 0x%llx background reclaim flush failed with %d",
			(long long)ip->i_ino, error);
	}
out:
	xfs_iflags_clear(ip, XFS_IRECLAIM);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;

reclaim:
	xfs_ifunlock(ip);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	XFS_STATS_INC(xs_ig_reclaims);
	spin_lock(&pag->pag_ici_lock);
	if (!radix_tree_delete(&pag->pag_ici_root,
				XFS_INO_TO_AGINO(ip->i_mount, ip->i_ino)))
		ASSERT(0);
	__xfs_inode_clear_reclaim(pag, ip);
	spin_unlock(&pag->pag_ici_lock);

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_qm_dqdetach(ip);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	xfs_inode_free(ip);

	return error;
}

int
xfs_reclaim_inodes_ag(
	struct xfs_mount	*mp,
	int			flags,
	int			*nr_to_scan)
{
	struct xfs_perag	*pag;
	int			error = 0;
	int			last_error = 0;
	xfs_agnumber_t		ag;
	int			trylock = flags & SYNC_TRYLOCK;
	int			skipped;

restart:
	ag = 0;
	skipped = 0;
	while ((pag = xfs_perag_get_tag(mp, ag, XFS_ICI_RECLAIM_TAG))) {
		unsigned long	first_index = 0;
		int		done = 0;
		int		nr_found = 0;

		ag = pag->pag_agno + 1;

		if (trylock) {
			if (!mutex_trylock(&pag->pag_ici_reclaim_lock)) {
				skipped++;
				xfs_perag_put(pag);
				continue;
			}
			first_index = pag->pag_ici_reclaim_cursor;
		} else
			mutex_lock(&pag->pag_ici_reclaim_lock);

		do {
			struct xfs_inode *batch[XFS_LOOKUP_BATCH];
			int	i;

			rcu_read_lock();
			nr_found = radix_tree_gang_lookup_tag(
					&pag->pag_ici_root,
					(void **)batch, first_index,
					XFS_LOOKUP_BATCH,
					XFS_ICI_RECLAIM_TAG);
			if (!nr_found) {
				done = 1;
				rcu_read_unlock();
				break;
			}

			for (i = 0; i < nr_found; i++) {
				struct xfs_inode *ip = batch[i];

				if (done || xfs_reclaim_inode_grab(ip, flags))
					batch[i] = NULL;

				if (XFS_INO_TO_AGNO(mp, ip->i_ino) !=
								pag->pag_agno)
					continue;
				first_index = XFS_INO_TO_AGINO(mp, ip->i_ino + 1);
				if (first_index < XFS_INO_TO_AGINO(mp, ip->i_ino))
					done = 1;
			}

			
			rcu_read_unlock();

			for (i = 0; i < nr_found; i++) {
				if (!batch[i])
					continue;
				error = xfs_reclaim_inode(batch[i], pag, flags);
				if (error && last_error != EFSCORRUPTED)
					last_error = error;
			}

			*nr_to_scan -= XFS_LOOKUP_BATCH;

			cond_resched();

		} while (nr_found && !done && *nr_to_scan > 0);

		if (trylock && !done)
			pag->pag_ici_reclaim_cursor = first_index;
		else
			pag->pag_ici_reclaim_cursor = 0;
		mutex_unlock(&pag->pag_ici_reclaim_lock);
		xfs_perag_put(pag);
	}

	if (skipped && (flags & SYNC_WAIT) && *nr_to_scan > 0) {
		trylock = 0;
		goto restart;
	}
	return XFS_ERROR(last_error);
}

int
xfs_reclaim_inodes(
	xfs_mount_t	*mp,
	int		mode)
{
	int		nr_to_scan = INT_MAX;

	return xfs_reclaim_inodes_ag(mp, mode, &nr_to_scan);
}

void
xfs_reclaim_inodes_nr(
	struct xfs_mount	*mp,
	int			nr_to_scan)
{
	
	xfs_syncd_queue_reclaim(mp);
	xfs_ail_push_all(mp->m_ail);

	xfs_reclaim_inodes_ag(mp, SYNC_TRYLOCK | SYNC_WAIT, &nr_to_scan);
}

int
xfs_reclaim_inodes_count(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		ag = 0;
	int			reclaimable = 0;

	while ((pag = xfs_perag_get_tag(mp, ag, XFS_ICI_RECLAIM_TAG))) {
		ag = pag->pag_agno + 1;
		reclaimable += pag->pag_ici_reclaimable;
		xfs_perag_put(pag);
	}
	return reclaimable;
}

