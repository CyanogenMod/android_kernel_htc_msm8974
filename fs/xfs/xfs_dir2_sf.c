/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
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
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_error.h"
#include "xfs_dir2.h"
#include "xfs_dir2_format.h"
#include "xfs_dir2_priv.h"
#include "xfs_trace.h"

static void xfs_dir2_sf_addname_easy(xfs_da_args_t *args,
				     xfs_dir2_sf_entry_t *sfep,
				     xfs_dir2_data_aoff_t offset,
				     int new_isize);
static void xfs_dir2_sf_addname_hard(xfs_da_args_t *args, int objchange,
				     int new_isize);
static int xfs_dir2_sf_addname_pick(xfs_da_args_t *args, int objchange,
				    xfs_dir2_sf_entry_t **sfepp,
				    xfs_dir2_data_aoff_t *offsetp);
#ifdef DEBUG
static void xfs_dir2_sf_check(xfs_da_args_t *args);
#else
#define	xfs_dir2_sf_check(args)
#endif 
#if XFS_BIG_INUMS
static void xfs_dir2_sf_toino4(xfs_da_args_t *args);
static void xfs_dir2_sf_toino8(xfs_da_args_t *args);
#endif 

static xfs_ino_t
xfs_dir2_sf_get_ino(
	struct xfs_dir2_sf_hdr	*hdr,
	xfs_dir2_inou_t		*from)
{
	if (hdr->i8count)
		return get_unaligned_be64(&from->i8.i) & 0x00ffffffffffffffULL;
	else
		return get_unaligned_be32(&from->i4.i);
}

static void
xfs_dir2_sf_put_ino(
	struct xfs_dir2_sf_hdr	*hdr,
	xfs_dir2_inou_t		*to,
	xfs_ino_t		ino)
{
	ASSERT((ino & 0xff00000000000000ULL) == 0);

	if (hdr->i8count)
		put_unaligned_be64(ino, &to->i8.i);
	else
		put_unaligned_be32(ino, &to->i4.i);
}

xfs_ino_t
xfs_dir2_sf_get_parent_ino(
	struct xfs_dir2_sf_hdr	*hdr)
{
	return xfs_dir2_sf_get_ino(hdr, &hdr->parent);
}

static void
xfs_dir2_sf_put_parent_ino(
	struct xfs_dir2_sf_hdr	*hdr,
	xfs_ino_t		ino)
{
	xfs_dir2_sf_put_ino(hdr, &hdr->parent, ino);
}

static xfs_dir2_inou_t *
xfs_dir2_sfe_inop(
	struct xfs_dir2_sf_entry *sfep)
{
	return (xfs_dir2_inou_t *)&sfep->name[sfep->namelen];
}

xfs_ino_t
xfs_dir2_sfe_get_ino(
	struct xfs_dir2_sf_hdr	*hdr,
	struct xfs_dir2_sf_entry *sfep)
{
	return xfs_dir2_sf_get_ino(hdr, xfs_dir2_sfe_inop(sfep));
}

static void
xfs_dir2_sfe_put_ino(
	struct xfs_dir2_sf_hdr	*hdr,
	struct xfs_dir2_sf_entry *sfep,
	xfs_ino_t		ino)
{
	xfs_dir2_sf_put_ino(hdr, xfs_dir2_sfe_inop(sfep), ino);
}

