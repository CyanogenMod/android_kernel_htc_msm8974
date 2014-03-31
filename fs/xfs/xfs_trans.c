/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * Copyright (C) 2010 Red Hat, Inc.
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
#include "xfs_error.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_quota.h"
#include "xfs_trans_priv.h"
#include "xfs_trans_space.h"
#include "xfs_inode_item.h"
#include "xfs_trace.h"

kmem_zone_t	*xfs_trans_zone;
kmem_zone_t	*xfs_log_item_desc_zone;




STATIC uint
xfs_calc_write_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     XFS_FSB_TO_B(mp, XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK)) +
		     2 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 2) +
		     128 * (4 + XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) +
			    XFS_ALLOCFREE_LOG_COUNT(mp, 2))),
		    (2 * mp->m_sb.sb_sectsize +
		     2 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 2) +
		     128 * (5 + XFS_ALLOCFREE_LOG_COUNT(mp, 2))));
}

STATIC uint
xfs_calc_itruncate_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     XFS_FSB_TO_B(mp, XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) + 1) +
		     128 * (2 + XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK))),
		    (4 * mp->m_sb.sb_sectsize +
		     4 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 4) +
		     128 * (9 + XFS_ALLOCFREE_LOG_COUNT(mp, 4)) +
		     128 * 5 +
		     XFS_ALLOCFREE_LOG_RES(mp, 1) +
		     128 * (2 + XFS_IALLOC_BLOCKS(mp) + mp->m_in_maxlevels +
			    XFS_ALLOCFREE_LOG_COUNT(mp, 1))));
}

STATIC uint
xfs_calc_rename_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((4 * mp->m_sb.sb_inodesize +
		     2 * XFS_DIROP_LOG_RES(mp) +
		     128 * (4 + 2 * XFS_DIROP_LOG_COUNT(mp))),
		    (3 * mp->m_sb.sb_sectsize +
		     3 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 3) +
		     128 * (7 + XFS_ALLOCFREE_LOG_COUNT(mp, 3))));
}

STATIC uint
xfs_calc_link_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     mp->m_sb.sb_inodesize +
		     XFS_DIROP_LOG_RES(mp) +
		     128 * (2 + XFS_DIROP_LOG_COUNT(mp))),
		    (mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 1) +
		     128 * (3 + XFS_ALLOCFREE_LOG_COUNT(mp, 1))));
}

STATIC uint
xfs_calc_remove_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     mp->m_sb.sb_inodesize +
		     XFS_DIROP_LOG_RES(mp) +
		     128 * (2 + XFS_DIROP_LOG_COUNT(mp))),
		    (2 * mp->m_sb.sb_sectsize +
		     2 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 2) +
		     128 * (5 + XFS_ALLOCFREE_LOG_COUNT(mp, 2))));
}

STATIC uint
xfs_calc_symlink_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     mp->m_sb.sb_inodesize +
		     XFS_FSB_TO_B(mp, 1) +
		     XFS_DIROP_LOG_RES(mp) +
		     1024 +
		     128 * (4 + XFS_DIROP_LOG_COUNT(mp))),
		    (2 * mp->m_sb.sb_sectsize +
		     XFS_FSB_TO_B(mp, XFS_IALLOC_BLOCKS(mp)) +
		     XFS_FSB_TO_B(mp, mp->m_in_maxlevels) +
		     XFS_ALLOCFREE_LOG_RES(mp, 1) +
		     128 * (2 + XFS_IALLOC_BLOCKS(mp) + mp->m_in_maxlevels +
			    XFS_ALLOCFREE_LOG_COUNT(mp, 1))));
}

STATIC uint
xfs_calc_create_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     mp->m_sb.sb_inodesize +
		     mp->m_sb.sb_sectsize +
		     XFS_FSB_TO_B(mp, 1) +
		     XFS_DIROP_LOG_RES(mp) +
		     128 * (3 + XFS_DIROP_LOG_COUNT(mp))),
		    (3 * mp->m_sb.sb_sectsize +
		     XFS_FSB_TO_B(mp, XFS_IALLOC_BLOCKS(mp)) +
		     XFS_FSB_TO_B(mp, mp->m_in_maxlevels) +
		     XFS_ALLOCFREE_LOG_RES(mp, 1) +
		     128 * (2 + XFS_IALLOC_BLOCKS(mp) + mp->m_in_maxlevels +
			    XFS_ALLOCFREE_LOG_COUNT(mp, 1))));
}

STATIC uint
xfs_calc_mkdir_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_create_reservation(mp);
}

