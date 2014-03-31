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
#include "xfs_dir2.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_mount.h"
#include "xfs_itable.h"
#include "xfs_inode_item.h"
#include "xfs_extfree_item.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_attr_leaf.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_trans_space.h"
#include "xfs_buf_item.h"
#include "xfs_filestream.h"
#include "xfs_vnodeops.h"
#include "xfs_trace.h"


kmem_zone_t		*xfs_bmap_free_item_zone;


#ifdef DEBUG
STATIC void
xfs_bmap_check_leaf_extents(
	struct xfs_btree_cur	*cur,
	struct xfs_inode	*ip,
	int			whichfork);
#else
#define xfs_bmap_check_leaf_extents(cur, ip, whichfork)		do { } while (0)
#endif


STATIC int					
xfs_bmap_add_attrfork_extents(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*flags);	

STATIC int					
xfs_bmap_add_attrfork_local(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*flags);	

STATIC int				
xfs_bmap_alloc(
	xfs_bmalloca_t		*ap);	

STATIC int				
xfs_bmap_btree_to_extents(
	xfs_trans_t		*tp,	
	xfs_inode_t		*ip,	
	xfs_btree_cur_t		*cur,	
	int			*logflagsp, 
	int			whichfork); 

STATIC void
xfs_bmap_del_free(
	xfs_bmap_free_t		*flist,	
	xfs_bmap_free_item_t	*prev,	
	xfs_bmap_free_item_t	*free);	

STATIC int					
xfs_bmap_extents_to_btree(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	xfs_btree_cur_t		**curp,		
	int			wasdel,		
	int			*logflagsp,	
	int			whichfork);	

STATIC int				
xfs_bmap_local_to_extents(
	xfs_trans_t	*tp,		
	xfs_inode_t	*ip,		
	xfs_fsblock_t	*firstblock,	
	xfs_extlen_t	total,		
	int		*logflagsp,	
	int		whichfork);	

STATIC xfs_bmbt_rec_host_t *		
xfs_bmap_search_extents(
	xfs_inode_t	*ip,		
	xfs_fileoff_t	bno,		
	int		whichfork,	
	int		*eofp,		
	xfs_extnum_t	*lastxp,	
	xfs_bmbt_irec_t	*gotp,		
	xfs_bmbt_irec_t	*prevp);	

STATIC xfs_filblks_t
xfs_bmap_worst_indlen(
	xfs_inode_t		*ip,	
	xfs_filblks_t		len);	

#ifdef DEBUG
STATIC void
xfs_bmap_validate_ret(
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	int			flags,
	xfs_bmbt_irec_t		*mval,
	int			nmap,
	int			ret_nmap);
#else
#define	xfs_bmap_validate_ret(bno,len,flags,mval,onmap,nmap)
#endif 

STATIC int
xfs_bmap_count_tree(
	xfs_mount_t     *mp,
	xfs_trans_t     *tp,
	xfs_ifork_t	*ifp,
	xfs_fsblock_t   blockno,
	int             levelin,
	int		*count);

STATIC void
xfs_bmap_count_leaves(
	xfs_ifork_t		*ifp,
	xfs_extnum_t		idx,
	int			numrecs,
	int			*count);

STATIC void
xfs_bmap_disk_count_leaves(
	struct xfs_mount	*mp,
	struct xfs_btree_block	*block,
	int			numrecs,
	int			*count);


STATIC int				
xfs_bmbt_lookup_eq(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	int			*stat)	
{
	cur->bc_rec.b.br_startoff = off;
	cur->bc_rec.b.br_startblock = bno;
	cur->bc_rec.b.br_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

STATIC int				
xfs_bmbt_lookup_ge(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	int			*stat)	
{
	cur->bc_rec.b.br_startoff = off;
	cur->bc_rec.b.br_startblock = bno;
	cur->bc_rec.b.br_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
}

static inline bool xfs_bmap_needs_btree(struct xfs_inode *ip, int whichfork)
{
	return XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS &&
		XFS_IFORK_NEXTENTS(ip, whichfork) >
			XFS_IFORK_MAXEXT(ip, whichfork);
}

static inline bool xfs_bmap_wants_extents(struct xfs_inode *ip, int whichfork)
{
	return XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE &&
		XFS_IFORK_NEXTENTS(ip, whichfork) <=
			XFS_IFORK_MAXEXT(ip, whichfork);
}

STATIC int
xfs_bmbt_update(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	xfs_exntst_t		state)
{
	union xfs_btree_rec	rec;

	xfs_bmbt_disk_set_allf(&rec.bmbt, off, bno, len, state);
	return xfs_btree_update(cur, &rec);
}

STATIC int					
xfs_bmap_add_attrfork_btree(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*flags)		
{
	xfs_btree_cur_t		*cur;		
	int			error;		
	xfs_mount_t		*mp;		
	int			stat;		

	mp = ip->i_mount;
	if (ip->i_df.if_broot_bytes <= XFS_IFORK_DSIZE(ip))
		*flags |= XFS_ILOG_DBROOT;
	else {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, XFS_DATA_FORK);
		cur->bc_private.b.flist = flist;
		cur->bc_private.b.firstblock = *firstblock;
		if ((error = xfs_bmbt_lookup_ge(cur, 0, 0, 0, &stat)))
			goto error0;
		
		XFS_WANT_CORRUPTED_GOTO(stat == 1, error0);
		if ((error = xfs_btree_new_iroot(cur, flags, &stat)))
			goto error0;
		if (stat == 0) {
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			return XFS_ERROR(ENOSPC);
		}
		*firstblock = cur->bc_private.b.firstblock;
		cur->bc_private.b.allocated = 0;
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	}
	return 0;
error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

STATIC int					
xfs_bmap_add_attrfork_extents(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*flags)		
{
	xfs_btree_cur_t		*cur;		
	int			error;		

	if (ip->i_d.di_nextents * sizeof(xfs_bmbt_rec_t) <= XFS_IFORK_DSIZE(ip))
		return 0;
	cur = NULL;
	error = xfs_bmap_extents_to_btree(tp, ip, firstblock, flist, &cur, 0,
		flags, XFS_DATA_FORK);
	if (cur) {
		cur->bc_private.b.allocated = 0;
		xfs_btree_del_cursor(cur,
			error ? XFS_BTREE_ERROR : XFS_BTREE_NOERROR);
	}
	return error;
}

STATIC int					
xfs_bmap_add_attrfork_local(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*flags)		
{
	xfs_da_args_t		dargs;		
	int			error;		
	xfs_mount_t		*mp;		

	if (ip->i_df.if_bytes <= XFS_IFORK_DSIZE(ip))
		return 0;
	if (S_ISDIR(ip->i_d.di_mode)) {
		mp = ip->i_mount;
		memset(&dargs, 0, sizeof(dargs));
		dargs.dp = ip;
		dargs.firstblock = firstblock;
		dargs.flist = flist;
		dargs.total = mp->m_dirblkfsbs;
		dargs.whichfork = XFS_DATA_FORK;
		dargs.trans = tp;
		error = xfs_dir2_sf_to_block(&dargs);
	} else
		error = xfs_bmap_local_to_extents(tp, ip, firstblock, 1, flags,
			XFS_DATA_FORK);
	return error;
}

STATIC int				
xfs_bmap_add_extent_delay_real(
	struct xfs_bmalloca	*bma)
{
	struct xfs_bmbt_irec	*new = &bma->got;
	int			diff;	
	xfs_bmbt_rec_host_t	*ep;	
	int			error;	
	int			i;	
	xfs_ifork_t		*ifp;	
	xfs_fileoff_t		new_endoff;	
	xfs_bmbt_irec_t		r[3];	
					
	int			rval=0;	
	int			state = 0;
	xfs_filblks_t		da_new; 
	xfs_filblks_t		da_old; 
	xfs_filblks_t		temp=0;	
	xfs_filblks_t		temp2=0;
	int			tmp_rval;	

	ifp = XFS_IFORK_PTR(bma->ip, XFS_DATA_FORK);

	ASSERT(bma->idx >= 0);
	ASSERT(bma->idx <= ifp->if_bytes / sizeof(struct xfs_bmbt_rec));
	ASSERT(!isnullstartblock(new->br_startblock));
	ASSERT(!bma->cur ||
	       (bma->cur->bc_private.b.flags & XFS_BTCUR_BPRV_WASDEL));

	XFS_STATS_INC(xs_add_exlist);

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]

	ep = xfs_iext_get_ext(ifp, bma->idx);
	xfs_bmbt_get_all(ep, &PREV);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);

	da_old = startblockval(PREV.br_startblock);
	da_new = 0;

	if (PREV.br_startoff == new->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (PREV.br_startoff + PREV.br_blockcount == new_endoff)
		state |= BMAP_RIGHT_FILLING;

	if (bma->idx > 0) {
		state |= BMAP_LEFT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx - 1), &LEFT);

		if (isnullstartblock(LEFT.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
	    LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
	    LEFT.br_state == new->br_state &&
	    LEFT.br_blockcount + new->br_blockcount <= MAXEXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if (bma->idx < bma->ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t) - 1) {
		state |= BMAP_RIGHT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx + 1), &RIGHT);

		if (isnullstartblock(RIGHT.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new_endoff == RIGHT.br_startoff &&
	    new->br_startblock + new->br_blockcount == RIGHT.br_startblock &&
	    new->br_state == RIGHT.br_state &&
	    new->br_blockcount + RIGHT.br_blockcount <= MAXEXTLEN &&
	    ((state & (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING)) !=
		      (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING) ||
	     LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
			<= MAXEXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	error = 0;
	switch (state & (BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
			 BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
	     BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		bma->idx--;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, bma->idx),
			LEFT.br_blockcount + PREV.br_blockcount +
			RIGHT.br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		xfs_iext_remove(bma->ip, bma->idx + 1, 2, state);
		bma->ip->i_d.di_nextents--;
		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_btree_delete(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_btree_decrement(bma->cur, 0, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					PREV.br_blockcount +
					RIGHT.br_blockcount, LEFT.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
		bma->idx--;

		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, bma->idx),
			LEFT.br_blockcount + PREV.br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		xfs_iext_remove(bma->ip, bma->idx + 1, 1, state);
		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, LEFT.br_startoff,
					LEFT.br_startblock, LEFT.br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					PREV.br_blockcount, LEFT.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_startblock(ep, new->br_startblock);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount + RIGHT.br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		xfs_iext_remove(bma->ip, bma->idx + 1, 1, state);
		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, PREV.br_startoff,
					new->br_startblock,
					PREV.br_blockcount +
					RIGHT.br_blockcount, PREV.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_startblock(ep, new->br_startblock);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		bma->ip->i_d.di_nextents++;
		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			bma->cur->bc_rec.b.br_state = XFS_EXT_NORM;
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG:
		trace_xfs_bmap_pre_update(bma->ip, bma->idx - 1, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, bma->idx - 1),
			LEFT.br_blockcount + new->br_blockcount);
		xfs_bmbt_set_startoff(ep,
			PREV.br_startoff + new->br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx - 1, state, _THIS_IP_);

		temp = PREV.br_blockcount - new->br_blockcount;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);
		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, LEFT.br_startoff,
					LEFT.br_startblock, LEFT.br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					new->br_blockcount,
					LEFT.br_state);
			if (error)
				goto done;
		}
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock));
		xfs_bmbt_set_startblock(ep, nullstartblock(da_new));
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		bma->idx--;
		break;

	case BMAP_LEFT_FILLING:
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_startoff(ep, new_endoff);
		temp = PREV.br_blockcount - new->br_blockcount;
		xfs_bmbt_set_blockcount(ep, temp);
		xfs_iext_insert(bma->ip, bma->idx, 1, new, state);
		bma->ip->i_d.di_nextents++;
		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			bma->cur->bc_rec.b.br_state = XFS_EXT_NORM;
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}

		if (xfs_bmap_needs_btree(bma->ip, XFS_DATA_FORK)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
					bma->firstblock, bma->flist,
					&bma->cur, 1, &tmp_rval, XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock) -
			(bma->cur ? bma->cur->bc_private.b.allocated : 0));
		ep = xfs_iext_get_ext(ifp, bma->idx + 1);
		xfs_bmbt_set_startblock(ep, nullstartblock(da_new));
		trace_xfs_bmap_post_update(bma->ip, bma->idx + 1, state, _THIS_IP_);
		break;

	case BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		temp = PREV.br_blockcount - new->br_blockcount;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx + 1, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, bma->idx + 1),
			new->br_startoff, new->br_startblock,
			new->br_blockcount + RIGHT.br_blockcount,
			RIGHT.br_state);
		trace_xfs_bmap_post_update(bma->ip, bma->idx + 1, state, _THIS_IP_);
		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, new->br_startoff,
					new->br_startblock,
					new->br_blockcount +
					RIGHT.br_blockcount,
					RIGHT.br_state);
			if (error)
				goto done;
		}

		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock));
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_startblock(ep, nullstartblock(da_new));
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		bma->idx++;
		break;

	case BMAP_RIGHT_FILLING:
		temp = PREV.br_blockcount - new->br_blockcount;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);
		xfs_iext_insert(bma->ip, bma->idx + 1, 1, new, state);
		bma->ip->i_d.di_nextents++;
		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			bma->cur->bc_rec.b.br_state = XFS_EXT_NORM;
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}

		if (xfs_bmap_needs_btree(bma->ip, XFS_DATA_FORK)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
				bma->firstblock, bma->flist, &bma->cur, 1,
				&tmp_rval, XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock) -
			(bma->cur ? bma->cur->bc_private.b.allocated : 0));
		ep = xfs_iext_get_ext(ifp, bma->idx);
		xfs_bmbt_set_startblock(ep, nullstartblock(da_new));
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		bma->idx++;
		break;

	case 0:
		temp = new->br_startoff - PREV.br_startoff;
		temp2 = PREV.br_startoff + PREV.br_blockcount - new_endoff;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, 0, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);	
		LEFT = *new;
		RIGHT.br_state = PREV.br_state;
		RIGHT.br_startblock = nullstartblock(
				(int)xfs_bmap_worst_indlen(bma->ip, temp2));
		RIGHT.br_startoff = new_endoff;
		RIGHT.br_blockcount = temp2;
		
		xfs_iext_insert(bma->ip, bma->idx + 1, 2, &LEFT, state);
		bma->ip->i_d.di_nextents++;
		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			bma->cur->bc_rec.b.br_state = XFS_EXT_NORM;
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}

		if (xfs_bmap_needs_btree(bma->ip, XFS_DATA_FORK)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
					bma->firstblock, bma->flist, &bma->cur,
					1, &tmp_rval, XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		temp = xfs_bmap_worst_indlen(bma->ip, temp);
		temp2 = xfs_bmap_worst_indlen(bma->ip, temp2);
		diff = (int)(temp + temp2 - startblockval(PREV.br_startblock) -
			(bma->cur ? bma->cur->bc_private.b.allocated : 0));
		if (diff > 0) {
			error = xfs_icsb_modify_counters(bma->ip->i_mount,
					XFS_SBS_FDBLOCKS,
					-((int64_t)diff), 0);
			ASSERT(!error);
			if (error)
				goto done;
		}

		ep = xfs_iext_get_ext(ifp, bma->idx);
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);
		trace_xfs_bmap_pre_update(bma->ip, bma->idx + 2, state, _THIS_IP_);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, bma->idx + 2),
			nullstartblock((int)temp2));
		trace_xfs_bmap_post_update(bma->ip, bma->idx + 2, state, _THIS_IP_);

		bma->idx++;
		da_new = temp + temp2;
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_FILLING | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_CONTIG:
	case BMAP_RIGHT_CONTIG:
		ASSERT(0);
	}

	
	if (xfs_bmap_needs_btree(bma->ip, XFS_DATA_FORK)) {
		int	tmp_logflags;	

		ASSERT(bma->cur == NULL);
		error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
				bma->firstblock, bma->flist, &bma->cur,
				da_old > 0, &tmp_logflags, XFS_DATA_FORK);
		bma->logflags |= tmp_logflags;
		if (error)
			goto done;
	}

	
	if (da_old || da_new) {
		temp = da_new;
		if (bma->cur)
			temp += bma->cur->bc_private.b.allocated;
		ASSERT(temp <= da_old);
		if (temp < da_old)
			xfs_icsb_modify_counters(bma->ip->i_mount,
					XFS_SBS_FDBLOCKS,
					(int64_t)(da_old - temp), 0);
	}

	
	if (bma->cur)
		bma->cur->bc_private.b.allocated = 0;

	xfs_bmap_check_leaf_extents(bma->cur, bma->ip, XFS_DATA_FORK);
done:
	bma->logflags |= rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
}

/*
 * Convert an unwritten allocation to a real allocation or vice versa.
 */
