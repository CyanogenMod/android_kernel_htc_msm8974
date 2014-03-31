/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_fsops.h"
#include "xfs_error.h"
#include "xfs_rw.h"
#include "xfs_inode_item.h"
#include "xfs_trans_space.h"
#include "xfs_utils.h"
#include "xfs_trace.h"
#include "xfs_buf.h"




STATIC int xfs_rtallocate_range(xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
		xfs_extlen_t, xfs_buf_t **, xfs_fsblock_t *);
STATIC int xfs_rtany_summary(xfs_mount_t *, xfs_trans_t *, int, int,
		xfs_rtblock_t, xfs_buf_t **, xfs_fsblock_t *, int *);
STATIC int xfs_rtcheck_range(xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
		xfs_extlen_t, int, xfs_rtblock_t *, int *);
STATIC int xfs_rtfind_back(xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
		xfs_rtblock_t, xfs_rtblock_t *);
STATIC int xfs_rtfind_forw(xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
		xfs_rtblock_t, xfs_rtblock_t *);
STATIC int xfs_rtget_summary( xfs_mount_t *, xfs_trans_t *, int,
		xfs_rtblock_t, xfs_buf_t **, xfs_fsblock_t *, xfs_suminfo_t *);
STATIC int xfs_rtmodify_range(xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
		xfs_extlen_t, int);
STATIC int xfs_rtmodify_summary(xfs_mount_t *, xfs_trans_t *, int,
		xfs_rtblock_t, int, xfs_buf_t **, xfs_fsblock_t *);


STATIC int				
xfs_growfs_rt_alloc(
	xfs_mount_t	*mp,		
	xfs_extlen_t	oblocks,	
	xfs_extlen_t	nblocks,	
	xfs_inode_t	*ip)		
{
	xfs_fileoff_t	bno;		
	xfs_buf_t	*bp;		
	int		committed;	
	xfs_daddr_t	d;		
	int		error;		
	xfs_fsblock_t	firstblock;	
	xfs_bmap_free_t	flist;		
	xfs_fsblock_t	fsbno;		
	xfs_bmbt_irec_t	map;		
	int		nmap;		
	int		resblks;	

	while (oblocks < nblocks) {
		int		cancelflags = 0;
		xfs_trans_t	*tp;

		tp = xfs_trans_alloc(mp, XFS_TRANS_GROWFSRT_ALLOC);
		resblks = XFS_GROWFSRT_SPACE_RES(mp, nblocks - oblocks);
		if ((error = xfs_trans_reserve(tp, resblks,
				XFS_GROWRTALLOC_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES,
				XFS_DEFAULT_PERM_LOG_COUNT)))
			goto error_cancel;
		cancelflags = XFS_TRANS_RELEASE_LOG_RES;
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

		xfs_bmap_init(&flist, &firstblock);
		nmap = 1;
		cancelflags |= XFS_TRANS_ABORT;
		error = xfs_bmapi_write(tp, ip, oblocks, nblocks - oblocks,
					XFS_BMAPI_METADATA, &firstblock,
					resblks, &map, &nmap, &flist);
		if (!error && nmap < 1)
			error = XFS_ERROR(ENOSPC);
		if (error)
			goto error_cancel;
		error = xfs_bmap_finish(&tp, &flist, &committed);
		if (error)
			goto error_cancel;
		error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES);
		if (error)
			goto error;
		cancelflags = 0;
		for (bno = map.br_startoff, fsbno = map.br_startblock;
		     bno < map.br_startoff + map.br_blockcount;
		     bno++, fsbno++) {
			tp = xfs_trans_alloc(mp, XFS_TRANS_GROWFSRT_ZERO);
			if ((error = xfs_trans_reserve(tp, 0,
					XFS_GROWRTZERO_LOG_RES(mp), 0, 0, 0)))
				goto error_cancel;
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			d = XFS_FSB_TO_DADDR(mp, fsbno);
			bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d,
				mp->m_bsize, 0);
			if (bp == NULL) {
				error = XFS_ERROR(EIO);
error_cancel:
				xfs_trans_cancel(tp, cancelflags);
				goto error;
			}
			memset(bp->b_addr, 0, mp->m_sb.sb_blocksize);
			xfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);
			error = xfs_trans_commit(tp, 0);
			if (error)
				goto error;
		}
		oblocks = map.br_startoff + map.br_blockcount;
	}
	return 0;

error:
	return error;
}

STATIC int				
xfs_rtallocate_extent_block(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bbno,		
	xfs_extlen_t	minlen,		
	xfs_extlen_t	maxlen,		
	xfs_extlen_t	*len,		
	xfs_rtblock_t	*nextp,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	xfs_extlen_t	prod,		
	xfs_rtblock_t	*rtblock)	
{
	xfs_rtblock_t	besti;		
	xfs_rtblock_t	bestlen;	
	xfs_rtblock_t	end;		
	int		error;		
	xfs_rtblock_t	i;		
	xfs_rtblock_t	next;		
	int		stat;		

	for (i = XFS_BLOCKTOBIT(mp, bbno), besti = -1, bestlen = 0,
		end = XFS_BLOCKTOBIT(mp, bbno + 1) - 1;
	     i <= end;
	     i++) {
		error = xfs_rtcheck_range(mp, tp, i, maxlen, 1, &next, &stat);
		if (error) {
			return error;
		}
		if (stat) {
			error = xfs_rtallocate_range(mp, tp, i, maxlen, rbpp,
				rsb);
			if (error) {
				return error;
			}
			*len = maxlen;
			*rtblock = i;
			return 0;
		}
		if (minlen < maxlen) {
			xfs_rtblock_t	thislen;	

			thislen = next - i;
			if (thislen >= minlen && thislen > bestlen) {
				besti = i;
				bestlen = thislen;
			}
		}
		if (next < end) {
			error = xfs_rtfind_forw(mp, tp, next, end, &i);
			if (error) {
				return error;
			}
		} else
			break;
	}
	if (minlen < maxlen && besti != -1) {
		xfs_extlen_t	p;	

		if (prod > 1 && (p = do_mod(bestlen, prod)))
			bestlen -= p;
		error = xfs_rtallocate_range(mp, tp, besti, bestlen, rbpp, rsb);
		if (error) {
			return error;
		}
		*len = bestlen;
		*rtblock = besti;
		return 0;
	}
	*nextp = next;
	*rtblock = NULLRTBLOCK;
	return 0;
}