STATIC uint
xfs_calc_ifree_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		mp->m_sb.sb_inodesize +
		mp->m_sb.sb_sectsize +
		mp->m_sb.sb_sectsize +
		XFS_FSB_TO_B(mp, 1) +
		MAX((__uint16_t)XFS_FSB_TO_B(mp, 1),
		    XFS_INODE_CLUSTER_SIZE(mp)) +
		128 * 5 +
		XFS_ALLOCFREE_LOG_RES(mp, 1) +
		128 * (2 + XFS_IALLOC_BLOCKS(mp) + mp->m_in_maxlevels +
		       XFS_ALLOCFREE_LOG_COUNT(mp, 1));
}

STATIC uint
xfs_calc_ichange_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		mp->m_sb.sb_inodesize +
		mp->m_sb.sb_sectsize +
		512;

}

STATIC uint
xfs_calc_growdata_reservation(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_sectsize * 3 +
		XFS_ALLOCFREE_LOG_RES(mp, 1) +
		128 * (3 + XFS_ALLOCFREE_LOG_COUNT(mp, 1));
}

STATIC uint
xfs_calc_growrtalloc_reservation(
	struct xfs_mount	*mp)
{
	return 2 * mp->m_sb.sb_sectsize +
		XFS_FSB_TO_B(mp, XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK)) +
		mp->m_sb.sb_inodesize +
		XFS_ALLOCFREE_LOG_RES(mp, 1) +
		128 * (3 + XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) +
		       XFS_ALLOCFREE_LOG_COUNT(mp, 1));
}

STATIC uint
xfs_calc_growrtzero_reservation(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_blocksize + 128;
}

STATIC uint
xfs_calc_growrtfree_reservation(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_sectsize +
		2 * mp->m_sb.sb_inodesize +
		mp->m_sb.sb_blocksize +
		mp->m_rsumsize +
		128 * 5;
}

STATIC uint
xfs_calc_swrite_reservation(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_inodesize + 128;
}

STATIC uint
xfs_calc_writeid_reservation(xfs_mount_t *mp)
{
	return mp->m_sb.sb_inodesize + 128;
}

STATIC uint
xfs_calc_addafork_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		mp->m_sb.sb_inodesize +
		mp->m_sb.sb_sectsize * 2 +
		mp->m_dirblksize +
		XFS_FSB_TO_B(mp, XFS_DAENTER_BMAP1B(mp, XFS_DATA_FORK) + 1) +
		XFS_ALLOCFREE_LOG_RES(mp, 1) +
		128 * (4 + XFS_DAENTER_BMAP1B(mp, XFS_DATA_FORK) + 1 +
		       XFS_ALLOCFREE_LOG_COUNT(mp, 1));
}

STATIC uint
xfs_calc_attrinval_reservation(
	struct xfs_mount	*mp)
{
	return MAX((mp->m_sb.sb_inodesize +
		    XFS_FSB_TO_B(mp, XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK)) +
		    128 * (1 + XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK))),
		   (4 * mp->m_sb.sb_sectsize +
		    4 * mp->m_sb.sb_sectsize +
		    mp->m_sb.sb_sectsize +
		    XFS_ALLOCFREE_LOG_RES(mp, 4) +
		    128 * (9 + XFS_ALLOCFREE_LOG_COUNT(mp, 4))));
}

STATIC uint
xfs_calc_attrset_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		mp->m_sb.sb_inodesize +
		mp->m_sb.sb_sectsize +
		XFS_FSB_TO_B(mp, XFS_DA_NODE_MAXDEPTH) +
		128 * (2 + XFS_DA_NODE_MAXDEPTH);
}

STATIC uint
xfs_calc_attrrm_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		MAX((mp->m_sb.sb_inodesize +
		     XFS_FSB_TO_B(mp, XFS_DA_NODE_MAXDEPTH) +
		     XFS_FSB_TO_B(mp, XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK)) +
		     128 * (1 + XFS_DA_NODE_MAXDEPTH +
			    XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK))),
		    (2 * mp->m_sb.sb_sectsize +
		     2 * mp->m_sb.sb_sectsize +
		     mp->m_sb.sb_sectsize +
		     XFS_ALLOCFREE_LOG_RES(mp, 2) +
		     128 * (5 + XFS_ALLOCFREE_LOG_COUNT(mp, 2))));
}

STATIC uint
xfs_calc_clear_agi_bucket_reservation(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_sectsize + 128;
}

