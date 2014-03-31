/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
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
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_trans_priv.h"
#include "xfs_bmap_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_error.h"
#include "xfs_trace.h"


kmem_zone_t	*xfs_ili_zone;		

static inline struct xfs_inode_log_item *INODE_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_inode_log_item, ili_item);
}


STATIC uint
xfs_inode_item_size(
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;
	uint			nvecs = 2;

	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if ((iip->ili_fields & XFS_ILOG_DEXT) &&
		    ip->i_d.di_nextents > 0 &&
		    ip->i_df.if_bytes > 0)
			nvecs++;
		break;

	case XFS_DINODE_FMT_BTREE:
		if ((iip->ili_fields & XFS_ILOG_DBROOT) &&
		    ip->i_df.if_broot_bytes > 0)
			nvecs++;
		break;

	case XFS_DINODE_FMT_LOCAL:
		if ((iip->ili_fields & XFS_ILOG_DDATA) &&
		    ip->i_df.if_bytes > 0)
			nvecs++;
		break;

	case XFS_DINODE_FMT_DEV:
	case XFS_DINODE_FMT_UUID:
		break;

	default:
		ASSERT(0);
		break;
	}

	if (!XFS_IFORK_Q(ip))
		return nvecs;


	switch (ip->i_d.di_aformat) {
	case XFS_DINODE_FMT_EXTENTS:
		if ((iip->ili_fields & XFS_ILOG_AEXT) &&
		    ip->i_d.di_anextents > 0 &&
		    ip->i_afp->if_bytes > 0)
			nvecs++;
		break;

	case XFS_DINODE_FMT_BTREE:
		if ((iip->ili_fields & XFS_ILOG_ABROOT) &&
		    ip->i_afp->if_broot_bytes > 0)
			nvecs++;
		break;

	case XFS_DINODE_FMT_LOCAL:
		if ((iip->ili_fields & XFS_ILOG_ADATA) &&
		    ip->i_afp->if_bytes > 0)
			nvecs++;
		break;

	default:
		ASSERT(0);
		break;
	}

	return nvecs;
}

STATIC void
xfs_inode_item_format_extents(
	struct xfs_inode	*ip,
	struct xfs_log_iovec	*vecp,
	int			whichfork,
	int			type)
{
	xfs_bmbt_rec_t		*ext_buffer;

	ext_buffer = kmem_alloc(XFS_IFORK_SIZE(ip, whichfork), KM_SLEEP);
	if (whichfork == XFS_DATA_FORK)
		ip->i_itemp->ili_extents_buf = ext_buffer;
	else
		ip->i_itemp->ili_aextents_buf = ext_buffer;

	vecp->i_addr = ext_buffer;
	vecp->i_len = xfs_iextents_copy(ip, ext_buffer, whichfork);
	vecp->i_type = type;
}

