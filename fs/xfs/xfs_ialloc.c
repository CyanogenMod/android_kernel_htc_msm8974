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
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_bmap.h"


static inline int
xfs_ialloc_cluster_alignment(
	xfs_alloc_arg_t	*args)
{
	if (xfs_sb_version_hasalign(&args->mp->m_sb) &&
	    args->mp->m_sb.sb_inoalignmt >=
	     XFS_B_TO_FSBT(args->mp, XFS_INODE_CLUSTER_SIZE(args->mp)))
		return args->mp->m_sb.sb_inoalignmt;
	return 1;
}

int					
xfs_inobt_lookup(
	struct xfs_btree_cur	*cur,	
	xfs_agino_t		ino,	
	xfs_lookup_t		dir,	
	int			*stat)	
{
	cur->bc_rec.i.ir_startino = ino;
	cur->bc_rec.i.ir_freecount = 0;
	cur->bc_rec.i.ir_free = 0;
	return xfs_btree_lookup(cur, dir, stat);
}

STATIC int				
xfs_inobt_update(
	struct xfs_btree_cur	*cur,	
	xfs_inobt_rec_incore_t	*irec)	
{
	union xfs_btree_rec	rec;

	rec.inobt.ir_startino = cpu_to_be32(irec->ir_startino);
	rec.inobt.ir_freecount = cpu_to_be32(irec->ir_freecount);
	rec.inobt.ir_free = cpu_to_be64(irec->ir_free);
	return xfs_btree_update(cur, &rec);
}

int					
xfs_inobt_get_rec(
	struct xfs_btree_cur	*cur,	
	xfs_inobt_rec_incore_t	*irec,	
	int			*stat)	
{
	union xfs_btree_rec	*rec;
	int			error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (!error && *stat == 1) {
		irec->ir_startino = be32_to_cpu(rec->inobt.ir_startino);
		irec->ir_freecount = be32_to_cpu(rec->inobt.ir_freecount);
		irec->ir_free = be64_to_cpu(rec->inobt.ir_free);
	}
	return error;
}

#ifdef DEBUG
STATIC int
xfs_check_agi_freecount(
	struct xfs_btree_cur	*cur,
	struct xfs_agi		*agi)
{
	if (cur->bc_nlevels == 1) {
		xfs_inobt_rec_incore_t rec;
		int		freecount = 0;
		int		error;
		int		i;

		error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &i);
		if (error)
			return error;

		do {
			error = xfs_inobt_get_rec(cur, &rec, &i);
			if (error)
				return error;

			if (i) {
				freecount += rec.ir_freecount;
				error = xfs_btree_increment(cur, 0, &i);
				if (error)
					return error;
			}
		} while (i == 1);

		if (!XFS_FORCED_SHUTDOWN(cur->bc_mp))
			ASSERT(freecount == be32_to_cpu(agi->agi_freecount));
	}
	return 0;
}
#else
#define xfs_check_agi_freecount(cur, agi)	0
#endif

STATIC int
xfs_ialloc_inode_init(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		agbno,
	xfs_agblock_t		length,
	unsigned int		gen)
{
	struct xfs_buf		*fbuf;
	struct xfs_dinode	*free;
	int			blks_per_cluster, nbufs, ninodes;
	int			version;
	int			i, j;
	xfs_daddr_t		d;

	if (mp->m_sb.sb_blocksize >= XFS_INODE_CLUSTER_SIZE(mp)) {
		blks_per_cluster = 1;
		nbufs = length;
		ninodes = mp->m_sb.sb_inopblock;
	} else {
		blks_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) /
				   mp->m_sb.sb_blocksize;
		nbufs = length / blks_per_cluster;
		ninodes = blks_per_cluster * mp->m_sb.sb_inopblock;
	}

	if (xfs_sb_version_hasnlink(&mp->m_sb))
		version = 2;
	else
		version = 1;

	for (j = 0; j < nbufs; j++) {
		d = XFS_AGB_TO_DADDR(mp, agno, agbno + (j * blks_per_cluster));
		fbuf = xfs_trans_get_buf(tp, mp->m_ddev_targp, d,
					 mp->m_bsize * blks_per_cluster,
					 XBF_LOCK);
		if (!fbuf)
			return ENOMEM;
		xfs_buf_zero(fbuf, 0, ninodes << mp->m_sb.sb_inodelog);
		for (i = 0; i < ninodes; i++) {
			int	ioffset = i << mp->m_sb.sb_inodelog;
			uint	isize = sizeof(struct xfs_dinode);

			free = xfs_make_iptr(mp, fbuf, i);
			free->di_magic = cpu_to_be16(XFS_DINODE_MAGIC);
			free->di_version = version;
			free->di_gen = cpu_to_be32(gen);
			free->di_next_unlinked = cpu_to_be32(NULLAGINO);
			xfs_trans_log_buf(tp, fbuf, ioffset, ioffset + isize - 1);
		}
		xfs_trans_inode_alloc_buf(tp, fbuf);
	}
	return 0;
}