STATIC int				
xfs_bmap_add_extent_unwritten_real(
	struct xfs_trans	*tp,
	xfs_inode_t		*ip,	
	xfs_extnum_t		*idx,	
	xfs_btree_cur_t		**curp,	
	xfs_bmbt_irec_t		*new,	
	xfs_fsblock_t		*first,	
	xfs_bmap_free_t		*flist,	
	int			*logflagsp) 
{
	xfs_btree_cur_t		*cur;	
	xfs_bmbt_rec_host_t	*ep;	
	int			error;	
	int			i;	
	xfs_ifork_t		*ifp;	
	xfs_fileoff_t		new_endoff;	
	xfs_exntst_t		newext;	
	xfs_exntst_t		oldext;	
	xfs_bmbt_irec_t		r[3];	
					
	int			rval=0;	
	int			state = 0;

	*logflagsp = 0;

	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);

	ASSERT(*idx >= 0);
	ASSERT(*idx <= ifp->if_bytes / sizeof(struct xfs_bmbt_rec));
	ASSERT(!isnullstartblock(new->br_startblock));

	XFS_STATS_INC(xs_add_exlist);

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]

	error = 0;
	ep = xfs_iext_get_ext(ifp, *idx);
	xfs_bmbt_get_all(ep, &PREV);
	newext = new->br_state;
	oldext = (newext == XFS_EXT_UNWRITTEN) ?
		XFS_EXT_NORM : XFS_EXT_UNWRITTEN;
	ASSERT(PREV.br_state == oldext);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);

	if (PREV.br_startoff == new->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (PREV.br_startoff + PREV.br_blockcount == new_endoff)
		state |= BMAP_RIGHT_FILLING;

	if (*idx > 0) {
		state |= BMAP_LEFT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, *idx - 1), &LEFT);

		if (isnullstartblock(LEFT.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
	    LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
	    LEFT.br_state == newext &&
	    LEFT.br_blockcount + new->br_blockcount <= MAXEXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if (*idx < ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t) - 1) {
		state |= BMAP_RIGHT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, *idx + 1), &RIGHT);
		if (isnullstartblock(RIGHT.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new_endoff == RIGHT.br_startoff &&
	    new->br_startblock + new->br_blockcount == RIGHT.br_startblock &&
	    newext == RIGHT.br_state &&
	    new->br_blockcount + RIGHT.br_blockcount <= MAXEXTLEN &&
	    ((state & (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING)) !=
		      (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING) ||
	     LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
			<= MAXEXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	switch (state & (BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
			 BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
	     BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		--*idx;

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, *idx),
			LEFT.br_blockcount + PREV.br_blockcount +
			RIGHT.br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		xfs_iext_remove(ip, *idx + 1, 2, state);
		ip->i_d.di_nextents -= 2;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + PREV.br_blockcount +
				RIGHT.br_blockcount, LEFT.br_state)))
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
		--*idx;

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, *idx),
			LEFT.br_blockcount + PREV.br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		xfs_iext_remove(ip, *idx + 1, 1, state);
		ip->i_d.di_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + PREV.br_blockcount,
				LEFT.br_state)))
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount + RIGHT.br_blockcount);
		xfs_bmbt_set_state(ep, newext);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		xfs_iext_remove(ip, *idx + 1, 1, state);
		ip->i_d.di_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock,
				new->br_blockcount + RIGHT.br_blockcount,
				newext)))
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_state(ep, newext);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock, new->br_blockcount,
				newext)))
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG:
		trace_xfs_bmap_pre_update(ip, *idx - 1, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, *idx - 1),
			LEFT.br_blockcount + new->br_blockcount);
		xfs_bmbt_set_startoff(ep,
			PREV.br_startoff + new->br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx - 1, state, _THIS_IP_);

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_startblock(ep,
			new->br_startblock + new->br_blockcount);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		--*idx;

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur,
				PREV.br_startoff + new->br_blockcount,
				PREV.br_startblock + new->br_blockcount,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			error = xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + new->br_blockcount,
				LEFT.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		ASSERT(ep && xfs_bmbt_get_state(ep) == oldext);
		xfs_bmbt_set_startoff(ep, new_endoff);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		xfs_bmbt_set_startblock(ep,
			new->br_startblock + new->br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		xfs_iext_insert(ip, *idx, 1, new, state);
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur,
				PREV.br_startoff + new->br_blockcount,
				PREV.br_startblock + new->br_blockcount,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			cur->bc_rec.b = *new;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		break;

	case BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		++*idx;

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, *idx),
			new->br_startoff, new->br_startblock,
			new->br_blockcount + RIGHT.br_blockcount, newext);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock,
					PREV.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, PREV.br_startoff,
				PREV.br_startblock,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_btree_increment(cur, 0, &i)))
				goto done;
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock,
				new->br_blockcount + RIGHT.br_blockcount,
				newext)))
				goto done;
		}
		break;

	case BMAP_RIGHT_FILLING:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		++*idx;
		xfs_iext_insert(ip, *idx, 1, new, state);

		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, PREV.br_startoff,
				PREV.br_startblock,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		break;

	case 0:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep,
			new->br_startoff - PREV.br_startoff);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		r[0] = *new;
		r[1].br_startoff = new_endoff;
		r[1].br_blockcount =
			PREV.br_startoff + PREV.br_blockcount - new_endoff;
		r[1].br_startblock = new->br_startblock + new->br_blockcount;
		r[1].br_state = oldext;

		++*idx;
		xfs_iext_insert(ip, *idx, 2, &r[0], state);

		ip->i_d.di_nextents += 2;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			
			if ((error = xfs_bmbt_update(cur, r[1].br_startoff,
				r[1].br_startblock, r[1].br_blockcount,
				r[1].br_state)))
				goto done;
			
			cur->bc_rec.b = PREV;
			cur->bc_rec.b.br_blockcount =
				new->br_startoff - PREV.br_startoff;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			
			cur->bc_rec.b.br_state = new->br_state;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_FILLING | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_CONTIG:
	case BMAP_RIGHT_CONTIG:
		ASSERT(0);
	}

	
	if (xfs_bmap_needs_btree(ip, XFS_DATA_FORK)) {
		int	tmp_logflags;	

		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, first, flist, &cur,
				0, &tmp_logflags, XFS_DATA_FORK);
		*logflagsp |= tmp_logflags;
		if (error)
			goto done;
	}

	
	if (cur) {
		cur->bc_private.b.allocated = 0;
		*curp = cur;
	}

	xfs_bmap_check_leaf_extents(*curp, ip, XFS_DATA_FORK);
done:
	*logflagsp |= rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
}

STATIC void
xfs_bmap_add_extent_hole_delay(
	xfs_inode_t		*ip,	
	xfs_extnum_t		*idx,	
	xfs_bmbt_irec_t		*new)	
{
	xfs_ifork_t		*ifp;	
	xfs_bmbt_irec_t		left;	
	xfs_filblks_t		newlen=0;	
	xfs_filblks_t		oldlen=0;	
	xfs_bmbt_irec_t		right;	
	int			state;  
	xfs_filblks_t		temp=0;	

	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	state = 0;
	ASSERT(isnullstartblock(new->br_startblock));

	if (*idx > 0) {
		state |= BMAP_LEFT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, *idx - 1), &left);

		if (isnullstartblock(left.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if (*idx < ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t)) {
		state |= BMAP_RIGHT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, *idx), &right);

		if (isnullstartblock(right.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && (state & BMAP_LEFT_DELAY) &&
	    left.br_startoff + left.br_blockcount == new->br_startoff &&
	    left.br_blockcount + new->br_blockcount <= MAXEXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if ((state & BMAP_RIGHT_VALID) && (state & BMAP_RIGHT_DELAY) &&
	    new->br_startoff + new->br_blockcount == right.br_startoff &&
	    new->br_blockcount + right.br_blockcount <= MAXEXTLEN &&
	    (!(state & BMAP_LEFT_CONTIG) ||
	     (left.br_blockcount + new->br_blockcount +
	      right.br_blockcount <= MAXEXTLEN)))
		state |= BMAP_RIGHT_CONTIG;

	switch (state & (BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
		--*idx;
		temp = left.br_blockcount + new->br_blockcount +
			right.br_blockcount;

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, *idx), temp);
		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, *idx),
			nullstartblock((int)newlen));
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);

		xfs_iext_remove(ip, *idx + 1, 1, state);
		break;

	case BMAP_LEFT_CONTIG:
		--*idx;
		temp = left.br_blockcount + new->br_blockcount;

		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, *idx), temp);
		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, *idx),
			nullstartblock((int)newlen));
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		break;

	case BMAP_RIGHT_CONTIG:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		temp = new->br_blockcount + right.br_blockcount;
		oldlen = startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, *idx),
			new->br_startoff,
			nullstartblock((int)newlen), temp, right.br_state);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		break;

	case 0:
		oldlen = newlen = 0;
		xfs_iext_insert(ip, *idx, 1, new, state);
		break;
	}
	if (oldlen != newlen) {
		ASSERT(oldlen > newlen);
		xfs_icsb_modify_counters(ip->i_mount, XFS_SBS_FDBLOCKS,
			(int64_t)(oldlen - newlen), 0);
	}
}

STATIC int				
xfs_bmap_add_extent_hole_real(
	struct xfs_bmalloca	*bma,
	int			whichfork)
{
	struct xfs_bmbt_irec	*new = &bma->got;
	int			error;	
	int			i;	
	xfs_ifork_t		*ifp;	
	xfs_bmbt_irec_t		left;	
	xfs_bmbt_irec_t		right;	
	int			rval=0;	
	int			state;	

	ifp = XFS_IFORK_PTR(bma->ip, whichfork);

	ASSERT(bma->idx >= 0);
	ASSERT(bma->idx <= ifp->if_bytes / sizeof(struct xfs_bmbt_rec));
	ASSERT(!isnullstartblock(new->br_startblock));
	ASSERT(!bma->cur ||
	       !(bma->cur->bc_private.b.flags & XFS_BTCUR_BPRV_WASDEL));

	XFS_STATS_INC(xs_add_exlist);

	state = 0;
	if (whichfork == XFS_ATTR_FORK)
		state |= BMAP_ATTRFORK;

	if (bma->idx > 0) {
		state |= BMAP_LEFT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx - 1), &left);
		if (isnullstartblock(left.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if (bma->idx < ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t)) {
		state |= BMAP_RIGHT_VALID;
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx), &right);
		if (isnullstartblock(right.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    left.br_startoff + left.br_blockcount == new->br_startoff &&
	    left.br_startblock + left.br_blockcount == new->br_startblock &&
	    left.br_state == new->br_state &&
	    left.br_blockcount + new->br_blockcount <= MAXEXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new->br_startoff + new->br_blockcount == right.br_startoff &&
	    new->br_startblock + new->br_blockcount == right.br_startblock &&
	    new->br_state == right.br_state &&
	    new->br_blockcount + right.br_blockcount <= MAXEXTLEN &&
	    (!(state & BMAP_LEFT_CONTIG) ||
	     left.br_blockcount + new->br_blockcount +
	     right.br_blockcount <= MAXEXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	error = 0;
	switch (state & (BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
		--bma->idx;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, bma->idx),
			left.br_blockcount + new->br_blockcount +
			right.br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		xfs_iext_remove(bma->ip, bma->idx + 1, 1, state);

		XFS_IFORK_NEXT_SET(bma->ip, whichfork,
			XFS_IFORK_NEXTENTS(bma->ip, whichfork) - 1);
		if (bma->cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, right.br_startoff,
					right.br_startblock, right.br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_btree_delete(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_btree_decrement(bma->cur, 0, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, left.br_startoff,
					left.br_startblock,
					left.br_blockcount +
						new->br_blockcount +
						right.br_blockcount,
					left.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_CONTIG:
		--bma->idx;
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, bma->idx),
			left.br_blockcount + new->br_blockcount);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		if (bma->cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, left.br_startoff,
					left.br_startblock, left.br_blockcount,
					&i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, left.br_startoff,
					left.br_startblock,
					left.br_blockcount +
						new->br_blockcount,
					left.br_state);
			if (error)
				goto done;
		}
		break;

	case BMAP_RIGHT_CONTIG:
		trace_xfs_bmap_pre_update(bma->ip, bma->idx, state, _THIS_IP_);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, bma->idx),
			new->br_startoff, new->br_startblock,
			new->br_blockcount + right.br_blockcount,
			right.br_state);
		trace_xfs_bmap_post_update(bma->ip, bma->idx, state, _THIS_IP_);

		if (bma->cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur,
					right.br_startoff,
					right.br_startblock,
					right.br_blockcount, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			error = xfs_bmbt_update(bma->cur, new->br_startoff,
					new->br_startblock,
					new->br_blockcount +
						right.br_blockcount,
					right.br_state);
			if (error)
				goto done;
		}
		break;

	case 0:
		xfs_iext_insert(bma->ip, bma->idx, 1, new, state);
		XFS_IFORK_NEXT_SET(bma->ip, whichfork,
			XFS_IFORK_NEXTENTS(bma->ip, whichfork) + 1);
		if (bma->cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur,
					new->br_startoff,
					new->br_startblock,
					new->br_blockcount, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			bma->cur->bc_rec.b.br_state = new->br_state;
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		break;
	}

	
	if (xfs_bmap_needs_btree(bma->ip, whichfork)) {
		int	tmp_logflags;	

		ASSERT(bma->cur == NULL);
		error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
				bma->firstblock, bma->flist, &bma->cur,
				0, &tmp_logflags, whichfork);
		bma->logflags |= tmp_logflags;
		if (error)
			goto done;
	}

	
	if (bma->cur)
		bma->cur->bc_private.b.allocated = 0;

	xfs_bmap_check_leaf_extents(bma->cur, bma->ip, whichfork);
done:
	bma->logflags |= rval;
	return error;
}

STATIC int
xfs_bmap_extsize_align(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*gotp,		
	xfs_bmbt_irec_t	*prevp,		
	xfs_extlen_t	extsz,		
	int		rt,		
	int		eof,		
	int		delay,		
	int		convert,	/* overwriting unwritten extent? */
	xfs_fileoff_t	*offp,		
	xfs_extlen_t	*lenp)		
{
	xfs_fileoff_t	orig_off;	
	xfs_extlen_t	orig_alen;	
	xfs_fileoff_t	orig_end;	
	xfs_fileoff_t	nexto;		
	xfs_fileoff_t	prevo;		
	xfs_fileoff_t	align_off;	
	xfs_extlen_t	align_alen;	
	xfs_extlen_t	temp;		

	if (convert)
		return 0;

	orig_off = align_off = *offp;
	orig_alen = align_alen = *lenp;
	orig_end = orig_off + orig_alen;

	if (!delay && !eof &&
	    (orig_off >= gotp->br_startoff) &&
	    (orig_end <= gotp->br_startoff + gotp->br_blockcount)) {
		return 0;
	}

	/*
	 * If the file offset is unaligned vs. the extent size
	 * we need to align it.  This will be possible unless
	 * the file was previously written with a kernel that didn't
	 * perform this alignment, or if a truncate shot us in the
	 * foot.
	 */
	temp = do_mod(orig_off, extsz);
	if (temp) {
		align_alen += temp;
		align_off -= temp;
	}
	if ((temp = (align_alen % extsz))) {
		align_alen += extsz - temp;
	}
	if (prevp->br_startoff != NULLFILEOFF) {
		if (prevp->br_startblock == HOLESTARTBLOCK)
			prevo = prevp->br_startoff;
		else
			prevo = prevp->br_startoff + prevp->br_blockcount;
	} else
		prevo = 0;
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	if (!eof && gotp->br_startoff != NULLFILEOFF) {
		if ((delay && gotp->br_startblock == HOLESTARTBLOCK) ||
		    (!delay && gotp->br_startblock == DELAYSTARTBLOCK))
			nexto = gotp->br_startoff + gotp->br_blockcount;
		else
			nexto = gotp->br_startoff;
	} else
		nexto = NULLFILEOFF;
	if (!eof &&
	    align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto)
		align_off = nexto > align_alen ? nexto - align_alen : 0;
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	if (align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto &&
	    nexto != NULLFILEOFF) {
		ASSERT(nexto > prevo);
		align_alen = nexto - align_off;
	}

	if (rt && (temp = (align_alen % mp->m_sb.sb_rextsize))) {
		if (orig_off < align_off ||
		    orig_end > align_off + align_alen ||
		    align_alen - temp < orig_alen)
			return XFS_ERROR(EINVAL);
		if (align_off + temp <= orig_off) {
			align_alen -= temp;
			align_off += temp;
		}
		else if (align_off + align_alen - temp >= orig_end)
			align_alen -= temp;
		else {
			align_alen -= orig_off - align_off;
			align_off = orig_off;
			align_alen -= align_alen % mp->m_sb.sb_rextsize;
		}
		if (orig_off < align_off || orig_end > align_off + align_alen)
			return XFS_ERROR(EINVAL);
	} else {
		ASSERT(orig_off >= align_off);
		ASSERT(orig_end <= align_off + align_alen);
	}

#ifdef DEBUG
	if (!eof && gotp->br_startoff != NULLFILEOFF)
		ASSERT(align_off + align_alen <= gotp->br_startoff);
	if (prevp->br_startoff != NULLFILEOFF)
		ASSERT(align_off >= prevp->br_startoff + prevp->br_blockcount);
#endif

	*lenp = align_alen;
	*offp = align_off;
	return 0;
}

#define XFS_ALLOC_GAP_UNITS	4

STATIC void
xfs_bmap_adjacent(
	xfs_bmalloca_t	*ap)		
{
	xfs_fsblock_t	adjust;		
	xfs_agnumber_t	fb_agno;	
	xfs_mount_t	*mp;		
	int		nullfb;		
	int		rt;		

#define	ISVALID(x,y)	\
	(rt ? \
		(x) < mp->m_sb.sb_rblocks : \
		XFS_FSB_TO_AGNO(mp, x) == XFS_FSB_TO_AGNO(mp, y) && \
		XFS_FSB_TO_AGNO(mp, x) < mp->m_sb.sb_agcount && \
		XFS_FSB_TO_AGBNO(mp, x) < mp->m_sb.sb_agblocks)

	mp = ap->ip->i_mount;
	nullfb = *ap->firstblock == NULLFSBLOCK;
	rt = XFS_IS_REALTIME_INODE(ap->ip) && ap->userdata;
	fb_agno = nullfb ? NULLAGNUMBER : XFS_FSB_TO_AGNO(mp, *ap->firstblock);
	if (ap->eof && ap->prev.br_startoff != NULLFILEOFF &&
	    !isnullstartblock(ap->prev.br_startblock) &&
	    ISVALID(ap->prev.br_startblock + ap->prev.br_blockcount,
		    ap->prev.br_startblock)) {
		ap->blkno = ap->prev.br_startblock + ap->prev.br_blockcount;
		adjust = ap->offset -
			(ap->prev.br_startoff + ap->prev.br_blockcount);
		if (adjust &&
		    ISVALID(ap->blkno + adjust, ap->prev.br_startblock))
			ap->blkno += adjust;
	}
	else if (!ap->eof) {
		xfs_fsblock_t	gotbno;		
		xfs_fsblock_t	gotdiff=0;	
		xfs_fsblock_t	prevbno;	
		xfs_fsblock_t	prevdiff=0;	

		if (ap->prev.br_startoff != NULLFILEOFF &&
		    !isnullstartblock(ap->prev.br_startblock) &&
		    (prevbno = ap->prev.br_startblock +
			       ap->prev.br_blockcount) &&
		    ISVALID(prevbno, ap->prev.br_startblock)) {
			adjust = prevdiff = ap->offset -
				(ap->prev.br_startoff +
				 ap->prev.br_blockcount);
			if (prevdiff <= XFS_ALLOC_GAP_UNITS * ap->length &&
			    ISVALID(prevbno + prevdiff,
				    ap->prev.br_startblock))
				prevbno += adjust;
			else
				prevdiff += adjust;
			if (!rt && !nullfb &&
			    XFS_FSB_TO_AGNO(mp, prevbno) != fb_agno)
				prevbno = NULLFSBLOCK;
		}
		else
			prevbno = NULLFSBLOCK;
		if (!isnullstartblock(ap->got.br_startblock)) {
			adjust = gotdiff = ap->got.br_startoff - ap->offset;
			gotbno = ap->got.br_startblock;
			if (gotdiff <= XFS_ALLOC_GAP_UNITS * ap->length &&
			    ISVALID(gotbno - gotdiff, gotbno))
				gotbno -= adjust;
			else if (ISVALID(gotbno - ap->length, gotbno)) {
				gotbno -= ap->length;
				gotdiff += adjust - ap->length;
			} else
				gotdiff += adjust;
			if (!rt && !nullfb &&
			    XFS_FSB_TO_AGNO(mp, gotbno) != fb_agno)
				gotbno = NULLFSBLOCK;
		}
		else
			gotbno = NULLFSBLOCK;
		if (prevbno != NULLFSBLOCK && gotbno != NULLFSBLOCK)
			ap->blkno = prevdiff <= gotdiff ? prevbno : gotbno;
		else if (prevbno != NULLFSBLOCK)
			ap->blkno = prevbno;
		else if (gotbno != NULLFSBLOCK)
			ap->blkno = gotbno;
	}
#undef ISVALID
}

