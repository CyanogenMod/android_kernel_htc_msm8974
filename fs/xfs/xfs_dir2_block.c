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
#include "xfs_dir2.h"
#include "xfs_dir2_format.h"
#include "xfs_dir2_priv.h"
#include "xfs_error.h"
#include "xfs_trace.h"

static void xfs_dir2_block_log_leaf(xfs_trans_t *tp, xfs_dabuf_t *bp, int first,
				    int last);
static void xfs_dir2_block_log_tail(xfs_trans_t *tp, xfs_dabuf_t *bp);
static int xfs_dir2_block_lookup_int(xfs_da_args_t *args, xfs_dabuf_t **bpp,
				     int *entno);
static int xfs_dir2_block_sort(const void *a, const void *b);

static xfs_dahash_t xfs_dir_hash_dot, xfs_dir_hash_dotdot;

void
xfs_dir_startup(void)
{
	xfs_dir_hash_dot = xfs_da_hashname((unsigned char *)".", 1);
	xfs_dir_hash_dotdot = xfs_da_hashname((unsigned char *)"..", 2);
}

int						
xfs_dir2_block_addname(
	xfs_da_args_t		*args)		
{
	xfs_dir2_data_free_t	*bf;		
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	int			compact;	
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	xfs_dir2_data_unused_t	*dup;		
	int			error;		
	xfs_dir2_data_unused_t	*enddup=NULL;	
	xfs_dahash_t		hash;		
	int			high;		
	int			highstale;	
	int			lfloghigh=0;	
	int			lfloglow=0;	
	int			len;		
	int			low;		
	int			lowstale;	
	int			mid=0;		
	xfs_mount_t		*mp;		
	int			needlog;	
	int			needscan;	
	__be16			*tagp;		
	xfs_trans_t		*tp;		

	trace_xfs_dir2_block_addname(args);

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	if ((error =
	    xfs_da_read_buf(tp, dp, mp->m_dirdatablk, -1, &bp, XFS_DATA_FORK))) {
		return error;
	}
	ASSERT(bp != NULL);
	hdr = bp->data;
	if (unlikely(hdr->magic != cpu_to_be32(XFS_DIR2_BLOCK_MAGIC))) {
		XFS_CORRUPTION_ERROR("xfs_dir2_block_addname",
				     XFS_ERRLEVEL_LOW, mp, hdr);
		xfs_da_brelse(tp, bp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	len = xfs_dir2_data_entsize(args->namelen);
	bf = hdr->bestfree;
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	if (!btp->stale) {
		tagp = (__be16 *)blp - 1;
		enddup = (xfs_dir2_data_unused_t *)((char *)hdr + be16_to_cpu(*tagp));
		if (be16_to_cpu(enddup->freetag) != XFS_DIR2_DATA_FREE_TAG)
			dup = enddup = NULL;
		else {
			dup = (xfs_dir2_data_unused_t *)
			      ((char *)hdr + be16_to_cpu(bf[0].offset));
			if (dup == enddup) {
				if (be16_to_cpu(dup->length) < len + (uint)sizeof(*blp)) {
					if (be16_to_cpu(bf[1].length) >= len)
						dup = (xfs_dir2_data_unused_t *)
						      ((char *)hdr +
						       be16_to_cpu(bf[1].offset));
					else
						dup = NULL;
				}
			} else {
				if (be16_to_cpu(dup->length) < len) {
					dup = NULL;
				}
			}
		}
		compact = 0;
	}
	else if (be16_to_cpu(bf[0].length) >= len) {
		dup = (xfs_dir2_data_unused_t *)
		      ((char *)hdr + be16_to_cpu(bf[0].offset));
		compact = 0;
	}
	else {
		tagp = (__be16 *)blp - 1;
		dup = (xfs_dir2_data_unused_t *)((char *)hdr + be16_to_cpu(*tagp));
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			if (be16_to_cpu(dup->length) + (be32_to_cpu(btp->stale) - 1) *
			    (uint)sizeof(*blp) < len)
				dup = NULL;
		} else if ((be32_to_cpu(btp->stale) - 1) * (uint)sizeof(*blp) < len)
			dup = NULL;
		else
			dup = (xfs_dir2_data_unused_t *)blp;
		compact = 1;
	}
	if (args->op_flags & XFS_DA_OP_JUSTCHECK)
		xfs_da_brelse(tp, bp);
	if (!dup) {
		if ((args->op_flags & XFS_DA_OP_JUSTCHECK) || args->total == 0)
			return XFS_ERROR(ENOSPC);
		error = xfs_dir2_block_to_leaf(args, bp);
		xfs_da_buf_done(bp);
		if (error)
			return error;
		return xfs_dir2_leaf_addname(args);
	}
	if (args->op_flags & XFS_DA_OP_JUSTCHECK)
		return 0;
	needlog = needscan = 0;
	if (compact) {
		int	fromidx;		
		int	toidx;			

		for (fromidx = toidx = be32_to_cpu(btp->count) - 1,
			highstale = lfloghigh = -1;
		     fromidx >= 0;
		     fromidx--) {
			if (blp[fromidx].address ==
			    cpu_to_be32(XFS_DIR2_NULL_DATAPTR)) {
				if (highstale == -1)
					highstale = toidx;
				else {
					if (lfloghigh == -1)
						lfloghigh = toidx;
					continue;
				}
			}
			if (fromidx < toidx)
				blp[toidx] = blp[fromidx];
			toidx--;
		}
		lfloglow = toidx + 1 - (be32_to_cpu(btp->stale) - 1);
		lfloghigh -= be32_to_cpu(btp->stale) - 1;
		be32_add_cpu(&btp->count, -(be32_to_cpu(btp->stale) - 1));
		xfs_dir2_data_make_free(tp, bp,
			(xfs_dir2_data_aoff_t)((char *)blp - (char *)hdr),
			(xfs_dir2_data_aoff_t)((be32_to_cpu(btp->stale) - 1) * sizeof(*blp)),
			&needlog, &needscan);
		blp += be32_to_cpu(btp->stale) - 1;
		btp->stale = cpu_to_be32(1);
		if (needscan) {
			xfs_dir2_data_freescan(mp, hdr, &needlog);
			needscan = 0;
		}
	}
	else if (btp->stale) {
		lfloglow = be32_to_cpu(btp->count);
		lfloghigh = -1;
	}
	for (low = 0, high = be32_to_cpu(btp->count) - 1; low <= high; ) {
		mid = (low + high) >> 1;
		if ((hash = be32_to_cpu(blp[mid].hashval)) == args->hashval)
			break;
		if (hash < args->hashval)
			low = mid + 1;
		else
			high = mid - 1;
	}
	while (mid >= 0 && be32_to_cpu(blp[mid].hashval) >= args->hashval) {
		mid--;
	}
	if (!btp->stale) {
		xfs_dir2_data_use_free(tp, bp, enddup,
			(xfs_dir2_data_aoff_t)
			((char *)enddup - (char *)hdr + be16_to_cpu(enddup->length) -
			 sizeof(*blp)),
			(xfs_dir2_data_aoff_t)sizeof(*blp),
			&needlog, &needscan);
		be32_add_cpu(&btp->count, 1);
		if (needscan) {
			xfs_dir2_data_freescan(mp, hdr, &needlog);
			needscan = 0;
		}
		blp--;
		mid++;
		if (mid)
			memmove(blp, &blp[1], mid * sizeof(*blp));
		lfloglow = 0;
		lfloghigh = mid;
	}
	else {
		for (lowstale = mid;
		     lowstale >= 0 &&
			blp[lowstale].address !=
			cpu_to_be32(XFS_DIR2_NULL_DATAPTR);
		     lowstale--)
			continue;
		for (highstale = mid + 1;
		     highstale < be32_to_cpu(btp->count) &&
			blp[highstale].address !=
			cpu_to_be32(XFS_DIR2_NULL_DATAPTR) &&
			(lowstale < 0 || mid - lowstale > highstale - mid);
		     highstale++)
			continue;
		if (lowstale >= 0 &&
		    (highstale == be32_to_cpu(btp->count) ||
		     mid - lowstale <= highstale - mid)) {
			if (mid - lowstale)
				memmove(&blp[lowstale], &blp[lowstale + 1],
					(mid - lowstale) * sizeof(*blp));
			lfloglow = MIN(lowstale, lfloglow);
			lfloghigh = MAX(mid, lfloghigh);
		}
		else {
			ASSERT(highstale < be32_to_cpu(btp->count));
			mid++;
			if (highstale - mid)
				memmove(&blp[mid + 1], &blp[mid],
					(highstale - mid) * sizeof(*blp));
			lfloglow = MIN(mid, lfloglow);
			lfloghigh = MAX(highstale, lfloghigh);
		}
		be32_add_cpu(&btp->stale, -1);
	}
	dep = (xfs_dir2_data_entry_t *)dup;
	blp[mid].hashval = cpu_to_be32(args->hashval);
	blp[mid].address = cpu_to_be32(xfs_dir2_byte_to_dataptr(mp,
				(char *)dep - (char *)hdr));
	xfs_dir2_block_log_leaf(tp, bp, lfloglow, lfloghigh);
	xfs_dir2_data_use_free(tp, bp, dup,
		(xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr),
		(xfs_dir2_data_aoff_t)len, &needlog, &needscan);
	dep->inumber = cpu_to_be64(args->inumber);
	dep->namelen = args->namelen;
	memcpy(dep->name, args->name, args->namelen);
	tagp = xfs_dir2_data_entry_tag_p(dep);
	*tagp = cpu_to_be16((char *)dep - (char *)hdr);
	if (needscan)
		xfs_dir2_data_freescan(mp, hdr, &needlog);
	if (needlog)
		xfs_dir2_data_log_header(tp, bp);
	xfs_dir2_block_log_tail(tp, bp);
	xfs_dir2_data_log_entry(tp, bp, dep);
	xfs_dir2_data_check(dp, bp);
	xfs_da_buf_done(bp);
	return 0;
}

int						
xfs_dir2_block_getdents(
	xfs_inode_t		*dp,		
	void			*dirent,
	xfs_off_t		*offset,
	filldir_t		filldir)
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_dir2_data_unused_t	*dup;		
	char			*endptr;	
	int			error;		
	xfs_mount_t		*mp;		
	char			*ptr;		
	int			wantoff;	
	xfs_off_t		cook;

	mp = dp->i_mount;
	if (xfs_dir2_dataptr_to_db(mp, *offset) > mp->m_dirdatablk) {
		return 0;
	}
	error = xfs_da_read_buf(NULL, dp, mp->m_dirdatablk, -1,
				&bp, XFS_DATA_FORK);
	if (error)
		return error;

	ASSERT(bp != NULL);
	wantoff = xfs_dir2_dataptr_to_off(mp, *offset);
	hdr = bp->data;
	xfs_dir2_data_check(dp, bp);
	btp = xfs_dir2_block_tail_p(mp, hdr);
	ptr = (char *)(hdr + 1);
	endptr = (char *)xfs_dir2_block_leaf_p(btp);

	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ptr += be16_to_cpu(dup->length);
			continue;
		}

		dep = (xfs_dir2_data_entry_t *)ptr;

		ptr += xfs_dir2_data_entsize(dep->namelen);
		if ((char *)dep - (char *)hdr < wantoff)
			continue;

		cook = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
					    (char *)dep - (char *)hdr);

		if (filldir(dirent, (char *)dep->name, dep->namelen,
			    cook & 0x7fffffff, be64_to_cpu(dep->inumber),
			    DT_UNKNOWN)) {
			*offset = cook & 0x7fffffff;
			xfs_da_brelse(NULL, bp);
			return 0;
		}
	}

	*offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk + 1, 0) &
			0x7fffffff;
	xfs_da_brelse(NULL, bp);
	return 0;
}