void
xfs_trans_init(
	struct xfs_mount	*mp)
{
	struct xfs_trans_reservations *resp = &mp->m_reservations;

	resp->tr_write = xfs_calc_write_reservation(mp);
	resp->tr_itruncate = xfs_calc_itruncate_reservation(mp);
	resp->tr_rename = xfs_calc_rename_reservation(mp);
	resp->tr_link = xfs_calc_link_reservation(mp);
	resp->tr_remove = xfs_calc_remove_reservation(mp);
	resp->tr_symlink = xfs_calc_symlink_reservation(mp);
	resp->tr_create = xfs_calc_create_reservation(mp);
	resp->tr_mkdir = xfs_calc_mkdir_reservation(mp);
	resp->tr_ifree = xfs_calc_ifree_reservation(mp);
	resp->tr_ichange = xfs_calc_ichange_reservation(mp);
	resp->tr_growdata = xfs_calc_growdata_reservation(mp);
	resp->tr_swrite = xfs_calc_swrite_reservation(mp);
	resp->tr_writeid = xfs_calc_writeid_reservation(mp);
	resp->tr_addafork = xfs_calc_addafork_reservation(mp);
	resp->tr_attrinval = xfs_calc_attrinval_reservation(mp);
	resp->tr_attrset = xfs_calc_attrset_reservation(mp);
	resp->tr_attrrm = xfs_calc_attrrm_reservation(mp);
	resp->tr_clearagi = xfs_calc_clear_agi_bucket_reservation(mp);
	resp->tr_growrtalloc = xfs_calc_growrtalloc_reservation(mp);
	resp->tr_growrtzero = xfs_calc_growrtzero_reservation(mp);
	resp->tr_growrtfree = xfs_calc_growrtfree_reservation(mp);
}

xfs_trans_t *
xfs_trans_alloc(
	xfs_mount_t	*mp,
	uint		type)
{
	xfs_wait_for_freeze(mp, SB_FREEZE_TRANS);
	return _xfs_trans_alloc(mp, type, KM_SLEEP);
}

xfs_trans_t *
_xfs_trans_alloc(
	xfs_mount_t	*mp,
	uint		type,
	uint		memflags)
{
	xfs_trans_t	*tp;

	atomic_inc(&mp->m_active_trans);

	tp = kmem_zone_zalloc(xfs_trans_zone, memflags);
	tp->t_magic = XFS_TRANS_MAGIC;
	tp->t_type = type;
	tp->t_mountp = mp;
	INIT_LIST_HEAD(&tp->t_items);
	INIT_LIST_HEAD(&tp->t_busy);
	return tp;
}

STATIC void
xfs_trans_free(
	struct xfs_trans	*tp)
{
	xfs_alloc_busy_sort(&tp->t_busy);
	xfs_alloc_busy_clear(tp->t_mountp, &tp->t_busy, false);

	atomic_dec(&tp->t_mountp->m_active_trans);
	xfs_trans_free_dqinfo(tp);
	kmem_zone_free(xfs_trans_zone, tp);
}

xfs_trans_t *
xfs_trans_dup(
	xfs_trans_t	*tp)
{
	xfs_trans_t	*ntp;

	ntp = kmem_zone_zalloc(xfs_trans_zone, KM_SLEEP);

	ntp->t_magic = XFS_TRANS_MAGIC;
	ntp->t_type = tp->t_type;
	ntp->t_mountp = tp->t_mountp;
	INIT_LIST_HEAD(&ntp->t_items);
	INIT_LIST_HEAD(&ntp->t_busy);

	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
	ASSERT(tp->t_ticket != NULL);

	ntp->t_flags = XFS_TRANS_PERM_LOG_RES | (tp->t_flags & XFS_TRANS_RESERVE);
	ntp->t_ticket = xfs_log_ticket_get(tp->t_ticket);
	ntp->t_blk_res = tp->t_blk_res - tp->t_blk_res_used;
	tp->t_blk_res = tp->t_blk_res_used;
	ntp->t_rtx_res = tp->t_rtx_res - tp->t_rtx_res_used;
	tp->t_rtx_res = tp->t_rtx_res_used;
	ntp->t_pflags = tp->t_pflags;

	xfs_trans_dup_dqinfo(tp, ntp);

	atomic_inc(&tp->t_mountp->m_active_trans);
	return ntp;
}

