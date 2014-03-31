/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_alloc.h"
#include "xfs_quota.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_attr.h"
#include "xfs_buf_item.h"
#include "xfs_trans_space.h"
#include "xfs_trans_priv.h"
#include "xfs_qm.h"
#include "xfs_trace.h"


#ifdef DEBUG
xfs_buftarg_t *xfs_dqerror_target;
int xfs_do_dqerror;
int xfs_dqreq_num;
int xfs_dqerror_mod = 33;
#endif

struct kmem_zone		*xfs_qm_dqtrxzone;
static struct kmem_zone		*xfs_qm_dqzone;

static struct lock_class_key xfs_dquot_other_class;

void
xfs_qm_dqdestroy(
	xfs_dquot_t	*dqp)
{
	ASSERT(list_empty(&dqp->q_lru));

	mutex_destroy(&dqp->q_qlock);
	kmem_zone_free(xfs_qm_dqzone, dqp);

	XFS_STATS_DEC(xs_qm_dquot);
}

void
xfs_qm_adjust_dqlimits(
	xfs_mount_t		*mp,
	xfs_disk_dquot_t	*d)
{
	xfs_quotainfo_t		*q = mp->m_quotainfo;

	ASSERT(d->d_id);

	if (q->qi_bsoftlimit && !d->d_blk_softlimit)
		d->d_blk_softlimit = cpu_to_be64(q->qi_bsoftlimit);
	if (q->qi_bhardlimit && !d->d_blk_hardlimit)
		d->d_blk_hardlimit = cpu_to_be64(q->qi_bhardlimit);
	if (q->qi_isoftlimit && !d->d_ino_softlimit)
		d->d_ino_softlimit = cpu_to_be64(q->qi_isoftlimit);
	if (q->qi_ihardlimit && !d->d_ino_hardlimit)
		d->d_ino_hardlimit = cpu_to_be64(q->qi_ihardlimit);
	if (q->qi_rtbsoftlimit && !d->d_rtb_softlimit)
		d->d_rtb_softlimit = cpu_to_be64(q->qi_rtbsoftlimit);
	if (q->qi_rtbhardlimit && !d->d_rtb_hardlimit)
		d->d_rtb_hardlimit = cpu_to_be64(q->qi_rtbhardlimit);
}

void
xfs_qm_adjust_dqtimers(
	xfs_mount_t		*mp,
	xfs_disk_dquot_t	*d)
{
	ASSERT(d->d_id);

#ifdef DEBUG
	if (d->d_blk_hardlimit)
		ASSERT(be64_to_cpu(d->d_blk_softlimit) <=
		       be64_to_cpu(d->d_blk_hardlimit));
	if (d->d_ino_hardlimit)
		ASSERT(be64_to_cpu(d->d_ino_softlimit) <=
		       be64_to_cpu(d->d_ino_hardlimit));
	if (d->d_rtb_hardlimit)
		ASSERT(be64_to_cpu(d->d_rtb_softlimit) <=
		       be64_to_cpu(d->d_rtb_hardlimit));
#endif

	if (!d->d_btimer) {
		if ((d->d_blk_softlimit &&
		     (be64_to_cpu(d->d_bcount) >
		      be64_to_cpu(d->d_blk_softlimit))) ||
		    (d->d_blk_hardlimit &&
		     (be64_to_cpu(d->d_bcount) >
		      be64_to_cpu(d->d_blk_hardlimit)))) {
			d->d_btimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_btimelimit);
		} else {
			d->d_bwarns = 0;
		}
	} else {
		if ((!d->d_blk_softlimit ||
		     (be64_to_cpu(d->d_bcount) <=
		      be64_to_cpu(d->d_blk_softlimit))) &&
		    (!d->d_blk_hardlimit ||
		    (be64_to_cpu(d->d_bcount) <=
		     be64_to_cpu(d->d_blk_hardlimit)))) {
			d->d_btimer = 0;
		}
	}

	if (!d->d_itimer) {
		if ((d->d_ino_softlimit &&
		     (be64_to_cpu(d->d_icount) >
		      be64_to_cpu(d->d_ino_softlimit))) ||
		    (d->d_ino_hardlimit &&
		     (be64_to_cpu(d->d_icount) >
		      be64_to_cpu(d->d_ino_hardlimit)))) {
			d->d_itimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_itimelimit);
		} else {
			d->d_iwarns = 0;
		}
	} else {
		if ((!d->d_ino_softlimit ||
		     (be64_to_cpu(d->d_icount) <=
		      be64_to_cpu(d->d_ino_softlimit)))  &&
		    (!d->d_ino_hardlimit ||
		     (be64_to_cpu(d->d_icount) <=
		      be64_to_cpu(d->d_ino_hardlimit)))) {
			d->d_itimer = 0;
		}
	}

	if (!d->d_rtbtimer) {
		if ((d->d_rtb_softlimit &&
		     (be64_to_cpu(d->d_rtbcount) >
		      be64_to_cpu(d->d_rtb_softlimit))) ||
		    (d->d_rtb_hardlimit &&
		     (be64_to_cpu(d->d_rtbcount) >
		      be64_to_cpu(d->d_rtb_hardlimit)))) {
			d->d_rtbtimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_rtbtimelimit);
		} else {
			d->d_rtbwarns = 0;
		}
	} else {
		if ((!d->d_rtb_softlimit ||
		     (be64_to_cpu(d->d_rtbcount) <=
		      be64_to_cpu(d->d_rtb_softlimit))) &&
		    (!d->d_rtb_hardlimit ||
		     (be64_to_cpu(d->d_rtbcount) <=
		      be64_to_cpu(d->d_rtb_hardlimit)))) {
			d->d_rtbtimer = 0;
		}
	}
}