STATIC int
xfs_bmap_rtalloc(
	xfs_bmalloca_t	*ap)		
{
	xfs_alloctype_t	atype = 0;	
	int		error;		
	xfs_mount_t	*mp;		
	xfs_extlen_t	prod = 0;	
	xfs_extlen_t	ralen = 0;	
	xfs_extlen_t	align;		
	xfs_rtblock_t	rtb;

	mp = ap->ip->i_mount;
	align = xfs_get_extsz_hint(ap->ip);
	prod = align / mp->m_sb.sb_rextsize;
	error = xfs_bmap_extsize_align(mp, &ap->got, &ap->prev,
					align, 1, ap->eof, 0,
					ap->conv, &ap->offset, &ap->length);
	if (error)
		return error;
	ASSERT(ap->length);
	ASSERT(ap->length % mp->m_sb.sb_rextsize == 0);

	if (do_mod(ap->offset, align) || ap->length % align)
		prod = 1;
	ralen = ap->length / mp->m_sb.sb_rextsize;
	if (ralen * mp->m_sb.sb_rextsize >= MAXEXTLEN)
		ralen = MAXEXTLEN / mp->m_sb.sb_rextsize;

	xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(ap->tp, mp->m_rbmip, XFS_ILOCK_EXCL);

	if (ap->eof && ap->offset == 0) {
		xfs_rtblock_t uninitialized_var(rtx); 

		error = xfs_rtpick_extent(mp, ap->tp, ralen, &rtx);
		if (error)
			return error;
		ap->blkno = rtx * mp->m_sb.sb_rextsize;
	} else {
		ap->blkno = 0;
	}

	xfs_bmap_adjacent(ap);

	atype = ap->blkno == 0 ?  XFS_ALLOCTYPE_ANY_AG : XFS_ALLOCTYPE_NEAR_BNO;
	do_div(ap->blkno, mp->m_sb.sb_rextsize);
	rtb = ap->blkno;
	ap->length = ralen;
	if ((error = xfs_rtallocate_extent(ap->tp, ap->blkno, 1, ap->length,
				&ralen, atype, ap->wasdel, prod, &rtb)))
		return error;
	if (rtb == NULLFSBLOCK && prod > 1 &&
	    (error = xfs_rtallocate_extent(ap->tp, ap->blkno, 1,
					   ap->length, &ralen, atype,
					   ap->wasdel, 1, &rtb)))
		return error;
	ap->blkno = rtb;
	if (ap->blkno != NULLFSBLOCK) {
		ap->blkno *= mp->m_sb.sb_rextsize;
		ralen *= mp->m_sb.sb_rextsize;
		ap->length = ralen;
		ap->ip->i_d.di_nblocks += ralen;
		xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= ralen;
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
			ap->wasdel ? XFS_TRANS_DQ_DELRTBCOUNT :
					XFS_TRANS_DQ_RTBCOUNT, (long) ralen);
	} else {
		ap->length = 0;
	}
	return 0;
}

STATIC int
xfs_bmap_btalloc_nullfb(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	xfs_extlen_t		*blen)
{
	struct xfs_mount	*mp = ap->ip->i_mount;
	struct xfs_perag	*pag;
	xfs_agnumber_t		ag, startag;
	int			notinit = 0;
	int			error;

	if (ap->userdata && xfs_inode_is_filestream(ap->ip))
		args->type = XFS_ALLOCTYPE_NEAR_BNO;
	else
		args->type = XFS_ALLOCTYPE_START_BNO;
	args->total = ap->total;

	startag = ag = XFS_FSB_TO_AGNO(mp, args->fsbno);
	if (startag == NULLAGNUMBER)
		startag = ag = 0;

	pag = xfs_perag_get(mp, ag);
	while (*blen < args->maxlen) {
		if (!pag->pagf_init) {
			error = xfs_alloc_pagf_init(mp, args->tp, ag,
						    XFS_ALLOC_FLAG_TRYLOCK);
			if (error) {
				xfs_perag_put(pag);
				return error;
			}
		}

		if (pag->pagf_init) {
			xfs_extlen_t	longest;
			longest = xfs_alloc_longest_free_extent(mp, pag);
			if (*blen < longest)
				*blen = longest;
		} else
			notinit = 1;

		if (xfs_inode_is_filestream(ap->ip)) {
			if (*blen >= args->maxlen)
				break;

			if (ap->userdata) {
				if (startag == NULLAGNUMBER)
					break;

				error = xfs_filestream_new_ag(ap, &ag);
				xfs_perag_put(pag);
				if (error)
					return error;

				
				startag = NULLAGNUMBER;
				pag = xfs_perag_get(mp, ag);
				continue;
			}
		}
		if (++ag == mp->m_sb.sb_agcount)
			ag = 0;
		if (ag == startag)
			break;
		xfs_perag_put(pag);
		pag = xfs_perag_get(mp, ag);
	}
	xfs_perag_put(pag);

	if (notinit || *blen < ap->minlen)
		args->minlen = ap->minlen;
	else if (*blen < args->maxlen)
		args->minlen = *blen;
	else
		args->minlen = args->maxlen;

	if (xfs_inode_is_filestream(ap->ip))
		ap->blkno = args->fsbno = XFS_AGB_TO_FSB(mp, ag, 0);

	return 0;
}

STATIC int
xfs_bmap_btalloc(
	xfs_bmalloca_t	*ap)		
{
	xfs_mount_t	*mp;		
	xfs_alloctype_t	atype = 0;	
	xfs_extlen_t	align;		
	xfs_agnumber_t	fb_agno;	
	xfs_agnumber_t	ag;
	xfs_alloc_arg_t	args;
	xfs_extlen_t	blen;
	xfs_extlen_t	nextminlen = 0;
	int		nullfb;		
	int		isaligned;
	int		tryagain;
	int		error;

	ASSERT(ap->length);

	mp = ap->ip->i_mount;
	align = ap->userdata ? xfs_get_extsz_hint(ap->ip) : 0;
	if (unlikely(align)) {
		error = xfs_bmap_extsize_align(mp, &ap->got, &ap->prev,
						align, 0, ap->eof, 0, ap->conv,
						&ap->offset, &ap->length);
		ASSERT(!error);
		ASSERT(ap->length);
	}
	nullfb = *ap->firstblock == NULLFSBLOCK;
	fb_agno = nullfb ? NULLAGNUMBER : XFS_FSB_TO_AGNO(mp, *ap->firstblock);
	if (nullfb) {
		if (ap->userdata && xfs_inode_is_filestream(ap->ip)) {
			ag = xfs_filestream_lookup_ag(ap->ip);
			ag = (ag != NULLAGNUMBER) ? ag : 0;
			ap->blkno = XFS_AGB_TO_FSB(mp, ag, 0);
		} else {
			ap->blkno = XFS_INO_TO_FSB(mp, ap->ip->i_ino);
		}
	} else
		ap->blkno = *ap->firstblock;

	xfs_bmap_adjacent(ap);

	if (nullfb || XFS_FSB_TO_AGNO(mp, ap->blkno) == fb_agno)
		;
	else
		ap->blkno = *ap->firstblock;
	tryagain = isaligned = 0;
	args.tp = ap->tp;
	args.mp = mp;
	args.fsbno = ap->blkno;

	
	args.maxlen = MIN(ap->length, XFS_ALLOC_AG_MAX_USABLE(mp));
	args.firstblock = *ap->firstblock;
	blen = 0;
	if (nullfb) {
		error = xfs_bmap_btalloc_nullfb(ap, &args, &blen);
		if (error)
			return error;
	} else if (ap->flist->xbf_low) {
		if (xfs_inode_is_filestream(ap->ip))
			args.type = XFS_ALLOCTYPE_FIRST_AG;
		else
			args.type = XFS_ALLOCTYPE_START_BNO;
		args.total = args.minlen = ap->minlen;
	} else {
		args.type = XFS_ALLOCTYPE_NEAR_BNO;
		args.total = ap->total;
		args.minlen = ap->minlen;
	}
	
	if (unlikely(align)) {
		args.prod = align;
		if ((args.mod = (xfs_extlen_t)do_mod(ap->offset, args.prod)))
			args.mod = (xfs_extlen_t)(args.prod - args.mod);
	} else if (mp->m_sb.sb_blocksize >= PAGE_CACHE_SIZE) {
		args.prod = 1;
		args.mod = 0;
	} else {
		args.prod = PAGE_CACHE_SIZE >> mp->m_sb.sb_blocklog;
		if ((args.mod = (xfs_extlen_t)(do_mod(ap->offset, args.prod))))
			args.mod = (xfs_extlen_t)(args.prod - args.mod);
	}
	if (!ap->flist->xbf_low && ap->aeof) {
		if (!ap->offset) {
			args.alignment = mp->m_dalign;
			atype = args.type;
			isaligned = 1;
			if (blen > args.alignment && blen <= args.maxlen)
				args.minlen = blen - args.alignment;
			args.minalignslop = 0;
		} else {
			atype = args.type;
			tryagain = 1;
			args.type = XFS_ALLOCTYPE_THIS_BNO;
			args.alignment = 1;
			if (blen > mp->m_dalign && blen <= args.maxlen)
				nextminlen = blen - mp->m_dalign;
			else
				nextminlen = args.minlen;
			if (nextminlen + mp->m_dalign > args.minlen + 1)
				args.minalignslop =
					nextminlen + mp->m_dalign -
					args.minlen - 1;
			else
				args.minalignslop = 0;
		}
	} else {
		args.alignment = 1;
		args.minalignslop = 0;
	}
	args.minleft = ap->minleft;
	args.wasdel = ap->wasdel;
	args.isfl = 0;
	args.userdata = ap->userdata;
	if ((error = xfs_alloc_vextent(&args)))
		return error;
	if (tryagain && args.fsbno == NULLFSBLOCK) {
		args.type = atype;
		args.fsbno = ap->blkno;
		args.alignment = mp->m_dalign;
		args.minlen = nextminlen;
		args.minalignslop = 0;
		isaligned = 1;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (isaligned && args.fsbno == NULLFSBLOCK) {
		args.type = atype;
		args.fsbno = ap->blkno;
		args.alignment = 0;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (args.fsbno == NULLFSBLOCK && nullfb &&
	    args.minlen > ap->minlen) {
		args.minlen = ap->minlen;
		args.type = XFS_ALLOCTYPE_START_BNO;
		args.fsbno = ap->blkno;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (args.fsbno == NULLFSBLOCK && nullfb) {
		args.fsbno = 0;
		args.type = XFS_ALLOCTYPE_FIRST_AG;
		args.total = ap->minlen;
		args.minleft = 0;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
		ap->flist->xbf_low = 1;
	}
	if (args.fsbno != NULLFSBLOCK) {
		ASSERT(*ap->firstblock == NULLFSBLOCK ||
		       XFS_FSB_TO_AGNO(mp, *ap->firstblock) ==
		       XFS_FSB_TO_AGNO(mp, args.fsbno) ||
		       (ap->flist->xbf_low &&
			XFS_FSB_TO_AGNO(mp, *ap->firstblock) <
			XFS_FSB_TO_AGNO(mp, args.fsbno)));

		ap->blkno = args.fsbno;
		if (*ap->firstblock == NULLFSBLOCK)
			*ap->firstblock = args.fsbno;
		ASSERT(nullfb || fb_agno == args.agno ||
		       (ap->flist->xbf_low && fb_agno < args.agno));
		ap->length = args.len;
		ap->ip->i_d.di_nblocks += args.len;
		xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= args.len;
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
			ap->wasdel ? XFS_TRANS_DQ_DELBCOUNT :
					XFS_TRANS_DQ_BCOUNT,
			(long) args.len);
	} else {
		ap->blkno = NULLFSBLOCK;
		ap->length = 0;
	}
	return 0;
}

STATIC int
xfs_bmap_alloc(
	xfs_bmalloca_t	*ap)		
{
	if (XFS_IS_REALTIME_INODE(ap->ip) && ap->userdata)
		return xfs_bmap_rtalloc(ap);
	return xfs_bmap_btalloc(ap);
}

STATIC int				
xfs_bmap_btree_to_extents(
	xfs_trans_t		*tp,	
	xfs_inode_t		*ip,	
	xfs_btree_cur_t		*cur,	
	int			*logflagsp, 
	int			whichfork)  
{
	
	struct xfs_btree_block	*cblock;
	xfs_fsblock_t		cbno;	
	xfs_buf_t		*cbp;	
	int			error;	
	xfs_ifork_t		*ifp;	
	xfs_mount_t		*mp;	
	__be64			*pp;	
	struct xfs_btree_block	*rblock;

	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(ifp->if_flags & XFS_IFEXTENTS);
	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE);
	rblock = ifp->if_broot;
	ASSERT(be16_to_cpu(rblock->bb_level) == 1);
	ASSERT(be16_to_cpu(rblock->bb_numrecs) == 1);
	ASSERT(xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0) == 1);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, rblock, 1, ifp->if_broot_bytes);
	cbno = be64_to_cpu(*pp);
	*logflagsp = 0;
#ifdef DEBUG
	if ((error = xfs_btree_check_lptr(cur, cbno, 1)))
		return error;
#endif
	if ((error = xfs_btree_read_bufl(mp, tp, cbno, 0, &cbp,
			XFS_BMAP_BTREE_REF)))
		return error;
	cblock = XFS_BUF_TO_BLOCK(cbp);
	if ((error = xfs_btree_check_block(cur, cblock, 0, cbp)))
		return error;
	xfs_bmap_add_free(cbno, 1, cur->bc_private.b.flist, mp);
	ip->i_d.di_nblocks--;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, -1L);
	xfs_trans_binval(tp, cbp);
	if (cur->bc_bufs[0] == cbp)
		cur->bc_bufs[0] = NULL;
	xfs_iroot_realloc(ip, -1, whichfork);
	ASSERT(ifp->if_broot == NULL);
	ASSERT((ifp->if_flags & XFS_IFBROOT) == 0);
	XFS_IFORK_FMT_SET(ip, whichfork, XFS_DINODE_FMT_EXTENTS);
	*logflagsp = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
	return 0;
}

STATIC int				
xfs_bmap_del_extent(
	xfs_inode_t		*ip,	
	xfs_trans_t		*tp,	
	xfs_extnum_t		*idx,	
	xfs_bmap_free_t		*flist,	
	xfs_btree_cur_t		*cur,	
	xfs_bmbt_irec_t		*del,	
	int			*logflagsp, 
	int			whichfork) 
{
	xfs_filblks_t		da_new;	
	xfs_filblks_t		da_old;	
	xfs_fsblock_t		del_endblock=0;	
	xfs_fileoff_t		del_endoff;	
	int			delay;	
	int			do_fx;	
	xfs_bmbt_rec_host_t	*ep;	
	int			error;	
	int			flags;	
	xfs_bmbt_irec_t		got;	
	xfs_fileoff_t		got_endoff;	
	int			i;	
	xfs_ifork_t		*ifp;	
	xfs_mount_t		*mp;	
	xfs_filblks_t		nblks;	
	xfs_bmbt_irec_t		new;	
	
	uint			qfield;	
	xfs_filblks_t		temp;	
	xfs_filblks_t		temp2;	
	int			state = 0;

	XFS_STATS_INC(xs_del_exlist);

	if (whichfork == XFS_ATTR_FORK)
		state |= BMAP_ATTRFORK;

	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT((*idx >= 0) && (*idx < ifp->if_bytes /
		(uint)sizeof(xfs_bmbt_rec_t)));
	ASSERT(del->br_blockcount > 0);
	ep = xfs_iext_get_ext(ifp, *idx);
	xfs_bmbt_get_all(ep, &got);
	ASSERT(got.br_startoff <= del->br_startoff);
	del_endoff = del->br_startoff + del->br_blockcount;
	got_endoff = got.br_startoff + got.br_blockcount;
	ASSERT(got_endoff >= del_endoff);
	delay = isnullstartblock(got.br_startblock);
	ASSERT(isnullstartblock(del->br_startblock) == delay);
	flags = 0;
	qfield = 0;
	error = 0;
	if (!delay) {
		flags = XFS_ILOG_CORE;
		if (whichfork == XFS_DATA_FORK && XFS_IS_REALTIME_INODE(ip)) {
			xfs_fsblock_t	bno;
			xfs_filblks_t	len;

			ASSERT(do_mod(del->br_blockcount,
				      mp->m_sb.sb_rextsize) == 0);
			ASSERT(do_mod(del->br_startblock,
				      mp->m_sb.sb_rextsize) == 0);
			bno = del->br_startblock;
			len = del->br_blockcount;
			do_div(bno, mp->m_sb.sb_rextsize);
			do_div(len, mp->m_sb.sb_rextsize);
			error = xfs_rtfree_extent(tp, bno, (xfs_extlen_t)len);
			if (error)
				goto done;
			do_fx = 0;
			nblks = len * mp->m_sb.sb_rextsize;
			qfield = XFS_TRANS_DQ_RTBCOUNT;
		}
		else {
			do_fx = 1;
			nblks = del->br_blockcount;
			qfield = XFS_TRANS_DQ_BCOUNT;
		}
		del_endblock = del->br_startblock + del->br_blockcount;
		if (cur) {
			if ((error = xfs_bmbt_lookup_eq(cur, got.br_startoff,
					got.br_startblock, got.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		da_old = da_new = 0;
	} else {
		da_old = startblockval(got.br_startblock);
		da_new = 0;
		nblks = 0;
		do_fx = 0;
	}
	switch (((got.br_startoff == del->br_startoff) << 1) |
		(got_endoff == del_endoff)) {
	case 3:
		xfs_iext_remove(ip, *idx, 1,
				whichfork == XFS_ATTR_FORK ? BMAP_ATTRFORK : 0);
		--*idx;
		if (delay)
			break;

		XFS_IFORK_NEXT_SET(ip, whichfork,
			XFS_IFORK_NEXTENTS(ip, whichfork) - 1);
		flags |= XFS_ILOG_CORE;
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_btree_delete(cur, &i)))
			goto done;
		XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		break;

	case 2:
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_startoff(ep, del_endoff);
		temp = got.br_blockcount - del->br_blockcount;
		xfs_bmbt_set_blockcount(ep, temp);
		if (delay) {
			temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
				da_old);
			xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
			trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
			da_new = temp;
			break;
		}
		xfs_bmbt_set_startblock(ep, del_endblock);
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_bmbt_update(cur, del_endoff, del_endblock,
				got.br_blockcount - del->br_blockcount,
				got.br_state)))
			goto done;
		break;

	case 1:
		temp = got.br_blockcount - del->br_blockcount;
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);
		if (delay) {
			temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
				da_old);
			xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
			trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
			da_new = temp;
			break;
		}
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_bmbt_update(cur, got.br_startoff,
				got.br_startblock,
				got.br_blockcount - del->br_blockcount,
				got.br_state)))
			goto done;
		break;

	case 0:
		temp = del->br_startoff - got.br_startoff;
		trace_xfs_bmap_pre_update(ip, *idx, state, _THIS_IP_);
		xfs_bmbt_set_blockcount(ep, temp);
		new.br_startoff = del_endoff;
		temp2 = got_endoff - del_endoff;
		new.br_blockcount = temp2;
		new.br_state = got.br_state;
		if (!delay) {
			new.br_startblock = del_endblock;
			flags |= XFS_ILOG_CORE;
			if (cur) {
				if ((error = xfs_bmbt_update(cur,
						got.br_startoff,
						got.br_startblock, temp,
						got.br_state)))
					goto done;
				if ((error = xfs_btree_increment(cur, 0, &i)))
					goto done;
				cur->bc_rec.b = new;
				error = xfs_btree_insert(cur, &i);
				if (error && error != ENOSPC)
					goto done;
				if (error == ENOSPC) {
					if ((error = xfs_bmbt_lookup_eq(cur,
							got.br_startoff,
							got.br_startblock,
							temp, &i)))
						goto done;
					XFS_WANT_CORRUPTED_GOTO(i == 1, done);
					if ((error = xfs_bmbt_update(cur,
							got.br_startoff,
							got.br_startblock,
							got.br_blockcount,
							got.br_state)))
						goto done;
					xfs_bmbt_set_blockcount(ep,
						got.br_blockcount);
					flags = 0;
					error = XFS_ERROR(ENOSPC);
					goto done;
				}
				XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			} else
				flags |= xfs_ilog_fext(whichfork);
			XFS_IFORK_NEXT_SET(ip, whichfork,
				XFS_IFORK_NEXTENTS(ip, whichfork) + 1);
		} else {
			ASSERT(whichfork == XFS_DATA_FORK);
			temp = xfs_bmap_worst_indlen(ip, temp);
			xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
			temp2 = xfs_bmap_worst_indlen(ip, temp2);
			new.br_startblock = nullstartblock((int)temp2);
			da_new = temp + temp2;
			while (da_new > da_old) {
				if (temp) {
					temp--;
					da_new--;
					xfs_bmbt_set_startblock(ep,
						nullstartblock((int)temp));
				}
				if (da_new == da_old)
					break;
				if (temp2) {
					temp2--;
					da_new--;
					new.br_startblock =
						nullstartblock((int)temp2);
				}
			}
		}
		trace_xfs_bmap_post_update(ip, *idx, state, _THIS_IP_);
		xfs_iext_insert(ip, *idx + 1, 1, &new, state);
		++*idx;
		break;
	}
	if (do_fx)
		xfs_bmap_add_free(del->br_startblock, del->br_blockcount, flist,
			mp);
	if (nblks)
		ip->i_d.di_nblocks -= nblks;
	if (qfield)
		xfs_trans_mod_dquot_byino(tp, ip, qfield, (long)-nblks);

	ASSERT(da_old >= da_new);
	if (da_old > da_new) {
		xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS,
			(int64_t)(da_old - da_new), 0);
	}