static void
xfs_dir2_block_log_leaf(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp,		
	int			first,		
	int			last)		
{
	xfs_dir2_data_hdr_t	*hdr = bp->data;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dir2_block_tail_t	*btp;

	btp = xfs_dir2_block_tail_p(tp->t_mountp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	xfs_da_log_buf(tp, bp, (uint)((char *)&blp[first] - (char *)hdr),
		(uint)((char *)&blp[last + 1] - (char *)hdr - 1));
}

static void
xfs_dir2_block_log_tail(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp)		
{
	xfs_dir2_data_hdr_t	*hdr = bp->data;
	xfs_dir2_block_tail_t	*btp;

	btp = xfs_dir2_block_tail_p(tp->t_mountp, hdr);
	xfs_da_log_buf(tp, bp, (uint)((char *)btp - (char *)hdr),
		(uint)((char *)(btp + 1) - (char *)hdr - 1));
}

int						
xfs_dir2_block_lookup(
	xfs_da_args_t		*args)		
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	int			ent;		
	int			error;		
	xfs_mount_t		*mp;		

	trace_xfs_dir2_block_lookup(args);

	if ((error = xfs_dir2_block_lookup_int(args, &bp, &ent)))
		return error;
	dp = args->dp;
	mp = dp->i_mount;
	hdr = bp->data;
	xfs_dir2_data_check(dp, bp);
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	dep = (xfs_dir2_data_entry_t *)((char *)hdr +
		xfs_dir2_dataptr_to_off(mp, be32_to_cpu(blp[ent].address)));
	args->inumber = be64_to_cpu(dep->inumber);
	error = xfs_dir_cilookup_result(args, dep->name, dep->namelen);
	xfs_da_brelse(args->trans, bp);
	return XFS_ERROR(error);
}