STATIC int				
xfs_rtallocate_extent_exact(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bno,		
	xfs_extlen_t	minlen,		
	xfs_extlen_t	maxlen,		
	xfs_extlen_t	*len,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	xfs_extlen_t	prod,		
	xfs_rtblock_t	*rtblock)	
{
	int		error;		
	xfs_extlen_t	i;		
	int		isfree;		
	xfs_rtblock_t	next;		

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	error = xfs_rtcheck_range(mp, tp, bno, maxlen, 1, &next, &isfree);
	if (error) {
		return error;
	}
	if (isfree) {
		error = xfs_rtallocate_range(mp, tp, bno, maxlen, rbpp, rsb);
		if (error) {
			return error;
		}
		*len = maxlen;
		*rtblock = bno;
		return 0;
	}
	maxlen = next - bno;
	if (maxlen < minlen) {
		*rtblock = NULLRTBLOCK;
		return 0;
	}
	if (prod > 1 && (i = maxlen % prod)) {
		maxlen -= i;
		if (maxlen < minlen) {
			*rtblock = NULLRTBLOCK;
			return 0;
		}
	}
	error = xfs_rtallocate_range(mp, tp, bno, maxlen, rbpp, rsb);
	if (error) {
		return error;
	}
	*len = maxlen;
	*rtblock = bno;
	return 0;
}

STATIC int				
xfs_rtallocate_extent_near(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bno,		
	xfs_extlen_t	minlen,		
	xfs_extlen_t	maxlen,		
	xfs_extlen_t	*len,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	xfs_extlen_t	prod,		
	xfs_rtblock_t	*rtblock)	
{
	int		any;		
	xfs_rtblock_t	bbno;		
	int		error;		
	int		i;		
	int		j;		
	int		log2len;	
	xfs_rtblock_t	n;		
	xfs_rtblock_t	r;		

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	if (bno >= mp->m_sb.sb_rextents)
		bno = mp->m_sb.sb_rextents - 1;
	error = xfs_rtallocate_extent_exact(mp, tp, bno, minlen, maxlen, len,
		rbpp, rsb, prod, &r);
	if (error) {
		return error;
	}
	if (r != NULLRTBLOCK) {
		*rtblock = r;
		return 0;
	}
	bbno = XFS_BITTOBLOCK(mp, bno);
	i = 0;
	ASSERT(minlen != 0);
	log2len = xfs_highbit32(minlen);
	for (;;) {
		error = xfs_rtany_summary(mp, tp, log2len, mp->m_rsumlevels - 1,
			bbno + i, rbpp, rsb, &any);
		if (error) {
			return error;
		}
		if (any) {
			if (i >= 0) {
				error = xfs_rtallocate_extent_block(mp, tp,
					bbno + i, minlen, maxlen, len, &n, rbpp,
					rsb, prod, &r);
				if (error) {
					return error;
				}
				if (r != NULLRTBLOCK) {
					*rtblock = r;
					return 0;
				}
			}
			else {		
				for (j = -1; j > i; j--) {
					error = xfs_rtany_summary(mp, tp,
						log2len, mp->m_rsumlevels - 1,
						bbno + j, rbpp, rsb, &any);
					if (error) {
						return error;
					}
					if (any)
						continue;
					error = xfs_rtallocate_extent_block(mp,
						tp, bbno + j, minlen, maxlen,
						len, &n, rbpp, rsb, prod, &r);
					if (error) {
						return error;
					}
					if (r != NULLRTBLOCK) {
						*rtblock = r;
						return 0;
					}
				}
				error = xfs_rtallocate_extent_block(mp, tp,
					bbno + i, minlen, maxlen, len, &n, rbpp,
					rsb, prod, &r);
				if (error) {
					return error;
				}
				if (r != NULLRTBLOCK) {
					*rtblock = r;
					return 0;
				}
			}
		}
		if (i > 0 && (int)bbno - i >= 0)
			i = -i;
		else if (i > 0 && (int)bbno + i < mp->m_sb.sb_rbmblocks - 1)
			i++;
		else if (i <= 0 && (int)bbno - i < mp->m_sb.sb_rbmblocks - 1)
			i = 1 - i;
		else if (i <= 0 && (int)bbno + i > 0)
			i--;
		else
			break;
	}
	*rtblock = NULLRTBLOCK;
	return 0;
}

