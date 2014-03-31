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
#ifndef __XFS_MOUNT_H__
#define	__XFS_MOUNT_H__

typedef struct xfs_trans_reservations {
	uint	tr_write;	
	uint	tr_itruncate;	
	uint	tr_rename;	
	uint	tr_link;	
	uint	tr_remove;	
	uint	tr_symlink;	
	uint	tr_create;	
	uint	tr_mkdir;	
	uint	tr_ifree;	
	uint	tr_ichange;	
	uint	tr_growdata;	
	uint	tr_swrite;	
	uint	tr_addafork;	
	uint	tr_writeid;	
	uint	tr_attrinval;	
	uint	tr_attrset;	
	uint	tr_attrrm;	
	uint	tr_clearagi;	
	uint	tr_growrtalloc;	
	uint	tr_growrtzero;	
	uint	tr_growrtfree;	
} xfs_trans_reservations_t;

#ifndef __KERNEL__

#define xfs_daddr_to_agno(mp,d) \
	((xfs_agnumber_t)(XFS_BB_TO_FSBT(mp, d) / (mp)->m_sb.sb_agblocks))
#define xfs_daddr_to_agbno(mp,d) \
	((xfs_agblock_t)(XFS_BB_TO_FSBT(mp, d) % (mp)->m_sb.sb_agblocks))

#else 

#include "xfs_sync.h"

struct log;
struct xfs_mount_args;
struct xfs_inode;
struct xfs_bmbt_irec;
struct xfs_bmap_free;
struct xfs_extdelta;
struct xfs_swapext;
struct xfs_mru_cache;
struct xfs_nameops;
struct xfs_ail;
struct xfs_quotainfo;

#ifdef HAVE_PERCPU_SB

typedef struct xfs_icsb_cnts {
	uint64_t	icsb_fdblocks;
	uint64_t	icsb_ifree;
	uint64_t	icsb_icount;
	unsigned long	icsb_flags;
} xfs_icsb_cnts_t;

#define XFS_ICSB_FLAG_LOCK	(1 << 0)	

#define XFS_ICSB_LAZY_COUNT	(1 << 1)	

extern int	xfs_icsb_init_counters(struct xfs_mount *);
extern void	xfs_icsb_reinit_counters(struct xfs_mount *);
extern void	xfs_icsb_destroy_counters(struct xfs_mount *);
extern void	xfs_icsb_sync_counters(struct xfs_mount *, int);
extern void	xfs_icsb_sync_counters_locked(struct xfs_mount *, int);
extern int	xfs_icsb_modify_counters(struct xfs_mount *, xfs_sb_field_t,
						int64_t, int);

#else
#define xfs_icsb_init_counters(mp)		(0)
#define xfs_icsb_destroy_counters(mp)		do { } while (0)
#define xfs_icsb_reinit_counters(mp)		do { } while (0)
#define xfs_icsb_sync_counters(mp, flags)	do { } while (0)
#define xfs_icsb_sync_counters_locked(mp, flags) do { } while (0)
#define xfs_icsb_modify_counters(mp, field, delta, rsvd) \
	xfs_mod_incore_sb(mp, field, delta, rsvd)
#endif

enum {
	XFS_LOWSP_1_PCNT = 0,
	XFS_LOWSP_2_PCNT,
	XFS_LOWSP_3_PCNT,
	XFS_LOWSP_4_PCNT,
	XFS_LOWSP_5_PCNT,
	XFS_LOWSP_MAX,
};