done:
	*logflagsp = flags;
	return error;
}

STATIC void
xfs_bmap_del_free(
	xfs_bmap_free_t		*flist,	
	xfs_bmap_free_item_t	*prev,	
	xfs_bmap_free_item_t	*free)	
{
	if (prev)
		prev->xbfi_next = free->xbfi_next;
	else
		flist->xbf_first = free->xbfi_next;
	flist->xbf_count--;
	kmem_zone_free(xfs_bmap_free_item_zone, free);
}

STATIC int					
xfs_bmap_extents_to_btree(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	xfs_btree_cur_t		**curp,		
	int			wasdel,		
	int			*logflagsp,	
	int			whichfork)	
{
	struct xfs_btree_block	*ablock;	
	xfs_buf_t		*abp;		
	xfs_alloc_arg_t		args;		
	xfs_bmbt_rec_t		*arp;		
	struct xfs_btree_block	*block;		
	xfs_btree_cur_t		*cur;		
	xfs_bmbt_rec_host_t	*ep;		
	int			error;		
	xfs_extnum_t		i, cnt;		
	xfs_ifork_t		*ifp;		
	xfs_bmbt_key_t		*kp;		
	xfs_mount_t		*mp;		
	xfs_extnum_t		nextents;	
	xfs_bmbt_ptr_t		*pp;		

	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS);

	xfs_iroot_realloc(ip, 1, whichfork);
	ifp->if_flags |= XFS_IFBROOT;

	block = ifp->if_broot;
	block->bb_magic = cpu_to_be32(XFS_BMAP_MAGIC);
	block->bb_level = cpu_to_be16(1);
	block->bb_numrecs = cpu_to_be16(1);
	block->bb_u.l.bb_leftsib = cpu_to_be64(NULLDFSBNO);
	block->bb_u.l.bb_rightsib = cpu_to_be64(NULLDFSBNO);

	mp = ip->i_mount;
	cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
	cur->bc_private.b.firstblock = *firstblock;
	cur->bc_private.b.flist = flist;
	cur->bc_private.b.flags = wasdel ? XFS_BTCUR_BPRV_WASDEL : 0;
	XFS_IFORK_FMT_SET(ip, whichfork, XFS_DINODE_FMT_BTREE);
	args.tp = tp;
	args.mp = mp;
	args.firstblock = *firstblock;
	if (*firstblock == NULLFSBLOCK) {
		args.type = XFS_ALLOCTYPE_START_BNO;
		args.fsbno = XFS_INO_TO_FSB(mp, ip->i_ino);
	} else if (flist->xbf_low) {
		args.type = XFS_ALLOCTYPE_START_BNO;
		args.fsbno = *firstblock;
	} else {
		args.type = XFS_ALLOCTYPE_NEAR_BNO;
		args.fsbno = *firstblock;
	}
	args.minlen = args.maxlen = args.prod = 1;
	args.total = args.minleft = args.alignment = args.mod = args.isfl =
		args.minalignslop = 0;
	args.wasdel = wasdel;
	*logflagsp = 0;
	if ((error = xfs_alloc_vextent(&args))) {
		xfs_iroot_realloc(ip, -1, whichfork);
		xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
		return error;
	}
	ASSERT(args.fsbno != NULLFSBLOCK);
	ASSERT(*firstblock == NULLFSBLOCK ||
	       args.agno == XFS_FSB_TO_AGNO(mp, *firstblock) ||
	       (flist->xbf_low &&
		args.agno > XFS_FSB_TO_AGNO(mp, *firstblock)));
	*firstblock = cur->bc_private.b.firstblock = args.fsbno;
	cur->bc_private.b.allocated++;
	ip->i_d.di_nblocks++;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, 1L);
	abp = xfs_btree_get_bufl(mp, tp, args.fsbno, 0);
	ablock = XFS_BUF_TO_BLOCK(abp);
	ablock->bb_magic = cpu_to_be32(XFS_BMAP_MAGIC);
	ablock->bb_level = 0;
	ablock->bb_u.l.bb_leftsib = cpu_to_be64(NULLDFSBNO);
	ablock->bb_u.l.bb_rightsib = cpu_to_be64(NULLDFSBNO);
	arp = XFS_BMBT_REC_ADDR(mp, ablock, 1);
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	for (cnt = i = 0; i < nextents; i++) {
		ep = xfs_iext_get_ext(ifp, i);
		if (!isnullstartblock(xfs_bmbt_get_startblock(ep))) {
			arp->l0 = cpu_to_be64(ep->l0);
			arp->l1 = cpu_to_be64(ep->l1);
			arp++; cnt++;
		}
	}
	ASSERT(cnt == XFS_IFORK_NEXTENTS(ip, whichfork));
	xfs_btree_set_numrecs(ablock, cnt);

	kp = XFS_BMBT_KEY_ADDR(mp, block, 1);
	arp = XFS_BMBT_REC_ADDR(mp, ablock, 1);
	kp->br_startoff = cpu_to_be64(xfs_bmbt_disk_get_startoff(arp));
	pp = XFS_BMBT_PTR_ADDR(mp, block, 1, xfs_bmbt_get_maxrecs(cur,
						be16_to_cpu(block->bb_level)));
	*pp = cpu_to_be64(args.fsbno);

	xfs_btree_log_block(cur, abp, XFS_BB_ALL_BITS);
	xfs_btree_log_recs(cur, abp, 1, be16_to_cpu(ablock->bb_numrecs));
	ASSERT(*curp == NULL);
	*curp = cur;
	*logflagsp = XFS_ILOG_CORE | xfs_ilog_fbroot(whichfork);
	return 0;
}

uint
xfs_default_attroffset(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	uint			offset;

	if (mp->m_sb.sb_inodesize == 256) {
		offset = XFS_LITINO(mp) -
				XFS_BMDR_SPACE_CALC(MINABTPTRS);
	} else {
		offset = XFS_BMDR_SPACE_CALC(6 * MINABTPTRS);
	}

	ASSERT(offset < XFS_LITINO(mp));
	return offset;
}

STATIC void
xfs_bmap_forkoff_reset(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	int		whichfork)
{
	if (whichfork == XFS_ATTR_FORK &&
	    ip->i_d.di_format != XFS_DINODE_FMT_DEV &&
	    ip->i_d.di_format != XFS_DINODE_FMT_UUID &&
	    ip->i_d.di_format != XFS_DINODE_FMT_BTREE) {
		uint	dfl_forkoff = xfs_default_attroffset(ip) >> 3;

		if (dfl_forkoff > ip->i_d.di_forkoff)
			ip->i_d.di_forkoff = dfl_forkoff;
	}
}

STATIC int				
xfs_bmap_local_to_extents(
	xfs_trans_t	*tp,		
	xfs_inode_t	*ip,		
	xfs_fsblock_t	*firstblock,	
	xfs_extlen_t	total,		
	int		*logflagsp,	
	int		whichfork)	
{
	int		error;		
	int		flags;		
	xfs_ifork_t	*ifp;		

	ASSERT(!(S_ISREG(ip->i_d.di_mode) && whichfork == XFS_DATA_FORK));
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL);
	flags = 0;
	error = 0;
	if (ifp->if_bytes) {
		xfs_alloc_arg_t	args;	
		xfs_buf_t	*bp;	
		xfs_bmbt_rec_host_t *ep;

		args.tp = tp;
		args.mp = ip->i_mount;
		args.firstblock = *firstblock;
		ASSERT((ifp->if_flags &
			(XFS_IFINLINE|XFS_IFEXTENTS|XFS_IFEXTIREC)) == XFS_IFINLINE);
		if (*firstblock == NULLFSBLOCK) {
			args.fsbno = XFS_INO_TO_FSB(args.mp, ip->i_ino);
			args.type = XFS_ALLOCTYPE_START_BNO;
		} else {
			args.fsbno = *firstblock;
			args.type = XFS_ALLOCTYPE_NEAR_BNO;
		}
		args.total = total;
		args.mod = args.minleft = args.alignment = args.wasdel =
			args.isfl = args.minalignslop = 0;
		args.minlen = args.maxlen = args.prod = 1;
		if ((error = xfs_alloc_vextent(&args)))
			goto done;
		ASSERT(args.fsbno != NULLFSBLOCK);
		ASSERT(args.len == 1);
		*firstblock = args.fsbno;
		bp = xfs_btree_get_bufl(args.mp, tp, args.fsbno, 0);
		memcpy(bp->b_addr, ifp->if_u1.if_data, ifp->if_bytes);
		xfs_trans_log_buf(tp, bp, 0, ifp->if_bytes - 1);
		xfs_bmap_forkoff_reset(args.mp, ip, whichfork);
		xfs_idata_realloc(ip, -ifp->if_bytes, whichfork);
		xfs_iext_add(ifp, 0, 1);
		ep = xfs_iext_get_ext(ifp, 0);
		xfs_bmbt_set_allf(ep, 0, args.fsbno, 1, XFS_EXT_NORM);
		trace_xfs_bmap_post_update(ip, 0,
				whichfork == XFS_ATTR_FORK ? BMAP_ATTRFORK : 0,
				_THIS_IP_);
		XFS_IFORK_NEXT_SET(ip, whichfork, 1);
		ip->i_d.di_nblocks = 1;
		xfs_trans_mod_dquot_byino(tp, ip,
			XFS_TRANS_DQ_BCOUNT, 1L);
		flags |= xfs_ilog_fext(whichfork);
	} else {
		ASSERT(XFS_IFORK_NEXTENTS(ip, whichfork) == 0);
		xfs_bmap_forkoff_reset(ip->i_mount, ip, whichfork);
	}
	ifp->if_flags &= ~XFS_IFINLINE;
	ifp->if_flags |= XFS_IFEXTENTS;
	XFS_IFORK_FMT_SET(ip, whichfork, XFS_DINODE_FMT_EXTENTS);
	flags |= XFS_ILOG_CORE;
done:
	*logflagsp = flags;
	return error;
}

STATIC xfs_bmbt_rec_host_t *		
xfs_bmap_search_multi_extents(
	xfs_ifork_t	*ifp,		
	xfs_fileoff_t	bno,		
	int		*eofp,		
	xfs_extnum_t	*lastxp,	
	xfs_bmbt_irec_t	*gotp,		
	xfs_bmbt_irec_t	*prevp)		
{
	xfs_bmbt_rec_host_t *ep;		
	xfs_extnum_t	lastx;		

	gotp->br_startoff = 0xffa5a5a5a5a5a5a5LL;
	gotp->br_blockcount = 0xa55a5a5a5a5a5a5aLL;
	gotp->br_state = XFS_EXT_INVALID;
#if XFS_BIG_BLKNOS
	gotp->br_startblock = 0xffffa5a5a5a5a5a5LL;
#else
	gotp->br_startblock = 0xffffa5a5;
#endif
	prevp->br_startoff = NULLFILEOFF;

	ep = xfs_iext_bno_to_ext(ifp, bno, &lastx);
	if (lastx > 0) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, lastx - 1), prevp);
	}
	if (lastx < (ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t))) {
		xfs_bmbt_get_all(ep, gotp);
		*eofp = 0;
	} else {
		if (lastx > 0) {
			*gotp = *prevp;
		}
		*eofp = 1;
		ep = NULL;
	}
	*lastxp = lastx;
	return ep;
}

STATIC xfs_bmbt_rec_host_t *                 
xfs_bmap_search_extents(
	xfs_inode_t     *ip,            
	xfs_fileoff_t   bno,            
	int             fork,      	
	int             *eofp,          
	xfs_extnum_t    *lastxp,        
	xfs_bmbt_irec_t *gotp,          
	xfs_bmbt_irec_t *prevp)         
{
	xfs_ifork_t	*ifp;		
	xfs_bmbt_rec_host_t  *ep;            

	XFS_STATS_INC(xs_look_exlist);
	ifp = XFS_IFORK_PTR(ip, fork);

	ep = xfs_bmap_search_multi_extents(ifp, bno, eofp, lastxp, gotp, prevp);

	if (unlikely(!(gotp->br_startblock) && (*lastxp != NULLEXTNUM) &&
		     !(XFS_IS_REALTIME_INODE(ip) && fork == XFS_DATA_FORK))) {
		xfs_alert_tag(ip->i_mount, XFS_PTAG_FSBLOCK_ZERO,
				"Access to block zero in inode %llu "
				"start_block: %llx start_off: %llx "
				"blkcnt: %llx extent-state: %x lastx: %x\n",
			(unsigned long long)ip->i_ino,
			(unsigned long long)gotp->br_startblock,
			(unsigned long long)gotp->br_startoff,
			(unsigned long long)gotp->br_blockcount,
			gotp->br_state, *lastxp);
		*lastxp = NULLEXTNUM;
		*eofp = 1;
		return NULL;
	}
	return ep;
}

STATIC xfs_filblks_t
xfs_bmap_worst_indlen(
	xfs_inode_t	*ip,		
	xfs_filblks_t	len)		
{
	int		level;		
	int		maxrecs;	
	xfs_mount_t	*mp;		
	xfs_filblks_t	rval;		

	mp = ip->i_mount;
	maxrecs = mp->m_bmap_dmxr[0];
	for (level = 0, rval = 0;
	     level < XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK);
	     level++) {
		len += maxrecs - 1;
		do_div(len, maxrecs);
		rval += len;
		if (len == 1)
			return rval + XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) -
				level - 1;
		if (level == 0)
			maxrecs = mp->m_bmap_dmxr[1];
	}
	return rval;
}

int						
xfs_bmap_add_attrfork(
	xfs_inode_t		*ip,		
	int			size,		
	int			rsvd)		
{
	xfs_fsblock_t		firstblock;	
	xfs_bmap_free_t		flist;		
	xfs_mount_t		*mp;		
	xfs_trans_t		*tp;		
	int			blks;		
	int			version = 1;	
	int			committed;	
	int			logflags;	
	int			error;		

	ASSERT(XFS_IFORK_Q(ip) == 0);

	mp = ip->i_mount;
	ASSERT(!XFS_NOT_DQATTACHED(mp, ip));
	tp = xfs_trans_alloc(mp, XFS_TRANS_ADDAFORK);
	blks = XFS_ADDAFORK_SPACE_RES(mp);
	if (rsvd)
		tp->t_flags |= XFS_TRANS_RESERVE;
	if ((error = xfs_trans_reserve(tp, blks, XFS_ADDAFORK_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_ADDAFORK_LOG_COUNT)))
		goto error0;
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_trans_reserve_quota_nblks(tp, ip, blks, 0, rsvd ?
			XFS_QMOPT_RES_REGBLKS | XFS_QMOPT_FORCE_RES :
			XFS_QMOPT_RES_REGBLKS);
	if (error) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
		return error;
	}
	if (XFS_IFORK_Q(ip))
		goto error1;
	if (ip->i_d.di_aformat != XFS_DINODE_FMT_EXTENTS) {
		ASSERT(ip->i_d.di_aformat == 0);
		ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;
	}
	ASSERT(ip->i_d.di_anextents == 0);

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_DEV:
		ip->i_d.di_forkoff = roundup(sizeof(xfs_dev_t), 8) >> 3;
		break;
	case XFS_DINODE_FMT_UUID:
		ip->i_d.di_forkoff = roundup(sizeof(uuid_t), 8) >> 3;
		break;
	case XFS_DINODE_FMT_LOCAL:
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		ip->i_d.di_forkoff = xfs_attr_shortform_bytesfit(ip, size);
		if (!ip->i_d.di_forkoff)
			ip->i_d.di_forkoff = xfs_default_attroffset(ip) >> 3;
		else if (mp->m_flags & XFS_MOUNT_ATTR2)
			version = 2;
		break;
	default:
		ASSERT(0);
		error = XFS_ERROR(EINVAL);
		goto error1;
	}

	ASSERT(ip->i_afp == NULL);
	ip->i_afp = kmem_zone_zalloc(xfs_ifork_zone, KM_SLEEP);
	ip->i_afp->if_flags = XFS_IFEXTENTS;
	logflags = 0;
	xfs_bmap_init(&flist, &firstblock);
	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_LOCAL:
		error = xfs_bmap_add_attrfork_local(tp, ip, &firstblock, &flist,
			&logflags);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		error = xfs_bmap_add_attrfork_extents(tp, ip, &firstblock,
			&flist, &logflags);
		break;
	case XFS_DINODE_FMT_BTREE:
		error = xfs_bmap_add_attrfork_btree(tp, ip, &firstblock, &flist,
			&logflags);
		break;
	default:
		error = 0;
		break;
	}
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	if (error)
		goto error2;
	if (!xfs_sb_version_hasattr(&mp->m_sb) ||
	   (!xfs_sb_version_hasattr2(&mp->m_sb) && version == 2)) {
		__int64_t sbfields = 0;

		spin_lock(&mp->m_sb_lock);
		if (!xfs_sb_version_hasattr(&mp->m_sb)) {
			xfs_sb_version_addattr(&mp->m_sb);
			sbfields |= XFS_SB_VERSIONNUM;
		}
		if (!xfs_sb_version_hasattr2(&mp->m_sb) && version == 2) {
			xfs_sb_version_addattr2(&mp->m_sb);
			sbfields |= (XFS_SB_VERSIONNUM | XFS_SB_FEATURES2);
		}
		if (sbfields) {
			spin_unlock(&mp->m_sb_lock);
			xfs_mod_sb(tp, sbfields);
		} else
			spin_unlock(&mp->m_sb_lock);
	}

	error = xfs_bmap_finish(&tp, &flist, &committed);
	if (error)
		goto error2;
	return xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES);
