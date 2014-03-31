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
#ifndef __XFS_IALLOC_H__
#define	__XFS_IALLOC_H__

struct xfs_buf;
struct xfs_dinode;
struct xfs_imap;
struct xfs_mount;
struct xfs_trans;

#define	XFS_IALLOC_INODES(mp)	(mp)->m_ialloc_inos
#define	XFS_IALLOC_BLOCKS(mp)	(mp)->m_ialloc_blks

#define	XFS_INODE_BIG_CLUSTER_SIZE	8192
#define	XFS_INODE_CLUSTER_SIZE(mp)	(mp)->m_inode_cluster_size

static inline struct xfs_dinode *
xfs_make_iptr(struct xfs_mount *mp, struct xfs_buf *b, int o)
{
	return (xfs_dinode_t *)
		(xfs_buf_offset(b, o << (mp)->m_sb.sb_inodelog));
}

static inline int xfs_ialloc_find_free(xfs_inofree_t *fp)
{
	return xfs_lowbit64(*fp);
}


int					
xfs_dialloc(
	struct xfs_trans *tp,		
	xfs_ino_t	parent,		
	umode_t		mode,		
	int		okalloc,	
	struct xfs_buf	**agbp,		
	boolean_t	*alloc_done,	
	xfs_ino_t	*inop);		

int					
xfs_difree(
	struct xfs_trans *tp,		
	xfs_ino_t	inode,		
	struct xfs_bmap_free *flist,	
	int		*delete,	
	xfs_ino_t	*first_ino);	

int
xfs_imap(
	struct xfs_mount *mp,		
	struct xfs_trans *tp,		
	xfs_ino_t	ino,		
	struct xfs_imap	*imap,		
	uint		flags);		

void
xfs_ialloc_compute_maxlevels(
	struct xfs_mount *mp);		

void
xfs_ialloc_log_agi(
	struct xfs_trans *tp,		
	struct xfs_buf	*bp,		
	int		fields);	

int					
xfs_ialloc_read_agi(
	struct xfs_mount *mp,		
	struct xfs_trans *tp,		
	xfs_agnumber_t	agno,		
	struct xfs_buf	**bpp);		

int
xfs_ialloc_pagi_init(
	struct xfs_mount *mp,		
	struct xfs_trans *tp,		
        xfs_agnumber_t  agno);		

int xfs_inobt_lookup(struct xfs_btree_cur *cur, xfs_agino_t ino,
		xfs_lookup_t dir, int *stat);

extern int xfs_inobt_get_rec(struct xfs_btree_cur *cur,
		xfs_inobt_rec_incore_t *rec, int *stat);

#endif	
