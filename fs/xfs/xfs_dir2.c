/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
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
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_bmap.h"
#include "xfs_dir2.h"
#include "xfs_dir2_format.h"
#include "xfs_dir2_priv.h"
#include "xfs_error.h"
#include "xfs_vnodeops.h"
#include "xfs_trace.h"

struct xfs_name xfs_name_dotdot = { (unsigned char *)"..", 2};

STATIC xfs_dahash_t
xfs_ascii_ci_hashname(
	struct xfs_name	*name)
{
	xfs_dahash_t	hash;
	int		i;

	for (i = 0, hash = 0; i < name->len; i++)
		hash = tolower(name->name[i]) ^ rol32(hash, 7);

	return hash;
}

STATIC enum xfs_dacmp
xfs_ascii_ci_compname(
	struct xfs_da_args *args,
	const unsigned char *name,
	int		len)
{
	enum xfs_dacmp	result;
	int		i;

	if (args->namelen != len)
		return XFS_CMP_DIFFERENT;

	result = XFS_CMP_EXACT;
	for (i = 0; i < len; i++) {
		if (args->name[i] == name[i])
			continue;
		if (tolower(args->name[i]) != tolower(name[i]))
			return XFS_CMP_DIFFERENT;
		result = XFS_CMP_CASE;
	}

	return result;
}

static struct xfs_nameops xfs_ascii_ci_nameops = {
	.hashname	= xfs_ascii_ci_hashname,
	.compname	= xfs_ascii_ci_compname,
};

void
xfs_dir_mount(
	xfs_mount_t	*mp)
{
	ASSERT(xfs_sb_version_hasdirv2(&mp->m_sb));
	ASSERT((1 << (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog)) <=
	       XFS_MAX_BLOCKSIZE);
	mp->m_dirblksize = 1 << (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog);
	mp->m_dirblkfsbs = 1 << mp->m_sb.sb_dirblklog;
	mp->m_dirdatablk = xfs_dir2_db_to_da(mp, XFS_DIR2_DATA_FIRSTDB(mp));
	mp->m_dirleafblk = xfs_dir2_db_to_da(mp, XFS_DIR2_LEAF_FIRSTDB(mp));
	mp->m_dirfreeblk = xfs_dir2_db_to_da(mp, XFS_DIR2_FREE_FIRSTDB(mp));
	mp->m_attr_node_ents =
		(mp->m_sb.sb_blocksize - (uint)sizeof(xfs_da_node_hdr_t)) /
		(uint)sizeof(xfs_da_node_entry_t);
	mp->m_dir_node_ents =
		(mp->m_dirblksize - (uint)sizeof(xfs_da_node_hdr_t)) /
		(uint)sizeof(xfs_da_node_entry_t);
	mp->m_dir_magicpct = (mp->m_dirblksize * 37) / 100;
	if (xfs_sb_version_hasasciici(&mp->m_sb))
		mp->m_dirnameops = &xfs_ascii_ci_nameops;
	else
		mp->m_dirnameops = &xfs_default_nameops;
}

int
xfs_dir_isempty(
	xfs_inode_t	*dp)
{
	xfs_dir2_sf_hdr_t	*sfp;

	ASSERT(S_ISDIR(dp->i_d.di_mode));
	if (dp->i_d.di_size == 0)	
		return 1;
	if (dp->i_d.di_size > XFS_IFORK_DSIZE(dp))
		return 0;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	return !sfp->count;
}