int
xfs_trans_reserve(
	xfs_trans_t	*tp,
	uint		blocks,
	uint		logspace,
	uint		rtextents,
	uint		flags,
	uint		logcount)
{
	int		error = 0;
	int		rsvd = (tp->t_flags & XFS_TRANS_RESERVE) != 0;

	
	current_set_flags_nested(&tp->t_pflags, PF_FSTRANS);

	if (blocks > 0) {
		error = xfs_icsb_modify_counters(tp->t_mountp, XFS_SBS_FDBLOCKS,
					  -((int64_t)blocks), rsvd);
		if (error != 0) {
			current_restore_flags_nested(&tp->t_pflags, PF_FSTRANS);
			return (XFS_ERROR(ENOSPC));
		}
		tp->t_blk_res += blocks;
	}

	if (logspace > 0) {
		bool	permanent = false;

		ASSERT(tp->t_log_res == 0 || tp->t_log_res == logspace);
		ASSERT(tp->t_log_count == 0 || tp->t_log_count == logcount);

		if (flags & XFS_TRANS_PERM_LOG_RES) {
			tp->t_flags |= XFS_TRANS_PERM_LOG_RES;
			permanent = true;
		} else {
			ASSERT(tp->t_ticket == NULL);
			ASSERT(!(tp->t_flags & XFS_TRANS_PERM_LOG_RES));
		}

		if (tp->t_ticket != NULL) {
			ASSERT(flags & XFS_TRANS_PERM_LOG_RES);
			error = xfs_log_regrant(tp->t_mountp, tp->t_ticket);
		} else {
			error = xfs_log_reserve(tp->t_mountp, logspace,
						logcount, &tp->t_ticket,
						XFS_TRANSACTION, permanent,
						tp->t_type);
		}

		if (error)
			goto undo_blocks;

		tp->t_log_res = logspace;
		tp->t_log_count = logcount;
	}

	if (rtextents > 0) {
		error = xfs_mod_incore_sb(tp->t_mountp, XFS_SBS_FREXTENTS,
					  -((int64_t)rtextents), rsvd);
		if (error) {
			error = XFS_ERROR(ENOSPC);
			goto undo_log;
		}
		tp->t_rtx_res += rtextents;
	}

	return 0;

undo_log:
	if (logspace > 0) {
		int		log_flags;

		if (flags & XFS_TRANS_PERM_LOG_RES) {
			log_flags = XFS_LOG_REL_PERM_RESERV;
		} else {
			log_flags = 0;
		}
		xfs_log_done(tp->t_mountp, tp->t_ticket, NULL, log_flags);
		tp->t_ticket = NULL;
		tp->t_log_res = 0;
		tp->t_flags &= ~XFS_TRANS_PERM_LOG_RES;
	}

undo_blocks:
	if (blocks > 0) {
		xfs_icsb_modify_counters(tp->t_mountp, XFS_SBS_FDBLOCKS,
					 (int64_t)blocks, rsvd);
		tp->t_blk_res = 0;
	}

	current_restore_flags_nested(&tp->t_pflags, PF_FSTRANS);

	return error;
}

void
xfs_trans_mod_sb(
	xfs_trans_t	*tp,
	uint		field,
	int64_t		delta)
{
	uint32_t	flags = (XFS_TRANS_DIRTY|XFS_TRANS_SB_DIRTY);
	xfs_mount_t	*mp = tp->t_mountp;

	switch (field) {
	case XFS_TRANS_SB_ICOUNT:
		tp->t_icount_delta += delta;
		if (xfs_sb_version_haslazysbcount(&mp->m_sb))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_IFREE:
		tp->t_ifree_delta += delta;
		if (xfs_sb_version_haslazysbcount(&mp->m_sb))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_FDBLOCKS:
		if (delta < 0) {
			tp->t_blk_res_used += (uint)-delta;
			ASSERT(tp->t_blk_res_used <= tp->t_blk_res);
		}
		tp->t_fdblocks_delta += delta;
		if (xfs_sb_version_haslazysbcount(&mp->m_sb))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_RES_FDBLOCKS:
		ASSERT(delta < 0);
		tp->t_res_fdblocks_delta += delta;
		if (xfs_sb_version_haslazysbcount(&mp->m_sb))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_FREXTENTS:
		if (delta < 0) {
			tp->t_rtx_res_used += (uint)-delta;
			ASSERT(tp->t_rtx_res_used <= tp->t_rtx_res);
		}
		tp->t_frextents_delta += delta;
		break;
	case XFS_TRANS_SB_RES_FREXTENTS:
		ASSERT(delta < 0);
		tp->t_res_frextents_delta += delta;
		break;
	case XFS_TRANS_SB_DBLOCKS:
		ASSERT(delta > 0);
		tp->t_dblocks_delta += delta;
		break;
	case XFS_TRANS_SB_AGCOUNT:
		ASSERT(delta > 0);
		tp->t_agcount_delta += delta;
		break;
	case XFS_TRANS_SB_IMAXPCT:
		tp->t_imaxpct_delta += delta;
		break;
	case XFS_TRANS_SB_REXTSIZE:
		tp->t_rextsize_delta += delta;
		break;
	case XFS_TRANS_SB_RBMBLOCKS:
		tp->t_rbmblocks_delta += delta;
		break;
	case XFS_TRANS_SB_RBLOCKS:
		tp->t_rblocks_delta += delta;
		break;
	case XFS_TRANS_SB_REXTENTS:
		tp->t_rextents_delta += delta;
		break;
	case XFS_TRANS_SB_REXTSLOG:
		tp->t_rextslog_delta += delta;
		break;
	default:
		ASSERT(0);
		return;
	}

	tp->t_flags |= flags;
}