int						
xfs_dir2_block_sfsize(
	xfs_inode_t		*dp,		
	xfs_dir2_data_hdr_t	*hdr,		
	xfs_dir2_sf_hdr_t	*sfhp)		
{
	xfs_dir2_dataptr_t	addr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dir2_block_tail_t	*btp;		
	int			count;		
	xfs_dir2_data_entry_t	*dep;		
	int			i;		
	int			i8count;	
	int			isdot;		
	int			isdotdot;	
	xfs_mount_t		*mp;		
	int			namelen;	
	xfs_ino_t		parent = 0;	
	int			size=0;		

	mp = dp->i_mount;

	count = i8count = namelen = 0;
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);

	for (i = 0; i < be32_to_cpu(btp->count); i++) {
		if ((addr = be32_to_cpu(blp[i].address)) == XFS_DIR2_NULL_DATAPTR)
			continue;
		dep = (xfs_dir2_data_entry_t *)
		      ((char *)hdr + xfs_dir2_dataptr_to_off(mp, addr));
		isdot = dep->namelen == 1 && dep->name[0] == '.';
		isdotdot =
			dep->namelen == 2 &&
			dep->name[0] == '.' && dep->name[1] == '.';
#if XFS_BIG_INUMS
		if (!isdot)
			i8count += be64_to_cpu(dep->inumber) > XFS_DIR2_MAX_SHORT_INUM;
#endif
		if (!isdot && !isdotdot) {
			count++;
			namelen += dep->namelen;
		} else if (isdotdot)
			parent = be64_to_cpu(dep->inumber);
		size = xfs_dir2_sf_hdr_size(i8count) +		
		       count +					
		       count * (uint)sizeof(xfs_dir2_sf_off_t) + 
		       namelen +				
		       (i8count ?				
				(uint)sizeof(xfs_dir2_ino8_t) * count :
				(uint)sizeof(xfs_dir2_ino4_t) * count);
		if (size > XFS_IFORK_DSIZE(dp))
			return size;		
	}
	sfhp->count = count;
	sfhp->i8count = i8count;
	xfs_dir2_sf_put_parent_ino(sfhp, parent);
	return size;
}

int						
xfs_dir2_block_to_sf(
	xfs_da_args_t		*args,		
	xfs_dabuf_t		*bp,		
	int			size,		
	xfs_dir2_sf_hdr_t	*sfhp)		
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	xfs_dir2_data_unused_t	*dup;		
	char			*endptr;	
	int			error;		
	int			logflags;	
	xfs_mount_t		*mp;		
	char			*ptr;		
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	trace_xfs_dir2_block_to_sf(args);

	dp = args->dp;
	mp = dp->i_mount;

	hdr = kmem_alloc(mp->m_dirblksize, KM_SLEEP);
	memcpy(hdr, bp->data, mp->m_dirblksize);
	logflags = XFS_ILOG_CORE;
	if ((error = xfs_dir2_shrink_inode(args, mp->m_dirdatablk, bp))) {
		ASSERT(error != ENOSPC);
		goto out;
	}

	dp->i_df.if_flags &= ~XFS_IFEXTENTS;
	dp->i_df.if_flags |= XFS_IFINLINE;
	dp->i_d.di_format = XFS_DINODE_FMT_LOCAL;
	ASSERT(dp->i_df.if_bytes == 0);
	xfs_idata_realloc(dp, size, XFS_DATA_FORK);
	logflags |= XFS_ILOG_DDATA;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	memcpy(sfp, sfhp, xfs_dir2_sf_hdr_size(sfhp->i8count));
	dp->i_d.di_size = size;
	btp = xfs_dir2_block_tail_p(mp, hdr);
	ptr = (char *)(hdr + 1);
	endptr = (char *)xfs_dir2_block_leaf_p(btp);
	sfep = xfs_dir2_sf_firstentry(sfp);
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ptr += be16_to_cpu(dup->length);
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		if (dep->namelen == 1 && dep->name[0] == '.')
			ASSERT(be64_to_cpu(dep->inumber) == dp->i_ino);
		else if (dep->namelen == 2 &&
			 dep->name[0] == '.' && dep->name[1] == '.')
			ASSERT(be64_to_cpu(dep->inumber) ==
			       xfs_dir2_sf_get_parent_ino(sfp));
		else {
			sfep->namelen = dep->namelen;
			xfs_dir2_sf_put_offset(sfep,
				(xfs_dir2_data_aoff_t)
				((char *)dep - (char *)hdr));
			memcpy(sfep->name, dep->name, dep->namelen);
			xfs_dir2_sfe_put_ino(sfp, sfep,
					     be64_to_cpu(dep->inumber));

			sfep = xfs_dir2_sf_nextentry(sfp, sfep);
		}
		ptr += xfs_dir2_data_entsize(dep->namelen);
	}
	ASSERT((char *)sfep - (char *)sfp == size);
	xfs_dir2_sf_check(args);
out:
	xfs_trans_log_inode(args->trans, dp, logflags);
	kmem_free(hdr);
	return error;
}

