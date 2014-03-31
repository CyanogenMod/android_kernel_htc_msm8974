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
#include "xfs_ialloc.h"
#include "xfs_itable.h"
#include "xfs_error.h"
#include "xfs_btree.h"
#include "xfs_trace.h"

STATIC int
xfs_internal_inum(
	xfs_mount_t	*mp,
	xfs_ino_t	ino)
{
	return (ino == mp->m_sb.sb_rbmino || ino == mp->m_sb.sb_rsumino ||
		(xfs_sb_version_hasquota(&mp->m_sb) &&
		 (ino == mp->m_sb.sb_uquotino || ino == mp->m_sb.sb_gquotino)));
}

int
xfs_bulkstat_one_int(
	struct xfs_mount	*mp,		
	xfs_ino_t		ino,		
	void __user		*buffer,	
	int			ubsize,		
	bulkstat_one_fmt_pf	formatter,	
	int			*ubused,	
	int			*stat)		
{
	struct xfs_icdinode	*dic;		
	struct xfs_inode	*ip;		
	struct xfs_bstat	*buf;		
	int			error = 0;	

	*stat = BULKSTAT_RV_NOTHING;

	if (!buffer || xfs_internal_inum(mp, ino))
		return XFS_ERROR(EINVAL);

	buf = kmem_alloc(sizeof(*buf), KM_SLEEP | KM_MAYFAIL);
	if (!buf)
		return XFS_ERROR(ENOMEM);

	error = xfs_iget(mp, NULL, ino,
			 (XFS_IGET_DONTCACHE | XFS_IGET_UNTRUSTED),
			 XFS_ILOCK_SHARED, &ip);
	if (error) {
		*stat = BULKSTAT_RV_NOTHING;
		goto out_free;
	}

	ASSERT(ip != NULL);
	ASSERT(ip->i_imap.im_blkno != 0);

	dic = &ip->i_d;

	buf->bs_nlink = dic->di_nlink;
	buf->bs_projid_lo = dic->di_projid_lo;
	buf->bs_projid_hi = dic->di_projid_hi;
	buf->bs_ino = ino;
	buf->bs_mode = dic->di_mode;
	buf->bs_uid = dic->di_uid;
	buf->bs_gid = dic->di_gid;
	buf->bs_size = dic->di_size;
	buf->bs_atime.tv_sec = dic->di_atime.t_sec;
	buf->bs_atime.tv_nsec = dic->di_atime.t_nsec;
	buf->bs_mtime.tv_sec = dic->di_mtime.t_sec;
	buf->bs_mtime.tv_nsec = dic->di_mtime.t_nsec;
	buf->bs_ctime.tv_sec = dic->di_ctime.t_sec;
	buf->bs_ctime.tv_nsec = dic->di_ctime.t_nsec;
	buf->bs_xflags = xfs_ip2xflags(ip);
	buf->bs_extsize = dic->di_extsize << mp->m_sb.sb_blocklog;
	buf->bs_extents = dic->di_nextents;
	buf->bs_gen = dic->di_gen;
	memset(buf->bs_pad, 0, sizeof(buf->bs_pad));
	buf->bs_dmevmask = dic->di_dmevmask;
	buf->bs_dmstate = dic->di_dmstate;
	buf->bs_aextents = dic->di_anextents;
	buf->bs_forkoff = XFS_IFORK_BOFF(ip);

	switch (dic->di_format) {
	case XFS_DINODE_FMT_DEV:
		buf->bs_rdev = ip->i_df.if_u2.if_rdev;
		buf->bs_blksize = BLKDEV_IOSIZE;
		buf->bs_blocks = 0;
		break;
	case XFS_DINODE_FMT_LOCAL:
	case XFS_DINODE_FMT_UUID:
		buf->bs_rdev = 0;
		buf->bs_blksize = mp->m_sb.sb_blocksize;
		buf->bs_blocks = 0;
		break;
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		buf->bs_rdev = 0;
		buf->bs_blksize = mp->m_sb.sb_blocksize;
		buf->bs_blocks = dic->di_nblocks + ip->i_delayed_blks;
		break;
	}
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	IRELE(ip);

	error = formatter(buffer, ubsize, ubused, buf);

	if (!error)
		*stat = BULKSTAT_RV_DIDONE;

 out_free:
	kmem_free(buf);
	return error;
}

STATIC int
xfs_bulkstat_one_fmt(
	void			__user *ubuffer,
	int			ubsize,
	int			*ubused,
	const xfs_bstat_t	*buffer)
{
	if (ubsize < sizeof(*buffer))
		return XFS_ERROR(ENOMEM);
	if (copy_to_user(ubuffer, buffer, sizeof(*buffer)))
		return XFS_ERROR(EFAULT);
	if (ubused)
		*ubused = sizeof(*buffer);
	return 0;
}