STATIC int				
xfs_ialloc_ag_alloc(
	xfs_trans_t	*tp,		
	xfs_buf_t	*agbp,		
	int		*alloc)
{
	xfs_agi_t	*agi;		
	xfs_alloc_arg_t	args;		
	xfs_btree_cur_t	*cur;		
	xfs_agnumber_t	agno;
	int		error;
	int		i;
	xfs_agino_t	newino;		
	xfs_agino_t	newlen;		
	xfs_agino_t	thisino;	
	int		isaligned = 0;	
					
	struct xfs_perag *pag;

	args.tp = tp;
	args.mp = tp->t_mountp;

	newlen = XFS_IALLOC_INODES(args.mp);
	if (args.mp->m_maxicount &&
	    args.mp->m_sb.sb_icount + newlen > args.mp->m_maxicount)
		return XFS_ERROR(ENOSPC);
	args.minlen = args.maxlen = XFS_IALLOC_BLOCKS(args.mp);
	agi = XFS_BUF_TO_AGI(agbp);
	newino = be32_to_cpu(agi->agi_newino);
	agno = be32_to_cpu(agi->agi_seqno);
	args.agbno = XFS_AGINO_TO_AGBNO(args.mp, newino) +
			XFS_IALLOC_BLOCKS(args.mp);
	if (likely(newino != NULLAGINO &&
		  (args.agbno < be32_to_cpu(agi->agi_length)))) {
		args.fsbno = XFS_AGB_TO_FSB(args.mp, agno, args.agbno);
		args.type = XFS_ALLOCTYPE_THIS_BNO;
		args.mod = args.total = args.wasdel = args.isfl =
			args.userdata = args.minalignslop = 0;
		args.prod = 1;

		args.alignment = 1;
		args.minalignslop = xfs_ialloc_cluster_alignment(&args) - 1;

		
		args.minleft = args.mp->m_in_maxlevels - 1;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	} else
		args.fsbno = NULLFSBLOCK;

	if (unlikely(args.fsbno == NULLFSBLOCK)) {
		isaligned = 0;
		if (args.mp->m_sinoalign) {
			ASSERT(!(args.mp->m_flags & XFS_MOUNT_NOALIGN));
			args.alignment = args.mp->m_dalign;
			isaligned = 1;
		} else
			args.alignment = xfs_ialloc_cluster_alignment(&args);
		args.agbno = be32_to_cpu(agi->agi_root);
		args.fsbno = XFS_AGB_TO_FSB(args.mp, agno, args.agbno);
		args.type = XFS_ALLOCTYPE_NEAR_BNO;
		args.mod = args.total = args.wasdel = args.isfl =
			args.userdata = args.minalignslop = 0;
		args.prod = 1;
		args.minleft = args.mp->m_in_maxlevels - 1;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}

	if (isaligned && args.fsbno == NULLFSBLOCK) {
		args.type = XFS_ALLOCTYPE_NEAR_BNO;
		args.agbno = be32_to_cpu(agi->agi_root);
		args.fsbno = XFS_AGB_TO_FSB(args.mp, agno, args.agbno);
		args.alignment = xfs_ialloc_cluster_alignment(&args);
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}

	if (args.fsbno == NULLFSBLOCK) {
		*alloc = 0;
		return 0;
	}
	ASSERT(args.len == args.minlen);

	error = xfs_ialloc_inode_init(args.mp, tp, agno, args.agbno,
			args.len, random32());

	if (error)
		return error;
	newino = XFS_OFFBNO_TO_AGINO(args.mp, args.agbno, 0);
	be32_add_cpu(&agi->agi_count, newlen);
	be32_add_cpu(&agi->agi_freecount, newlen);
	pag = xfs_perag_get(args.mp, agno);
	pag->pagi_freecount += newlen;
	xfs_perag_put(pag);
	agi->agi_newino = cpu_to_be32(newino);

	cur = xfs_inobt_init_cursor(args.mp, tp, agbp, agno);
	for (thisino = newino;
	     thisino < newino + newlen;
	     thisino += XFS_INODES_PER_CHUNK) {
		cur->bc_rec.i.ir_startino = thisino;
		cur->bc_rec.i.ir_freecount = XFS_INODES_PER_CHUNK;
		cur->bc_rec.i.ir_free = XFS_INOBT_ALL_FREE;
		error = xfs_btree_lookup(cur, XFS_LOOKUP_EQ, &i);
		if (error) {
			xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
			return error;
		}
		ASSERT(i == 0);
		error = xfs_btree_insert(cur, &i);
		if (error) {
			xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
			return error;
		}
		ASSERT(i == 1);
	}
	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	xfs_ialloc_log_agi(tp, agbp,
		XFS_AGI_COUNT | XFS_AGI_FREECOUNT | XFS_AGI_NEWINO);
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_ICOUNT, (long)newlen);
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, (long)newlen);
	*alloc = 1;
	return 0;
}