int
xfs_dir_ino_validate(
	xfs_mount_t	*mp,
	xfs_ino_t	ino)
{
	xfs_agblock_t	agblkno;
	xfs_agino_t	agino;
	xfs_agnumber_t	agno;
	int		ino_ok;
	int		ioff;

	agno = XFS_INO_TO_AGNO(mp, ino);
	agblkno = XFS_INO_TO_AGBNO(mp, ino);
	ioff = XFS_INO_TO_OFFSET(mp, ino);
	agino = XFS_OFFBNO_TO_AGINO(mp, agblkno, ioff);
	ino_ok =
		agno < mp->m_sb.sb_agcount &&
		agblkno < mp->m_sb.sb_agblocks &&
		agblkno != 0 &&
		ioff < (1 << mp->m_sb.sb_inopblog) &&
		XFS_AGINO_TO_INO(mp, agno, agino) == ino;
	if (unlikely(XFS_TEST_ERROR(!ino_ok, mp, XFS_ERRTAG_DIR_INO_VALIDATE,
			XFS_RANDOM_DIR_INO_VALIDATE))) {
		xfs_warn(mp, "Invalid inode number 0x%Lx",
				(unsigned long long) ino);
		XFS_ERROR_REPORT("xfs_dir_ino_validate", XFS_ERRLEVEL_LOW, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

int
xfs_dir_init(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	xfs_inode_t	*pdp)
{
	xfs_da_args_t	args;
	int		error;

	memset((char *)&args, 0, sizeof(args));
	args.dp = dp;
	args.trans = tp;
	ASSERT(S_ISDIR(dp->i_d.di_mode));
	if ((error = xfs_dir_ino_validate(tp->t_mountp, pdp->i_ino)))
		return error;
	return xfs_dir2_sf_create(&args, pdp->i_ino);
}

int
xfs_dir_createname(
	xfs_trans_t		*tp,
	xfs_inode_t		*dp,
	struct xfs_name		*name,
	xfs_ino_t		inum,		
	xfs_fsblock_t		*first,		
	xfs_bmap_free_t		*flist,		
	xfs_extlen_t		total)		
{
	xfs_da_args_t		args;
	int			rval;
	int			v;		

	ASSERT(S_ISDIR(dp->i_d.di_mode));
	if ((rval = xfs_dir_ino_validate(tp->t_mountp, inum)))
		return rval;
	XFS_STATS_INC(xs_dir_create);

	memset(&args, 0, sizeof(xfs_da_args_t));
	args.name = name->name;
	args.namelen = name->len;
	args.hashval = dp->i_mount->m_dirnameops->hashname(name);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.op_flags = XFS_DA_OP_ADDNAME | XFS_DA_OP_OKNOENT;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_addname(&args);
	else if ((rval = xfs_dir2_isblock(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_block_addname(&args);
	else if ((rval = xfs_dir2_isleaf(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_addname(&args);
	else
		rval = xfs_dir2_node_addname(&args);
	return rval;
}

int
xfs_dir_cilookup_result(
	struct xfs_da_args *args,
	const unsigned char *name,
	int		len)
{
	if (args->cmpresult == XFS_CMP_DIFFERENT)
		return ENOENT;
	if (args->cmpresult != XFS_CMP_CASE ||
					!(args->op_flags & XFS_DA_OP_CILOOKUP))
		return EEXIST;

	args->value = kmem_alloc(len, KM_NOFS | KM_MAYFAIL);
	if (!args->value)
		return ENOMEM;

	memcpy(args->value, name, len);
	args->valuelen = len;
	return EEXIST;
}


int
xfs_dir_lookup(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	struct xfs_name	*name,
	xfs_ino_t	*inum,		
	struct xfs_name *ci_name)	
{
	xfs_da_args_t	args;
	int		rval;
	int		v;		

	ASSERT(S_ISDIR(dp->i_d.di_mode));
	XFS_STATS_INC(xs_dir_lookup);

	memset(&args, 0, sizeof(xfs_da_args_t));
	args.name = name->name;
	args.namelen = name->len;
	args.hashval = dp->i_mount->m_dirnameops->hashname(name);
	args.dp = dp;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.op_flags = XFS_DA_OP_OKNOENT;
	if (ci_name)
		args.op_flags |= XFS_DA_OP_CILOOKUP;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_lookup(&args);
	else if ((rval = xfs_dir2_isblock(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_block_lookup(&args);
	else if ((rval = xfs_dir2_isleaf(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_lookup(&args);
	else
		rval = xfs_dir2_node_lookup(&args);
	if (rval == EEXIST)
		rval = 0;
	if (!rval) {
		*inum = args.inumber;
		if (ci_name) {
			ci_name->name = args.value;
			ci_name->len = args.valuelen;
		}
	}
	return rval;
}

int
xfs_dir_removename(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	struct xfs_name	*name,
	xfs_ino_t	ino,
	xfs_fsblock_t	*first,		
	xfs_bmap_free_t	*flist,		
	xfs_extlen_t	total)		
{
	xfs_da_args_t	args;
	int		rval;
	int		v;		

	ASSERT(S_ISDIR(dp->i_d.di_mode));
	XFS_STATS_INC(xs_dir_remove);

	memset(&args, 0, sizeof(xfs_da_args_t));
	args.name = name->name;
	args.namelen = name->len;
	args.hashval = dp->i_mount->m_dirnameops->hashname(name);
	args.inumber = ino;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_removename(&args);
	else if ((rval = xfs_dir2_isblock(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_block_removename(&args);
	else if ((rval = xfs_dir2_isleaf(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_removename(&args);
	else
		rval = xfs_dir2_node_removename(&args);
	return rval;
}

int
xfs_readdir(
	xfs_inode_t	*dp,
	void		*dirent,
	size_t		bufsize,
	xfs_off_t	*offset,
	filldir_t	filldir)
{
	int		rval;		
	int		v;		

	trace_xfs_readdir(dp);

	if (XFS_FORCED_SHUTDOWN(dp->i_mount))
		return XFS_ERROR(EIO);

	ASSERT(S_ISDIR(dp->i_d.di_mode));
	XFS_STATS_INC(xs_dir_getdents);

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_getdents(dp, dirent, offset, filldir);
	else if ((rval = xfs_dir2_isblock(NULL, dp, &v)))
		;
	else if (v)
		rval = xfs_dir2_block_getdents(dp, dirent, offset, filldir);
	else
		rval = xfs_dir2_leaf_getdents(dp, dirent, bufsize, offset,
					      filldir);
	return rval;
}

int
xfs_dir_replace(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	struct xfs_name	*name,		
	xfs_ino_t	inum,		
	xfs_fsblock_t	*first,		
	xfs_bmap_free_t	*flist,		
	xfs_extlen_t	total)		
{
	xfs_da_args_t	args;
	int		rval;
	int		v;		

	ASSERT(S_ISDIR(dp->i_d.di_mode));

	if ((rval = xfs_dir_ino_validate(tp->t_mountp, inum)))
		return rval;

	memset(&args, 0, sizeof(xfs_da_args_t));
	args.name = name->name;
	args.namelen = name->len;
	args.hashval = dp->i_mount->m_dirnameops->hashname(name);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_replace(&args);
	else if ((rval = xfs_dir2_isblock(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_block_replace(&args);
	else if ((rval = xfs_dir2_isleaf(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_replace(&args);
	else
		rval = xfs_dir2_node_replace(&args);
	return rval;
}

int
xfs_dir_canenter(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	struct xfs_name	*name,		
	uint		resblks)
{
	xfs_da_args_t	args;
	int		rval;
	int		v;		

	if (resblks)
		return 0;

	ASSERT(S_ISDIR(dp->i_d.di_mode));

	memset(&args, 0, sizeof(xfs_da_args_t));
	args.name = name->name;
	args.namelen = name->len;
	args.hashval = dp->i_mount->m_dirnameops->hashname(name);
	args.dp = dp;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.op_flags = XFS_DA_OP_JUSTCHECK | XFS_DA_OP_ADDNAME |
							XFS_DA_OP_OKNOENT;

	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_addname(&args);
	else if ((rval = xfs_dir2_isblock(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_block_addname(&args);
	else if ((rval = xfs_dir2_isleaf(tp, dp, &v)))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_addname(&args);
	else
		rval = xfs_dir2_node_addname(&args);
	return rval;
}


int
xfs_dir2_grow_inode(
	struct xfs_da_args	*args,
	int			space,	
	xfs_dir2_db_t		*dbp)	
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	xfs_fileoff_t		bno;	
	int			count;	
	int			error;

	trace_xfs_dir2_grow_inode(args, space);

	bno = XFS_B_TO_FSBT(mp, space * XFS_DIR2_SPACE_SIZE);
	count = mp->m_dirblkfsbs;

	error = xfs_da_grow_inode_int(args, &bno, count);
	if (error)
		return error;

	*dbp = xfs_dir2_da_to_db(mp, (xfs_dablk_t)bno);

	if (space == XFS_DIR2_DATA_SPACE) {
		xfs_fsize_t	size;		

		size = XFS_FSB_TO_B(mp, bno + count);
		if (size > dp->i_d.di_size) {
			dp->i_d.di_size = size;
			xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE);
		}
	}
	return 0;
}

int
xfs_dir2_isblock(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	int		*vp)		
{
	xfs_fileoff_t	last;		
	xfs_mount_t	*mp;
	int		rval;

	mp = dp->i_mount;
	if ((rval = xfs_bmap_last_offset(tp, dp, &last, XFS_DATA_FORK)))
		return rval;
	rval = XFS_FSB_TO_B(mp, last) == mp->m_dirblksize;
	ASSERT(rval == 0 || dp->i_d.di_size == mp->m_dirblksize);
	*vp = rval;
	return 0;
}

int
xfs_dir2_isleaf(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	int		*vp)		
{
	xfs_fileoff_t	last;		
	xfs_mount_t	*mp;
	int		rval;

	mp = dp->i_mount;
	if ((rval = xfs_bmap_last_offset(tp, dp, &last, XFS_DATA_FORK)))
		return rval;
	*vp = last == mp->m_dirleafblk + (1 << mp->m_sb.sb_dirblklog);
	return 0;
}

int
xfs_dir2_shrink_inode(
	xfs_da_args_t	*args,
	xfs_dir2_db_t	db,
	xfs_dabuf_t	*bp)
{
	xfs_fileoff_t	bno;		
	xfs_dablk_t	da;		
	int		done;		
	xfs_inode_t	*dp;
	int		error;
	xfs_mount_t	*mp;
	xfs_trans_t	*tp;

	trace_xfs_dir2_shrink_inode(args, db);

	dp = args->dp;
	mp = dp->i_mount;
	tp = args->trans;
	da = xfs_dir2_db_to_da(mp, db);
	if ((error = xfs_bunmapi(tp, dp, da, mp->m_dirblkfsbs,
			XFS_BMAPI_METADATA, 0, args->firstblock, args->flist,
			&done))) {
		return error;
	}
	ASSERT(done);
	xfs_da_binval(tp, bp);
	if (db >= XFS_DIR2_LEAF_FIRSTDB(mp))
		return 0;
	if (dp->i_d.di_size > xfs_dir2_db_off_to_byte(mp, db + 1, 0))
		return 0;
	bno = da;
	if ((error = xfs_bmap_last_before(tp, dp, &bno, XFS_DATA_FORK))) {
		return error;
	}
	if (db == mp->m_dirdatablk)
		ASSERT(bno == 0);
	else
		ASSERT(bno > 0);
	dp->i_d.di_size = XFS_FSB_TO_B(mp, bno);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
	return 0;
}
