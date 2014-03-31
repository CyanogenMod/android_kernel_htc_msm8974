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
#ifndef __XFS_BTREE_H__
#define	__XFS_BTREE_H__

struct xfs_buf;
struct xfs_bmap_free;
struct xfs_inode;
struct xfs_mount;
struct xfs_trans;

extern kmem_zone_t	*xfs_btree_cur_zone;

#define	XFS_LOOKUP_EQ	((xfs_lookup_t)XFS_LOOKUP_EQi)
#define	XFS_LOOKUP_LE	((xfs_lookup_t)XFS_LOOKUP_LEi)
#define	XFS_LOOKUP_GE	((xfs_lookup_t)XFS_LOOKUP_GEi)

#define	XFS_BTNUM_BNO	((xfs_btnum_t)XFS_BTNUM_BNOi)
#define	XFS_BTNUM_CNT	((xfs_btnum_t)XFS_BTNUM_CNTi)
#define	XFS_BTNUM_BMAP	((xfs_btnum_t)XFS_BTNUM_BMAPi)
#define	XFS_BTNUM_INO	((xfs_btnum_t)XFS_BTNUM_INOi)

struct xfs_btree_block {
	__be32		bb_magic;	
	__be16		bb_level;	
	__be16		bb_numrecs;	
	union {
		struct {
			__be32		bb_leftsib;
			__be32		bb_rightsib;
		} s;			
		struct	{
			__be64		bb_leftsib;
			__be64		bb_rightsib;
		} l;			
	} bb_u;				
};

#define XFS_BTREE_SBLOCK_LEN	16	
#define XFS_BTREE_LBLOCK_LEN	24	


union xfs_btree_ptr {
	__be32			s;	
	__be64			l;	
};

union xfs_btree_key {
	xfs_bmbt_key_t		bmbt;
	xfs_bmdr_key_t		bmbr;	
	xfs_alloc_key_t		alloc;
	xfs_inobt_key_t		inobt;
};

union xfs_btree_rec {
	xfs_bmbt_rec_t		bmbt;
	xfs_bmdr_rec_t		bmbr;	
	xfs_alloc_rec_t		alloc;
	xfs_inobt_rec_t		inobt;
};

#define	XFS_BB_MAGIC		0x01
#define	XFS_BB_LEVEL		0x02
#define	XFS_BB_NUMRECS		0x04
#define	XFS_BB_LEFTSIB		0x08
#define	XFS_BB_RIGHTSIB		0x10
#define	XFS_BB_NUM_BITS		5
#define	XFS_BB_ALL_BITS		((1 << XFS_BB_NUM_BITS) - 1)

extern const __uint32_t	xfs_magics[];