STATIC int				
xfs_rtallocate_extent_size(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_extlen_t	minlen,		
	xfs_extlen_t	maxlen,		
	xfs_extlen_t	*len,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	xfs_extlen_t	prod,		
	xfs_rtblock_t	*rtblock)	
{
	int		error;		
	int		i;		
	int		l;		
	xfs_rtblock_t	n;		
	xfs_rtblock_t	r;		
	xfs_suminfo_t	sum;		

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	ASSERT(maxlen != 0);

	for (l = xfs_highbit32(maxlen); l < mp->m_rsumlevels; l++) {
		for (i = 0; i < mp->m_sb.sb_rbmblocks; i++) {
			error = xfs_rtget_summary(mp, tp, l, i, rbpp, rsb,
				&sum);
			if (error) {
				return error;
			}
			if (!sum)
				continue;
			error = xfs_rtallocate_extent_block(mp, tp, i, maxlen,
				maxlen, len, &n, rbpp, rsb, prod, &r);
			if (error) {
				return error;
			}
			if (r != NULLRTBLOCK) {
				*rtblock = r;
				return 0;
			}
			if (XFS_BITTOBLOCK(mp, n) > i + 1)
				i = XFS_BITTOBLOCK(mp, n) - 1;
		}
	}
	if (minlen > --maxlen) {
		*rtblock = NULLRTBLOCK;
		return 0;
	}
	ASSERT(minlen != 0);
	ASSERT(maxlen != 0);

	for (l = xfs_highbit32(maxlen); l >= xfs_highbit32(minlen); l--) {
		for (i = 0; i < mp->m_sb.sb_rbmblocks; i++) {
			error =	xfs_rtget_summary(mp, tp, l, i, rbpp, rsb,
						  &sum);
			if (error) {
				return error;
			}
			if (!sum)
				continue;
			error = xfs_rtallocate_extent_block(mp, tp, i,
					XFS_RTMAX(minlen, 1 << l),
					XFS_RTMIN(maxlen, (1 << (l + 1)) - 1),
					len, &n, rbpp, rsb, prod, &r);
			if (error) {
				return error;
			}
			if (r != NULLRTBLOCK) {
				*rtblock = r;
				return 0;
			}
			if (XFS_BITTOBLOCK(mp, n) > i + 1)
				i = XFS_BITTOBLOCK(mp, n) - 1;
		}
	}
	*rtblock = NULLRTBLOCK;
	return 0;
}

STATIC int				
xfs_rtallocate_range(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_extlen_t	len,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb)		
{
	xfs_rtblock_t	end;		
	int		error;		
	xfs_rtblock_t	postblock;	
	xfs_rtblock_t	preblock;	

	end = start + len - 1;
	error = xfs_rtfind_back(mp, tp, start, 0, &preblock);
	if (error) {
		return error;
	}
	error = xfs_rtfind_forw(mp, tp, end, mp->m_sb.sb_rextents - 1,
		&postblock);
	if (error) {
		return error;
	}
	error = xfs_rtmodify_summary(mp, tp,
		XFS_RTBLOCKLOG(postblock + 1 - preblock),
		XFS_BITTOBLOCK(mp, preblock), -1, rbpp, rsb);
	if (error) {
		return error;
	}
	if (preblock < start) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(start - preblock),
			XFS_BITTOBLOCK(mp, preblock), 1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	if (postblock > end) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(postblock - end),
			XFS_BITTOBLOCK(mp, end + 1), 1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	error = xfs_rtmodify_range(mp, tp, start, len, 0);
	return error;
}

STATIC int				
xfs_rtany_summary(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	int		low,		
	int		high,		
	xfs_rtblock_t	bbno,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	int		*stat)		
{
	int		error;		
	int		log;		
	xfs_suminfo_t	sum;		

	for (log = low; log <= high; log++) {
		error = xfs_rtget_summary(mp, tp, log, bbno, rbpp, rsb, &sum);
		if (error) {
			return error;
		}
		if (sum) {
			*stat = 1;
			return 0;
		}
	}
	*stat = 0;
	return 0;
}

STATIC int				
xfs_rtbuf_get(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	block,		
	int		issum,		
	xfs_buf_t	**bpp)		
{
	xfs_buf_t	*bp;		
	xfs_inode_t	*ip;		
	xfs_bmbt_irec_t	map;
	int		nmap;
	int		error;		

	ip = issum ? mp->m_rsumip : mp->m_rbmip;

	error = xfs_bmapi_read(ip, block, 1, &map, &nmap, XFS_DATA_FORK);
	if (error)
		return error;

	ASSERT(map.br_startblock != NULLFSBLOCK);
	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
				   XFS_FSB_TO_DADDR(mp, map.br_startblock),
				   mp->m_bsize, 0, &bp);
	if (error)
		return error;
	ASSERT(!xfs_buf_geterror(bp));
	*bpp = bp;
	return 0;
}

#ifdef DEBUG
STATIC int				
xfs_rtcheck_alloc_range(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bno,		
	xfs_extlen_t	len,		
	int		*stat)		
{
	xfs_rtblock_t	new;		

	return xfs_rtcheck_range(mp, tp, bno, len, 0, &new, stat);
}
#endif

