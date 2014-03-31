/*
 * Copyright (c) 2000,2002,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_DINODE_H__
#define	__XFS_DINODE_H__

#define	XFS_DINODE_MAGIC		0x494e	
#define XFS_DINODE_GOOD_VERSION(v)	(((v) == 1 || (v) == 2))

typedef struct xfs_timestamp {
	__be32		t_sec;		
	__be32		t_nsec;		
} xfs_timestamp_t;

typedef struct xfs_dinode {
	__be16		di_magic;	
	__be16		di_mode;	
	__u8		di_version;	
	__u8		di_format;	
	__be16		di_onlink;	
	__be32		di_uid;		
	__be32		di_gid;		
	__be32		di_nlink;	
	__be16		di_projid_lo;	
	__be16		di_projid_hi;	
	__u8		di_pad[6];	
	__be16		di_flushiter;	
	xfs_timestamp_t	di_atime;	
	xfs_timestamp_t	di_mtime;	
	xfs_timestamp_t	di_ctime;	
	__be64		di_size;	
	__be64		di_nblocks;	
	__be32		di_extsize;	
	__be32		di_nextents;	
	__be16		di_anextents;	
	__u8		di_forkoff;	
	__s8		di_aformat;	
	__be32		di_dmevmask;	
	__be16		di_dmstate;	
	__be16		di_flags;	
	__be32		di_gen;		

	
	__be32		di_next_unlinked;
} __attribute__((packed)) xfs_dinode_t;

#define DI_MAX_FLUSH 0xffff

#define	XFS_MAXLINK		((1U << 31) - 1U)
#define	XFS_MAXLINK_1		65535U

typedef enum xfs_dinode_fmt {
	XFS_DINODE_FMT_DEV,		
	XFS_DINODE_FMT_LOCAL,		
	XFS_DINODE_FMT_EXTENTS,		
	XFS_DINODE_FMT_BTREE,		
	XFS_DINODE_FMT_UUID		
} xfs_dinode_fmt_t;

#define	XFS_DINODE_MIN_LOG	8
#define	XFS_DINODE_MAX_LOG	11
#define	XFS_DINODE_MIN_SIZE	(1 << XFS_DINODE_MIN_LOG)
#define	XFS_DINODE_MAX_SIZE	(1 << XFS_DINODE_MAX_LOG)

#define XFS_LITINO(mp) \
	((int)(((mp)->m_sb.sb_inodesize) - sizeof(struct xfs_dinode)))

#define	XFS_BROOT_SIZE_ADJ	\
	(XFS_BTREE_LBLOCK_LEN - sizeof(xfs_bmdr_block_t))

#define XFS_DFORK_Q(dip)		((dip)->di_forkoff != 0)
#define XFS_DFORK_BOFF(dip)		((int)((dip)->di_forkoff << 3))

#define XFS_DFORK_DSIZE(dip,mp) \
	(XFS_DFORK_Q(dip) ? \
		XFS_DFORK_BOFF(dip) : \
		XFS_LITINO(mp))
#define XFS_DFORK_ASIZE(dip,mp) \
	(XFS_DFORK_Q(dip) ? \
		XFS_LITINO(mp) - XFS_DFORK_BOFF(dip) : \
		0)
#define XFS_DFORK_SIZE(dip,mp,w) \
	((w) == XFS_DATA_FORK ? \
		XFS_DFORK_DSIZE(dip, mp) : \
		XFS_DFORK_ASIZE(dip, mp))

#define XFS_DFORK_DPTR(dip) \
	((char *)(dip) + sizeof(struct xfs_dinode))
#define XFS_DFORK_APTR(dip)	\
	(XFS_DFORK_DPTR(dip) + XFS_DFORK_BOFF(dip))
#define XFS_DFORK_PTR(dip,w)	\
	((w) == XFS_DATA_FORK ? XFS_DFORK_DPTR(dip) : XFS_DFORK_APTR(dip))

#define XFS_DFORK_FORMAT(dip,w) \
	((w) == XFS_DATA_FORK ? \
		(dip)->di_format : \
		(dip)->di_aformat)
#define XFS_DFORK_NEXTENTS(dip,w) \
	((w) == XFS_DATA_FORK ? \
		be32_to_cpu((dip)->di_nextents) : \
		be16_to_cpu((dip)->di_anextents))

#define	XFS_BUF_TO_DINODE(bp)	((xfs_dinode_t *)((bp)->b_addr))

static inline xfs_dev_t xfs_dinode_get_rdev(struct xfs_dinode *dip)
{
	return be32_to_cpu(*(__be32 *)XFS_DFORK_DPTR(dip));
}

static inline void xfs_dinode_put_rdev(struct xfs_dinode *dip, xfs_dev_t rdev)
{
	*(__be32 *)XFS_DFORK_DPTR(dip) = cpu_to_be32(rdev);
}

#define XFS_DIFLAG_REALTIME_BIT  0	
#define XFS_DIFLAG_PREALLOC_BIT  1	
#define XFS_DIFLAG_NEWRTBM_BIT   2	
#define XFS_DIFLAG_IMMUTABLE_BIT 3	
#define XFS_DIFLAG_APPEND_BIT    4	
#define XFS_DIFLAG_SYNC_BIT      5	/* inode is written synchronously */
#define XFS_DIFLAG_NOATIME_BIT   6	
#define XFS_DIFLAG_NODUMP_BIT    7	
#define XFS_DIFLAG_RTINHERIT_BIT 8	
#define XFS_DIFLAG_PROJINHERIT_BIT   9	
#define XFS_DIFLAG_NOSYMLINKS_BIT   10	
#define XFS_DIFLAG_EXTSIZE_BIT      11	
#define XFS_DIFLAG_EXTSZINHERIT_BIT 12	
#define XFS_DIFLAG_NODEFRAG_BIT     13	
#define XFS_DIFLAG_FILESTREAM_BIT   14  
#define XFS_DIFLAG_REALTIME      (1 << XFS_DIFLAG_REALTIME_BIT)
#define XFS_DIFLAG_PREALLOC      (1 << XFS_DIFLAG_PREALLOC_BIT)
#define XFS_DIFLAG_NEWRTBM       (1 << XFS_DIFLAG_NEWRTBM_BIT)
#define XFS_DIFLAG_IMMUTABLE     (1 << XFS_DIFLAG_IMMUTABLE_BIT)
#define XFS_DIFLAG_APPEND        (1 << XFS_DIFLAG_APPEND_BIT)
#define XFS_DIFLAG_SYNC          (1 << XFS_DIFLAG_SYNC_BIT)
#define XFS_DIFLAG_NOATIME       (1 << XFS_DIFLAG_NOATIME_BIT)
#define XFS_DIFLAG_NODUMP        (1 << XFS_DIFLAG_NODUMP_BIT)
#define XFS_DIFLAG_RTINHERIT     (1 << XFS_DIFLAG_RTINHERIT_BIT)
#define XFS_DIFLAG_PROJINHERIT   (1 << XFS_DIFLAG_PROJINHERIT_BIT)
#define XFS_DIFLAG_NOSYMLINKS    (1 << XFS_DIFLAG_NOSYMLINKS_BIT)
#define XFS_DIFLAG_EXTSIZE       (1 << XFS_DIFLAG_EXTSIZE_BIT)
#define XFS_DIFLAG_EXTSZINHERIT  (1 << XFS_DIFLAG_EXTSZINHERIT_BIT)
#define XFS_DIFLAG_NODEFRAG      (1 << XFS_DIFLAG_NODEFRAG_BIT)
#define XFS_DIFLAG_FILESTREAM    (1 << XFS_DIFLAG_FILESTREAM_BIT)

#ifdef CONFIG_XFS_RT
#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)
#else
#define XFS_IS_REALTIME_INODE(ip) (0)
#endif

#define XFS_DIFLAG_ANY \
	(XFS_DIFLAG_REALTIME | XFS_DIFLAG_PREALLOC | XFS_DIFLAG_NEWRTBM | \
	 XFS_DIFLAG_IMMUTABLE | XFS_DIFLAG_APPEND | XFS_DIFLAG_SYNC | \
	 XFS_DIFLAG_NOATIME | XFS_DIFLAG_NODUMP | XFS_DIFLAG_RTINHERIT | \
	 XFS_DIFLAG_PROJINHERIT | XFS_DIFLAG_NOSYMLINKS | XFS_DIFLAG_EXTSIZE | \
	 XFS_DIFLAG_EXTSZINHERIT | XFS_DIFLAG_NODEFRAG | XFS_DIFLAG_FILESTREAM)

#endif	