STATIC void
xfs_trans_apply_sb_deltas(
	xfs_trans_t	*tp)
{
	xfs_dsb_t	*sbp;
	xfs_buf_t	*bp;
	int		whole = 0;

	bp = xfs_trans_getsb(tp, tp->t_mountp, 0);
	sbp = XFS_BUF_TO_SBP(bp);

	ASSERT((tp->t_fdblocks_delta + tp->t_res_fdblocks_delta) ==
	       (tp->t_ag_freeblks_delta + tp->t_ag_flist_delta +
		tp->t_ag_btree_delta));

	if (!xfs_sb_version_haslazysbcount(&(tp->t_mountp->m_sb))) {
		if (tp->t_icount_delta)
			be64_add_cpu(&sbp->sb_icount, tp->t_icount_delta);
		if (tp->t_ifree_delta)
			be64_add_cpu(&sbp->sb_ifree, tp->t_ifree_delta);
		if (tp->t_fdblocks_delta)
			be64_add_cpu(&sbp->sb_fdblocks, tp->t_fdblocks_delta);
		if (tp->t_res_fdblocks_delta)
			be64_add_cpu(&sbp->sb_fdblocks, tp->t_res_fdblocks_delta);
	}

	if (tp->t_frextents_delta)
		be64_add_cpu(&sbp->sb_frextents, tp->t_frextents_delta);
	if (tp->t_res_frextents_delta)
		be64_add_cpu(&sbp->sb_frextents, tp->t_res_frextents_delta);

	if (tp->t_dblocks_delta) {
		be64_add_cpu(&sbp->sb_dblocks, tp->t_dblocks_delta);
		whole = 1;
	}
	if (tp->t_agcount_delta) {
		be32_add_cpu(&sbp->sb_agcount, tp->t_agcount_delta);
		whole = 1;
	}
	if (tp->t_imaxpct_delta) {
		sbp->sb_imax_pct += tp->t_imaxpct_delta;
		whole = 1;
	}
	if (tp->t_rextsize_delta) {
		be32_add_cpu(&sbp->sb_rextsize, tp->t_rextsize_delta);
		whole = 1;
	}
	if (tp->t_rbmblocks_delta) {
		be32_add_cpu(&sbp->sb_rbmblocks, tp->t_rbmblocks_delta);
		whole = 1;
	}
	if (tp->t_rblocks_delta) {
		be64_add_cpu(&sbp->sb_rblocks, tp->t_rblocks_delta);
		whole = 1;
	}
	if (tp->t_rextents_delta) {
		be64_add_cpu(&sbp->sb_rextents, tp->t_rextents_delta);
		whole = 1;
	}
	if (tp->t_rextslog_delta) {
		sbp->sb_rextslog += tp->t_rextslog_delta;
		whole = 1;
	}

	if (whole)
		xfs_trans_log_buf(tp, bp, 0, sizeof(xfs_dsb_t) - 1);
	else
		xfs_trans_log_buf(tp, bp, offsetof(xfs_dsb_t, sb_icount),
				  offsetof(xfs_dsb_t, sb_frextents) +
				  sizeof(sbp->sb_frextents) - 1);
}