STATIC int				
xfs_rtcheck_range(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_extlen_t	len,		
	int		val,		
	xfs_rtblock_t	*new,		
	int		*stat)		
{
	xfs_rtword_t	*b;		
	int		bit;		
	xfs_rtblock_t	block;		
	xfs_buf_t	*bp;		
	xfs_rtword_t	*bufp;		
	int		error;		
	xfs_rtblock_t	i;		
	xfs_rtblock_t	lastbit;	
	xfs_rtword_t	mask;		
	xfs_rtword_t	wdiff;		
	int		word;		

	block = XFS_BITTOBLOCK(mp, start);
	error = xfs_rtbuf_get(mp, tp, block, 0, &bp);
	if (error) {
		return error;
	}
	bufp = bp->b_addr;
	word = XFS_BITTOWORD(mp, start);
	b = &bufp[word];
	bit = (int)(start & (XFS_NBWORD - 1));
	val = -val;
	if (bit) {
		lastbit = XFS_RTMIN(bit + len, XFS_NBWORD);
		mask = (((xfs_rtword_t)1 << (lastbit - bit)) - 1) << bit;
		if ((wdiff = (*b ^ val) & mask)) {
			xfs_trans_brelse(tp, bp);
			i = XFS_RTLOBIT(wdiff) - bit;
			*new = start + i;
			*stat = 0;
			return 0;
		}
		i = lastbit - bit;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	} else {
		i = 0;
	}
	while (len - i >= XFS_NBWORD) {
		if ((wdiff = *b ^ val)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_RTLOBIT(wdiff);
			*new = start + i;
			*stat = 0;
			return 0;
		}
		i += XFS_NBWORD;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	}
	if ((lastbit = len - i)) {
		mask = ((xfs_rtword_t)1 << lastbit) - 1;
		if ((wdiff = (*b ^ val) & mask)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_RTLOBIT(wdiff);
			*new = start + i;
			*stat = 0;
			return 0;
		} else
			i = len;
	}
	xfs_trans_brelse(tp, bp);
	*new = start + i;
	*stat = 1;
	return 0;
}

STATIC int				
xfs_rtcopy_summary(
	xfs_mount_t	*omp,		
	xfs_mount_t	*nmp,		
	xfs_trans_t	*tp)		
{
	xfs_rtblock_t	bbno;		
	xfs_buf_t	*bp;		
	int		error;		
	int		log;		
	xfs_suminfo_t	sum;		
	xfs_fsblock_t	sumbno;		

	bp = NULL;
	for (log = omp->m_rsumlevels - 1; log >= 0; log--) {
		for (bbno = omp->m_sb.sb_rbmblocks - 1;
		     (xfs_srtblock_t)bbno >= 0;
		     bbno--) {
			error = xfs_rtget_summary(omp, tp, log, bbno, &bp,
				&sumbno, &sum);
			if (error)
				return error;
			if (sum == 0)
				continue;
			error = xfs_rtmodify_summary(omp, tp, log, bbno, -sum,
				&bp, &sumbno);
			if (error)
				return error;
			error = xfs_rtmodify_summary(nmp, tp, log, bbno, sum,
				&bp, &sumbno);
			if (error)
				return error;
			ASSERT(sum > 0);
		}
	}
	return 0;
}

STATIC int				
xfs_rtfind_back(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_rtblock_t	limit,		
	xfs_rtblock_t	*rtblock)	
{
	xfs_rtword_t	*b;		
	int		bit;		
	xfs_rtblock_t	block;		
	xfs_buf_t	*bp;		
	xfs_rtword_t	*bufp;		
	int		error;		
	xfs_rtblock_t	firstbit;	
	xfs_rtblock_t	i;		
	xfs_rtblock_t	len;		
	xfs_rtword_t	mask;		
	xfs_rtword_t	want;		
	xfs_rtword_t	wdiff;		
	int		word;		

	block = XFS_BITTOBLOCK(mp, start);
	error = xfs_rtbuf_get(mp, tp, block, 0, &bp);
	if (error) {
		return error;
	}
	bufp = bp->b_addr;
	word = XFS_BITTOWORD(mp, start);
	b = &bufp[word];
	bit = (int)(start & (XFS_NBWORD - 1));
	len = start - limit + 1;
	want = (*b & ((xfs_rtword_t)1 << bit)) ? -1 : 0;
	if (bit < XFS_NBWORD - 1) {
		firstbit = XFS_RTMAX((xfs_srtblock_t)(bit - len + 1), 0);
		mask = (((xfs_rtword_t)1 << (bit - firstbit + 1)) - 1) <<
			firstbit;
		if ((wdiff = (*b ^ want) & mask)) {
			xfs_trans_brelse(tp, bp);
			i = bit - XFS_RTHIBIT(wdiff);
			*rtblock = start - i + 1;
			return 0;
		}
		i = bit - firstbit + 1;
		if (--word == -1 && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, --block, 0, &bp);
			if (error) {
				return error;
			}
			bufp = bp->b_addr;
			word = XFS_BLOCKWMASK(mp);
			b = &bufp[word];
		} else {
			b--;
		}
	} else {
		i = 0;
	}
	while (len - i >= XFS_NBWORD) {
		if ((wdiff = *b ^ want)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_NBWORD - 1 - XFS_RTHIBIT(wdiff);
			*rtblock = start - i + 1;
			return 0;
		}
		i += XFS_NBWORD;
		if (--word == -1 && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, --block, 0, &bp);
			if (error) {
				return error;
			}
			bufp = bp->b_addr;
			word = XFS_BLOCKWMASK(mp);
			b = &bufp[word];
		} else {
			b--;
		}
	}
	if (len - i) {
		firstbit = XFS_NBWORD - (len - i);
		mask = (((xfs_rtword_t)1 << (len - i)) - 1) << firstbit;
		if ((wdiff = (*b ^ want) & mask)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_NBWORD - 1 - XFS_RTHIBIT(wdiff);
			*rtblock = start - i + 1;
			return 0;
		} else
			i = len;
	}
	xfs_trans_brelse(tp, bp);
	*rtblock = start - i + 1;
	return 0;
}

