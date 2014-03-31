/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#ifndef __XFS_SB_H__
#define	__XFS_SB_H__


struct xfs_buf;
struct xfs_mount;

#define	XFS_SB_MAGIC		0x58465342	
#define	XFS_SB_VERSION_1	1		
#define	XFS_SB_VERSION_2	2		
#define	XFS_SB_VERSION_3	3		
#define	XFS_SB_VERSION_4	4		
#define	XFS_SB_VERSION_NUMBITS		0x000f
#define	XFS_SB_VERSION_ALLFBITS		0xfff0
#define	XFS_SB_VERSION_SASHFBITS	0xf000
#define	XFS_SB_VERSION_REALFBITS	0x0ff0
#define	XFS_SB_VERSION_ATTRBIT		0x0010
#define	XFS_SB_VERSION_NLINKBIT		0x0020
#define	XFS_SB_VERSION_QUOTABIT		0x0040
#define	XFS_SB_VERSION_ALIGNBIT		0x0080
#define	XFS_SB_VERSION_DALIGNBIT	0x0100
#define	XFS_SB_VERSION_SHAREDBIT	0x0200
#define XFS_SB_VERSION_LOGV2BIT		0x0400
#define XFS_SB_VERSION_SECTORBIT	0x0800
#define	XFS_SB_VERSION_EXTFLGBIT	0x1000
#define	XFS_SB_VERSION_DIRV2BIT		0x2000
#define	XFS_SB_VERSION_BORGBIT		0x4000	
#define	XFS_SB_VERSION_MOREBITSBIT	0x8000
#define	XFS_SB_VERSION_OKSASHFBITS	\
	(XFS_SB_VERSION_EXTFLGBIT | \
	 XFS_SB_VERSION_DIRV2BIT | \
	 XFS_SB_VERSION_BORGBIT)
#define	XFS_SB_VERSION_OKREALFBITS	\
	(XFS_SB_VERSION_ATTRBIT | \
	 XFS_SB_VERSION_NLINKBIT | \
	 XFS_SB_VERSION_QUOTABIT | \
	 XFS_SB_VERSION_ALIGNBIT | \
	 XFS_SB_VERSION_DALIGNBIT | \
	 XFS_SB_VERSION_SHAREDBIT | \
	 XFS_SB_VERSION_LOGV2BIT | \
	 XFS_SB_VERSION_SECTORBIT | \
	 XFS_SB_VERSION_MOREBITSBIT)
#define	XFS_SB_VERSION_OKREALBITS	\
	(XFS_SB_VERSION_NUMBITS | \
	 XFS_SB_VERSION_OKREALFBITS | \
	 XFS_SB_VERSION_OKSASHFBITS)

#define XFS_SB_VERSION2_REALFBITS	0x00ffffff	
#define XFS_SB_VERSION2_RESERVED1BIT	0x00000001
#define XFS_SB_VERSION2_LAZYSBCOUNTBIT	0x00000002	
#define XFS_SB_VERSION2_RESERVED4BIT	0x00000004
#define XFS_SB_VERSION2_ATTR2BIT	0x00000008	
#define XFS_SB_VERSION2_PARENTBIT	0x00000010	
#define XFS_SB_VERSION2_PROJID32BIT	0x00000080	

#define	XFS_SB_VERSION2_OKREALFBITS	\
	(XFS_SB_VERSION2_LAZYSBCOUNTBIT	| \
	 XFS_SB_VERSION2_ATTR2BIT	| \
	 XFS_SB_VERSION2_PROJID32BIT)
#define	XFS_SB_VERSION2_OKSASHFBITS	\
	(0)
#define XFS_SB_VERSION2_OKREALBITS	\
	(XFS_SB_VERSION2_OKREALFBITS |	\
	 XFS_SB_VERSION2_OKSASHFBITS )

