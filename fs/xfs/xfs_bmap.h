/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
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
#ifndef __XFS_BMAP_H__
#define	__XFS_BMAP_H__

struct getbmap;
struct xfs_bmbt_irec;
struct xfs_ifork;
struct xfs_inode;
struct xfs_mount;
struct xfs_trans;

extern kmem_zone_t	*xfs_bmap_free_item_zone;

typedef struct xfs_bmap_free_item
{
	xfs_fsblock_t		xbfi_startblock;
	xfs_extlen_t		xbfi_blockcount;
	struct xfs_bmap_free_item *xbfi_next;	
} xfs_bmap_free_item_t;

typedef	struct xfs_bmap_free
{
	xfs_bmap_free_item_t	*xbf_first;	
	int			xbf_count;	
	int			xbf_low;	
} xfs_bmap_free_t;

#define	XFS_BMAP_MAX_NMAP	4

#define XFS_BMAPI_ENTIRE	0x001	
#define XFS_BMAPI_METADATA	0x002	
#define XFS_BMAPI_ATTRFORK	0x004	
#define XFS_BMAPI_PREALLOC	0x008	/* preallocation op: unwritten space */
#define XFS_BMAPI_IGSTATE	0x010	
					
#define XFS_BMAPI_CONTIG	0x020	
/*
 * unwritten extent conversion - this needs write cache flushing and no additional
 * allocation alignments. When specified with XFS_BMAPI_PREALLOC it converts
 * from written to unwritten, otherwise convert from unwritten to written.
 */
#define XFS_BMAPI_CONVERT	0x040

#define XFS_BMAPI_FLAGS \
	{ XFS_BMAPI_ENTIRE,	"ENTIRE" }, \
	{ XFS_BMAPI_METADATA,	"METADATA" }, \
	{ XFS_BMAPI_ATTRFORK,	"ATTRFORK" }, \
	{ XFS_BMAPI_PREALLOC,	"PREALLOC" }, \
	{ XFS_BMAPI_IGSTATE,	"IGSTATE" }, \
	{ XFS_BMAPI_CONTIG,	"CONTIG" }, \
	{ XFS_BMAPI_CONVERT,	"CONVERT" }


static inline int xfs_bmapi_aflag(int w)
{
	return (w == XFS_ATTR_FORK ? XFS_BMAPI_ATTRFORK : 0);
}

#define	DELAYSTARTBLOCK		((xfs_fsblock_t)-1LL)
#define	HOLESTARTBLOCK		((xfs_fsblock_t)-2LL)

static inline void xfs_bmap_init(xfs_bmap_free_t *flp, xfs_fsblock_t *fbp)
{
	((flp)->xbf_first = NULL, (flp)->xbf_count = 0, \
		(flp)->xbf_low = 0, *(fbp) = NULLFSBLOCK);
}

typedef struct xfs_bmalloca {
	xfs_fsblock_t		*firstblock; 
	struct xfs_bmap_free	*flist;	
	struct xfs_trans	*tp;	
	struct xfs_inode	*ip;	
	struct xfs_bmbt_irec	prev;	
	struct xfs_bmbt_irec	got;	

	xfs_fileoff_t		offset;	
	xfs_extlen_t		length;	
	xfs_fsblock_t		blkno;	

	struct xfs_btree_cur	*cur;	
	xfs_extnum_t		idx;	
	int			nallocs;
	int			logflags;

	xfs_extlen_t		total;	
	xfs_extlen_t		minlen;	
	xfs_extlen_t		minleft; 
	char			eof;	
	char			wasdel;	
	char			userdata;
	char			aeof;	
	char			conv;	/* overwriting unwritten extents */
} xfs_bmalloca_t;