STATIC int				
xfs_rtfind_forw(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_rtblock_t	limit,		
	xfs_rtblock_t	*rtblock)	
{
	xfs_rtword_t	*b;		
	int		bit;		
	xfs_rtblock_t	block;		
	xfs_buf_t	*bp;		
	xfs_rtword_t	*bufp;		
	int		error;		
	xfs_rtblock_t	i;		
	xfs_rtblock_t	lastbit;	
	xfs_rtblock_t	len;		
	xfs_rtword_t	mask;		
	xfs_rtword_t	want;		
	xfs_rtword_t	wdiff;		
	int		word;		

	block = XFS_BITTOBLOCK(mp, start);
	error = xfs_rtbuf_get(mp, tp, block, 0, &bp);
	if (error) {
		return error;
	}
	bufp = bp->b_addr;
	word = XFS_BITTOWORD(mp, start);
	b = &bufp[word];
	bit = (int)(start & (XFS_NBWORD - 1));
	len = limit - start + 1;
	want = (*b & ((xfs_rtword_t)1 << bit)) ? -1 : 0;
	if (bit) {
		lastbit = XFS_RTMIN(bit + len, XFS_NBWORD);
		mask = (((xfs_rtword_t)1 << (lastbit - bit)) - 1) << bit;
		if ((wdiff = (*b ^ want) & mask)) {
			xfs_trans_brelse(tp, bp);
			i = XFS_RTLOBIT(wdiff) - bit;
			*rtblock = start + i - 1;
			return 0;
		}
		i = lastbit - bit;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	} else {
		i = 0;
	}
	while (len - i >= XFS_NBWORD) {
		if ((wdiff = *b ^ want)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_RTLOBIT(wdiff);
			*rtblock = start + i - 1;
			return 0;
		}
		i += XFS_NBWORD;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_brelse(tp, bp);
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	}
	if ((lastbit = len - i)) {
		mask = ((xfs_rtword_t)1 << lastbit) - 1;
		if ((wdiff = (*b ^ want) & mask)) {
			xfs_trans_brelse(tp, bp);
			i += XFS_RTLOBIT(wdiff);
			*rtblock = start + i - 1;
			return 0;
		} else
			i = len;
	}
	xfs_trans_brelse(tp, bp);
	*rtblock = start + i - 1;
	return 0;
}

STATIC int				
xfs_rtfree_range(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_extlen_t	len,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb)		
{
	xfs_rtblock_t	end;		
	int		error;		
	xfs_rtblock_t	postblock;	
	xfs_rtblock_t	preblock;	

	end = start + len - 1;
	error = xfs_rtmodify_range(mp, tp, start, len, 1);
	if (error) {
		return error;
	}
	error = xfs_rtfind_back(mp, tp, start, 0, &preblock);
	if (error) {
		return error;
	}
	error = xfs_rtfind_forw(mp, tp, end, mp->m_sb.sb_rextents - 1,
		&postblock);
	if (error)
		return error;
	if (preblock < start) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(start - preblock),
			XFS_BITTOBLOCK(mp, preblock), -1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	if (postblock > end) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(postblock - end),
			XFS_BITTOBLOCK(mp, end + 1), -1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	error = xfs_rtmodify_summary(mp, tp,
		XFS_RTBLOCKLOG(postblock + 1 - preblock),
		XFS_BITTOBLOCK(mp, preblock), 1, rbpp, rsb);
	return error;
}

STATIC int				
xfs_rtget_summary(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	int		log,		
	xfs_rtblock_t	bbno,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb,		
	xfs_suminfo_t	*sum)		
{
	xfs_buf_t	*bp;		
	int		error;		
	xfs_fsblock_t	sb;		
	int		so;		
	xfs_suminfo_t	*sp;		

	so = XFS_SUMOFFS(mp, log, bbno);
	sb = XFS_SUMOFFSTOBLOCK(mp, so);
	if (rbpp && *rbpp && *rsb == sb)
		bp = *rbpp;
	else {
		if (rbpp && *rbpp)
			xfs_trans_brelse(tp, *rbpp);
		error = xfs_rtbuf_get(mp, tp, sb, 1, &bp);
		if (error) {
			return error;
		}
		if (rbpp) {
			*rbpp = bp;
			*rsb = sb;
		}
	}
	sp = XFS_SUMPTR(mp, bp, so);
	*sum = *sp;
	if (!rbpp)
		xfs_trans_brelse(tp, bp);
	return 0;
}

STATIC int				
xfs_rtmodify_range(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_rtblock_t	start,		
	xfs_extlen_t	len,		
	int		val)		
{
	xfs_rtword_t	*b;		
	int		bit;		
	xfs_rtblock_t	block;		
	xfs_buf_t	*bp;		
	xfs_rtword_t	*bufp;		
	int		error;		
	xfs_rtword_t	*first;		
	int		i;		
	int		lastbit;	
	xfs_rtword_t	mask;		
	int		word;		

	block = XFS_BITTOBLOCK(mp, start);
	error = xfs_rtbuf_get(mp, tp, block, 0, &bp);
	if (error) {
		return error;
	}
	bufp = bp->b_addr;
	word = XFS_BITTOWORD(mp, start);
	first = b = &bufp[word];
	bit = (int)(start & (XFS_NBWORD - 1));
	val = -val;
	if (bit) {
		lastbit = XFS_RTMIN(bit + len, XFS_NBWORD);
		mask = (((xfs_rtword_t)1 << (lastbit - bit)) - 1) << bit;
		if (val)
			*b |= mask;
		else
			*b &= ~mask;
		i = lastbit - bit;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_log_buf(tp, bp,
				(uint)((char *)first - (char *)bufp),
				(uint)((char *)b - (char *)bufp));
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			first = b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	} else {
		i = 0;
	}
	while (len - i >= XFS_NBWORD) {
		*b = val;
		i += XFS_NBWORD;
		if (++word == XFS_BLOCKWSIZE(mp) && i < len) {
			xfs_trans_log_buf(tp, bp,
				(uint)((char *)first - (char *)bufp),
				(uint)((char *)b - (char *)bufp));
			error = xfs_rtbuf_get(mp, tp, ++block, 0, &bp);
			if (error) {
				return error;
			}
			first = b = bufp = bp->b_addr;
			word = 0;
		} else {
			b++;
		}
	}
	if ((lastbit = len - i)) {
		bit = 0;
		mask = ((xfs_rtword_t)1 << lastbit) - 1;
		if (val)
			*b |= mask;
		else
			*b &= ~mask;
		b++;
	}
	if (b > first)
		xfs_trans_log_buf(tp, bp, (uint)((char *)first - (char *)bufp),
			(uint)((char *)b - (char *)bufp - 1));
	return 0;
}