void
xfs_trans_unreserve_and_mod_sb(
	xfs_trans_t	*tp)
{
	xfs_mod_sb_t	msb[9];	
	xfs_mod_sb_t	*msbp;
	xfs_mount_t	*mp = tp->t_mountp;
	
	int		error;
	int		rsvd;
	int64_t		blkdelta = 0;
	int64_t		rtxdelta = 0;
	int64_t		idelta = 0;
	int64_t		ifreedelta = 0;

	msbp = msb;
	rsvd = (tp->t_flags & XFS_TRANS_RESERVE) != 0;

	
	if (tp->t_blk_res > 0)
		blkdelta = tp->t_blk_res;
	if ((tp->t_fdblocks_delta != 0) &&
	    (xfs_sb_version_haslazysbcount(&mp->m_sb) ||
	     (tp->t_flags & XFS_TRANS_SB_DIRTY)))
	        blkdelta += tp->t_fdblocks_delta;

	if (tp->t_rtx_res > 0)
		rtxdelta = tp->t_rtx_res;
	if ((tp->t_frextents_delta != 0) &&
	    (tp->t_flags & XFS_TRANS_SB_DIRTY))
		rtxdelta += tp->t_frextents_delta;

	if (xfs_sb_version_haslazysbcount(&mp->m_sb) ||
	     (tp->t_flags & XFS_TRANS_SB_DIRTY)) {
		idelta = tp->t_icount_delta;
		ifreedelta = tp->t_ifree_delta;
	}

	
	if (blkdelta) {
		error = xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS,
						 blkdelta, rsvd);
		if (error)
			goto out;
	}

	if (idelta) {
		error = xfs_icsb_modify_counters(mp, XFS_SBS_ICOUNT,
						 idelta, rsvd);
		if (error)
			goto out_undo_fdblocks;
	}

	if (ifreedelta) {
		error = xfs_icsb_modify_counters(mp, XFS_SBS_IFREE,
						 ifreedelta, rsvd);
		if (error)
			goto out_undo_icount;
	}

	
	if (rtxdelta != 0) {
		msbp->msb_field = XFS_SBS_FREXTENTS;
		msbp->msb_delta = rtxdelta;
		msbp++;
	}

	if (tp->t_flags & XFS_TRANS_SB_DIRTY) {
		if (tp->t_dblocks_delta != 0) {
			msbp->msb_field = XFS_SBS_DBLOCKS;
			msbp->msb_delta = tp->t_dblocks_delta;
			msbp++;
		}
		if (tp->t_agcount_delta != 0) {
			msbp->msb_field = XFS_SBS_AGCOUNT;
			msbp->msb_delta = tp->t_agcount_delta;
			msbp++;
		}
		if (tp->t_imaxpct_delta != 0) {
			msbp->msb_field = XFS_SBS_IMAX_PCT;
			msbp->msb_delta = tp->t_imaxpct_delta;
			msbp++;
		}
		if (tp->t_rextsize_delta != 0) {
			msbp->msb_field = XFS_SBS_REXTSIZE;
			msbp->msb_delta = tp->t_rextsize_delta;
			msbp++;
		}
		if (tp->t_rbmblocks_delta != 0) {
			msbp->msb_field = XFS_SBS_RBMBLOCKS;
			msbp->msb_delta = tp->t_rbmblocks_delta;
			msbp++;
		}
		if (tp->t_rblocks_delta != 0) {
			msbp->msb_field = XFS_SBS_RBLOCKS;
			msbp->msb_delta = tp->t_rblocks_delta;
			msbp++;
		}
		if (tp->t_rextents_delta != 0) {
			msbp->msb_field = XFS_SBS_REXTENTS;
			msbp->msb_delta = tp->t_rextents_delta;
			msbp++;
		}
		if (tp->t_rextslog_delta != 0) {
			msbp->msb_field = XFS_SBS_REXTSLOG;
			msbp->msb_delta = tp->t_rextslog_delta;
			msbp++;
		}
	}

	if (msbp > msb) {
		error = xfs_mod_incore_sb_batch(tp->t_mountp, msb,
			(uint)(msbp - msb), rsvd);
		if (error)
			goto out_undo_ifreecount;
	}

	return;

out_undo_ifreecount:
	if (ifreedelta)
		xfs_icsb_modify_counters(mp, XFS_SBS_IFREE, -ifreedelta, rsvd);
out_undo_icount:
	if (idelta)
		xfs_icsb_modify_counters(mp, XFS_SBS_ICOUNT, -idelta, rsvd);
out_undo_fdblocks:
	if (blkdelta)
		xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS, -blkdelta, rsvd);
out:
	ASSERT(error == 0);
	return;
}

void
xfs_trans_add_item(
	struct xfs_trans	*tp,
	struct xfs_log_item	*lip)
{
	struct xfs_log_item_desc *lidp;

	ASSERT(lip->li_mountp == tp->t_mountp);
	ASSERT(lip->li_ailp == tp->t_mountp->m_ail);

