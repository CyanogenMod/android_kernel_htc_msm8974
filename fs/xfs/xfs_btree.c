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
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_btree.h"
#include "xfs_error.h"
#include "xfs_trace.h"

kmem_zone_t	*xfs_btree_cur_zone;

const __uint32_t xfs_magics[XFS_BTNUM_MAX] = {
	XFS_ABTB_MAGIC, XFS_ABTC_MAGIC, XFS_BMAP_MAGIC, XFS_IBT_MAGIC
};


STATIC int				
xfs_btree_check_lblock(
	struct xfs_btree_cur	*cur,	
	struct xfs_btree_block	*block,	
	int			level,	
	struct xfs_buf		*bp)	
{
	int			lblock_ok; 
	struct xfs_mount	*mp;	

	mp = cur->bc_mp;
	lblock_ok =
		be32_to_cpu(block->bb_magic) == xfs_magics[cur->bc_btnum] &&
		be16_to_cpu(block->bb_level) == level &&
		be16_to_cpu(block->bb_numrecs) <=
			cur->bc_ops->get_maxrecs(cur, level) &&
		block->bb_u.l.bb_leftsib &&
		(block->bb_u.l.bb_leftsib == cpu_to_be64(NULLDFSBNO) ||
		 XFS_FSB_SANITY_CHECK(mp,
		 	be64_to_cpu(block->bb_u.l.bb_leftsib))) &&
		block->bb_u.l.bb_rightsib &&
		(block->bb_u.l.bb_rightsib == cpu_to_be64(NULLDFSBNO) ||
		 XFS_FSB_SANITY_CHECK(mp,
		 	be64_to_cpu(block->bb_u.l.bb_rightsib)));
	if (unlikely(XFS_TEST_ERROR(!lblock_ok, mp,
			XFS_ERRTAG_BTREE_CHECK_LBLOCK,
			XFS_RANDOM_BTREE_CHECK_LBLOCK))) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		XFS_ERROR_REPORT("xfs_btree_check_lblock", XFS_ERRLEVEL_LOW,
				 mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

STATIC int				
xfs_btree_check_sblock(
	struct xfs_btree_cur	*cur,	
	struct xfs_btree_block	*block,	
	int			level,	
	struct xfs_buf		*bp)	
{
	struct xfs_buf		*agbp;	
	struct xfs_agf		*agf;	
	xfs_agblock_t		agflen;	
	int			sblock_ok; 

	agbp = cur->bc_private.a.agbp;
	agf = XFS_BUF_TO_AGF(agbp);
	agflen = be32_to_cpu(agf->agf_length);
	sblock_ok =
		be32_to_cpu(block->bb_magic) == xfs_magics[cur->bc_btnum] &&
		be16_to_cpu(block->bb_level) == level &&
		be16_to_cpu(block->bb_numrecs) <=
			cur->bc_ops->get_maxrecs(cur, level) &&
		(block->bb_u.s.bb_leftsib == cpu_to_be32(NULLAGBLOCK) ||
		 be32_to_cpu(block->bb_u.s.bb_leftsib) < agflen) &&
		block->bb_u.s.bb_leftsib &&
		(block->bb_u.s.bb_rightsib == cpu_to_be32(NULLAGBLOCK) ||
		 be32_to_cpu(block->bb_u.s.bb_rightsib) < agflen) &&
		block->bb_u.s.bb_rightsib;
	if (unlikely(XFS_TEST_ERROR(!sblock_ok, cur->bc_mp,
			XFS_ERRTAG_BTREE_CHECK_SBLOCK,
			XFS_RANDOM_BTREE_CHECK_SBLOCK))) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		XFS_CORRUPTION_ERROR("xfs_btree_check_sblock",
			XFS_ERRLEVEL_LOW, cur->bc_mp, block);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

int
xfs_btree_check_block(
	struct xfs_btree_cur	*cur,	
	struct xfs_btree_block	*block,	
	int			level,	
	struct xfs_buf		*bp)	
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_check_lblock(cur, block, level, bp);
	else
		return xfs_btree_check_sblock(cur, block, level, bp);
}

int					
xfs_btree_check_lptr(
	struct xfs_btree_cur	*cur,	
	xfs_dfsbno_t		bno,	
	int			level)	
{
	XFS_WANT_CORRUPTED_RETURN(
		level > 0 &&
		bno != NULLDFSBNO &&
		XFS_FSB_SANITY_CHECK(cur->bc_mp, bno));
	return 0;
}

#ifdef DEBUG
STATIC int				
xfs_btree_check_sptr(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	int			level)	
{
	xfs_agblock_t		agblocks = cur->bc_mp->m_sb.sb_agblocks;

	XFS_WANT_CORRUPTED_RETURN(
		level > 0 &&
		bno != NULLAGBLOCK &&
		bno != 0 &&
		bno < agblocks);
	return 0;
}

STATIC int				
xfs_btree_check_ptr(
	struct xfs_btree_cur	*cur,	
	union xfs_btree_ptr	*ptr,	
	int			index,	
	int			level)	
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		return xfs_btree_check_lptr(cur,
				be64_to_cpu((&ptr->l)[index]), level);
	} else {
		return xfs_btree_check_sptr(cur,
				be32_to_cpu((&ptr->s)[index]), level);
	}
}
#endif

void
xfs_btree_del_cursor(
	xfs_btree_cur_t	*cur,		
	int		error)		
{
	int		i;		

	for (i = 0; i < cur->bc_nlevels; i++) {
		if (cur->bc_bufs[i])
			xfs_trans_brelse(cur->bc_tp, cur->bc_bufs[i]);
		else if (!error)
			break;
	}
	ASSERT(cur->bc_btnum != XFS_BTNUM_BMAP ||
	       cur->bc_private.b.allocated == 0);
	kmem_zone_free(xfs_btree_cur_zone, cur);
}

int					
xfs_btree_dup_cursor(
	xfs_btree_cur_t	*cur,		
	xfs_btree_cur_t	**ncur)		
{
	xfs_buf_t	*bp;		
	int		error;		
	int		i;		
	xfs_mount_t	*mp;		
	xfs_btree_cur_t	*new;		
	xfs_trans_t	*tp;		

	tp = cur->bc_tp;
	mp = cur->bc_mp;

	new = cur->bc_ops->dup_cursor(cur);

	new->bc_rec = cur->bc_rec;

	for (i = 0; i < new->bc_nlevels; i++) {
		new->bc_ptrs[i] = cur->bc_ptrs[i];
		new->bc_ra[i] = cur->bc_ra[i];
		if ((bp = cur->bc_bufs[i])) {
			if ((error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
				XFS_BUF_ADDR(bp), mp->m_bsize, 0, &bp))) {
				xfs_btree_del_cursor(new, error);
				*ncur = NULL;
				return error;
			}
			new->bc_bufs[i] = bp;
			ASSERT(!xfs_buf_geterror(bp));
		} else
			new->bc_bufs[i] = NULL;
	}
	*ncur = new;
	return 0;
}


static inline size_t xfs_btree_block_len(struct xfs_btree_cur *cur)
{
	return (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
		XFS_BTREE_LBLOCK_LEN :
		XFS_BTREE_SBLOCK_LEN;
}

static inline size_t xfs_btree_ptr_len(struct xfs_btree_cur *cur)
{
	return (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
		sizeof(__be64) : sizeof(__be32);
}

STATIC size_t
xfs_btree_rec_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->rec_len;
}

STATIC size_t
xfs_btree_key_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->key_len;
}

STATIC size_t
xfs_btree_ptr_offset(
	struct xfs_btree_cur	*cur,
	int			n,
	int			level)
{
	return xfs_btree_block_len(cur) +
		cur->bc_ops->get_maxrecs(cur, level) * cur->bc_ops->key_len +
		(n - 1) * xfs_btree_ptr_len(cur);
}

STATIC union xfs_btree_rec *
xfs_btree_rec_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_rec *)
		((char *)block + xfs_btree_rec_offset(cur, n));
}

STATIC union xfs_btree_key *
xfs_btree_key_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_key *)
		((char *)block + xfs_btree_key_offset(cur, n));
}

STATIC union xfs_btree_ptr *
xfs_btree_ptr_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	int			level = xfs_btree_get_level(block);

	ASSERT(block->bb_level != 0);

	return (union xfs_btree_ptr *)
		((char *)block + xfs_btree_ptr_offset(cur, n, level));
}