STATIC int				
xfs_rtmodify_summary(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	int		log,		
	xfs_rtblock_t	bbno,		
	int		delta,		
	xfs_buf_t	**rbpp,		
	xfs_fsblock_t	*rsb)		
{
	xfs_buf_t	*bp;		
	int		error;		
	xfs_fsblock_t	sb;		
	int		so;		
	xfs_suminfo_t	*sp;		

	so = XFS_SUMOFFS(mp, log, bbno);
	sb = XFS_SUMOFFSTOBLOCK(mp, so);
	if (rbpp && *rbpp && *rsb == sb)
		bp = *rbpp;
	else {
		if (rbpp && *rbpp)
			xfs_trans_brelse(tp, *rbpp);
		error = xfs_rtbuf_get(mp, tp, sb, 1, &bp);
		if (error) {
			return error;
		}
		if (rbpp) {
			*rbpp = bp;
			*rsb = sb;
		}
	}
	sp = XFS_SUMPTR(mp, bp, so);
	*sp += delta;
	xfs_trans_log_buf(tp, bp, (uint)((char *)sp - (char *)bp->b_addr),
		(uint)((char *)sp - (char *)bp->b_addr + sizeof(*sp) - 1));
	return 0;
}


int
xfs_growfs_rt(
	xfs_mount_t	*mp,		
	xfs_growfs_rt_t	*in)		
{
	xfs_rtblock_t	bmbno;		
	xfs_buf_t	*bp;		
	int		error;		
	xfs_mount_t	*nmp;		
	xfs_drfsbno_t	nrblocks;	
	xfs_extlen_t	nrbmblocks;	
	xfs_drtbno_t	nrextents;	
	uint8_t		nrextslog;	
	xfs_extlen_t	nrsumblocks;	
	uint		nrsumlevels;	
	uint		nrsumsize;	
	xfs_sb_t	*nsbp;		
	xfs_extlen_t	rbmblocks;	
	xfs_extlen_t	rsumblocks;	
	xfs_sb_t	*sbp;		
	xfs_fsblock_t	sumbno;		

	sbp = &mp->m_sb;
	if (!capable(CAP_SYS_ADMIN))
		return XFS_ERROR(EPERM);
	if (mp->m_rtdev_targp == NULL || mp->m_rbmip == NULL ||
	    (nrblocks = in->newblocks) <= sbp->sb_rblocks ||
	    (sbp->sb_rblocks && (in->extsize != sbp->sb_rextsize)))
		return XFS_ERROR(EINVAL);
	if ((error = xfs_sb_validate_fsb_count(sbp, nrblocks)))
		return error;
	bp = xfs_buf_read_uncached(mp, mp->m_rtdev_targp,
				XFS_FSB_TO_BB(mp, nrblocks - 1),
				XFS_FSB_TO_B(mp, 1), 0);
	if (!bp)
		return EIO;
	xfs_buf_relse(bp);

	nrextents = nrblocks;
	do_div(nrextents, in->extsize);
	nrbmblocks = howmany_64(nrextents, NBBY * sbp->sb_blocksize);
	nrextslog = xfs_highbit32(nrextents);
	nrsumlevels = nrextslog + 1;
	nrsumsize = (uint)sizeof(xfs_suminfo_t) * nrsumlevels * nrbmblocks;
	nrsumblocks = XFS_B_TO_FSB(mp, nrsumsize);
	nrsumsize = XFS_FSB_TO_B(mp, nrsumblocks);
	if (nrsumblocks > (mp->m_sb.sb_logblocks >> 1))
		return XFS_ERROR(EINVAL);
	rbmblocks = XFS_B_TO_FSB(mp, mp->m_rbmip->i_d.di_size);
	rsumblocks = XFS_B_TO_FSB(mp, mp->m_rsumip->i_d.di_size);
	error = xfs_growfs_rt_alloc(mp, rbmblocks, nrbmblocks, mp->m_rbmip);
	if (error)
		return error;
	error = xfs_growfs_rt_alloc(mp, rsumblocks, nrsumblocks, mp->m_rsumip);
	if (error)
		return error;
	nmp = kmem_alloc(sizeof(*nmp), KM_SLEEP);
	for (bmbno = sbp->sb_rbmblocks -
		     ((sbp->sb_rextents & ((1 << mp->m_blkbit_log) - 1)) != 0);
	     bmbno < nrbmblocks;
	     bmbno++) {
		xfs_trans_t	*tp;
		int		cancelflags = 0;

		*nmp = *mp;
		nsbp = &nmp->m_sb;
		nsbp->sb_rextsize = in->extsize;
		nsbp->sb_rbmblocks = bmbno + 1;
		nsbp->sb_rblocks =
			XFS_RTMIN(nrblocks,
				  nsbp->sb_rbmblocks * NBBY *
				  nsbp->sb_blocksize * nsbp->sb_rextsize);
		nsbp->sb_rextents = nsbp->sb_rblocks;
		do_div(nsbp->sb_rextents, nsbp->sb_rextsize);
		ASSERT(nsbp->sb_rextents != 0);
		nsbp->sb_rextslog = xfs_highbit32(nsbp->sb_rextents);
		nrsumlevels = nmp->m_rsumlevels = nsbp->sb_rextslog + 1;
		nrsumsize =
			(uint)sizeof(xfs_suminfo_t) * nrsumlevels *
			nsbp->sb_rbmblocks;
		nrsumblocks = XFS_B_TO_FSB(mp, nrsumsize);
		nmp->m_rsumsize = nrsumsize = XFS_FSB_TO_B(mp, nrsumblocks);
		tp = xfs_trans_alloc(mp, XFS_TRANS_GROWFSRT_FREE);
		if ((error = xfs_trans_reserve(tp, 0,
				XFS_GROWRTFREE_LOG_RES(nmp), 0, 0, 0)))
			goto error_cancel;
		xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, mp->m_rbmip, XFS_ILOCK_EXCL);
		mp->m_rbmip->i_d.di_size =
			nsbp->sb_rbmblocks * nsbp->sb_blocksize;
		xfs_trans_log_inode(tp, mp->m_rbmip, XFS_ILOG_CORE);
		cancelflags |= XFS_TRANS_ABORT;
		xfs_ilock(mp->m_rsumip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, mp->m_rsumip, XFS_ILOCK_EXCL);
		mp->m_rsumip->i_d.di_size = nmp->m_rsumsize;
		xfs_trans_log_inode(tp, mp->m_rsumip, XFS_ILOG_CORE);
		if (sbp->sb_rbmblocks != nsbp->sb_rbmblocks ||
		    mp->m_rsumlevels != nmp->m_rsumlevels) {
			error = xfs_rtcopy_summary(mp, nmp, tp);
			if (error)
				goto error_cancel;
		}
		if (nsbp->sb_rextsize != sbp->sb_rextsize)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTSIZE,
				nsbp->sb_rextsize - sbp->sb_rextsize);
		if (nsbp->sb_rbmblocks != sbp->sb_rbmblocks)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RBMBLOCKS,
				nsbp->sb_rbmblocks - sbp->sb_rbmblocks);
		if (nsbp->sb_rblocks != sbp->sb_rblocks)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RBLOCKS,
				nsbp->sb_rblocks - sbp->sb_rblocks);
		if (nsbp->sb_rextents != sbp->sb_rextents)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTENTS,
				nsbp->sb_rextents - sbp->sb_rextents);
		if (nsbp->sb_rextslog != sbp->sb_rextslog)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTSLOG,
				nsbp->sb_rextslog - sbp->sb_rextslog);
		bp = NULL;
		error = xfs_rtfree_range(nmp, tp, sbp->sb_rextents,
			nsbp->sb_rextents - sbp->sb_rextents, &bp, &sumbno);
		if (error) {
error_cancel:
			xfs_trans_cancel(tp, cancelflags);
			break;
		}
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FREXTENTS,
			nsbp->sb_rextents - sbp->sb_rextents);
		mp->m_rsumlevels = nrsumlevels;
		mp->m_rsumsize = nrsumsize;

		error = xfs_trans_commit(tp, 0);
		if (error)
			break;
	}

	kmem_free(nmp);

	return error;
}