	lidp = kmem_zone_zalloc(xfs_log_item_desc_zone, KM_SLEEP | KM_NOFS);

	lidp->lid_item = lip;
	lidp->lid_flags = 0;
	list_add_tail(&lidp->lid_trans, &tp->t_items);

	lip->li_desc = lidp;
}

STATIC void
xfs_trans_free_item_desc(
	struct xfs_log_item_desc *lidp)
{
	list_del_init(&lidp->lid_trans);
	kmem_zone_free(xfs_log_item_desc_zone, lidp);
}

void
xfs_trans_del_item(
	struct xfs_log_item	*lip)
{
	xfs_trans_free_item_desc(lip->li_desc);
	lip->li_desc = NULL;
}

void
xfs_trans_free_items(
	struct xfs_trans	*tp,
	xfs_lsn_t		commit_lsn,
	int			flags)
{
	struct xfs_log_item_desc *lidp, *next;

	list_for_each_entry_safe(lidp, next, &tp->t_items, lid_trans) {
		struct xfs_log_item	*lip = lidp->lid_item;

		lip->li_desc = NULL;

		if (commit_lsn != NULLCOMMITLSN)
			IOP_COMMITTING(lip, commit_lsn);
		if (flags & XFS_TRANS_ABORT)
			lip->li_flags |= XFS_LI_ABORTED;
		IOP_UNLOCK(lip);

		xfs_trans_free_item_desc(lidp);
	}
}

static inline void
xfs_log_item_batch_insert(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct xfs_log_item	**log_items,
	int			nr_items,
	xfs_lsn_t		commit_lsn)
{
	int	i;

	spin_lock(&ailp->xa_lock);
	
	xfs_trans_ail_update_bulk(ailp, cur, log_items, nr_items, commit_lsn);

	for (i = 0; i < nr_items; i++)
		IOP_UNPIN(log_items[i], 0);
}

void
xfs_trans_committed_bulk(
	struct xfs_ail		*ailp,
	struct xfs_log_vec	*log_vector,
	xfs_lsn_t		commit_lsn,
	int			aborted)
{
#define LOG_ITEM_BATCH_SIZE	32
	struct xfs_log_item	*log_items[LOG_ITEM_BATCH_SIZE];
	struct xfs_log_vec	*lv;
	struct xfs_ail_cursor	cur;
	int			i = 0;

	spin_lock(&ailp->xa_lock);
	xfs_trans_ail_cursor_last(ailp, &cur, commit_lsn);
	spin_unlock(&ailp->xa_lock);

	
	for (lv = log_vector; lv; lv = lv->lv_next ) {
		struct xfs_log_item	*lip = lv->lv_item;
		xfs_lsn_t		item_lsn;

		if (aborted)
			lip->li_flags |= XFS_LI_ABORTED;
		item_lsn = IOP_COMMITTED(lip, commit_lsn);

		
		if (XFS_LSN_CMP(item_lsn, (xfs_lsn_t)-1) == 0)
			continue;

		if (aborted) {
			ASSERT(XFS_FORCED_SHUTDOWN(ailp->xa_mount));
			IOP_UNPIN(lip, 1);
			continue;
		}

		if (item_lsn != commit_lsn) {

			spin_lock(&ailp->xa_lock);
			if (XFS_LSN_CMP(item_lsn, lip->li_lsn) > 0)
				xfs_trans_ail_update(ailp, lip, item_lsn);
			else
				spin_unlock(&ailp->xa_lock);
			IOP_UNPIN(lip, 0);
			continue;
		}

		
		log_items[i++] = lv->lv_item;
		if (i >= LOG_ITEM_BATCH_SIZE) {
			xfs_log_item_batch_insert(ailp, &cur, log_items,
					LOG_ITEM_BATCH_SIZE, commit_lsn);
			i = 0;
		}
	}

	
	if (i)
		xfs_log_item_batch_insert(ailp, &cur, log_items, i, commit_lsn);

	spin_lock(&ailp->xa_lock);
	xfs_trans_ail_cursor_done(ailp, &cur);
	spin_unlock(&ailp->xa_lock);
}

