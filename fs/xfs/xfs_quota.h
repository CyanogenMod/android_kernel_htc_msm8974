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
#ifndef __XFS_QUOTA_H__
#define __XFS_QUOTA_H__

struct xfs_trans;

#define XFS_DQUOT_MAGIC		0x4451		
#define XFS_DQUOT_VERSION	(u_int8_t)0x01	

typedef __uint32_t	xfs_dqid_t;

typedef __uint64_t	xfs_qcnt_t;
typedef __uint16_t	xfs_qwarncnt_t;

typedef struct	xfs_disk_dquot {
	__be16		d_magic;	
	__u8		d_version;	
	__u8		d_flags;	
	__be32		d_id;		
	__be64		d_blk_hardlimit;
	__be64		d_blk_softlimit;
	__be64		d_ino_hardlimit;
	__be64		d_ino_softlimit;
	__be64		d_bcount;	
	__be64		d_icount;	
	__be32		d_itimer;	
	__be32		d_btimer;	
	__be16		d_iwarns;	
	__be16		d_bwarns;	
	__be32		d_pad0;		
	__be64		d_rtb_hardlimit;
	__be64		d_rtb_softlimit;
	__be64		d_rtbcount;	
	__be32		d_rtbtimer;	
	__be16		d_rtbwarns;	
	__be16		d_pad;
} xfs_disk_dquot_t;

typedef struct xfs_dqblk {
	xfs_disk_dquot_t  dd_diskdq;	
	char		  dd_fill[32];	
} xfs_dqblk_t;

#define XFS_DQ_USER		0x0001		
#define XFS_DQ_PROJ		0x0002		
#define XFS_DQ_GROUP		0x0004		
#define XFS_DQ_DIRTY		0x0008		
#define XFS_DQ_FREEING		0x0010		

#define XFS_DQ_ALLTYPES		(XFS_DQ_USER|XFS_DQ_PROJ|XFS_DQ_GROUP)

#define XFS_DQ_FLAGS \
	{ XFS_DQ_USER,		"USER" }, \
	{ XFS_DQ_PROJ,		"PROJ" }, \
	{ XFS_DQ_GROUP,		"GROUP" }, \
	{ XFS_DQ_DIRTY,		"DIRTY" }, \
	{ XFS_DQ_FREEING,	"FREEING" }

#define XFS_DQUOT_LOGRES(mp)	(sizeof(xfs_disk_dquot_t) * 3)



typedef struct xfs_dq_logformat {
	__uint16_t		qlf_type;      
	__uint16_t		qlf_size;      
	xfs_dqid_t		qlf_id;	       
	__int64_t		qlf_blkno;     
	__int32_t		qlf_len;       
	__uint32_t		qlf_boffset;   
} xfs_dq_logformat_t;

typedef struct xfs_qoff_logformat {
	unsigned short		qf_type;	
	unsigned short		qf_size;	
	unsigned int		qf_flags;	
	char			qf_pad[12];	
} xfs_qoff_logformat_t;


#define XFS_UQUOTA_ACCT	0x0001  
#define XFS_UQUOTA_ENFD	0x0002  
#define XFS_UQUOTA_CHKD	0x0004  
#define XFS_PQUOTA_ACCT	0x0008  
#define XFS_OQUOTA_ENFD	0x0010  
#define XFS_OQUOTA_CHKD	0x0020  
#define XFS_GQUOTA_ACCT	0x0040  

#define XFS_ALL_QUOTA_ACCT	\
		(XFS_UQUOTA_ACCT | XFS_GQUOTA_ACCT | XFS_PQUOTA_ACCT)
#define XFS_ALL_QUOTA_ENFD	(XFS_UQUOTA_ENFD | XFS_OQUOTA_ENFD)
#define XFS_ALL_QUOTA_CHKD	(XFS_UQUOTA_CHKD | XFS_OQUOTA_CHKD)

