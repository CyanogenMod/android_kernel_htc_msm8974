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
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_itable.h"
#include "xfs_utils.h"


int
xfs_dir_ialloc(
	xfs_trans_t	**tpp,		
	xfs_inode_t	*dp,		
	umode_t		mode,
	xfs_nlink_t	nlink,
	xfs_dev_t	rdev,
	prid_t		prid,		
	int		okalloc,	
	xfs_inode_t	**ipp,		
	int		*committed)

{
	xfs_trans_t	*tp;
	xfs_trans_t	*ntp;
	xfs_inode_t	*ip;
	xfs_buf_t	*ialloc_context = NULL;
	boolean_t	call_again = B_FALSE;
	int		code;
	uint		log_res;
	uint		log_count;
	void		*dqinfo;
	uint		tflags;

	tp = *tpp;
	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);

	code = xfs_ialloc(tp, dp, mode, nlink, rdev, prid, okalloc,
			  &ialloc_context, &call_again, &ip);

	if (code) {
		*ipp = NULL;
		return code;
	}
	if (!call_again && (ip == NULL)) {
		*ipp = NULL;
		return XFS_ERROR(ENOSPC);
	}

	if (call_again) {

		xfs_trans_bhold(tp, ialloc_context);
		log_res = xfs_trans_get_log_res(tp);
		log_count = xfs_trans_get_log_count(tp);

		dqinfo = NULL;
		tflags = 0;
		if (tp->t_dqinfo) {
			dqinfo = (void *)tp->t_dqinfo;
			tp->t_dqinfo = NULL;
			tflags = tp->t_flags & XFS_TRANS_DQ_DIRTY;
			tp->t_flags &= ~(XFS_TRANS_DQ_DIRTY);
		}

		ntp = xfs_trans_dup(tp);
		code = xfs_trans_commit(tp, 0);
		tp = ntp;
		if (committed != NULL) {
			*committed = 1;
		}
		if (code) {
			xfs_buf_relse(ialloc_context);
			if (dqinfo) {
				tp->t_dqinfo = dqinfo;
				xfs_trans_free_dqinfo(tp);
			}
			*tpp = ntp;
			*ipp = NULL;
			return code;
		}

		xfs_log_ticket_put(tp->t_ticket);
		code = xfs_trans_reserve(tp, 0, log_res, 0,
					 XFS_TRANS_PERM_LOG_RES, log_count);
		if (dqinfo) {
			tp->t_dqinfo = dqinfo;
			tp->t_flags |= tflags;
		}

		if (code) {
			xfs_buf_relse(ialloc_context);
			*tpp = ntp;
			*ipp = NULL;
			return code;
		}
		xfs_trans_bjoin(tp, ialloc_context);

		code = xfs_ialloc(tp, dp, mode, nlink, rdev, prid,
				  okalloc, &ialloc_context, &call_again, &ip);

		if (code) {
			*tpp = tp;
			*ipp = NULL;
			return code;
		}
		ASSERT ((!call_again) && (ip != NULL));

	} else {
		if (committed != NULL) {
			*committed = 0;
		}
	}

	*ipp = ip;
	*tpp = tp;

	return 0;
}

int				
xfs_droplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	int	error;

	xfs_trans_ichgtime(tp, ip, XFS_ICHGTIME_CHG);

	ASSERT (ip->i_d.di_nlink > 0);
	ip->i_d.di_nlink--;
	drop_nlink(VFS_I(ip));
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = 0;
	if (ip->i_d.di_nlink == 0) {
		error = xfs_iunlink(tp, ip);
	}
	return error;
}

void
xfs_bump_ino_vers2(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(ip->i_d.di_version == 1);

	ip->i_d.di_version = 2;
	ip->i_d.di_onlink = 0;
	memset(&(ip->i_d.di_pad[0]), 0, sizeof(ip->i_d.di_pad));
	mp = tp->t_mountp;
	if (!xfs_sb_version_hasnlink(&mp->m_sb)) {
		spin_lock(&mp->m_sb_lock);
		if (!xfs_sb_version_hasnlink(&mp->m_sb)) {
			xfs_sb_version_addnlink(&mp->m_sb);
			spin_unlock(&mp->m_sb_lock);
			xfs_mod_sb(tp, XFS_SB_VERSIONNUM);
		} else {
			spin_unlock(&mp->m_sb_lock);
		}
	}
	
}

int
xfs_bumplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	xfs_trans_ichgtime(tp, ip, XFS_ICHGTIME_CHG);

	ASSERT(ip->i_d.di_nlink > 0);
	ip->i_d.di_nlink++;
	inc_nlink(VFS_I(ip));
	if ((ip->i_d.di_version == 1) &&
	    (ip->i_d.di_nlink > XFS_MAXLINK_1)) {
		xfs_bump_ino_vers2(tp, ip);
	}

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	return 0;
}