#define __XFS_BTREE_STATS_INC(type, stat) \
	XFS_STATS_INC(xs_ ## type ## _2_ ## stat)
#define XFS_BTREE_STATS_INC(cur, stat)  \
do {    \
	switch (cur->bc_btnum) {  \
	case XFS_BTNUM_BNO: __XFS_BTREE_STATS_INC(abtb, stat); break;	\
	case XFS_BTNUM_CNT: __XFS_BTREE_STATS_INC(abtc, stat); break;	\
	case XFS_BTNUM_BMAP: __XFS_BTREE_STATS_INC(bmbt, stat); break;	\
	case XFS_BTNUM_INO: __XFS_BTREE_STATS_INC(ibt, stat); break;	\
	case XFS_BTNUM_MAX: ASSERT(0);  ; break;	\
	}       \
} while (0)

#define __XFS_BTREE_STATS_ADD(type, stat, val) \
	XFS_STATS_ADD(xs_ ## type ## _2_ ## stat, val)
#define XFS_BTREE_STATS_ADD(cur, stat, val)  \
do {    \
	switch (cur->bc_btnum) {  \
	case XFS_BTNUM_BNO: __XFS_BTREE_STATS_ADD(abtb, stat, val); break; \
	case XFS_BTNUM_CNT: __XFS_BTREE_STATS_ADD(abtc, stat, val); break; \
	case XFS_BTNUM_BMAP: __XFS_BTREE_STATS_ADD(bmbt, stat, val); break; \
	case XFS_BTNUM_INO: __XFS_BTREE_STATS_ADD(ibt, stat, val); break; \
	case XFS_BTNUM_MAX: ASSERT(0);  ; break;	\
	}       \
} while (0)

#define	XFS_BTREE_MAXLEVELS	8	

struct xfs_btree_ops {
	
	size_t	key_len;
	size_t	rec_len;

	
	struct xfs_btree_cur *(*dup_cursor)(struct xfs_btree_cur *);
	void	(*update_cursor)(struct xfs_btree_cur *src,
				 struct xfs_btree_cur *dst);

	
	void	(*set_root)(struct xfs_btree_cur *cur,
			    union xfs_btree_ptr *nptr, int level_change);

	
	int	(*alloc_block)(struct xfs_btree_cur *cur,
			       union xfs_btree_ptr *start_bno,
			       union xfs_btree_ptr *new_bno,
			       int length, int *stat);
	int	(*free_block)(struct xfs_btree_cur *cur, struct xfs_buf *bp);

	
	void	(*update_lastrec)(struct xfs_btree_cur *cur,
				  struct xfs_btree_block *block,
				  union xfs_btree_rec *rec,
				  int ptr, int reason);

	
	int	(*get_minrecs)(struct xfs_btree_cur *cur, int level);
	int	(*get_maxrecs)(struct xfs_btree_cur *cur, int level);

	
	int	(*get_dmaxrecs)(struct xfs_btree_cur *cur, int level);

	
	void	(*init_key_from_rec)(union xfs_btree_key *key,
				     union xfs_btree_rec *rec);
	void	(*init_rec_from_key)(union xfs_btree_key *key,
				     union xfs_btree_rec *rec);
	void	(*init_rec_from_cur)(struct xfs_btree_cur *cur,
				     union xfs_btree_rec *rec);
	void	(*init_ptr_from_cur)(struct xfs_btree_cur *cur,
				     union xfs_btree_ptr *ptr);

	
	__int64_t (*key_diff)(struct xfs_btree_cur *cur,
			      union xfs_btree_key *key);

#ifdef DEBUG
	
	int	(*keys_inorder)(struct xfs_btree_cur *cur,
				union xfs_btree_key *k1,
				union xfs_btree_key *k2);

	
	int	(*recs_inorder)(struct xfs_btree_cur *cur,
				union xfs_btree_rec *r1,
				union xfs_btree_rec *r2);
#endif
};

#define LASTREC_UPDATE	0
#define LASTREC_INSREC	1
#define LASTREC_DELREC	2


typedef struct xfs_btree_cur
{
	struct xfs_trans	*bc_tp;	
	struct xfs_mount	*bc_mp;	
	const struct xfs_btree_ops *bc_ops;
	uint			bc_flags; 
	union {
		xfs_alloc_rec_incore_t	a;
		xfs_bmbt_irec_t		b;
		xfs_inobt_rec_incore_t	i;
	}		bc_rec;		
	struct xfs_buf	*bc_bufs[XFS_BTREE_MAXLEVELS];	
	int		bc_ptrs[XFS_BTREE_MAXLEVELS];	
	__uint8_t	bc_ra[XFS_BTREE_MAXLEVELS];	
#define	XFS_BTCUR_LEFTRA	1	
#define	XFS_BTCUR_RIGHTRA	2	
	__uint8_t	bc_nlevels;	
	__uint8_t	bc_blocklog;	
	xfs_btnum_t	bc_btnum;	
	union {
		struct {			
			struct xfs_buf	*agbp;	
			xfs_agnumber_t	agno;	
		} a;
		struct {			
			struct xfs_inode *ip;	
			struct xfs_bmap_free *flist;	
			xfs_fsblock_t	firstblock;	
			int		allocated;	
			short		forksize;	
			char		whichfork;	
			char		flags;		
#define	XFS_BTCUR_BPRV_WASDEL	1			
		} b;
	}		bc_private;	
} xfs_btree_cur_t;

#define XFS_BTREE_LONG_PTRS		(1<<0)	
#define XFS_BTREE_ROOT_IN_INODE		(1<<1)	
#define XFS_BTREE_LASTREC_UPDATE	(1<<2)	


#define	XFS_BTREE_NOERROR	0
#define	XFS_BTREE_ERROR		1

#define	XFS_BUF_TO_BLOCK(bp)	((struct xfs_btree_block *)((bp)->b_addr))


int
xfs_btree_check_block(
	struct xfs_btree_cur	*cur,	
	struct xfs_btree_block	*block,	
	int			level,	
	struct xfs_buf		*bp);	

int					
xfs_btree_check_lptr(
	struct xfs_btree_cur	*cur,	
	xfs_dfsbno_t		ptr,	
	int			level);	

void
xfs_btree_del_cursor(
	xfs_btree_cur_t		*cur,	
	int			error);	

int					
xfs_btree_dup_cursor(
	xfs_btree_cur_t		*cur,	
	xfs_btree_cur_t		**ncur);

struct xfs_buf *				
xfs_btree_get_bufl(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_fsblock_t		fsbno,	
	uint			lock);	

struct xfs_buf *				
xfs_btree_get_bufs(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_agnumber_t		agno,	
	xfs_agblock_t		agbno,	
	uint			lock);	

int					
xfs_btree_islastblock(
	xfs_btree_cur_t		*cur,	
	int			level);	

void
xfs_btree_offsets(
	__int64_t		fields,	
	const short		*offsets,
	int			nbits,	
	int			*first,	
	int			*last);	

int					
xfs_btree_read_bufl(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_fsblock_t		fsbno,	
	uint			lock,	
	struct xfs_buf		**bpp,	
	int			refval);

void					
xfs_btree_reada_bufl(
	struct xfs_mount	*mp,	
	xfs_fsblock_t		fsbno,	
	xfs_extlen_t		count);	

void					
xfs_btree_reada_bufs(
	struct xfs_mount	*mp,	
	xfs_agnumber_t		agno,	
	xfs_agblock_t		agbno,	
	xfs_extlen_t		count);	


int xfs_btree_increment(struct xfs_btree_cur *, int, int *);
int xfs_btree_decrement(struct xfs_btree_cur *, int, int *);
int xfs_btree_lookup(struct xfs_btree_cur *, xfs_lookup_t, int *);
int xfs_btree_update(struct xfs_btree_cur *, union xfs_btree_rec *);
int xfs_btree_new_iroot(struct xfs_btree_cur *, int *, int *);
int xfs_btree_insert(struct xfs_btree_cur *, int *);
int xfs_btree_delete(struct xfs_btree_cur *, int *);
int xfs_btree_get_rec(struct xfs_btree_cur *, union xfs_btree_rec **, int *);

void xfs_btree_log_block(struct xfs_btree_cur *, struct xfs_buf *, int);
void xfs_btree_log_recs(struct xfs_btree_cur *, struct xfs_buf *, int, int);

static inline int xfs_btree_get_numrecs(struct xfs_btree_block *block)
{
	return be16_to_cpu(block->bb_numrecs);
}

static inline void xfs_btree_set_numrecs(struct xfs_btree_block *block,
		__uint16_t numrecs)
{
	block->bb_numrecs = cpu_to_be16(numrecs);
}

static inline int xfs_btree_get_level(struct xfs_btree_block *block)
{
	return be16_to_cpu(block->bb_level);
}


#define	XFS_EXTLEN_MIN(a,b)	min_t(xfs_extlen_t, (a), (b))
#define	XFS_EXTLEN_MAX(a,b)	max_t(xfs_extlen_t, (a), (b))
#define	XFS_AGBLOCK_MIN(a,b)	min_t(xfs_agblock_t, (a), (b))
#define	XFS_AGBLOCK_MAX(a,b)	max_t(xfs_agblock_t, (a), (b))
#define	XFS_FILEOFF_MIN(a,b)	min_t(xfs_fileoff_t, (a), (b))
#define	XFS_FILEOFF_MAX(a,b)	max_t(xfs_fileoff_t, (a), (b))
#define	XFS_FILBLKS_MIN(a,b)	min_t(xfs_filblks_t, (a), (b))
#define	XFS_FILBLKS_MAX(a,b)	max_t(xfs_filblks_t, (a), (b))

#define	XFS_FSB_SANITY_CHECK(mp,fsb)	\
	(XFS_FSB_TO_AGNO(mp, fsb) < mp->m_sb.sb_agcount && \
		XFS_FSB_TO_AGBNO(mp, fsb) < mp->m_sb.sb_agblocks)

#define	XFS_BTREE_TRACE_ARGBI(c, b, i)
#define	XFS_BTREE_TRACE_ARGBII(c, b, i, j)
#define	XFS_BTREE_TRACE_ARGI(c, i)
#define	XFS_BTREE_TRACE_ARGIPK(c, i, p, s)
#define	XFS_BTREE_TRACE_ARGIPR(c, i, p, r)
#define	XFS_BTREE_TRACE_ARGIK(c, i, k)
#define XFS_BTREE_TRACE_ARGR(c, r)
#define	XFS_BTREE_TRACE_CURSOR(c, t)

#endif	