typedef struct xfs_sb {
	__uint32_t	sb_magicnum;	
	__uint32_t	sb_blocksize;	
	xfs_drfsbno_t	sb_dblocks;	
	xfs_drfsbno_t	sb_rblocks;	
	xfs_drtbno_t	sb_rextents;	
	uuid_t		sb_uuid;	
	xfs_dfsbno_t	sb_logstart;	
	xfs_ino_t	sb_rootino;	
	xfs_ino_t	sb_rbmino;	
	xfs_ino_t	sb_rsumino;	
	xfs_agblock_t	sb_rextsize;	
	xfs_agblock_t	sb_agblocks;	
	xfs_agnumber_t	sb_agcount;	
	xfs_extlen_t	sb_rbmblocks;	
	xfs_extlen_t	sb_logblocks;	
	__uint16_t	sb_versionnum;	
	__uint16_t	sb_sectsize;	
	__uint16_t	sb_inodesize;	
	__uint16_t	sb_inopblock;	
	char		sb_fname[12];	
	__uint8_t	sb_blocklog;	
	__uint8_t	sb_sectlog;	
	__uint8_t	sb_inodelog;	
	__uint8_t	sb_inopblog;	
	__uint8_t	sb_agblklog;	
	__uint8_t	sb_rextslog;	
	__uint8_t	sb_inprogress;	
	__uint8_t	sb_imax_pct;	
					
	__uint64_t	sb_icount;	
	__uint64_t	sb_ifree;	
	__uint64_t	sb_fdblocks;	
	__uint64_t	sb_frextents;	
	xfs_ino_t	sb_uquotino;	
	xfs_ino_t	sb_gquotino;	
	__uint16_t	sb_qflags;	
	__uint8_t	sb_flags;	
	__uint8_t	sb_shared_vn;	
	xfs_extlen_t	sb_inoalignmt;	
	__uint32_t	sb_unit;	
	__uint32_t	sb_width;	
	__uint8_t	sb_dirblklog;	
	__uint8_t	sb_logsectlog;	
	__uint16_t	sb_logsectsize;	
	__uint32_t	sb_logsunit;	
	__uint32_t	sb_features2;	

	__uint32_t	sb_bad_features2;

	
} xfs_sb_t;

typedef struct xfs_dsb {
	__be32		sb_magicnum;	
	__be32		sb_blocksize;	
	__be64		sb_dblocks;	
	__be64		sb_rblocks;	
	__be64		sb_rextents;	
	uuid_t		sb_uuid;	
	__be64		sb_logstart;	
	__be64		sb_rootino;	
	__be64		sb_rbmino;	
	__be64		sb_rsumino;	
	__be32		sb_rextsize;	
	__be32		sb_agblocks;	
	__be32		sb_agcount;	
	__be32		sb_rbmblocks;	
	__be32		sb_logblocks;	
	__be16		sb_versionnum;	
	__be16		sb_sectsize;	
	__be16		sb_inodesize;	
	__be16		sb_inopblock;	
	char		sb_fname[12];	
	__u8		sb_blocklog;	
	__u8		sb_sectlog;	
	__u8		sb_inodelog;	
	__u8		sb_inopblog;	
	__u8		sb_agblklog;	
	__u8		sb_rextslog;	
	__u8		sb_inprogress;	
	__u8		sb_imax_pct;	
					
	__be64		sb_icount;	
	__be64		sb_ifree;	
	__be64		sb_fdblocks;	
	__be64		sb_frextents;	
	__be64		sb_uquotino;	
	__be64		sb_gquotino;	
	__be16		sb_qflags;	
	__u8		sb_flags;	
	__u8		sb_shared_vn;	
	__be32		sb_inoalignmt;	
	__be32		sb_unit;	
	__be32		sb_width;	
	__u8		sb_dirblklog;	
	__u8		sb_logsectlog;	
	__be16		sb_logsectsize;	
	__be32		sb_logsunit;	
	__be32		sb_features2;	
	__be32	sb_bad_features2;

	
} xfs_dsb_t;