STATIC xfs_agnumber_t
xfs_ialloc_next_ag(
	xfs_mount_t	*mp)
{
	xfs_agnumber_t	agno;

	spin_lock(&mp->m_agirotor_lock);
	agno = mp->m_agirotor;
	if (++mp->m_agirotor == mp->m_maxagi)
		mp->m_agirotor = 0;
	spin_unlock(&mp->m_agirotor_lock);

	return agno;
}

STATIC xfs_buf_t *			
xfs_ialloc_ag_select(
	xfs_trans_t	*tp,		
	xfs_ino_t	parent,		
	umode_t		mode,		
	int		okalloc)	
{
	xfs_buf_t	*agbp;		
	xfs_agnumber_t	agcount;	
	xfs_agnumber_t	agno;		
	int		flags;		
	xfs_extlen_t	ineed;		
	xfs_extlen_t	longest = 0;	
	xfs_mount_t	*mp;		
	int		needspace;	
	xfs_perag_t	*pag;		
	xfs_agnumber_t	pagno;		

	needspace = S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode);
	mp = tp->t_mountp;
	agcount = mp->m_maxagi;
	if (S_ISDIR(mode))
		pagno = xfs_ialloc_next_ag(mp);
	else {
		pagno = XFS_INO_TO_AGNO(mp, parent);
		if (pagno >= agcount)
			pagno = 0;
	}
	ASSERT(pagno < agcount);
	agno = pagno;
	flags = XFS_ALLOC_FLAG_TRYLOCK;
	for (;;) {
		pag = xfs_perag_get(mp, agno);
		if (!pag->pagi_init) {
			if (xfs_ialloc_read_agi(mp, tp, agno, &agbp)) {
				agbp = NULL;
				goto nextag;
			}
		} else
			agbp = NULL;

		if (!pag->pagi_inodeok) {
			xfs_ialloc_next_ag(mp);
			goto unlock_nextag;
		}

		ineed = pag->pagi_freecount ? 0 : XFS_IALLOC_BLOCKS(mp);
		if (ineed && !pag->pagf_init) {
			if (agbp == NULL &&
			    xfs_ialloc_read_agi(mp, tp, agno, &agbp)) {
				agbp = NULL;
				goto nextag;
			}
			(void)xfs_alloc_pagf_init(mp, tp, agno, flags);
		}
		if (!ineed || pag->pagf_init) {
			if (ineed && !(longest = pag->pagf_longest))
				longest = pag->pagf_flcount > 0;
			if (!ineed ||
			    (pag->pagf_freeblks >= needspace + ineed &&
			     longest >= ineed &&
			     okalloc)) {
				if (agbp == NULL &&
				    xfs_ialloc_read_agi(mp, tp, agno, &agbp)) {
					agbp = NULL;
					goto nextag;
				}
				xfs_perag_put(pag);
				return agbp;
			}
		}
unlock_nextag:
		if (agbp)
			xfs_trans_brelse(tp, agbp);
nextag:
		xfs_perag_put(pag);
		if (XFS_FORCED_SHUTDOWN(mp))
			return NULL;
		agno++;
		if (agno >= agcount)
			agno = 0;
		if (agno == pagno) {
			if (flags == 0)
				return NULL;
			flags = 0;
		}
	}
}

STATIC int
xfs_ialloc_next_rec(
	struct xfs_btree_cur	*cur,
	xfs_inobt_rec_incore_t	*rec,
	int			*done,
	int			left)
{
	int                     error;
	int			i;

	if (left)
		error = xfs_btree_decrement(cur, 0, &i);
	else
		error = xfs_btree_increment(cur, 0, &i);

	if (error)
		return error;
	*done = !i;
	if (i) {
		error = xfs_inobt_get_rec(cur, rec, &i);
		if (error)
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}

	return 0;
}

STATIC int
xfs_ialloc_get_rec(
	struct xfs_btree_cur	*cur,
	xfs_agino_t		agino,
	xfs_inobt_rec_incore_t	*rec,
	int			*done,
	int			left)
{
	int                     error;
	int			i;

	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_EQ, &i);
	if (error)
		return error;
	*done = !i;
	if (i) {
		error = xfs_inobt_get_rec(cur, rec, &i);
		if (error)
			return error;
		XFS_WANT_CORRUPTED_RETURN(i == 1);
	}

	return 0;
}