STATIC void
xfs_qm_init_dquot_blk(
	xfs_trans_t	*tp,
	xfs_mount_t	*mp,
	xfs_dqid_t	id,
	uint		type,
	xfs_buf_t	*bp)
{
	struct xfs_quotainfo	*q = mp->m_quotainfo;
	xfs_dqblk_t	*d;
	int		curid, i;

	ASSERT(tp);
	ASSERT(xfs_buf_islocked(bp));

	d = bp->b_addr;

	curid = id - (id % q->qi_dqperchunk);
	ASSERT(curid >= 0);
	memset(d, 0, BBTOB(q->qi_dqchunklen));
	for (i = 0; i < q->qi_dqperchunk; i++, d++, curid++) {
		d->dd_diskdq.d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
		d->dd_diskdq.d_version = XFS_DQUOT_VERSION;
		d->dd_diskdq.d_id = cpu_to_be32(curid);
		d->dd_diskdq.d_flags = type;
	}

	xfs_trans_dquot_buf(tp, bp,
			    (type & XFS_DQ_USER ? XFS_BLF_UDQUOT_BUF :
			    ((type & XFS_DQ_PROJ) ? XFS_BLF_PDQUOT_BUF :
			     XFS_BLF_GDQUOT_BUF)));
	xfs_trans_log_buf(tp, bp, 0, BBTOB(q->qi_dqchunklen) - 1);
}



STATIC int
xfs_qm_dqalloc(
	xfs_trans_t	**tpp,
	xfs_mount_t	*mp,
	xfs_dquot_t	*dqp,
	xfs_inode_t	*quotip,
	xfs_fileoff_t	offset_fsb,
	xfs_buf_t	**O_bpp)
{
	xfs_fsblock_t	firstblock;
	xfs_bmap_free_t flist;
	xfs_bmbt_irec_t map;
	int		nmaps, error, committed;
	xfs_buf_t	*bp;
	xfs_trans_t	*tp = *tpp;

	ASSERT(tp != NULL);

	trace_xfs_dqalloc(dqp);

	xfs_bmap_init(&flist, &firstblock);
	xfs_ilock(quotip, XFS_ILOCK_EXCL);
	if (!xfs_this_quota_on(dqp->q_mount, dqp->dq_flags)) {
		xfs_iunlock(quotip, XFS_ILOCK_EXCL);
		return (ESRCH);
	}

	xfs_trans_ijoin(tp, quotip, XFS_ILOCK_EXCL);
	nmaps = 1;
	error = xfs_bmapi_write(tp, quotip, offset_fsb,
				XFS_DQUOT_CLUSTER_SIZE_FSB, XFS_BMAPI_METADATA,
				&firstblock, XFS_QM_DQALLOC_SPACE_RES(mp),
				&map, &nmaps, &flist);
	if (error)
		goto error0;
	ASSERT(map.br_blockcount == XFS_DQUOT_CLUSTER_SIZE_FSB);
	ASSERT(nmaps == 1);
	ASSERT((map.br_startblock != DELAYSTARTBLOCK) &&
	       (map.br_startblock != HOLESTARTBLOCK));

	dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);

	
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp,
			       dqp->q_blkno,
			       mp->m_quotainfo->qi_dqchunklen,
			       0);

	error = xfs_buf_geterror(bp);
	if (error)
		goto error1;

	xfs_qm_init_dquot_blk(tp, mp, be32_to_cpu(dqp->q_core.d_id),
			      dqp->dq_flags & XFS_DQ_ALLTYPES, bp);


	xfs_trans_bhold(tp, bp);

	if ((error = xfs_bmap_finish(tpp, &flist, &committed))) {
		goto error1;
	}

	if (committed) {
		tp = *tpp;
		xfs_trans_bjoin(tp, bp);
	} else {
		xfs_trans_bhold_release(tp, bp);
	}

	*O_bpp = bp;
	return 0;

      error1:
	xfs_bmap_cancel(&flist);
      error0:
	xfs_iunlock(quotip, XFS_ILOCK_EXCL);

	return (error);
}