#define XFS_IS_QUOTA_RUNNING(mp)	((mp)->m_qflags & XFS_ALL_QUOTA_ACCT)
#define XFS_IS_UQUOTA_RUNNING(mp)	((mp)->m_qflags & XFS_UQUOTA_ACCT)
#define XFS_IS_PQUOTA_RUNNING(mp)	((mp)->m_qflags & XFS_PQUOTA_ACCT)
#define XFS_IS_GQUOTA_RUNNING(mp)	((mp)->m_qflags & XFS_GQUOTA_ACCT)
#define XFS_IS_UQUOTA_ENFORCED(mp)	((mp)->m_qflags & XFS_UQUOTA_ENFD)
#define XFS_IS_OQUOTA_ENFORCED(mp)	((mp)->m_qflags & XFS_OQUOTA_ENFD)

#define XFS_UQUOTA_ACTIVE	0x0100  
#define XFS_PQUOTA_ACTIVE	0x0200  
#define XFS_GQUOTA_ACTIVE	0x0400  
#define XFS_ALL_QUOTA_ACTIVE	\
	(XFS_UQUOTA_ACTIVE | XFS_PQUOTA_ACTIVE | XFS_GQUOTA_ACTIVE)

#define XFS_IS_QUOTA_ON(mp)	((mp)->m_qflags & (XFS_UQUOTA_ACTIVE | \
						   XFS_GQUOTA_ACTIVE | \
						   XFS_PQUOTA_ACTIVE))
#define XFS_IS_OQUOTA_ON(mp)	((mp)->m_qflags & (XFS_GQUOTA_ACTIVE | \
						   XFS_PQUOTA_ACTIVE))
#define XFS_IS_UQUOTA_ON(mp)	((mp)->m_qflags & XFS_UQUOTA_ACTIVE)
#define XFS_IS_GQUOTA_ON(mp)	((mp)->m_qflags & XFS_GQUOTA_ACTIVE)
#define XFS_IS_PQUOTA_ON(mp)	((mp)->m_qflags & XFS_PQUOTA_ACTIVE)

#define XFS_QMOPT_DQALLOC	0x0000002 
#define XFS_QMOPT_UQUOTA	0x0000004 
#define XFS_QMOPT_PQUOTA	0x0000008 
#define XFS_QMOPT_FORCE_RES	0x0000010 
#define XFS_QMOPT_SBVERSION	0x0000040 
#define XFS_QMOPT_DOWARN        0x0000400 
#define XFS_QMOPT_DQREPAIR	0x0001000 
#define XFS_QMOPT_GQUOTA	0x0002000 
#define XFS_QMOPT_ENOSPC	0x0004000 

#define XFS_QMOPT_RES_REGBLKS	0x0010000
#define XFS_QMOPT_RES_RTBLKS	0x0020000
#define XFS_QMOPT_BCOUNT	0x0040000
#define XFS_QMOPT_ICOUNT	0x0080000
#define XFS_QMOPT_RTBCOUNT	0x0100000
#define XFS_QMOPT_DELBCOUNT	0x0200000
#define XFS_QMOPT_DELRTBCOUNT	0x0400000
#define XFS_QMOPT_RES_INOS	0x0800000

#define XFS_QMOPT_INHERIT	0x1000000

#define XFS_TRANS_DQ_RES_BLKS	XFS_QMOPT_RES_REGBLKS
#define XFS_TRANS_DQ_RES_RTBLKS	XFS_QMOPT_RES_RTBLKS
#define XFS_TRANS_DQ_RES_INOS	XFS_QMOPT_RES_INOS
#define XFS_TRANS_DQ_BCOUNT	XFS_QMOPT_BCOUNT
#define XFS_TRANS_DQ_DELBCOUNT	XFS_QMOPT_DELBCOUNT
#define XFS_TRANS_DQ_ICOUNT	XFS_QMOPT_ICOUNT
#define XFS_TRANS_DQ_RTBCOUNT	XFS_QMOPT_RTBCOUNT
#define XFS_TRANS_DQ_DELRTBCOUNT XFS_QMOPT_DELRTBCOUNT