STATIC void
xfs_inode_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_iovec	*vecp)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;
	uint			nvecs;
	size_t			data_bytes;
	xfs_mount_t		*mp;

	vecp->i_addr = &iip->ili_format;
	vecp->i_len  = sizeof(xfs_inode_log_format_t);
	vecp->i_type = XLOG_REG_TYPE_IFORMAT;
	vecp++;
	nvecs	     = 1;

	vecp->i_addr = &ip->i_d;
	vecp->i_len  = sizeof(struct xfs_icdinode);
	vecp->i_type = XLOG_REG_TYPE_ICORE;
	vecp++;
	nvecs++;

	mp = ip->i_mount;
	ASSERT(ip->i_d.di_version == 1 || xfs_sb_version_hasnlink(&mp->m_sb));
	if (ip->i_d.di_version == 1) {
		if (!xfs_sb_version_hasnlink(&mp->m_sb)) {
			ASSERT(ip->i_d.di_nlink <= XFS_MAXLINK_1);
			ip->i_d.di_onlink = ip->i_d.di_nlink;
		} else {
			ip->i_d.di_version = 2;
			ip->i_d.di_onlink = 0;
			memset(&(ip->i_d.di_pad[0]), 0, sizeof(ip->i_d.di_pad));
		}
	}

	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		iip->ili_fields &=
			~(XFS_ILOG_DDATA | XFS_ILOG_DBROOT |
			  XFS_ILOG_DEV | XFS_ILOG_UUID);

		if ((iip->ili_fields & XFS_ILOG_DEXT) &&
		    ip->i_d.di_nextents > 0 &&
		    ip->i_df.if_bytes > 0) {
			ASSERT(ip->i_df.if_u1.if_extents != NULL);
			ASSERT(ip->i_df.if_bytes / sizeof(xfs_bmbt_rec_t) > 0);
			ASSERT(iip->ili_extents_buf == NULL);

#ifdef XFS_NATIVE_HOST
                       if (ip->i_d.di_nextents == ip->i_df.if_bytes /
                                               (uint)sizeof(xfs_bmbt_rec_t)) {
				vecp->i_addr = ip->i_df.if_u1.if_extents;
				vecp->i_len = ip->i_df.if_bytes;
				vecp->i_type = XLOG_REG_TYPE_IEXT;
			} else
#endif
			{
				xfs_inode_item_format_extents(ip, vecp,
					XFS_DATA_FORK, XLOG_REG_TYPE_IEXT);
			}
			ASSERT(vecp->i_len <= ip->i_df.if_bytes);
			iip->ili_format.ilf_dsize = vecp->i_len;
			vecp++;
			nvecs++;
		} else {
			iip->ili_fields &= ~XFS_ILOG_DEXT;
		}
		break;

	case XFS_DINODE_FMT_BTREE:
		iip->ili_fields &=
			~(XFS_ILOG_DDATA | XFS_ILOG_DEXT |
			  XFS_ILOG_DEV | XFS_ILOG_UUID);

		if ((iip->ili_fields & XFS_ILOG_DBROOT) &&
		    ip->i_df.if_broot_bytes > 0) {
			ASSERT(ip->i_df.if_broot != NULL);
			vecp->i_addr = ip->i_df.if_broot;
			vecp->i_len = ip->i_df.if_broot_bytes;
			vecp->i_type = XLOG_REG_TYPE_IBROOT;
			vecp++;
			nvecs++;
			iip->ili_format.ilf_dsize = ip->i_df.if_broot_bytes;
		} else {
			ASSERT(!(iip->ili_fields &
				 XFS_ILOG_DBROOT));
#ifdef XFS_TRANS_DEBUG
			if (iip->ili_root_size > 0) {
				ASSERT(iip->ili_root_size ==
				       ip->i_df.if_broot_bytes);
				ASSERT(memcmp(iip->ili_orig_root,
					    ip->i_df.if_broot,
					    iip->ili_root_size) == 0);
			} else {
				ASSERT(ip->i_df.if_broot_bytes == 0);
			}
#endif
			iip->ili_fields &= ~XFS_ILOG_DBROOT;
		}
		break;

	case XFS_DINODE_FMT_LOCAL:
		iip->ili_fields &=
			~(XFS_ILOG_DEXT | XFS_ILOG_DBROOT |
			  XFS_ILOG_DEV | XFS_ILOG_UUID);
		if ((iip->ili_fields & XFS_ILOG_DDATA) &&
		    ip->i_df.if_bytes > 0) {
			ASSERT(ip->i_df.if_u1.if_data != NULL);
			ASSERT(ip->i_d.di_size > 0);

			vecp->i_addr = ip->i_df.if_u1.if_data;
			data_bytes = roundup(ip->i_df.if_bytes, 4);
			ASSERT((ip->i_df.if_real_bytes == 0) ||
			       (ip->i_df.if_real_bytes == data_bytes));
			vecp->i_len = (int)data_bytes;
			vecp->i_type = XLOG_REG_TYPE_ILOCAL;
			vecp++;
			nvecs++;
			iip->ili_format.ilf_dsize = (unsigned)data_bytes;
		} else {
			iip->ili_fields &= ~XFS_ILOG_DDATA;
		}
		break;

	case XFS_DINODE_FMT_DEV:
		iip->ili_fields &=
			~(XFS_ILOG_DDATA | XFS_ILOG_DBROOT |
			  XFS_ILOG_DEXT | XFS_ILOG_UUID);
		if (iip->ili_fields & XFS_ILOG_DEV) {
			iip->ili_format.ilf_u.ilfu_rdev =
				ip->i_df.if_u2.if_rdev;
		}
		break;

	case XFS_DINODE_FMT_UUID:
		iip->ili_fields &=
			~(XFS_ILOG_DDATA | XFS_ILOG_DBROOT |
			  XFS_ILOG_DEXT | XFS_ILOG_DEV);
		if (iip->ili_fields & XFS_ILOG_UUID) {
			iip->ili_format.ilf_u.ilfu_uuid =
				ip->i_df.if_u2.if_uuid;
		}
		break;

	default:
		ASSERT(0);
		break;
	}

	if (!XFS_IFORK_Q(ip)) {
		iip->ili_fields &=
			~(XFS_ILOG_ADATA | XFS_ILOG_ABROOT | XFS_ILOG_AEXT);
		goto out;
	}

	switch (ip->i_d.di_aformat) {
	case XFS_DINODE_FMT_EXTENTS:
		iip->ili_fields &=
			~(XFS_ILOG_ADATA | XFS_ILOG_ABROOT);

		if ((iip->ili_fields & XFS_ILOG_AEXT) &&
		    ip->i_d.di_anextents > 0 &&
		    ip->i_afp->if_bytes > 0) {
			ASSERT(ip->i_afp->if_bytes / sizeof(xfs_bmbt_rec_t) ==
				ip->i_d.di_anextents);
			ASSERT(ip->i_afp->if_u1.if_extents != NULL);
#ifdef XFS_NATIVE_HOST
			vecp->i_addr = ip->i_afp->if_u1.if_extents;
			vecp->i_len = ip->i_afp->if_bytes;
			vecp->i_type = XLOG_REG_TYPE_IATTR_EXT;
#else
			ASSERT(iip->ili_aextents_buf == NULL);
			xfs_inode_item_format_extents(ip, vecp,
					XFS_ATTR_FORK, XLOG_REG_TYPE_IATTR_EXT);
#endif
			iip->ili_format.ilf_asize = vecp->i_len;
			vecp++;
			nvecs++;
		} else {
			iip->ili_fields &= ~XFS_ILOG_AEXT;
		}
		break;

	case XFS_DINODE_FMT_BTREE:
		iip->ili_fields &=
			~(XFS_ILOG_ADATA | XFS_ILOG_AEXT);

		if ((iip->ili_fields & XFS_ILOG_ABROOT) &&
		    ip->i_afp->if_broot_bytes > 0) {
			ASSERT(ip->i_afp->if_broot != NULL);

			vecp->i_addr = ip->i_afp->if_broot;
			vecp->i_len = ip->i_afp->if_broot_bytes;
			vecp->i_type = XLOG_REG_TYPE_IATTR_BROOT;
			vecp++;
			nvecs++;
			iip->ili_format.ilf_asize = ip->i_afp->if_broot_bytes;
		} else {
			iip->ili_fields &= ~XFS_ILOG_ABROOT;
		}
		break;

	case XFS_DINODE_FMT_LOCAL:
		iip->ili_fields &=
			~(XFS_ILOG_AEXT | XFS_ILOG_ABROOT);

		if ((iip->ili_fields & XFS_ILOG_ADATA) &&
		    ip->i_afp->if_bytes > 0) {
			ASSERT(ip->i_afp->if_u1.if_data != NULL);

			vecp->i_addr = ip->i_afp->if_u1.if_data;
			data_bytes = roundup(ip->i_afp->if_bytes, 4);
			ASSERT((ip->i_afp->if_real_bytes == 0) ||
			       (ip->i_afp->if_real_bytes == data_bytes));
			vecp->i_len = (int)data_bytes;
			vecp->i_type = XLOG_REG_TYPE_IATTR_LOCAL;
			vecp++;
			nvecs++;
			iip->ili_format.ilf_asize = (unsigned)data_bytes;
		} else {
			iip->ili_fields &= ~XFS_ILOG_ADATA;
		}
		break;

	default:
		ASSERT(0);
		break;
	}