STATIC struct xfs_btree_block *
xfs_btree_get_iroot(
       struct xfs_btree_cur    *cur)
{
       struct xfs_ifork        *ifp;

       ifp = XFS_IFORK_PTR(cur->bc_private.b.ip, cur->bc_private.b.whichfork);
       return (struct xfs_btree_block *)ifp->if_broot;
}

STATIC struct xfs_btree_block *		
xfs_btree_get_block(
	struct xfs_btree_cur	*cur,	
	int			level,	
	struct xfs_buf		**bpp)	
{
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*bpp = NULL;
		return xfs_btree_get_iroot(cur);
	}

	*bpp = cur->bc_bufs[level];
	return XFS_BUF_TO_BLOCK(*bpp);
}

xfs_buf_t *				
xfs_btree_get_bufl(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_fsblock_t	fsbno,		
	uint		lock)		
{
	xfs_buf_t	*bp;		
	xfs_daddr_t		d;		

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d, mp->m_bsize, lock);
	ASSERT(!xfs_buf_geterror(bp));
	return bp;
}

xfs_buf_t *				
xfs_btree_get_bufs(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_agnumber_t	agno,		
	xfs_agblock_t	agbno,		
	uint		lock)		
{
	xfs_buf_t	*bp;		
	xfs_daddr_t		d;		

	ASSERT(agno != NULLAGNUMBER);
	ASSERT(agbno != NULLAGBLOCK);
	d = XFS_AGB_TO_DADDR(mp, agno, agbno);
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d, mp->m_bsize, lock);
	ASSERT(!xfs_buf_geterror(bp));
	return bp;
}

int					
xfs_btree_islastblock(
	xfs_btree_cur_t		*cur,	
	int			level)	
{
	struct xfs_btree_block	*block;	
	xfs_buf_t		*bp;	

	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return block->bb_u.l.bb_rightsib == cpu_to_be64(NULLDFSBNO);
	else
		return block->bb_u.s.bb_rightsib == cpu_to_be32(NULLAGBLOCK);
}

STATIC int				
xfs_btree_firstrec(
	xfs_btree_cur_t		*cur,	
	int			level)	
{
	struct xfs_btree_block	*block;	
	xfs_buf_t		*bp;	

	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	if (!block->bb_numrecs)
		return 0;
	cur->bc_ptrs[level] = 1;
	return 1;
}

STATIC int				
xfs_btree_lastrec(
	xfs_btree_cur_t		*cur,	
	int			level)	
{
	struct xfs_btree_block	*block;	
	xfs_buf_t		*bp;	

	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	if (!block->bb_numrecs)
		return 0;
	cur->bc_ptrs[level] = be16_to_cpu(block->bb_numrecs);
	return 1;
}

void
xfs_btree_offsets(
	__int64_t	fields,		
	const short	*offsets,	
	int		nbits,		
	int		*first,		
	int		*last)		
{
	int		i;		
	__int64_t	imask;		

	ASSERT(fields != 0);
	for (i = 0, imask = 1LL; ; i++, imask <<= 1) {
		if (imask & fields) {
			*first = offsets[i];
			break;
		}
	}
	for (i = nbits - 1, imask = 1LL << i; ; i--, imask >>= 1) {
		if (imask & fields) {
			*last = offsets[i + 1] - 1;
			break;
		}
	}
}

int					
xfs_btree_read_bufl(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_fsblock_t	fsbno,		
	uint		lock,		
	xfs_buf_t	**bpp,		
	int		refval)		
{
	xfs_buf_t	*bp;		
	xfs_daddr_t		d;		
	int		error;

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	if ((error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, d,
			mp->m_bsize, lock, &bp))) {
		return error;
	}
	ASSERT(!xfs_buf_geterror(bp));
	if (bp)
		xfs_buf_set_ref(bp, refval);
	*bpp = bp;
	return 0;
}

void
xfs_btree_reada_bufl(
	xfs_mount_t	*mp,		
	xfs_fsblock_t	fsbno,		
	xfs_extlen_t	count)		
{
	xfs_daddr_t		d;

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	xfs_buf_readahead(mp->m_ddev_targp, d, mp->m_bsize * count);
}

void
xfs_btree_reada_bufs(
	xfs_mount_t	*mp,		
	xfs_agnumber_t	agno,		
	xfs_agblock_t	agbno,		
	xfs_extlen_t	count)		
{
	xfs_daddr_t		d;

	ASSERT(agno != NULLAGNUMBER);
	ASSERT(agbno != NULLAGBLOCK);
	d = XFS_AGB_TO_DADDR(mp, agno, agbno);
	xfs_buf_readahead(mp->m_ddev_targp, d, mp->m_bsize * count);
}

STATIC int
xfs_btree_readahead_lblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block	*block)
{
	int			rval = 0;
	xfs_dfsbno_t		left = be64_to_cpu(block->bb_u.l.bb_leftsib);
	xfs_dfsbno_t		right = be64_to_cpu(block->bb_u.l.bb_rightsib);

	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLDFSBNO) {
		xfs_btree_reada_bufl(cur->bc_mp, left, 1);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLDFSBNO) {
		xfs_btree_reada_bufl(cur->bc_mp, right, 1);
		rval++;
	}

	return rval;
}

STATIC int
xfs_btree_readahead_sblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block *block)
{
	int			rval = 0;
	xfs_agblock_t		left = be32_to_cpu(block->bb_u.s.bb_leftsib);
	xfs_agblock_t		right = be32_to_cpu(block->bb_u.s.bb_rightsib);


	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_private.a.agno,
				     left, 1);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_private.a.agno,
				     right, 1);
		rval++;
	}

	return rval;
}

STATIC int
xfs_btree_readahead(
	struct xfs_btree_cur	*cur,		
	int			lev,		
	int			lr)		
{
	struct xfs_btree_block	*block;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (lev == cur->bc_nlevels - 1))
		return 0;

	if ((cur->bc_ra[lev] | lr) == cur->bc_ra[lev])
		return 0;

	cur->bc_ra[lev] |= lr;
	block = XFS_BUF_TO_BLOCK(cur->bc_bufs[lev]);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_readahead_lblock(cur, lr, block);
	return xfs_btree_readahead_sblock(cur, lr, block);
}

STATIC void
xfs_btree_setbuf(
	xfs_btree_cur_t		*cur,	
	int			lev,	
	xfs_buf_t		*bp)	
{
	struct xfs_btree_block	*b;	

	if (cur->bc_bufs[lev])
		xfs_trans_brelse(cur->bc_tp, cur->bc_bufs[lev]);
	cur->bc_bufs[lev] = bp;
	cur->bc_ra[lev] = 0;

	b = XFS_BUF_TO_BLOCK(bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (b->bb_u.l.bb_leftsib == cpu_to_be64(NULLDFSBNO))
			cur->bc_ra[lev] |= XFS_BTCUR_LEFTRA;
		if (b->bb_u.l.bb_rightsib == cpu_to_be64(NULLDFSBNO))
			cur->bc_ra[lev] |= XFS_BTCUR_RIGHTRA;
	} else {
		if (b->bb_u.s.bb_leftsib == cpu_to_be32(NULLAGBLOCK))
			cur->bc_ra[lev] |= XFS_BTCUR_LEFTRA;
		if (b->bb_u.s.bb_rightsib == cpu_to_be32(NULLAGBLOCK))
			cur->bc_ra[lev] |= XFS_BTCUR_RIGHTRA;
	}
}

STATIC int
xfs_btree_ptr_is_null(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return ptr->l == cpu_to_be64(NULLDFSBNO);
	else
		return ptr->s == cpu_to_be32(NULLAGBLOCK);
}

STATIC void
xfs_btree_set_ptr_null(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(NULLDFSBNO);
	else
		ptr->s = cpu_to_be32(NULLAGBLOCK);
}

STATIC void
xfs_btree_get_sibling(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_ptr	*ptr,
	int			lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->l = block->bb_u.l.bb_rightsib;
		else
			ptr->l = block->bb_u.l.bb_leftsib;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->s = block->bb_u.s.bb_rightsib;
		else
			ptr->s = block->bb_u.s.bb_leftsib;
	}
}