STATIC int
xfs_qm_dqtobp(
	xfs_trans_t		**tpp,
	xfs_dquot_t		*dqp,
	xfs_disk_dquot_t	**O_ddpp,
	xfs_buf_t		**O_bpp,
	uint			flags)
{
	xfs_bmbt_irec_t map;
	int		nmaps = 1, error;
	xfs_buf_t	*bp;
	xfs_inode_t	*quotip = XFS_DQ_TO_QIP(dqp);
	xfs_mount_t	*mp = dqp->q_mount;
	xfs_disk_dquot_t *ddq;
	xfs_dqid_t	id = be32_to_cpu(dqp->q_core.d_id);
	xfs_trans_t	*tp = (tpp ? *tpp : NULL);

	dqp->q_fileoffset = (xfs_fileoff_t)id / mp->m_quotainfo->qi_dqperchunk;

	xfs_ilock(quotip, XFS_ILOCK_SHARED);
	if (!xfs_this_quota_on(dqp->q_mount, dqp->dq_flags)) {
		xfs_iunlock(quotip, XFS_ILOCK_SHARED);
		return ESRCH;
	}

	error = xfs_bmapi_read(quotip, dqp->q_fileoffset,
			       XFS_DQUOT_CLUSTER_SIZE_FSB, &map, &nmaps, 0);

	xfs_iunlock(quotip, XFS_ILOCK_SHARED);
	if (error)
		return error;

	ASSERT(nmaps == 1);
	ASSERT(map.br_blockcount == 1);

	dqp->q_bufoffset = (id % mp->m_quotainfo->qi_dqperchunk) *
		sizeof(xfs_dqblk_t);

	ASSERT(map.br_startblock != DELAYSTARTBLOCK);
	if (map.br_startblock == HOLESTARTBLOCK) {
		if (!(flags & XFS_QMOPT_DQALLOC))
			return ENOENT;

		ASSERT(tp);
		error = xfs_qm_dqalloc(tpp, mp, dqp, quotip,
					dqp->q_fileoffset, &bp);
		if (error)
			return error;
		tp = *tpp;
	} else {
		trace_xfs_dqtobp_read(dqp);

		dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);

		error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
					   dqp->q_blkno,
					   mp->m_quotainfo->qi_dqchunklen,
					   0, &bp);
		if (error || !bp)
			return XFS_ERROR(error);
	}

	ASSERT(xfs_buf_islocked(bp));

	ddq = bp->b_addr + dqp->q_bufoffset;

	error = xfs_qm_dqcheck(mp, ddq, id, dqp->dq_flags & XFS_DQ_ALLTYPES,
			   flags & (XFS_QMOPT_DQREPAIR|XFS_QMOPT_DOWARN),
			   "dqtobp");
	if (error) {
		if (!(flags & XFS_QMOPT_DQREPAIR)) {
			xfs_trans_brelse(tp, bp);
			return XFS_ERROR(EIO);
		}
	}

	*O_bpp = bp;
	*O_ddpp = ddq;

	return (0);
}


int
xfs_qm_dqread(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	uint			type,
	uint			flags,
	struct xfs_dquot	**O_dqpp)
{
	struct xfs_dquot	*dqp;
	struct xfs_disk_dquot	*ddqp;
	struct xfs_buf		*bp;
	struct xfs_trans	*tp = NULL;
	int			error;
	int			cancelflags = 0;


	dqp = kmem_zone_zalloc(xfs_qm_dqzone, KM_SLEEP);

