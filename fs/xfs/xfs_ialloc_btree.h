/*
 * Copyright (c) 2000,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_IALLOC_BTREE_H__
#define	__XFS_IALLOC_BTREE_H__


struct xfs_buf;
struct xfs_btree_cur;
struct xfs_mount;

#define	XFS_IBT_MAGIC	0x49414254	

typedef	__uint64_t	xfs_inofree_t;
#define	XFS_INODES_PER_CHUNK		(NBBY * sizeof(xfs_inofree_t))
#define	XFS_INODES_PER_CHUNK_LOG	(XFS_NBBYLOG + 3)
#define	XFS_INOBT_ALL_FREE		((xfs_inofree_t)-1)
#define	XFS_INOBT_MASK(i)		((xfs_inofree_t)1 << (i))

static inline xfs_inofree_t xfs_inobt_maskn(int i, int n)
{
	return ((n >= XFS_INODES_PER_CHUNK ? 0 : XFS_INOBT_MASK(n)) - 1) << i;
}

typedef struct xfs_inobt_rec {
	__be32		ir_startino;	
	__be32		ir_freecount;	
	__be64		ir_free;	
} xfs_inobt_rec_t;

typedef struct xfs_inobt_rec_incore {
	xfs_agino_t	ir_startino;	
	__int32_t	ir_freecount;	
	xfs_inofree_t	ir_free;	
} xfs_inobt_rec_incore_t;


typedef struct xfs_inobt_key {
	__be32		ir_startino;	
} xfs_inobt_key_t;

typedef __be32 xfs_inobt_ptr_t;

#define	XFS_IBT_BLOCK(mp)		((xfs_agblock_t)(XFS_CNT_BLOCK(mp) + 1))
#define	XFS_PREALLOC_BLOCKS(mp)		((xfs_agblock_t)(XFS_IBT_BLOCK(mp) + 1))

#define XFS_INOBT_BLOCK_LEN(mp)	XFS_BTREE_SBLOCK_LEN

#define XFS_INOBT_REC_ADDR(mp, block, index) \
	((xfs_inobt_rec_t *) \
		((char *)(block) + \
		 XFS_INOBT_BLOCK_LEN(mp) + \
		 (((index) - 1) * sizeof(xfs_inobt_rec_t))))

#define XFS_INOBT_KEY_ADDR(mp, block, index) \
	((xfs_inobt_key_t *) \
		((char *)(block) + \
		 XFS_INOBT_BLOCK_LEN(mp) + \
		 ((index) - 1) * sizeof(xfs_inobt_key_t)))

#define XFS_INOBT_PTR_ADDR(mp, block, index, maxrecs) \
	((xfs_inobt_ptr_t *) \
		((char *)(block) + \
		 XFS_INOBT_BLOCK_LEN(mp) + \
		 (maxrecs) * sizeof(xfs_inobt_key_t) + \
		 ((index) - 1) * sizeof(xfs_inobt_ptr_t)))

extern struct xfs_btree_cur *xfs_inobt_init_cursor(struct xfs_mount *,
		struct xfs_trans *, struct xfs_buf *, xfs_agnumber_t);
extern int xfs_inobt_maxrecs(struct xfs_mount *, int, int);

#endif	