out:
	iip->ili_format.ilf_fields = XFS_ILOG_CORE |
		(iip->ili_fields & ~XFS_ILOG_TIMESTAMP);
	iip->ili_format.ilf_size = nvecs;
}


/*
 * This is called to pin the inode associated with the inode log
 * item in memory so it cannot be written out.
 */
STATIC void
xfs_inode_item_pin(
	struct xfs_log_item	*lip)
{
	struct xfs_inode	*ip = INODE_ITEM(lip)->ili_inode;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	trace_xfs_inode_pin(ip, _RET_IP_);
	atomic_inc(&ip->i_pincount);
}


STATIC void
xfs_inode_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
	struct xfs_inode	*ip = INODE_ITEM(lip)->ili_inode;

	trace_xfs_inode_unpin(ip, _RET_IP_);
	ASSERT(atomic_read(&ip->i_pincount) > 0);
	if (atomic_dec_and_test(&ip->i_pincount))
		wake_up_bit(&ip->i_flags, __XFS_IPINNED_BIT);
}

STATIC uint
xfs_inode_item_trylock(
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;

	if (xfs_ipincount(ip) > 0)
		return XFS_ITEM_PINNED;

	if (!xfs_ilock_nowait(ip, XFS_ILOCK_SHARED))
		return XFS_ITEM_LOCKED;

	if (!xfs_iflock_nowait(ip)) {
		return XFS_ITEM_PUSHBUF;
	}

	
	if (ip->i_flags & XFS_ISTALE) {
		xfs_ifunlock(ip);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return XFS_ITEM_PINNED;
	}

#ifdef DEBUG
	if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		ASSERT(iip->ili_fields != 0);
		ASSERT(iip->ili_logged == 0);
		ASSERT(lip->li_flags & XFS_LI_IN_AIL);
	}