	dqp->dq_flags = type;
	dqp->q_core.d_id = cpu_to_be32(id);
	dqp->q_mount = mp;
	INIT_LIST_HEAD(&dqp->q_lru);
	mutex_init(&dqp->q_qlock);
	init_waitqueue_head(&dqp->q_pinwait);

	init_completion(&dqp->q_flush);
	complete(&dqp->q_flush);

	if (!(type & XFS_DQ_USER))
		lockdep_set_class(&dqp->q_qlock, &xfs_dquot_other_class);

	XFS_STATS_INC(xs_qm_dquot);

	trace_xfs_dqread(dqp);

	if (flags & XFS_QMOPT_DQALLOC) {
		tp = xfs_trans_alloc(mp, XFS_TRANS_QM_DQALLOC);
		error = xfs_trans_reserve(tp, XFS_QM_DQALLOC_SPACE_RES(mp),
				XFS_WRITE_LOG_RES(mp) +
				BBTOB(mp->m_quotainfo->qi_dqchunklen) - 1 + 128,
				0,
				XFS_TRANS_PERM_LOG_RES,
				XFS_WRITE_LOG_COUNT);
		if (error)
			goto error1;
		cancelflags = XFS_TRANS_RELEASE_LOG_RES;
	}

	error = xfs_qm_dqtobp(&tp, dqp, &ddqp, &bp, flags);
	if (error) {
		trace_xfs_dqread_fail(dqp);
		cancelflags |= XFS_TRANS_ABORT;
		goto error1;
	}

	
	memcpy(&dqp->q_core, ddqp, sizeof(xfs_disk_dquot_t));
	xfs_qm_dquot_logitem_init(dqp);

	dqp->q_res_bcount = be64_to_cpu(ddqp->d_bcount);
	dqp->q_res_icount = be64_to_cpu(ddqp->d_icount);
	dqp->q_res_rtbcount = be64_to_cpu(ddqp->d_rtbcount);

	
	xfs_buf_set_ref(bp, XFS_DQUOT_REF);

	ASSERT(xfs_buf_islocked(bp));
	xfs_trans_brelse(tp, bp);

	if (tp) {
		error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES);
		if (error)
			goto error0;
	}

	*O_dqpp = dqp;
	return error;

error1:
	if (tp)
		xfs_trans_cancel(tp, cancelflags);
error0:
	xfs_qm_dqdestroy(dqp);
	*O_dqpp = NULL;
	return error;
}

int
xfs_qm_dqget(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,	  
	xfs_dqid_t	id,	  
	uint		type,	  
	uint		flags,	  
	xfs_dquot_t	**O_dqpp) 
{
	struct xfs_quotainfo	*qi = mp->m_quotainfo;
	struct radix_tree_root *tree = XFS_DQUOT_TREE(qi, type);
	struct xfs_dquot	*dqp;
	int			error;

	ASSERT(XFS_IS_QUOTA_RUNNING(mp));
	if ((! XFS_IS_UQUOTA_ON(mp) && type == XFS_DQ_USER) ||
	    (! XFS_IS_PQUOTA_ON(mp) && type == XFS_DQ_PROJ) ||
	    (! XFS_IS_GQUOTA_ON(mp) && type == XFS_DQ_GROUP)) {
		return (ESRCH);
	}

#ifdef DEBUG
	if (xfs_do_dqerror) {
		if ((xfs_dqerror_target == mp->m_ddev_targp) &&
		    (xfs_dqreq_num++ % xfs_dqerror_mod) == 0) {
			xfs_debug(mp, "Returning error in dqget");
			return (EIO);
		}
	}

	ASSERT(type == XFS_DQ_USER ||
	       type == XFS_DQ_PROJ ||
	       type == XFS_DQ_GROUP);
	if (ip) {
		ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
		ASSERT(xfs_inode_dquot(ip, type) == NULL);
	}
#endif

restart:
	mutex_lock(&qi->qi_tree_lock);
	dqp = radix_tree_lookup(tree, id);
	if (dqp) {
		xfs_dqlock(dqp);
		if (dqp->dq_flags & XFS_DQ_FREEING) {
			xfs_dqunlock(dqp);
			mutex_unlock(&qi->qi_tree_lock);
			trace_xfs_dqget_freeing(dqp);
			delay(1);
			goto restart;
		}

		dqp->q_nrefs++;
		mutex_unlock(&qi->qi_tree_lock);

		trace_xfs_dqget_hit(dqp);
		XFS_STATS_INC(xs_qm_dqcachehits);
		*O_dqpp = dqp;
		return 0;
	}
	mutex_unlock(&qi->qi_tree_lock);
	XFS_STATS_INC(xs_qm_dqcachemisses);

	if (ip)
		xfs_iunlock(ip, XFS_ILOCK_EXCL);

	error = xfs_qm_dqread(mp, id, type, flags, &dqp);

	if (ip)
		xfs_ilock(ip, XFS_ILOCK_EXCL);

	if (error)
		return error;

	if (ip) {
		if (xfs_this_quota_on(mp, type)) {
			struct xfs_dquot	*dqp1;

			dqp1 = xfs_inode_dquot(ip, type);
			if (dqp1) {
				xfs_qm_dqdestroy(dqp);
				dqp = dqp1;
				xfs_dqlock(dqp);
				goto dqret;
			}
		} else {
			
			xfs_qm_dqdestroy(dqp);
			return XFS_ERROR(ESRCH);
		}
	}

	mutex_lock(&qi->qi_tree_lock);
	error = -radix_tree_insert(tree, id, dqp);
	if (unlikely(error)) {
		WARN_ON(error != EEXIST);

		mutex_unlock(&qi->qi_tree_lock);
		trace_xfs_dqget_dup(dqp);
		xfs_qm_dqdestroy(dqp);
		XFS_STATS_INC(xs_qm_dquot_dups);
		goto restart;
	}

	xfs_dqlock(dqp);
	dqp->q_nrefs = 1;

	qi->qi_dquots++;
	mutex_unlock(&qi->qi_tree_lock);

 dqret:
	ASSERT((ip == NULL) || xfs_isilocked(ip, XFS_ILOCK_EXCL));
	trace_xfs_dqget_miss(dqp);
	*O_dqpp = dqp;
	return (0);
}