error2:
	xfs_bmap_cancel(&flist);
error1:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
error0:
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_ABORT);
	return error;
}

void
xfs_bmap_add_free(
	xfs_fsblock_t		bno,		
	xfs_filblks_t		len,		
	xfs_bmap_free_t		*flist,		
	xfs_mount_t		*mp)		
{
	xfs_bmap_free_item_t	*cur;		
	xfs_bmap_free_item_t	*new;		
	xfs_bmap_free_item_t	*prev;		
#ifdef DEBUG
	xfs_agnumber_t		agno;
	xfs_agblock_t		agbno;

	ASSERT(bno != NULLFSBLOCK);
	ASSERT(len > 0);
	ASSERT(len <= MAXEXTLEN);
	ASSERT(!isnullstartblock(bno));
	agno = XFS_FSB_TO_AGNO(mp, bno);
	agbno = XFS_FSB_TO_AGBNO(mp, bno);
	ASSERT(agno < mp->m_sb.sb_agcount);
	ASSERT(agbno < mp->m_sb.sb_agblocks);
	ASSERT(len < mp->m_sb.sb_agblocks);
	ASSERT(agbno + len <= mp->m_sb.sb_agblocks);
#endif
	ASSERT(xfs_bmap_free_item_zone != NULL);
	new = kmem_zone_alloc(xfs_bmap_free_item_zone, KM_SLEEP);
	new->xbfi_startblock = bno;
	new->xbfi_blockcount = (xfs_extlen_t)len;
	for (prev = NULL, cur = flist->xbf_first;
	     cur != NULL;
	     prev = cur, cur = cur->xbfi_next) {
		if (cur->xbfi_startblock >= bno)
			break;
	}
	if (prev)
		prev->xbfi_next = new;
	else
		flist->xbf_first = new;
	new->xbfi_next = cur;
	flist->xbf_count++;
}

void
xfs_bmap_compute_maxlevels(
	xfs_mount_t	*mp,		
	int		whichfork)	
{
	int		level;		
	uint		maxblocks;	
	uint		maxleafents;	
	int		maxrootrecs;	
	int		minleafrecs;	
	int		minnoderecs;	
	int		sz;		

	if (whichfork == XFS_DATA_FORK) {
		maxleafents = MAXEXTNUM;
		sz = XFS_BMDR_SPACE_CALC(MINDBTPTRS);
	} else {
		maxleafents = MAXAEXTNUM;
		sz = XFS_BMDR_SPACE_CALC(MINABTPTRS);
	}
	maxrootrecs = xfs_bmdr_maxrecs(mp, sz, 0);
	minleafrecs = mp->m_bmap_dmnr[0];
	minnoderecs = mp->m_bmap_dmnr[1];
	maxblocks = (maxleafents + minleafrecs - 1) / minleafrecs;
	for (level = 1; maxblocks > 1; level++) {
		if (maxblocks <= maxrootrecs)
			maxblocks = 1;
		else
			maxblocks = (maxblocks + minnoderecs - 1) / minnoderecs;
	}
	mp->m_bm_maxlevels[whichfork] = level;
}

int						
xfs_bmap_finish(
	xfs_trans_t		**tp,		
	xfs_bmap_free_t		*flist,		
	int			*committed)	
{
	xfs_efd_log_item_t	*efd;		
	xfs_efi_log_item_t	*efi;		
	int			error;		
	xfs_bmap_free_item_t	*free;		
	unsigned int		logres;		
	unsigned int		logcount;	
	xfs_mount_t		*mp;		
	xfs_bmap_free_item_t	*next;		
	xfs_trans_t		*ntp;		

	ASSERT((*tp)->t_flags & XFS_TRANS_PERM_LOG_RES);
	if (flist->xbf_count == 0) {
		*committed = 0;
		return 0;
	}
	ntp = *tp;
	efi = xfs_trans_get_efi(ntp, flist->xbf_count);
	for (free = flist->xbf_first; free; free = free->xbfi_next)
		xfs_trans_log_efi_extent(ntp, efi, free->xbfi_startblock,
			free->xbfi_blockcount);
	logres = ntp->t_log_res;
	logcount = ntp->t_log_count;
	ntp = xfs_trans_dup(*tp);
	error = xfs_trans_commit(*tp, 0);
	*tp = ntp;
	*committed = 1;
	if (error)
		return error;

	xfs_log_ticket_put(ntp->t_ticket);

	if ((error = xfs_trans_reserve(ntp, 0, logres, 0, XFS_TRANS_PERM_LOG_RES,
			logcount)))
		return error;
	efd = xfs_trans_get_efd(ntp, efi, flist->xbf_count);
	for (free = flist->xbf_first; free != NULL; free = next) {
		next = free->xbfi_next;
		if ((error = xfs_free_extent(ntp, free->xbfi_startblock,
				free->xbfi_blockcount))) {
			mp = ntp->t_mountp;
			if (!XFS_FORCED_SHUTDOWN(mp))
				xfs_force_shutdown(mp,
						   (error == EFSCORRUPTED) ?
						   SHUTDOWN_CORRUPT_INCORE :
						   SHUTDOWN_META_IO_ERROR);
			return error;
		}
		xfs_trans_log_efd_extent(ntp, efd, free->xbfi_startblock,
			free->xbfi_blockcount);
		xfs_bmap_del_free(flist, NULL, free);
	}
	return 0;
}

void
xfs_bmap_cancel(
	xfs_bmap_free_t		*flist)	
{
	xfs_bmap_free_item_t	*free;	
	xfs_bmap_free_item_t	*next;

	if (flist->xbf_count == 0)
		return;
	ASSERT(flist->xbf_first != NULL);
	for (free = flist->xbf_first; free; free = next) {
		next = free->xbfi_next;
		xfs_bmap_del_free(flist, NULL, free);
	}
	ASSERT(flist->xbf_count == 0);
}

int						
xfs_bmap_first_unused(
	xfs_trans_t	*tp,			
	xfs_inode_t	*ip,			
	xfs_extlen_t	len,			
	xfs_fileoff_t	*first_unused,		
	int		whichfork)		
{
	int		error;			
	int		idx;			
	xfs_ifork_t	*ifp;			
	xfs_fileoff_t	lastaddr;		
	xfs_fileoff_t	lowest;			
	xfs_fileoff_t	max;			
	xfs_fileoff_t	off;			
	xfs_extnum_t	nextents;		

	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE ||
	       XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS ||
	       XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL);
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		*first_unused = 0;
		return 0;
	}
	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (!(ifp->if_flags & XFS_IFEXTENTS) &&
	    (error = xfs_iread_extents(tp, ip, whichfork)))
		return error;
	lowest = *first_unused;
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	for (idx = 0, lastaddr = 0, max = lowest; idx < nextents; idx++) {
		xfs_bmbt_rec_host_t *ep = xfs_iext_get_ext(ifp, idx);
		off = xfs_bmbt_get_startoff(ep);
		if (off >= lowest + len && off - max >= len) {
			*first_unused = max;
			return 0;
		}
		lastaddr = off + xfs_bmbt_get_blockcount(ep);
		max = XFS_FILEOFF_MAX(lastaddr, lowest);
	}
	*first_unused = max;
	return 0;
}

int						
xfs_bmap_last_before(
	xfs_trans_t	*tp,			
	xfs_inode_t	*ip,			
	xfs_fileoff_t	*last_block,		
	int		whichfork)		
{
	xfs_fileoff_t	bno;			
	int		eof;			
	xfs_bmbt_rec_host_t *ep;		
	int		error;			
	xfs_bmbt_irec_t	got;			
	xfs_ifork_t	*ifp;			
	xfs_extnum_t	lastx;			
	xfs_bmbt_irec_t	prev;			

	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_LOCAL)
	       return XFS_ERROR(EIO);
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		*last_block = 0;
		return 0;
	}
	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (!(ifp->if_flags & XFS_IFEXTENTS) &&
	    (error = xfs_iread_extents(tp, ip, whichfork)))
		return error;
	bno = *last_block - 1;
	ep = xfs_bmap_search_extents(ip, bno, whichfork, &eof, &lastx, &got,
		&prev);
	if (eof || xfs_bmbt_get_startoff(ep) > bno) {
		if (prev.br_startoff == NULLFILEOFF)
			*last_block = 0;
		else
			*last_block = prev.br_startoff + prev.br_blockcount;
	}
	return 0;
}

STATIC int
xfs_bmap_last_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*rec,
	int			*is_empty)
{
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(ip, whichfork);
	int			error;
	int			nextents;

	if (!(ifp->if_flags & XFS_IFEXTENTS)) {
		error = xfs_iread_extents(tp, ip, whichfork);
		if (error)
			return error;
	}

	nextents = ifp->if_bytes / sizeof(xfs_bmbt_rec_t);
	if (nextents == 0) {
		*is_empty = 1;
		return 0;
	}

	xfs_bmbt_get_all(xfs_iext_get_ext(ifp, nextents - 1), rec);
	*is_empty = 0;
	return 0;
}

STATIC int
xfs_bmap_isaeof(
	struct xfs_bmalloca	*bma,
	int			whichfork)
{
	struct xfs_bmbt_irec	rec;
	int			is_empty;
	int			error;

	bma->aeof = 0;
	error = xfs_bmap_last_extent(NULL, bma->ip, whichfork, &rec,
				     &is_empty);
	if (error || is_empty)
		return error;

	bma->aeof = bma->offset >= rec.br_startoff + rec.br_blockcount ||
		(bma->offset >= rec.br_startoff &&
		 isnullstartblock(rec.br_startblock));
	return 0;
}

int
xfs_bmap_eof(
	struct xfs_inode	*ip,
	xfs_fileoff_t		endoff,
	int			whichfork,
	int			*eof)
{
	struct xfs_bmbt_irec	rec;
	int			error;

	error = xfs_bmap_last_extent(NULL, ip, whichfork, &rec, eof);
	if (error || *eof)
		return error;

	*eof = endoff >= rec.br_startoff + rec.br_blockcount;
	return 0;
}

int
xfs_bmap_last_offset(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		*last_block,
	int			whichfork)
{
	struct xfs_bmbt_irec	rec;
	int			is_empty;
	int			error;

	*last_block = 0;

	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL)
		return 0;

	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS)
	       return XFS_ERROR(EIO);

	error = xfs_bmap_last_extent(NULL, ip, whichfork, &rec, &is_empty);
	if (error || is_empty)
		return error;

	*last_block = rec.br_startoff + rec.br_blockcount;
	return 0;
}

int					
xfs_bmap_one_block(
	xfs_inode_t	*ip,		
	int		whichfork)	
{
	xfs_bmbt_rec_host_t *ep;	
	xfs_ifork_t	*ifp;		
	int		rval;		
	xfs_bmbt_irec_t	s;		

#ifndef DEBUG
	if (whichfork == XFS_DATA_FORK)
		return XFS_ISIZE(ip) == ip->i_mount->m_sb.sb_blocksize;
#endif	
	if (XFS_IFORK_NEXTENTS(ip, whichfork) != 1)
		return 0;
	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS)
		return 0;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(ifp->if_flags & XFS_IFEXTENTS);
	ep = xfs_iext_get_ext(ifp, 0);
	xfs_bmbt_get_all(ep, &s);
	rval = s.br_startoff == 0 && s.br_blockcount == 1;
	if (rval && whichfork == XFS_DATA_FORK)
		ASSERT(XFS_ISIZE(ip) == ip->i_mount->m_sb.sb_blocksize);
	return rval;
}

STATIC int
xfs_bmap_sanity_check(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp,
	int			level)
{
	struct xfs_btree_block  *block = XFS_BUF_TO_BLOCK(bp);

	if (block->bb_magic != cpu_to_be32(XFS_BMAP_MAGIC) ||
	    be16_to_cpu(block->bb_level) != level ||
	    be16_to_cpu(block->bb_numrecs) == 0 ||
	    be16_to_cpu(block->bb_numrecs) > mp->m_bmap_dmxr[level != 0])
		return 0;
	return 1;
}

/*
 * Read in the extents to if_extents.
 * All inode fields are set up by caller, we just traverse the btree
 * and copy the records in. If the file system cannot contain unwritten
 * extents, the records are checked for no "state" flags.
 */
int					
xfs_bmap_read_extents(
	xfs_trans_t		*tp,	
	xfs_inode_t		*ip,	
	int			whichfork) 
{
	struct xfs_btree_block	*block;	
	xfs_fsblock_t		bno;	
	xfs_buf_t		*bp;	
	int			error;	
	xfs_exntfmt_t		exntf;	
	xfs_extnum_t		i, j;	
	xfs_ifork_t		*ifp;	
	int			level;	
	xfs_mount_t		*mp;	
	__be64			*pp;	
	
	xfs_extnum_t		room;	

	bno = NULLFSBLOCK;
	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	exntf = (whichfork != XFS_DATA_FORK) ? XFS_EXTFMT_NOSTATE :
					XFS_EXTFMT_INODE(ip);
	block = ifp->if_broot;
	level = be16_to_cpu(block->bb_level);
	ASSERT(level > 0);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, 1, ifp->if_broot_bytes);
	bno = be64_to_cpu(*pp);
	ASSERT(bno != NULLDFSBNO);
	ASSERT(XFS_FSB_TO_AGNO(mp, bno) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, bno) < mp->m_sb.sb_agblocks);
	while (level-- > 0) {
		if ((error = xfs_btree_read_bufl(mp, tp, bno, 0, &bp,
				XFS_BMAP_BTREE_REF)))
			return error;
		block = XFS_BUF_TO_BLOCK(bp);
		XFS_WANT_CORRUPTED_GOTO(
			xfs_bmap_sanity_check(mp, bp, level),
			error0);
		if (level == 0)
			break;
		pp = XFS_BMBT_PTR_ADDR(mp, block, 1, mp->m_bmap_dmxr[1]);
		bno = be64_to_cpu(*pp);
		XFS_WANT_CORRUPTED_GOTO(XFS_FSB_SANITY_CHECK(mp, bno), error0);
		xfs_trans_brelse(tp, bp);
	}
	room = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	i = 0;
	for (;;) {
		xfs_bmbt_rec_t	*frp;
		xfs_fsblock_t	nextbno;
		xfs_extnum_t	num_recs;
		xfs_extnum_t	start;

		num_recs = xfs_btree_get_numrecs(block);
		if (unlikely(i + num_recs > room)) {
			ASSERT(i + num_recs <= room);
			xfs_warn(ip->i_mount,
				"corrupt dinode %Lu, (btree extents).",
				(unsigned long long) ip->i_ino);
			XFS_CORRUPTION_ERROR("xfs_bmap_read_extents(1)",
				XFS_ERRLEVEL_LOW, ip->i_mount, block);
			goto error0;
		}
		XFS_WANT_CORRUPTED_GOTO(
			xfs_bmap_sanity_check(mp, bp, 0),
			error0);
		nextbno = be64_to_cpu(block->bb_u.l.bb_rightsib);
		if (nextbno != NULLFSBLOCK)
			xfs_btree_reada_bufl(mp, nextbno, 1);
		frp = XFS_BMBT_REC_ADDR(mp, block, 1);
		start = i;
		for (j = 0; j < num_recs; j++, i++, frp++) {
			xfs_bmbt_rec_host_t *trp = xfs_iext_get_ext(ifp, i);
			trp->l0 = be64_to_cpu(frp->l0);
			trp->l1 = be64_to_cpu(frp->l1);
		}
		if (exntf == XFS_EXTFMT_NOSTATE) {
			if (unlikely(xfs_check_nostate_extents(ifp,
					start, num_recs))) {
				XFS_ERROR_REPORT("xfs_bmap_read_extents(2)",
						 XFS_ERRLEVEL_LOW,
						 ip->i_mount);
				goto error0;
			}
		}
		xfs_trans_brelse(tp, bp);
		bno = nextbno;
		if (bno == NULLFSBLOCK)
			break;
		if ((error = xfs_btree_read_bufl(mp, tp, bno, 0, &bp,
				XFS_BMAP_BTREE_REF)))
			return error;
		block = XFS_BUF_TO_BLOCK(bp);
	}
	ASSERT(i == (ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t)));
	ASSERT(i == XFS_IFORK_NEXTENTS(ip, whichfork));
	XFS_BMAP_TRACE_EXLIST(ip, i, whichfork);
	return 0;
error0:
	xfs_trans_brelse(tp, bp);
	return XFS_ERROR(EFSCORRUPTED);
}

#ifdef DEBUG
void
xfs_bmap_trace_exlist(
	xfs_inode_t	*ip,		
	xfs_extnum_t	cnt,		
	int		whichfork,	
	unsigned long	caller_ip)
{
	xfs_extnum_t	idx;		
	xfs_ifork_t	*ifp;		
	int		state = 0;

	if (whichfork == XFS_ATTR_FORK)
		state |= BMAP_ATTRFORK;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(cnt == (ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t)));
	for (idx = 0; idx < cnt; idx++)
		trace_xfs_extlist(ip, idx, whichfork, caller_ip);
}

STATIC void
xfs_bmap_validate_ret(
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	int			flags,
	xfs_bmbt_irec_t		*mval,
	int			nmap,
	int			ret_nmap)
{
	int			i;		

	ASSERT(ret_nmap <= nmap);

	for (i = 0; i < ret_nmap; i++) {
		ASSERT(mval[i].br_blockcount > 0);
		if (!(flags & XFS_BMAPI_ENTIRE)) {
			ASSERT(mval[i].br_startoff >= bno);
			ASSERT(mval[i].br_blockcount <= len);
			ASSERT(mval[i].br_startoff + mval[i].br_blockcount <=
			       bno + len);
		} else {
			ASSERT(mval[i].br_startoff < bno + len);
			ASSERT(mval[i].br_startoff + mval[i].br_blockcount >
			       bno);
		}
		ASSERT(i == 0 ||
		       mval[i - 1].br_startoff + mval[i - 1].br_blockcount ==
		       mval[i].br_startoff);
		ASSERT(mval[i].br_startblock != DELAYSTARTBLOCK &&
		       mval[i].br_startblock != HOLESTARTBLOCK);
		ASSERT(mval[i].br_state == XFS_EXT_NORM ||
		       mval[i].br_state == XFS_EXT_UNWRITTEN);
	}
}
#endif 