int					
xfs_rtallocate_extent(
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bno,		
	xfs_extlen_t	minlen,		
	xfs_extlen_t	maxlen,		
	xfs_extlen_t	*len,		
	xfs_alloctype_t	type,		
	int		wasdel,		
	xfs_extlen_t	prod,		
	xfs_rtblock_t	*rtblock)	
{
	xfs_mount_t	*mp = tp->t_mountp;
	int		error;		
	xfs_rtblock_t	r;		
	xfs_fsblock_t	sb;		
	xfs_buf_t	*sumbp;		

	ASSERT(xfs_isilocked(mp->m_rbmip, XFS_ILOCK_EXCL));
	ASSERT(minlen > 0 && minlen <= maxlen);

	if (prod > 1) {
		xfs_extlen_t	i;

		if ((i = maxlen % prod))
			maxlen -= i;
		if ((i = minlen % prod))
			minlen += prod - i;
		if (maxlen < minlen) {
			*rtblock = NULLRTBLOCK;
			return 0;
		}
	}

	sumbp = NULL;
	switch (type) {
	case XFS_ALLOCTYPE_ANY_AG:
		error = xfs_rtallocate_extent_size(mp, tp, minlen, maxlen, len,
				&sumbp,	&sb, prod, &r);
		break;
	case XFS_ALLOCTYPE_NEAR_BNO:
		error = xfs_rtallocate_extent_near(mp, tp, bno, minlen, maxlen,
				len, &sumbp, &sb, prod, &r);
		break;
	case XFS_ALLOCTYPE_THIS_BNO:
		error = xfs_rtallocate_extent_exact(mp, tp, bno, minlen, maxlen,
				len, &sumbp, &sb, prod, &r);
		break;
	default:
		error = EIO;
		ASSERT(0);
	}
	if (error)
		return error;

	if (r != NULLRTBLOCK) {
		long	slen = (long)*len;

		ASSERT(*len >= minlen && *len <= maxlen);
		if (wasdel)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RES_FREXTENTS, -slen);
		else
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_FREXTENTS, -slen);
	}
	*rtblock = r;
	return 0;
}