int						
xfs_dir2_sf_addname(
	xfs_da_args_t		*args)		
{
	int			add_entsize;	
	xfs_inode_t		*dp;		
	int			error;		
	int			incr_isize;	
	int			new_isize;	
	int			objchange;	
	xfs_dir2_data_aoff_t	offset = 0;	
	int			old_isize;	
	int			pick;		
	xfs_dir2_sf_hdr_t	*sfp;		
	xfs_dir2_sf_entry_t	*sfep = NULL;	

	trace_xfs_dir2_sf_addname(args);

	ASSERT(xfs_dir2_sf_lookup(args) == ENOENT);
	dp = args->dp;
	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(dp->i_mount));
		return XFS_ERROR(EIO);
	}
	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(sfp->i8count));
	add_entsize = xfs_dir2_sf_entsize(sfp, args->namelen);
	incr_isize = add_entsize;
	objchange = 0;
#if XFS_BIG_INUMS
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && sfp->i8count == 0) {
		add_entsize +=
			(uint)sizeof(xfs_dir2_ino8_t) -
			(uint)sizeof(xfs_dir2_ino4_t);
		incr_isize +=
			(sfp->count + 2) *
			((uint)sizeof(xfs_dir2_ino8_t) -
			 (uint)sizeof(xfs_dir2_ino4_t));
		objchange = 1;
	}
#endif
	old_isize = (int)dp->i_d.di_size;
	new_isize = old_isize + incr_isize;
	if (new_isize > XFS_IFORK_DSIZE(dp) ||
	    (pick =
	     xfs_dir2_sf_addname_pick(args, objchange, &sfep, &offset)) == 0) {
		if ((args->op_flags & XFS_DA_OP_JUSTCHECK) || args->total == 0)
			return XFS_ERROR(ENOSPC);
		error = xfs_dir2_sf_to_block(args);
		if (error)
			return error;
		return xfs_dir2_block_addname(args);
	}
	if (args->op_flags & XFS_DA_OP_JUSTCHECK)
		return 0;
	if (pick == 1)
		xfs_dir2_sf_addname_easy(args, sfep, offset, new_isize);
	else {
		ASSERT(pick == 2);
#if XFS_BIG_INUMS
		if (objchange)
			xfs_dir2_sf_toino8(args);
#endif
		xfs_dir2_sf_addname_hard(args, objchange, new_isize);
	}
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

static void
xfs_dir2_sf_addname_easy(
	xfs_da_args_t		*args,		
	xfs_dir2_sf_entry_t	*sfep,		
	xfs_dir2_data_aoff_t	offset,		
	int			new_isize)	
{
	int			byteoff;	
	xfs_inode_t		*dp;		
	xfs_dir2_sf_hdr_t	*sfp;		

	dp = args->dp;

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	byteoff = (int)((char *)sfep - (char *)sfp);
	xfs_idata_realloc(dp, xfs_dir2_sf_entsize(sfp, args->namelen),
		XFS_DATA_FORK);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfep = (xfs_dir2_sf_entry_t *)((char *)sfp + byteoff);
	sfep->namelen = args->namelen;
	xfs_dir2_sf_put_offset(sfep, offset);
	memcpy(sfep->name, args->name, sfep->namelen);
	xfs_dir2_sfe_put_ino(sfp, sfep, args->inumber);
	sfp->count++;
#if XFS_BIG_INUMS
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM)
		sfp->i8count++;
#endif
	dp->i_d.di_size = new_isize;
	xfs_dir2_sf_check(args);
}

static void
xfs_dir2_sf_addname_hard(
	xfs_da_args_t		*args,		
	int			objchange,	
	int			new_isize)	
{
	int			add_datasize;	
	char			*buf;		
	xfs_inode_t		*dp;		
	int			eof;		
	int			nbytes;		
	xfs_dir2_data_aoff_t	new_offset;	
	xfs_dir2_data_aoff_t	offset;		
	int			old_isize;	
	xfs_dir2_sf_entry_t	*oldsfep;	
	xfs_dir2_sf_hdr_t	*oldsfp;	
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	dp = args->dp;

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	old_isize = (int)dp->i_d.di_size;
	buf = kmem_alloc(old_isize, KM_SLEEP);
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	memcpy(oldsfp, sfp, old_isize);
	for (offset = XFS_DIR2_DATA_FIRST_OFFSET,
	      oldsfep = xfs_dir2_sf_firstentry(oldsfp),
	      add_datasize = xfs_dir2_data_entsize(args->namelen),
	      eof = (char *)oldsfep == &buf[old_isize];
	     !eof;
	     offset = new_offset + xfs_dir2_data_entsize(oldsfep->namelen),
	      oldsfep = xfs_dir2_sf_nextentry(oldsfp, oldsfep),
	      eof = (char *)oldsfep == &buf[old_isize]) {
		new_offset = xfs_dir2_sf_get_offset(oldsfep);
		if (offset + add_datasize <= new_offset)
			break;
	}
	xfs_idata_realloc(dp, -old_isize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, new_isize, XFS_DATA_FORK);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	nbytes = (int)((char *)oldsfep - (char *)oldsfp);
	memcpy(sfp, oldsfp, nbytes);
	sfep = (xfs_dir2_sf_entry_t *)((char *)sfp + nbytes);
	sfep->namelen = args->namelen;
	xfs_dir2_sf_put_offset(sfep, offset);
	memcpy(sfep->name, args->name, sfep->namelen);
	xfs_dir2_sfe_put_ino(sfp, sfep, args->inumber);
	sfp->count++;
#if XFS_BIG_INUMS
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && !objchange)
		sfp->i8count++;