static int					
xfs_dir2_block_lookup_int(
	xfs_da_args_t		*args,		
	xfs_dabuf_t		**bpp,		
	int			*entno)		
{
	xfs_dir2_dataptr_t	addr;		
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	int			error;		
	xfs_dahash_t		hash;		
	int			high;		
	int			low;		
	int			mid;		
	xfs_mount_t		*mp;		
	xfs_trans_t		*tp;		
	enum xfs_dacmp		cmp;		

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	if ((error =
	    xfs_da_read_buf(tp, dp, mp->m_dirdatablk, -1, &bp, XFS_DATA_FORK))) {
		return error;
	}
	ASSERT(bp != NULL);
	hdr = bp->data;
	xfs_dir2_data_check(dp, bp);
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	for (low = 0, high = be32_to_cpu(btp->count) - 1; ; ) {
		ASSERT(low <= high);
		mid = (low + high) >> 1;
		if ((hash = be32_to_cpu(blp[mid].hashval)) == args->hashval)
			break;
		if (hash < args->hashval)
			low = mid + 1;
		else
			high = mid - 1;
		if (low > high) {
			ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
			xfs_da_brelse(tp, bp);
			return XFS_ERROR(ENOENT);
		}
	}
	while (mid > 0 && be32_to_cpu(blp[mid - 1].hashval) == args->hashval) {
		mid--;
	}
	do {
		if ((addr = be32_to_cpu(blp[mid].address)) == XFS_DIR2_NULL_DATAPTR)
			continue;
		dep = (xfs_dir2_data_entry_t *)
			((char *)hdr + xfs_dir2_dataptr_to_off(mp, addr));
		cmp = mp->m_dirnameops->compname(args, dep->name, dep->namelen);
		if (cmp != XFS_CMP_DIFFERENT && cmp != args->cmpresult) {
			args->cmpresult = cmp;
			*bpp = bp;
			*entno = mid;
			if (cmp == XFS_CMP_EXACT)
				return 0;
		}
	} while (++mid < be32_to_cpu(btp->count) &&
			be32_to_cpu(blp[mid].hashval) == hash);

	ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
	if (args->cmpresult == XFS_CMP_CASE)
		return 0;
	xfs_da_brelse(tp, bp);
	return XFS_ERROR(ENOENT);
}

