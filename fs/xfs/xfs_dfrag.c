/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
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
#include "xfs_bmap_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_bmap.h"
#include "xfs_itable.h"
#include "xfs_dfrag.h"
#include "xfs_error.h"
#include "xfs_vnodeops.h"
#include "xfs_trace.h"


static int xfs_swap_extents(
	xfs_inode_t	*ip,	
	xfs_inode_t	*tip,	
	xfs_swapext_t	*sxp);

int
xfs_swapext(
	xfs_swapext_t	*sxp)
{
	xfs_inode_t     *ip, *tip;
	struct file	*file, *tmp_file;
	int		error = 0;

	
	file = fget((int)sxp->sx_fdtarget);
	if (!file) {
		error = XFS_ERROR(EINVAL);
		goto out;
	}

	if (!(file->f_mode & FMODE_WRITE) ||
	    !(file->f_mode & FMODE_READ) ||
	    (file->f_flags & O_APPEND)) {
		error = XFS_ERROR(EBADF);
		goto out_put_file;
	}

	tmp_file = fget((int)sxp->sx_fdtmp);
	if (!tmp_file) {
		error = XFS_ERROR(EINVAL);
		goto out_put_file;
	}

	if (!(tmp_file->f_mode & FMODE_WRITE) ||
	    !(tmp_file->f_mode & FMODE_READ) ||
	    (tmp_file->f_flags & O_APPEND)) {
		error = XFS_ERROR(EBADF);
		goto out_put_tmp_file;
	}

	if (IS_SWAPFILE(file->f_path.dentry->d_inode) ||
	    IS_SWAPFILE(tmp_file->f_path.dentry->d_inode)) {
		error = XFS_ERROR(EINVAL);
		goto out_put_tmp_file;
	}

	ip = XFS_I(file->f_path.dentry->d_inode);
	tip = XFS_I(tmp_file->f_path.dentry->d_inode);

	if (ip->i_mount != tip->i_mount) {
		error = XFS_ERROR(EINVAL);
		goto out_put_tmp_file;
	}

	if (ip->i_ino == tip->i_ino) {
		error = XFS_ERROR(EINVAL);
		goto out_put_tmp_file;
	}

	if (XFS_FORCED_SHUTDOWN(ip->i_mount)) {
		error = XFS_ERROR(EIO);
		goto out_put_tmp_file;
	}

	error = xfs_swap_extents(ip, tip, sxp);

 out_put_tmp_file:
	fput(tmp_file);
 out_put_file:
	fput(file);
 out:
	return error;
}