int
xfs_bulkstat_one(
	xfs_mount_t	*mp,		
	xfs_ino_t	ino,		
	void		__user *buffer,	
	int		ubsize,		
	int		*ubused,	
	int		*stat)		
{
	return xfs_bulkstat_one_int(mp, ino, buffer, ubsize,
				    xfs_bulkstat_one_fmt, ubused, stat);
}

#define XFS_BULKSTAT_UBLEFT(ubleft)	((ubleft) >= statstruct_size)

int					
xfs_bulkstat(
	xfs_mount_t		*mp,	
	xfs_ino_t		*lastinop, 
	int			*ubcountp, 
	bulkstat_one_pf		formatter, 
	size_t			statstruct_size, 
	char			__user *ubuffer, 
	int			*done)	
{
	xfs_agblock_t		agbno=0;
	xfs_buf_t		*agbp;	
	xfs_agi_t		*agi;	
	xfs_agino_t		agino;	
	xfs_agnumber_t		agno;	
	int			chunkidx; 
	int			clustidx; 
	xfs_btree_cur_t		*cur;	
	int			end_of_ag; 
	int			error;	
	int                     fmterror;
	int			i;	
	int			icount;	
	size_t			irbsize; 
	xfs_ino_t		ino;	
	xfs_inobt_rec_incore_t	*irbp;	
	xfs_inobt_rec_incore_t	*irbuf;	
	xfs_inobt_rec_incore_t	*irbufend; 
	xfs_ino_t		lastino; 
	int			nbcluster; 
	int			nicluster; 
	int			nimask;	
	int			nirbuf;	
	int			rval;	
	int			tmp;	
	int			ubcount; 
	int			ubleft;	
	char			__user *ubufp;	
	int			ubelem;	
	int			ubused;	
	xfs_buf_t		*bp;	

	ino = (xfs_ino_t)*lastinop;
	lastino = ino;
	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	if (agno >= mp->m_sb.sb_agcount ||
	    ino != XFS_AGINO_TO_INO(mp, agno, agino)) {
		*done = 1;
		*ubcountp = 0;
		return 0;
	}
	if (!ubcountp || *ubcountp <= 0) {
		return EINVAL;
	}
	ubcount = *ubcountp; 
	ubleft = ubcount * statstruct_size; 
	*ubcountp = ubelem = 0;
	*done = 0;
	fmterror = 0;
	ubufp = ubuffer;
	nicluster = mp->m_sb.sb_blocksize >= XFS_INODE_CLUSTER_SIZE(mp) ?
		mp->m_sb.sb_inopblock :
		(XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog);
	nimask = ~(nicluster - 1);
	nbcluster = nicluster >> mp->m_sb.sb_inopblog;
	irbuf = kmem_zalloc_greedy(&irbsize, PAGE_SIZE, PAGE_SIZE * 4);
	if (!irbuf)
		return ENOMEM;

	nirbuf = irbsize / sizeof(*irbuf);

	rval = 0;
	while (XFS_BULKSTAT_UBLEFT(ubleft) && agno < mp->m_sb.sb_agcount) {
		cond_resched();
		bp = NULL;
		error = xfs_ialloc_read_agi(mp, NULL, agno, &agbp);
		if (error) {
			agno++;
			agino = 0;
			continue;
		}
		agi = XFS_BUF_TO_AGI(agbp);
		cur = xfs_inobt_init_cursor(mp, NULL, agbp, agno);
		irbp = irbuf;
		irbufend = irbuf + nirbuf;
		end_of_ag = 0;
		if (agino > 0) {
			xfs_inobt_rec_incore_t r;

			error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE,
						 &tmp);
			if (!error &&	
			    tmp &&	
					
			    !(error = xfs_inobt_get_rec(cur, &r, &i)) &&
			    i == 1 &&
					
			    agino < r.ir_startino + XFS_INODES_PER_CHUNK &&
					
			    (chunkidx = agino - r.ir_startino + 1) <
				    XFS_INODES_PER_CHUNK &&
					
			    xfs_inobt_maskn(chunkidx,
				    XFS_INODES_PER_CHUNK - chunkidx) &
				    ~r.ir_free) {
				for (i = 0; i < chunkidx; i++) {
					if (XFS_INOBT_MASK(i) & ~r.ir_free)
						r.ir_freecount++;
				}
				r.ir_free |= xfs_inobt_maskn(0, chunkidx);
				irbp->ir_startino = r.ir_startino;
				irbp->ir_freecount = r.ir_freecount;
				irbp->ir_free = r.ir_free;
				irbp++;
				agino = r.ir_startino + XFS_INODES_PER_CHUNK;
				icount = XFS_INODES_PER_CHUNK - r.ir_freecount;
			} else {
				agino++;
				icount = 0;
			}
			if (!error)
				error = xfs_btree_increment(cur, 0, &tmp);
		} else {
			error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &tmp);
			icount = 0;
		}
		while (irbp < irbufend && icount < ubcount) {
			xfs_inobt_rec_incore_t r;

			while (error) {
				agino += XFS_INODES_PER_CHUNK;
				if (XFS_AGINO_TO_AGBNO(mp, agino) >=
						be32_to_cpu(agi->agi_length))
					break;
				error = xfs_inobt_lookup(cur, agino,
							 XFS_LOOKUP_GE, &tmp);
				cond_resched();
			}
			if (error) {
				end_of_ag = 1;
				break;
			}

			error = xfs_inobt_get_rec(cur, &r, &i);
			if (error || i == 0) {
				end_of_ag = 1;
				break;
			}

			if (r.ir_freecount < XFS_INODES_PER_CHUNK) {
				agbno = XFS_AGINO_TO_AGBNO(mp, r.ir_startino);
				for (chunkidx = 0;
				     chunkidx < XFS_INODES_PER_CHUNK;
				     chunkidx += nicluster,
				     agbno += nbcluster) {
					if (xfs_inobt_maskn(chunkidx, nicluster)
							& ~r.ir_free)
						xfs_btree_reada_bufs(mp, agno,
							agbno, nbcluster);
				}
				irbp->ir_startino = r.ir_startino;
				irbp->ir_freecount = r.ir_freecount;
				irbp->ir_free = r.ir_free;
				irbp++;
				icount += XFS_INODES_PER_CHUNK - r.ir_freecount;
			}
			agino = r.ir_startino + XFS_INODES_PER_CHUNK;
			error = xfs_btree_increment(cur, 0, &tmp);
			cond_resched();
		}
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
		xfs_buf_relse(agbp);
		irbufend = irbp;
		for (irbp = irbuf;
		     irbp < irbufend && XFS_BULKSTAT_UBLEFT(ubleft); irbp++) {
			for (agino = irbp->ir_startino, chunkidx = clustidx = 0;
			     XFS_BULKSTAT_UBLEFT(ubleft) &&
				irbp->ir_freecount < XFS_INODES_PER_CHUNK;
			     chunkidx++, clustidx++, agino++) {
				ASSERT(chunkidx < XFS_INODES_PER_CHUNK);
				if ((chunkidx & (nicluster - 1)) == 0) {
					agbno = XFS_AGINO_TO_AGBNO(mp,
							irbp->ir_startino) +
						((chunkidx & nimask) >>
						 mp->m_sb.sb_inopblog);
				}
				ino = XFS_AGINO_TO_INO(mp, agno, agino);
				if (XFS_INOBT_MASK(chunkidx) & irbp->ir_free) {
					lastino = ino;
					continue;
				}
				irbp->ir_freecount++;

				ubused = statstruct_size;
				error = formatter(mp, ino, ubufp, ubleft,
						  &ubused, &fmterror);
				if (fmterror == BULKSTAT_RV_NOTHING) {
					if (error && error != ENOENT &&
						error != EINVAL) {
						ubleft = 0;
						rval = error;
						break;
					}
					lastino = ino;
					continue;
				}
				if (fmterror == BULKSTAT_RV_GIVEUP) {
					ubleft = 0;
					ASSERT(error);
					rval = error;
					break;
				}
				if (ubufp)
					ubufp += ubused;
				ubleft -= ubused;
				ubelem++;
				lastino = ino;
			}

			cond_resched();
		}

		if (bp)
			xfs_buf_relse(bp);

		if (XFS_BULKSTAT_UBLEFT(ubleft)) {
			if (end_of_ag) {
				agno++;
				agino = 0;
			} else
				agino = XFS_INO_TO_AGINO(mp, lastino);
		} else
			break;
	}
	kmem_free_large(irbuf);
	*ubcountp = ubelem;
	if (ubelem)
		rval = 0;
	if (agno >= mp->m_sb.sb_agcount) {
		*lastinop = (xfs_ino_t)XFS_AGINO_TO_INO(mp, agno, 0);
		*done = 1;
	} else
		*lastinop = (xfs_ino_t)lastino;

	return rval;
}