typedef enum {
	XFS_SBS_MAGICNUM, XFS_SBS_BLOCKSIZE, XFS_SBS_DBLOCKS, XFS_SBS_RBLOCKS,
	XFS_SBS_REXTENTS, XFS_SBS_UUID, XFS_SBS_LOGSTART, XFS_SBS_ROOTINO,
	XFS_SBS_RBMINO, XFS_SBS_RSUMINO, XFS_SBS_REXTSIZE, XFS_SBS_AGBLOCKS,
	XFS_SBS_AGCOUNT, XFS_SBS_RBMBLOCKS, XFS_SBS_LOGBLOCKS,
	XFS_SBS_VERSIONNUM, XFS_SBS_SECTSIZE, XFS_SBS_INODESIZE,
	XFS_SBS_INOPBLOCK, XFS_SBS_FNAME, XFS_SBS_BLOCKLOG,
	XFS_SBS_SECTLOG, XFS_SBS_INODELOG, XFS_SBS_INOPBLOG, XFS_SBS_AGBLKLOG,
	XFS_SBS_REXTSLOG, XFS_SBS_INPROGRESS, XFS_SBS_IMAX_PCT, XFS_SBS_ICOUNT,
	XFS_SBS_IFREE, XFS_SBS_FDBLOCKS, XFS_SBS_FREXTENTS, XFS_SBS_UQUOTINO,
	XFS_SBS_GQUOTINO, XFS_SBS_QFLAGS, XFS_SBS_FLAGS, XFS_SBS_SHARED_VN,
	XFS_SBS_INOALIGNMT, XFS_SBS_UNIT, XFS_SBS_WIDTH, XFS_SBS_DIRBLKLOG,
	XFS_SBS_LOGSECTLOG, XFS_SBS_LOGSECTSIZE, XFS_SBS_LOGSUNIT,
	XFS_SBS_FEATURES2, XFS_SBS_BAD_FEATURES2,
	XFS_SBS_FIELDCOUNT
} xfs_sb_field_t;

#define	XFS_SB_MVAL(x)		(1LL << XFS_SBS_ ## x)
#define	XFS_SB_UUID		XFS_SB_MVAL(UUID)
#define	XFS_SB_FNAME		XFS_SB_MVAL(FNAME)
#define	XFS_SB_ROOTINO		XFS_SB_MVAL(ROOTINO)
#define	XFS_SB_RBMINO		XFS_SB_MVAL(RBMINO)
#define	XFS_SB_RSUMINO		XFS_SB_MVAL(RSUMINO)
#define	XFS_SB_VERSIONNUM	XFS_SB_MVAL(VERSIONNUM)
#define XFS_SB_UQUOTINO		XFS_SB_MVAL(UQUOTINO)
#define XFS_SB_GQUOTINO		XFS_SB_MVAL(GQUOTINO)
#define XFS_SB_QFLAGS		XFS_SB_MVAL(QFLAGS)
#define XFS_SB_SHARED_VN	XFS_SB_MVAL(SHARED_VN)
#define XFS_SB_UNIT		XFS_SB_MVAL(UNIT)
#define XFS_SB_WIDTH		XFS_SB_MVAL(WIDTH)
#define XFS_SB_ICOUNT		XFS_SB_MVAL(ICOUNT)
#define XFS_SB_IFREE		XFS_SB_MVAL(IFREE)
#define XFS_SB_FDBLOCKS		XFS_SB_MVAL(FDBLOCKS)
#define XFS_SB_FEATURES2	XFS_SB_MVAL(FEATURES2)
#define XFS_SB_BAD_FEATURES2	XFS_SB_MVAL(BAD_FEATURES2)
#define	XFS_SB_NUM_BITS		((int)XFS_SBS_FIELDCOUNT)
#define	XFS_SB_ALL_BITS		((1LL << XFS_SB_NUM_BITS) - 1)
#define	XFS_SB_MOD_BITS		\
	(XFS_SB_UUID | XFS_SB_ROOTINO | XFS_SB_RBMINO | XFS_SB_RSUMINO | \
	 XFS_SB_VERSIONNUM | XFS_SB_UQUOTINO | XFS_SB_GQUOTINO | \
	 XFS_SB_QFLAGS | XFS_SB_SHARED_VN | XFS_SB_UNIT | XFS_SB_WIDTH | \
	 XFS_SB_ICOUNT | XFS_SB_IFREE | XFS_SB_FDBLOCKS | XFS_SB_FEATURES2 | \
	 XFS_SB_BAD_FEATURES2)


#define XFS_SBF_NOFLAGS		0x00	
#define XFS_SBF_READONLY	0x01	