int						
xfs_dir2_block_removename(
	xfs_da_args_t		*args)		
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	int			ent;		
	int			error;		
	xfs_mount_t		*mp;		
	int			needlog;	
	int			needscan;	
	xfs_dir2_sf_hdr_t	sfh;		
	int			size;		
	xfs_trans_t		*tp;		

	trace_xfs_dir2_block_removename(args);

	if ((error = xfs_dir2_block_lookup_int(args, &bp, &ent))) {
		return error;
	}
	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	hdr = bp->data;
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	dep = (xfs_dir2_data_entry_t *)
	      ((char *)hdr + xfs_dir2_dataptr_to_off(mp, be32_to_cpu(blp[ent].address)));
	needlog = needscan = 0;
	xfs_dir2_data_make_free(tp, bp,
		(xfs_dir2_data_aoff_t)((char *)dep - (char *)hdr),
		xfs_dir2_data_entsize(dep->namelen), &needlog, &needscan);
	be32_add_cpu(&btp->stale, 1);
	xfs_dir2_block_log_tail(tp, bp);
	blp[ent].address = cpu_to_be32(XFS_DIR2_NULL_DATAPTR);
	xfs_dir2_block_log_leaf(tp, bp, ent, ent);
	if (needscan)
		xfs_dir2_data_freescan(mp, hdr, &needlog);
	if (needlog)
		xfs_dir2_data_log_header(tp, bp);
	xfs_dir2_data_check(dp, bp);
	size = xfs_dir2_block_sfsize(dp, hdr, &sfh);
	if (size > XFS_IFORK_DSIZE(dp)) {
		xfs_da_buf_done(bp);
		return 0;
	}
	return xfs_dir2_block_to_sf(args, bp, size, &sfh);
}