int
xfs_trans_commit(
	struct xfs_trans	*tp,
	uint			flags)
{
	struct xfs_mount	*mp = tp->t_mountp;
	xfs_lsn_t		commit_lsn = -1;
	int			error = 0;
	int			log_flags = 0;
	int			sync = tp->t_flags & XFS_TRANS_SYNC;

	if (flags & XFS_TRANS_RELEASE_LOG_RES) {
		ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
		log_flags = XFS_LOG_REL_PERM_RESERV;
	}

	if (!(tp->t_flags & XFS_TRANS_DIRTY))
		goto out_unreserve;

	if (XFS_FORCED_SHUTDOWN(mp)) {
		error = XFS_ERROR(EIO);
		goto out_unreserve;
	}

	ASSERT(tp->t_ticket != NULL);

	if (tp->t_flags & XFS_TRANS_SB_DIRTY)
		xfs_trans_apply_sb_deltas(tp);
	xfs_trans_apply_dquot_deltas(tp);

	error = xfs_log_commit_cil(mp, tp, &commit_lsn, flags);
	if (error == ENOMEM) {
		xfs_force_shutdown(mp, SHUTDOWN_LOG_IO_ERROR);
		error = XFS_ERROR(EIO);
		goto out_unreserve;
	}

	current_restore_flags_nested(&tp->t_pflags, PF_FSTRANS);
	xfs_trans_free(tp);

	if (sync) {
		if (!error) {
			error = _xfs_log_force_lsn(mp, commit_lsn,
				      XFS_LOG_SYNC, NULL);
		}
		XFS_STATS_INC(xs_trans_sync);
	} else {
		XFS_STATS_INC(xs_trans_async);
	}

	return error;

out_unreserve:
	xfs_trans_unreserve_and_mod_sb(tp);

	xfs_trans_unreserve_and_mod_dquots(tp);
	if (tp->t_ticket) {
		commit_lsn = xfs_log_done(mp, tp->t_ticket, NULL, log_flags);
		if (commit_lsn == -1 && !error)
			error = XFS_ERROR(EIO);
	}
	current_restore_flags_nested(&tp->t_pflags, PF_FSTRANS);
	xfs_trans_free_items(tp, NULLCOMMITLSN, error ? XFS_TRANS_ABORT : 0);
	xfs_trans_free(tp);

	XFS_STATS_INC(xs_trans_empty);
	return error;
}

void
xfs_trans_cancel(
	xfs_trans_t		*tp,
	int			flags)
{
	int			log_flags;
	xfs_mount_t		*mp = tp->t_mountp;

	if ((flags & XFS_TRANS_ABORT) && !(tp->t_flags & XFS_TRANS_DIRTY))
		flags &= ~XFS_TRANS_ABORT;
	if ((tp->t_flags & XFS_TRANS_DIRTY) && !XFS_FORCED_SHUTDOWN(mp)) {
		XFS_ERROR_REPORT("xfs_trans_cancel", XFS_ERRLEVEL_LOW, mp);
		xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
	}
#ifdef DEBUG
	if (!(flags & XFS_TRANS_ABORT) && !XFS_FORCED_SHUTDOWN(mp)) {
		struct xfs_log_item_desc *lidp;

		list_for_each_entry(lidp, &tp->t_items, lid_trans)
			ASSERT(!(lidp->lid_item->li_type == XFS_LI_EFD));
	}
#endif
	xfs_trans_unreserve_and_mod_sb(tp);
	xfs_trans_unreserve_and_mod_dquots(tp);

	if (tp->t_ticket) {
		if (flags & XFS_TRANS_RELEASE_LOG_RES) {
			ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
			log_flags = XFS_LOG_REL_PERM_RESERV;
		} else {
			log_flags = 0;
		}
		xfs_log_done(mp, tp->t_ticket, NULL, log_flags);
	}

	
	current_restore_flags_nested(&tp->t_pflags, PF_FSTRANS);

	xfs_trans_free_items(tp, NULLCOMMITLSN, flags);
	xfs_trans_free(tp);
}

int
xfs_trans_roll(
	struct xfs_trans	**tpp,
	struct xfs_inode	*dp)
{
	struct xfs_trans	*trans;
	unsigned int		logres, count;
	int			error;

	trans = *tpp;
	xfs_trans_log_inode(trans, dp, XFS_ILOG_CORE);

	logres = trans->t_log_res;
	count = trans->t_log_count;
	*tpp = xfs_trans_dup(trans);

	error = xfs_trans_commit(trans, 0);
	if (error)
		return (error);

	trans = *tpp;

	xfs_log_ticket_put(trans->t_ticket);


	error = xfs_trans_reserve(trans, 0, logres, 0,
				  XFS_TRANS_PERM_LOG_RES, count);
	if (error)
		return error;

	xfs_trans_ijoin(trans, dp, 0);
	return 0;
}
