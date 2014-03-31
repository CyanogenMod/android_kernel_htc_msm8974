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
#ifndef __XFS_QM_H__
#define __XFS_QM_H__

#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_quota_priv.h"

struct xfs_inode;

extern struct kmem_zone	*xfs_qm_dqtrxzone;

#define XFS_DQUOT_CLUSTER_SIZE_FSB	(xfs_filblks_t)1

typedef struct xfs_quotainfo {
	struct radix_tree_root qi_uquota_tree;
	struct radix_tree_root qi_gquota_tree;
	struct mutex qi_tree_lock;
	xfs_inode_t	*qi_uquotaip;	 
	xfs_inode_t	*qi_gquotaip;	 
	struct list_head qi_lru_list;
	struct mutex	 qi_lru_lock;
	int		 qi_lru_count;
	int		 qi_dquots;
	time_t		 qi_btimelimit;	 
	time_t		 qi_itimelimit;	 
	time_t		 qi_rtbtimelimit;
	xfs_qwarncnt_t	 qi_bwarnlimit;	 
	xfs_qwarncnt_t	 qi_iwarnlimit;	 
	xfs_qwarncnt_t	 qi_rtbwarnlimit;
	struct mutex	 qi_quotaofflock;
	xfs_filblks_t	 qi_dqchunklen;	 
	uint		 qi_dqperchunk;	 
	xfs_qcnt_t	 qi_bhardlimit;	 
	xfs_qcnt_t	 qi_bsoftlimit;	 
	xfs_qcnt_t	 qi_ihardlimit;	 
	xfs_qcnt_t	 qi_isoftlimit;	 
	xfs_qcnt_t	 qi_rtbhardlimit;
	xfs_qcnt_t	 qi_rtbsoftlimit;
	struct shrinker  qi_shrinker;
} xfs_quotainfo_t;

#define XFS_DQUOT_TREE(qi, type) \
	((type & XFS_DQ_USER) ? \
	 &((qi)->qi_uquota_tree) : \
	 &((qi)->qi_gquota_tree))


extern void	xfs_trans_mod_dquot(xfs_trans_t *, xfs_dquot_t *, uint, long);
extern int	xfs_trans_reserve_quota_bydquots(xfs_trans_t *, xfs_mount_t *,
			xfs_dquot_t *, xfs_dquot_t *, long, long, uint);
extern void	xfs_trans_dqjoin(xfs_trans_t *, xfs_dquot_t *);
extern void	xfs_trans_log_dquot(xfs_trans_t *, xfs_dquot_t *);

#define XFS_QM_TRANS_MAXDQS		2
typedef struct xfs_dquot_acct {
	xfs_dqtrx_t	dqa_usrdquots[XFS_QM_TRANS_MAXDQS];
	xfs_dqtrx_t	dqa_grpdquots[XFS_QM_TRANS_MAXDQS];
} xfs_dquot_acct_t;

#define XFS_QM_BTIMELIMIT	(7 * 24*60*60)          
#define XFS_QM_RTBTIMELIMIT	(7 * 24*60*60)          
#define XFS_QM_ITIMELIMIT	(7 * 24*60*60)          

#define XFS_QM_BWARNLIMIT	5
#define XFS_QM_IWARNLIMIT	5
#define XFS_QM_RTBWARNLIMIT	5

extern void		xfs_qm_destroy_quotainfo(xfs_mount_t *);
extern int		xfs_qm_quotacheck(xfs_mount_t *);
extern int		xfs_qm_write_sb_changes(xfs_mount_t *, __int64_t);

extern void		xfs_qm_dqpurge_all(xfs_mount_t *, uint);
extern void		xfs_qm_dqrele_all_inodes(xfs_mount_t *, uint);

extern int		xfs_qm_scall_trunc_qfiles(xfs_mount_t *, uint);
extern int		xfs_qm_scall_getquota(xfs_mount_t *, xfs_dqid_t, uint,
					fs_disk_quota_t *);
extern int		xfs_qm_scall_setqlim(xfs_mount_t *, xfs_dqid_t, uint,
					fs_disk_quota_t *);
extern int		xfs_qm_scall_getqstat(xfs_mount_t *, fs_quota_stat_t *);
extern int		xfs_qm_scall_quotaon(xfs_mount_t *, uint);
extern int		xfs_qm_scall_quotaoff(xfs_mount_t *, uint);

#endif 
