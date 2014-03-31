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
#ifndef __XFS_ALLOC_H__
#define	__XFS_ALLOC_H__

struct xfs_buf;
struct xfs_btree_cur;
struct xfs_mount;
struct xfs_perag;
struct xfs_trans;
struct xfs_busy_extent;

extern struct workqueue_struct *xfs_alloc_wq;

#define XFS_ALLOCTYPE_ANY_AG	0x01	
#define XFS_ALLOCTYPE_FIRST_AG	0x02	
#define XFS_ALLOCTYPE_START_AG	0x04	
#define XFS_ALLOCTYPE_THIS_AG	0x08	
#define XFS_ALLOCTYPE_START_BNO	0x10	
#define XFS_ALLOCTYPE_NEAR_BNO	0x20	
#define XFS_ALLOCTYPE_THIS_BNO	0x40	

typedef unsigned int xfs_alloctype_t;

#define XFS_ALLOC_TYPES \
	{ XFS_ALLOCTYPE_ANY_AG,		"ANY_AG" }, \
	{ XFS_ALLOCTYPE_FIRST_AG,	"FIRST_AG" }, \
	{ XFS_ALLOCTYPE_START_AG,	"START_AG" }, \
	{ XFS_ALLOCTYPE_THIS_AG,	"THIS_AG" }, \
	{ XFS_ALLOCTYPE_START_BNO,	"START_BNO" }, \
	{ XFS_ALLOCTYPE_NEAR_BNO,	"NEAR_BNO" }, \
	{ XFS_ALLOCTYPE_THIS_BNO,	"THIS_BNO" }

#define	XFS_ALLOC_FLAG_TRYLOCK	0x00000001  
#define	XFS_ALLOC_FLAG_FREEING	0x00000002  

#define XFS_ALLOC_SET_ASIDE(mp)  (4 + ((mp)->m_sb.sb_agcount * 4))

#define XFS_ALLOC_AG_MAX_USABLE(mp)	\
	((mp)->m_sb.sb_agblocks - XFS_BB_TO_FSB(mp, XFS_FSS_TO_BB(mp, 4)) - 7)


typedef struct xfs_alloc_arg {
	struct xfs_trans *tp;		
	struct xfs_mount *mp;		
	struct xfs_buf	*agbp;		
	struct xfs_perag *pag;		
	xfs_fsblock_t	fsbno;		
	xfs_agnumber_t	agno;		
	xfs_agblock_t	agbno;		
	xfs_extlen_t	minlen;		
	xfs_extlen_t	maxlen;		
	xfs_extlen_t	mod;		
	xfs_extlen_t	prod;		
	xfs_extlen_t	minleft;	
	xfs_extlen_t	total;		
	xfs_extlen_t	alignment;	
	xfs_extlen_t	minalignslop;	
	xfs_extlen_t	len;		
	xfs_alloctype_t	type;		
	xfs_alloctype_t	otype;		
	char		wasdel;		
	char		wasfromfl;	
	char		isfl;		
	char		userdata;	
	xfs_fsblock_t	firstblock;	
	struct completion *done;
	struct work_struct work;
	int		result;
} xfs_alloc_arg_t;

#define XFS_ALLOC_USERDATA		1	
#define XFS_ALLOC_INITIAL_USER_DATA	2	

xfs_extlen_t
xfs_alloc_longest_free_extent(struct xfs_mount *mp,
		struct xfs_perag *pag);

#ifdef __KERNEL__
void
xfs_alloc_busy_insert(struct xfs_trans *tp, xfs_agnumber_t agno,
	xfs_agblock_t bno, xfs_extlen_t len, unsigned int flags);

void
xfs_alloc_busy_clear(struct xfs_mount *mp, struct list_head *list,
	bool do_discard);

int
xfs_alloc_busy_search(struct xfs_mount *mp, xfs_agnumber_t agno,
	xfs_agblock_t bno, xfs_extlen_t len);

void
xfs_alloc_busy_reuse(struct xfs_mount *mp, xfs_agnumber_t agno,
	xfs_agblock_t fbno, xfs_extlen_t flen, bool userdata);

int
xfs_busy_extent_ag_cmp(void *priv, struct list_head *a, struct list_head *b);

static inline void xfs_alloc_busy_sort(struct list_head *list)
{
	list_sort(NULL, list, xfs_busy_extent_ag_cmp);
}

#endif	

void
xfs_alloc_compute_maxlevels(
	struct xfs_mount	*mp);	

int				
xfs_alloc_get_freelist(
	struct xfs_trans *tp,	
	struct xfs_buf	*agbp,	
	xfs_agblock_t	*bnop,	
	int		btreeblk); 

void
xfs_alloc_log_agf(
	struct xfs_trans *tp,	
	struct xfs_buf	*bp,	
	int		fields);

int				
xfs_alloc_pagf_init(
	struct xfs_mount *mp,	
	struct xfs_trans *tp,	
	xfs_agnumber_t	agno,	
	int		flags);	

int				
xfs_alloc_put_freelist(
	struct xfs_trans *tp,	
	struct xfs_buf	*agbp,	
	struct xfs_buf	*agflbp,
	xfs_agblock_t	bno,	
	int		btreeblk); 

int					
xfs_alloc_read_agf(
	struct xfs_mount *mp,		
	struct xfs_trans *tp,		
	xfs_agnumber_t	agno,		
	int		flags,		
	struct xfs_buf	**bpp);		

int				
xfs_alloc_vextent(
	xfs_alloc_arg_t	*args);	

int				
xfs_free_extent(
	struct xfs_trans *tp,	
	xfs_fsblock_t	bno,	
	xfs_extlen_t	len);	

int					
xfs_alloc_lookup_le(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len,	
	int			*stat);	

int				
xfs_alloc_lookup_ge(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		bno,	
	xfs_extlen_t		len,	
	int			*stat);	

int					
xfs_alloc_get_rec(
	struct xfs_btree_cur	*cur,	
	xfs_agblock_t		*bno,	
	xfs_extlen_t		*len,	
	int			*stat);	

#endif	