int					
xfs_bulkstat_single(
	xfs_mount_t		*mp,	
	xfs_ino_t		*lastinop, 
	char			__user *buffer, 
	int			*done)	
{
	int			count;	
	int			error;	
	xfs_ino_t		ino;	
	int			res;	


	ino = (xfs_ino_t)*lastinop;
	error = xfs_bulkstat_one(mp, ino, buffer, sizeof(xfs_bstat_t), 0, &res);
	if (error) {
		(*lastinop)--;
		count = 1;
		if (xfs_bulkstat(mp, lastinop, &count, xfs_bulkstat_one,
				sizeof(xfs_bstat_t), buffer, done))
			return error;
		if (count == 0 || (xfs_ino_t)*lastinop != ino)
			return error == EFSCORRUPTED ?
				XFS_ERROR(EINVAL) : error;
		else
			return 0;
	}
	*done = 0;
	return 0;
}

int
xfs_inumbers_fmt(
	void			__user *ubuffer, 
	const xfs_inogrp_t	*buffer,	
	long			count,		
	long			*written)	/* # of bytes written */
{
	if (copy_to_user(ubuffer, buffer, count * sizeof(*buffer)))
		return -EFAULT;
	*written = count * sizeof(*buffer);
	return 0;
}

int					
xfs_inumbers(
	xfs_mount_t	*mp,		
	xfs_ino_t	*lastino,	
	int		*count,		
	void		__user *ubuffer,
	inumbers_fmt_pf	formatter)
{
	xfs_buf_t	*agbp;
	xfs_agino_t	agino;
	xfs_agnumber_t	agno;
	int		bcount;
	xfs_inogrp_t	*buffer;
	int		bufidx;
	xfs_btree_cur_t	*cur;
	int		error;
	xfs_inobt_rec_incore_t r;
	int		i;
	xfs_ino_t	ino;
	int		left;
	int		tmp;

	ino = (xfs_ino_t)*lastino;
	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	left = *count;
	*count = 0;
	bcount = MIN(left, (int)(PAGE_SIZE / sizeof(*buffer)));
	buffer = kmem_alloc(bcount * sizeof(*buffer), KM_SLEEP);
	error = bufidx = 0;
	cur = NULL;
	agbp = NULL;
	while (left > 0 && agno < mp->m_sb.sb_agcount) {
		if (agbp == NULL) {
			error = xfs_ialloc_read_agi(mp, NULL, agno, &agbp);
			if (error) {
				ASSERT(cur == NULL);
				agbp = NULL;
				agno++;
				agino = 0;
				continue;
			}
			cur = xfs_inobt_init_cursor(mp, NULL, agbp, agno);
			error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_GE,
						 &tmp);
			if (error) {
				xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
				cur = NULL;
				xfs_buf_relse(agbp);
				agbp = NULL;
				agino += XFS_INODES_PER_CHUNK - 1;
				continue;
			}
		}
		error = xfs_inobt_get_rec(cur, &r, &i);
		if (error || i == 0) {
			xfs_buf_relse(agbp);
			agbp = NULL;
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			cur = NULL;
			agno++;
			agino = 0;
			continue;
		}
		agino = r.ir_startino + XFS_INODES_PER_CHUNK - 1;
		buffer[bufidx].xi_startino =
			XFS_AGINO_TO_INO(mp, agno, r.ir_startino);
		buffer[bufidx].xi_alloccount =
			XFS_INODES_PER_CHUNK - r.ir_freecount;
		buffer[bufidx].xi_allocmask = ~r.ir_free;
		bufidx++;
		left--;
		if (bufidx == bcount) {
			long written;
			if (formatter(ubuffer, buffer, bufidx, &written)) {
				error = XFS_ERROR(EFAULT);
				break;
			}
			ubuffer += written;
			*count += bufidx;
			bufidx = 0;
		}
		if (left) {
			error = xfs_btree_increment(cur, 0, &tmp);
			if (error) {
				xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
				cur = NULL;
				xfs_buf_relse(agbp);
				agbp = NULL;
				agino += XFS_INODES_PER_CHUNK;
				continue;
			}
		}
	}
	if (!error) {
		if (bufidx) {
			long written;
			if (formatter(ubuffer, buffer, bufidx, &written))
				error = XFS_ERROR(EFAULT);
			else
				*count += bufidx;
		}
		*lastino = XFS_AGINO_TO_INO(mp, agno, agino);
	}
	kmem_free(buffer);
	if (cur)
		xfs_btree_del_cursor(cur, (error ? XFS_BTREE_ERROR :
					   XFS_BTREE_NOERROR));
	if (agbp)
		xfs_buf_relse(agbp);
	return error;
}