STATIC void
xfs_bmapi_trim_map(
	struct xfs_bmbt_irec	*mval,
	struct xfs_bmbt_irec	*got,
	xfs_fileoff_t		*bno,
	xfs_filblks_t		len,
	xfs_fileoff_t		obno,
	xfs_fileoff_t		end,
	int			n,
	int			flags)
{
	if ((flags & XFS_BMAPI_ENTIRE) ||
	    got->br_startoff + got->br_blockcount <= obno) {
		*mval = *got;
		if (isnullstartblock(got->br_startblock))
			mval->br_startblock = DELAYSTARTBLOCK;
		return;
	}

	if (obno > *bno)
		*bno = obno;
	ASSERT((*bno >= obno) || (n == 0));
	ASSERT(*bno < end);
	mval->br_startoff = *bno;
	if (isnullstartblock(got->br_startblock))
		mval->br_startblock = DELAYSTARTBLOCK;
	else
		mval->br_startblock = got->br_startblock +
					(*bno - got->br_startoff);
	mval->br_blockcount = XFS_FILBLKS_MIN(end - *bno,
			got->br_blockcount - (*bno - got->br_startoff));
	mval->br_state = got->br_state;
	ASSERT(mval->br_blockcount <= len);
	return;
}

STATIC void
xfs_bmapi_update_map(
	struct xfs_bmbt_irec	**map,
	xfs_fileoff_t		*bno,
	xfs_filblks_t		*len,
	xfs_fileoff_t		obno,
	xfs_fileoff_t		end,
	int			*n,
	int			flags)
{
	xfs_bmbt_irec_t	*mval = *map;

	ASSERT((flags & XFS_BMAPI_ENTIRE) ||
	       ((mval->br_startoff + mval->br_blockcount) <= end));
	ASSERT((flags & XFS_BMAPI_ENTIRE) || (mval->br_blockcount <= *len) ||
	       (mval->br_startoff < obno));

	*bno = mval->br_startoff + mval->br_blockcount;
	*len = end - *bno;
	if (*n > 0 && mval->br_startoff == mval[-1].br_startoff) {
		
		ASSERT(mval->br_startblock == mval[-1].br_startblock);
		ASSERT(mval->br_blockcount > mval[-1].br_blockcount);
		ASSERT(mval->br_state == mval[-1].br_state);
		mval[-1].br_blockcount = mval->br_blockcount;
		mval[-1].br_state = mval->br_state;
	} else if (*n > 0 && mval->br_startblock != DELAYSTARTBLOCK &&
		   mval[-1].br_startblock != DELAYSTARTBLOCK &&
		   mval[-1].br_startblock != HOLESTARTBLOCK &&
		   mval->br_startblock == mval[-1].br_startblock +
					  mval[-1].br_blockcount &&
		   ((flags & XFS_BMAPI_IGSTATE) ||
			mval[-1].br_state == mval->br_state)) {
		ASSERT(mval->br_startoff ==
		       mval[-1].br_startoff + mval[-1].br_blockcount);
		mval[-1].br_blockcount += mval->br_blockcount;
	} else if (*n > 0 &&
		   mval->br_startblock == DELAYSTARTBLOCK &&
		   mval[-1].br_startblock == DELAYSTARTBLOCK &&
		   mval->br_startoff ==
		   mval[-1].br_startoff + mval[-1].br_blockcount) {
		mval[-1].br_blockcount += mval->br_blockcount;
		mval[-1].br_state = mval->br_state;
	} else if (!((*n == 0) &&
		     ((mval->br_startoff + mval->br_blockcount) <=
		      obno))) {
		mval++;
		(*n)++;
	}
	*map = mval;
}

int
xfs_bmapi_read(
	struct xfs_inode	*ip,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	struct xfs_bmbt_irec	*mval,
	int			*nmap,
	int			flags)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp;
	struct xfs_bmbt_irec	got;
	struct xfs_bmbt_irec	prev;
	xfs_fileoff_t		obno;
	xfs_fileoff_t		end;
	xfs_extnum_t		lastx;
	int			error;
	int			eof;
	int			n = 0;
	int			whichfork = (flags & XFS_BMAPI_ATTRFORK) ?
						XFS_ATTR_FORK : XFS_DATA_FORK;

	ASSERT(*nmap >= 1);
	ASSERT(!(flags & ~(XFS_BMAPI_ATTRFORK|XFS_BMAPI_ENTIRE|
			   XFS_BMAPI_IGSTATE)));

	if (unlikely(XFS_TEST_ERROR(
	    (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS &&
	     XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE),
	     mp, XFS_ERRTAG_BMAPIFORMAT, XFS_RANDOM_BMAPIFORMAT))) {
		XFS_ERROR_REPORT("xfs_bmapi_read", XFS_ERRLEVEL_LOW, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	XFS_STATS_INC(xs_blk_mapr);

	ifp = XFS_IFORK_PTR(ip, whichfork);

	if (!(ifp->if_flags & XFS_IFEXTENTS)) {
		error = xfs_iread_extents(NULL, ip, whichfork);
		if (error)
			return error;
	}

	xfs_bmap_search_extents(ip, bno, whichfork, &eof, &lastx, &got, &prev);
	end = bno + len;
	obno = bno;

	while (bno < end && n < *nmap) {
		
		if (eof)
			got.br_startoff = end;
		if (got.br_startoff > bno) {
			
			mval->br_startoff = bno;
			mval->br_startblock = HOLESTARTBLOCK;
			mval->br_blockcount =
				XFS_FILBLKS_MIN(len, got.br_startoff - bno);
			mval->br_state = XFS_EXT_NORM;
			bno += mval->br_blockcount;
			len -= mval->br_blockcount;
			mval++;
			n++;
			continue;
		}

		
		xfs_bmapi_trim_map(mval, &got, &bno, len, obno, end, n, flags);
		xfs_bmapi_update_map(&mval, &bno, &len, obno, end, &n, flags);

		
		if (bno >= end || n >= *nmap)
			break;

		
		if (++lastx < ifp->if_bytes / sizeof(xfs_bmbt_rec_t))
			xfs_bmbt_get_all(xfs_iext_get_ext(ifp, lastx), &got);
		else
			eof = 1;
	}
	*nmap = n;
	return 0;
}

STATIC int
xfs_bmapi_reserve_delalloc(
	struct xfs_inode	*ip,
	xfs_fileoff_t		aoff,
	xfs_filblks_t		len,
	struct xfs_bmbt_irec	*got,
	struct xfs_bmbt_irec	*prev,
	xfs_extnum_t		*lastx,
	int			eof)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	xfs_extlen_t		alen;
	xfs_extlen_t		indlen;
	char			rt = XFS_IS_REALTIME_INODE(ip);
	xfs_extlen_t		extsz;
	int			error;

	alen = XFS_FILBLKS_MIN(len, MAXEXTLEN);
	if (!eof)
		alen = XFS_FILBLKS_MIN(alen, got->br_startoff - aoff);

	
	extsz = xfs_get_extsz_hint(ip);
	if (extsz) {
		alen = XFS_FILBLKS_MIN(len, MAXEXTLEN - (2 * extsz - 1));
		error = xfs_bmap_extsize_align(mp, got, prev, extsz, rt, eof,
					       1, 0, &aoff, &alen);
		ASSERT(!error);
	}

	if (rt)
		extsz = alen / mp->m_sb.sb_rextsize;

	error = xfs_trans_reserve_quota_nblks(NULL, ip, (long)alen, 0,
			rt ? XFS_QMOPT_RES_RTBLKS : XFS_QMOPT_RES_REGBLKS);
	if (error)
		return error;

	indlen = (xfs_extlen_t)xfs_bmap_worst_indlen(ip, alen);
	ASSERT(indlen > 0);

	if (rt) {
		error = xfs_mod_incore_sb(mp, XFS_SBS_FREXTENTS,
					  -((int64_t)extsz), 0);
	} else {
		error = xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS,
						 -((int64_t)alen), 0);
	}

	if (error)
		goto out_unreserve_quota;

	error = xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS,
					 -((int64_t)indlen), 0);
	if (error)
		goto out_unreserve_blocks;


	ip->i_delayed_blks += alen;

	got->br_startoff = aoff;
	got->br_startblock = nullstartblock(indlen);
	got->br_blockcount = alen;
	got->br_state = XFS_EXT_NORM;
	xfs_bmap_add_extent_hole_delay(ip, lastx, got);

	xfs_bmbt_get_all(xfs_iext_get_ext(ifp, *lastx), got);

	ASSERT(got->br_startoff <= aoff);
	ASSERT(got->br_startoff + got->br_blockcount >= aoff + alen);
	ASSERT(isnullstartblock(got->br_startblock));
	ASSERT(got->br_state == XFS_EXT_NORM);
	return 0;

out_unreserve_blocks:
	if (rt)
		xfs_mod_incore_sb(mp, XFS_SBS_FREXTENTS, extsz, 0);
	else
		xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS, alen, 0);
out_unreserve_quota:
	if (XFS_IS_QUOTA_ON(mp))
		xfs_trans_unreserve_quota_nblks(NULL, ip, alen, 0, rt ?
				XFS_QMOPT_RES_RTBLKS : XFS_QMOPT_RES_REGBLKS);
	return error;
}

int
xfs_bmapi_delay(
	struct xfs_inode	*ip,	
	xfs_fileoff_t		bno,	
	xfs_filblks_t		len,	
	struct xfs_bmbt_irec	*mval,	
	int			*nmap,	
	int			flags)	
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	struct xfs_bmbt_irec	got;	
	struct xfs_bmbt_irec	prev;	
	xfs_fileoff_t		obno;	
	xfs_fileoff_t		end;	
	xfs_extnum_t		lastx;	
	int			eof;	
	int			n = 0;	
	int			error = 0;

	ASSERT(*nmap >= 1);
	ASSERT(*nmap <= XFS_BMAP_MAX_NMAP);
	ASSERT(!(flags & ~XFS_BMAPI_ENTIRE));

	if (unlikely(XFS_TEST_ERROR(
	    (XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) != XFS_DINODE_FMT_EXTENTS &&
	     XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) != XFS_DINODE_FMT_BTREE),
	     mp, XFS_ERRTAG_BMAPIFORMAT, XFS_RANDOM_BMAPIFORMAT))) {
		XFS_ERROR_REPORT("xfs_bmapi_delay", XFS_ERRLEVEL_LOW, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	XFS_STATS_INC(xs_blk_mapw);

	if (!(ifp->if_flags & XFS_IFEXTENTS)) {
		error = xfs_iread_extents(NULL, ip, XFS_DATA_FORK);
		if (error)
			return error;
	}

	xfs_bmap_search_extents(ip, bno, XFS_DATA_FORK, &eof, &lastx, &got, &prev);
	end = bno + len;
	obno = bno;

	while (bno < end && n < *nmap) {
		if (eof || got.br_startoff > bno) {
			error = xfs_bmapi_reserve_delalloc(ip, bno, len, &got,
							   &prev, &lastx, eof);
			if (error) {
				if (n == 0) {
					*nmap = 0;
					return error;
				}
				break;
			}
		}

		
		xfs_bmapi_trim_map(mval, &got, &bno, len, obno, end, n, flags);
		xfs_bmapi_update_map(&mval, &bno, &len, obno, end, &n, flags);

		
		if (bno >= end || n >= *nmap)
			break;

		
		prev = got;
		if (++lastx < ifp->if_bytes / sizeof(xfs_bmbt_rec_t))
			xfs_bmbt_get_all(xfs_iext_get_ext(ifp, lastx), &got);
		else
			eof = 1;
	}

	*nmap = n;
	return 0;
}


STATIC int
xfs_bmapi_allocate(
	struct xfs_bmalloca	*bma,
	int			flags)
{
	struct xfs_mount	*mp = bma->ip->i_mount;
	int			whichfork = (flags & XFS_BMAPI_ATTRFORK) ?
						XFS_ATTR_FORK : XFS_DATA_FORK;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(bma->ip, whichfork);
	int			tmp_logflags = 0;
	int			error;
	int			rt;

	ASSERT(bma->length > 0);

	rt = (whichfork == XFS_DATA_FORK) && XFS_IS_REALTIME_INODE(bma->ip);

	if (bma->wasdel) {
		bma->length = (xfs_extlen_t)bma->got.br_blockcount;
		bma->offset = bma->got.br_startoff;
		if (bma->idx != NULLEXTNUM && bma->idx) {
			xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx - 1),
					 &bma->prev);
		}
	} else {
		bma->length = XFS_FILBLKS_MIN(bma->length, MAXEXTLEN);
		if (!bma->eof)
			bma->length = XFS_FILBLKS_MIN(bma->length,
					bma->got.br_startoff - bma->offset);
	}

	if (!(flags & XFS_BMAPI_METADATA)) {
		bma->userdata = (bma->offset == 0) ?
			XFS_ALLOC_INITIAL_USER_DATA : XFS_ALLOC_USERDATA;
	}

	bma->minlen = (flags & XFS_BMAPI_CONTIG) ? bma->length : 1;

	if (mp->m_dalign && bma->length >= mp->m_dalign &&
	    !(flags & XFS_BMAPI_METADATA) && whichfork == XFS_DATA_FORK) {
		error = xfs_bmap_isaeof(bma, whichfork);
		if (error)
			return error;
	}

	error = xfs_bmap_alloc(bma);
	if (error)
		return error;

	if (bma->flist->xbf_low)
		bma->minleft = 0;
	if (bma->cur)
		bma->cur->bc_private.b.firstblock = *bma->firstblock;
	if (bma->blkno == NULLFSBLOCK)
		return 0;
	if ((ifp->if_flags & XFS_IFBROOT) && !bma->cur) {
		bma->cur = xfs_bmbt_init_cursor(mp, bma->tp, bma->ip, whichfork);
		bma->cur->bc_private.b.firstblock = *bma->firstblock;
		bma->cur->bc_private.b.flist = bma->flist;
	}
	bma->nallocs++;

	if (bma->cur)
		bma->cur->bc_private.b.flags =
			bma->wasdel ? XFS_BTCUR_BPRV_WASDEL : 0;

	bma->got.br_startoff = bma->offset;
	bma->got.br_startblock = bma->blkno;
	bma->got.br_blockcount = bma->length;
	bma->got.br_state = XFS_EXT_NORM;

	/*
	 * A wasdelay extent has been initialized, so shouldn't be flagged
	 * as unwritten.
	 */
	if (!bma->wasdel && (flags & XFS_BMAPI_PREALLOC) &&
	    xfs_sb_version_hasextflgbit(&mp->m_sb))
		bma->got.br_state = XFS_EXT_UNWRITTEN;

	if (bma->wasdel)
		error = xfs_bmap_add_extent_delay_real(bma);
	else
		error = xfs_bmap_add_extent_hole_real(bma, whichfork);

	bma->logflags |= tmp_logflags;
	if (error)
		return error;

	xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx), &bma->got);

	ASSERT(bma->got.br_startoff <= bma->offset);
	ASSERT(bma->got.br_startoff + bma->got.br_blockcount >=
	       bma->offset + bma->length);
	ASSERT(bma->got.br_state == XFS_EXT_NORM ||
	       bma->got.br_state == XFS_EXT_UNWRITTEN);
	return 0;
}

STATIC int
xfs_bmapi_convert_unwritten(
	struct xfs_bmalloca	*bma,
	struct xfs_bmbt_irec	*mval,
	xfs_filblks_t		len,
	int			flags)
{
	int			whichfork = (flags & XFS_BMAPI_ATTRFORK) ?
						XFS_ATTR_FORK : XFS_DATA_FORK;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(bma->ip, whichfork);
	int			tmp_logflags = 0;
	int			error;

	/* check if we need to do unwritten->real conversion */
	if (mval->br_state == XFS_EXT_UNWRITTEN &&
	    (flags & XFS_BMAPI_PREALLOC))
		return 0;

	/* check if we need to do real->unwritten conversion */
	if (mval->br_state == XFS_EXT_NORM &&
	    (flags & (XFS_BMAPI_PREALLOC | XFS_BMAPI_CONVERT)) !=
			(XFS_BMAPI_PREALLOC | XFS_BMAPI_CONVERT))
		return 0;

	ASSERT(mval->br_blockcount <= len);
	if ((ifp->if_flags & XFS_IFBROOT) && !bma->cur) {
		bma->cur = xfs_bmbt_init_cursor(bma->ip->i_mount, bma->tp,
					bma->ip, whichfork);
		bma->cur->bc_private.b.firstblock = *bma->firstblock;
		bma->cur->bc_private.b.flist = bma->flist;
	}
	mval->br_state = (mval->br_state == XFS_EXT_UNWRITTEN)
				? XFS_EXT_NORM : XFS_EXT_UNWRITTEN;

	error = xfs_bmap_add_extent_unwritten_real(bma->tp, bma->ip, &bma->idx,
			&bma->cur, mval, bma->firstblock, bma->flist,
			&tmp_logflags);
	bma->logflags |= tmp_logflags;
	if (error)
		return error;

	/*
	 * Update our extent pointer, given that
	 * xfs_bmap_add_extent_unwritten_real might have merged it into one
	 * of the neighbouring ones.
	 */
	xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma->idx), &bma->got);

	/*
	 * We may have combined previously unwritten space with written space,
	 * so generate another request.
	 */
	if (mval->br_blockcount < len)
		return EAGAIN;
	return 0;
}

int
xfs_bmapi_write(
	struct xfs_trans	*tp,		
	struct xfs_inode	*ip,		
	xfs_fileoff_t		bno,		
	xfs_filblks_t		len,		
	int			flags,		
	xfs_fsblock_t		*firstblock,	
	xfs_extlen_t		total,		
	struct xfs_bmbt_irec	*mval,		
	int			*nmap,		
	struct xfs_bmap_free	*flist)		
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp;
	struct xfs_bmalloca	bma = { 0 };	
	xfs_fileoff_t		end;		
	int			eof;		
	int			error;		
	int			n;		
	xfs_fileoff_t		obno;		
	int			whichfork;	
	char			inhole;		
	char			wasdelay;	

#ifdef DEBUG
	xfs_fileoff_t		orig_bno;	
	int			orig_flags;	
	xfs_filblks_t		orig_len;	
	struct xfs_bmbt_irec	*orig_mval;	
	int			orig_nmap;	

	orig_bno = bno;
	orig_len = len;
	orig_flags = flags;
	orig_mval = mval;
	orig_nmap = *nmap;