STATIC void
xfs_btree_set_sibling(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_ptr	*ptr,
	int			lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.l.bb_rightsib = ptr->l;
		else
			block->bb_u.l.bb_leftsib = ptr->l;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.s.bb_rightsib = ptr->s;
		else
			block->bb_u.s.bb_leftsib = ptr->s;
	}
}

STATIC void
xfs_btree_init_block(
	struct xfs_btree_cur	*cur,
	int			level,
	int			numrecs,
	struct xfs_btree_block	*new)	
{
	new->bb_magic = cpu_to_be32(xfs_magics[cur->bc_btnum]);
	new->bb_level = cpu_to_be16(level);
	new->bb_numrecs = cpu_to_be16(numrecs);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		new->bb_u.l.bb_leftsib = cpu_to_be64(NULLDFSBNO);
		new->bb_u.l.bb_rightsib = cpu_to_be64(NULLDFSBNO);
	} else {
		new->bb_u.s.bb_leftsib = cpu_to_be32(NULLAGBLOCK);
		new->bb_u.s.bb_rightsib = cpu_to_be32(NULLAGBLOCK);
	}
}

STATIC int
xfs_btree_is_lastrec(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level)
{
	union xfs_btree_ptr	ptr;

	if (level > 0)
		return 0;
	if (!(cur->bc_flags & XFS_BTREE_LASTREC_UPDATE))
		return 0;

	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &ptr))
		return 0;
	return 1;
}

STATIC void
xfs_btree_buf_to_ptr(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(XFS_DADDR_TO_FSB(cur->bc_mp,
					XFS_BUF_ADDR(bp)));
	else {
		ptr->s = cpu_to_be32(xfs_daddr_to_agbno(cur->bc_mp,
					XFS_BUF_ADDR(bp)));
	}
}

STATIC xfs_daddr_t
xfs_btree_ptr_to_daddr(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		ASSERT(ptr->l != cpu_to_be64(NULLDFSBNO));

		return XFS_FSB_TO_DADDR(cur->bc_mp, be64_to_cpu(ptr->l));
	} else {
		ASSERT(cur->bc_private.a.agno != NULLAGNUMBER);
		ASSERT(ptr->s != cpu_to_be32(NULLAGBLOCK));

		return XFS_AGB_TO_DADDR(cur->bc_mp, cur->bc_private.a.agno,
					be32_to_cpu(ptr->s));
	}
}

STATIC void
xfs_btree_set_refs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	switch (cur->bc_btnum) {
	case XFS_BTNUM_BNO:
	case XFS_BTNUM_CNT:
		xfs_buf_set_ref(bp, XFS_ALLOC_BTREE_REF);
		break;
	case XFS_BTNUM_INO:
		xfs_buf_set_ref(bp, XFS_INO_BTREE_REF);
		break;
	case XFS_BTNUM_BMAP:
		xfs_buf_set_ref(bp, XFS_BMAP_BTREE_REF);
		break;
	default:
		ASSERT(0);
	}
}

STATIC int
xfs_btree_get_buf_block(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			flags,
	struct xfs_btree_block	**block,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;

	
	ASSERT(!(flags & XBF_TRYLOCK));

	d = xfs_btree_ptr_to_daddr(cur, ptr);
	*bpp = xfs_trans_get_buf(cur->bc_tp, mp->m_ddev_targp, d,
				 mp->m_bsize, flags);

	if (!*bpp)
		return ENOMEM;

	*block = XFS_BUF_TO_BLOCK(*bpp);
	return 0;
}

STATIC int
xfs_btree_read_buf_block(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			level,
	int			flags,
	struct xfs_btree_block	**block,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;
	int			error;

	
	ASSERT(!(flags & XBF_TRYLOCK));

	d = xfs_btree_ptr_to_daddr(cur, ptr);
	error = xfs_trans_read_buf(mp, cur->bc_tp, mp->m_ddev_targp, d,
				   mp->m_bsize, flags, bpp);
	if (error)
		return error;

	ASSERT(!xfs_buf_geterror(*bpp));

	xfs_btree_set_refs(cur, *bpp);
	*block = XFS_BUF_TO_BLOCK(*bpp);

	error = xfs_btree_check_block(cur, *block, level, *bpp);
	if (error)
		xfs_trans_brelse(cur->bc_tp, *bpp);
	return error;
}

STATIC void
xfs_btree_copy_keys(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*dst_key,
	union xfs_btree_key	*src_key,
	int			numkeys)
{
	ASSERT(numkeys >= 0);
	memcpy(dst_key, src_key, numkeys * cur->bc_ops->key_len);
}

STATIC void
xfs_btree_copy_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*dst_rec,
	union xfs_btree_rec	*src_rec,
	int			numrecs)
{
	ASSERT(numrecs >= 0);
	memcpy(dst_rec, src_rec, numrecs * cur->bc_ops->rec_len);
}

STATIC void
xfs_btree_copy_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*dst_ptr,
	union xfs_btree_ptr	*src_ptr,
	int			numptrs)
{
	ASSERT(numptrs >= 0);
	memcpy(dst_ptr, src_ptr, numptrs * xfs_btree_ptr_len(cur));
}