int
xfs_dialloc(
	xfs_trans_t	*tp,		
	xfs_ino_t	parent,		
	umode_t		mode,		
	int		okalloc,	
	xfs_buf_t	**IO_agbp,	
	boolean_t	*alloc_done,	
	xfs_ino_t	*inop)		
{
	xfs_agnumber_t	agcount;	
	xfs_buf_t	*agbp;		
	xfs_agnumber_t	agno;		
	xfs_agi_t	*agi;		
	xfs_btree_cur_t	*cur;		
	int		error;		
	int		i;		
	int		ialloced;	
	int		noroom = 0;	
	xfs_ino_t	ino;		
	
	int		j;		
	xfs_mount_t	*mp;		
	int		offset;		
	xfs_agino_t	pagino;		
	xfs_agnumber_t	pagno;		
	xfs_inobt_rec_incore_t rec;	
	xfs_agnumber_t	tagno;		
	xfs_btree_cur_t	*tcur;		
	xfs_inobt_rec_incore_t trec;	
	struct xfs_perag *pag;


	if (*IO_agbp == NULL) {
		agbp = xfs_ialloc_ag_select(tp, parent, mode, okalloc);
		if (!agbp) {
			*inop = NULLFSINO;
			return 0;
		}
		agi = XFS_BUF_TO_AGI(agbp);
		ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
	} else {
		agbp = *IO_agbp;
		agi = XFS_BUF_TO_AGI(agbp);
		ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
		ASSERT(be32_to_cpu(agi->agi_freecount) > 0);
	}
	mp = tp->t_mountp;
	agcount = mp->m_sb.sb_agcount;
	agno = be32_to_cpu(agi->agi_seqno);
	tagno = agno;
	pagno = XFS_INO_TO_AGNO(mp, parent);
	pagino = XFS_INO_TO_AGINO(mp, parent);


	if (mp->m_maxicount &&
	    mp->m_sb.sb_icount + XFS_IALLOC_INODES(mp) > mp->m_maxicount) {
		noroom = 1;
		okalloc = 0;
	}

	*alloc_done = B_FALSE;
	while (!agi->agi_freecount) {
		if (okalloc) {
			if ((error = xfs_ialloc_ag_alloc(tp, agbp, &ialloced))) {
				xfs_trans_brelse(tp, agbp);
				if (error == ENOSPC) {
					*inop = NULLFSINO;
					return 0;
				} else
					return error;
			}
			if (ialloced) {
				ASSERT(be32_to_cpu(agi->agi_freecount) > 0);
				*alloc_done = B_TRUE;
				*IO_agbp = agbp;
				*inop = NULLFSINO;
				return 0;
			}
		}
		xfs_trans_brelse(tp, agbp);
nextag:
		if (++tagno == agcount)
			tagno = 0;
		if (tagno == agno) {
			*inop = NULLFSINO;
			return noroom ? ENOSPC : 0;
		}
		pag = xfs_perag_get(mp, tagno);
		if (pag->pagi_inodeok == 0) {
			xfs_perag_put(pag);
			goto nextag;
		}
		error = xfs_ialloc_read_agi(mp, tp, tagno, &agbp);
		xfs_perag_put(pag);
		if (error)
			goto nextag;
		agi = XFS_BUF_TO_AGI(agbp);
		ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
	}
	agno = tagno;
	*IO_agbp = NULL;
	pag = xfs_perag_get(mp, agno);

 restart_pagno:
	cur = xfs_inobt_init_cursor(mp, tp, agbp, be32_to_cpu(agi->agi_seqno));
	if (!pagino)
		pagino = be32_to_cpu(agi->agi_newino);

	error = xfs_check_agi_freecount(cur, agi);
	if (error)
		goto error0;

	if (pagno == agno) {
		int		doneleft;	
		int		doneright;	
		int		searchdistance = 10;

		error = xfs_inobt_lookup(cur, pagino, XFS_LOOKUP_LE, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		error = xfs_inobt_get_rec(cur, &rec, &j);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		if (rec.ir_freecount > 0) {
			goto alloc_inode;
		}



		
		error = xfs_btree_dup_cursor(cur, &tcur);
		if (error)
			goto error0;

		if (pagino != NULLAGINO &&
		    pag->pagl_pagino == pagino &&
		    pag->pagl_leftrec != NULLAGINO &&
		    pag->pagl_rightrec != NULLAGINO) {
			error = xfs_ialloc_get_rec(tcur, pag->pagl_leftrec,
						   &trec, &doneleft, 1);
			if (error)
				goto error1;

			error = xfs_ialloc_get_rec(cur, pag->pagl_rightrec,
						   &rec, &doneright, 0);
			if (error)
				goto error1;
		} else {
			
			error = xfs_ialloc_next_rec(tcur, &trec, &doneleft, 1);
			if (error)
				goto error1;

			
			error = xfs_ialloc_next_rec(cur, &rec, &doneright, 0);
			if (error)
				goto error1;
		}

		while (!doneleft || !doneright) {
			int	useleft;  

			if (!--searchdistance) {
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				pag->pagl_leftrec = trec.ir_startino;
				pag->pagl_rightrec = rec.ir_startino;
				pag->pagl_pagino = pagino;
				goto newino;
			}

			
			if (!doneleft && !doneright) {
				useleft = pagino -
				 (trec.ir_startino + XFS_INODES_PER_CHUNK - 1) <
				  rec.ir_startino - pagino;
			} else {
				useleft = !doneleft;
			}

			
			if (useleft && trec.ir_freecount) {
				rec = trec;
				xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
				cur = tcur;

				pag->pagl_leftrec = trec.ir_startino;
				pag->pagl_rightrec = rec.ir_startino;
				pag->pagl_pagino = pagino;
				goto alloc_inode;
			}

			
			if (!useleft && rec.ir_freecount) {
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);

				pag->pagl_leftrec = trec.ir_startino;
				pag->pagl_rightrec = rec.ir_startino;
				pag->pagl_pagino = pagino;
				goto alloc_inode;
			}

			
			if (useleft) {
				error = xfs_ialloc_next_rec(tcur, &trec,
								 &doneleft, 1);
			} else {
				error = xfs_ialloc_next_rec(cur, &rec,
								 &doneright, 0);
			}
			if (error)
				goto error1;
		}

		pag->pagl_pagino = NULLAGINO;
		pag->pagl_leftrec = NULLAGINO;
		pag->pagl_rightrec = NULLAGINO;
		xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
		goto restart_pagno;
	}

newino:
	if (agi->agi_newino != cpu_to_be32(NULLAGINO)) {
		error = xfs_inobt_lookup(cur, be32_to_cpu(agi->agi_newino),
					 XFS_LOOKUP_EQ, &i);
		if (error)
			goto error0;

		if (i == 1) {
			error = xfs_inobt_get_rec(cur, &rec, &j);
			if (error)
				goto error0;

			if (j == 1 && rec.ir_freecount > 0) {
				goto alloc_inode;
			}
		}
	}

	error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &i);
	if (error)
		goto error0;
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

	for (;;) {
		error = xfs_inobt_get_rec(cur, &rec, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		if (rec.ir_freecount > 0)
			break;
		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	}

alloc_inode:
	offset = xfs_ialloc_find_free(&rec.ir_free);
	ASSERT(offset >= 0);
	ASSERT(offset < XFS_INODES_PER_CHUNK);
	ASSERT((XFS_AGINO_TO_OFFSET(mp, rec.ir_startino) %
				   XFS_INODES_PER_CHUNK) == 0);
	ino = XFS_AGINO_TO_INO(mp, agno, rec.ir_startino + offset);
	rec.ir_free &= ~XFS_INOBT_MASK(offset);
	rec.ir_freecount--;
	error = xfs_inobt_update(cur, &rec);
	if (error)
		goto error0;
	be32_add_cpu(&agi->agi_freecount, -1);
	xfs_ialloc_log_agi(tp, agbp, XFS_AGI_FREECOUNT);
	pag->pagi_freecount--;

	error = xfs_check_agi_freecount(cur, agi);
	if (error)
		goto error0;

	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, -1);
	xfs_perag_put(pag);
	*inop = ino;
	return 0;