#endif
	return XFS_ITEM_SUCCESS;
}

STATIC void
xfs_inode_item_unlock(
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;
	unsigned short		lock_flags;

	ASSERT(ip->i_itemp != NULL);
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	if (iip->ili_extents_buf != NULL) {
		ASSERT(ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS);
		ASSERT(ip->i_d.di_nextents > 0);
		ASSERT(iip->ili_fields & XFS_ILOG_DEXT);
		ASSERT(ip->i_df.if_bytes > 0);
		kmem_free(iip->ili_extents_buf);
		iip->ili_extents_buf = NULL;
	}
	if (iip->ili_aextents_buf != NULL) {
		ASSERT(ip->i_d.di_aformat == XFS_DINODE_FMT_EXTENTS);
		ASSERT(ip->i_d.di_anextents > 0);
		ASSERT(iip->ili_fields & XFS_ILOG_AEXT);
		ASSERT(ip->i_afp->if_bytes > 0);
		kmem_free(iip->ili_aextents_buf);
		iip->ili_aextents_buf = NULL;
	}

	lock_flags = iip->ili_lock_flags;
	iip->ili_lock_flags = 0;
	if (lock_flags)
		xfs_iunlock(ip, lock_flags);
}

/*
 * This is called to find out where the oldest active copy of the inode log
 * item in the on disk log resides now that the last log write of it completed
 * at the given lsn.  Since we always re-log all dirty data in an inode, the
 * latest copy in the on disk log is the only one that matters.  Therefore,
 * simply return the given lsn.
 *
 * If the inode has been marked stale because the cluster is being freed, we
 * don't want to (re-)insert this inode into the AIL. There is a race condition
 * where the cluster buffer may be unpinned before the inode is inserted into
 * the AIL during transaction committed processing. If the buffer is unpinned
 * before the inode item has been committed and inserted, then it is possible
 * for the buffer to be written and IO completes before the inode is inserted
 * into the AIL. In that case, we'd be inserting a clean, stale inode into the
 * AIL which will never get removed. It will, however, get reclaimed which
 * triggers an assert in xfs_inode_free() complaining about freein an inode
 * still in the AIL.
 *
 * To avoid this, just unpin the inode directly and return a LSN of -1 so the
 * transaction committed code knows that it does not need to do any further
 * processing on the item.
 */
STATIC xfs_lsn_t
xfs_inode_item_committed(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;

	if (xfs_iflags_test(ip, XFS_ISTALE)) {
		xfs_inode_item_unpin(lip, 0);
		return -1;
	}
	return lsn;
}

