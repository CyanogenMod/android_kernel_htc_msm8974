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
#ifndef	__XFS_INODE_ITEM_H__
#define	__XFS_INODE_ITEM_H__

typedef struct xfs_inode_log_format {
	__uint16_t		ilf_type;	
	__uint16_t		ilf_size;	
	__uint32_t		ilf_fields;	
	__uint16_t		ilf_asize;	
	__uint16_t		ilf_dsize;	
	__uint64_t		ilf_ino;	
	union {
		__uint32_t	ilfu_rdev;	
		uuid_t		ilfu_uuid;	
	} ilf_u;
	__int64_t		ilf_blkno;	
	__int32_t		ilf_len;	
	__int32_t		ilf_boffset;	
} xfs_inode_log_format_t;

typedef struct xfs_inode_log_format_32 {
	__uint16_t		ilf_type;	
	__uint16_t		ilf_size;	
	__uint32_t		ilf_fields;	
	__uint16_t		ilf_asize;	
	__uint16_t		ilf_dsize;	
	__uint64_t		ilf_ino;	
	union {
		__uint32_t	ilfu_rdev;	
		uuid_t		ilfu_uuid;	
	} ilf_u;
	__int64_t		ilf_blkno;	
	__int32_t		ilf_len;	
	__int32_t		ilf_boffset;	
} __attribute__((packed)) xfs_inode_log_format_32_t;

typedef struct xfs_inode_log_format_64 {
	__uint16_t		ilf_type;	
	__uint16_t		ilf_size;	
	__uint32_t		ilf_fields;	
	__uint16_t		ilf_asize;	
	__uint16_t		ilf_dsize;	
	__uint32_t		ilf_pad;	
	__uint64_t		ilf_ino;	
	union {
		__uint32_t	ilfu_rdev;	
		uuid_t		ilfu_uuid;	
	} ilf_u;
	__int64_t		ilf_blkno;	
	__int32_t		ilf_len;	
	__int32_t		ilf_boffset;	
} xfs_inode_log_format_64_t;

#define	XFS_ILOG_CORE	0x001	
#define	XFS_ILOG_DDATA	0x002	
#define	XFS_ILOG_DEXT	0x004	
#define	XFS_ILOG_DBROOT	0x008	
#define	XFS_ILOG_DEV	0x010	
#define	XFS_ILOG_UUID	0x020	
#define	XFS_ILOG_ADATA	0x040	
#define	XFS_ILOG_AEXT	0x080	
#define	XFS_ILOG_ABROOT	0x100	


#define XFS_ILOG_TIMESTAMP	0x4000

#define	XFS_ILOG_NONCORE	(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT | XFS_ILOG_DEV | \
				 XFS_ILOG_UUID | XFS_ILOG_ADATA | \
				 XFS_ILOG_AEXT | XFS_ILOG_ABROOT)

#define	XFS_ILOG_DFORK		(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT)

#define	XFS_ILOG_AFORK		(XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT)

#define	XFS_ILOG_ALL		(XFS_ILOG_CORE | XFS_ILOG_DDATA | \
				 XFS_ILOG_DEXT | XFS_ILOG_DBROOT | \
				 XFS_ILOG_DEV | XFS_ILOG_UUID | \
				 XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT | XFS_ILOG_TIMESTAMP)

static inline int xfs_ilog_fbroot(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DBROOT : XFS_ILOG_ABROOT);
}

static inline int xfs_ilog_fext(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DEXT : XFS_ILOG_AEXT);
}

static inline int xfs_ilog_fdata(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DDATA : XFS_ILOG_ADATA);
}

#ifdef __KERNEL__

struct xfs_buf;
struct xfs_bmbt_rec;
struct xfs_inode;
struct xfs_mount;


typedef struct xfs_inode_log_item {
	xfs_log_item_t		ili_item;	   
	struct xfs_inode	*ili_inode;	   
	xfs_lsn_t		ili_flush_lsn;	   
	xfs_lsn_t		ili_last_lsn;	   
	unsigned short		ili_lock_flags;	   
	unsigned short		ili_logged;	   
	unsigned int		ili_last_fields;   
	unsigned int		ili_fields;	   
	struct xfs_bmbt_rec	*ili_extents_buf;  
	struct xfs_bmbt_rec	*ili_aextents_buf; 
#ifdef XFS_TRANS_DEBUG
	int			ili_root_size;
	char			*ili_orig_root;
#endif
	xfs_inode_log_format_t	ili_format;	   
} xfs_inode_log_item_t;


static inline int xfs_inode_clean(xfs_inode_t *ip)
{
	return !ip->i_itemp || !(ip->i_itemp->ili_fields & XFS_ILOG_ALL);
}

extern void xfs_inode_item_init(struct xfs_inode *, struct xfs_mount *);
extern void xfs_inode_item_destroy(struct xfs_inode *);
extern void xfs_iflush_done(struct xfs_buf *, struct xfs_log_item *);
extern void xfs_istale_done(struct xfs_buf *, struct xfs_log_item *);
extern void xfs_iflush_abort(struct xfs_inode *);
extern int xfs_inode_item_format_convert(xfs_log_iovec_t *,
					 xfs_inode_log_format_t *);

#endif	

#endif	