#endif
	if (!eof) {
		sfep = xfs_dir2_sf_nextentry(sfp, sfep);
		memcpy(sfep, oldsfep, old_isize - nbytes);
	}
	kmem_free(buf);
	dp->i_d.di_size = new_isize;
	xfs_dir2_sf_check(args);
}

static int					
xfs_dir2_sf_addname_pick(
	xfs_da_args_t		*args,		
	int			objchange,	
	xfs_dir2_sf_entry_t	**sfepp,	
	xfs_dir2_data_aoff_t	*offsetp)	
{
	xfs_inode_t		*dp;		
	int			holefit;	
	int			i;		
	xfs_mount_t		*mp;		
	xfs_dir2_data_aoff_t	offset;		
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		
	int			size;		
	int			used;		

	dp = args->dp;
	mp = dp->i_mount;

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	size = xfs_dir2_data_entsize(args->namelen);
	offset = XFS_DIR2_DATA_FIRST_OFFSET;
	sfep = xfs_dir2_sf_firstentry(sfp);
	holefit = 0;
	for (i = 0; i < sfp->count; i++) {
		if (!holefit)
			holefit = offset + size <= xfs_dir2_sf_get_offset(sfep);
		offset = xfs_dir2_sf_get_offset(sfep) +
			 xfs_dir2_data_entsize(sfep->namelen);
		sfep = xfs_dir2_sf_nextentry(sfp, sfep);
	}
	used = offset +
	       (sfp->count + 3) * (uint)sizeof(xfs_dir2_leaf_entry_t) +
	       (uint)sizeof(xfs_dir2_block_tail_t);
	if (used + (holefit ? 0 : size) > mp->m_dirblksize)
		return 0;
#if XFS_BIG_INUMS
	if (objchange) {
		return 2;
	}
#else
	ASSERT(objchange == 0);
#endif
	if (used + size > mp->m_dirblksize)
		return 2;
	*sfepp = sfep;
	*offsetp = offset;
	return 1;
}

#ifdef DEBUG
static void
xfs_dir2_sf_check(
	xfs_da_args_t		*args)		
{
	xfs_inode_t		*dp;		
	int			i;		
	int			i8count;	
	xfs_ino_t		ino;		
	int			offset;		
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	dp = args->dp;

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	offset = XFS_DIR2_DATA_FIRST_OFFSET;
	ino = xfs_dir2_sf_get_parent_ino(sfp);
	i8count = ino > XFS_DIR2_MAX_SHORT_INUM;

	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep)) {
		ASSERT(xfs_dir2_sf_get_offset(sfep) >= offset);
		ino = xfs_dir2_sfe_get_ino(sfp, sfep);
		i8count += ino > XFS_DIR2_MAX_SHORT_INUM;
		offset =
			xfs_dir2_sf_get_offset(sfep) +
			xfs_dir2_data_entsize(sfep->namelen);
	}
	ASSERT(i8count == sfp->i8count);
	ASSERT(XFS_BIG_INUMS || i8count == 0);
	ASSERT((char *)sfep - (char *)sfp == dp->i_d.di_size);
	ASSERT(offset +
	       (sfp->count + 2) * (uint)sizeof(xfs_dir2_leaf_entry_t) +
	       (uint)sizeof(xfs_dir2_block_tail_t) <=
	       dp->i_mount->m_dirblksize);
}
#endif	