STATIC void
xfs_btree_shift_keys(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*key,
	int			dir,
	int			numkeys)
{
	char			*dst_key;

	ASSERT(numkeys >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_key = (char *)key + (dir * cur->bc_ops->key_len);
	memmove(dst_key, key, numkeys * cur->bc_ops->key_len);
}

STATIC void
xfs_btree_shift_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec,
	int			dir,
	int			numrecs)
{
	char			*dst_rec;

	ASSERT(numrecs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_rec = (char *)rec + (dir * cur->bc_ops->rec_len);
	memmove(dst_rec, rec, numrecs * cur->bc_ops->rec_len);
}

STATIC void
xfs_btree_shift_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			dir,
	int			numptrs)
{
	char			*dst_ptr;

	ASSERT(numptrs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_ptr = (char *)ptr + (dir * xfs_btree_ptr_len(cur));
	memmove(dst_ptr, ptr, numptrs * xfs_btree_ptr_len(cur));
}

STATIC void
xfs_btree_log_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	if (bp) {
		xfs_trans_log_buf(cur->bc_tp, bp,
				  xfs_btree_key_offset(cur, first),
				  xfs_btree_key_offset(cur, last + 1) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
				xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

void
xfs_btree_log_recs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	xfs_trans_log_buf(cur->bc_tp, bp,
			  xfs_btree_rec_offset(cur, first),
			  xfs_btree_rec_offset(cur, last + 1) - 1);

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

STATIC void
xfs_btree_log_ptrs(
	struct xfs_btree_cur	*cur,	
	struct xfs_buf		*bp,	
	int			first,	
	int			last)	
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	if (bp) {
		struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
		int			level = xfs_btree_get_level(block);

		xfs_trans_log_buf(cur->bc_tp, bp,
				xfs_btree_ptr_offset(cur, first, level),
				xfs_btree_ptr_offset(cur, last + 1, level) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
			xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

void
xfs_btree_log_block(
	struct xfs_btree_cur	*cur,	
	struct xfs_buf		*bp,	
	int			fields)	
{
	int			first;	
	int			last;	
	static const short	soffsets[] = {	
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.s.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.s.bb_rightsib),
		XFS_BTREE_SBLOCK_LEN
	};
	static const short	loffsets[] = {	
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.l.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.l.bb_rightsib),
		XFS_BTREE_LBLOCK_LEN
	};

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBI(cur, bp, fields);

	if (bp) {
		xfs_btree_offsets(fields,
				  (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
					loffsets : soffsets,
				  XFS_BB_NUM_BITS, &first, &last);
		xfs_trans_log_buf(cur->bc_tp, bp, first, last);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
			xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

int						
xfs_btree_increment(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		
{
	struct xfs_btree_block	*block;
	union xfs_btree_ptr	ptr;
	struct xfs_buf		*bp;
	int			error;		
	int			lev;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	ASSERT(level < cur->bc_nlevels);

	
	xfs_btree_readahead(cur, level, XFS_BTCUR_RIGHTRA);

	
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	
	if (++cur->bc_ptrs[level] <= xfs_btree_get_numrecs(block))
		goto out1;

	
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, increment);

	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		block = xfs_btree_get_block(cur, lev, &bp);

#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, lev, bp);
		if (error)
			goto error0;
#endif

		if (++cur->bc_ptrs[lev] <= xfs_btree_get_numrecs(block))
			break;

		
		xfs_btree_readahead(cur, lev, XFS_BTCUR_RIGHTRA);
	}

	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_ptrs[lev], block);
		error = xfs_btree_read_buf_block(cur, ptrp, --lev,
							0, &block, &bp);
		if (error)
			goto error0;

		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_ptrs[lev] = 1;
	}
out1:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

int						
xfs_btree_decrement(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		
{
	struct xfs_btree_block	*block;
	xfs_buf_t		*bp;
	int			error;		
	int			lev;
	union xfs_btree_ptr	ptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	ASSERT(level < cur->bc_nlevels);

	
	xfs_btree_readahead(cur, level, XFS_BTCUR_LEFTRA);

	
	if (--cur->bc_ptrs[level] > 0)
		goto out1;

	
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, decrement);

	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		if (--cur->bc_ptrs[lev] > 0)
			break;
		
		xfs_btree_readahead(cur, lev, XFS_BTCUR_LEFTRA);
	}

	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_ptrs[lev], block);
		error = xfs_btree_read_buf_block(cur, ptrp, --lev,
							0, &block, &bp);
		if (error)
			goto error0;
		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_ptrs[lev] = xfs_btree_get_numrecs(block);
	}
out1:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int
xfs_btree_lookup_get_block(
	struct xfs_btree_cur	*cur,	
	int			level,	
	union xfs_btree_ptr	*pp,	
	struct xfs_btree_block	**blkp) 
{
	struct xfs_buf		*bp;	
	int			error = 0;

	
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*blkp = xfs_btree_get_iroot(cur);
		return 0;
	}

	bp = cur->bc_bufs[level];
	if (bp && XFS_BUF_ADDR(bp) == xfs_btree_ptr_to_daddr(cur, pp)) {
		*blkp = XFS_BUF_TO_BLOCK(bp);
		return 0;
	}

	error = xfs_btree_read_buf_block(cur, pp, level, 0, blkp, &bp);
	if (error)
		return error;

	xfs_btree_setbuf(cur, level, bp);
	return 0;
}

STATIC union xfs_btree_key *
xfs_lookup_get_search_key(
	struct xfs_btree_cur	*cur,
	int			level,
	int			keyno,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*kp)
{
	if (level == 0) {
		cur->bc_ops->init_key_from_rec(kp,
				xfs_btree_rec_addr(cur, keyno, block));
		return kp;
	}

	return xfs_btree_key_addr(cur, keyno, block);
}

int					
xfs_btree_lookup(
	struct xfs_btree_cur	*cur,	
	xfs_lookup_t		dir,	
	int			*stat)	
{
	struct xfs_btree_block	*block;	
	__int64_t		diff;	
	int			error;	
	int			keyno;	
	int			level;	
	union xfs_btree_ptr	*pp;	
	union xfs_btree_ptr	ptr;	

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, dir);

	XFS_BTREE_STATS_INC(cur, lookup);

	block = NULL;
	keyno = 0;

	
	cur->bc_ops->init_ptr_from_cur(cur, &ptr);
	pp = &ptr;

	for (level = cur->bc_nlevels - 1, diff = 1; level >= 0; level--) {
		
		error = xfs_btree_lookup_get_block(cur, level, pp, &block);
		if (error)
			goto error0;

		if (diff == 0) {
			keyno = 1;
		} else {
			

			int	high;	
			int	low;	

			
			low = 1;
			high = xfs_btree_get_numrecs(block);
			if (!high) {
				
				ASSERT(level == 0 && cur->bc_nlevels == 1);

				cur->bc_ptrs[0] = dir != XFS_LOOKUP_LE;
				XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
				*stat = 0;
				return 0;
			}

			
			while (low <= high) {
				union xfs_btree_key	key;
				union xfs_btree_key	*kp;

				XFS_BTREE_STATS_INC(cur, compare);

				
				keyno = (low + high) >> 1;

				
				kp = xfs_lookup_get_search_key(cur, level,
						keyno, block, &key);

				diff = cur->bc_ops->key_diff(cur, kp);
				if (diff < 0)
					low = keyno + 1;
				else if (diff > 0)
					high = keyno - 1;
				else
					break;
			}
		}

		if (level > 0) {
			if (diff > 0 && --keyno < 1)
				keyno = 1;
			pp = xfs_btree_ptr_addr(cur, keyno, block);

#ifdef DEBUG
			error = xfs_btree_check_ptr(cur, pp, 0, level);
			if (error)
				goto error0;
#endif
			cur->bc_ptrs[level] = keyno;
		}
	}

	
	if (dir != XFS_LOOKUP_LE && diff < 0) {
		keyno++;
		xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
		if (dir == XFS_LOOKUP_GE &&
		    keyno > xfs_btree_get_numrecs(block) &&
		    !xfs_btree_ptr_is_null(cur, &ptr)) {
			int	i;

			cur->bc_ptrs[0] = keyno;
			error = xfs_btree_increment(cur, 0, &i);
			if (error)
				goto error0;
			XFS_WANT_CORRUPTED_RETURN(i == 1);
			XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
			*stat = 1;
			return 0;
		}
	} else if (dir == XFS_LOOKUP_LE && diff > 0)
		keyno--;
	cur->bc_ptrs[0] = keyno;

	
	if (keyno == 0 || keyno > xfs_btree_get_numrecs(block))
		*stat = 0;
	else if (dir != XFS_LOOKUP_EQ || diff == 0)
		*stat = 1;
	else
		*stat = 0;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int
xfs_btree_updkey(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*keyp,
	int			level)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	union xfs_btree_key	*kp;
	int			ptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIK(cur, level, keyp);

	ASSERT(!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) || level >= 1);

	for (ptr = 1; ptr == 1 && level < cur->bc_nlevels; level++) {
#ifdef DEBUG
		int		error;
#endif
		block = xfs_btree_get_block(cur, level, &bp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, level, bp);
		if (error) {
			XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
			return error;
		}
#endif
		ptr = cur->bc_ptrs[level];
		kp = xfs_btree_key_addr(cur, ptr, block);
		xfs_btree_copy_keys(cur, kp, keyp, 1);
		xfs_btree_log_keys(cur, bp, ptr, ptr);
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
}

int
xfs_btree_update(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	int			error;
	int			ptr;
	union xfs_btree_rec	*rp;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGR(cur, rec);

	
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		goto error0;
#endif
	
	ptr = cur->bc_ptrs[0];
	rp = xfs_btree_rec_addr(cur, ptr, block);

	
	xfs_btree_copy_recs(cur, rp, rec, 1);
	xfs_btree_log_recs(cur, bp, ptr, ptr);

	if (xfs_btree_is_lastrec(cur, block, 0)) {
		cur->bc_ops->update_lastrec(cur, block, rec,
					    ptr, LASTREC_UPDATE);
	}

	
	if (ptr == 1) {
		union xfs_btree_key	key;

		cur->bc_ops->init_key_from_rec(&key, rec);
		error = xfs_btree_updkey(cur, &key, 1);
		if (error)
			goto error0;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int					
xfs_btree_lshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		
{
	union xfs_btree_key	key;		
	struct xfs_buf		*lbp;		
	struct xfs_btree_block	*left;		
	int			lrecs;		
	struct xfs_buf		*rbp;		
	struct xfs_btree_block	*right;		
	int			rrecs;		
	union xfs_btree_ptr	lptr;		
	union xfs_btree_key	*rkp = NULL;	
	union xfs_btree_ptr	*rpp = NULL;	
	union xfs_btree_rec	*rrp = NULL;	
	int			error;		

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1)
		goto out0;

	
	right = xfs_btree_get_block(cur, level, &rbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, right, level, rbp);
	if (error)
		goto error0;
#endif

	
	xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &lptr))
		goto out0;

	if (cur->bc_ptrs[level] <= 1)
		goto out0;

	
	error = xfs_btree_read_buf_block(cur, &lptr, level, 0, &left, &lbp);
	if (error)
		goto error0;

	
	lrecs = xfs_btree_get_numrecs(left);
	if (lrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	rrecs = xfs_btree_get_numrecs(right);

	lrecs++;
	rrecs--;

	XFS_BTREE_STATS_INC(cur, lshift);
	XFS_BTREE_STATS_ADD(cur, moves, 1);

	if (level > 0) {
		
		union xfs_btree_key	*lkp;	
		union xfs_btree_ptr	*lpp;	

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);

		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rpp = xfs_btree_ptr_addr(cur, 1, right);
#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, rpp, 0, level);
		if (error)
			goto error0;
#endif
		xfs_btree_copy_keys(cur, lkp, rkp, 1);
		xfs_btree_copy_ptrs(cur, lpp, rpp, 1);

		xfs_btree_log_keys(cur, lbp, lrecs, lrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->keys_inorder(cur,
			xfs_btree_key_addr(cur, lrecs - 1, left), lkp));
	} else {
		
		union xfs_btree_rec	*lrp;	

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, 1);
		xfs_btree_log_recs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->recs_inorder(cur,
			xfs_btree_rec_addr(cur, lrecs - 1, left), lrp));
	}

	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	XFS_BTREE_STATS_ADD(cur, moves, rrecs - 1);
	if (level > 0) {
		
#ifdef DEBUG
		int			i;		

		for (i = 0; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, rpp, i + 1, level);
			if (error)
				goto error0;
		}