STATIC bool
xfs_inode_item_pushbuf(
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;
	struct xfs_buf		*bp;
	bool			ret = true;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_SHARED));

	if (!xfs_isiflocked(ip) ||
	    !(lip->li_flags & XFS_LI_IN_AIL)) {
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return true;
	}

	bp = xfs_incore(ip->i_mount->m_ddev_targp, iip->ili_format.ilf_blkno,
			iip->ili_format.ilf_len, XBF_TRYLOCK);

	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	if (!bp)
		return true;
	if (XFS_BUF_ISDELAYWRITE(bp))
		xfs_buf_delwri_promote(bp);
	if (xfs_buf_ispinned(bp))
		ret = false;
	xfs_buf_relse(bp);
	return ret;
}

STATIC void
xfs_inode_item_push(
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip = INODE_ITEM(lip);
	struct xfs_inode	*ip = iip->ili_inode;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_SHARED));
	ASSERT(xfs_isiflocked(ip));

	ASSERT(XFS_FORCED_SHUTDOWN(ip->i_mount) || iip->ili_fields != 0);

	(void) xfs_iflush(ip, SYNC_TRYLOCK);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
}

STATIC void
xfs_inode_item_committing(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	INODE_ITEM(lip)->ili_last_lsn = lsn;
}

static const struct xfs_item_ops xfs_inode_item_ops = {
	.iop_size	= xfs_inode_item_size,
	.iop_format	= xfs_inode_item_format,
	.iop_pin	= xfs_inode_item_pin,
	.iop_unpin	= xfs_inode_item_unpin,
	.iop_trylock	= xfs_inode_item_trylock,
	.iop_unlock	= xfs_inode_item_unlock,
	.iop_committed	= xfs_inode_item_committed,
	.iop_push	= xfs_inode_item_push,
	.iop_pushbuf	= xfs_inode_item_pushbuf,
	.iop_committing = xfs_inode_item_committing
};


void
xfs_inode_item_init(
	struct xfs_inode	*ip,
	struct xfs_mount	*mp)
{
	struct xfs_inode_log_item *iip;

	ASSERT(ip->i_itemp == NULL);
	iip = ip->i_itemp = kmem_zone_zalloc(xfs_ili_zone, KM_SLEEP);

	iip->ili_inode = ip;
	xfs_log_item_init(mp, &iip->ili_item, XFS_LI_INODE,
						&xfs_inode_item_ops);
	iip->ili_format.ilf_type = XFS_LI_INODE;
	iip->ili_format.ilf_ino = ip->i_ino;
	iip->ili_format.ilf_blkno = ip->i_imap.im_blkno;
	iip->ili_format.ilf_len = ip->i_imap.im_len;
	iip->ili_format.ilf_boffset = ip->i_imap.im_boffset;
}

void
xfs_inode_item_destroy(
	xfs_inode_t	*ip)
{
#ifdef XFS_TRANS_DEBUG
	if (ip->i_itemp->ili_root_size != 0) {
		kmem_free(ip->i_itemp->ili_orig_root);
	}
#endif
	kmem_zone_free(xfs_ili_zone, ip->i_itemp);
}


void
xfs_iflush_done(
	struct xfs_buf		*bp,
	struct xfs_log_item	*lip)
{
	struct xfs_inode_log_item *iip;
	struct xfs_log_item	*blip;
	struct xfs_log_item	*next;
	struct xfs_log_item	*prev;
	struct xfs_ail		*ailp = lip->li_ailp;
	int			need_ail = 0;

	blip = bp->b_fspriv;
	prev = NULL;
	while (blip != NULL) {
		if (lip->li_cb != xfs_iflush_done) {
			prev = blip;
			blip = blip->li_bio_list;
			continue;
		}

		
		next = blip->li_bio_list;
		if (!prev) {
			bp->b_fspriv = next;
		} else {
			prev->li_bio_list = next;
		}

		
		blip->li_bio_list = lip->li_bio_list;
		lip->li_bio_list = blip;

		iip = INODE_ITEM(blip);
		if (iip->ili_logged && blip->li_lsn == iip->ili_flush_lsn)
			need_ail++;

		blip = next;
	}

	
	iip = INODE_ITEM(lip);
	if (iip->ili_logged && lip->li_lsn == iip->ili_flush_lsn)
		need_ail++;