#define XFS_QMOPT_QUOTALL	\
		(XFS_QMOPT_UQUOTA | XFS_QMOPT_PQUOTA | XFS_QMOPT_GQUOTA)
#define XFS_QMOPT_RESBLK_MASK	(XFS_QMOPT_RES_REGBLKS | XFS_QMOPT_RES_RTBLKS)

#ifdef __KERNEL__
#define XFS_NOT_DQATTACHED(mp, ip) ((XFS_IS_UQUOTA_ON(mp) &&\
				     (ip)->i_udquot == NULL) || \
				    (XFS_IS_OQUOTA_ON(mp) && \
				     (ip)->i_gdquot == NULL))

#define XFS_QM_NEED_QUOTACHECK(mp) \
	((XFS_IS_UQUOTA_ON(mp) && \
		(mp->m_sb.sb_qflags & XFS_UQUOTA_CHKD) == 0) || \
	 (XFS_IS_GQUOTA_ON(mp) && \
		((mp->m_sb.sb_qflags & XFS_OQUOTA_CHKD) == 0 || \
		 (mp->m_sb.sb_qflags & XFS_PQUOTA_ACCT))) || \
	 (XFS_IS_PQUOTA_ON(mp) && \
		((mp->m_sb.sb_qflags & XFS_OQUOTA_CHKD) == 0 || \
		 (mp->m_sb.sb_qflags & XFS_GQUOTA_ACCT))))

#define XFS_MOUNT_QUOTA_SET1	(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|\
				 XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|\
				 XFS_OQUOTA_ENFD|XFS_OQUOTA_CHKD)

#define XFS_MOUNT_QUOTA_SET2	(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|\
				 XFS_UQUOTA_CHKD|XFS_GQUOTA_ACCT|\
				 XFS_OQUOTA_ENFD|XFS_OQUOTA_CHKD)

#define XFS_MOUNT_QUOTA_ALL	(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|\
				 XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|\
				 XFS_OQUOTA_ENFD|XFS_OQUOTA_CHKD|\
				 XFS_GQUOTA_ACCT)


typedef struct xfs_dqtrx {
	struct xfs_dquot *qt_dquot;	  
	ulong		qt_blk_res;	  
	ulong		qt_blk_res_used;  
	ulong		qt_ino_res;	  
	ulong		qt_ino_res_used;  
	long		qt_bcount_delta;  
	long		qt_delbcnt_delta; 
	long		qt_icount_delta;  
	ulong		qt_rtblk_res;	  
	ulong		qt_rtblk_res_used;
	long		qt_rtbcount_delta;
	long		qt_delrtb_delta;  
} xfs_dqtrx_t;

#ifdef CONFIG_XFS_QUOTA
extern void xfs_trans_dup_dqinfo(struct xfs_trans *, struct xfs_trans *);
extern void xfs_trans_free_dqinfo(struct xfs_trans *);
extern void xfs_trans_mod_dquot_byino(struct xfs_trans *, struct xfs_inode *,
		uint, long);
extern void xfs_trans_apply_dquot_deltas(struct xfs_trans *);
extern void xfs_trans_unreserve_and_mod_dquots(struct xfs_trans *);
extern int xfs_trans_reserve_quota_nblks(struct xfs_trans *,
		struct xfs_inode *, long, long, uint);
extern int xfs_trans_reserve_quota_bydquots(struct xfs_trans *,
		struct xfs_mount *, struct xfs_dquot *,
		struct xfs_dquot *, long, long, uint);

extern int xfs_qm_vop_dqalloc(struct xfs_inode *, uid_t, gid_t, prid_t, uint,
		struct xfs_dquot **, struct xfs_dquot **);