int					
xfs_dir2_sf_create(
	xfs_da_args_t	*args,		
	xfs_ino_t	pino)		
{
	xfs_inode_t	*dp;		
	int		i8count;	
	xfs_dir2_sf_hdr_t *sfp;		
	int		size;		

	trace_xfs_dir2_sf_create(args);

	dp = args->dp;

	ASSERT(dp != NULL);
	ASSERT(dp->i_d.di_size == 0);
	if (dp->i_d.di_format == XFS_DINODE_FMT_EXTENTS) {
		dp->i_df.if_flags &= ~XFS_IFEXTENTS;	
		dp->i_d.di_format = XFS_DINODE_FMT_LOCAL;
		xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE);
		dp->i_df.if_flags |= XFS_IFINLINE;
	}
	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	ASSERT(dp->i_df.if_bytes == 0);
	i8count = pino > XFS_DIR2_MAX_SHORT_INUM;
	size = xfs_dir2_sf_hdr_size(i8count);
	xfs_idata_realloc(dp, size, XFS_DATA_FORK);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfp->i8count = i8count;
	xfs_dir2_sf_put_parent_ino(sfp, pino);
	sfp->count = 0;
	dp->i_d.di_size = size;
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

int						
xfs_dir2_sf_getdents(
	xfs_inode_t		*dp,		
	void			*dirent,
	xfs_off_t		*offset,
	filldir_t		filldir)
{
	int			i;		
	xfs_mount_t		*mp;		
	xfs_dir2_dataptr_t	off;		
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		
	xfs_dir2_dataptr_t	dot_offset;
	xfs_dir2_dataptr_t	dotdot_offset;
	xfs_ino_t		ino;

	mp = dp->i_mount;

	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		return XFS_ERROR(EIO);
	}

	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;

	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(sfp->i8count));

	if (xfs_dir2_dataptr_to_db(mp, *offset) > mp->m_dirdatablk)
		return 0;

	dot_offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
					     XFS_DIR2_DATA_DOT_OFFSET);
	dotdot_offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
						XFS_DIR2_DATA_DOTDOT_OFFSET);

	if (*offset <= dot_offset) {
		if (filldir(dirent, ".", 1, dot_offset & 0x7fffffff, dp->i_ino, DT_DIR)) {
			*offset = dot_offset & 0x7fffffff;
			return 0;
		}
	}

	if (*offset <= dotdot_offset) {
		ino = xfs_dir2_sf_get_parent_ino(sfp);
		if (filldir(dirent, "..", 2, dotdot_offset & 0x7fffffff, ino, DT_DIR)) {
			*offset = dotdot_offset & 0x7fffffff;
			return 0;
		}
	}

	sfep = xfs_dir2_sf_firstentry(sfp);
	for (i = 0; i < sfp->count; i++) {
		off = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
				xfs_dir2_sf_get_offset(sfep));

		if (*offset > off) {
			sfep = xfs_dir2_sf_nextentry(sfp, sfep);
			continue;
		}

		ino = xfs_dir2_sfe_get_ino(sfp, sfep);
		if (filldir(dirent, (char *)sfep->name, sfep->namelen,
			    off & 0x7fffffff, ino, DT_UNKNOWN)) {
			*offset = off & 0x7fffffff;
			return 0;
		}
		sfep = xfs_dir2_sf_nextentry(sfp, sfep);
	}

	*offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk + 1, 0) &
			0x7fffffff;
	return 0;
}

int						
xfs_dir2_sf_lookup(
	xfs_da_args_t		*args)		
{
	xfs_inode_t		*dp;		
	int			i;		
	int			error;
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		
	enum xfs_dacmp		cmp;		
	xfs_dir2_sf_entry_t	*ci_sfep;	

	trace_xfs_dir2_sf_lookup(args);

	xfs_dir2_sf_check(args);
	dp = args->dp;

	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(dp->i_mount));
		return XFS_ERROR(EIO);
	}
	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(sfp->i8count));
	if (args->namelen == 1 && args->name[0] == '.') {
		args->inumber = dp->i_ino;
		args->cmpresult = XFS_CMP_EXACT;
		return XFS_ERROR(EEXIST);
	}
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
		args->inumber = xfs_dir2_sf_get_parent_ino(sfp);
		args->cmpresult = XFS_CMP_EXACT;
		return XFS_ERROR(EEXIST);
	}
	ci_sfep = NULL;
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp); i < sfp->count;
				i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep)) {
		cmp = dp->i_mount->m_dirnameops->compname(args, sfep->name,
								sfep->namelen);
		if (cmp != XFS_CMP_DIFFERENT && cmp != args->cmpresult) {
			args->cmpresult = cmp;
			args->inumber = xfs_dir2_sfe_get_ino(sfp, sfep);
			if (cmp == XFS_CMP_EXACT)
				return XFS_ERROR(EEXIST);
			ci_sfep = sfep;
		}
	}
	ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
	if (!ci_sfep)
		return XFS_ERROR(ENOENT);
	
	error = xfs_dir_cilookup_result(args, ci_sfep->name, ci_sfep->namelen);
	return XFS_ERROR(error);
}

