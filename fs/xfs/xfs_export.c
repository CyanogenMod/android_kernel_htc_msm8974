/*
 * Copyright (c) 2004-2005 Silicon Graphics, Inc.
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
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_export.h"
#include "xfs_vnodeops.h"
#include "xfs_bmap_btree.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_trace.h"

static int xfs_fileid_length(int fileid_type)
{
	switch (fileid_type) {
	case FILEID_INO32_GEN:
		return 2;
	case FILEID_INO32_GEN_PARENT:
		return 4;
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		return 3;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		return 6;
	}
	return 255; 
}

STATIC int
xfs_fs_encode_fh(
	struct dentry		*dentry,
	__u32			*fh,
	int			*max_len,
	int			connectable)
{
	struct fid		*fid = (struct fid *)fh;
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fh;
	struct inode		*inode = dentry->d_inode;
	int			fileid_type;
	int			len;

	
	if (S_ISDIR(inode->i_mode) || !connectable)
		fileid_type = FILEID_INO32_GEN;
	else
		fileid_type = FILEID_INO32_GEN_PARENT;

	if (!(XFS_M(inode->i_sb)->m_flags & XFS_MOUNT_SMALL_INUMS) ||
	    (XFS_M(inode->i_sb)->m_flags & XFS_MOUNT_32BITINODES))
		fileid_type |= XFS_FILEID_TYPE_64FLAG;

	len = xfs_fileid_length(fileid_type);
	if (*max_len < len) {
		*max_len = len;
		return 255;
	}
	*max_len = len;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
		spin_lock(&dentry->d_lock);
		fid->i32.parent_ino = XFS_I(dentry->d_parent->d_inode)->i_ino;
		fid->i32.parent_gen = dentry->d_parent->d_inode->i_generation;
		spin_unlock(&dentry->d_lock);
		
	case FILEID_INO32_GEN:
		fid->i32.ino = XFS_I(inode)->i_ino;
		fid->i32.gen = inode->i_generation;
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		spin_lock(&dentry->d_lock);
		fid64->parent_ino = XFS_I(dentry->d_parent->d_inode)->i_ino;
		fid64->parent_gen = dentry->d_parent->d_inode->i_generation;
		spin_unlock(&dentry->d_lock);
		
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		fid64->ino = XFS_I(inode)->i_ino;
		fid64->gen = inode->i_generation;
		break;
	}

	return fileid_type;
}

STATIC struct inode *
xfs_nfs_get_inode(
	struct super_block	*sb,
	u64			ino,
	u32			generation)
 {
 	xfs_mount_t		*mp = XFS_M(sb);
	xfs_inode_t		*ip;
	int			error;

	if (ino == 0)
		return ERR_PTR(-ESTALE);

	error = xfs_iget(mp, NULL, ino, XFS_IGET_UNTRUSTED, 0, &ip);
	if (error) {
		if (error == EINVAL || error == ENOENT)
			error = ESTALE;
		return ERR_PTR(-error);
	}

	if (ip->i_d.di_gen != generation) {
		IRELE(ip);
		return ERR_PTR(-ESTALE);
	}

	return VFS_I(ip);
}

STATIC struct dentry *
xfs_fs_fh_to_dentry(struct super_block *sb, struct fid *fid,
		 int fh_len, int fileid_type)
{
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fid;
	struct inode		*inode = NULL;

	if (fh_len < xfs_fileid_length(fileid_type))
		return NULL;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
	case FILEID_INO32_GEN:
		inode = xfs_nfs_get_inode(sb, fid->i32.ino, fid->i32.gen);
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		inode = xfs_nfs_get_inode(sb, fid64->ino, fid64->gen);
		break;
	}

	return d_obtain_alias(inode);
}

STATIC struct dentry *
xfs_fs_fh_to_parent(struct super_block *sb, struct fid *fid,
		 int fh_len, int fileid_type)
{
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fid;
	struct inode		*inode = NULL;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
		inode = xfs_nfs_get_inode(sb, fid->i32.parent_ino,
					      fid->i32.parent_gen);
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		inode = xfs_nfs_get_inode(sb, fid64->parent_ino,
					      fid64->parent_gen);
		break;
	}

	return d_obtain_alias(inode);
}

STATIC struct dentry *
xfs_fs_get_parent(
	struct dentry		*child)
{
	int			error;
	struct xfs_inode	*cip;

	error = xfs_lookup(XFS_I(child->d_inode), &xfs_name_dotdot, &cip, NULL);
	if (unlikely(error))
		return ERR_PTR(-error);

	return d_obtain_alias(VFS_I(cip));
}

STATIC int
xfs_fs_nfs_commit_metadata(
	struct inode		*inode)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	xfs_lsn_t		lsn = 0;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	if (xfs_ipincount(ip))
		lsn = ip->i_itemp->ili_last_lsn;
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	if (!lsn)
		return 0;
	return _xfs_log_force_lsn(mp, lsn, XFS_LOG_SYNC, NULL);
}

const struct export_operations xfs_export_operations = {
	.encode_fh		= xfs_fs_encode_fh,
	.fh_to_dentry		= xfs_fs_fh_to_dentry,
	.fh_to_parent		= xfs_fs_fh_to_parent,
	.get_parent		= xfs_fs_get_parent,
	.commit_metadata	= xfs_fs_nfs_commit_metadata,
};