#define BMAP_LEFT_CONTIG	(1 << 0)
#define BMAP_RIGHT_CONTIG	(1 << 1)
#define BMAP_LEFT_FILLING	(1 << 2)
#define BMAP_RIGHT_FILLING	(1 << 3)
#define BMAP_LEFT_DELAY		(1 << 4)
#define BMAP_RIGHT_DELAY	(1 << 5)
#define BMAP_LEFT_VALID		(1 << 6)
#define BMAP_RIGHT_VALID	(1 << 7)
#define BMAP_ATTRFORK		(1 << 8)

#define XFS_BMAP_EXT_FLAGS \
	{ BMAP_LEFT_CONTIG,	"LC" }, \
	{ BMAP_RIGHT_CONTIG,	"RC" }, \
	{ BMAP_LEFT_FILLING,	"LF" }, \
	{ BMAP_RIGHT_FILLING,	"RF" }, \
	{ BMAP_ATTRFORK,	"ATTR" }

#if defined(__KERNEL) && defined(DEBUG)
void	xfs_bmap_trace_exlist(struct xfs_inode *ip, xfs_extnum_t cnt,
		int whichfork, unsigned long caller_ip);
#define	XFS_BMAP_TRACE_EXLIST(ip,c,w)	\
	xfs_bmap_trace_exlist(ip,c,w, _THIS_IP_)
#else
#define	XFS_BMAP_TRACE_EXLIST(ip,c,w)
#endif

int	xfs_bmap_add_attrfork(struct xfs_inode *ip, int size, int rsvd);
void	xfs_bmap_add_free(xfs_fsblock_t bno, xfs_filblks_t len,
		struct xfs_bmap_free *flist, struct xfs_mount *mp);
void	xfs_bmap_cancel(struct xfs_bmap_free *flist);
void	xfs_bmap_compute_maxlevels(struct xfs_mount *mp, int whichfork);
int	xfs_bmap_first_unused(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_extlen_t len, xfs_fileoff_t *unused, int whichfork);
int	xfs_bmap_last_before(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t *last_block, int whichfork);
int	xfs_bmap_last_offset(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t *unused, int whichfork);
int	xfs_bmap_one_block(struct xfs_inode *ip, int whichfork);
int	xfs_bmap_read_extents(struct xfs_trans *tp, struct xfs_inode *ip,
		int whichfork);
int	xfs_bmapi_read(struct xfs_inode *ip, xfs_fileoff_t bno,
		xfs_filblks_t len, struct xfs_bmbt_irec *mval,
		int *nmap, int flags);
int	xfs_bmapi_delay(struct xfs_inode *ip, xfs_fileoff_t bno,
		xfs_filblks_t len, struct xfs_bmbt_irec *mval,
		int *nmap, int flags);
int	xfs_bmapi_write(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t len, int flags,
		xfs_fsblock_t *firstblock, xfs_extlen_t total,
		struct xfs_bmbt_irec *mval, int *nmap,
		struct xfs_bmap_free *flist);
int	xfs_bunmapi(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t len, int flags,
		xfs_extnum_t nexts, xfs_fsblock_t *firstblock,
		struct xfs_bmap_free *flist, int *done);
int	xfs_check_nostate_extents(struct xfs_ifork *ifp, xfs_extnum_t idx,
		xfs_extnum_t num);
uint	xfs_default_attroffset(struct xfs_inode *ip);

#ifdef __KERNEL__
typedef int (*xfs_bmap_format_t)(void **, struct getbmapx *, int *);

int	xfs_bmap_finish(struct xfs_trans **tp, struct xfs_bmap_free *flist,
		int *committed);
int	xfs_getbmap(struct xfs_inode *ip, struct getbmapx *bmv,
		xfs_bmap_format_t formatter, void *arg);
int	xfs_bmap_eof(struct xfs_inode *ip, xfs_fileoff_t endoff,
		int whichfork, int *eof);
int	xfs_bmap_count_blocks(struct xfs_trans *tp, struct xfs_inode *ip,
		int whichfork, int *count);
int	xfs_bmap_punch_delalloc_range(struct xfs_inode *ip,
		xfs_fileoff_t start_fsb, xfs_fileoff_t length);
#endif	

#endif	