#endif

	ASSERT(*nmap >= 1);
	ASSERT(*nmap <= XFS_BMAP_MAX_NMAP);
	ASSERT(!(flags & XFS_BMAPI_IGSTATE));
	ASSERT(tp != NULL);
	ASSERT(len > 0);

	whichfork = (flags & XFS_BMAPI_ATTRFORK) ?
		XFS_ATTR_FORK : XFS_DATA_FORK;

	if (unlikely(XFS_TEST_ERROR(
	    (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS &&
	     XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE &&
	     XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_LOCAL),
	     mp, XFS_ERRTAG_BMAPIFORMAT, XFS_RANDOM_BMAPIFORMAT))) {
		XFS_ERROR_REPORT("xfs_bmapi_write", XFS_ERRLEVEL_LOW, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	ifp = XFS_IFORK_PTR(ip, whichfork);

	XFS_STATS_INC(xs_blk_mapw);

	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		error = xfs_bmap_local_to_extents(tp, ip, firstblock, total,
						  &bma.logflags, whichfork);
		if (error)
			goto error0;
	}

	if (*firstblock == NULLFSBLOCK) {
		if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE)
			bma.minleft = be16_to_cpu(ifp->if_broot->bb_level) + 1;
		else
			bma.minleft = 1;
	} else {
		bma.minleft = 0;
	}

	if (!(ifp->if_flags & XFS_IFEXTENTS)) {
		error = xfs_iread_extents(tp, ip, whichfork);
		if (error)
			goto error0;
	}

	xfs_bmap_search_extents(ip, bno, whichfork, &eof, &bma.idx, &bma.got,
				&bma.prev);
	n = 0;
	end = bno + len;
	obno = bno;

	bma.tp = tp;
	bma.ip = ip;
	bma.total = total;
	bma.userdata = 0;
	bma.flist = flist;
	bma.firstblock = firstblock;

	while (bno < end && n < *nmap) {
		inhole = eof || bma.got.br_startoff > bno;
		wasdelay = !inhole && isnullstartblock(bma.got.br_startblock);

		if (inhole || wasdelay) {
			bma.eof = eof;
			bma.conv = !!(flags & XFS_BMAPI_CONVERT);
			bma.wasdel = wasdelay;
			bma.offset = bno;

			if (len > (xfs_filblks_t)MAXEXTLEN)
				bma.length = MAXEXTLEN;
			else
				bma.length = len;

			ASSERT(len > 0);
			ASSERT(bma.length > 0);
			error = xfs_bmapi_allocate(&bma, flags);
			if (error)
				goto error0;
			if (bma.blkno == NULLFSBLOCK)
				break;
		}

		
		xfs_bmapi_trim_map(mval, &bma.got, &bno, len, obno,
							end, n, flags);

		/* Execute unwritten extent conversion if necessary */
		error = xfs_bmapi_convert_unwritten(&bma, mval, len, flags);
		if (error == EAGAIN)
			continue;
		if (error)
			goto error0;

		
		xfs_bmapi_update_map(&mval, &bno, &len, obno, end, &n, flags);

		if (bno >= end || n >= *nmap || bma.nallocs >= *nmap)
			break;

		
		bma.prev = bma.got;
		if (++bma.idx < ifp->if_bytes / sizeof(xfs_bmbt_rec_t)) {
			xfs_bmbt_get_all(xfs_iext_get_ext(ifp, bma.idx),
					 &bma.got);
		} else
			eof = 1;
	}
	*nmap = n;

	if (xfs_bmap_wants_extents(ip, whichfork)) {
		int		tmp_logflags = 0;

		ASSERT(bma.cur);
		error = xfs_bmap_btree_to_extents(tp, ip, bma.cur,
			&tmp_logflags, whichfork);
		bma.logflags |= tmp_logflags;
		if (error)
			goto error0;
	}

	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE ||
	       XFS_IFORK_NEXTENTS(ip, whichfork) >
		XFS_IFORK_MAXEXT(ip, whichfork));
	error = 0;
error0:
	if ((bma.logflags & xfs_ilog_fext(whichfork)) &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS)
		bma.logflags &= ~xfs_ilog_fext(whichfork);
	else if ((bma.logflags & xfs_ilog_fbroot(whichfork)) &&
		 XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE)
		bma.logflags &= ~xfs_ilog_fbroot(whichfork);
	if (bma.logflags)
		xfs_trans_log_inode(tp, ip, bma.logflags);

	if (bma.cur) {
		if (!error) {
			ASSERT(*firstblock == NULLFSBLOCK ||
			       XFS_FSB_TO_AGNO(mp, *firstblock) ==
			       XFS_FSB_TO_AGNO(mp,
				       bma.cur->bc_private.b.firstblock) ||
			       (flist->xbf_low &&
				XFS_FSB_TO_AGNO(mp, *firstblock) <
				XFS_FSB_TO_AGNO(mp,
					bma.cur->bc_private.b.firstblock)));
			*firstblock = bma.cur->bc_private.b.firstblock;
		}
		xfs_btree_del_cursor(bma.cur,
			error ? XFS_BTREE_ERROR : XFS_BTREE_NOERROR);
	}
	if (!error)
		xfs_bmap_validate_ret(orig_bno, orig_len, orig_flags, orig_mval,
			orig_nmap, *nmap);
	return error;
}

int						
xfs_bunmapi(
	xfs_trans_t		*tp,		
	struct xfs_inode	*ip,		
	xfs_fileoff_t		bno,		
	xfs_filblks_t		len,		
	int			flags,		
	xfs_extnum_t		nexts,		
	xfs_fsblock_t		*firstblock,	
	xfs_bmap_free_t		*flist,		
	int			*done)		
{
	xfs_btree_cur_t		*cur;		
	xfs_bmbt_irec_t		del;		
	int			eof;		
	xfs_bmbt_rec_host_t	*ep;		
	int			error;		
	xfs_extnum_t		extno;		
	xfs_bmbt_irec_t		got;		
	xfs_ifork_t		*ifp;		
	int			isrt;		
	xfs_extnum_t		lastx;		
	int			logflags;	
	xfs_extlen_t		mod;		
	xfs_mount_t		*mp;		
	xfs_extnum_t		nextents;	
	xfs_bmbt_irec_t		prev;		
	xfs_fileoff_t		start;		
	int			tmp_logflags;	
	int			wasdel;		
	int			whichfork;	
	xfs_fsblock_t		sum;

	trace_xfs_bunmap(ip, bno, len, flags, _RET_IP_);

	whichfork = (flags & XFS_BMAPI_ATTRFORK) ?
		XFS_ATTR_FORK : XFS_DATA_FORK;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (unlikely(
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE)) {
		XFS_ERROR_REPORT("xfs_bunmapi", XFS_ERRLEVEL_LOW,
				 ip->i_mount);
		return XFS_ERROR(EFSCORRUPTED);
	}
	mp = ip->i_mount;
	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	ASSERT(len > 0);
	ASSERT(nexts >= 0);

	if (!(ifp->if_flags & XFS_IFEXTENTS) &&
	    (error = xfs_iread_extents(tp, ip, whichfork)))
		return error;
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	if (nextents == 0) {
		*done = 1;
		return 0;
	}
	XFS_STATS_INC(xs_blk_unmap);
	isrt = (whichfork == XFS_DATA_FORK) && XFS_IS_REALTIME_INODE(ip);
	start = bno;
	bno = start + len - 1;
	ep = xfs_bmap_search_extents(ip, bno, whichfork, &eof, &lastx, &got,
		&prev);

	if (eof) {
		ep = xfs_iext_get_ext(ifp, --lastx);
		xfs_bmbt_get_all(ep, &got);
		bno = got.br_startoff + got.br_blockcount - 1;
	}
	logflags = 0;
	if (ifp->if_flags & XFS_IFBROOT) {
		ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE);
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_private.b.firstblock = *firstblock;
		cur->bc_private.b.flist = flist;
		cur->bc_private.b.flags = 0;
	} else
		cur = NULL;

	if (isrt) {
		xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, mp->m_rbmip, XFS_ILOCK_EXCL);
	}

	extno = 0;
	while (bno != (xfs_fileoff_t)-1 && bno >= start && lastx >= 0 &&
	       (nexts == 0 || extno < nexts)) {
		if (got.br_startoff > bno) {
			if (--lastx < 0)
				break;
			ep = xfs_iext_get_ext(ifp, lastx);
			xfs_bmbt_get_all(ep, &got);
		}
		bno = XFS_FILEOFF_MIN(bno,
			got.br_startoff + got.br_blockcount - 1);
		if (bno < start)
			break;
		ASSERT(ep != NULL);
		del = got;
		wasdel = isnullstartblock(del.br_startblock);
		if (got.br_startoff < start) {
			del.br_startoff = start;
			del.br_blockcount -= start - got.br_startoff;
			if (!wasdel)
				del.br_startblock += start - got.br_startoff;
		}
		if (del.br_startoff + del.br_blockcount > bno + 1)
			del.br_blockcount = bno + 1 - del.br_startoff;
		sum = del.br_startblock + del.br_blockcount;
		if (isrt &&
		    (mod = do_mod(sum, mp->m_sb.sb_rextsize))) {
			/*
			 * Realtime extent not lined up at the end.
			 * The extent could have been split into written
			 * and unwritten pieces, or we could just be
			 * unmapping part of it.  But we can't really
			 * get rid of part of a realtime extent.
			 */
			if (del.br_state == XFS_EXT_UNWRITTEN ||
			    !xfs_sb_version_hasextflgbit(&mp->m_sb)) {
				/*
				 * This piece is unwritten, or we're not
				 * using unwritten extents.  Skip over it.
				 */
				ASSERT(bno >= mod);
				bno -= mod > del.br_blockcount ?
					del.br_blockcount : mod;
				if (bno < got.br_startoff) {
					if (--lastx >= 0)
						xfs_bmbt_get_all(xfs_iext_get_ext(
							ifp, lastx), &got);
				}
				continue;
			}
			/*
			 * It's written, turn it unwritten.
			 * This is better than zeroing it.
			 */
			ASSERT(del.br_state == XFS_EXT_NORM);
			ASSERT(xfs_trans_get_block_res(tp) > 0);
			if (del.br_blockcount > mod) {
				del.br_startoff += del.br_blockcount - mod;
				del.br_startblock += del.br_blockcount - mod;
				del.br_blockcount = mod;
			}
			del.br_state = XFS_EXT_UNWRITTEN;
			error = xfs_bmap_add_extent_unwritten_real(tp, ip,
					&lastx, &cur, &del, firstblock, flist,
					&logflags);
			if (error)
				goto error0;
			goto nodelete;
		}
		if (isrt && (mod = do_mod(del.br_startblock, mp->m_sb.sb_rextsize))) {
			mod = mp->m_sb.sb_rextsize - mod;
			if (del.br_blockcount > mod) {
				del.br_blockcount -= mod;
				del.br_startoff += mod;
				del.br_startblock += mod;
			} else if ((del.br_startoff == start &&
				    (del.br_state == XFS_EXT_UNWRITTEN ||
				     xfs_trans_get_block_res(tp) == 0)) ||
				   !xfs_sb_version_hasextflgbit(&mp->m_sb)) {
				/*
				 * Can't make it unwritten.  There isn't
				 * a full extent here so just skip it.
				 */
				ASSERT(bno >= del.br_blockcount);
				bno -= del.br_blockcount;
				if (got.br_startoff > bno) {
					if (--lastx >= 0) {
						ep = xfs_iext_get_ext(ifp,
								      lastx);
						xfs_bmbt_get_all(ep, &got);
					}
				}
				continue;
			} else if (del.br_state == XFS_EXT_UNWRITTEN) {
				/*
				 * This one is already unwritten.
				 * It must have a written left neighbor.
				 * Unwrite the killed part of that one and
				 * try again.
				 */
				ASSERT(lastx > 0);
				xfs_bmbt_get_all(xfs_iext_get_ext(ifp,
						lastx - 1), &prev);
				ASSERT(prev.br_state == XFS_EXT_NORM);
				ASSERT(!isnullstartblock(prev.br_startblock));
				ASSERT(del.br_startblock ==
				       prev.br_startblock + prev.br_blockcount);
				if (prev.br_startoff < start) {
					mod = start - prev.br_startoff;
					prev.br_blockcount -= mod;
					prev.br_startblock += mod;
					prev.br_startoff = start;
				}
				prev.br_state = XFS_EXT_UNWRITTEN;
				lastx--;
				error = xfs_bmap_add_extent_unwritten_real(tp,
						ip, &lastx, &cur, &prev,
						firstblock, flist, &logflags);
				if (error)
					goto error0;
				goto nodelete;
			} else {
				ASSERT(del.br_state == XFS_EXT_NORM);
				del.br_state = XFS_EXT_UNWRITTEN;
				error = xfs_bmap_add_extent_unwritten_real(tp,
						ip, &lastx, &cur, &del,
						firstblock, flist, &logflags);
				if (error)
					goto error0;
				goto nodelete;
			}
		}
		if (wasdel) {
			ASSERT(startblockval(del.br_startblock) > 0);
			
			if (isrt) {
				xfs_filblks_t rtexts;

				rtexts = XFS_FSB_TO_B(mp, del.br_blockcount);
				do_div(rtexts, mp->m_sb.sb_rextsize);
				xfs_mod_incore_sb(mp, XFS_SBS_FREXTENTS,
						(int64_t)rtexts, 0);
				(void)xfs_trans_reserve_quota_nblks(NULL,
					ip, -((long)del.br_blockcount), 0,
					XFS_QMOPT_RES_RTBLKS);
			} else {
				xfs_icsb_modify_counters(mp, XFS_SBS_FDBLOCKS,
						(int64_t)del.br_blockcount, 0);
				(void)xfs_trans_reserve_quota_nblks(NULL,
					ip, -((long)del.br_blockcount), 0,
					XFS_QMOPT_RES_REGBLKS);
			}
			ip->i_delayed_blks -= del.br_blockcount;
			if (cur)
				cur->bc_private.b.flags |=
					XFS_BTCUR_BPRV_WASDEL;
		} else if (cur)
			cur->bc_private.b.flags &= ~XFS_BTCUR_BPRV_WASDEL;
		if (!wasdel && xfs_trans_get_block_res(tp) == 0 &&
		    XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS &&
		    XFS_IFORK_NEXTENTS(ip, whichfork) >= 
			XFS_IFORK_MAXEXT(ip, whichfork) &&
		    del.br_startoff > got.br_startoff &&
		    del.br_startoff + del.br_blockcount <
		    got.br_startoff + got.br_blockcount) {
			error = XFS_ERROR(ENOSPC);
			goto error0;
		}
		error = xfs_bmap_del_extent(ip, tp, &lastx, flist, cur, &del,
				&tmp_logflags, whichfork);
		logflags |= tmp_logflags;
		if (error)
			goto error0;
		bno = del.br_startoff - 1;
nodelete:
		if (bno != (xfs_fileoff_t)-1 && bno >= start) {
			if (lastx >= 0) {
				ep = xfs_iext_get_ext(ifp, lastx);
				if (xfs_bmbt_get_startoff(ep) > bno) {
					if (--lastx >= 0)
						ep = xfs_iext_get_ext(ifp,
								      lastx);
				}
				xfs_bmbt_get_all(ep, &got);
			}
			extno++;
		}
	}
	*done = bno == (xfs_fileoff_t)-1 || bno < start || lastx < 0;

	if (xfs_bmap_needs_btree(ip, whichfork)) {
		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, firstblock, flist,
			&cur, 0, &tmp_logflags, whichfork);
		logflags |= tmp_logflags;
		if (error)
			goto error0;
	}
	else if (xfs_bmap_wants_extents(ip, whichfork)) {
		ASSERT(cur != NULL);
		error = xfs_bmap_btree_to_extents(tp, ip, cur, &tmp_logflags,
			whichfork);
		logflags |= tmp_logflags;
		if (error)
			goto error0;
	}
	error = 0;
error0:
	if ((logflags & xfs_ilog_fext(whichfork)) &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS)
		logflags &= ~xfs_ilog_fext(whichfork);
	else if ((logflags & xfs_ilog_fbroot(whichfork)) &&
		 XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE)
		logflags &= ~xfs_ilog_fbroot(whichfork);
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	if (cur) {
		if (!error) {
			*firstblock = cur->bc_private.b.firstblock;
			cur->bc_private.b.allocated = 0;
		}
		xfs_btree_del_cursor(cur,
			error ? XFS_BTREE_ERROR : XFS_BTREE_NOERROR);
	}
	return error;
}

STATIC int
xfs_getbmapx_fix_eof_hole(
	xfs_inode_t		*ip,		
	struct getbmapx		*out,		
	int			prealloced,	
	__int64_t		end,		
	xfs_fsblock_t		startblock)
{
	__int64_t		fixlen;
	xfs_mount_t		*mp;		
	xfs_ifork_t		*ifp;		
	xfs_extnum_t		lastx;		
	xfs_fileoff_t		fileblock;

	if (startblock == HOLESTARTBLOCK) {
		mp = ip->i_mount;
		out->bmv_block = -1;
		fixlen = XFS_FSB_TO_BB(mp, XFS_B_TO_FSB(mp, XFS_ISIZE(ip)));
		fixlen -= out->bmv_offset;
		if (prealloced && out->bmv_offset + out->bmv_length == end) {
			
			if (fixlen <= 0)
				return 0;
			out->bmv_length = fixlen;
		}
	} else {
		if (startblock == DELAYSTARTBLOCK)
			out->bmv_block = -2;
		else
			out->bmv_block = xfs_fsb_to_db(ip, startblock);
		fileblock = XFS_BB_TO_FSB(ip->i_mount, out->bmv_offset);
		ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
		if (xfs_iext_bno_to_ext(ifp, fileblock, &lastx) &&
		   (lastx == (ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t))-1))
			out->bmv_oflags |= BMV_OF_LAST;
	}

	return 1;
}