extern void xfs_qm_vop_create_dqattach(struct xfs_trans *, struct xfs_inode *,
		struct xfs_dquot *, struct xfs_dquot *);
extern int xfs_qm_vop_rename_dqattach(struct xfs_inode **);
extern struct xfs_dquot *xfs_qm_vop_chown(struct xfs_trans *,
		struct xfs_inode *, struct xfs_dquot **, struct xfs_dquot *);
extern int xfs_qm_vop_chown_reserve(struct xfs_trans *, struct xfs_inode *,
		struct xfs_dquot *, struct xfs_dquot *, uint);
extern int xfs_qm_dqattach(struct xfs_inode *, uint);
extern int xfs_qm_dqattach_locked(struct xfs_inode *, uint);
extern void xfs_qm_dqdetach(struct xfs_inode *);
extern void xfs_qm_dqrele(struct xfs_dquot *);
extern void xfs_qm_statvfs(struct xfs_inode *, struct kstatfs *);
extern int xfs_qm_newmount(struct xfs_mount *, uint *, uint *);
extern void xfs_qm_mount_quotas(struct xfs_mount *);
extern void xfs_qm_unmount(struct xfs_mount *);
extern void xfs_qm_unmount_quotas(struct xfs_mount *);

#else
static inline int
xfs_qm_vop_dqalloc(struct xfs_inode *ip, uid_t uid, gid_t gid, prid_t prid,
		uint flags, struct xfs_dquot **udqp, struct xfs_dquot **gdqp)
{
	*udqp = NULL;
	*gdqp = NULL;
	return 0;
}
#define xfs_trans_dup_dqinfo(tp, tp2)
#define xfs_trans_free_dqinfo(tp)
#define xfs_trans_mod_dquot_byino(tp, ip, fields, delta)
#define xfs_trans_apply_dquot_deltas(tp)
#define xfs_trans_unreserve_and_mod_dquots(tp)
static inline int xfs_trans_reserve_quota_nblks(struct xfs_trans *tp,
		struct xfs_inode *ip, long nblks, long ninos, uint flags)
{
	return 0;
}
static inline int xfs_trans_reserve_quota_bydquots(struct xfs_trans *tp,
		struct xfs_mount *mp, struct xfs_dquot *udqp,
		struct xfs_dquot *gdqp, long nblks, long nions, uint flags)
{
	return 0;
}
#define xfs_qm_vop_create_dqattach(tp, ip, u, g)
#define xfs_qm_vop_rename_dqattach(it)					(0)
#define xfs_qm_vop_chown(tp, ip, old, new)				(NULL)
#define xfs_qm_vop_chown_reserve(tp, ip, u, g, fl)			(0)
#define xfs_qm_dqattach(ip, fl)						(0)
#define xfs_qm_dqattach_locked(ip, fl)					(0)
#define xfs_qm_dqdetach(ip)
#define xfs_qm_dqrele(d)
#define xfs_qm_statvfs(ip, s)
#define xfs_qm_newmount(mp, a, b)					(0)
#define xfs_qm_mount_quotas(mp)
#define xfs_qm_unmount(mp)
#define xfs_qm_unmount_quotas(mp)
#endif 

#define xfs_trans_unreserve_quota_nblks(tp, ip, nblks, ninos, flags) \
	xfs_trans_reserve_quota_nblks(tp, ip, -(nblks), -(ninos), flags)
#define xfs_trans_reserve_quota(tp, mp, ud, gd, nb, ni, f) \
	xfs_trans_reserve_quota_bydquots(tp, mp, ud, gd, nb, ni, \
				f | XFS_QMOPT_RES_REGBLKS)

extern int xfs_qm_dqcheck(struct xfs_mount *, xfs_disk_dquot_t *,
				xfs_dqid_t, uint, uint, char *);
extern int xfs_mount_reset_sbqflags(struct xfs_mount *);

#endif	
#endif	