int						
xfs_dir2_sf_removename(
	xfs_da_args_t		*args)
{
	int			byteoff;	
	xfs_inode_t		*dp;		
	int			entsize;	
	int			i;		
	int			newsize;	
	int			oldsize;	
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	trace_xfs_dir2_sf_removename(args);

	dp = args->dp;

	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	oldsize = (int)dp->i_d.di_size;
	if (oldsize < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(dp->i_mount));
		return XFS_ERROR(EIO);
	}
	ASSERT(dp->i_df.if_bytes == oldsize);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsize >= xfs_dir2_sf_hdr_size(sfp->i8count));
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp); i < sfp->count;
				i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep)) {
		if (xfs_da_compname(args, sfep->name, sfep->namelen) ==
								XFS_CMP_EXACT) {
			ASSERT(xfs_dir2_sfe_get_ino(sfp, sfep) ==
			       args->inumber);
			break;
		}
	}
	if (i == sfp->count)
		return XFS_ERROR(ENOENT);
	byteoff = (int)((char *)sfep - (char *)sfp);
	entsize = xfs_dir2_sf_entsize(sfp, args->namelen);
	newsize = oldsize - entsize;
	if (byteoff + entsize < oldsize)
		memmove((char *)sfp + byteoff, (char *)sfp + byteoff + entsize,
			oldsize - (byteoff + entsize));
	sfp->count--;
	dp->i_d.di_size = newsize;
	xfs_idata_realloc(dp, newsize - oldsize, XFS_DATA_FORK);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
#if XFS_BIG_INUMS
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM) {
		if (sfp->i8count == 1)
			xfs_dir2_sf_toino4(args);
		else
			sfp->i8count--;
	}
#endif
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

int						
xfs_dir2_sf_replace(
	xfs_da_args_t		*args)		
{
	xfs_inode_t		*dp;		
	int			i;		
#if XFS_BIG_INUMS || defined(DEBUG)
	xfs_ino_t		ino=0;		
#endif
#if XFS_BIG_INUMS
	int			i8elevated;	
#endif
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	trace_xfs_dir2_sf_replace(args);

	dp = args->dp;

	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(dp->i_mount));
		return XFS_ERROR(EIO);
	}
	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(sfp->i8count));
#if XFS_BIG_INUMS
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && sfp->i8count == 0) {
		int	error;			
		int	newsize;		

		newsize =
			dp->i_df.if_bytes +
			(sfp->count + 1) *
			((uint)sizeof(xfs_dir2_ino8_t) -
			 (uint)sizeof(xfs_dir2_ino4_t));
		if (newsize > XFS_IFORK_DSIZE(dp)) {
			error = xfs_dir2_sf_to_block(args);
			if (error) {
				return error;
			}
			return xfs_dir2_block_replace(args);
		}
		xfs_dir2_sf_toino8(args);
		i8elevated = 1;
		sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	} else
		i8elevated = 0;
#endif
	ASSERT(args->namelen != 1 || args->name[0] != '.');
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
#if XFS_BIG_INUMS || defined(DEBUG)
		ino = xfs_dir2_sf_get_parent_ino(sfp);
		ASSERT(args->inumber != ino);
#endif
		xfs_dir2_sf_put_parent_ino(sfp, args->inumber);
	}
	else {
		for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp);
				i < sfp->count;
				i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep)) {
			if (xfs_da_compname(args, sfep->name, sfep->namelen) ==
								XFS_CMP_EXACT) {
#if XFS_BIG_INUMS || defined(DEBUG)
				ino = xfs_dir2_sfe_get_ino(sfp, sfep);
				ASSERT(args->inumber != ino);
#endif
				xfs_dir2_sfe_put_ino(sfp, sfep, args->inumber);
				break;
			}
		}
		if (i == sfp->count) {
			ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
#if XFS_BIG_INUMS
			if (i8elevated)
				xfs_dir2_sf_toino4(args);
#endif
			return XFS_ERROR(ENOENT);
		}
	}