int						
xfs_dir2_block_replace(
	xfs_da_args_t		*args)		
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	int			ent;		
	int			error;		
	xfs_mount_t		*mp;		

	trace_xfs_dir2_block_replace(args);

	if ((error = xfs_dir2_block_lookup_int(args, &bp, &ent))) {
		return error;
	}
	dp = args->dp;
	mp = dp->i_mount;
	hdr = bp->data;
	btp = xfs_dir2_block_tail_p(mp, hdr);
	blp = xfs_dir2_block_leaf_p(btp);
	dep = (xfs_dir2_data_entry_t *)
	      ((char *)hdr + xfs_dir2_dataptr_to_off(mp, be32_to_cpu(blp[ent].address)));
	ASSERT(be64_to_cpu(dep->inumber) != args->inumber);
	dep->inumber = cpu_to_be64(args->inumber);
	xfs_dir2_data_log_entry(args->trans, bp, dep);
	xfs_dir2_data_check(dp, bp);
	xfs_da_buf_done(bp);
	return 0;
}

static int					
xfs_dir2_block_sort(
	const void			*a,	
	const void			*b)	
{
	const xfs_dir2_leaf_entry_t	*la;	
	const xfs_dir2_leaf_entry_t	*lb;	

	la = a;
	lb = b;
	return be32_to_cpu(la->hashval) < be32_to_cpu(lb->hashval) ? -1 :
		(be32_to_cpu(la->hashval) > be32_to_cpu(lb->hashval) ? 1 : 0);
}

int						
xfs_dir2_leaf_to_block(
	xfs_da_args_t		*args,		
	xfs_dabuf_t		*lbp,		
	xfs_dabuf_t		*dbp)		
{
	__be16			*bestsp;	
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_inode_t		*dp;		
	xfs_dir2_data_unused_t	*dup;		
	int			error;		
	int			from;		
	xfs_dir2_leaf_t		*leaf;		
	xfs_dir2_leaf_entry_t	*lep;		
	xfs_dir2_leaf_tail_t	*ltp;		
	xfs_mount_t		*mp;		
	int			needlog;	
	int			needscan;	
	xfs_dir2_sf_hdr_t	sfh;		
	int			size;		
	__be16			*tagp;		
	int			to;		
	xfs_trans_t		*tp;		

	trace_xfs_dir2_leaf_to_block(args);

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	leaf = lbp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_DIR2_LEAF1_MAGIC));
	ltp = xfs_dir2_leaf_tail_p(mp, leaf);
	while (dp->i_d.di_size > mp->m_dirblksize) {
		bestsp = xfs_dir2_leaf_bests_p(ltp);
		if (be16_to_cpu(bestsp[be32_to_cpu(ltp->bestcount) - 1]) ==
		    mp->m_dirblksize - (uint)sizeof(*hdr)) {
			if ((error =
			    xfs_dir2_leaf_trim_data(args, lbp,
				    (xfs_dir2_db_t)(be32_to_cpu(ltp->bestcount) - 1))))
				goto out;
		} else {
			error = 0;
			goto out;
		}
	}
	if (dbp == NULL &&
	    (error = xfs_da_read_buf(tp, dp, mp->m_dirdatablk, -1, &dbp,
		    XFS_DATA_FORK))) {
		goto out;
	}
	hdr = dbp->data;
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC));
	size = (uint)sizeof(xfs_dir2_block_tail_t) +
	       (uint)sizeof(*lep) * (be16_to_cpu(leaf->hdr.count) - be16_to_cpu(leaf->hdr.stale));
	tagp = (__be16 *)((char *)hdr + mp->m_dirblksize) - 1;
	dup = (xfs_dir2_data_unused_t *)((char *)hdr + be16_to_cpu(*tagp));
	if (be16_to_cpu(dup->freetag) != XFS_DIR2_DATA_FREE_TAG ||
	    be16_to_cpu(dup->length) < size) {
		error = 0;
		goto out;
	}
	hdr->magic = cpu_to_be32(XFS_DIR2_BLOCK_MAGIC);
	needlog = 1;
	needscan = 0;
	xfs_dir2_data_use_free(tp, dbp, dup, mp->m_dirblksize - size, size,
		&needlog, &needscan);
	btp = xfs_dir2_block_tail_p(mp, hdr);
	btp->count = cpu_to_be32(be16_to_cpu(leaf->hdr.count) - be16_to_cpu(leaf->hdr.stale));
	btp->stale = 0;
	xfs_dir2_block_log_tail(tp, dbp);
	lep = xfs_dir2_block_leaf_p(btp);
	for (from = to = 0; from < be16_to_cpu(leaf->hdr.count); from++) {
		if (leaf->ents[from].address ==
		    cpu_to_be32(XFS_DIR2_NULL_DATAPTR))
			continue;
		lep[to++] = leaf->ents[from];
	}
	ASSERT(to == be32_to_cpu(btp->count));
	xfs_dir2_block_log_leaf(tp, dbp, 0, be32_to_cpu(btp->count) - 1);
	if (needscan)
		xfs_dir2_data_freescan(mp, hdr, &needlog);
	if (needlog)
		xfs_dir2_data_log_header(tp, dbp);
	error = xfs_da_shrink_inode(args, mp->m_dirleafblk, lbp);
	lbp = NULL;
	if (error) {
		goto out;
	}
	size = xfs_dir2_block_sfsize(dp, hdr, &sfh);
	if (size > XFS_IFORK_DSIZE(dp)) {
		error = 0;
		goto out;
	}
	return xfs_dir2_block_to_sf(args, dbp, size, &sfh);