#endif
		xfs_btree_shift_keys(cur,
				xfs_btree_key_addr(cur, 2, right),
				-1, rrecs);
		xfs_btree_shift_ptrs(cur,
				xfs_btree_ptr_addr(cur, 2, right),
				-1, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);
	} else {
		
		xfs_btree_shift_recs(cur,
			xfs_btree_rec_addr(cur, 2, right),
			-1, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);

		cur->bc_ops->init_key_from_rec(&key,
			xfs_btree_rec_addr(cur, 1, right));
		rkp = &key;
	}

	
	error = xfs_btree_updkey(cur, rkp, level + 1);
	if (error)
		goto error0;

	
	cur->bc_ptrs[level]--;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int					
xfs_btree_rshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		
{
	union xfs_btree_key	key;		
	struct xfs_buf		*lbp;		
	struct xfs_btree_block	*left;		
	struct xfs_buf		*rbp;		
	struct xfs_btree_block	*right;		
	struct xfs_btree_cur	*tcur;		
	union xfs_btree_ptr	rptr;		
	union xfs_btree_key	*rkp;		
	int			rrecs;		
	int			lrecs;		
	int			error;		
	int			i;		

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1))
		goto out0;

	
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	
	xfs_btree_get_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &rptr))
		goto out0;

	lrecs = xfs_btree_get_numrecs(left);
	if (cur->bc_ptrs[level] >= lrecs)
		goto out0;

	
	error = xfs_btree_read_buf_block(cur, &rptr, level, 0, &right, &rbp);
	if (error)
		goto error0;

	
	rrecs = xfs_btree_get_numrecs(right);
	if (rrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, rshift);
	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	if (level > 0) {
		
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;
		union xfs_btree_ptr	*rpp;

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

#ifdef DEBUG
		for (i = rrecs - 1; i >= 0; i--) {
			error = xfs_btree_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}
#endif

		xfs_btree_shift_keys(cur, rkp, 1, rrecs);
		xfs_btree_shift_ptrs(cur, rpp, 1, rrecs);

#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, lpp, 0, level);
		if (error)
			goto error0;
#endif

		
		xfs_btree_copy_keys(cur, rkp, lkp, 1);
		xfs_btree_copy_ptrs(cur, rpp, lpp, 1);

		xfs_btree_log_keys(cur, rbp, 1, rrecs + 1);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs + 1);

		ASSERT(cur->bc_ops->keys_inorder(cur, rkp,
			xfs_btree_key_addr(cur, 2, right)));
	} else {
		
		union xfs_btree_rec	*lrp;
		union xfs_btree_rec	*rrp;

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_shift_recs(cur, rrp, 1, rrecs);

		
		xfs_btree_copy_recs(cur, rrp, lrp, 1);
		xfs_btree_log_recs(cur, rbp, 1, rrecs + 1);

		cur->bc_ops->init_key_from_rec(&key, rrp);
		rkp = &key;

		ASSERT(cur->bc_ops->recs_inorder(cur, rrp,
			xfs_btree_rec_addr(cur, 2, right)));
	}

	xfs_btree_set_numrecs(left, --lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, ++rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;
	i = xfs_btree_lastrec(tcur, level);
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

	error = xfs_btree_increment(tcur, level, &i);
	if (error)
		goto error1;

	error = xfs_btree_updkey(tcur, rkp, level + 1);
	if (error)
		goto error1;

	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;

error1:
	XFS_BTREE_TRACE_CURSOR(tcur, XBT_ERROR);
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

STATIC int					
xfs_btree_split(
	struct xfs_btree_cur	*cur,
	int			level,
	union xfs_btree_ptr	*ptrp,
	union xfs_btree_key	*key,
	struct xfs_btree_cur	**curp,
	int			*stat)		
{
	union xfs_btree_ptr	lptr;		
	struct xfs_buf		*lbp;		
	struct xfs_btree_block	*left;		
	union xfs_btree_ptr	rptr;		
	struct xfs_buf		*rbp;		
	struct xfs_btree_block	*right;		
	union xfs_btree_ptr	rrptr;		
	struct xfs_buf		*rrbp;		
	struct xfs_btree_block	*rrblock;	
	int			lrecs;
	int			rrecs;
	int			src_index;
	int			error;		
#ifdef DEBUG
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIPK(cur, level, *ptrp, key);

	XFS_BTREE_STATS_INC(cur, split);

	
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	xfs_btree_buf_to_ptr(cur, lbp, &lptr);

	
	error = cur->bc_ops->alloc_block(cur, &lptr, &rptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	
	error = xfs_btree_get_buf_block(cur, &rptr, 0, &right, &rbp);
	if (error)
		goto error0;

	
	xfs_btree_init_block(cur, xfs_btree_get_level(left), 0, right);

	lrecs = xfs_btree_get_numrecs(left);
	rrecs = lrecs / 2;
	if ((lrecs & 1) && cur->bc_ptrs[level] <= rrecs + 1)
		rrecs++;
	src_index = (lrecs - rrecs + 1);

	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	if (level > 0) {
		
		union xfs_btree_key	*lkp;	
		union xfs_btree_ptr	*lpp;	
		union xfs_btree_key	*rkp;	
		union xfs_btree_ptr	*rpp;	

		lkp = xfs_btree_key_addr(cur, src_index, left);
		lpp = xfs_btree_ptr_addr(cur, src_index, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

#ifdef DEBUG
		for (i = src_index; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}
#endif

		xfs_btree_copy_keys(cur, rkp, lkp, rrecs);
		xfs_btree_copy_ptrs(cur, rpp, lpp, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);

		
		xfs_btree_copy_keys(cur, key, rkp, 1);
	} else {
		
		union xfs_btree_rec	*lrp;	
		union xfs_btree_rec	*rrp;	

		lrp = xfs_btree_rec_addr(cur, src_index, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, rrp, lrp, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);

		cur->bc_ops->init_key_from_rec(key,
			xfs_btree_rec_addr(cur, 1, right));
	}


	xfs_btree_get_sibling(cur, left, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	xfs_btree_set_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);

	lrecs -= rrecs;
	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_set_numrecs(right, xfs_btree_get_numrecs(right) + rrecs);

	xfs_btree_log_block(cur, rbp, XFS_BB_ALL_BITS);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	if (!xfs_btree_ptr_is_null(cur, &rrptr)) {
		error = xfs_btree_read_buf_block(cur, &rrptr, level,
							0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &rptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}
	if (cur->bc_ptrs[level] > lrecs + 1) {
		xfs_btree_setbuf(cur, level, rbp);
		cur->bc_ptrs[level] -= lrecs;
	}
	if (level + 1 < cur->bc_nlevels) {
		error = xfs_btree_dup_cursor(cur, curp);
		if (error)
			goto error0;
		(*curp)->bc_ptrs[level + 1]++;
	}
	*ptrp = rptr;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

int						
xfs_btree_new_iroot(
	struct xfs_btree_cur	*cur,		
	int			*logflags,	
	int			*stat)		
{
	struct xfs_buf		*cbp;		
	struct xfs_btree_block	*block;		
	struct xfs_btree_block	*cblock;	
	union xfs_btree_key	*ckp;		
	union xfs_btree_ptr	*cpp;		
	union xfs_btree_key	*kp;		
	union xfs_btree_ptr	*pp;		
	union xfs_btree_ptr	nptr;		
	int			level;		
	int			error;		
#ifdef DEBUG
	int			i;		
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_STATS_INC(cur, newroot);

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);

	level = cur->bc_nlevels - 1;

	block = xfs_btree_get_iroot(cur);
	pp = xfs_btree_ptr_addr(cur, 1, block);

	
	error = cur->bc_ops->alloc_block(cur, pp, &nptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		return 0;
	}
	XFS_BTREE_STATS_INC(cur, alloc);

	
	error = xfs_btree_get_buf_block(cur, &nptr, 0, &cblock, &cbp);
	if (error)
		goto error0;

	memcpy(cblock, block, xfs_btree_block_len(cur));

	be16_add_cpu(&block->bb_level, 1);
	xfs_btree_set_numrecs(block, 1);
	cur->bc_nlevels++;
	cur->bc_ptrs[level + 1] = 1;

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, ckp, kp, xfs_btree_get_numrecs(cblock));

	cpp = xfs_btree_ptr_addr(cur, 1, cblock);
#ifdef DEBUG
	for (i = 0; i < be16_to_cpu(cblock->bb_numrecs); i++) {
		error = xfs_btree_check_ptr(cur, pp, i, level);
		if (error)
			goto error0;
	}
#endif
	xfs_btree_copy_ptrs(cur, cpp, pp, xfs_btree_get_numrecs(cblock));

#ifdef DEBUG
	error = xfs_btree_check_ptr(cur, &nptr, 0, level);
	if (error)
		goto error0;
#endif
	xfs_btree_copy_ptrs(cur, pp, &nptr, 1);

	xfs_iroot_realloc(cur->bc_private.b.ip,
			  1 - xfs_btree_get_numrecs(cblock),
			  cur->bc_private.b.whichfork);

	xfs_btree_setbuf(cur, level, cbp);

	xfs_btree_log_block(cur, cbp, XFS_BB_ALL_BITS);
	xfs_btree_log_keys(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));
	xfs_btree_log_ptrs(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));

	*logflags |=
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_private.b.whichfork);
	*stat = 1;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int				
xfs_btree_new_root(
	struct xfs_btree_cur	*cur,	
	int			*stat)	
{
	struct xfs_btree_block	*block;	
	struct xfs_buf		*bp;	
	int			error;	
	struct xfs_buf		*lbp;	
	struct xfs_btree_block	*left;	
	struct xfs_buf		*nbp;	
	struct xfs_btree_block	*new;	
	int			nptr;	
	struct xfs_buf		*rbp;	
	struct xfs_btree_block	*right;	
	union xfs_btree_ptr	rptr;
	union xfs_btree_ptr	lptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_STATS_INC(cur, newroot);

	
	cur->bc_ops->init_ptr_from_cur(cur, &rptr);

	
	error = cur->bc_ops->alloc_block(cur, &rptr, &lptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	
	error = xfs_btree_get_buf_block(cur, &lptr, 0, &new, &nbp);
	if (error)
		goto error0;

	
	cur->bc_ops->set_root(cur, &lptr, 1);

	block = xfs_btree_get_block(cur, cur->bc_nlevels - 1, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, cur->bc_nlevels - 1, bp);
	if (error)
		goto error0;
#endif

	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		
		lbp = bp;
		xfs_btree_buf_to_ptr(cur, lbp, &lptr);
		left = block;
		error = xfs_btree_read_buf_block(cur, &rptr,
					cur->bc_nlevels - 1, 0, &right, &rbp);
		if (error)
			goto error0;
		bp = rbp;
		nptr = 1;
	} else {
		
		rbp = bp;
		xfs_btree_buf_to_ptr(cur, rbp, &rptr);
		right = block;
		xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
		error = xfs_btree_read_buf_block(cur, &lptr,
					cur->bc_nlevels - 1, 0, &left, &lbp);
		if (error)
			goto error0;
		bp = lbp;
		nptr = 2;
	}
	
	xfs_btree_init_block(cur, cur->bc_nlevels, 2, new);
	xfs_btree_log_block(cur, nbp, XFS_BB_ALL_BITS);
	ASSERT(!xfs_btree_ptr_is_null(cur, &lptr) &&
			!xfs_btree_ptr_is_null(cur, &rptr));

	
	if (xfs_btree_get_level(left) > 0) {
		xfs_btree_copy_keys(cur,
				xfs_btree_key_addr(cur, 1, new),
				xfs_btree_key_addr(cur, 1, left), 1);
		xfs_btree_copy_keys(cur,
				xfs_btree_key_addr(cur, 2, new),
				xfs_btree_key_addr(cur, 1, right), 1);
	} else {
		cur->bc_ops->init_key_from_rec(
				xfs_btree_key_addr(cur, 1, new),
				xfs_btree_rec_addr(cur, 1, left));
		cur->bc_ops->init_key_from_rec(
				xfs_btree_key_addr(cur, 2, new),
				xfs_btree_rec_addr(cur, 1, right));
	}
	xfs_btree_log_keys(cur, nbp, 1, 2);

	
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 1, new), &lptr, 1);
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 2, new), &rptr, 1);
	xfs_btree_log_ptrs(cur, nbp, 1, 2);

	
	xfs_btree_setbuf(cur, cur->bc_nlevels, nbp);
	cur->bc_ptrs[cur->bc_nlevels] = nptr;
	cur->bc_nlevels++;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;
}