error1:
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	xfs_perag_put(pag);
	return error;
}

int
xfs_difree(
	xfs_trans_t	*tp,		
	xfs_ino_t	inode,		
	xfs_bmap_free_t	*flist,		
	int		*delete,	
	xfs_ino_t	*first_ino)	
{
	
	xfs_agblock_t	agbno;	
	xfs_buf_t	*agbp;	
	xfs_agino_t	agino;	
	xfs_agnumber_t	agno;	
	xfs_agi_t	*agi;	
	xfs_btree_cur_t	*cur;	
	int		error;	
	int		i;	
	int		ilen;	
	xfs_mount_t	*mp;	
	int		off;	
	xfs_inobt_rec_incore_t rec;	
	struct xfs_perag *pag;

	mp = tp->t_mountp;

	agno = XFS_INO_TO_AGNO(mp, inode);
	if (agno >= mp->m_sb.sb_agcount)  {
		xfs_warn(mp, "%s: agno >= mp->m_sb.sb_agcount (%d >= %d).",
			__func__, agno, mp->m_sb.sb_agcount);
		ASSERT(0);
		return XFS_ERROR(EINVAL);
	}
	agino = XFS_INO_TO_AGINO(mp, inode);
	if (inode != XFS_AGINO_TO_INO(mp, agno, agino))  {
		xfs_warn(mp, "%s: inode != XFS_AGINO_TO_INO() (%llu != %llu).",
			__func__, (unsigned long long)inode,
			(unsigned long long)XFS_AGINO_TO_INO(mp, agno, agino));
		ASSERT(0);
		return XFS_ERROR(EINVAL);
	}
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	if (agbno >= mp->m_sb.sb_agblocks)  {
		xfs_warn(mp, "%s: agbno >= mp->m_sb.sb_agblocks (%d >= %d).",
			__func__, agbno, mp->m_sb.sb_agblocks);
		ASSERT(0);
		return XFS_ERROR(EINVAL);
	}
	error = xfs_ialloc_read_agi(mp, tp, agno, &agbp);
	if (error) {
		xfs_warn(mp, "%s: xfs_ialloc_read_agi() returned error %d.",
			__func__, error);
		return error;
	}
	agi = XFS_BUF_TO_AGI(agbp);
	ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
	ASSERT(agbno < be32_to_cpu(agi->agi_length));
	cur = xfs_inobt_init_cursor(mp, tp, agbp, agno);

	error = xfs_check_agi_freecount(cur, agi);
	if (error)
		goto error0;

	if ((error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &i))) {
		xfs_warn(mp, "%s: xfs_inobt_lookup() returned error %d.",
			__func__, error);
		goto error0;
	}
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	error = xfs_inobt_get_rec(cur, &rec, &i);
	if (error) {
		xfs_warn(mp, "%s: xfs_inobt_get_rec() returned error %d.",
			__func__, error);
		goto error0;
	}
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
	off = agino - rec.ir_startino;
	ASSERT(off >= 0 && off < XFS_INODES_PER_CHUNK);
	ASSERT(!(rec.ir_free & XFS_INOBT_MASK(off)));
	rec.ir_free |= XFS_INOBT_MASK(off);
	rec.ir_freecount++;

	if (!(mp->m_flags & XFS_MOUNT_IKEEP) &&
	    (rec.ir_freecount == XFS_IALLOC_INODES(mp))) {

		*delete = 1;
		*first_ino = XFS_AGINO_TO_INO(mp, agno, rec.ir_startino);

		ilen = XFS_IALLOC_INODES(mp);
		be32_add_cpu(&agi->agi_count, -ilen);
		be32_add_cpu(&agi->agi_freecount, -(ilen - 1));
		xfs_ialloc_log_agi(tp, agbp, XFS_AGI_COUNT | XFS_AGI_FREECOUNT);
		pag = xfs_perag_get(mp, agno);
		pag->pagi_freecount -= ilen - 1;
		xfs_perag_put(pag);
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_ICOUNT, -ilen);
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, -(ilen - 1));

		if ((error = xfs_btree_delete(cur, &i))) {
			xfs_warn(mp, "%s: xfs_btree_delete returned error %d.",
				__func__, error);
			goto error0;
		}

		xfs_bmap_add_free(XFS_AGB_TO_FSB(mp,
				agno, XFS_INO_TO_AGBNO(mp,rec.ir_startino)),
				XFS_IALLOC_BLOCKS(mp), flist, mp);
	} else {
		*delete = 0;

		error = xfs_inobt_update(cur, &rec);
		if (error) {
			xfs_warn(mp, "%s: xfs_inobt_update returned error %d.",
				__func__, error);
			goto error0;
		}

		be32_add_cpu(&agi->agi_freecount, 1);
		xfs_ialloc_log_agi(tp, agbp, XFS_AGI_FREECOUNT);
		pag = xfs_perag_get(mp, agno);
		pag->pagi_freecount++;
		xfs_perag_put(pag);
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, 1);
	}

	error = xfs_check_agi_freecount(cur, agi);
	if (error)
		goto error0;

	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	return 0;