#define XFS_SB_MAX_SHARED_VN	0

#define	XFS_SB_VERSION_NUM(sbp)	((sbp)->sb_versionnum & XFS_SB_VERSION_NUMBITS)

static inline int xfs_sb_good_version(xfs_sb_t *sbp)
{
	
	if (sbp->sb_versionnum >= XFS_SB_VERSION_1 &&
	    sbp->sb_versionnum <= XFS_SB_VERSION_3)
		return 1;

	
	if (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4) {
		if ((sbp->sb_versionnum & ~XFS_SB_VERSION_OKREALBITS) ||
		    ((sbp->sb_versionnum & XFS_SB_VERSION_MOREBITSBIT) &&
		     (sbp->sb_features2 & ~XFS_SB_VERSION2_OKREALBITS)))
			return 0;

#ifdef __KERNEL__
		if (sbp->sb_shared_vn > XFS_SB_MAX_SHARED_VN)
			return 0;
#else
		if ((sbp->sb_versionnum & XFS_SB_VERSION_SHAREDBIT) &&
		    sbp->sb_shared_vn > XFS_SB_MAX_SHARED_VN)
			return 0;
#endif

		return 1;
	}

	return 0;
}

static inline int xfs_sb_has_mismatched_features2(xfs_sb_t *sbp)
{
	return (sbp->sb_bad_features2 != sbp->sb_features2);
}

static inline unsigned xfs_sb_version_tonew(unsigned v)
{
	if (v == XFS_SB_VERSION_1)
		return XFS_SB_VERSION_4;

	if (v == XFS_SB_VERSION_2)
		return XFS_SB_VERSION_4 | XFS_SB_VERSION_ATTRBIT;

	return XFS_SB_VERSION_4 | XFS_SB_VERSION_ATTRBIT |
		XFS_SB_VERSION_NLINKBIT;
}

static inline unsigned xfs_sb_version_toold(unsigned v)
{
	if (v & (XFS_SB_VERSION_QUOTABIT | XFS_SB_VERSION_ALIGNBIT))
		return 0;
	if (v & XFS_SB_VERSION_NLINKBIT)
		return XFS_SB_VERSION_3;
	if (v & XFS_SB_VERSION_ATTRBIT)
		return XFS_SB_VERSION_2;
	return XFS_SB_VERSION_1;
}

static inline int xfs_sb_version_hasattr(xfs_sb_t *sbp)
{
	return sbp->sb_versionnum == XFS_SB_VERSION_2 ||
		sbp->sb_versionnum == XFS_SB_VERSION_3 ||
		(XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		 (sbp->sb_versionnum & XFS_SB_VERSION_ATTRBIT));
}

static inline void xfs_sb_version_addattr(xfs_sb_t *sbp)
{
	if (sbp->sb_versionnum == XFS_SB_VERSION_1)
		sbp->sb_versionnum = XFS_SB_VERSION_2;
	else if (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4)
		sbp->sb_versionnum |= XFS_SB_VERSION_ATTRBIT;
	else
		sbp->sb_versionnum = XFS_SB_VERSION_4 | XFS_SB_VERSION_ATTRBIT;
}

static inline int xfs_sb_version_hasnlink(xfs_sb_t *sbp)
{
	return sbp->sb_versionnum == XFS_SB_VERSION_3 ||
		 (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		  (sbp->sb_versionnum & XFS_SB_VERSION_NLINKBIT));
}

static inline void xfs_sb_version_addnlink(xfs_sb_t *sbp)
{
	if (sbp->sb_versionnum <= XFS_SB_VERSION_2)
		sbp->sb_versionnum = XFS_SB_VERSION_3;
	else
		sbp->sb_versionnum |= XFS_SB_VERSION_NLINKBIT;
}

static inline int xfs_sb_version_hasquota(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_QUOTABIT);
}

static inline void xfs_sb_version_addquota(xfs_sb_t *sbp)
{
	if (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4)
		sbp->sb_versionnum |= XFS_SB_VERSION_QUOTABIT;
	else
		sbp->sb_versionnum = xfs_sb_version_tonew(sbp->sb_versionnum) |
					XFS_SB_VERSION_QUOTABIT;
}

