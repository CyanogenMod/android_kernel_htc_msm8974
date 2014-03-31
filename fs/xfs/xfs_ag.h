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
#ifndef __XFS_AG_H__
#define	__XFS_AG_H__


struct xfs_buf;
struct xfs_mount;
struct xfs_trans;

#define	XFS_AGF_MAGIC	0x58414746	
#define	XFS_AGI_MAGIC	0x58414749	
#define	XFS_AGF_VERSION	1
#define	XFS_AGI_VERSION	1

#define	XFS_AGF_GOOD_VERSION(v)	((v) == XFS_AGF_VERSION)
#define	XFS_AGI_GOOD_VERSION(v)	((v) == XFS_AGI_VERSION)

#define	XFS_BTNUM_AGF	((int)XFS_BTNUM_CNTi + 1)


typedef struct xfs_agf {
	__be32		agf_magicnum;	
	__be32		agf_versionnum;	
	__be32		agf_seqno;	
	__be32		agf_length;	
	__be32		agf_roots[XFS_BTNUM_AGF];	
	__be32		agf_spare0;	
	__be32		agf_levels[XFS_BTNUM_AGF];	
	__be32		agf_spare1;	
	__be32		agf_flfirst;	
	__be32		agf_fllast;	
	__be32		agf_flcount;	
	__be32		agf_freeblks;	
	__be32		agf_longest;	
	__be32		agf_btreeblks;	
} xfs_agf_t;

#define	XFS_AGF_MAGICNUM	0x00000001
#define	XFS_AGF_VERSIONNUM	0x00000002
#define	XFS_AGF_SEQNO		0x00000004
#define	XFS_AGF_LENGTH		0x00000008
#define	XFS_AGF_ROOTS		0x00000010
#define	XFS_AGF_LEVELS		0x00000020
#define	XFS_AGF_FLFIRST		0x00000040
#define	XFS_AGF_FLLAST		0x00000080
#define	XFS_AGF_FLCOUNT		0x00000100
#define	XFS_AGF_FREEBLKS	0x00000200
#define	XFS_AGF_LONGEST		0x00000400
#define	XFS_AGF_BTREEBLKS	0x00000800
#define	XFS_AGF_NUM_BITS	12
#define	XFS_AGF_ALL_BITS	((1 << XFS_AGF_NUM_BITS) - 1)

#define XFS_AGF_FLAGS \
	{ XFS_AGF_MAGICNUM,	"MAGICNUM" }, \
	{ XFS_AGF_VERSIONNUM,	"VERSIONNUM" }, \
	{ XFS_AGF_SEQNO,	"SEQNO" }, \
	{ XFS_AGF_LENGTH,	"LENGTH" }, \
	{ XFS_AGF_ROOTS,	"ROOTS" }, \
	{ XFS_AGF_LEVELS,	"LEVELS" }, \
	{ XFS_AGF_FLFIRST,	"FLFIRST" }, \
	{ XFS_AGF_FLLAST,	"FLLAST" }, \
	{ XFS_AGF_FLCOUNT,	"FLCOUNT" }, \
	{ XFS_AGF_FREEBLKS,	"FREEBLKS" }, \
	{ XFS_AGF_LONGEST,	"LONGEST" }, \
	{ XFS_AGF_BTREEBLKS,	"BTREEBLKS" }

#define XFS_AGF_DADDR(mp)	((xfs_daddr_t)(1 << (mp)->m_sectbb_log))
#define	XFS_AGF_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGF_DADDR(mp))
#define	XFS_BUF_TO_AGF(bp)	((xfs_agf_t *)((bp)->b_addr))

extern int xfs_read_agf(struct xfs_mount *mp, struct xfs_trans *tp,
			xfs_agnumber_t agno, int flags, struct xfs_buf **bpp);

#define	XFS_AGI_UNLINKED_BUCKETS	64

typedef struct xfs_agi {
	__be32		agi_magicnum;	
	__be32		agi_versionnum;	
	__be32		agi_seqno;	
	__be32		agi_length;	
	__be32		agi_count;	
	__be32		agi_root;	
	__be32		agi_level;	
	__be32		agi_freecount;	
	__be32		agi_newino;	
	__be32		agi_dirino;	
	__be32		agi_unlinked[XFS_AGI_UNLINKED_BUCKETS];
} xfs_agi_t;

#define	XFS_AGI_MAGICNUM	0x00000001
#define	XFS_AGI_VERSIONNUM	0x00000002
#define	XFS_AGI_SEQNO		0x00000004
#define	XFS_AGI_LENGTH		0x00000008
#define	XFS_AGI_COUNT		0x00000010
#define	XFS_AGI_ROOT		0x00000020
#define	XFS_AGI_LEVEL		0x00000040
#define	XFS_AGI_FREECOUNT	0x00000080
#define	XFS_AGI_NEWINO		0x00000100
#define	XFS_AGI_DIRINO		0x00000200
#define	XFS_AGI_UNLINKED	0x00000400
#define	XFS_AGI_NUM_BITS	11
#define	XFS_AGI_ALL_BITS	((1 << XFS_AGI_NUM_BITS) - 1)