error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

STATIC int
xfs_imap_lookup(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agino_t		agino,
	xfs_agblock_t		agbno,
	xfs_agblock_t		*chunk_agbno,
	xfs_agblock_t		*offset_agbno,
	int			flags)
{
	struct xfs_inobt_rec_incore rec;
	struct xfs_btree_cur	*cur;
	struct xfs_buf		*agbp;
	int			error;
	int			i;

	error = xfs_ialloc_read_agi(mp, tp, agno, &agbp);
	if (error) {
		xfs_alert(mp,
			"%s: xfs_ialloc_read_agi() returned error %d, agno %d",
			__func__, error, agno);
		return error;
	}

	cur = xfs_inobt_init_cursor(mp, tp, agbp, agno);
	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &i);
	if (!error) {
		if (i)
			error = xfs_inobt_get_rec(cur, &rec, &i);
		if (!error && i == 0)
			error = EINVAL;
	}

	xfs_trans_brelse(tp, agbp);
	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	if (error)
		return error;

	
	if (rec.ir_startino > agino ||
	    rec.ir_startino + XFS_IALLOC_INODES(mp) <= agino)
		return EINVAL;

	
	if ((flags & XFS_IGET_UNTRUSTED) &&
	    (rec.ir_free & XFS_INOBT_MASK(agino - rec.ir_startino)))
		return EINVAL;

	*chunk_agbno = XFS_AGINO_TO_AGBNO(mp, rec.ir_startino);
	*offset_agbno = agbno - *chunk_agbno;
	return 0;
}

