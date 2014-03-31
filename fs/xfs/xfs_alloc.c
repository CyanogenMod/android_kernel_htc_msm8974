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
#include "xfs_btree.h"
#include "xfs_alloc.h"
#include "xfs_error.h"
#include "xfs_trace.h"

struct workqueue_struct *xfs_alloc_wq;

#define XFS_ABSDIFF(a,b)	(((a) <= (b)) ? ((b) - (a)) : ((a) - (b)))

#define	XFSA_FIXUP_BNO_OK	1
#define	XFSA_FIXUP_CNT_OK	2

STATIC int xfs_alloc_ag_vextent_exact(xfs_alloc_arg_t *);
STATIC int xfs_alloc_ag_vextent_near(xfs_alloc_arg_t *);
STATIC int xfs_alloc_ag_vextent_size(xfs_alloc_arg_t *);
STATIC int xfs_alloc_ag_vextent_small(xfs_alloc_arg_t *,
		xfs_btree_cur_t *, xfs_agblock_t *, xfs_extlen_t *, int *);
STATIC void xfs_alloc_busy_trim(struct xfs_alloc_arg *,
		xfs_agblock_t, xfs_extlen_t, xfs_agblock_t *, xfs_extlen_t *);

STATIC int				
xfs_alloc_lookup_eq(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len,	
	int			*stat)	
{
	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

int				
xfs_alloc_lookup_ge(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len,	
	int			*stat)	
{
	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
}

int					
xfs_alloc_lookup_le(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len,	
	int			*stat)	
{
	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_LE, stat);
}

STATIC int				
xfs_alloc_update(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len)	
{
	union xfs_btree_rec	rec;

	rec.alloc.ar_startblock = cpu_to_be32(bno);
	rec.alloc.ar_blockcount = cpu_to_be32(len);
	return xfs_btree_update(cur, &rec);
}

int					
xfs_alloc_get_rec(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		*bno,	
	xfs_extlen_t		*len,	
	int			*stat)	
{
	union xfs_btree_rec	*rec;
	int			error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (!error && *stat == 1) {
		*bno = be32_to_cpu(rec->alloc.ar_startblock);
		*len = be32_to_cpu(rec->alloc.ar_blockcount);
	}
	return error;
}

STATIC void
xfs_alloc_compute_aligned(
	xfs_alloc_arg_t	*args,		
	xfs_agblock_t	foundbno,	
	xfs_extlen_t	foundlen,	
	xfs_agblock_t	*resbno,	
	xfs_extlen_t	*reslen)	
{
	xfs_agblock_t	bno;
	xfs_extlen_t	len;

	
	xfs_alloc_busy_trim(args, foundbno, foundlen, &bno, &len);

	if (args->alignment > 1 && len >= args->minlen) {
		xfs_agblock_t	aligned_bno = roundup(bno, args->alignment);
		xfs_extlen_t	diff = aligned_bno - bno;

		*resbno = aligned_bno;
		*reslen = diff >= len ? 0 : len - diff;
	} else {
		*resbno = bno;
		*reslen = len;
	}
}

STATIC xfs_extlen_t			
xfs_alloc_compute_diff(
	xfs_agblock_t	wantbno,	
	xfs_extlen_t	wantlen,	
	xfs_extlen_t	alignment,	
	xfs_agblock_t	freebno,	
	xfs_extlen_t	freelen,	
	xfs_agblock_t	*newbnop)	
{
	xfs_agblock_t	freeend;	
	xfs_agblock_t	newbno1;	
	xfs_agblock_t	newbno2;	
	xfs_extlen_t	newlen1=0;	
	xfs_extlen_t	newlen2=0;	
	xfs_agblock_t	wantend;	

	ASSERT(freelen >= wantlen);
	freeend = freebno + freelen;
	wantend = wantbno + wantlen;
	if (freebno >= wantbno) {
		if ((newbno1 = roundup(freebno, alignment)) >= freeend)
			newbno1 = NULLAGBLOCK;
	} else if (freeend >= wantend && alignment > 1) {
		newbno1 = roundup(wantbno, alignment);
		newbno2 = newbno1 - alignment;
		if (newbno1 >= freeend)
			newbno1 = NULLAGBLOCK;
		else
			newlen1 = XFS_EXTLEN_MIN(wantlen, freeend - newbno1);
		if (newbno2 < freebno)
			newbno2 = NULLAGBLOCK;
		else
			newlen2 = XFS_EXTLEN_MIN(wantlen, freeend - newbno2);
		if (newbno1 != NULLAGBLOCK && newbno2 != NULLAGBLOCK) {
			if (newlen1 < newlen2 ||
			    (newlen1 == newlen2 &&
			     XFS_ABSDIFF(newbno1, wantbno) >
			     XFS_ABSDIFF(newbno2, wantbno)))
				newbno1 = newbno2;
		} else if (newbno2 != NULLAGBLOCK)
			newbno1 = newbno2;
	} else if (freeend >= wantend) {
		newbno1 = wantbno;
	} else if (alignment > 1) {
		newbno1 = roundup(freeend - wantlen, alignment);
		if (newbno1 > freeend - wantlen &&
		    newbno1 - alignment >= freebno)
			newbno1 -= alignment;
		else if (newbno1 >= freeend)
			newbno1 = NULLAGBLOCK;
	} else
		newbno1 = freeend - wantlen;
	*newbnop = newbno1;
	return newbno1 == NULLAGBLOCK ? 0 : XFS_ABSDIFF(newbno1, wantbno);
}

STATIC void
xfs_alloc_fix_len(
	xfs_alloc_arg_t	*args)		
{
	xfs_extlen_t	k;
	xfs_extlen_t	rlen;

	ASSERT(args->mod < args->prod);
	rlen = args->len;
	ASSERT(rlen >= args->minlen);
	ASSERT(rlen <= args->maxlen);
	if (args->prod <= 1 || rlen < args->mod || rlen == args->maxlen ||
	    (args->mod == 0 && rlen < args->prod))
		return;
	k = rlen % args->prod;
	if (k == args->mod)
		return;
	if (k > args->mod) {
		if ((int)(rlen = rlen - k - args->mod) < (int)args->minlen)
			return;
	} else {
		if ((int)(rlen = rlen - args->prod - (args->mod - k)) <
		    (int)args->minlen)
			return;
	}
	ASSERT(rlen >= args->minlen);
	ASSERT(rlen <= args->maxlen);
	args->len = rlen;
}

STATIC int
xfs_alloc_fix_minleft(
	xfs_alloc_arg_t	*args)		
{
	xfs_agf_t	*agf;		
	int		diff;		

	if (args->minleft == 0)
		return 1;
	agf = XFS_BUF_TO_AGF(args->agbp);
	diff = be32_to_cpu(agf->agf_freeblks)
		- args->len - args->minleft;
	if (diff >= 0)
		return 1;
	args->len += diff;		
	if (args->len >= args->minlen)
		return 1;
	args->agbno = NULLAGBLOCK;
	return 0;
}