int					
xfs_rtfree_extent(
	xfs_trans_t	*tp,		
	xfs_rtblock_t	bno,		
	xfs_extlen_t	len)		
{
	int		error;		
	xfs_mount_t	*mp;		
	xfs_fsblock_t	sb;		
	xfs_buf_t	*sumbp;		

	mp = tp->t_mountp;

	ASSERT(mp->m_rbmip->i_itemp != NULL);
	ASSERT(xfs_isilocked(mp->m_rbmip, XFS_ILOCK_EXCL));

#if defined(__KERNEL__) && defined(DEBUG)
	{
		int	stat;		

		error = xfs_rtcheck_alloc_range(mp, tp, bno, len, &stat);
		if (error) {
			return error;
		}
		ASSERT(stat);
	}
#endif
	sumbp = NULL;
	error = xfs_rtfree_range(mp, tp, bno, len, &sumbp, &sb);
	if (error) {
		return error;
	}
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_FREXTENTS, (long)len);
	if (tp->t_frextents_delta + mp->m_sb.sb_frextents ==
	    mp->m_sb.sb_rextents) {
		if (!(mp->m_rbmip->i_d.di_flags & XFS_DIFLAG_NEWRTBM))
			mp->m_rbmip->i_d.di_flags |= XFS_DIFLAG_NEWRTBM;
		*(__uint64_t *)&mp->m_rbmip->i_d.di_atime = 0;
		xfs_trans_log_inode(tp, mp->m_rbmip, XFS_ILOG_CORE);
	}
	return 0;
}

int				
xfs_rtmount_init(
	xfs_mount_t	*mp)	
{
	xfs_buf_t	*bp;	
	xfs_daddr_t	d;	
	xfs_sb_t	*sbp;	

	sbp = &mp->m_sb;
	if (sbp->sb_rblocks == 0)
		return 0;
	if (mp->m_rtdev_targp == NULL) {
		xfs_warn(mp,
	"Filesystem has a realtime volume, use rtdev=device option");
		return XFS_ERROR(ENODEV);
	}
	mp->m_rsumlevels = sbp->sb_rextslog + 1;
	mp->m_rsumsize =
		(uint)sizeof(xfs_suminfo_t) * mp->m_rsumlevels *
		sbp->sb_rbmblocks;
	mp->m_rsumsize = roundup(mp->m_rsumsize, sbp->sb_blocksize);
	mp->m_rbmip = mp->m_rsumip = NULL;
	d = (xfs_daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_rblocks);
	if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_rblocks) {
		xfs_warn(mp, "realtime mount -- %llu != %llu",
			(unsigned long long) XFS_BB_TO_FSB(mp, d),
			(unsigned long long) mp->m_sb.sb_rblocks);
		return XFS_ERROR(EFBIG);
	}
	bp = xfs_buf_read_uncached(mp, mp->m_rtdev_targp,
					d - XFS_FSB_TO_BB(mp, 1),
					XFS_FSB_TO_B(mp, 1), 0);
	if (!bp) {
		xfs_warn(mp, "realtime device size check failed");
		return EIO;
	}
	xfs_buf_relse(bp);
	return 0;
}

int					
xfs_rtmount_inodes(
	xfs_mount_t	*mp)		
{
	int		error;		
	xfs_sb_t	*sbp;

	sbp = &mp->m_sb;
	if (sbp->sb_rbmino == NULLFSINO)
		return 0;
	error = xfs_iget(mp, NULL, sbp->sb_rbmino, 0, 0, &mp->m_rbmip);
	if (error)
		return error;
	ASSERT(mp->m_rbmip != NULL);
	ASSERT(sbp->sb_rsumino != NULLFSINO);
	error = xfs_iget(mp, NULL, sbp->sb_rsumino, 0, 0, &mp->m_rsumip);
	if (error) {
		IRELE(mp->m_rbmip);
		return error;
	}
	ASSERT(mp->m_rsumip != NULL);
	return 0;
}

void
xfs_rtunmount_inodes(
	struct xfs_mount	*mp)
{
	if (mp->m_rbmip)
		IRELE(mp->m_rbmip);
	if (mp->m_rsumip)
		IRELE(mp->m_rsumip);
}

int					
xfs_rtpick_extent(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_extlen_t	len,		
	xfs_rtblock_t	*pick)		
{
	xfs_rtblock_t	b;		
	int		log2;		
	__uint64_t	resid;		
	__uint64_t	seq;		
	__uint64_t	*seqp;		

	ASSERT(xfs_isilocked(mp->m_rbmip, XFS_ILOCK_EXCL));

	seqp = (__uint64_t *)&mp->m_rbmip->i_d.di_atime;
	if (!(mp->m_rbmip->i_d.di_flags & XFS_DIFLAG_NEWRTBM)) {
		mp->m_rbmip->i_d.di_flags |= XFS_DIFLAG_NEWRTBM;
		*seqp = 0;
	}
	seq = *seqp;
	if ((log2 = xfs_highbit64(seq)) == -1)
		b = 0;
	else {
		resid = seq - (1ULL << log2);
		b = (mp->m_sb.sb_rextents * ((resid << 1) + 1ULL)) >>
		    (log2 + 1);
		if (b >= mp->m_sb.sb_rextents)
			b = do_mod(b, mp->m_sb.sb_rextents);
		if (b + len > mp->m_sb.sb_rextents)
			b = mp->m_sb.sb_rextents - len;
	}
	*seqp = seq + 1;
	xfs_trans_log_inode(tp, mp->m_rbmip, XFS_ILOG_CORE);
	*pick = b;
	return 0;
}