STATIC void
xfs_qm_dqput_final(
	struct xfs_dquot	*dqp)
{
	struct xfs_quotainfo	*qi = dqp->q_mount->m_quotainfo;
	struct xfs_dquot	*gdqp;

	trace_xfs_dqput_free(dqp);

	mutex_lock(&qi->qi_lru_lock);
	if (list_empty(&dqp->q_lru)) {
		list_add_tail(&dqp->q_lru, &qi->qi_lru_list);
		qi->qi_lru_count++;
		XFS_STATS_INC(xs_qm_dquot_unused);
	}
	mutex_unlock(&qi->qi_lru_lock);

	gdqp = dqp->q_gdquot;
	if (gdqp) {
		xfs_dqlock(gdqp);
		dqp->q_gdquot = NULL;
	}
	xfs_dqunlock(dqp);

	if (gdqp)
		xfs_qm_dqput(gdqp);
}

void
xfs_qm_dqput(
	struct xfs_dquot	*dqp)
{
	ASSERT(dqp->q_nrefs > 0);
	ASSERT(XFS_DQ_IS_LOCKED(dqp));

	trace_xfs_dqput(dqp);

	if (--dqp->q_nrefs > 0)
		xfs_dqunlock(dqp);
	else
		xfs_qm_dqput_final(dqp);
}

void
xfs_qm_dqrele(
	xfs_dquot_t	*dqp)
{
	if (!dqp)
		return;

	trace_xfs_dqrele(dqp);

	xfs_dqlock(dqp);
	xfs_qm_dqput(dqp);
}

STATIC void
xfs_qm_dqflush_done(
	struct xfs_buf		*bp,
	struct xfs_log_item	*lip)
{
	xfs_dq_logitem_t	*qip = (struct xfs_dq_logitem *)lip;
	xfs_dquot_t		*dqp = qip->qli_dquot;
	struct xfs_ail		*ailp = lip->li_ailp;

	if ((lip->li_flags & XFS_LI_IN_AIL) &&
	    lip->li_lsn == qip->qli_flush_lsn) {

		
		spin_lock(&ailp->xa_lock);
		if (lip->li_lsn == qip->qli_flush_lsn)
			xfs_trans_ail_delete(ailp, lip);
		else
			spin_unlock(&ailp->xa_lock);
	}

	xfs_dqfunlock(dqp);
}