typedef struct xfs_mount {
	struct super_block	*m_super;
	xfs_tid_t		m_tid;		
	struct xfs_ail		*m_ail;		
	xfs_sb_t		m_sb;		
	spinlock_t		m_sb_lock;	
	struct xfs_buf		*m_sb_bp;	
	char			*m_fsname;	
	int			m_fsname_len;	
	char			*m_rtname;	
	char			*m_logname;	
	int			m_bsize;	
	xfs_agnumber_t		m_agfrotor;	
	xfs_agnumber_t		m_agirotor;	
	spinlock_t		m_agirotor_lock;
	xfs_agnumber_t		m_maxagi;	
	uint			m_readio_log;	
	uint			m_readio_blocks; 
	uint			m_writeio_log;	
	uint			m_writeio_blocks; 
	struct log		*m_log;		
	int			m_logbufs;	
	int			m_logbsize;	
	uint			m_rsumlevels;	
	uint			m_rsumsize;	
	struct xfs_inode	*m_rbmip;	
	struct xfs_inode	*m_rsumip;	
	struct xfs_inode	*m_rootip;	
	struct xfs_quotainfo	*m_quotainfo;	
	xfs_buftarg_t		*m_ddev_targp;	
	xfs_buftarg_t		*m_logdev_targp;
	xfs_buftarg_t		*m_rtdev_targp;	
	__uint8_t		m_blkbit_log;	
	__uint8_t		m_blkbb_log;	
	__uint8_t		m_agno_log;	
	__uint8_t		m_agino_log;	
	__uint16_t		m_inode_cluster_size;
	uint			m_blockmask;	
	uint			m_blockwsize;	
	uint			m_blockwmask;	
	uint			m_alloc_mxr[2];	
	uint			m_alloc_mnr[2];	
	uint			m_bmap_dmxr[2];	
	uint			m_bmap_dmnr[2];	
	uint			m_inobt_mxr[2];	
	uint			m_inobt_mnr[2];	
	uint			m_ag_maxlevels;	
	uint			m_bm_maxlevels[2]; 
	uint			m_in_maxlevels;	
	struct radix_tree_root	m_perag_tree;	
	spinlock_t		m_perag_lock;	
	struct mutex		m_growlock;	
	int			m_fixedfsid[2];	
	uint			m_dmevmask;	
	__uint64_t		m_flags;	
	uint			m_dir_node_ents; 
	uint			m_attr_node_ents; 
	int			m_ialloc_inos;	
	int			m_ialloc_blks;	
	int			m_inoalign_mask;
	uint			m_qflags;	
	xfs_trans_reservations_t m_reservations;
	__uint64_t		m_maxicount;	
	__uint64_t		m_maxioffset;	
	__uint64_t		m_resblks;	
	__uint64_t		m_resblks_avail;
	__uint64_t		m_resblks_save;	
	int			m_dalign;	
	int			m_swidth;	
	int			m_sinoalign;	
	int			m_attr_magicpct;
	int			m_dir_magicpct;	
	__uint8_t		m_sectbb_log;	
	const struct xfs_nameops *m_dirnameops;	
	int			m_dirblksize;	
	int			m_dirblkfsbs;	
	xfs_dablk_t		m_dirdatablk;	
	xfs_dablk_t		m_dirleafblk;	
	xfs_dablk_t		m_dirfreeblk;	
	uint			m_chsize;	
	struct xfs_chash	*m_chash;	
	atomic_t		m_active_trans;	
#ifdef HAVE_PERCPU_SB
	xfs_icsb_cnts_t __percpu *m_sb_cnts;	
	unsigned long		m_icsb_counters; 
	struct notifier_block	m_icsb_notifier; 
	struct mutex		m_icsb_mutex;	
#endif
	struct xfs_mru_cache	*m_filestream;  
	struct delayed_work	m_sync_work;	
	struct delayed_work	m_reclaim_work;	
	struct work_struct	m_flush_work;	
	__int64_t		m_update_flags;	
	struct shrinker		m_inode_shrink;	
	int64_t			m_low_space[XFS_LOWSP_MAX];
						

	struct workqueue_struct	*m_data_workqueue;
	struct workqueue_struct	*m_unwritten_workqueue;
} xfs_mount_t;

#define XFS_MOUNT_WSYNC		(1ULL << 0)	
#define XFS_MOUNT_WAS_CLEAN	(1ULL << 3)
#define XFS_MOUNT_FS_SHUTDOWN	(1ULL << 4)	
#define XFS_MOUNT_DISCARD	(1ULL << 5)	
#define XFS_MOUNT_RETERR	(1ULL << 6)     
#define XFS_MOUNT_NOALIGN	(1ULL << 7)	
#define XFS_MOUNT_ATTR2		(1ULL << 8)	
#define XFS_MOUNT_GRPID		(1ULL << 9)	
#define XFS_MOUNT_NORECOVERY	(1ULL << 10)	
#define XFS_MOUNT_DFLT_IOSIZE	(1ULL << 12)	
#define XFS_MOUNT_32BITINODES	(1ULL << 14)	
#define XFS_MOUNT_SMALL_INUMS	(1ULL << 15)	
#define XFS_MOUNT_NOUUID	(1ULL << 16)	
#define XFS_MOUNT_BARRIER	(1ULL << 17)
#define XFS_MOUNT_IKEEP		(1ULL << 18)	
#define XFS_MOUNT_SWALLOC	(1ULL << 19)	
#define XFS_MOUNT_RDONLY	(1ULL << 20)	
#define XFS_MOUNT_DIRSYNC	(1ULL << 21)	
#define XFS_MOUNT_COMPAT_IOSIZE	(1ULL << 22)	
#define XFS_MOUNT_FILESTREAMS	(1ULL << 24)	
#define XFS_MOUNT_NOATTR2	(1ULL << 25)	


#define XFS_READIO_LOG_LARGE	16
#define XFS_WRITEIO_LOG_LARGE	16

#define XFS_MAX_IO_LOG		30	
#define XFS_MIN_IO_LOG		PAGE_SHIFT

#define	XFS_WSYNC_READIO_LOG	15	
#define	XFS_WSYNC_WRITEIO_LOG	14	