static int
xfs_swap_extents_check_format(
	xfs_inode_t	*ip,	
	xfs_inode_t	*tip)	
{

	
	if (ip->i_d.di_format == XFS_DINODE_FMT_LOCAL ||
	    tip->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		return EINVAL;

	if (ip->i_d.di_nextents < tip->i_d.di_nextents)
		return EINVAL;

	if (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
	    tip->i_d.di_format == XFS_DINODE_FMT_BTREE)
		return EINVAL;

	
	if (tip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_NEXTENTS(tip, XFS_DATA_FORK) >
			XFS_IFORK_MAXEXT(ip, XFS_DATA_FORK))
		return EINVAL;

	
	if (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_NEXTENTS(ip, XFS_DATA_FORK) >
			XFS_IFORK_MAXEXT(tip, XFS_DATA_FORK))
		return EINVAL;

	if (tip->i_d.di_format == XFS_DINODE_FMT_BTREE) {
		if (XFS_IFORK_BOFF(ip) &&
		    tip->i_df.if_broot_bytes > XFS_IFORK_BOFF(ip))
			return EINVAL;
		if (XFS_IFORK_NEXTENTS(tip, XFS_DATA_FORK) <=
		    XFS_IFORK_MAXEXT(ip, XFS_DATA_FORK))
			return EINVAL;
	}

	
	if (ip->i_d.di_format == XFS_DINODE_FMT_BTREE) {
		if (XFS_IFORK_BOFF(tip) &&
		    ip->i_df.if_broot_bytes > XFS_IFORK_BOFF(tip))
			return EINVAL;

		if (XFS_IFORK_NEXTENTS(ip, XFS_DATA_FORK) <=
		    XFS_IFORK_MAXEXT(tip, XFS_DATA_FORK))
			return EINVAL;
	}

	return 0;
}

static int
xfs_swap_extents(
	xfs_inode_t	*ip,	
	xfs_inode_t	*tip,	
	xfs_swapext_t	*sxp)
{
	xfs_mount_t	*mp = ip->i_mount;
	xfs_trans_t	*tp;
	xfs_bstat_t	*sbp = &sxp->sx_stat;
	xfs_ifork_t	*tempifp, *ifp, *tifp;
	int		src_log_flags, target_log_flags;
	int		error = 0;
	int		aforkblks = 0;
	int		taforkblks = 0;
	__uint64_t	tmp;

	tempifp = kmem_alloc(sizeof(xfs_ifork_t), KM_MAYFAIL);
	if (!tempifp) {
		error = XFS_ERROR(ENOMEM);
		goto out;
	}

	xfs_lock_two_inodes(ip, tip, XFS_IOLOCK_EXCL);
	xfs_lock_two_inodes(ip, tip, XFS_ILOCK_EXCL);

	
	if ((ip->i_d.di_mode & S_IFMT) != (tip->i_d.di_mode & S_IFMT)) {
		error = XFS_ERROR(EINVAL);
		goto out_unlock;
	}

	
	if (XFS_IS_REALTIME_INODE(ip) != XFS_IS_REALTIME_INODE(tip)) {
		error = XFS_ERROR(EINVAL);
		goto out_unlock;
	}

	if (VN_CACHED(VFS_I(tip)) != 0) {
		error = xfs_flushinval_pages(tip, 0, -1,
				FI_REMAPF_LOCKED);
		if (error)
			goto out_unlock;
	}

	
	if (VN_CACHED(VFS_I(tip)) != 0) {
		error = XFS_ERROR(EINVAL);
		goto out_unlock;
	}

	
	if (sxp->sx_offset != 0 ||
	    sxp->sx_length != ip->i_d.di_size ||
	    sxp->sx_length != tip->i_d.di_size) {
		error = XFS_ERROR(EFAULT);
		goto out_unlock;
	}

	trace_xfs_swap_extent_before(ip, 0);
	trace_xfs_swap_extent_before(tip, 1);

	
	error = xfs_swap_extents_check_format(ip, tip);
	if (error) {
		xfs_notice(mp,
		    "%s: inode 0x%llx format is incompatible for exchanging.",
				__func__, ip->i_ino);
		goto out_unlock;
	}

	if ((sbp->bs_ctime.tv_sec != VFS_I(ip)->i_ctime.tv_sec) ||
	    (sbp->bs_ctime.tv_nsec != VFS_I(ip)->i_ctime.tv_nsec) ||
	    (sbp->bs_mtime.tv_sec != VFS_I(ip)->i_mtime.tv_sec) ||
	    (sbp->bs_mtime.tv_nsec != VFS_I(ip)->i_mtime.tv_nsec)) {
		error = XFS_ERROR(EBUSY);
		goto out_unlock;
	}

	if (VN_MAPPED(VFS_I(ip))) {
		error = XFS_ERROR(EBUSY);
		goto out_unlock;
	}

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	xfs_iunlock(tip, XFS_ILOCK_EXCL);


	xfs_tosspages(ip, 0, -1, FI_REMAPF);

	tp = xfs_trans_alloc(mp, XFS_TRANS_SWAPEXT);
	if ((error = xfs_trans_reserve(tp, 0,
				     XFS_ICHANGE_LOG_RES(mp), 0,
				     0, 0))) {
		xfs_iunlock(ip,  XFS_IOLOCK_EXCL);
		xfs_iunlock(tip, XFS_IOLOCK_EXCL);
		xfs_trans_cancel(tp, 0);
		goto out;
	}
	xfs_lock_two_inodes(ip, tip, XFS_ILOCK_EXCL);

	if ( ((XFS_IFORK_Q(ip) != 0) && (ip->i_d.di_anextents > 0)) &&
	     (ip->i_d.di_aformat != XFS_DINODE_FMT_LOCAL)) {
		error = xfs_bmap_count_blocks(tp, ip, XFS_ATTR_FORK, &aforkblks);
		if (error)
			goto out_trans_cancel;
	}
	if ( ((XFS_IFORK_Q(tip) != 0) && (tip->i_d.di_anextents > 0)) &&
	     (tip->i_d.di_aformat != XFS_DINODE_FMT_LOCAL)) {
		error = xfs_bmap_count_blocks(tp, tip, XFS_ATTR_FORK,
			&taforkblks);
		if (error)
			goto out_trans_cancel;
	}

	ifp = &ip->i_df;
	tifp = &tip->i_df;
	*tempifp = *ifp;	
	*ifp = *tifp;		
	*tifp = *tempifp;	

	tmp = (__uint64_t)ip->i_d.di_nblocks;
	ip->i_d.di_nblocks = tip->i_d.di_nblocks - taforkblks + aforkblks;
	tip->i_d.di_nblocks = tmp + taforkblks - aforkblks;

	tmp = (__uint64_t) ip->i_d.di_nextents;
	ip->i_d.di_nextents = tip->i_d.di_nextents;
	tip->i_d.di_nextents = tmp;

	tmp = (__uint64_t) ip->i_d.di_format;
	ip->i_d.di_format = tip->i_d.di_format;
	tip->i_d.di_format = tmp;

	ASSERT(tip->i_delayed_blks == 0);
	tip->i_delayed_blks = ip->i_delayed_blks;
	ip->i_delayed_blks = 0;

	src_log_flags = XFS_ILOG_CORE;
	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if (ip->i_d.di_nextents <= XFS_INLINE_EXTS) {
			ifp->if_u1.if_extents =
				ifp->if_u2.if_inline_ext;
		}
		src_log_flags |= XFS_ILOG_DEXT;
		break;
	case XFS_DINODE_FMT_BTREE:
		src_log_flags |= XFS_ILOG_DBROOT;
		break;
	}

	target_log_flags = XFS_ILOG_CORE;
	switch (tip->i_d.di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if (tip->i_d.di_nextents <= XFS_INLINE_EXTS) {
			tifp->if_u1.if_extents =
				tifp->if_u2.if_inline_ext;
		}
		target_log_flags |= XFS_ILOG_DEXT;
		break;
	case XFS_DINODE_FMT_BTREE:
		target_log_flags |= XFS_ILOG_DBROOT;
		break;
	}


	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_trans_ijoin(tp, tip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);

	xfs_trans_log_inode(tp, ip,  src_log_flags);
	xfs_trans_log_inode(tp, tip, target_log_flags);

	if (mp->m_flags & XFS_MOUNT_WSYNC)
		xfs_trans_set_sync(tp);

	error = xfs_trans_commit(tp, 0);

	trace_xfs_swap_extent_after(ip, 0);
	trace_xfs_swap_extent_after(tip, 1);
out:
	kmem_free(tempifp);
	return error;

out_unlock:
	xfs_iunlock(ip,  XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_iunlock(tip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	goto out;

out_trans_cancel:
	xfs_trans_cancel(tp, 0);
	goto out_unlock;
}