STATIC int
xfs_btree_make_block_unfull(
	struct xfs_btree_cur	*cur,	
	int			level,	
	int			numrecs,
	int			*oindex,
	int			*index,	
	union xfs_btree_ptr	*nptr,	
	struct xfs_btree_cur	**ncur,	
	union xfs_btree_rec	*nrec,	
	int			*stat)
{
	union xfs_btree_key	key;	
	int			error = 0;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1) {
	    	struct xfs_inode *ip = cur->bc_private.b.ip;

		if (numrecs < cur->bc_ops->get_dmaxrecs(cur, level)) {
			

			xfs_iroot_realloc(ip, 1, cur->bc_private.b.whichfork);
		} else {
			
			int	logflags = 0;

			error = xfs_btree_new_iroot(cur, &logflags, stat);
			if (error || *stat == 0)
				return error;

			xfs_trans_log_inode(cur->bc_tp, ip, logflags);
		}

		return 0;
	}

	
	error = xfs_btree_rshift(cur, level, stat);
	if (error || *stat)
		return error;

	
	error = xfs_btree_lshift(cur, level, stat);
	if (error)
		return error;

	if (*stat) {
		*oindex = *index = cur->bc_ptrs[level];
		return 0;
	}

	error = xfs_btree_split(cur, level, nptr, &key, ncur, stat);
	if (error || *stat == 0)
		return error;


	*index = cur->bc_ptrs[level];
	cur->bc_ops->init_rec_from_key(&key, nrec);
	return 0;
}