static inline unsigned long
xfs_preferred_iosize(xfs_mount_t *mp)
{
	if (mp->m_flags & XFS_MOUNT_COMPAT_IOSIZE)
		return PAGE_CACHE_SIZE;
	return (mp->m_swidth ?
		(mp->m_swidth << mp->m_sb.sb_blocklog) :
		((mp->m_flags & XFS_MOUNT_DFLT_IOSIZE) ?
			(1 << (int)MAX(mp->m_readio_log, mp->m_writeio_log)) :
			PAGE_CACHE_SIZE));
}

#define XFS_MAXIOFFSET(mp)	((mp)->m_maxioffset)

#define XFS_LAST_UNMOUNT_WAS_CLEAN(mp)	\
				((mp)->m_flags & XFS_MOUNT_WAS_CLEAN)
#define XFS_FORCED_SHUTDOWN(mp)	((mp)->m_flags & XFS_MOUNT_FS_SHUTDOWN)
void xfs_do_force_shutdown(struct xfs_mount *mp, int flags, char *fname,
		int lnnum);
#define xfs_force_shutdown(m,f)	\
	xfs_do_force_shutdown(m, f, __FILE__, __LINE__)

#define SHUTDOWN_META_IO_ERROR	0x0001	
#define SHUTDOWN_LOG_IO_ERROR	0x0002	
#define SHUTDOWN_FORCE_UMOUNT	0x0004	
#define SHUTDOWN_CORRUPT_INCORE	0x0008	
#define SHUTDOWN_REMOTE_REQ	0x0010	
#define SHUTDOWN_DEVICE_REQ	0x0020	

#define xfs_test_for_freeze(mp)		((mp)->m_super->s_frozen)
#define xfs_wait_for_freeze(mp,l)	vfs_check_frozen((mp)->m_super, (l))

#define XFS_MFSI_QUIET		0x40	

static inline xfs_agnumber_t
xfs_daddr_to_agno(struct xfs_mount *mp, xfs_daddr_t d)
{
	xfs_daddr_t ld = XFS_BB_TO_FSBT(mp, d);
	do_div(ld, mp->m_sb.sb_agblocks);
	return (xfs_agnumber_t) ld;
}

static inline xfs_agblock_t
xfs_daddr_to_agbno(struct xfs_mount *mp, xfs_daddr_t d)
{
	xfs_daddr_t ld = XFS_BB_TO_FSBT(mp, d);
	return (xfs_agblock_t) do_div(ld, mp->m_sb.sb_agblocks);
}

struct xfs_perag *xfs_perag_get(struct xfs_mount *mp, xfs_agnumber_t agno);
struct xfs_perag *xfs_perag_get_tag(struct xfs_mount *mp, xfs_agnumber_t agno,
					int tag);
void	xfs_perag_put(struct xfs_perag *pag);

#ifdef HAVE_PERCPU_SB
static inline void
xfs_icsb_lock(xfs_mount_t *mp)
{
	mutex_lock(&mp->m_icsb_mutex);
}

static inline void
xfs_icsb_unlock(xfs_mount_t *mp)
{
	mutex_unlock(&mp->m_icsb_mutex);
}
#else
#define xfs_icsb_lock(mp)
#define xfs_icsb_unlock(mp)
#endif

typedef struct xfs_mod_sb {
	xfs_sb_field_t	msb_field;	
	int64_t		msb_delta;	
} xfs_mod_sb_t;

extern int	xfs_log_sbcount(xfs_mount_t *);
extern __uint64_t xfs_default_resblks(xfs_mount_t *mp);
extern int	xfs_mountfs(xfs_mount_t *mp);

extern void	xfs_unmountfs(xfs_mount_t *);
extern int	xfs_unmountfs_writesb(xfs_mount_t *);
extern int	xfs_mod_incore_sb(xfs_mount_t *, xfs_sb_field_t, int64_t, int);
extern int	xfs_mod_incore_sb_batch(xfs_mount_t *, xfs_mod_sb_t *,
			uint, int);
extern int	xfs_mount_log_sb(xfs_mount_t *, __int64_t);
extern struct xfs_buf *xfs_getsb(xfs_mount_t *, int);
extern int	xfs_readsb(xfs_mount_t *, int);
extern void	xfs_freesb(xfs_mount_t *);
extern int	xfs_fs_writable(xfs_mount_t *);
extern int	xfs_sb_validate_fsb_count(struct xfs_sb *, __uint64_t);

extern int	xfs_dev_is_read_only(struct xfs_mount *, char *);

extern void	xfs_set_low_space_thresholds(struct xfs_mount *);

#endif	

extern void	xfs_mod_sb(struct xfs_trans *, __int64_t);
extern int	xfs_initialize_perag(struct xfs_mount *, xfs_agnumber_t,
					xfs_agnumber_t *);
extern void	xfs_sb_from_disk(struct xfs_mount *, struct xfs_dsb *);
extern void	xfs_sb_to_disk(struct xfs_dsb *, struct xfs_sb *, __int64_t);

#endif	
