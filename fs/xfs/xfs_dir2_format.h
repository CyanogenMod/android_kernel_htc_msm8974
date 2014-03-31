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
#ifndef __XFS_DIR2_FORMAT_H__
#define __XFS_DIR2_FORMAT_H__


#define	XFS_DIR2_BLOCK_MAGIC	0x58443242	
#define	XFS_DIR2_DATA_MAGIC	0x58443244	
#define	XFS_DIR2_FREE_MAGIC	0x58443246	

typedef	__uint16_t	xfs_dir2_data_off_t;
#define	NULLDATAOFF	0xffffU
typedef uint		xfs_dir2_data_aoff_t;	

typedef struct { __uint8_t i[2]; } __arch_pack xfs_dir2_sf_off_t;

typedef	__uint32_t	xfs_dir2_dataptr_t;
#define	XFS_DIR2_MAX_DATAPTR	((xfs_dir2_dataptr_t)0xffffffff)
#define	XFS_DIR2_NULL_DATAPTR	((xfs_dir2_dataptr_t)0)

typedef	xfs_off_t	xfs_dir2_off_t;

typedef	__uint32_t	xfs_dir2_db_t;

typedef	struct { __uint8_t i[8]; } xfs_dir2_ino8_t;

typedef struct { __uint8_t i[4]; } xfs_dir2_ino4_t;

typedef union {
	xfs_dir2_ino8_t	i8;
	xfs_dir2_ino4_t	i4;
} xfs_dir2_inou_t;
#define	XFS_DIR2_MAX_SHORT_INUM	((xfs_ino_t)0xffffffffULL)

typedef struct xfs_dir2_sf_hdr {
	__uint8_t		count;		
	__uint8_t		i8count;	
	xfs_dir2_inou_t		parent;		
} __arch_pack xfs_dir2_sf_hdr_t;

typedef struct xfs_dir2_sf_entry {
	__u8			namelen;	
	xfs_dir2_sf_off_t	offset;		
	__u8			name[];		
} __arch_pack xfs_dir2_sf_entry_t;

static inline int xfs_dir2_sf_hdr_size(int i8count)
{
	return sizeof(struct xfs_dir2_sf_hdr) -
		(i8count == 0) *
		(sizeof(xfs_dir2_ino8_t) - sizeof(xfs_dir2_ino4_t));
}

static inline xfs_dir2_data_aoff_t
xfs_dir2_sf_get_offset(xfs_dir2_sf_entry_t *sfep)
{
	return get_unaligned_be16(&sfep->offset.i);
}

static inline void
xfs_dir2_sf_put_offset(xfs_dir2_sf_entry_t *sfep, xfs_dir2_data_aoff_t off)
{
	put_unaligned_be16(off, &sfep->offset.i);
}

static inline int
xfs_dir2_sf_entsize(struct xfs_dir2_sf_hdr *hdr, int len)
{
	return sizeof(struct xfs_dir2_sf_entry) +	
		len +					
		(hdr->i8count ?				
		 sizeof(xfs_dir2_ino8_t) :
		 sizeof(xfs_dir2_ino4_t));
}

static inline struct xfs_dir2_sf_entry *
xfs_dir2_sf_firstentry(struct xfs_dir2_sf_hdr *hdr)
{
	return (struct xfs_dir2_sf_entry *)
		((char *)hdr + xfs_dir2_sf_hdr_size(hdr->i8count));
}

static inline struct xfs_dir2_sf_entry *
xfs_dir2_sf_nextentry(struct xfs_dir2_sf_hdr *hdr,
		struct xfs_dir2_sf_entry *sfep)
{
	return (struct xfs_dir2_sf_entry *)
		((char *)sfep + xfs_dir2_sf_entsize(hdr, sfep->namelen));
}



#define	XFS_DIR2_DATA_ALIGN_LOG	3		
#define	XFS_DIR2_DATA_ALIGN	(1 << XFS_DIR2_DATA_ALIGN_LOG)
#define	XFS_DIR2_DATA_FREE_TAG	0xffff
#define	XFS_DIR2_DATA_FD_COUNT	3

#define	XFS_DIR2_SPACE_SIZE	(1ULL << (32 + XFS_DIR2_DATA_ALIGN_LOG))
#define	XFS_DIR2_DATA_SPACE	0
#define	XFS_DIR2_DATA_OFFSET	(XFS_DIR2_DATA_SPACE * XFS_DIR2_SPACE_SIZE)
#define	XFS_DIR2_DATA_FIRSTDB(mp)	\
	xfs_dir2_byte_to_db(mp, XFS_DIR2_DATA_OFFSET)