static inline int xfs_sb_version_hasalign(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_ALIGNBIT);
}

static inline int xfs_sb_version_hasdalign(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_DALIGNBIT);
}

static inline int xfs_sb_version_hasshared(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_SHAREDBIT);
}

static inline int xfs_sb_version_hasdirv2(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_DIRV2BIT);
}

static inline int xfs_sb_version_haslogv2(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_LOGV2BIT);
}

static inline int xfs_sb_version_hasextflgbit(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_EXTFLGBIT);
}

static inline int xfs_sb_version_hassector(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_SECTORBIT);
}

static inline int xfs_sb_version_hasasciici(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_BORGBIT);
}

static inline int xfs_sb_version_hasmorebits(xfs_sb_t *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_4 &&
		(sbp->sb_versionnum & XFS_SB_VERSION_MOREBITSBIT);
}


static inline int xfs_sb_version_haslazysbcount(xfs_sb_t *sbp)
{
	return xfs_sb_version_hasmorebits(sbp) &&
		(sbp->sb_features2 & XFS_SB_VERSION2_LAZYSBCOUNTBIT);
}

static inline int xfs_sb_version_hasattr2(xfs_sb_t *sbp)
{
	return xfs_sb_version_hasmorebits(sbp) &&
		(sbp->sb_features2 & XFS_SB_VERSION2_ATTR2BIT);
}

static inline void xfs_sb_version_addattr2(xfs_sb_t *sbp)
{
	sbp->sb_versionnum |= XFS_SB_VERSION_MOREBITSBIT;
	sbp->sb_features2 |= XFS_SB_VERSION2_ATTR2BIT;
}

static inline void xfs_sb_version_removeattr2(xfs_sb_t *sbp)
{
	sbp->sb_features2 &= ~XFS_SB_VERSION2_ATTR2BIT;
	if (!sbp->sb_features2)
		sbp->sb_versionnum &= ~XFS_SB_VERSION_MOREBITSBIT;
}

static inline int xfs_sb_version_hasprojid32bit(xfs_sb_t *sbp)
{
	return xfs_sb_version_hasmorebits(sbp) &&
		(sbp->sb_features2 & XFS_SB_VERSION2_PROJID32BIT);
}


#define XFS_SB_DADDR		((xfs_daddr_t)0) 
#define	XFS_SB_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_SB_DADDR)
#define XFS_BUF_TO_SBP(bp)	((xfs_dsb_t *)((bp)->b_addr))

#define	XFS_HDR_BLOCK(mp,d)	((xfs_agblock_t)XFS_BB_TO_FSBT(mp,d))
#define	XFS_DADDR_TO_FSB(mp,d)	XFS_AGB_TO_FSB(mp, \
			xfs_daddr_to_agno(mp,d), xfs_daddr_to_agbno(mp,d))
#define	XFS_FSB_TO_DADDR(mp,fsbno)	XFS_AGB_TO_DADDR(mp, \
			XFS_FSB_TO_AGNO(mp,fsbno), XFS_FSB_TO_AGBNO(mp,fsbno))

#define XFS_FSS_TO_BB(mp,sec)	((sec) << (mp)->m_sectbb_log)

#define	XFS_FSB_TO_BB(mp,fsbno)	((fsbno) << (mp)->m_blkbb_log)
#define	XFS_BB_TO_FSB(mp,bb)	\
	(((bb) + (XFS_FSB_TO_BB(mp,1) - 1)) >> (mp)->m_blkbb_log)
#define	XFS_BB_TO_FSBT(mp,bb)	((bb) >> (mp)->m_blkbb_log)

#define XFS_FSB_TO_B(mp,fsbno)	((xfs_fsize_t)(fsbno) << (mp)->m_sb.sb_blocklog)
#define XFS_B_TO_FSB(mp,b)	\
	((((__uint64_t)(b)) + (mp)->m_blockmask) >> (mp)->m_sb.sb_blocklog)
#define XFS_B_TO_FSBT(mp,b)	(((__uint64_t)(b)) >> (mp)->m_sb.sb_blocklog)
#define XFS_B_FSB_OFFSET(mp,b)	((b) & (mp)->m_blockmask)

#endif	