STATIC int				
xfs_alloc_fixup_trees(
	xfs_btree_cur_t	*cnt_cur,	
	xfs_btree_cur_t	*bno_cur,	
	xfs_agblock_t	fbno,		
	xfs_extlen_t	flen,		
	xfs_agblock_t	rbno,		
	xfs_extlen_t	rlen,		
	int		flags)		
{
	int		error;		
	int		i;		
	xfs_agblock_t	nfbno1;		
	xfs_agblock_t	nfbno2;		
	xfs_extlen_t	nflen1=0;	
	xfs_extlen_t	nflen2=0;	

	if (flags & XFSA_FIXUP_CNT_OK) {
#ifdef DEBUG
		if ((error = xfs_alloc_get_rec(cnt_cur, &nfbno1, &nflen1, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(
			i == 1 && nfbno1 == fbno && nflen1 == flen);
#endif
	} else {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, fbno, flen, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}
	if (flags & XFSA_FIXUP_BNO_OK) {
#ifdef DEBUG
		if ((error = xfs_alloc_get_rec(bno_cur, &nfbno1, &nflen1, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(
			i == 1 && nfbno1 == fbno && nflen1 == flen);
#endif
	} else {
		if ((error = xfs_alloc_lookup_eq(bno_cur, fbno, flen, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}

#ifdef DEBUG
	if (bno_cur->bc_nlevels == 1 && cnt_cur->bc_nlevels == 1) {
		struct xfs_btree_block	*bnoblock;
		struct xfs_btree_block	*cntblock;

		bnoblock = XFS_BUF_TO_BLOCK(bno_cur->bc_bufs[0]);
		cntblock = XFS_BUF_TO_BLOCK(cnt_cur->bc_bufs[0]);

		XFS_WANT_CORRUPTED_RETURN(
			bnoblock->bb_numrecs == cntblock->bb_numrecs);
	}
#endif

	if (rbno == fbno && rlen == flen)
		nfbno1 = nfbno2 = NULLAGBLOCK;
	else if (rbno == fbno) {
		nfbno1 = rbno + rlen;
		nflen1 = flen - rlen;
		nfbno2 = NULLAGBLOCK;
	} else if (rbno + rlen == fbno + flen) {
		nfbno1 = fbno;
		nflen1 = flen - rlen;
		nfbno2 = NULLAGBLOCK;
	} else {
		nfbno1 = fbno;
		nflen1 = rbno - fbno;
		nfbno2 = rbno + rlen;
		nflen2 = (fbno + flen) - nfbno2;
	}
	if ((error = xfs_btree_delete(cnt_cur, &i)))
		return error;
	XFS_WANT_CORRUPTED_RETURN(i == 1);
	if (nfbno1 != NULLAGBLOCK) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, nfbno1, nflen1, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 0);
		if ((error = xfs_btree_insert(cnt_cur, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}
	if (nfbno2 != NULLAGBLOCK) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, nfbno2, nflen2, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 0);
		if ((error = xfs_btree_insert(cnt_cur, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}
	if (nfbno1 == NULLAGBLOCK) {
		if ((error = xfs_btree_delete(bno_cur, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	} else {
		if ((error = xfs_alloc_update(bno_cur, nfbno1, nflen1)))
			return error;
	}
	if (nfbno2 != NULLAGBLOCK) {
		if ((error = xfs_alloc_lookup_eq(bno_cur, nfbno2, nflen2, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 0);
		if ((error = xfs_btree_insert(bno_cur, &i)))
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}
	return 0;
}

STATIC int				
xfs_alloc_read_agfl(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_agnumber_t	agno,		
	xfs_buf_t	**bpp)		
{
	xfs_buf_t	*bp;		
	int		error;

	ASSERT(agno != NULLAGNUMBER);
	error = xfs_trans_read_buf(
			mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, agno, XFS_AGFL_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, &bp);
	if (error)
		return error;
	ASSERT(!xfs_buf_geterror(bp));
	xfs_buf_set_ref(bp, XFS_AGFL_REF);
	*bpp = bp;
	return 0;
}

STATIC int
xfs_alloc_update_counters(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_buf		*agbp,
	long			len)
{
	struct xfs_agf		*agf = XFS_BUF_TO_AGF(agbp);

	pag->pagf_freeblks += len;
	be32_add_cpu(&agf->agf_freeblks, len);

	xfs_trans_agblocks_delta(tp, len);
	if (unlikely(be32_to_cpu(agf->agf_freeblks) >
		     be32_to_cpu(agf->agf_length)))
		return EFSCORRUPTED;

	xfs_alloc_log_agf(tp, agbp, XFS_AGF_FREEBLKS);
	return 0;
}


STATIC int			
xfs_alloc_ag_vextent(
	xfs_alloc_arg_t	*args)	
{
	int		error=0;

	ASSERT(args->minlen > 0);
	ASSERT(args->maxlen > 0);
	ASSERT(args->minlen <= args->maxlen);
	ASSERT(args->mod < args->prod);
	ASSERT(args->alignment > 0);
	args->wasfromfl = 0;
	switch (args->type) {
	case XFS_ALLOCTYPE_THIS_AG:
		error = xfs_alloc_ag_vextent_size(args);
		break;
	case XFS_ALLOCTYPE_NEAR_BNO:
		error = xfs_alloc_ag_vextent_near(args);
		break;
	case XFS_ALLOCTYPE_THIS_BNO:
		error = xfs_alloc_ag_vextent_exact(args);
		break;
	default:
		ASSERT(0);
		
	}

	if (error || args->agbno == NULLAGBLOCK)
		return error;

	ASSERT(args->len >= args->minlen);
	ASSERT(args->len <= args->maxlen);
	ASSERT(!args->wasfromfl || !args->isfl);
	ASSERT(args->agbno % args->alignment == 0);

	if (!args->wasfromfl) {
		error = xfs_alloc_update_counters(args->tp, args->pag,
						  args->agbp,
						  -((long)(args->len)));
		if (error)
			return error;

		ASSERT(!xfs_alloc_busy_search(args->mp, args->agno,
					      args->agbno, args->len));
	}

	if (!args->isfl) {
		xfs_trans_mod_sb(args->tp, args->wasdel ?
				 XFS_TRANS_SB_RES_FDBLOCKS :
				 XFS_TRANS_SB_FDBLOCKS,
				 -((long)(args->len)));
	}

	XFS_STATS_INC(xs_allocx);
	XFS_STATS_ADD(xs_allocb, args->len);
	return error;
}

STATIC int			
xfs_alloc_ag_vextent_exact(
	xfs_alloc_arg_t	*args)	
{
	xfs_btree_cur_t	*bno_cur;
	xfs_btree_cur_t	*cnt_cur;
	int		error;
	xfs_agblock_t	fbno;	
	xfs_extlen_t	flen;	
	xfs_agblock_t	tbno;	
	xfs_extlen_t	tlen;	
	xfs_agblock_t	tend;	
	int		i;	

	ASSERT(args->alignment == 1);

	bno_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
					  args->agno, XFS_BTNUM_BNO);

	error = xfs_alloc_lookup_le(bno_cur, args->agbno, args->minlen, &i);
	if (error)
		goto error0;
	if (!i)
		goto not_found;

	error = xfs_alloc_get_rec(bno_cur, &fbno, &flen, &i);
	if (error)
		goto error0;
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	ASSERT(fbno <= args->agbno);

	xfs_alloc_busy_trim(args, fbno, flen, &tbno, &tlen);

	if (tbno > args->agbno)
		goto not_found;
	if (tlen < args->minlen)
		goto not_found;
	tend = tbno + tlen;
	if (tend < args->agbno + args->minlen)
		goto not_found;

	args->len = XFS_AGBLOCK_MIN(tend, args->agbno + args->maxlen)
						- args->agbno;
	xfs_alloc_fix_len(args);
	if (!xfs_alloc_fix_minleft(args))
		goto not_found;

	ASSERT(args->agbno + args->len <= tend);

	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
		args->agno, XFS_BTNUM_CNT);
	ASSERT(args->agbno + args->len <=
		be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_length));
	error = xfs_alloc_fixup_trees(cnt_cur, bno_cur, fbno, flen, args->agbno,
				      args->len, XFSA_FIXUP_BNO_OK);
	if (error) {
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
		goto error0;
	}

	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);

	args->wasfromfl = 0;
	trace_xfs_alloc_exact_done(args);
	return 0;

not_found:
	
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	args->agbno = NULLAGBLOCK;
	trace_xfs_alloc_exact_notfound(args);
	return 0;

error0:
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	trace_xfs_alloc_exact_error(args);
	return error;
}

STATIC int
xfs_alloc_find_best_extent(
	struct xfs_alloc_arg	*args,	
	struct xfs_btree_cur	**gcur,	
	struct xfs_btree_cur	**scur,	
	xfs_agblock_t		gdiff,	
	xfs_agblock_t		*sbno,	
	xfs_extlen_t		*slen,	
	xfs_agblock_t		*sbnoa,	
	xfs_extlen_t		*slena,	
	int			dir)	
{
	xfs_agblock_t		new;
	xfs_agblock_t		sdiff;
	int			error;
	int			i;

	
	if (!gdiff)
		goto out_use_good;

	do {
		error = xfs_alloc_get_rec(*scur, sbno, slen, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		xfs_alloc_compute_aligned(args, *sbno, *slen, sbnoa, slena);

		if (!dir) {
			if (*sbnoa >= args->agbno + gdiff)
				goto out_use_good;
		} else {
			if (*sbnoa <= args->agbno - gdiff)
				goto out_use_good;
		}

		if (*slena >= args->minlen) {
			args->len = XFS_EXTLEN_MIN(*slena, args->maxlen);
			xfs_alloc_fix_len(args);

			sdiff = xfs_alloc_compute_diff(args->agbno, args->len,
						       args->alignment, *sbnoa,
						       *slena, &new);

			if (sdiff < gdiff)
				goto out_use_search;
			goto out_use_good;
		}

		if (!dir)
			error = xfs_btree_increment(*scur, 0, &i);
		else
			error = xfs_btree_decrement(*scur, 0, &i);
		if (error)
			goto error0;
	} while (i);

out_use_good:
	xfs_btree_del_cursor(*scur, XFS_BTREE_NOERROR);
	*scur = NULL;
	return 0;

out_use_search:
	xfs_btree_del_cursor(*gcur, XFS_BTREE_NOERROR);
	*gcur = NULL;
	return 0;

error0:
	
	return error;
}

STATIC int				
xfs_alloc_ag_vextent_near(
	xfs_alloc_arg_t	*args)		
{
	xfs_btree_cur_t	*bno_cur_gt;	
	xfs_btree_cur_t	*bno_cur_lt;	
	xfs_btree_cur_t	*cnt_cur;	
	xfs_agblock_t	gtbno;		
	xfs_agblock_t	gtbnoa;		
	xfs_extlen_t	gtdiff;		
	xfs_extlen_t	gtlen;		
	xfs_extlen_t	gtlena;		
	xfs_agblock_t	gtnew;		
	int		error;		
	int		i;		
	int		j;		
	xfs_agblock_t	ltbno;		
	xfs_agblock_t	ltbnoa;		
	xfs_extlen_t	ltdiff;		
	xfs_extlen_t	ltlen;		
	xfs_extlen_t	ltlena;		
	xfs_agblock_t	ltnew;		
	xfs_extlen_t	rlen;		
	int		forced = 0;
#if defined(DEBUG) && defined(__KERNEL__)
	int		dofirst;	

	dofirst = random32() & 1;
#endif

restart:
	bno_cur_lt = NULL;
	bno_cur_gt = NULL;
	ltlen = 0;
	gtlena = 0;
	ltlena = 0;

	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
		args->agno, XFS_BTNUM_CNT);

	if ((error = xfs_alloc_lookup_ge(cnt_cur, 0, args->maxlen, &i)))
		goto error0;
	if (!i) {
		if ((error = xfs_alloc_ag_vextent_small(args, cnt_cur, &ltbno,
				&ltlen, &i)))
			goto error0;
		if (i == 0 || ltlen == 0) {
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			trace_xfs_alloc_near_noentry(args);
			return 0;
		}
		ASSERT(i == 1);
	}
	args->wasfromfl = 0;

	/*
	 * First algorithm.
	 * If the requested extent is large wrt the freespaces available
	 * in this a.g., then the cursor will be pointing to a btree entry
	 * near the right edge of the tree.  If it's in the last btree leaf
	 * block, then we just examine all the entries in that block
	 * that are big enough, and pick the best one.
	 * This is written as a while loop so we can break out of it,
	 * but we never loop back to the top.
	 */
	while (xfs_btree_islastblock(cnt_cur, 0)) {
		xfs_extlen_t	bdiff;
		int		besti=0;
		xfs_extlen_t	blen=0;
		xfs_agblock_t	bnew=0;

#if defined(DEBUG) && defined(__KERNEL__)
		if (!dofirst)
			break;
#endif
		if (ltlen || args->alignment > 1) {
			cnt_cur->bc_ptrs[0] = 1;
			do {
				if ((error = xfs_alloc_get_rec(cnt_cur, &ltbno,
						&ltlen, &i)))
					goto error0;
				XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
				if (ltlen >= args->minlen)
					break;
				if ((error = xfs_btree_increment(cnt_cur, 0, &i)))
					goto error0;
			} while (i);
			ASSERT(ltlen >= args->minlen);
			if (!i)
				break;
		}
		i = cnt_cur->bc_ptrs[0];
		for (j = 1, blen = 0, bdiff = 0;
		     !error && j && (blen < args->maxlen || bdiff > 0);
		     error = xfs_btree_increment(cnt_cur, 0, &j)) {
			if ((error = xfs_alloc_get_rec(cnt_cur, &ltbno, &ltlen, &i)))
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
			xfs_alloc_compute_aligned(args, ltbno, ltlen,
						  &ltbnoa, &ltlena);
			if (ltlena < args->minlen)
				continue;
			args->len = XFS_EXTLEN_MIN(ltlena, args->maxlen);
			xfs_alloc_fix_len(args);
			ASSERT(args->len >= args->minlen);
			if (args->len < blen)
				continue;
			ltdiff = xfs_alloc_compute_diff(args->agbno, args->len,
				args->alignment, ltbnoa, ltlena, &ltnew);
			if (ltnew != NULLAGBLOCK &&
			    (args->len > blen || ltdiff < bdiff)) {
				bdiff = ltdiff;
				bnew = ltnew;
				blen = args->len;
				besti = cnt_cur->bc_ptrs[0];
			}
		}
		if (blen == 0)
			break;
		cnt_cur->bc_ptrs[0] = besti;
		if ((error = xfs_alloc_get_rec(cnt_cur, &ltbno, &ltlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		ASSERT(ltbno + ltlen <= be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_length));
		args->len = blen;
		if (!xfs_alloc_fix_minleft(args)) {
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			trace_xfs_alloc_near_nominleft(args);
			return 0;
		}
		blen = args->len;
		args->agbno = bnew;
		ASSERT(bnew >= ltbno);
		ASSERT(bnew + blen <= ltbno + ltlen);
		bno_cur_lt = xfs_allocbt_init_cursor(args->mp, args->tp,
			args->agbp, args->agno, XFS_BTNUM_BNO);
		if ((error = xfs_alloc_fixup_trees(cnt_cur, bno_cur_lt, ltbno,
				ltlen, bnew, blen, XFSA_FIXUP_CNT_OK)))
			goto error0;
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
		xfs_btree_del_cursor(bno_cur_lt, XFS_BTREE_NOERROR);

		trace_xfs_alloc_near_first(args);
		return 0;
	}
	bno_cur_lt = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
		args->agno, XFS_BTNUM_BNO);
	if ((error = xfs_alloc_lookup_le(bno_cur_lt, args->agbno, args->maxlen, &i)))
		goto error0;
	if (!i) {
		bno_cur_gt = bno_cur_lt;
		bno_cur_lt = NULL;
	}
	else if ((error = xfs_btree_dup_cursor(bno_cur_lt, &bno_cur_gt)))
		goto error0;
	if ((error = xfs_btree_increment(bno_cur_gt, 0, &i)))
		goto error0;
	if (!i) {
		xfs_btree_del_cursor(bno_cur_gt, XFS_BTREE_NOERROR);
		bno_cur_gt = NULL;
	}
	do {
		if (bno_cur_lt) {
			if ((error = xfs_alloc_get_rec(bno_cur_lt, &ltbno, &ltlen, &i)))
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
			xfs_alloc_compute_aligned(args, ltbno, ltlen,
						  &ltbnoa, &ltlena);
			if (ltlena >= args->minlen)
				break;
			if ((error = xfs_btree_decrement(bno_cur_lt, 0, &i)))
				goto error0;
			if (!i) {
				xfs_btree_del_cursor(bno_cur_lt,
						     XFS_BTREE_NOERROR);
				bno_cur_lt = NULL;
			}
		}
		if (bno_cur_gt) {
			if ((error = xfs_alloc_get_rec(bno_cur_gt, &gtbno, &gtlen, &i)))
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
			xfs_alloc_compute_aligned(args, gtbno, gtlen,
						  &gtbnoa, &gtlena);
			if (gtlena >= args->minlen)
				break;
			if ((error = xfs_btree_increment(bno_cur_gt, 0, &i)))
				goto error0;
			if (!i) {
				xfs_btree_del_cursor(bno_cur_gt,
						     XFS_BTREE_NOERROR);
				bno_cur_gt = NULL;
			}
		}
	} while (bno_cur_lt || bno_cur_gt);

	if (bno_cur_lt && bno_cur_gt) {
		if (ltlena >= args->minlen) {
			args->len = XFS_EXTLEN_MIN(ltlena, args->maxlen);
			xfs_alloc_fix_len(args);
			ltdiff = xfs_alloc_compute_diff(args->agbno, args->len,
				args->alignment, ltbnoa, ltlena, &ltnew);

			error = xfs_alloc_find_best_extent(args,
						&bno_cur_lt, &bno_cur_gt,
						ltdiff, &gtbno, &gtlen,
						&gtbnoa, &gtlena,
						0 );
		} else {
			ASSERT(gtlena >= args->minlen);

			args->len = XFS_EXTLEN_MIN(gtlena, args->maxlen);
			xfs_alloc_fix_len(args);
			gtdiff = xfs_alloc_compute_diff(args->agbno, args->len,
				args->alignment, gtbnoa, gtlena, &gtnew);

			error = xfs_alloc_find_best_extent(args,
						&bno_cur_gt, &bno_cur_lt,
						gtdiff, &ltbno, &ltlen,
						&ltbnoa, &ltlena,
						1 );
		}

		if (error)
			goto error0;
	}

	if (bno_cur_lt == NULL && bno_cur_gt == NULL) {
		if (!forced++) {
			trace_xfs_alloc_near_busy(args);
			xfs_log_force(args->mp, XFS_LOG_SYNC);
			goto restart;
		}

		trace_xfs_alloc_size_neither(args);
		args->agbno = NULLAGBLOCK;
		return 0;
	}

	if (bno_cur_gt) {
		bno_cur_lt = bno_cur_gt;
		bno_cur_gt = NULL;
		ltbno = gtbno;
		ltbnoa = gtbnoa;
		ltlen = gtlen;
		ltlena = gtlena;
		j = 1;
	} else
		j = 0;

	args->len = XFS_EXTLEN_MIN(ltlena, args->maxlen);
	xfs_alloc_fix_len(args);
	if (!xfs_alloc_fix_minleft(args)) {
		trace_xfs_alloc_near_nominleft(args);
		xfs_btree_del_cursor(bno_cur_lt, XFS_BTREE_NOERROR);
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
		return 0;
	}
	rlen = args->len;
	(void)xfs_alloc_compute_diff(args->agbno, rlen, args->alignment,
				     ltbnoa, ltlena, &ltnew);
	ASSERT(ltnew >= ltbno);
	ASSERT(ltnew + rlen <= ltbnoa + ltlena);
	ASSERT(ltnew + rlen <= be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_length));
	args->agbno = ltnew;

	if ((error = xfs_alloc_fixup_trees(cnt_cur, bno_cur_lt, ltbno, ltlen,
			ltnew, rlen, XFSA_FIXUP_BNO_OK)))
		goto error0;

	if (j)
		trace_xfs_alloc_near_greater(args);
	else
		trace_xfs_alloc_near_lesser(args);

	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(bno_cur_lt, XFS_BTREE_NOERROR);
	return 0;

 error0:
	trace_xfs_alloc_near_error(args);
	if (cnt_cur != NULL)
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
	if (bno_cur_lt != NULL)
		xfs_btree_del_cursor(bno_cur_lt, XFS_BTREE_ERROR);
	if (bno_cur_gt != NULL)
		xfs_btree_del_cursor(bno_cur_gt, XFS_BTREE_ERROR);
	return error;
}

STATIC int				
xfs_alloc_ag_vextent_size(
	xfs_alloc_arg_t	*args)		
{
	xfs_btree_cur_t	*bno_cur;	
	xfs_btree_cur_t	*cnt_cur;	
	int		error;		
	xfs_agblock_t	fbno;		
	xfs_extlen_t	flen;		
	int		i;		
	xfs_agblock_t	rbno;		
	xfs_extlen_t	rlen;		
	int		forced = 0;

restart:
	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
		args->agno, XFS_BTNUM_CNT);
	bno_cur = NULL;

	if ((error = xfs_alloc_lookup_ge(cnt_cur, 0,
			args->maxlen + args->alignment - 1, &i)))
		goto error0;

	if (!i || forced > 1) {
		error = xfs_alloc_ag_vextent_small(args, cnt_cur,
						   &fbno, &flen, &i);
		if (error)
			goto error0;
		if (i == 0 || flen == 0) {
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			trace_xfs_alloc_size_noentry(args);
			return 0;
		}
		ASSERT(i == 1);
		xfs_alloc_compute_aligned(args, fbno, flen, &rbno, &rlen);
	} else {
		for (;;) {
			error = xfs_alloc_get_rec(cnt_cur, &fbno, &flen, &i);
			if (error)
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

			xfs_alloc_compute_aligned(args, fbno, flen,
						  &rbno, &rlen);

			if (rlen >= args->maxlen)
				break;

			error = xfs_btree_increment(cnt_cur, 0, &i);
			if (error)
				goto error0;
			if (i == 0) {
				xfs_btree_del_cursor(cnt_cur,
						     XFS_BTREE_NOERROR);
				trace_xfs_alloc_size_busy(args);
				if (!forced++)
					xfs_log_force(args->mp, XFS_LOG_SYNC);
				goto restart;
			}
		}
	}

	rlen = XFS_EXTLEN_MIN(args->maxlen, rlen);
	XFS_WANT_CORRUPTED_GOTO(rlen == 0 ||
			(rlen <= flen && rbno + rlen <= fbno + flen), error0);
	if (rlen < args->maxlen) {
		xfs_agblock_t	bestfbno;
		xfs_extlen_t	bestflen;
		xfs_agblock_t	bestrbno;
		xfs_extlen_t	bestrlen;

		bestrlen = rlen;
		bestrbno = rbno;
		bestflen = flen;
		bestfbno = fbno;
		for (;;) {
			if ((error = xfs_btree_decrement(cnt_cur, 0, &i)))
				goto error0;
			if (i == 0)
				break;
			if ((error = xfs_alloc_get_rec(cnt_cur, &fbno, &flen,
					&i)))
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
			if (flen < bestrlen)
				break;
			xfs_alloc_compute_aligned(args, fbno, flen,
						  &rbno, &rlen);
			rlen = XFS_EXTLEN_MIN(args->maxlen, rlen);
			XFS_WANT_CORRUPTED_GOTO(rlen == 0 ||
				(rlen <= flen && rbno + rlen <= fbno + flen),
				error0);
			if (rlen > bestrlen) {
				bestrlen = rlen;
				bestrbno = rbno;
				bestflen = flen;
				bestfbno = fbno;
				if (rlen == args->maxlen)
					break;
			}
		}
		if ((error = xfs_alloc_lookup_eq(cnt_cur, bestfbno, bestflen,
				&i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		rlen = bestrlen;
		rbno = bestrbno;
		flen = bestflen;
		fbno = bestfbno;
	}
	args->wasfromfl = 0;
	args->len = rlen;
	if (rlen < args->minlen) {
		if (!forced++) {
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			trace_xfs_alloc_size_busy(args);
			xfs_log_force(args->mp, XFS_LOG_SYNC);
			goto restart;
		}
		goto out_nominleft;
	}
	xfs_alloc_fix_len(args);

	if (!xfs_alloc_fix_minleft(args))
		goto out_nominleft;
	rlen = args->len;
	XFS_WANT_CORRUPTED_GOTO(rlen <= flen, error0);
	bno_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
		args->agno, XFS_BTNUM_BNO);
	if ((error = xfs_alloc_fixup_trees(cnt_cur, bno_cur, fbno, flen,
			rbno, rlen, XFSA_FIXUP_CNT_OK)))
		goto error0;
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	cnt_cur = bno_cur = NULL;
	args->len = rlen;
	args->agbno = rbno;
	XFS_WANT_CORRUPTED_GOTO(
		args->agbno + args->len <=
			be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_length),
		error0);
	trace_xfs_alloc_size_done(args);
	return 0;

error0:
	trace_xfs_alloc_size_error(args);
	if (cnt_cur)
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
	if (bno_cur)
		xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	return error;

out_nominleft:
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	trace_xfs_alloc_size_nominleft(args);
	args->agbno = NULLAGBLOCK;
	return 0;
}

STATIC int			
xfs_alloc_ag_vextent_small(
	xfs_alloc_arg_t	*args,	
	xfs_btree_cur_t	*ccur,	
	xfs_agblock_t	*fbnop,	
	xfs_extlen_t	*flenp,	
	int		*stat)	
{
	int		error;
	xfs_agblock_t	fbno;
	xfs_extlen_t	flen;
	int		i;

	if ((error = xfs_btree_decrement(ccur, 0, &i)))
		goto error0;
	if (i) {
		if ((error = xfs_alloc_get_rec(ccur, &fbno, &flen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	}
	else if (args->minlen == 1 && args->alignment == 1 && !args->isfl &&
		 (be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_flcount)
		  > args->minleft)) {
		error = xfs_alloc_get_freelist(args->tp, args->agbp, &fbno, 0);
		if (error)
			goto error0;
		if (fbno != NULLAGBLOCK) {
			xfs_alloc_busy_reuse(args->mp, args->agno, fbno, 1,
					     args->userdata);

			if (args->userdata) {
				xfs_buf_t	*bp;

				bp = xfs_btree_get_bufs(args->mp, args->tp,
					args->agno, fbno, 0);
				xfs_trans_binval(args->tp, bp);
			}
			args->len = 1;
			args->agbno = fbno;
			XFS_WANT_CORRUPTED_GOTO(
				args->agbno + args->len <=
				be32_to_cpu(XFS_BUF_TO_AGF(args->agbp)->agf_length),
				error0);
			args->wasfromfl = 1;
			trace_xfs_alloc_small_freelist(args);
			*stat = 0;
			return 0;
		}
		else
			flen = 0;
	}
	else {
		fbno = NULLAGBLOCK;
		flen = 0;
	}
	if (flen < args->minlen) {
		args->agbno = NULLAGBLOCK;
		trace_xfs_alloc_small_notenough(args);
		flen = 0;
	}
	*fbnop = fbno;
	*flenp = flen;
	*stat = 1;
	trace_xfs_alloc_small_done(args);
	return 0;

error0:
	trace_xfs_alloc_small_error(args);
	return error;
}

STATIC int			
xfs_free_ag_extent(
	xfs_trans_t	*tp,	
	xfs_buf_t	*agbp,	
	xfs_agnumber_t	agno,	
	xfs_agblock_t	bno,	
	xfs_extlen_t	len,	
	int		isfl)	
{
	xfs_btree_cur_t	*bno_cur;	
	xfs_btree_cur_t	*cnt_cur;	
	int		error;		
	xfs_agblock_t	gtbno;		
	xfs_extlen_t	gtlen;		
	int		haveleft;	
	int		haveright;	
	int		i;		
	xfs_agblock_t	ltbno;		
	xfs_extlen_t	ltlen;		
	xfs_mount_t	*mp;		
	xfs_agblock_t	nbno;		
	xfs_extlen_t	nlen;		
	xfs_perag_t	*pag;		

	mp = tp->t_mountp;
	bno_cur = xfs_allocbt_init_cursor(mp, tp, agbp, agno, XFS_BTNUM_BNO);
	cnt_cur = NULL;
	if ((error = xfs_alloc_lookup_le(bno_cur, bno, len, &haveleft)))
		goto error0;
	if (haveleft) {
		if ((error = xfs_alloc_get_rec(bno_cur, &ltbno, &ltlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if (ltbno + ltlen < bno)
			haveleft = 0;
		else {
			XFS_WANT_CORRUPTED_GOTO(ltbno + ltlen <= bno, error0);
		}
	}
	if ((error = xfs_btree_increment(bno_cur, 0, &haveright)))
		goto error0;
	if (haveright) {
		if ((error = xfs_alloc_get_rec(bno_cur, &gtbno, &gtlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if (bno + len < gtbno)
			haveright = 0;
		else {
			XFS_WANT_CORRUPTED_GOTO(gtbno >= bno + len, error0);
		}
	}
	cnt_cur = xfs_allocbt_init_cursor(mp, tp, agbp, agno, XFS_BTNUM_CNT);
	if (haveleft && haveright) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, ltbno, ltlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_alloc_lookup_eq(cnt_cur, gtbno, gtlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_delete(bno_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_decrement(bno_cur, 0, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
#ifdef DEBUG
		{
			xfs_agblock_t	xxbno;
			xfs_extlen_t	xxlen;

			if ((error = xfs_alloc_get_rec(bno_cur, &xxbno, &xxlen,
					&i)))
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(
				i == 1 && xxbno == ltbno && xxlen == ltlen,
				error0);
		}
#endif
		nbno = ltbno;
		nlen = len + ltlen + gtlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	else if (haveleft) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, ltbno, ltlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_decrement(bno_cur, 0, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		nbno = ltbno;
		nlen = len + ltlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	else if (haveright) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, gtbno, gtlen, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		nbno = bno;
		nlen = len + gtlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	else {
		nbno = bno;
		nlen = len;
		if ((error = xfs_btree_insert(bno_cur, &i)))
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	}
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	bno_cur = NULL;
	if ((error = xfs_alloc_lookup_eq(cnt_cur, nbno, nlen, &i)))
		goto error0;
	XFS_WANT_CORRUPTED_GOTO(i == 0, error0);
	if ((error = xfs_btree_insert(cnt_cur, &i)))
		goto error0;
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	cnt_cur = NULL;

	pag = xfs_perag_get(mp, agno);
	error = xfs_alloc_update_counters(tp, pag, agbp, len);
	xfs_perag_put(pag);
	if (error)
		goto error0;

	if (!isfl)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FDBLOCKS, (long)len);
	XFS_STATS_INC(xs_freex);
	XFS_STATS_ADD(xs_freeb, len);

	trace_xfs_free_extent(mp, agno, bno, len, isfl, haveleft, haveright);

	return 0;

 error0:
	trace_xfs_free_extent(mp, agno, bno, len, isfl, -1, -1);
	if (bno_cur)
		xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	if (cnt_cur)
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
	return error;
}


void
xfs_alloc_compute_maxlevels(
	xfs_mount_t	*mp)	
{
	int		level;
	uint		maxblocks;
	uint		maxleafents;
	int		minleafrecs;
	int		minnoderecs;

	maxleafents = (mp->m_sb.sb_agblocks + 1) / 2;
	minleafrecs = mp->m_alloc_mnr[0];
	minnoderecs = mp->m_alloc_mnr[1];
	maxblocks = (maxleafents + minleafrecs - 1) / minleafrecs;
	for (level = 1; maxblocks > 1; level++)
		maxblocks = (maxblocks + minnoderecs - 1) / minnoderecs;
	mp->m_ag_maxlevels = level;
}

xfs_extlen_t
xfs_alloc_longest_free_extent(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag)
{
	xfs_extlen_t		need, delta = 0;

	need = XFS_MIN_FREELIST_PAG(pag, mp);
	if (need > pag->pagf_flcount)
		delta = need - pag->pagf_flcount;

	if (pag->pagf_longest > delta)
		return pag->pagf_longest - delta;
	return pag->pagf_flcount > 0 || pag->pagf_longest > 0;
}

STATIC int			
xfs_alloc_fix_freelist(
	xfs_alloc_arg_t	*args,	
	int		flags)	
{
	xfs_buf_t	*agbp;	
	xfs_agf_t	*agf;	
	xfs_buf_t	*agflbp;
	xfs_agblock_t	bno;	
	xfs_extlen_t	delta;	
	int		error;	
	xfs_extlen_t	longest;
	xfs_mount_t	*mp;	
	xfs_extlen_t	need;	
	xfs_perag_t	*pag;	
	xfs_alloc_arg_t	targs;	
	xfs_trans_t	*tp;	

	mp = args->mp;

	pag = args->pag;
	tp = args->tp;
	if (!pag->pagf_init) {
		if ((error = xfs_alloc_read_agf(mp, tp, args->agno, flags,
				&agbp)))
			return error;
		if (!pag->pagf_init) {
			ASSERT(flags & XFS_ALLOC_FLAG_TRYLOCK);
			ASSERT(!(flags & XFS_ALLOC_FLAG_FREEING));
			args->agbp = NULL;
			return 0;
		}
	} else
		agbp = NULL;

	if (pag->pagf_metadata && args->userdata &&
	    (flags & XFS_ALLOC_FLAG_TRYLOCK)) {
		ASSERT(!(flags & XFS_ALLOC_FLAG_FREEING));
		args->agbp = NULL;
		return 0;
	}

	if (!(flags & XFS_ALLOC_FLAG_FREEING)) {
		need = XFS_MIN_FREELIST_PAG(pag, mp);
		longest = xfs_alloc_longest_free_extent(mp, pag);
		if ((args->minlen + args->alignment + args->minalignslop - 1) >
				longest ||
		    ((int)(pag->pagf_freeblks + pag->pagf_flcount -
			   need - args->total) < (int)args->minleft)) {
			if (agbp)
				xfs_trans_brelse(tp, agbp);
			args->agbp = NULL;
			return 0;
		}
	}

	if (agbp == NULL) {
		if ((error = xfs_alloc_read_agf(mp, tp, args->agno, flags,
				&agbp)))
			return error;
		if (agbp == NULL) {
			ASSERT(flags & XFS_ALLOC_FLAG_TRYLOCK);
			ASSERT(!(flags & XFS_ALLOC_FLAG_FREEING));
			args->agbp = NULL;
			return 0;
		}
	}
	agf = XFS_BUF_TO_AGF(agbp);
	need = XFS_MIN_FREELIST(agf, mp);
	if (!(flags & XFS_ALLOC_FLAG_FREEING)) {
		delta = need > be32_to_cpu(agf->agf_flcount) ?
			(need - be32_to_cpu(agf->agf_flcount)) : 0;
		longest = be32_to_cpu(agf->agf_longest);
		longest = (longest > delta) ? (longest - delta) :
			(be32_to_cpu(agf->agf_flcount) > 0 || longest > 0);
		if ((args->minlen + args->alignment + args->minalignslop - 1) >
				longest ||
		    ((int)(be32_to_cpu(agf->agf_freeblks) +
		     be32_to_cpu(agf->agf_flcount) - need - args->total) <
				(int)args->minleft)) {
			xfs_trans_brelse(tp, agbp);
			args->agbp = NULL;
			return 0;
		}
	}
	while (be32_to_cpu(agf->agf_flcount) > need) {
		xfs_buf_t	*bp;

		error = xfs_alloc_get_freelist(tp, agbp, &bno, 0);
		if (error)
			return error;
		if ((error = xfs_free_ag_extent(tp, agbp, args->agno, bno, 1, 1)))
			return error;
		bp = xfs_btree_get_bufs(mp, tp, args->agno, bno, 0);
		xfs_trans_binval(tp, bp);
	}
	targs.tp = tp;
	targs.mp = mp;
	targs.agbp = agbp;
	targs.agno = args->agno;
	targs.mod = targs.minleft = targs.wasdel = targs.userdata =
		targs.minalignslop = 0;
	targs.alignment = targs.minlen = targs.prod = targs.isfl = 1;
	targs.type = XFS_ALLOCTYPE_THIS_AG;
	targs.pag = pag;
	if ((error = xfs_alloc_read_agfl(mp, tp, targs.agno, &agflbp)))
		return error;
	while (be32_to_cpu(agf->agf_flcount) < need) {
		targs.agbno = 0;
		targs.maxlen = need - be32_to_cpu(agf->agf_flcount);
		if ((error = xfs_alloc_ag_vextent(&targs))) {
			xfs_trans_brelse(tp, agflbp);
			return error;
		}
		if (targs.agbno == NULLAGBLOCK) {
			if (flags & XFS_ALLOC_FLAG_FREEING)
				break;
			xfs_trans_brelse(tp, agflbp);
			args->agbp = NULL;
			return 0;
		}
		for (bno = targs.agbno; bno < targs.agbno + targs.len; bno++) {
			error = xfs_alloc_put_freelist(tp, agbp,
							agflbp, bno, 0);
			if (error)
				return error;
		}
	}
	xfs_trans_brelse(tp, agflbp);
	args->agbp = agbp;
	return 0;
}

int				
xfs_alloc_get_freelist(
	xfs_trans_t	*tp,	
	xfs_buf_t	*agbp,	
	xfs_agblock_t	*bnop,	
	int		btreeblk) 
{
	xfs_agf_t	*agf;	
	xfs_agfl_t	*agfl;	
	xfs_buf_t	*agflbp;
	xfs_agblock_t	bno;	
	int		error;
	int		logflags;
	xfs_mount_t	*mp;	
	xfs_perag_t	*pag;	

	agf = XFS_BUF_TO_AGF(agbp);
	if (!agf->agf_flcount) {
		*bnop = NULLAGBLOCK;
		return 0;
	}
	mp = tp->t_mountp;
	if ((error = xfs_alloc_read_agfl(mp, tp,
			be32_to_cpu(agf->agf_seqno), &agflbp)))
		return error;
	agfl = XFS_BUF_TO_AGFL(agflbp);
	bno = be32_to_cpu(agfl->agfl_bno[be32_to_cpu(agf->agf_flfirst)]);
	be32_add_cpu(&agf->agf_flfirst, 1);
	xfs_trans_brelse(tp, agflbp);
	if (be32_to_cpu(agf->agf_flfirst) == XFS_AGFL_SIZE(mp))
		agf->agf_flfirst = 0;

	pag = xfs_perag_get(mp, be32_to_cpu(agf->agf_seqno));
	be32_add_cpu(&agf->agf_flcount, -1);
	xfs_trans_agflist_delta(tp, -1);
	pag->pagf_flcount--;
	xfs_perag_put(pag);

	logflags = XFS_AGF_FLFIRST | XFS_AGF_FLCOUNT;
	if (btreeblk) {
		be32_add_cpu(&agf->agf_btreeblks, 1);
		pag->pagf_btreeblks++;
		logflags |= XFS_AGF_BTREEBLKS;
	}

	xfs_alloc_log_agf(tp, agbp, logflags);
	*bnop = bno;

	return 0;
}

void
xfs_alloc_log_agf(
	xfs_trans_t	*tp,	
	xfs_buf_t	*bp,	
	int		fields)	
{
	int	first;		
	int	last;		
	static const short	offsets[] = {
		offsetof(xfs_agf_t, agf_magicnum),
		offsetof(xfs_agf_t, agf_versionnum),
		offsetof(xfs_agf_t, agf_seqno),
		offsetof(xfs_agf_t, agf_length),
		offsetof(xfs_agf_t, agf_roots[0]),
		offsetof(xfs_agf_t, agf_levels[0]),
		offsetof(xfs_agf_t, agf_flfirst),
		offsetof(xfs_agf_t, agf_fllast),
		offsetof(xfs_agf_t, agf_flcount),
		offsetof(xfs_agf_t, agf_freeblks),
		offsetof(xfs_agf_t, agf_longest),
		offsetof(xfs_agf_t, agf_btreeblks),
		sizeof(xfs_agf_t)
	};

	trace_xfs_agf(tp->t_mountp, XFS_BUF_TO_AGF(bp), fields, _RET_IP_);

	xfs_btree_offsets(fields, offsets, XFS_AGF_NUM_BITS, &first, &last);
	xfs_trans_log_buf(tp, bp, (uint)first, (uint)last);
}

int					
xfs_alloc_pagf_init(
	xfs_mount_t		*mp,	
	xfs_trans_t		*tp,	
	xfs_agnumber_t		agno,	
	int			flags)	
{
	xfs_buf_t		*bp;
	int			error;

	if ((error = xfs_alloc_read_agf(mp, tp, agno, flags, &bp)))
		return error;
	if (bp)
		xfs_trans_brelse(tp, bp);
	return 0;
}

int					
xfs_alloc_put_freelist(
	xfs_trans_t		*tp,	
	xfs_buf_t		*agbp,	
	xfs_buf_t		*agflbp,
	xfs_agblock_t		bno,	
	int			btreeblk) 
{
	xfs_agf_t		*agf;	
	xfs_agfl_t		*agfl;	
	__be32			*blockp;
	int			error;
	int			logflags;
	xfs_mount_t		*mp;	
	xfs_perag_t		*pag;	

	agf = XFS_BUF_TO_AGF(agbp);
	mp = tp->t_mountp;

	if (!agflbp && (error = xfs_alloc_read_agfl(mp, tp,
			be32_to_cpu(agf->agf_seqno), &agflbp)))
		return error;
	agfl = XFS_BUF_TO_AGFL(agflbp);
	be32_add_cpu(&agf->agf_fllast, 1);
	if (be32_to_cpu(agf->agf_fllast) == XFS_AGFL_SIZE(mp))
		agf->agf_fllast = 0;

	pag = xfs_perag_get(mp, be32_to_cpu(agf->agf_seqno));
	be32_add_cpu(&agf->agf_flcount, 1);
	xfs_trans_agflist_delta(tp, 1);
	pag->pagf_flcount++;

	logflags = XFS_AGF_FLLAST | XFS_AGF_FLCOUNT;
	if (btreeblk) {
		be32_add_cpu(&agf->agf_btreeblks, -1);
		pag->pagf_btreeblks--;
		logflags |= XFS_AGF_BTREEBLKS;
	}
	xfs_perag_put(pag);

	xfs_alloc_log_agf(tp, agbp, logflags);

	ASSERT(be32_to_cpu(agf->agf_flcount) <= XFS_AGFL_SIZE(mp));
	blockp = &agfl->agfl_bno[be32_to_cpu(agf->agf_fllast)];
	*blockp = cpu_to_be32(bno);
	xfs_alloc_log_agf(tp, agbp, logflags);
	xfs_trans_log_buf(tp, agflbp,
		(int)((xfs_caddr_t)blockp - (xfs_caddr_t)agfl),
		(int)((xfs_caddr_t)blockp - (xfs_caddr_t)agfl +
			sizeof(xfs_agblock_t) - 1));
	return 0;
}

int					
xfs_read_agf(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_agnumber_t		agno,	
	int			flags,	
	struct xfs_buf		**bpp)	
{
	struct xfs_agf	*agf;		
	int		agf_ok;		
	int		error;

	ASSERT(agno != NULLAGNUMBER);
	error = xfs_trans_read_buf(
			mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), flags, bpp);
	if (error)
		return error;
	if (!*bpp)
		return 0;

	ASSERT(!(*bpp)->b_error);
	agf = XFS_BUF_TO_AGF(*bpp);

	agf_ok =
		agf->agf_magicnum == cpu_to_be32(XFS_AGF_MAGIC) &&
		XFS_AGF_GOOD_VERSION(be32_to_cpu(agf->agf_versionnum)) &&
		be32_to_cpu(agf->agf_freeblks) <= be32_to_cpu(agf->agf_length) &&
		be32_to_cpu(agf->agf_flfirst) < XFS_AGFL_SIZE(mp) &&
		be32_to_cpu(agf->agf_fllast) < XFS_AGFL_SIZE(mp) &&
		be32_to_cpu(agf->agf_flcount) <= XFS_AGFL_SIZE(mp) &&
		be32_to_cpu(agf->agf_seqno) == agno;
	if (xfs_sb_version_haslazysbcount(&mp->m_sb))
		agf_ok = agf_ok && be32_to_cpu(agf->agf_btreeblks) <=
						be32_to_cpu(agf->agf_length);
	if (unlikely(XFS_TEST_ERROR(!agf_ok, mp, XFS_ERRTAG_ALLOC_READ_AGF,
			XFS_RANDOM_ALLOC_READ_AGF))) {
		XFS_CORRUPTION_ERROR("xfs_alloc_read_agf",
				     XFS_ERRLEVEL_LOW, mp, agf);
		xfs_trans_brelse(tp, *bpp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	xfs_buf_set_ref(*bpp, XFS_AGF_REF);
	return 0;
}

int					
xfs_alloc_read_agf(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_agnumber_t		agno,	
	int			flags,	
	struct xfs_buf		**bpp)	
{
	struct xfs_agf		*agf;		
	struct xfs_perag	*pag;		
	int			error;

	ASSERT(agno != NULLAGNUMBER);

	error = xfs_read_agf(mp, tp, agno,
			(flags & XFS_ALLOC_FLAG_TRYLOCK) ? XBF_TRYLOCK : 0,
			bpp);
	if (error)
		return error;
	if (!*bpp)
		return 0;
	ASSERT(!(*bpp)->b_error);

	agf = XFS_BUF_TO_AGF(*bpp);
	pag = xfs_perag_get(mp, agno);
	if (!pag->pagf_init) {
		pag->pagf_freeblks = be32_to_cpu(agf->agf_freeblks);
		pag->pagf_btreeblks = be32_to_cpu(agf->agf_btreeblks);
		pag->pagf_flcount = be32_to_cpu(agf->agf_flcount);
		pag->pagf_longest = be32_to_cpu(agf->agf_longest);
		pag->pagf_levels[XFS_BTNUM_BNOi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNOi]);
		pag->pagf_levels[XFS_BTNUM_CNTi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNTi]);
		spin_lock_init(&pag->pagb_lock);
		pag->pagb_count = 0;
		pag->pagb_tree = RB_ROOT;
		pag->pagf_init = 1;
	}
#ifdef DEBUG
	else if (!XFS_FORCED_SHUTDOWN(mp)) {
		ASSERT(pag->pagf_freeblks == be32_to_cpu(agf->agf_freeblks));
		ASSERT(pag->pagf_btreeblks == be32_to_cpu(agf->agf_btreeblks));
		ASSERT(pag->pagf_flcount == be32_to_cpu(agf->agf_flcount));
		ASSERT(pag->pagf_longest == be32_to_cpu(agf->agf_longest));
		ASSERT(pag->pagf_levels[XFS_BTNUM_BNOi] ==
		       be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNOi]));
		ASSERT(pag->pagf_levels[XFS_BTNUM_CNTi] ==
		       be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNTi]));
	}
#endif
	xfs_perag_put(pag);
	return 0;
}

int				
__xfs_alloc_vextent(
	xfs_alloc_arg_t	*args)	
{
	xfs_agblock_t	agsize;	
	int		error;
	int		flags;	
	xfs_extlen_t	minleft;
	xfs_mount_t	*mp;	
	xfs_agnumber_t	sagno;	
	xfs_alloctype_t	type;	
	int		bump_rotor = 0;
	int		no_min = 0;
	xfs_agnumber_t	rotorstep = xfs_rotorstep; 

	mp = args->mp;
	type = args->otype = args->type;
	args->agbno = NULLAGBLOCK;
	agsize = mp->m_sb.sb_agblocks;
	if (args->maxlen > agsize)
		args->maxlen = agsize;
	if (args->alignment == 0)
		args->alignment = 1;
	ASSERT(XFS_FSB_TO_AGNO(mp, args->fsbno) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, args->fsbno) < agsize);
	ASSERT(args->minlen <= args->maxlen);
	ASSERT(args->minlen <= agsize);
	ASSERT(args->mod < args->prod);
	if (XFS_FSB_TO_AGNO(mp, args->fsbno) >= mp->m_sb.sb_agcount ||
	    XFS_FSB_TO_AGBNO(mp, args->fsbno) >= agsize ||
	    args->minlen > args->maxlen || args->minlen > agsize ||
	    args->mod >= args->prod) {
		args->fsbno = NULLFSBLOCK;
		trace_xfs_alloc_vextent_badargs(args);
		return 0;
	}
	minleft = args->minleft;

	switch (type) {
	case XFS_ALLOCTYPE_THIS_AG:
	case XFS_ALLOCTYPE_NEAR_BNO:
	case XFS_ALLOCTYPE_THIS_BNO:
		args->agno = XFS_FSB_TO_AGNO(mp, args->fsbno);
		args->pag = xfs_perag_get(mp, args->agno);
		args->minleft = 0;
		error = xfs_alloc_fix_freelist(args, 0);
		args->minleft = minleft;
		if (error) {
			trace_xfs_alloc_vextent_nofix(args);
			goto error0;
		}
		if (!args->agbp) {
			trace_xfs_alloc_vextent_noagbp(args);
			break;
		}
		args->agbno = XFS_FSB_TO_AGBNO(mp, args->fsbno);
		if ((error = xfs_alloc_ag_vextent(args)))
			goto error0;
		break;
	case XFS_ALLOCTYPE_START_BNO:
		if ((args->userdata  == XFS_ALLOC_INITIAL_USER_DATA) &&
		    (mp->m_flags & XFS_MOUNT_32BITINODES)) {
			args->fsbno = XFS_AGB_TO_FSB(mp,
					((mp->m_agfrotor / rotorstep) %
					mp->m_sb.sb_agcount), 0);
			bump_rotor = 1;
		}
		args->agbno = XFS_FSB_TO_AGBNO(mp, args->fsbno);
		args->type = XFS_ALLOCTYPE_NEAR_BNO;
		
	case XFS_ALLOCTYPE_ANY_AG:
	case XFS_ALLOCTYPE_START_AG:
	case XFS_ALLOCTYPE_FIRST_AG:
		if (type == XFS_ALLOCTYPE_ANY_AG) {
			args->agno = sagno = (mp->m_agfrotor / rotorstep) %
					mp->m_sb.sb_agcount;
			args->type = XFS_ALLOCTYPE_THIS_AG;
			flags = XFS_ALLOC_FLAG_TRYLOCK;
		} else if (type == XFS_ALLOCTYPE_FIRST_AG) {
			args->agno = XFS_FSB_TO_AGNO(mp, args->fsbno);
			args->type = XFS_ALLOCTYPE_THIS_AG;
			sagno = 0;
			flags = 0;
		} else {
			if (type == XFS_ALLOCTYPE_START_AG)
				args->type = XFS_ALLOCTYPE_THIS_AG;
			args->agno = sagno = XFS_FSB_TO_AGNO(mp, args->fsbno);
			flags = XFS_ALLOC_FLAG_TRYLOCK;
		}
		for (;;) {
			args->pag = xfs_perag_get(mp, args->agno);
			if (no_min) args->minleft = 0;
			error = xfs_alloc_fix_freelist(args, flags);
			args->minleft = minleft;
			if (error) {
				trace_xfs_alloc_vextent_nofix(args);
				goto error0;
			}
			if (args->agbp) {
				if ((error = xfs_alloc_ag_vextent(args)))
					goto error0;
				break;
			}

			trace_xfs_alloc_vextent_loopfailed(args);

			if (args->agno == sagno &&
			    type == XFS_ALLOCTYPE_START_BNO)
				args->type = XFS_ALLOCTYPE_THIS_AG;
			if (++(args->agno) == mp->m_sb.sb_agcount) {
				if (args->firstblock != NULLFSBLOCK)
					args->agno = sagno;
				else
					args->agno = 0;
			}
			if (args->agno == sagno) {
				if (no_min == 1) {
					args->agbno = NULLAGBLOCK;
					trace_xfs_alloc_vextent_allfailed(args);
					break;
				}
				if (flags == 0) {
					no_min = 1;
				} else {
					flags = 0;
					if (type == XFS_ALLOCTYPE_START_BNO) {
						args->agbno = XFS_FSB_TO_AGBNO(mp,
							args->fsbno);
						args->type = XFS_ALLOCTYPE_NEAR_BNO;
					}
				}
			}
			xfs_perag_put(args->pag);
		}
		if (bump_rotor || (type == XFS_ALLOCTYPE_ANY_AG)) {
			if (args->agno == sagno)
				mp->m_agfrotor = (mp->m_agfrotor + 1) %
					(mp->m_sb.sb_agcount * rotorstep);
			else
				mp->m_agfrotor = (args->agno * rotorstep + 1) %
					(mp->m_sb.sb_agcount * rotorstep);
		}
		break;
	default:
		ASSERT(0);
		
	}
	if (args->agbno == NULLAGBLOCK)
		args->fsbno = NULLFSBLOCK;
	else {
		args->fsbno = XFS_AGB_TO_FSB(mp, args->agno, args->agbno);
#ifdef DEBUG
		ASSERT(args->len >= args->minlen);
		ASSERT(args->len <= args->maxlen);
		ASSERT(args->agbno % args->alignment == 0);
		XFS_AG_CHECK_DADDR(mp, XFS_FSB_TO_DADDR(mp, args->fsbno),
			args->len);
#endif
	}
	xfs_perag_put(args->pag);
	return 0;
error0:
	xfs_perag_put(args->pag);
	return error;
}

static void
xfs_alloc_vextent_worker(
	struct work_struct	*work)
{
	struct xfs_alloc_arg	*args = container_of(work,
						struct xfs_alloc_arg, work);
	unsigned long		pflags;

	
	current_set_flags_nested(&pflags, PF_FSTRANS);

	args->result = __xfs_alloc_vextent(args);
	complete(args->done);

	current_restore_flags_nested(&pflags, PF_FSTRANS);
}


int				
xfs_alloc_vextent(
	xfs_alloc_arg_t	*args)	
{
	DECLARE_COMPLETION_ONSTACK(done);

	args->done = &done;
	INIT_WORK(&args->work, xfs_alloc_vextent_worker);
	queue_work(xfs_alloc_wq, &args->work);
	wait_for_completion(&done);
	return args->result;
}

int				
xfs_free_extent(
	xfs_trans_t	*tp,	
	xfs_fsblock_t	bno,	
	xfs_extlen_t	len)	
{
	xfs_alloc_arg_t	args;
	int		error;

	ASSERT(len != 0);
	memset(&args, 0, sizeof(xfs_alloc_arg_t));
	args.tp = tp;
	args.mp = tp->t_mountp;

	args.agno = XFS_FSB_TO_AGNO(args.mp, bno);
	if (args.agno >= args.mp->m_sb.sb_agcount)
		return EFSCORRUPTED;

	args.agbno = XFS_FSB_TO_AGBNO(args.mp, bno);
	if (args.agbno >= args.mp->m_sb.sb_agblocks)
		return EFSCORRUPTED;

	args.pag = xfs_perag_get(args.mp, args.agno);
	ASSERT(args.pag);

	error = xfs_alloc_fix_freelist(&args, XFS_ALLOC_FLAG_FREEING);
	if (error)
		goto error0;

	
	if (args.agbno + len >
			be32_to_cpu(XFS_BUF_TO_AGF(args.agbp)->agf_length)) {
		error = EFSCORRUPTED;
		goto error0;
	}

	error = xfs_free_ag_extent(tp, args.agbp, args.agno, args.agbno, len, 0);
	if (!error)
		xfs_alloc_busy_insert(tp, args.agno, args.agbno, len, 0);
error0:
	xfs_perag_put(args.pag);
	return error;
}

void
xfs_alloc_busy_insert(
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	unsigned int		flags)
{
	struct xfs_busy_extent	*new;
	struct xfs_busy_extent	*busyp;
	struct xfs_perag	*pag;
	struct rb_node		**rbp;
	struct rb_node		*parent = NULL;

	new = kmem_zalloc(sizeof(struct xfs_busy_extent), KM_MAYFAIL);
	if (!new) {
		trace_xfs_alloc_busy_enomem(tp->t_mountp, agno, bno, len);
		xfs_trans_set_sync(tp);
		return;
	}

	new->agno = agno;
	new->bno = bno;
	new->length = len;
	INIT_LIST_HEAD(&new->list);
	new->flags = flags;

	
	trace_xfs_alloc_busy(tp->t_mountp, agno, bno, len);

	pag = xfs_perag_get(tp->t_mountp, new->agno);
	spin_lock(&pag->pagb_lock);
	rbp = &pag->pagb_tree.rb_node;
	while (*rbp) {
		parent = *rbp;
		busyp = rb_entry(parent, struct xfs_busy_extent, rb_node);

		if (new->bno < busyp->bno) {
			rbp = &(*rbp)->rb_left;
			ASSERT(new->bno + new->length <= busyp->bno);
		} else if (new->bno > busyp->bno) {
			rbp = &(*rbp)->rb_right;
			ASSERT(bno >= busyp->bno + busyp->length);
		} else {
			ASSERT(0);
		}
	}

	rb_link_node(&new->rb_node, parent, rbp);
	rb_insert_color(&new->rb_node, &pag->pagb_tree);

	list_add(&new->list, &tp->t_busy);
	spin_unlock(&pag->pagb_lock);
	xfs_perag_put(pag);
}

int
xfs_alloc_busy_search(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	xfs_extlen_t		len)
{
	struct xfs_perag	*pag;
	struct rb_node		*rbp;
	struct xfs_busy_extent	*busyp;
	int			match = 0;

	pag = xfs_perag_get(mp, agno);
	spin_lock(&pag->pagb_lock);

	rbp = pag->pagb_tree.rb_node;

	
	while (rbp) {
		busyp = rb_entry(rbp, struct xfs_busy_extent, rb_node);
		if (bno < busyp->bno) {
			
			if (bno + len > busyp->bno)
				match = -1;
			rbp = rbp->rb_left;
		} else if (bno > busyp->bno) {
			
			if (bno < busyp->bno + busyp->length)
				match = -1;
			rbp = rbp->rb_right;
		} else {
			
			match = (busyp->length == len) ? 1 : -1;
			break;
		}
	}
	spin_unlock(&pag->pagb_lock);
	xfs_perag_put(pag);
	return match;
}

STATIC bool
xfs_alloc_busy_update_extent(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	struct xfs_busy_extent	*busyp,
	xfs_agblock_t		fbno,
	xfs_extlen_t		flen,
	bool			userdata)
{
	xfs_agblock_t		fend = fbno + flen;
	xfs_agblock_t		bbno = busyp->bno;
	xfs_agblock_t		bend = bbno + busyp->length;

	if (busyp->flags & XFS_ALLOC_BUSY_DISCARDED) {
		spin_unlock(&pag->pagb_lock);
		delay(1);
		spin_lock(&pag->pagb_lock);
		return false;
	}

	if (userdata)
		goto out_force_log;

	if (bbno < fbno && bend > fend) {

		goto out_force_log;
	} else if (bbno >= fbno && bend <= fend) {

		rb_erase(&busyp->rb_node, &pag->pagb_tree);
		busyp->length = 0;
		return false;
	} else if (fend < bend) {
		busyp->bno = fend;
	} else if (bbno < fbno) {
		busyp->length = fbno - busyp->bno;
	} else {
		ASSERT(0);
	}

	trace_xfs_alloc_busy_reuse(mp, pag->pag_agno, fbno, flen);
	return true;

out_force_log:
	spin_unlock(&pag->pagb_lock);
	xfs_log_force(mp, XFS_LOG_SYNC);
	trace_xfs_alloc_busy_force(mp, pag->pag_agno, fbno, flen);
	spin_lock(&pag->pagb_lock);
	return false;
}


void
xfs_alloc_busy_reuse(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		fbno,
	xfs_extlen_t		flen,
	bool			userdata)
{
	struct xfs_perag	*pag;
	struct rb_node		*rbp;

	ASSERT(flen > 0);

	pag = xfs_perag_get(mp, agno);
	spin_lock(&pag->pagb_lock);
restart:
	rbp = pag->pagb_tree.rb_node;
	while (rbp) {
		struct xfs_busy_extent *busyp =
			rb_entry(rbp, struct xfs_busy_extent, rb_node);
		xfs_agblock_t	bbno = busyp->bno;
		xfs_agblock_t	bend = bbno + busyp->length;

		if (fbno + flen <= bbno) {
			rbp = rbp->rb_left;
			continue;
		} else if (fbno >= bend) {
			rbp = rbp->rb_right;
			continue;
		}

		if (!xfs_alloc_busy_update_extent(mp, pag, busyp, fbno, flen,
						  userdata))
			goto restart;
	}
	spin_unlock(&pag->pagb_lock);
	xfs_perag_put(pag);
}

STATIC void
xfs_alloc_busy_trim(
	struct xfs_alloc_arg	*args,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	xfs_agblock_t		*rbno,
	xfs_extlen_t		*rlen)
{
	xfs_agblock_t		fbno;
	xfs_extlen_t		flen;
	struct rb_node		*rbp;

	ASSERT(len > 0);

	spin_lock(&args->pag->pagb_lock);
restart:
	fbno = bno;
	flen = len;
	rbp = args->pag->pagb_tree.rb_node;
	while (rbp && flen >= args->minlen) {
		struct xfs_busy_extent *busyp =
			rb_entry(rbp, struct xfs_busy_extent, rb_node);
		xfs_agblock_t	fend = fbno + flen;
		xfs_agblock_t	bbno = busyp->bno;
		xfs_agblock_t	bend = bbno + busyp->length;

		if (fend <= bbno) {
			rbp = rbp->rb_left;
			continue;
		} else if (fbno >= bend) {
			rbp = rbp->rb_right;
			continue;
		}

		if (!args->userdata &&
		    !(busyp->flags & XFS_ALLOC_BUSY_DISCARDED)) {
			if (!xfs_alloc_busy_update_extent(args->mp, args->pag,
							  busyp, fbno, flen,
							  false))
				goto restart;
			continue;
		}

		if (bbno <= fbno) {
			

			if (fend <= bend)
				goto fail;

			fbno = bend;
		} else if (bend >= fend) {
			

			fend = bbno;
		} else {
			

			if (bbno - fbno >= args->maxlen) {
				
				fend = bbno;
			} else if (fend - bend >= args->maxlen * 4) {
				
				fbno = bend;
			} else if (bbno - fbno >= args->minlen) {
				
				fend = bbno;
			} else {
				goto fail;
			}
		}

		flen = fend - fbno;
	}
	spin_unlock(&args->pag->pagb_lock);

	if (fbno != bno || flen != len) {
		trace_xfs_alloc_busy_trim(args->mp, args->agno, bno, len,
					  fbno, flen);
	}
	*rbno = fbno;
	*rlen = flen;
	return;
fail:
	spin_unlock(&args->pag->pagb_lock);
	trace_xfs_alloc_busy_trim(args->mp, args->agno, bno, len, fbno, 0);
	*rbno = fbno;
	*rlen = 0;
}

static void
xfs_alloc_busy_clear_one(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	struct xfs_busy_extent	*busyp)
{
	if (busyp->length) {
		trace_xfs_alloc_busy_clear(mp, busyp->agno, busyp->bno,
						busyp->length);
		rb_erase(&busyp->rb_node, &pag->pagb_tree);
	}

	list_del_init(&busyp->list);
	kmem_free(busyp);
}

void
xfs_alloc_busy_clear(
	struct xfs_mount	*mp,
	struct list_head	*list,
	bool			do_discard)
{
	struct xfs_busy_extent	*busyp, *n;
	struct xfs_perag	*pag = NULL;
	xfs_agnumber_t		agno = NULLAGNUMBER;

	list_for_each_entry_safe(busyp, n, list, list) {
		if (busyp->agno != agno) {
			if (pag) {
				spin_unlock(&pag->pagb_lock);
				xfs_perag_put(pag);
			}
			pag = xfs_perag_get(mp, busyp->agno);
			spin_lock(&pag->pagb_lock);
			agno = busyp->agno;
		}

		if (do_discard && busyp->length &&
		    !(busyp->flags & XFS_ALLOC_BUSY_SKIP_DISCARD))
			busyp->flags = XFS_ALLOC_BUSY_DISCARDED;
		else
			xfs_alloc_busy_clear_one(mp, pag, busyp);
	}

	if (pag) {
		spin_unlock(&pag->pagb_lock);
		xfs_perag_put(pag);
	}
}

int
xfs_busy_extent_ag_cmp(
	void			*priv,
	struct list_head	*a,
	struct list_head	*b)
{
	return container_of(a, struct xfs_busy_extent, list)->agno -
		container_of(b, struct xfs_busy_extent, list)->agno;
}