#define	XFS_DIR2_DATA_DOT_OFFSET	\
	((xfs_dir2_data_aoff_t)sizeof(struct xfs_dir2_data_hdr))
#define	XFS_DIR2_DATA_DOTDOT_OFFSET	\
	(XFS_DIR2_DATA_DOT_OFFSET + xfs_dir2_data_entsize(1))
#define	XFS_DIR2_DATA_FIRST_OFFSET		\
	(XFS_DIR2_DATA_DOTDOT_OFFSET + xfs_dir2_data_entsize(2))

typedef struct xfs_dir2_data_free {
	__be16			offset;		
	__be16			length;		
} xfs_dir2_data_free_t;

typedef struct xfs_dir2_data_hdr {
	__be32			magic;		
						
	xfs_dir2_data_free_t	bestfree[XFS_DIR2_DATA_FD_COUNT];
} xfs_dir2_data_hdr_t;

typedef struct xfs_dir2_data_entry {
	__be64			inumber;	
	__u8			namelen;	
	__u8			name[];		
     		
} xfs_dir2_data_entry_t;

typedef struct xfs_dir2_data_unused {
	__be16			freetag;	
	__be16			length;		
						
	__be16			tag;		
} xfs_dir2_data_unused_t;

static inline int xfs_dir2_data_entsize(int n)
{
	return (int)roundup(offsetof(struct xfs_dir2_data_entry, name[0]) + n +
		 (uint)sizeof(xfs_dir2_data_off_t), XFS_DIR2_DATA_ALIGN);
}

static inline __be16 *
xfs_dir2_data_entry_tag_p(struct xfs_dir2_data_entry *dep)
{
	return (__be16 *)((char *)dep +
		xfs_dir2_data_entsize(dep->namelen) - sizeof(__be16));
}

static inline __be16 *
xfs_dir2_data_unused_tag_p(struct xfs_dir2_data_unused *dup)
{
	return (__be16 *)((char *)dup +
			be16_to_cpu(dup->length) - sizeof(__be16));
}


#define	XFS_DIR2_LEAF_SPACE	1
#define	XFS_DIR2_LEAF_OFFSET	(XFS_DIR2_LEAF_SPACE * XFS_DIR2_SPACE_SIZE)
#define	XFS_DIR2_LEAF_FIRSTDB(mp)	\
	xfs_dir2_byte_to_db(mp, XFS_DIR2_LEAF_OFFSET)

typedef struct xfs_dir2_leaf_hdr {
	xfs_da_blkinfo_t	info;		
	__be16			count;		
	__be16			stale;		
} xfs_dir2_leaf_hdr_t;

typedef struct xfs_dir2_leaf_entry {
	__be32			hashval;	
	__be32			address;	
} xfs_dir2_leaf_entry_t;

typedef struct xfs_dir2_leaf_tail {
	__be32			bestcount;
} xfs_dir2_leaf_tail_t;

typedef struct xfs_dir2_leaf {
	xfs_dir2_leaf_hdr_t	hdr;		
	xfs_dir2_leaf_entry_t	ents[];		
} xfs_dir2_leaf_t;


static inline int xfs_dir2_max_leaf_ents(struct xfs_mount *mp)
{
	return (mp->m_dirblksize - (uint)sizeof(struct xfs_dir2_leaf_hdr)) /
		(uint)sizeof(struct xfs_dir2_leaf_entry);
}

static inline struct xfs_dir2_leaf_tail *
xfs_dir2_leaf_tail_p(struct xfs_mount *mp, struct xfs_dir2_leaf *lp)
{
	return (struct xfs_dir2_leaf_tail *)
		((char *)lp + mp->m_dirblksize -
		  sizeof(struct xfs_dir2_leaf_tail));
}

static inline __be16 *
xfs_dir2_leaf_bests_p(struct xfs_dir2_leaf_tail *ltp)
{
	return (__be16 *)ltp - be32_to_cpu(ltp->bestcount);
}

static inline xfs_dir2_off_t
xfs_dir2_dataptr_to_byte(struct xfs_mount *mp, xfs_dir2_dataptr_t dp)
{
	return (xfs_dir2_off_t)dp << XFS_DIR2_DATA_ALIGN_LOG;
}

static inline xfs_dir2_dataptr_t
xfs_dir2_byte_to_dataptr(struct xfs_mount *mp, xfs_dir2_off_t by)
{
	return (xfs_dir2_dataptr_t)(by >> XFS_DIR2_DATA_ALIGN_LOG);
}

static inline xfs_dir2_db_t
xfs_dir2_byte_to_db(struct xfs_mount *mp, xfs_dir2_off_t by)
{
	return (xfs_dir2_db_t)
		(by >> (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog));
}