#define XFS_AGI_DADDR(mp)	((xfs_daddr_t)(2 << (mp)->m_sectbb_log))
#define	XFS_AGI_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGI_DADDR(mp))
#define	XFS_BUF_TO_AGI(bp)	((xfs_agi_t *)((bp)->b_addr))

extern int xfs_read_agi(struct xfs_mount *mp, struct xfs_trans *tp,
				xfs_agnumber_t agno, struct xfs_buf **bpp);

#define XFS_AGFL_DADDR(mp)	((xfs_daddr_t)(3 << (mp)->m_sectbb_log))
#define	XFS_AGFL_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGFL_DADDR(mp))
#define XFS_AGFL_SIZE(mp)	((mp)->m_sb.sb_sectsize / sizeof(xfs_agblock_t))
#define	XFS_BUF_TO_AGFL(bp)	((xfs_agfl_t *)((bp)->b_addr))

typedef struct xfs_agfl {
	__be32		agfl_bno[1];	
} xfs_agfl_t;

struct xfs_busy_extent {
	struct rb_node	rb_node;	
	struct list_head list;		
	xfs_agnumber_t	agno;
	xfs_agblock_t	bno;
	xfs_extlen_t	length;
	unsigned int	flags;
#define XFS_ALLOC_BUSY_DISCARDED	0x01	
#define XFS_ALLOC_BUSY_SKIP_DISCARD	0x02	
};

#define XFS_PAGB_NUM_SLOTS	128

typedef struct xfs_perag {
	struct xfs_mount *pag_mount;	
	xfs_agnumber_t	pag_agno;	
	atomic_t	pag_ref;	
	char		pagf_init;	
	char		pagi_init;	
	char		pagf_metadata;	
	char		pagi_inodeok;	
	__uint8_t	pagf_levels[XFS_BTNUM_AGF];
					
	__uint32_t	pagf_flcount;	
	xfs_extlen_t	pagf_freeblks;	
	xfs_extlen_t	pagf_longest;	
	__uint32_t	pagf_btreeblks;	
	xfs_agino_t	pagi_freecount;	
	xfs_agino_t	pagi_count;	

	xfs_agino_t	pagl_pagino;
	xfs_agino_t	pagl_leftrec;
	xfs_agino_t	pagl_rightrec;
#ifdef __KERNEL__
	spinlock_t	pagb_lock;	
	struct rb_root	pagb_tree;	

	atomic_t        pagf_fstrms;    

	spinlock_t	pag_ici_lock;	
	struct radix_tree_root pag_ici_root;	
	int		pag_ici_reclaimable;	
	struct mutex	pag_ici_reclaim_lock;	
	unsigned long	pag_ici_reclaim_cursor;	

	
	spinlock_t	pag_buf_lock;	
	struct rb_root	pag_buf_tree;	

	
	struct rcu_head	rcu_head;
#endif
	int		pagb_count;	
} xfs_perag_t;

#define XFS_ICI_NO_TAG		(-1)	
#define XFS_ICI_RECLAIM_TAG	0	

#define	XFS_AG_MAXLEVELS(mp)		((mp)->m_ag_maxlevels)
#define	XFS_MIN_FREELIST_RAW(bl,cl,mp)	\
	(MIN(bl + 1, XFS_AG_MAXLEVELS(mp)) + MIN(cl + 1, XFS_AG_MAXLEVELS(mp)))
#define	XFS_MIN_FREELIST(a,mp)		\
	(XFS_MIN_FREELIST_RAW(		\
		be32_to_cpu((a)->agf_levels[XFS_BTNUM_BNOi]), \
		be32_to_cpu((a)->agf_levels[XFS_BTNUM_CNTi]), mp))
#define	XFS_MIN_FREELIST_PAG(pag,mp)	\
	(XFS_MIN_FREELIST_RAW(		\
		(unsigned int)(pag)->pagf_levels[XFS_BTNUM_BNOi], \
		(unsigned int)(pag)->pagf_levels[XFS_BTNUM_CNTi], mp))

#define XFS_AGB_TO_FSB(mp,agno,agbno)	\
	(((xfs_fsblock_t)(agno) << (mp)->m_sb.sb_agblklog) | (agbno))
#define	XFS_FSB_TO_AGNO(mp,fsbno)	\
	((xfs_agnumber_t)((fsbno) >> (mp)->m_sb.sb_agblklog))
#define	XFS_FSB_TO_AGBNO(mp,fsbno)	\
	((xfs_agblock_t)((fsbno) & xfs_mask32lo((mp)->m_sb.sb_agblklog)))
#define	XFS_AGB_TO_DADDR(mp,agno,agbno)	\
	((xfs_daddr_t)XFS_FSB_TO_BB(mp, \
		(xfs_fsblock_t)(agno) * (mp)->m_sb.sb_agblocks + (agbno)))
#define	XFS_AG_DADDR(mp,agno,d)		(XFS_AGB_TO_DADDR(mp, agno, 0) + (d))

#define	XFS_AG_CHECK_DADDR(mp,d,len)	\
	((len) == 1 ? \
	    ASSERT((d) == XFS_SB_DADDR || \
		   xfs_daddr_to_agbno(mp, d) != XFS_SB_DADDR) : \
	    ASSERT(xfs_daddr_to_agno(mp, d) == \
		   xfs_daddr_to_agno(mp, (d) + (len) - 1)))

#endif	