STATIC int
xfs_btree_insrec(
	struct xfs_btree_cur	*cur,	
	int			level,	
	union xfs_btree_ptr	*ptrp,	
	union xfs_btree_rec	*recp,	
	struct xfs_btree_cur	**curp,	
	int			*stat)	
{
	struct xfs_btree_block	*block;	
	struct xfs_buf		*bp;	
	union xfs_btree_key	key;	
	union xfs_btree_ptr	nptr;	
	struct xfs_btree_cur	*ncur;	
	union xfs_btree_rec	nrec;	
	int			optr;	
	int			ptr;	
	int			numrecs;
	int			error;	
#ifdef DEBUG
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIPR(cur, level, *ptrp, recp);

	ncur = NULL;

	if (!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level >= cur->bc_nlevels)) {
		error = xfs_btree_new_root(cur, stat);
		xfs_btree_set_ptr_null(cur, ptrp);

		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		return error;
	}

	
	ptr = cur->bc_ptrs[level];
	if (ptr == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	
	cur->bc_ops->init_key_from_rec(&key, recp);

	optr = ptr;

	XFS_BTREE_STATS_INC(cur, insrec);

	
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;

	
	if (ptr <= numrecs) {
		if (level == 0) {
			ASSERT(cur->bc_ops->recs_inorder(cur, recp,
				xfs_btree_rec_addr(cur, ptr, block)));
		} else {
			ASSERT(cur->bc_ops->keys_inorder(cur, &key,
				xfs_btree_key_addr(cur, ptr, block)));
		}
	}
#endif

	xfs_btree_set_ptr_null(cur, &nptr);
	if (numrecs == cur->bc_ops->get_maxrecs(cur, level)) {
		error = xfs_btree_make_block_unfull(cur, level, numrecs,
					&optr, &ptr, &nptr, &ncur, &nrec, stat);
		if (error || *stat == 0)
			goto error0;
	}

	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		return error;
#endif

	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr + 1);

	if (level > 0) {
		
		union xfs_btree_key	*kp;
		union xfs_btree_ptr	*pp;

		kp = xfs_btree_key_addr(cur, ptr, block);
		pp = xfs_btree_ptr_addr(cur, ptr, block);

#ifdef DEBUG
		for (i = numrecs - ptr; i >= 0; i--) {
			error = xfs_btree_check_ptr(cur, pp, i, level);
			if (error)
				return error;
		}
#endif

		xfs_btree_shift_keys(cur, kp, 1, numrecs - ptr + 1);
		xfs_btree_shift_ptrs(cur, pp, 1, numrecs - ptr + 1);

#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, ptrp, 0, level);
		if (error)
			goto error0;
#endif

		
		xfs_btree_copy_keys(cur, kp, &key, 1);
		xfs_btree_copy_ptrs(cur, pp, ptrp, 1);
		numrecs++;
		xfs_btree_set_numrecs(block, numrecs);
		xfs_btree_log_ptrs(cur, bp, ptr, numrecs);
		xfs_btree_log_keys(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->keys_inorder(cur, kp,
				xfs_btree_key_addr(cur, ptr + 1, block)));
		}
#endif
	} else {
		
		union xfs_btree_rec             *rp;

		rp = xfs_btree_rec_addr(cur, ptr, block);

		xfs_btree_shift_recs(cur, rp, 1, numrecs - ptr + 1);

		
		xfs_btree_copy_recs(cur, rp, recp, 1);
		xfs_btree_set_numrecs(block, ++numrecs);
		xfs_btree_log_recs(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->recs_inorder(cur, rp,
				xfs_btree_rec_addr(cur, ptr + 1, block)));
		}
#endif
	}

	
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	
	if (optr == 1) {
		error = xfs_btree_updkey(cur, &key, level + 1);
		if (error)
			goto error0;
	}

	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, recp,
					    ptr, LASTREC_INSREC);
	}

	*ptrp = nptr;
	if (!xfs_btree_ptr_is_null(cur, &nptr)) {
		*recp = nrec;
		*curp = ncur;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

int
xfs_btree_insert(
	struct xfs_btree_cur	*cur,
	int			*stat)
{
	int			error;	
	int			i;	
	int			level;	
	union xfs_btree_ptr	nptr;	
	struct xfs_btree_cur	*ncur;	
	struct xfs_btree_cur	*pcur;	
	union xfs_btree_rec	rec;	

	level = 0;
	ncur = NULL;
	pcur = cur;

	xfs_btree_set_ptr_null(cur, &nptr);
	cur->bc_ops->init_rec_from_cur(cur, &rec);

	do {
		error = xfs_btree_insrec(pcur, level, &nptr, &rec, &ncur, &i);
		if (error) {
			if (pcur != cur)
				xfs_btree_del_cursor(pcur, XFS_BTREE_ERROR);
			goto error0;
		}

		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		level++;

		if (pcur != cur &&
		    (ncur || xfs_btree_ptr_is_null(cur, &nptr))) {
			
			if (cur->bc_ops->update_cursor)
				cur->bc_ops->update_cursor(pcur, cur);
			cur->bc_nlevels = pcur->bc_nlevels;
			xfs_btree_del_cursor(pcur, XFS_BTREE_NOERROR);
		}
		
		if (ncur) {
			pcur = ncur;
			ncur = NULL;
		}
	} while (!xfs_btree_ptr_is_null(cur, &nptr));

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = i;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int
xfs_btree_kill_iroot(
	struct xfs_btree_cur	*cur)
{
	int			whichfork = cur->bc_private.b.whichfork;
	struct xfs_inode	*ip = cur->bc_private.b.ip;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(ip, whichfork);
	struct xfs_btree_block	*block;
	struct xfs_btree_block	*cblock;
	union xfs_btree_key	*kp;
	union xfs_btree_key	*ckp;
	union xfs_btree_ptr	*pp;
	union xfs_btree_ptr	*cpp;
	struct xfs_buf		*cbp;
	int			level;
	int			index;
	int			numrecs;
#ifdef DEBUG
	union xfs_btree_ptr	ptr;
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);
	ASSERT(cur->bc_nlevels > 1);

	level = cur->bc_nlevels - 1;
	if (level == 1)
		goto out0;

	block = xfs_btree_get_iroot(cur);
	if (xfs_btree_get_numrecs(block) != 1)
		goto out0;

	cblock = xfs_btree_get_block(cur, level - 1, &cbp);
	numrecs = xfs_btree_get_numrecs(cblock);

	if (numrecs > cur->bc_ops->get_dmaxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, killroot);

#ifdef DEBUG
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
#endif

	index = numrecs - cur->bc_ops->get_maxrecs(cur, level);
	if (index) {
		xfs_iroot_realloc(cur->bc_private.b.ip, index,
				  cur->bc_private.b.whichfork);
		block = ifp->if_broot;
	}

	be16_add_cpu(&block->bb_numrecs, index);
	ASSERT(block->bb_numrecs == cblock->bb_numrecs);

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, kp, ckp, numrecs);

	pp = xfs_btree_ptr_addr(cur, 1, block);
	cpp = xfs_btree_ptr_addr(cur, 1, cblock);
#ifdef DEBUG
	for (i = 0; i < numrecs; i++) {
		int		error;

		error = xfs_btree_check_ptr(cur, cpp, i, level - 1);
		if (error) {
			XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
			return error;
		}
	}
#endif
	xfs_btree_copy_ptrs(cur, pp, cpp, numrecs);

	cur->bc_ops->free_block(cur, cbp);
	XFS_BTREE_STATS_INC(cur, free);

	cur->bc_bufs[level - 1] = NULL;
	be16_add_cpu(&block->bb_level, -1);
	xfs_trans_log_inode(cur->bc_tp, ip,
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	cur->bc_nlevels--;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
}

STATIC int
xfs_btree_kill_root(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			level,
	union xfs_btree_ptr	*newroot)
{
	int			error;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_STATS_INC(cur, killroot);

	cur->bc_ops->set_root(cur, newroot, -1);

	error = cur->bc_ops->free_block(cur, bp);
	if (error) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
		return error;
	}

	XFS_BTREE_STATS_INC(cur, free);

	cur->bc_bufs[level] = NULL;
	cur->bc_ra[level] = 0;
	cur->bc_nlevels--;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
}