int
xfs_qm_dqflush(
	xfs_dquot_t		*dqp,
	uint			flags)
{
	struct xfs_mount	*mp = dqp->q_mount;
	struct xfs_buf		*bp;
	struct xfs_disk_dquot	*ddqp;
	int			error;

	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	ASSERT(!completion_done(&dqp->q_flush));

	trace_xfs_dqflush(dqp);

	if (!XFS_DQ_IS_DIRTY(dqp) ||
	    ((flags & SYNC_TRYLOCK) && atomic_read(&dqp->q_pincount) > 0)) {
		xfs_dqfunlock(dqp);
		return 0;
	}
	xfs_qm_dqunpin_wait(dqp);

	if (XFS_FORCED_SHUTDOWN(mp)) {
		dqp->dq_flags &= ~XFS_DQ_DIRTY;
		xfs_dqfunlock(dqp);
		return XFS_ERROR(EIO);
	}

	error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp, dqp->q_blkno,
				   mp->m_quotainfo->qi_dqchunklen, 0, &bp);
	if (error) {
		ASSERT(error != ENOENT);
		xfs_dqfunlock(dqp);
		return error;
	}

	ddqp = bp->b_addr + dqp->q_bufoffset;

	error = xfs_qm_dqcheck(mp, &dqp->q_core, be32_to_cpu(ddqp->d_id), 0,
			   XFS_QMOPT_DOWARN, "dqflush (incore copy)");
	if (error) {
		xfs_buf_relse(bp);
		xfs_dqfunlock(dqp);
		xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
		return XFS_ERROR(EIO);
	}

	
	memcpy(ddqp, &dqp->q_core, sizeof(xfs_disk_dquot_t));

	dqp->dq_flags &= ~XFS_DQ_DIRTY;

	xfs_trans_ail_copy_lsn(mp->m_ail, &dqp->q_logitem.qli_flush_lsn,
					&dqp->q_logitem.qli_item.li_lsn);

	xfs_buf_attach_iodone(bp, xfs_qm_dqflush_done,
				  &dqp->q_logitem.qli_item);

	if (xfs_buf_ispinned(bp)) {
		trace_xfs_dqflush_force(dqp);
		xfs_log_force(mp, 0);
	}

	if (flags & SYNC_WAIT)
		error = xfs_bwrite(bp);
	else
		xfs_buf_delwri_queue(bp);

	xfs_buf_relse(bp);

	trace_xfs_dqflush_done(dqp);

	return error;

}

void
xfs_dqlock2(
	xfs_dquot_t	*d1,
	xfs_dquot_t	*d2)
{
	if (d1 && d2) {
		ASSERT(d1 != d2);
		if (be32_to_cpu(d1->q_core.d_id) >
		    be32_to_cpu(d2->q_core.d_id)) {
			mutex_lock(&d2->q_qlock);
			mutex_lock_nested(&d1->q_qlock, XFS_QLOCK_NESTED);
		} else {
			mutex_lock(&d1->q_qlock);
			mutex_lock_nested(&d2->q_qlock, XFS_QLOCK_NESTED);
		}
	} else if (d1) {
		mutex_lock(&d1->q_qlock);
	} else if (d2) {
		mutex_lock(&d2->q_qlock);
	}
}

void
xfs_dqflock_pushbuf_wait(
	xfs_dquot_t	*dqp)
{
	xfs_mount_t	*mp = dqp->q_mount;
	xfs_buf_t	*bp;

	bp = xfs_incore(mp->m_ddev_targp, dqp->q_blkno,
			mp->m_quotainfo->qi_dqchunklen, XBF_TRYLOCK);
	if (!bp)
		goto out_lock;

	if (XFS_BUF_ISDELAYWRITE(bp)) {
		if (xfs_buf_ispinned(bp))
			xfs_log_force(mp, 0);
		xfs_buf_delwri_promote(bp);
		wake_up_process(bp->b_target->bt_task);
	}
	xfs_buf_relse(bp);
out_lock:
	xfs_dqflock(dqp);
}

int __init
xfs_qm_init(void)
{
	xfs_qm_dqzone =
		kmem_zone_init(sizeof(struct xfs_dquot), "xfs_dquot");
	if (!xfs_qm_dqzone)
		goto out;

	xfs_qm_dqtrxzone =
		kmem_zone_init(sizeof(struct xfs_dquot_acct), "xfs_dqtrx");
	if (!xfs_qm_dqtrxzone)
		goto out_free_dqzone;

	return 0;

out_free_dqzone:
	kmem_zone_destroy(xfs_qm_dqzone);
out:
	return -ENOMEM;
}

void
xfs_qm_exit(void)
{
	kmem_zone_destroy(xfs_qm_dqtrxzone);
	kmem_zone_destroy(xfs_qm_dqzone);
}