int
xfs_imap(
	xfs_mount_t	 *mp,	
	xfs_trans_t	 *tp,	
	xfs_ino_t	ino,	
	struct xfs_imap	*imap,	
	uint		flags)	
{
	xfs_agblock_t	agbno;	
	xfs_agino_t	agino;	
	xfs_agnumber_t	agno;	
	int		blks_per_cluster; 
	xfs_agblock_t	chunk_agbno;	
	xfs_agblock_t	cluster_agbno;	
	int		error;	
	int		offset;	
	int		offset_agbno;	

	ASSERT(ino != NULLFSINO);

	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	if (agno >= mp->m_sb.sb_agcount || agbno >= mp->m_sb.sb_agblocks ||
	    ino != XFS_AGINO_TO_INO(mp, agno, agino)) {
#ifdef DEBUG
		if (flags & XFS_IGET_UNTRUSTED)
			return XFS_ERROR(EINVAL);
		if (agno >= mp->m_sb.sb_agcount) {
			xfs_alert(mp,
				"%s: agno (%d) >= mp->m_sb.sb_agcount (%d)",
				__func__, agno, mp->m_sb.sb_agcount);
		}
		if (agbno >= mp->m_sb.sb_agblocks) {
			xfs_alert(mp,
		"%s: agbno (0x%llx) >= mp->m_sb.sb_agblocks (0x%lx)",
				__func__, (unsigned long long)agbno,
				(unsigned long)mp->m_sb.sb_agblocks);
		}
		if (ino != XFS_AGINO_TO_INO(mp, agno, agino)) {
			xfs_alert(mp,
		"%s: ino (0x%llx) != XFS_AGINO_TO_INO() (0x%llx)",
				__func__, ino,
				XFS_AGINO_TO_INO(mp, agno, agino));
		}
		xfs_stack_trace();
#endif 
		return XFS_ERROR(EINVAL);
	}

	blks_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_blocklog;

	if (flags & XFS_IGET_UNTRUSTED) {
		error = xfs_imap_lookup(mp, tp, agno, agino, agbno,
					&chunk_agbno, &offset_agbno, flags);
		if (error)
			return error;
		goto out_map;
	}

	if (XFS_INODE_CLUSTER_SIZE(mp) <= mp->m_sb.sb_blocksize) {
		offset = XFS_INO_TO_OFFSET(mp, ino);
		ASSERT(offset < mp->m_sb.sb_inopblock);

		imap->im_blkno = XFS_AGB_TO_DADDR(mp, agno, agbno);
		imap->im_len = XFS_FSB_TO_BB(mp, 1);
		imap->im_boffset = (ushort)(offset << mp->m_sb.sb_inodelog);
		return 0;
	}

	if (mp->m_inoalign_mask) {
		offset_agbno = agbno & mp->m_inoalign_mask;
		chunk_agbno = agbno - offset_agbno;
	} else {
		error = xfs_imap_lookup(mp, tp, agno, agino, agbno,
					&chunk_agbno, &offset_agbno, flags);
		if (error)
			return error;
	}

out_map:
	ASSERT(agbno >= chunk_agbno);
	cluster_agbno = chunk_agbno +
		((offset_agbno / blks_per_cluster) * blks_per_cluster);
	offset = ((agbno - cluster_agbno) * mp->m_sb.sb_inopblock) +
		XFS_INO_TO_OFFSET(mp, ino);

	imap->im_blkno = XFS_AGB_TO_DADDR(mp, agno, cluster_agbno);
	imap->im_len = XFS_FSB_TO_BB(mp, blks_per_cluster);
	imap->im_boffset = (ushort)(offset << mp->m_sb.sb_inodelog);

	if ((imap->im_blkno + imap->im_len) >
	    XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks)) {
		xfs_alert(mp,
	"%s: (im_blkno (0x%llx) + im_len (0x%llx)) > sb_dblocks (0x%llx)",
			__func__, (unsigned long long) imap->im_blkno,
			(unsigned long long) imap->im_len,
			XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks));
		return XFS_ERROR(EINVAL);
	}
	return 0;
}