STATIC int
xfs_btree_dec_cursor(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)
{
	int			error;
	int			i;

	if (level > 0) {
		error = xfs_btree_decrement(cur, level, &i);
		if (error)
			return error;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
}

STATIC int					
xfs_btree_delrec(
	struct xfs_btree_cur	*cur,		
	int			level,		
	int			*stat)		
{
	struct xfs_btree_block	*block;		
	union xfs_btree_ptr	cptr;		
	struct xfs_buf		*bp;		
	int			error;		
	int			i;		
	union xfs_btree_key	key;		
	union xfs_btree_key	*keyp = &key;	
	union xfs_btree_ptr	lptr;		
	struct xfs_buf		*lbp;		
	struct xfs_btree_block	*left;		
	int			lrecs = 0;	
	int			ptr;		
	union xfs_btree_ptr	rptr;		
	struct xfs_buf		*rbp;		
	struct xfs_btree_block	*right;		
	struct xfs_btree_block	*rrblock;	
	struct xfs_buf		*rrbp;		
	int			rrecs = 0;	
	struct xfs_btree_cur	*tcur;		
	int			numrecs;	

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	tcur = NULL;

	
	ptr = cur->bc_ptrs[level];
	if (ptr == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	
	if (ptr > numrecs) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	XFS_BTREE_STATS_INC(cur, delrec);
	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr);

	
	if (level > 0) {
		
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;

		lkp = xfs_btree_key_addr(cur, ptr + 1, block);
		lpp = xfs_btree_ptr_addr(cur, ptr + 1, block);

#ifdef DEBUG
		for (i = 0; i < numrecs - ptr; i++) {
			error = xfs_btree_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}
#endif

		if (ptr < numrecs) {
			xfs_btree_shift_keys(cur, lkp, -1, numrecs - ptr);
			xfs_btree_shift_ptrs(cur, lpp, -1, numrecs - ptr);
			xfs_btree_log_keys(cur, bp, ptr, numrecs - 1);
			xfs_btree_log_ptrs(cur, bp, ptr, numrecs - 1);
		}

		if (ptr == 1)
			keyp = xfs_btree_key_addr(cur, 1, block);
	} else {
		
		if (ptr < numrecs) {
			xfs_btree_shift_recs(cur,
				xfs_btree_rec_addr(cur, ptr + 1, block),
				-1, numrecs - ptr);
			xfs_btree_log_recs(cur, bp, ptr, numrecs - 1);
		}

		if (ptr == 1) {
			cur->bc_ops->init_key_from_rec(&key,
					xfs_btree_rec_addr(cur, 1, block));
			keyp = &key;
		}
	}

	xfs_btree_set_numrecs(block, --numrecs);
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, NULL,
					    ptr, LASTREC_DELREC);
	}

	if (level == cur->bc_nlevels - 1) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
			xfs_iroot_realloc(cur->bc_private.b.ip, -1,
					  cur->bc_private.b.whichfork);

			error = xfs_btree_kill_iroot(cur);
			if (error)
				goto error0;

			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			*stat = 1;
			return 0;
		}

		if (numrecs == 1 && level > 0) {
			union xfs_btree_ptr	*pp;
			pp = xfs_btree_ptr_addr(cur, 1, block);
			error = xfs_btree_kill_root(cur, bp, level, pp);
			if (error)
				goto error0;
		} else if (level > 0) {
			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
		}
		*stat = 1;
		return 0;
	}

	if (ptr == 1) {
		error = xfs_btree_updkey(cur, keyp, level + 1);
		if (error)
			goto error0;
	}

	if (numrecs >= cur->bc_ops->get_minrecs(cur, level)) {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	xfs_btree_get_sibling(cur, block, &lptr, XFS_BB_LEFTSIB);

	if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
		if (xfs_btree_ptr_is_null(cur, &rptr) &&
		    xfs_btree_ptr_is_null(cur, &lptr) &&
		    level == cur->bc_nlevels - 2) {
			error = xfs_btree_kill_iroot(cur);
			if (!error)
				error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			return 0;
		}
	}

	ASSERT(!xfs_btree_ptr_is_null(cur, &rptr) ||
	       !xfs_btree_ptr_is_null(cur, &lptr));

	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;

	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		i = xfs_btree_lastrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		error = xfs_btree_increment(tcur, level, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		i = xfs_btree_lastrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		
		right = xfs_btree_get_block(tcur, level, &rbp);
#ifdef DEBUG
		error = xfs_btree_check_block(tcur, right, level, rbp);
		if (error)
			goto error0;
#endif
		
		xfs_btree_get_sibling(tcur, right, &cptr, XFS_BB_LEFTSIB);

		if (xfs_btree_get_numrecs(right) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_lshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));

				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;

				error = xfs_btree_dec_cursor(cur, level, stat);
				if (error)
					goto error0;
				return 0;
			}
		}

		rrecs = xfs_btree_get_numrecs(right);
		if (!xfs_btree_ptr_is_null(cur, &lptr)) {
			i = xfs_btree_firstrec(tcur, level);
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

			error = xfs_btree_decrement(tcur, level, &i);
			if (error)
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		}
	}

	if (!xfs_btree_ptr_is_null(cur, &lptr)) {
		i = xfs_btree_firstrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		error = xfs_btree_decrement(tcur, level, &i);
		if (error)
			goto error0;
		i = xfs_btree_firstrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		
		left = xfs_btree_get_block(tcur, level, &lbp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, left, level, lbp);
		if (error)
			goto error0;
#endif
		
		xfs_btree_get_sibling(tcur, left, &cptr, XFS_BB_RIGHTSIB);

		if (xfs_btree_get_numrecs(left) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_rshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;
				if (level == 0)
					cur->bc_ptrs[0]++;
				XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
				*stat = 1;
				return 0;
			}
		}

		lrecs = xfs_btree_get_numrecs(left);
	}

	
	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
	tcur = NULL;

	
	ASSERT(!xfs_btree_ptr_is_null(cur, &cptr));

	if (!xfs_btree_ptr_is_null(cur, &lptr) &&
	    lrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		rptr = cptr;
		right = block;
		rbp = bp;
		error = xfs_btree_read_buf_block(cur, &lptr, level,
							0, &left, &lbp);
		if (error)
			goto error0;

	} else if (!xfs_btree_ptr_is_null(cur, &rptr) &&
		   rrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		lptr = cptr;
		left = block;
		lbp = bp;
		error = xfs_btree_read_buf_block(cur, &rptr, level,
							0, &right, &rbp);
		if (error)
			goto error0;

	} else {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	rrecs = xfs_btree_get_numrecs(right);
	lrecs = xfs_btree_get_numrecs(left);

	XFS_BTREE_STATS_ADD(cur, moves, rrecs);
	if (level > 0) {
		
		union xfs_btree_key	*lkp;	
		union xfs_btree_ptr	*lpp;	
		union xfs_btree_key	*rkp;	
		union xfs_btree_ptr	*rpp;	

		lkp = xfs_btree_key_addr(cur, lrecs + 1, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs + 1, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);
#ifdef DEBUG
		for (i = 1; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}
#endif
		xfs_btree_copy_keys(cur, lkp, rkp, rrecs);
		xfs_btree_copy_ptrs(cur, lpp, rpp, rrecs);

		xfs_btree_log_keys(cur, lbp, lrecs + 1, lrecs + rrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs + 1, lrecs + rrecs);
	} else {
		
		union xfs_btree_rec	*lrp;	
		union xfs_btree_rec	*rrp;	

		lrp = xfs_btree_rec_addr(cur, lrecs + 1, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, rrecs);
		xfs_btree_log_recs(cur, lbp, lrecs + 1, lrecs + rrecs);
	}

	XFS_BTREE_STATS_INC(cur, join);

	xfs_btree_set_numrecs(left, lrecs + rrecs);
	xfs_btree_get_sibling(cur, right, &cptr, XFS_BB_RIGHTSIB),
	xfs_btree_set_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	
	xfs_btree_get_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &cptr)) {
		error = xfs_btree_read_buf_block(cur, &cptr, level,
							0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &lptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}

	
	error = cur->bc_ops->free_block(cur, rbp);
	if (error)
		goto error0;
	XFS_BTREE_STATS_INC(cur, free);

	if (bp != lbp) {
		cur->bc_bufs[level] = lbp;
		cur->bc_ptrs[level] += lrecs;
		cur->bc_ra[level] = 0;
	}
	else if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) ||
		   (level + 1 < cur->bc_nlevels)) {
		error = xfs_btree_increment(cur, level + 1, &i);
		if (error)
			goto error0;
	}

	if (level > 0)
		cur->bc_ptrs[level]--;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	
	*stat = 2;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	if (tcur)
		xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

int					
xfs_btree_delete(
	struct xfs_btree_cur	*cur,
	int			*stat)	
{
	int			error;	
	int			level;
	int			i;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);

	for (level = 0, i = 2; i == 2; level++) {
		error = xfs_btree_delrec(cur, level, &i);
		if (error)
			goto error0;
	}

	if (i == 0) {
		for (level = 1; level < cur->bc_nlevels; level++) {
			if (cur->bc_ptrs[level] == 0) {
				error = xfs_btree_decrement(cur, level, &i);
				if (error)
					goto error0;
				break;
			}
		}
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = i;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

int					
xfs_btree_get_rec(
	struct xfs_btree_cur	*cur,	
	union xfs_btree_rec	**recp,	
	int			*stat)	
{
	struct xfs_btree_block	*block;	
	struct xfs_buf		*bp;	
	int			ptr;	
#ifdef DEBUG
	int			error;	
#endif

	ptr = cur->bc_ptrs[0];
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		return error;
#endif

	if (ptr > xfs_btree_get_numrecs(block) || ptr <= 0) {
		*stat = 0;
		return 0;
	}

	*recp = xfs_btree_rec_addr(cur, ptr, block);
	*stat = 1;
	return 0;
}