#if XFS_BIG_INUMS
	if (ino > XFS_DIR2_MAX_SHORT_INUM &&
	    args->inumber <= XFS_DIR2_MAX_SHORT_INUM) {
		if (sfp->i8count == 1)
			xfs_dir2_sf_toino4(args);
		else
			sfp->i8count--;
	}
	if (ino <= XFS_DIR2_MAX_SHORT_INUM &&
	    args->inumber > XFS_DIR2_MAX_SHORT_INUM) {
		ASSERT(sfp->i8count != 0);
		if (!i8elevated)
			sfp->i8count++;
	}
#endif
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_DDATA);
	return 0;
}

#if XFS_BIG_INUMS
static void
xfs_dir2_sf_toino4(
	xfs_da_args_t		*args)		
{
	char			*buf;		
	xfs_inode_t		*dp;		
	int			i;		
	int			newsize;	
	xfs_dir2_sf_entry_t	*oldsfep;	
	xfs_dir2_sf_hdr_t	*oldsfp;	
	int			oldsize;	
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	trace_xfs_dir2_sf_toino4(args);

	dp = args->dp;

	oldsize = dp->i_df.if_bytes;
	buf = kmem_alloc(oldsize, KM_SLEEP);
	oldsfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsfp->i8count == 1);
	memcpy(buf, oldsfp, oldsize);
	newsize =
		oldsize -
		(oldsfp->count + 1) *
		((uint)sizeof(xfs_dir2_ino8_t) - (uint)sizeof(xfs_dir2_ino4_t));
	xfs_idata_realloc(dp, -oldsize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, newsize, XFS_DATA_FORK);
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfp->count = oldsfp->count;
	sfp->i8count = 0;
	xfs_dir2_sf_put_parent_ino(sfp, xfs_dir2_sf_get_parent_ino(oldsfp));
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp),
		    oldsfep = xfs_dir2_sf_firstentry(oldsfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep),
		  oldsfep = xfs_dir2_sf_nextentry(oldsfp, oldsfep)) {
		sfep->namelen = oldsfep->namelen;
		sfep->offset = oldsfep->offset;
		memcpy(sfep->name, oldsfep->name, sfep->namelen);
		xfs_dir2_sfe_put_ino(sfp, sfep,
			xfs_dir2_sfe_get_ino(oldsfp, oldsfep));
	}
	kmem_free(buf);
	dp->i_d.di_size = newsize;
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
}

static void
xfs_dir2_sf_toino8(
	xfs_da_args_t		*args)		
{
	char			*buf;		
	xfs_inode_t		*dp;		
	int			i;		
	int			newsize;	
	xfs_dir2_sf_entry_t	*oldsfep;	
	xfs_dir2_sf_hdr_t	*oldsfp;	
	int			oldsize;	
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*sfp;		

	trace_xfs_dir2_sf_toino8(args);

	dp = args->dp;

	oldsize = dp->i_df.if_bytes;
	buf = kmem_alloc(oldsize, KM_SLEEP);
	oldsfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsfp->i8count == 0);
	memcpy(buf, oldsfp, oldsize);
	newsize =
		oldsize +
		(oldsfp->count + 1) *
		((uint)sizeof(xfs_dir2_ino8_t) - (uint)sizeof(xfs_dir2_ino4_t));
	xfs_idata_realloc(dp, -oldsize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, newsize, XFS_DATA_FORK);
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfp->count = oldsfp->count;
	sfp->i8count = 1;
	xfs_dir2_sf_put_parent_ino(sfp, xfs_dir2_sf_get_parent_ino(oldsfp));
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp),
		    oldsfep = xfs_dir2_sf_firstentry(oldsfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(sfp, sfep),
		  oldsfep = xfs_dir2_sf_nextentry(oldsfp, oldsfep)) {
		sfep->namelen = oldsfep->namelen;
		sfep->offset = oldsfep->offset;
		memcpy(sfep->name, oldsfep->name, sfep->namelen);
		xfs_dir2_sfe_put_ino(sfp, sfep,
			xfs_dir2_sfe_get_ino(oldsfp, oldsfep));
	}
	kmem_free(buf);
	dp->i_d.di_size = newsize;
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
}
#endif	