void
xfs_ialloc_compute_maxlevels(
	xfs_mount_t	*mp)		
{
	int		level;
	uint		maxblocks;
	uint		maxleafents;
	int		minleafrecs;
	int		minnoderecs;

	maxleafents = (1LL << XFS_INO_AGINO_BITS(mp)) >>
		XFS_INODES_PER_CHUNK_LOG;
	minleafrecs = mp->m_alloc_mnr[0];
	minnoderecs = mp->m_alloc_mnr[1];
	maxblocks = (maxleafents + minleafrecs - 1) / minleafrecs;
	for (level = 1; maxblocks > 1; level++)
		maxblocks = (maxblocks + minnoderecs - 1) / minnoderecs;
	mp->m_in_maxlevels = level;
}

void
xfs_ialloc_log_agi(
	xfs_trans_t	*tp,		
	xfs_buf_t	*bp,		
	int		fields)		
{
	int			first;		
	int			last;		
	static const short	offsets[] = {	
					
		offsetof(xfs_agi_t, agi_magicnum),
		offsetof(xfs_agi_t, agi_versionnum),
		offsetof(xfs_agi_t, agi_seqno),
		offsetof(xfs_agi_t, agi_length),
		offsetof(xfs_agi_t, agi_count),
		offsetof(xfs_agi_t, agi_root),
		offsetof(xfs_agi_t, agi_level),
		offsetof(xfs_agi_t, agi_freecount),
		offsetof(xfs_agi_t, agi_newino),
		offsetof(xfs_agi_t, agi_dirino),
		offsetof(xfs_agi_t, agi_unlinked),
		sizeof(xfs_agi_t)
	};
#ifdef DEBUG
	xfs_agi_t		*agi;	

	agi = XFS_BUF_TO_AGI(bp);
	ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
#endif
	xfs_btree_offsets(fields, offsets, XFS_AGI_NUM_BITS, &first, &last);
	xfs_trans_log_buf(tp, bp, first, last);
}

#ifdef DEBUG
STATIC void
xfs_check_agi_unlinked(
	struct xfs_agi		*agi)
{
	int			i;

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)
		ASSERT(agi->agi_unlinked[i]);
}
#else
#define xfs_check_agi_unlinked(agi)
#endif

int
xfs_read_agi(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_agnumber_t		agno,	
	struct xfs_buf		**bpp)	
{
	struct xfs_agi		*agi;	
	int			agi_ok;	
	int			error;

	ASSERT(agno != NULLAGNUMBER);

	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, bpp);
	if (error)
		return error;

	ASSERT(!xfs_buf_geterror(*bpp));
	agi = XFS_BUF_TO_AGI(*bpp);

	agi_ok = agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC) &&
		XFS_AGI_GOOD_VERSION(be32_to_cpu(agi->agi_versionnum)) &&
		be32_to_cpu(agi->agi_seqno) == agno;
	if (unlikely(XFS_TEST_ERROR(!agi_ok, mp, XFS_ERRTAG_IALLOC_READ_AGI,
			XFS_RANDOM_IALLOC_READ_AGI))) {
		XFS_CORRUPTION_ERROR("xfs_read_agi", XFS_ERRLEVEL_LOW,
				     mp, agi);
		xfs_trans_brelse(tp, *bpp);
		return XFS_ERROR(EFSCORRUPTED);
	}

	xfs_buf_set_ref(*bpp, XFS_AGI_REF);

	xfs_check_agi_unlinked(agi);
	return 0;
}

int
xfs_ialloc_read_agi(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_agnumber_t		agno,	
	struct xfs_buf		**bpp)	
{
	struct xfs_agi		*agi;	
	struct xfs_perag	*pag;	
	int			error;

	error = xfs_read_agi(mp, tp, agno, bpp);
	if (error)
		return error;

	agi = XFS_BUF_TO_AGI(*bpp);
	pag = xfs_perag_get(mp, agno);
	if (!pag->pagi_init) {
		pag->pagi_freecount = be32_to_cpu(agi->agi_freecount);
		pag->pagi_count = be32_to_cpu(agi->agi_count);
		pag->pagi_init = 1;
	}

	ASSERT(pag->pagi_freecount == be32_to_cpu(agi->agi_freecount) ||
		XFS_FORCED_SHUTDOWN(mp));
	xfs_perag_put(pag);
	return 0;
}

int
xfs_ialloc_pagi_init(
	xfs_mount_t	*mp,		
	xfs_trans_t	*tp,		
	xfs_agnumber_t	agno)		
{
	xfs_buf_t	*bp = NULL;
	int		error;

	error = xfs_ialloc_read_agi(mp, tp, agno, &bp);
	if (error)
		return error;
	if (bp)
		xfs_trans_brelse(tp, bp);
	return 0;
}