int						
xfs_getbmap(
	xfs_inode_t		*ip,
	struct getbmapx		*bmv,		
	xfs_bmap_format_t	formatter,	
	void			*arg)		
{
	__int64_t		bmvend;		
	int			error = 0;	
	__int64_t		fixlen;		
	int			i;		
	int			lock;		
	xfs_bmbt_irec_t		*map;		
	xfs_mount_t		*mp;		
	int			nex;		
	int			nexleft;	
	int			subnex;		
	int			nmap;		
	struct getbmapx		*out;		
	int			whichfork;	
	int			prealloced;	
	int			iflags;		
	int			bmapi_flags;	
	int			cur_ext = 0;

	mp = ip->i_mount;
	iflags = bmv->bmv_iflags;
	whichfork = iflags & BMV_IF_ATTRFORK ? XFS_ATTR_FORK : XFS_DATA_FORK;

	if (whichfork == XFS_ATTR_FORK) {
		if (XFS_IFORK_Q(ip)) {
			if (ip->i_d.di_aformat != XFS_DINODE_FMT_EXTENTS &&
			    ip->i_d.di_aformat != XFS_DINODE_FMT_BTREE &&
			    ip->i_d.di_aformat != XFS_DINODE_FMT_LOCAL)
				return XFS_ERROR(EINVAL);
		} else if (unlikely(
			   ip->i_d.di_aformat != 0 &&
			   ip->i_d.di_aformat != XFS_DINODE_FMT_EXTENTS)) {
			XFS_ERROR_REPORT("xfs_getbmap", XFS_ERRLEVEL_LOW,
					 ip->i_mount);
			return XFS_ERROR(EFSCORRUPTED);
		}

		prealloced = 0;
		fixlen = 1LL << 32;
	} else {
		if (ip->i_d.di_format != XFS_DINODE_FMT_EXTENTS &&
		    ip->i_d.di_format != XFS_DINODE_FMT_BTREE &&
		    ip->i_d.di_format != XFS_DINODE_FMT_LOCAL)
			return XFS_ERROR(EINVAL);

		if (xfs_get_extsz_hint(ip) ||
		    ip->i_d.di_flags & (XFS_DIFLAG_PREALLOC|XFS_DIFLAG_APPEND)){
			prealloced = 1;
			fixlen = XFS_MAXIOFFSET(mp);
		} else {
			prealloced = 0;
			fixlen = XFS_ISIZE(ip);
		}
	}

	if (bmv->bmv_length == -1) {
		fixlen = XFS_FSB_TO_BB(mp, XFS_B_TO_FSB(mp, fixlen));
		bmv->bmv_length =
			max_t(__int64_t, fixlen - bmv->bmv_offset, 0);
	} else if (bmv->bmv_length == 0) {
		bmv->bmv_entries = 0;
		return 0;
	} else if (bmv->bmv_length < 0) {
		return XFS_ERROR(EINVAL);
	}

	nex = bmv->bmv_count - 1;
	if (nex <= 0)
		return XFS_ERROR(EINVAL);
	bmvend = bmv->bmv_offset + bmv->bmv_length;


	if (bmv->bmv_count > ULONG_MAX / sizeof(struct getbmapx))
		return XFS_ERROR(ENOMEM);
	out = kmem_zalloc(bmv->bmv_count * sizeof(struct getbmapx), KM_MAYFAIL);
	if (!out) {
		out = kmem_zalloc_large(bmv->bmv_count *
					sizeof(struct getbmapx));
		if (!out)
			return XFS_ERROR(ENOMEM);
	}

	xfs_ilock(ip, XFS_IOLOCK_SHARED);
	if (whichfork == XFS_DATA_FORK && !(iflags & BMV_IF_DELALLOC)) {
		if (ip->i_delayed_blks || XFS_ISIZE(ip) > ip->i_d.di_size) {
			error = xfs_flush_pages(ip, 0, -1, 0, FI_REMAPF);
			if (error)
				goto out_unlock_iolock;
		}
	}

	lock = xfs_ilock_map_shared(ip);

	if (nex > XFS_IFORK_NEXTENTS(ip, whichfork) * 2 + 1)
		nex = XFS_IFORK_NEXTENTS(ip, whichfork) * 2 + 1;

	bmapi_flags = xfs_bmapi_aflag(whichfork);
	if (!(iflags & BMV_IF_PREALLOC))
		bmapi_flags |= XFS_BMAPI_IGSTATE;

	error = ENOMEM;
	subnex = 16;
	map = kmem_alloc(subnex * sizeof(*map), KM_MAYFAIL | KM_NOFS);
	if (!map)
		goto out_unlock_ilock;

	bmv->bmv_entries = 0;

	if (XFS_IFORK_NEXTENTS(ip, whichfork) == 0 &&
	    (whichfork == XFS_ATTR_FORK || !(iflags & BMV_IF_DELALLOC))) {
		error = 0;
		goto out_free_map;
	}

	nexleft = nex;

	do {
		nmap = (nexleft > subnex) ? subnex : nexleft;
		error = xfs_bmapi_read(ip, XFS_BB_TO_FSBT(mp, bmv->bmv_offset),
				       XFS_BB_TO_FSB(mp, bmv->bmv_length),
				       map, &nmap, bmapi_flags);
		if (error)
			goto out_free_map;
		ASSERT(nmap <= subnex);

		for (i = 0; i < nmap && nexleft && bmv->bmv_length; i++) {
			out[cur_ext].bmv_oflags = 0;
			if (map[i].br_state == XFS_EXT_UNWRITTEN)
				out[cur_ext].bmv_oflags |= BMV_OF_PREALLOC;
			else if (map[i].br_startblock == DELAYSTARTBLOCK)
				out[cur_ext].bmv_oflags |= BMV_OF_DELALLOC;
			out[cur_ext].bmv_offset =
				XFS_FSB_TO_BB(mp, map[i].br_startoff);
			out[cur_ext].bmv_length =
				XFS_FSB_TO_BB(mp, map[i].br_blockcount);
			out[cur_ext].bmv_unused1 = 0;
			out[cur_ext].bmv_unused2 = 0;
			ASSERT(((iflags & BMV_IF_DELALLOC) != 0) ||
			      (map[i].br_startblock != DELAYSTARTBLOCK));
                        if (map[i].br_startblock == HOLESTARTBLOCK &&
			    whichfork == XFS_ATTR_FORK) {
				
				out[cur_ext].bmv_oflags |= BMV_OF_LAST;
				goto out_free_map;
			}

			if (!xfs_getbmapx_fix_eof_hole(ip, &out[cur_ext],
					prealloced, bmvend,
					map[i].br_startblock))
				goto out_free_map;

			bmv->bmv_offset =
				out[cur_ext].bmv_offset +
				out[cur_ext].bmv_length;
			bmv->bmv_length =
				max_t(__int64_t, 0, bmvend - bmv->bmv_offset);

			if ((iflags & BMV_IF_NO_HOLES) &&
			    map[i].br_startblock == HOLESTARTBLOCK) {
				memset(&out[cur_ext], 0, sizeof(out[cur_ext]));
				continue;
			}

			nexleft--;
			bmv->bmv_entries++;
			cur_ext++;
		}
	} while (nmap && nexleft && bmv->bmv_length);

 out_free_map:
	kmem_free(map);
 out_unlock_ilock:
	xfs_iunlock_map_shared(ip, lock);
 out_unlock_iolock:
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);

	for (i = 0; i < cur_ext; i++) {
		int full = 0;	

		
		error = formatter(&arg, &out[i], &full);
		if (error || full)
			break;
	}

	if (is_vmalloc_addr(out))
		kmem_free_large(out);
	else
		kmem_free(out);
	return error;
}

#ifdef DEBUG
STATIC struct xfs_buf *
xfs_bmap_get_bp(
	struct xfs_btree_cur	*cur,
	xfs_fsblock_t		bno)
{
	struct xfs_log_item_desc *lidp;
	int			i;

	if (!cur)
		return NULL;

	for (i = 0; i < XFS_BTREE_MAXLEVELS; i++) {
		if (!cur->bc_bufs[i])
			break;
		if (XFS_BUF_ADDR(cur->bc_bufs[i]) == bno)
			return cur->bc_bufs[i];
	}

	
	list_for_each_entry(lidp, &cur->bc_tp->t_items, lid_trans) {
		struct xfs_buf_log_item	*bip;
		bip = (struct xfs_buf_log_item *)lidp->lid_item;
		if (bip->bli_item.li_type == XFS_LI_BUF &&
		    XFS_BUF_ADDR(bip->bli_buf) == bno)
			return bip->bli_buf;
	}

	return NULL;
}

STATIC void
xfs_check_block(
	struct xfs_btree_block	*block,
	xfs_mount_t		*mp,
	int			root,
	short			sz)
{
	int			i, j, dmxr;
	__be64			*pp, *thispa;	
	xfs_bmbt_key_t		*prevp, *keyp;

	ASSERT(be16_to_cpu(block->bb_level) > 0);

	prevp = NULL;
	for( i = 1; i <= xfs_btree_get_numrecs(block); i++) {
		dmxr = mp->m_bmap_dmxr[0];
		keyp = XFS_BMBT_KEY_ADDR(mp, block, i);

		if (prevp) {
			ASSERT(be64_to_cpu(prevp->br_startoff) <
			       be64_to_cpu(keyp->br_startoff));
		}
		prevp = keyp;

		if (root)
			pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, i, sz);
		else
			pp = XFS_BMBT_PTR_ADDR(mp, block, i, dmxr);

		for (j = i+1; j <= be16_to_cpu(block->bb_numrecs); j++) {
			if (root)
				thispa = XFS_BMAP_BROOT_PTR_ADDR(mp, block, j, sz);
			else
				thispa = XFS_BMBT_PTR_ADDR(mp, block, j, dmxr);
			if (*thispa == *pp) {
				xfs_warn(mp, "%s: thispa(%d) == pp(%d) %Ld",
					__func__, j, i,
					(unsigned long long)be64_to_cpu(*thispa));
				panic("%s: ptrs are equal in node\n",
					__func__);
			}
		}
	}
}


STATIC void
xfs_bmap_check_leaf_extents(
	xfs_btree_cur_t		*cur,	
	xfs_inode_t		*ip,		
	int			whichfork)	
{
	struct xfs_btree_block	*block;	
	xfs_fsblock_t		bno;	
	xfs_buf_t		*bp;	
	int			error;	
	xfs_extnum_t		i=0, j;	
	xfs_ifork_t		*ifp;	
	int			level;	
	xfs_mount_t		*mp;	
	__be64			*pp;	
	xfs_bmbt_rec_t		*ep;	
	xfs_bmbt_rec_t		last = {0, 0}; 
	xfs_bmbt_rec_t		*nextp;	
	int			bp_release = 0;

	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE) {
		return;
	}

	bno = NULLFSBLOCK;
	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	block = ifp->if_broot;
	level = be16_to_cpu(block->bb_level);
	ASSERT(level > 0);
	xfs_check_block(block, mp, 1, ifp->if_broot_bytes);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, 1, ifp->if_broot_bytes);
	bno = be64_to_cpu(*pp);

	ASSERT(bno != NULLDFSBNO);
	ASSERT(XFS_FSB_TO_AGNO(mp, bno) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, bno) < mp->m_sb.sb_agblocks);

	while (level-- > 0) {
		
		bp = xfs_bmap_get_bp(cur, XFS_FSB_TO_DADDR(mp, bno));
		if (bp) {
			bp_release = 0;
		} else {
			bp_release = 1;
		}
		if (!bp && (error = xfs_btree_read_bufl(mp, NULL, bno, 0, &bp,
				XFS_BMAP_BTREE_REF)))
			goto error_norelse;
		block = XFS_BUF_TO_BLOCK(bp);
		XFS_WANT_CORRUPTED_GOTO(
			xfs_bmap_sanity_check(mp, bp, level),
			error0);
		if (level == 0)
			break;


		xfs_check_block(block, mp, 0, 0);
		pp = XFS_BMBT_PTR_ADDR(mp, block, 1, mp->m_bmap_dmxr[1]);
		bno = be64_to_cpu(*pp);
		XFS_WANT_CORRUPTED_GOTO(XFS_FSB_SANITY_CHECK(mp, bno), error0);
		if (bp_release) {
			bp_release = 0;
			xfs_trans_brelse(NULL, bp);
		}
	}

	i = 0;

	for (;;) {
		xfs_fsblock_t	nextbno;
		xfs_extnum_t	num_recs;


		num_recs = xfs_btree_get_numrecs(block);


		nextbno = be64_to_cpu(block->bb_u.l.bb_rightsib);


		ep = XFS_BMBT_REC_ADDR(mp, block, 1);
		if (i) {
			ASSERT(xfs_bmbt_disk_get_startoff(&last) +
			       xfs_bmbt_disk_get_blockcount(&last) <=
			       xfs_bmbt_disk_get_startoff(ep));
		}
		for (j = 1; j < num_recs; j++) {
			nextp = XFS_BMBT_REC_ADDR(mp, block, j + 1);
			ASSERT(xfs_bmbt_disk_get_startoff(ep) +
			       xfs_bmbt_disk_get_blockcount(ep) <=
			       xfs_bmbt_disk_get_startoff(nextp));
			ep = nextp;
		}

		last = *ep;
		i += num_recs;
		if (bp_release) {
			bp_release = 0;
			xfs_trans_brelse(NULL, bp);
		}
		bno = nextbno;
		if (bno == NULLFSBLOCK)
			break;

		bp = xfs_bmap_get_bp(cur, XFS_FSB_TO_DADDR(mp, bno));
		if (bp) {
			bp_release = 0;
		} else {
			bp_release = 1;
		}
		if (!bp && (error = xfs_btree_read_bufl(mp, NULL, bno, 0, &bp,
				XFS_BMAP_BTREE_REF)))
			goto error_norelse;
		block = XFS_BUF_TO_BLOCK(bp);
	}
	if (bp_release) {
		bp_release = 0;
		xfs_trans_brelse(NULL, bp);
	}
	return;

error0:
	xfs_warn(mp, "%s: at error0", __func__);
	if (bp_release)
		xfs_trans_brelse(NULL, bp);
error_norelse:
	xfs_warn(mp, "%s: BAD after btree leaves for %d extents",
		__func__, i);
	panic("%s: CORRUPTED BTREE OR SOMETHING", __func__);
	return;
}
#endif

int						
xfs_bmap_count_blocks(
	xfs_trans_t		*tp,		
	xfs_inode_t		*ip,		
	int			whichfork,	
	int			*count)		
{
	struct xfs_btree_block	*block;	
	xfs_fsblock_t		bno;	
	xfs_ifork_t		*ifp;	
	int			level;	
	xfs_mount_t		*mp;	
	__be64			*pp;	

	bno = NULLFSBLOCK;
	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	if ( XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS ) {
		xfs_bmap_count_leaves(ifp, 0,
			ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t),
			count);
		return 0;
	}

	block = ifp->if_broot;
	level = be16_to_cpu(block->bb_level);
	ASSERT(level > 0);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, 1, ifp->if_broot_bytes);
	bno = be64_to_cpu(*pp);
	ASSERT(bno != NULLDFSBNO);
	ASSERT(XFS_FSB_TO_AGNO(mp, bno) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, bno) < mp->m_sb.sb_agblocks);

	if (unlikely(xfs_bmap_count_tree(mp, tp, ifp, bno, level, count) < 0)) {
		XFS_ERROR_REPORT("xfs_bmap_count_blocks(2)", XFS_ERRLEVEL_LOW,
				 mp);
		return XFS_ERROR(EFSCORRUPTED);
	}

	return 0;
}

STATIC int                                     
xfs_bmap_count_tree(
	xfs_mount_t     *mp,            
	xfs_trans_t     *tp,            
	xfs_ifork_t	*ifp,		
	xfs_fsblock_t   blockno,	
	int             levelin,	
	int		*count)		
{
	int			error;
	xfs_buf_t		*bp, *nbp;
	int			level = levelin;
	__be64			*pp;
	xfs_fsblock_t           bno = blockno;
	xfs_fsblock_t		nextbno;
	struct xfs_btree_block	*block, *nextblock;
	int			numrecs;

	if ((error = xfs_btree_read_bufl(mp, tp, bno, 0, &bp, XFS_BMAP_BTREE_REF)))
		return error;
	*count += 1;
	block = XFS_BUF_TO_BLOCK(bp);

	if (--level) {
		
		nextbno = be64_to_cpu(block->bb_u.l.bb_rightsib);
		while (nextbno != NULLFSBLOCK) {
			if ((error = xfs_btree_read_bufl(mp, tp, nextbno,
				0, &nbp, XFS_BMAP_BTREE_REF)))
				return error;
			*count += 1;
			nextblock = XFS_BUF_TO_BLOCK(nbp);
			nextbno = be64_to_cpu(nextblock->bb_u.l.bb_rightsib);
			xfs_trans_brelse(tp, nbp);
		}

		
		pp = XFS_BMBT_PTR_ADDR(mp, block, 1, mp->m_bmap_dmxr[1]);
		bno = be64_to_cpu(*pp);
		if (unlikely((error =
		     xfs_bmap_count_tree(mp, tp, ifp, bno, level, count)) < 0)) {
			xfs_trans_brelse(tp, bp);
			XFS_ERROR_REPORT("xfs_bmap_count_tree(1)",
					 XFS_ERRLEVEL_LOW, mp);
			return XFS_ERROR(EFSCORRUPTED);
		}
		xfs_trans_brelse(tp, bp);
	} else {
		
		for (;;) {
			nextbno = be64_to_cpu(block->bb_u.l.bb_rightsib);
			numrecs = be16_to_cpu(block->bb_numrecs);
			xfs_bmap_disk_count_leaves(mp, block, numrecs, count);
			xfs_trans_brelse(tp, bp);
			if (nextbno == NULLFSBLOCK)
				break;
			bno = nextbno;
			if ((error = xfs_btree_read_bufl(mp, tp, bno, 0, &bp,
				XFS_BMAP_BTREE_REF)))
				return error;
			*count += 1;
			block = XFS_BUF_TO_BLOCK(bp);
		}
	}
	return 0;
}

STATIC void
xfs_bmap_count_leaves(
	xfs_ifork_t		*ifp,
	xfs_extnum_t		idx,
	int			numrecs,
	int			*count)
{
	int		b;

	for (b = 0; b < numrecs; b++) {
		xfs_bmbt_rec_host_t *frp = xfs_iext_get_ext(ifp, idx + b);
		*count += xfs_bmbt_get_blockcount(frp);
	}
}

STATIC void
xfs_bmap_disk_count_leaves(
	struct xfs_mount	*mp,
	struct xfs_btree_block	*block,
	int			numrecs,
	int			*count)
{
	int		b;
	xfs_bmbt_rec_t	*frp;

	for (b = 1; b <= numrecs; b++) {
		frp = XFS_BMBT_REC_ADDR(mp, block, b);
		*count += xfs_bmbt_disk_get_blockcount(frp);
	}
}

int
xfs_bmap_punch_delalloc_range(
	struct xfs_inode	*ip,
	xfs_fileoff_t		start_fsb,
	xfs_fileoff_t		length)
{
	xfs_fileoff_t		remaining = length;
	int			error = 0;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	do {
		int		done;
		xfs_bmbt_irec_t	imap;
		int		nimaps = 1;
		xfs_fsblock_t	firstblock;
		xfs_bmap_free_t flist;

		error = xfs_bmapi_read(ip, start_fsb, 1, &imap, &nimaps,
				       XFS_BMAPI_ENTIRE);

		if (error) {
			
			if (!XFS_FORCED_SHUTDOWN(ip->i_mount)) {
				xfs_alert(ip->i_mount,
			"Failed delalloc mapping lookup ino %lld fsb %lld.",
						ip->i_ino, start_fsb);
			}
			break;
		}
		if (!nimaps) {
			
			goto next_block;
		}
		if (imap.br_startblock != DELAYSTARTBLOCK) {
			
			goto next_block;
		}
		WARN_ON(imap.br_blockcount == 0);

		xfs_bmap_init(&flist, &firstblock);
		error = xfs_bunmapi(NULL, ip, start_fsb, 1, 0, 1, &firstblock,
					&flist, &done);
		if (error)
			break;

		ASSERT(!flist.xbf_count && !flist.xbf_first);
next_block:
		start_fsb++;
		remaining--;
	} while(remaining > 0);

	return error;
}