	if (need_ail) {
		struct xfs_log_item *log_items[need_ail];
		int i = 0;
		spin_lock(&ailp->xa_lock);
		for (blip = lip; blip; blip = blip->li_bio_list) {
			iip = INODE_ITEM(blip);
			if (iip->ili_logged &&
			    blip->li_lsn == iip->ili_flush_lsn) {
				log_items[i++] = blip;
			}
			ASSERT(i <= need_ail);
		}
		
		xfs_trans_ail_delete_bulk(ailp, log_items, i);
	}


	for (blip = lip; blip; blip = next) {
		next = blip->li_bio_list;
		blip->li_bio_list = NULL;

		iip = INODE_ITEM(blip);
		iip->ili_logged = 0;
		iip->ili_last_fields = 0;
		xfs_ifunlock(iip->ili_inode);
	}
}

void
xfs_iflush_abort(
	xfs_inode_t		*ip)
{
	xfs_inode_log_item_t	*iip = ip->i_itemp;

	if (iip) {
		struct xfs_ail	*ailp = iip->ili_item.li_ailp;
		if (iip->ili_item.li_flags & XFS_LI_IN_AIL) {
			spin_lock(&ailp->xa_lock);
			if (iip->ili_item.li_flags & XFS_LI_IN_AIL) {
				
				xfs_trans_ail_delete(ailp, (xfs_log_item_t *)iip);
			} else
				spin_unlock(&ailp->xa_lock);
		}
		iip->ili_logged = 0;
		iip->ili_last_fields = 0;
		iip->ili_fields = 0;
	}
	xfs_ifunlock(ip);
}

void
xfs_istale_done(
	struct xfs_buf		*bp,
	struct xfs_log_item	*lip)
{
	xfs_iflush_abort(INODE_ITEM(lip)->ili_inode);
}

int
xfs_inode_item_format_convert(
	xfs_log_iovec_t		*buf,
	xfs_inode_log_format_t	*in_f)
{
	if (buf->i_len == sizeof(xfs_inode_log_format_32_t)) {
		xfs_inode_log_format_32_t *in_f32 = buf->i_addr;

		in_f->ilf_type = in_f32->ilf_type;
		in_f->ilf_size = in_f32->ilf_size;
		in_f->ilf_fields = in_f32->ilf_fields;
		in_f->ilf_asize = in_f32->ilf_asize;
		in_f->ilf_dsize = in_f32->ilf_dsize;
		in_f->ilf_ino = in_f32->ilf_ino;
		
		memcpy(in_f->ilf_u.ilfu_uuid.__u_bits,
		       in_f32->ilf_u.ilfu_uuid.__u_bits,
		       sizeof(uuid_t));
		in_f->ilf_blkno = in_f32->ilf_blkno;
		in_f->ilf_len = in_f32->ilf_len;
		in_f->ilf_boffset = in_f32->ilf_boffset;
		return 0;
	} else if (buf->i_len == sizeof(xfs_inode_log_format_64_t)){
		xfs_inode_log_format_64_t *in_f64 = buf->i_addr;

		in_f->ilf_type = in_f64->ilf_type;
		in_f->ilf_size = in_f64->ilf_size;
		in_f->ilf_fields = in_f64->ilf_fields;
		in_f->ilf_asize = in_f64->ilf_asize;
		in_f->ilf_dsize = in_f64->ilf_dsize;
		in_f->ilf_ino = in_f64->ilf_ino;
		
		memcpy(in_f->ilf_u.ilfu_uuid.__u_bits,
		       in_f64->ilf_u.ilfu_uuid.__u_bits,
		       sizeof(uuid_t));
		in_f->ilf_blkno = in_f64->ilf_blkno;
		in_f->ilf_len = in_f64->ilf_len;
		in_f->ilf_boffset = in_f64->ilf_boffset;
		return 0;
	}
	return EFSCORRUPTED;
}