out:
	if (lbp)
		xfs_da_buf_done(lbp);
	if (dbp)
		xfs_da_buf_done(dbp);
	return error;
}

int						
xfs_dir2_sf_to_block(
	xfs_da_args_t		*args)		
{
	xfs_dir2_db_t		blkno;		
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_leaf_entry_t	*blp;		
	xfs_dabuf_t		*bp;		
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_inode_t		*dp;		
	int			dummy;		
	xfs_dir2_data_unused_t	*dup;		
	int			endoffset;	
	int			error;		
	int			i;		
	xfs_mount_t		*mp;		
	int			needlog;	
	int			needscan;	
	int			newoffset;	
	int			offset;		
	xfs_dir2_sf_entry_t	*sfep;		
	xfs_dir2_sf_hdr_t	*oldsfp;	
	xfs_dir2_sf_hdr_t	*sfp;		
	__be16			*tagp;		
	xfs_trans_t		*tp;		
	struct xfs_name		name;

	trace_xfs_dir2_sf_to_block(args);

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
		ASSERT(XFS_FORCED_SHUTDOWN(mp));
		return XFS_ERROR(EIO);
	}

	oldsfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;

	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(oldsfp->i8count));

	sfp = kmem_alloc(dp->i_df.if_bytes, KM_SLEEP);
	memcpy(sfp, oldsfp, dp->i_df.if_bytes);

	xfs_idata_realloc(dp, -dp->i_df.if_bytes, XFS_DATA_FORK);
	dp->i_d.di_size = 0;
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

	error = xfs_dir2_grow_inode(args, XFS_DIR2_DATA_SPACE, &blkno);
	if (error) {
		kmem_free(sfp);
		return error;
	}
	error = xfs_dir2_data_init(args, blkno, &bp);
	if (error) {
		kmem_free(sfp);
		return error;
	}
	hdr = bp->data;
	hdr->magic = cpu_to_be32(XFS_DIR2_BLOCK_MAGIC);
	i = (uint)sizeof(*btp) +
	    (sfp->count + 2) * (uint)sizeof(xfs_dir2_leaf_entry_t);
	dup = (xfs_dir2_data_unused_t *)(hdr + 1);
	needlog = needscan = 0;
	xfs_dir2_data_use_free(tp, bp, dup, mp->m_dirblksize - i, i, &needlog,
		&needscan);
	ASSERT(needscan == 0);
	btp = xfs_dir2_block_tail_p(mp, hdr);
	btp->count = cpu_to_be32(sfp->count + 2);	
	btp->stale = 0;
	blp = xfs_dir2_block_leaf_p(btp);
	endoffset = (uint)((char *)blp - (char *)hdr);
	xfs_dir2_data_use_free(tp, bp, dup,
		(xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr),
		be16_to_cpu(dup->length), &needlog, &needscan);
	dep = (xfs_dir2_data_entry_t *)
	      ((char *)hdr + XFS_DIR2_DATA_DOT_OFFSET);
	dep->inumber = cpu_to_be64(dp->i_ino);
	dep->namelen = 1;
	dep->name[0] = '.';
	tagp = xfs_dir2_data_entry_tag_p(dep);
	*tagp = cpu_to_be16((char *)dep - (char *)hdr);
	xfs_dir2_data_log_entry(tp, bp, dep);
	blp[0].hashval = cpu_to_be32(xfs_dir_hash_dot);
	blp[0].address = cpu_to_be32(xfs_dir2_byte_to_dataptr(mp,
				(char *)dep - (char *)hdr));
	dep = (xfs_dir2_data_entry_t *)
		((char *)hdr + XFS_DIR2_DATA_DOTDOT_OFFSET);
	dep->inumber = cpu_to_be64(xfs_dir2_sf_get_parent_ino(sfp));
	dep->namelen = 2;
	dep->name[0] = dep->name[1] = '.';
	tagp = xfs_dir2_data_entry_tag_p(dep);
	*tagp = cpu_to_be16((char *)dep - (char *)hdr);
	xfs_dir2_data_log_entry(tp, bp, dep);
	blp[1].hashval = cpu_to_be32(xfs_dir_hash_dotdot);
	blp[1].address = cpu_to_be32(xfs_dir2_byte_to_dataptr(mp,
				(char *)dep - (char *)hdr));
	offset = XFS_DIR2_DATA_FIRST_OFFSET;
	i = 0;
	if (!sfp->count)
		sfep = NULL;
	else
		sfep = xfs_dir2_sf_firstentry(sfp);
	while (offset < endoffset) {
		if (sfep == NULL)
			newoffset = endoffset;
		else
			newoffset = xfs_dir2_sf_get_offset(sfep);
		if (offset < newoffset) {
			dup = (xfs_dir2_data_unused_t *)((char *)hdr + offset);
			dup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
			dup->length = cpu_to_be16(newoffset - offset);
			*xfs_dir2_data_unused_tag_p(dup) = cpu_to_be16(
				((char *)dup - (char *)hdr));
			xfs_dir2_data_log_unused(tp, bp, dup);
			xfs_dir2_data_freeinsert(hdr, dup, &dummy);
			offset += be16_to_cpu(dup->length);
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)((char *)hdr + newoffset);
		dep->inumber = cpu_to_be64(xfs_dir2_sfe_get_ino(sfp, sfep));
		dep->namelen = sfep->namelen;
		memcpy(dep->name, sfep->name, dep->namelen);
		tagp = xfs_dir2_data_entry_tag_p(dep);
		*tagp = cpu_to_be16((char *)dep - (char *)hdr);
		xfs_dir2_data_log_entry(tp, bp, dep);
		name.name = sfep->name;
		name.len = sfep->namelen;
		blp[2 + i].hashval = cpu_to_be32(mp->m_dirnameops->
							hashname(&name));
		blp[2 + i].address = cpu_to_be32(xfs_dir2_byte_to_dataptr(mp,
						 (char *)dep - (char *)hdr));
		offset = (int)((char *)(tagp + 1) - (char *)hdr);
		if (++i == sfp->count)
			sfep = NULL;
		else
			sfep = xfs_dir2_sf_nextentry(sfp, sfep);
	}
	
	kmem_free(sfp);
	xfs_sort(blp, be32_to_cpu(btp->count), sizeof(*blp), xfs_dir2_block_sort);
	ASSERT(needscan == 0);
	xfs_dir2_block_log_leaf(tp, bp, 0, be32_to_cpu(btp->count) - 1);
	xfs_dir2_block_log_tail(tp, bp);
	xfs_dir2_data_check(dp, bp);
	xfs_da_buf_done(bp);
	return 0;
}