static inline xfs_dir2_db_t
xfs_dir2_dataptr_to_db(struct xfs_mount *mp, xfs_dir2_dataptr_t dp)
{
	return xfs_dir2_byte_to_db(mp, xfs_dir2_dataptr_to_byte(mp, dp));
}

static inline xfs_dir2_data_aoff_t
xfs_dir2_byte_to_off(struct xfs_mount *mp, xfs_dir2_off_t by)
{
	return (xfs_dir2_data_aoff_t)(by &
		((1 << (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog)) - 1));
}

static inline xfs_dir2_data_aoff_t
xfs_dir2_dataptr_to_off(struct xfs_mount *mp, xfs_dir2_dataptr_t dp)
{
	return xfs_dir2_byte_to_off(mp, xfs_dir2_dataptr_to_byte(mp, dp));
}

static inline xfs_dir2_off_t
xfs_dir2_db_off_to_byte(struct xfs_mount *mp, xfs_dir2_db_t db,
			xfs_dir2_data_aoff_t o)
{
	return ((xfs_dir2_off_t)db <<
		(mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog)) + o;
}

static inline xfs_dablk_t
xfs_dir2_db_to_da(struct xfs_mount *mp, xfs_dir2_db_t db)
{
	return (xfs_dablk_t)(db << mp->m_sb.sb_dirblklog);
}

static inline xfs_dablk_t
xfs_dir2_byte_to_da(struct xfs_mount *mp, xfs_dir2_off_t by)
{
	return xfs_dir2_db_to_da(mp, xfs_dir2_byte_to_db(mp, by));
}

static inline xfs_dir2_dataptr_t
xfs_dir2_db_off_to_dataptr(struct xfs_mount *mp, xfs_dir2_db_t db,
			   xfs_dir2_data_aoff_t o)
{
	return xfs_dir2_byte_to_dataptr(mp, xfs_dir2_db_off_to_byte(mp, db, o));
}

static inline xfs_dir2_db_t
xfs_dir2_da_to_db(struct xfs_mount *mp, xfs_dablk_t da)
{
	return (xfs_dir2_db_t)(da >> mp->m_sb.sb_dirblklog);
}

static inline xfs_dir2_off_t
xfs_dir2_da_to_byte(struct xfs_mount *mp, xfs_dablk_t da)
{
	return xfs_dir2_db_off_to_byte(mp, xfs_dir2_da_to_db(mp, da), 0);
}


#define	XFS_DIR2_FREE_SPACE	2
#define	XFS_DIR2_FREE_OFFSET	(XFS_DIR2_FREE_SPACE * XFS_DIR2_SPACE_SIZE)
#define	XFS_DIR2_FREE_FIRSTDB(mp)	\
	xfs_dir2_byte_to_db(mp, XFS_DIR2_FREE_OFFSET)

typedef	struct xfs_dir2_free_hdr {
	__be32			magic;		
	__be32			firstdb;	
	__be32			nvalid;		
	__be32			nused;		
} xfs_dir2_free_hdr_t;

typedef struct xfs_dir2_free {
	xfs_dir2_free_hdr_t	hdr;		
	__be16			bests[];	
						
} xfs_dir2_free_t;

static inline int xfs_dir2_free_max_bests(struct xfs_mount *mp)
{
	return (mp->m_dirblksize - sizeof(struct xfs_dir2_free_hdr)) /
		sizeof(xfs_dir2_data_off_t);
}

static inline xfs_dir2_db_t
xfs_dir2_db_to_fdb(struct xfs_mount *mp, xfs_dir2_db_t db)
{
	return XFS_DIR2_FREE_FIRSTDB(mp) + db / xfs_dir2_free_max_bests(mp);
}

static inline int
xfs_dir2_db_to_fdindex(struct xfs_mount *mp, xfs_dir2_db_t db)
{
	return db % xfs_dir2_free_max_bests(mp);
}


typedef struct xfs_dir2_block_tail {
	__be32		count;			
	__be32		stale;			
} xfs_dir2_block_tail_t;

static inline struct xfs_dir2_block_tail *
xfs_dir2_block_tail_p(struct xfs_mount *mp, struct xfs_dir2_data_hdr *hdr)
{
	return ((struct xfs_dir2_block_tail *)
		((char *)hdr + mp->m_dirblksize)) - 1;
}

static inline struct xfs_dir2_leaf_entry *
xfs_dir2_block_leaf_p(struct xfs_dir2_block_tail *btp)
{
	return ((struct xfs_dir2_leaf_entry *)btp) - be32_to_cpu(btp->count);
}

#endif 
