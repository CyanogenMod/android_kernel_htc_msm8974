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
#ifndef __XFS_ALLOC_BTREE_H__
#define	__XFS_ALLOC_BTREE_H__


struct xfs_buf;
struct xfs_btree_cur;
struct xfs_mount;

#define	XFS_ABTB_MAGIC	0x41425442	
#define	XFS_ABTC_MAGIC	0x41425443	

typedef struct xfs_alloc_rec {
	__be32		ar_startblock;	
	__be32		ar_blockcount;	
} xfs_alloc_rec_t, xfs_alloc_key_t;

typedef struct xfs_alloc_rec_incore {
	xfs_agblock_t	ar_startblock;	
	xfs_extlen_t	ar_blockcount;	
} xfs_alloc_rec_incore_t;

typedef __be32 xfs_alloc_ptr_t;

#define XFS_MIN_BLOCKSIZE_LOG	9	
#define XFS_MAX_BLOCKSIZE_LOG	16	
#define XFS_MIN_BLOCKSIZE	(1 << XFS_MIN_BLOCKSIZE_LOG)
#define XFS_MAX_BLOCKSIZE	(1 << XFS_MAX_BLOCKSIZE_LOG)
#define XFS_MIN_SECTORSIZE_LOG	9	
#define XFS_MAX_SECTORSIZE_LOG	15	
#define XFS_MIN_SECTORSIZE	(1 << XFS_MIN_SECTORSIZE_LOG)
#define XFS_MAX_SECTORSIZE	(1 << XFS_MAX_SECTORSIZE_LOG)

#define	XFS_BNO_BLOCK(mp)	((xfs_agblock_t)(XFS_AGFL_BLOCK(mp) + 1))
#define	XFS_CNT_BLOCK(mp)	((xfs_agblock_t)(XFS_BNO_BLOCK(mp) + 1))

#define XFS_ALLOC_BLOCK_LEN(mp)	XFS_BTREE_SBLOCK_LEN

#define XFS_ALLOC_REC_ADDR(mp, block, index) \
	((xfs_alloc_rec_t *) \
		((char *)(block) + \
		 XFS_ALLOC_BLOCK_LEN(mp) + \
		 (((index) - 1) * sizeof(xfs_alloc_rec_t))))

#define XFS_ALLOC_KEY_ADDR(mp, block, index) \
	((xfs_alloc_key_t *) \
		((char *)(block) + \
		 XFS_ALLOC_BLOCK_LEN(mp) + \
		 ((index) - 1) * sizeof(xfs_alloc_key_t)))

#define XFS_ALLOC_PTR_ADDR(mp, block, index, maxrecs) \
	((xfs_alloc_ptr_t *) \
		((char *)(block) + \
		 XFS_ALLOC_BLOCK_LEN(mp) + \
		 (maxrecs) * sizeof(xfs_alloc_key_t) + \
		 ((index) - 1) * sizeof(xfs_alloc_ptr_t)))

extern struct xfs_btree_cur *xfs_allocbt_init_cursor(struct xfs_mount *,
		struct xfs_trans *, struct xfs_buf *,
		xfs_agnumber_t, xfs_btnum_t);
extern int xfs_allocbt_maxrecs(struct xfs_mount *, int, int);

#endif	
