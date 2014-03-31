/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
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
#ifndef	__XFS_TRANS_H__
#define	__XFS_TRANS_H__

struct xfs_log_item;

/*
 * This is the structure written in the log at the head of
 * every transaction. It identifies the type and id of the
 * transaction, and contains the number of items logged by
 * the transaction so we know how many to expect during recovery.
 *
 * Do not change the below structure without redoing the code in
 * xlog_recover_add_to_trans() and xlog_recover_add_to_cont_trans().
 */
typedef struct xfs_trans_header {
	uint		th_magic;		
	uint		th_type;		
	__int32_t	th_tid;			
	uint		th_num_items;		
} xfs_trans_header_t;

#define	XFS_TRANS_HEADER_MAGIC	0x5452414e	

#define	XFS_LI_EFI		0x1236
#define	XFS_LI_EFD		0x1237
#define	XFS_LI_IUNLINK		0x1238
#define	XFS_LI_INODE		0x123b	
#define	XFS_LI_BUF		0x123c	
#define	XFS_LI_DQUOT		0x123d
#define	XFS_LI_QUOTAOFF		0x123e

#define XFS_LI_TYPE_DESC \
	{ XFS_LI_EFI,		"XFS_LI_EFI" }, \
	{ XFS_LI_EFD,		"XFS_LI_EFD" }, \
	{ XFS_LI_IUNLINK,	"XFS_LI_IUNLINK" }, \
	{ XFS_LI_INODE,		"XFS_LI_INODE" }, \
	{ XFS_LI_BUF,		"XFS_LI_BUF" }, \
	{ XFS_LI_DQUOT,		"XFS_LI_DQUOT" }, \
	{ XFS_LI_QUOTAOFF,	"XFS_LI_QUOTAOFF" }

#define XFS_TRANS_SETATTR_NOT_SIZE	1
#define XFS_TRANS_SETATTR_SIZE		2
#define XFS_TRANS_INACTIVE		3
#define XFS_TRANS_CREATE		4
#define XFS_TRANS_CREATE_TRUNC		5
#define XFS_TRANS_TRUNCATE_FILE		6
#define XFS_TRANS_REMOVE		7
#define XFS_TRANS_LINK			8
#define XFS_TRANS_RENAME		9
#define XFS_TRANS_MKDIR			10
#define XFS_TRANS_RMDIR			11
#define XFS_TRANS_SYMLINK		12
#define XFS_TRANS_SET_DMATTRS		13
#define XFS_TRANS_GROWFS		14
#define XFS_TRANS_STRAT_WRITE		15
#define XFS_TRANS_DIOSTRAT		16
#define	XFS_TRANS_WRITEID		18
#define	XFS_TRANS_ADDAFORK		19
#define	XFS_TRANS_ATTRINVAL		20
#define	XFS_TRANS_ATRUNCATE		21
#define	XFS_TRANS_ATTR_SET		22
#define	XFS_TRANS_ATTR_RM		23
#define	XFS_TRANS_ATTR_FLAG		24
#define	XFS_TRANS_CLEAR_AGI_BUCKET	25
#define XFS_TRANS_QM_SBCHANGE		26
#define XFS_TRANS_DUMMY1		27
#define XFS_TRANS_DUMMY2		28
#define XFS_TRANS_QM_QUOTAOFF		29
#define XFS_TRANS_QM_DQALLOC		30
#define XFS_TRANS_QM_SETQLIM		31
#define XFS_TRANS_QM_DQCLUSTER		32
#define XFS_TRANS_QM_QINOCREATE		33
#define XFS_TRANS_QM_QUOTAOFF_END	34
#define XFS_TRANS_SB_UNIT		35
#define XFS_TRANS_FSYNC_TS		36
#define	XFS_TRANS_GROWFSRT_ALLOC	37
#define	XFS_TRANS_GROWFSRT_ZERO		38
#define	XFS_TRANS_GROWFSRT_FREE		39
#define	XFS_TRANS_SWAPEXT		40
#define	XFS_TRANS_SB_COUNT		41
#define	XFS_TRANS_CHECKPOINT		42
#define	XFS_TRANS_TYPE_MAX		42

#define XFS_TRANS_TYPES \
	{ XFS_TRANS_SETATTR_NOT_SIZE,	"SETATTR_NOT_SIZE" }, \
	{ XFS_TRANS_SETATTR_SIZE,	"SETATTR_SIZE" }, \
	{ XFS_TRANS_INACTIVE,		"INACTIVE" }, \
	{ XFS_TRANS_CREATE,		"CREATE" }, \
	{ XFS_TRANS_CREATE_TRUNC,	"CREATE_TRUNC" }, \
	{ XFS_TRANS_TRUNCATE_FILE,	"TRUNCATE_FILE" }, \
	{ XFS_TRANS_REMOVE,		"REMOVE" }, \
	{ XFS_TRANS_LINK,		"LINK" }, \
	{ XFS_TRANS_RENAME,		"RENAME" }, \
	{ XFS_TRANS_MKDIR,		"MKDIR" }, \
	{ XFS_TRANS_RMDIR,		"RMDIR" }, \
	{ XFS_TRANS_SYMLINK,		"SYMLINK" }, \
	{ XFS_TRANS_SET_DMATTRS,	"SET_DMATTRS" }, \
	{ XFS_TRANS_GROWFS,		"GROWFS" }, \
	{ XFS_TRANS_STRAT_WRITE,	"STRAT_WRITE" }, \
	{ XFS_TRANS_DIOSTRAT,		"DIOSTRAT" }, \
	{ XFS_TRANS_WRITEID,		"WRITEID" }, \
	{ XFS_TRANS_ADDAFORK,		"ADDAFORK" }, \
	{ XFS_TRANS_ATTRINVAL,		"ATTRINVAL" }, \
	{ XFS_TRANS_ATRUNCATE,		"ATRUNCATE" }, \
	{ XFS_TRANS_ATTR_SET,		"ATTR_SET" }, \
	{ XFS_TRANS_ATTR_RM,		"ATTR_RM" }, \
	{ XFS_TRANS_ATTR_FLAG,		"ATTR_FLAG" }, \
	{ XFS_TRANS_CLEAR_AGI_BUCKET,	"CLEAR_AGI_BUCKET" }, \
	{ XFS_TRANS_QM_SBCHANGE,	"QM_SBCHANGE" }, \
	{ XFS_TRANS_QM_QUOTAOFF,	"QM_QUOTAOFF" }, \
	{ XFS_TRANS_QM_DQALLOC,		"QM_DQALLOC" }, \
	{ XFS_TRANS_QM_SETQLIM,		"QM_SETQLIM" }, \
	{ XFS_TRANS_QM_DQCLUSTER,	"QM_DQCLUSTER" }, \
	{ XFS_TRANS_QM_QINOCREATE,	"QM_QINOCREATE" }, \
	{ XFS_TRANS_QM_QUOTAOFF_END,	"QM_QOFF_END" }, \
	{ XFS_TRANS_SB_UNIT,		"SB_UNIT" }, \
	{ XFS_TRANS_FSYNC_TS,		"FSYNC_TS" }, \
	{ XFS_TRANS_GROWFSRT_ALLOC,	"GROWFSRT_ALLOC" }, \
	{ XFS_TRANS_GROWFSRT_ZERO,	"GROWFSRT_ZERO" }, \
	{ XFS_TRANS_GROWFSRT_FREE,	"GROWFSRT_FREE" }, \
	{ XFS_TRANS_SWAPEXT,		"SWAPEXT" }, \
	{ XFS_TRANS_SB_COUNT,		"SB_COUNT" }, \
	{ XFS_TRANS_CHECKPOINT,		"CHECKPOINT" }, \
	{ XFS_TRANS_DUMMY1,		"DUMMY1" }, \
	{ XFS_TRANS_DUMMY2,		"DUMMY2" }, \
	{ XLOG_UNMOUNT_REC_TYPE,	"UNMOUNT" }

struct xfs_log_item_desc {
	struct xfs_log_item	*lid_item;
	struct list_head	lid_trans;
	unsigned char		lid_flags;
};

#define XFS_LID_DIRTY		0x1

#define	XFS_TRANS_MAGIC		0x5452414E	
#define	XFS_TRANS_DIRTY		0x01	
#define	XFS_TRANS_SB_DIRTY	0x02	
#define	XFS_TRANS_PERM_LOG_RES	0x04	
#define	XFS_TRANS_SYNC		0x08	
#define XFS_TRANS_DQ_DIRTY	0x10	
#define XFS_TRANS_RESERVE	0x20    

#define	XFS_TRANS_RELEASE_LOG_RES	0x4
#define	XFS_TRANS_ABORT			0x8

#define	XFS_TRANS_SB_ICOUNT		0x00000001
#define	XFS_TRANS_SB_IFREE		0x00000002
#define	XFS_TRANS_SB_FDBLOCKS		0x00000004
#define	XFS_TRANS_SB_RES_FDBLOCKS	0x00000008
#define	XFS_TRANS_SB_FREXTENTS		0x00000010
#define	XFS_TRANS_SB_RES_FREXTENTS	0x00000020
#define	XFS_TRANS_SB_DBLOCKS		0x00000040
#define	XFS_TRANS_SB_AGCOUNT		0x00000080
#define	XFS_TRANS_SB_IMAXPCT		0x00000100
#define	XFS_TRANS_SB_REXTSIZE		0x00000200
#define	XFS_TRANS_SB_RBMBLOCKS		0x00000400
#define	XFS_TRANS_SB_RBLOCKS		0x00000800
#define	XFS_TRANS_SB_REXTENTS		0x00001000
#define	XFS_TRANS_SB_REXTSLOG		0x00002000


#define	XFS_ALLOCFREE_LOG_RES(mp,nx) \
	((nx) * (2 * XFS_FSB_TO_B((mp), 2 * XFS_AG_MAXLEVELS(mp) - 1)))
#define	XFS_ALLOCFREE_LOG_COUNT(mp,nx) \
	((nx) * (2 * (2 * XFS_AG_MAXLEVELS(mp) - 1)))

#define	XFS_DIROP_LOG_RES(mp)	\
	(XFS_FSB_TO_B(mp, XFS_DAENTER_BLOCKS(mp, XFS_DATA_FORK)) + \
	 (XFS_FSB_TO_B(mp, XFS_DAENTER_BMAPS(mp, XFS_DATA_FORK) + 1)))
#define	XFS_DIROP_LOG_COUNT(mp)	\
	(XFS_DAENTER_BLOCKS(mp, XFS_DATA_FORK) + \
	 XFS_DAENTER_BMAPS(mp, XFS_DATA_FORK) + 1)


#define	XFS_WRITE_LOG_RES(mp)	((mp)->m_reservations.tr_write)
#define	XFS_ITRUNCATE_LOG_RES(mp)   ((mp)->m_reservations.tr_itruncate)
#define	XFS_RENAME_LOG_RES(mp)	((mp)->m_reservations.tr_rename)
#define	XFS_LINK_LOG_RES(mp)	((mp)->m_reservations.tr_link)
#define	XFS_REMOVE_LOG_RES(mp)	((mp)->m_reservations.tr_remove)
#define	XFS_SYMLINK_LOG_RES(mp)	((mp)->m_reservations.tr_symlink)
#define	XFS_CREATE_LOG_RES(mp)	((mp)->m_reservations.tr_create)
#define	XFS_MKDIR_LOG_RES(mp)	((mp)->m_reservations.tr_mkdir)
#define	XFS_IFREE_LOG_RES(mp)	((mp)->m_reservations.tr_ifree)
#define	XFS_ICHANGE_LOG_RES(mp)	((mp)->m_reservations.tr_ichange)
#define	XFS_GROWDATA_LOG_RES(mp)    ((mp)->m_reservations.tr_growdata)
#define	XFS_GROWRTALLOC_LOG_RES(mp)	((mp)->m_reservations.tr_growrtalloc)
#define	XFS_GROWRTZERO_LOG_RES(mp)	((mp)->m_reservations.tr_growrtzero)
#define	XFS_GROWRTFREE_LOG_RES(mp)	((mp)->m_reservations.tr_growrtfree)
#define	XFS_SWRITE_LOG_RES(mp)	((mp)->m_reservations.tr_swrite)
#define XFS_FSYNC_TS_LOG_RES(mp)        ((mp)->m_reservations.tr_swrite)
#define	XFS_WRITEID_LOG_RES(mp)	((mp)->m_reservations.tr_swrite)
#define	XFS_ADDAFORK_LOG_RES(mp)	((mp)->m_reservations.tr_addafork)
#define	XFS_ATTRINVAL_LOG_RES(mp)	((mp)->m_reservations.tr_attrinval)
#define	XFS_ATTRSET_LOG_RES(mp, ext)	\
	((mp)->m_reservations.tr_attrset + \
	 (ext * (mp)->m_sb.sb_sectsize) + \
	 (ext * XFS_FSB_TO_B((mp), XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK))) + \
	 (128 * (ext + (ext * XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK)))))
#define	XFS_ATTRRM_LOG_RES(mp)	((mp)->m_reservations.tr_attrrm)
#define	XFS_CLEAR_AGI_BUCKET_LOG_RES(mp)  ((mp)->m_reservations.tr_clearagi)


#define	XFS_DEFAULT_LOG_COUNT		1
#define	XFS_DEFAULT_PERM_LOG_COUNT	2
#define	XFS_ITRUNCATE_LOG_COUNT		2
#define XFS_INACTIVE_LOG_COUNT		2
#define	XFS_CREATE_LOG_COUNT		2
#define	XFS_MKDIR_LOG_COUNT		3
#define	XFS_SYMLINK_LOG_COUNT		3
#define	XFS_REMOVE_LOG_COUNT		2
#define	XFS_LINK_LOG_COUNT		2
#define	XFS_RENAME_LOG_COUNT		2
#define	XFS_WRITE_LOG_COUNT		2
#define	XFS_ADDAFORK_LOG_COUNT		2
#define	XFS_ATTRINVAL_LOG_COUNT		1
#define	XFS_ATTRSET_LOG_COUNT		3
#define	XFS_ATTRRM_LOG_COUNT		3

#define	XFS_AGF_REF		4
#define	XFS_AGI_REF		4
#define	XFS_AGFL_REF		3
#define	XFS_INO_BTREE_REF	3
#define	XFS_ALLOC_BTREE_REF	2
#define	XFS_BMAP_BTREE_REF	2
#define	XFS_DIR_BTREE_REF	2
#define	XFS_INO_REF		2
#define	XFS_ATTR_BTREE_REF	1
#define	XFS_DQUOT_REF		1

#ifdef __KERNEL__

struct xfs_buf;
struct xfs_buftarg;
struct xfs_efd_log_item;
struct xfs_efi_log_item;
struct xfs_inode;
struct xfs_item_ops;
struct xfs_log_iovec;
struct xfs_log_item_desc;
struct xfs_mount;
struct xfs_trans;
struct xfs_dquot_acct;
struct xfs_busy_extent;

typedef struct xfs_log_item {
	struct list_head		li_ail;		
	xfs_lsn_t			li_lsn;		
	struct xfs_log_item_desc	*li_desc;	
	struct xfs_mount		*li_mountp;	
	struct xfs_ail			*li_ailp;	
	uint				li_type;	
	uint				li_flags;	
	struct xfs_log_item		*li_bio_list;	
	void				(*li_cb)(struct xfs_buf *,
						 struct xfs_log_item *);
							
							
	const struct xfs_item_ops	*li_ops;	

	
	struct list_head		li_cil;		
	struct xfs_log_vec		*li_lv;		
	xfs_lsn_t			li_seq;		
} xfs_log_item_t;

#define	XFS_LI_IN_AIL	0x1
#define XFS_LI_ABORTED	0x2

#define XFS_LI_FLAGS \
	{ XFS_LI_IN_AIL,	"IN_AIL" }, \
	{ XFS_LI_ABORTED,	"ABORTED" }

struct xfs_item_ops {
	uint (*iop_size)(xfs_log_item_t *);
	void (*iop_format)(xfs_log_item_t *, struct xfs_log_iovec *);
	void (*iop_pin)(xfs_log_item_t *);
	void (*iop_unpin)(xfs_log_item_t *, int remove);
	uint (*iop_trylock)(xfs_log_item_t *);
	void (*iop_unlock)(xfs_log_item_t *);
	xfs_lsn_t (*iop_committed)(xfs_log_item_t *, xfs_lsn_t);
	void (*iop_push)(xfs_log_item_t *);
	bool (*iop_pushbuf)(xfs_log_item_t *);
	void (*iop_committing)(xfs_log_item_t *, xfs_lsn_t);
};

#define IOP_SIZE(ip)		(*(ip)->li_ops->iop_size)(ip)
#define IOP_FORMAT(ip,vp)	(*(ip)->li_ops->iop_format)(ip, vp)
#define IOP_PIN(ip)		(*(ip)->li_ops->iop_pin)(ip)
#define IOP_UNPIN(ip, remove)	(*(ip)->li_ops->iop_unpin)(ip, remove)
#define IOP_TRYLOCK(ip)		(*(ip)->li_ops->iop_trylock)(ip)
#define IOP_UNLOCK(ip)		(*(ip)->li_ops->iop_unlock)(ip)
#define IOP_COMMITTED(ip, lsn)	(*(ip)->li_ops->iop_committed)(ip, lsn)
#define IOP_PUSH(ip)		(*(ip)->li_ops->iop_push)(ip)
#define IOP_PUSHBUF(ip)		(*(ip)->li_ops->iop_pushbuf)(ip)
#define IOP_COMMITTING(ip, lsn) (*(ip)->li_ops->iop_committing)(ip, lsn)

#define	XFS_ITEM_SUCCESS	0
#define	XFS_ITEM_PINNED		1
#define	XFS_ITEM_LOCKED		2
#define XFS_ITEM_PUSHBUF	3

typedef void (*xfs_trans_callback_t)(struct xfs_trans *, void *);

typedef struct xfs_trans {
	unsigned int		t_magic;	
	xfs_log_callback_t	t_logcb;	
	unsigned int		t_type;		
	unsigned int		t_log_res;	
	unsigned int		t_log_count;	
	unsigned int		t_blk_res;	
	unsigned int		t_blk_res_used;	
	unsigned int		t_rtx_res;	
	unsigned int		t_rtx_res_used;	
	struct xlog_ticket	*t_ticket;	
	xfs_lsn_t		t_lsn;		
	xfs_lsn_t		t_commit_lsn;	
	struct xfs_mount	*t_mountp;	
	struct xfs_dquot_acct   *t_dqinfo;	
	unsigned int		t_flags;	
	int64_t			t_icount_delta;	
	int64_t			t_ifree_delta;	
	int64_t			t_fdblocks_delta; 
	int64_t			t_res_fdblocks_delta; 
	int64_t			t_frextents_delta;
	int64_t			t_res_frextents_delta; 
#ifdef DEBUG
	int64_t			t_ag_freeblks_delta; 
	int64_t			t_ag_flist_delta; 
	int64_t			t_ag_btree_delta; 
#endif
	int64_t			t_dblocks_delta;
	int64_t			t_agcount_delta;
	int64_t			t_imaxpct_delta;
	int64_t			t_rextsize_delta;
	int64_t			t_rbmblocks_delta;
	int64_t			t_rblocks_delta;
	int64_t			t_rextents_delta;
	int64_t			t_rextslog_delta;
	struct list_head	t_items;	
	xfs_trans_header_t	t_header;	
	struct list_head	t_busy;		
	unsigned long		t_pflags;	
} xfs_trans_t;

#define	xfs_trans_get_log_res(tp)	((tp)->t_log_res)
#define	xfs_trans_get_log_count(tp)	((tp)->t_log_count)
#define	xfs_trans_get_block_res(tp)	((tp)->t_blk_res)
#define	xfs_trans_set_sync(tp)		((tp)->t_flags |= XFS_TRANS_SYNC)

#ifdef DEBUG
#define	xfs_trans_agblocks_delta(tp, d)	((tp)->t_ag_freeblks_delta += (int64_t)d)
#define	xfs_trans_agflist_delta(tp, d)	((tp)->t_ag_flist_delta += (int64_t)d)
#define	xfs_trans_agbtree_delta(tp, d)	((tp)->t_ag_btree_delta += (int64_t)d)
#else
#define	xfs_trans_agblocks_delta(tp, d)
#define	xfs_trans_agflist_delta(tp, d)
#define	xfs_trans_agbtree_delta(tp, d)
#endif

xfs_trans_t	*xfs_trans_alloc(struct xfs_mount *, uint);
xfs_trans_t	*_xfs_trans_alloc(struct xfs_mount *, uint, uint);
xfs_trans_t	*xfs_trans_dup(xfs_trans_t *);
int		xfs_trans_reserve(xfs_trans_t *, uint, uint, uint,
				  uint, uint);
void		xfs_trans_mod_sb(xfs_trans_t *, uint, int64_t);
struct xfs_buf	*xfs_trans_get_buf(xfs_trans_t *, struct xfs_buftarg *, xfs_daddr_t,
				   int, uint);
int		xfs_trans_read_buf(struct xfs_mount *, xfs_trans_t *,
				   struct xfs_buftarg *, xfs_daddr_t, int, uint,
				   struct xfs_buf **);
struct xfs_buf	*xfs_trans_getsb(xfs_trans_t *, struct xfs_mount *, int);

void		xfs_trans_brelse(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_bjoin(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_bhold(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_bhold_release(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_binval(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_inode_buf(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_stale_inode_buf(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_dquot_buf(xfs_trans_t *, struct xfs_buf *, uint);
void		xfs_trans_inode_alloc_buf(xfs_trans_t *, struct xfs_buf *);
void		xfs_trans_ichgtime(struct xfs_trans *, struct xfs_inode *, int);
void		xfs_trans_ijoin(struct xfs_trans *, struct xfs_inode *, uint);
void		xfs_trans_log_buf(xfs_trans_t *, struct xfs_buf *, uint, uint);
void		xfs_trans_log_inode(xfs_trans_t *, struct xfs_inode *, uint);
struct xfs_efi_log_item	*xfs_trans_get_efi(xfs_trans_t *, uint);
void		xfs_efi_release(struct xfs_efi_log_item *, uint);
void		xfs_trans_log_efi_extent(xfs_trans_t *,
					 struct xfs_efi_log_item *,
					 xfs_fsblock_t,
					 xfs_extlen_t);
struct xfs_efd_log_item	*xfs_trans_get_efd(xfs_trans_t *,
				  struct xfs_efi_log_item *,
				  uint);
void		xfs_trans_log_efd_extent(xfs_trans_t *,
					 struct xfs_efd_log_item *,
					 xfs_fsblock_t,
					 xfs_extlen_t);
int		xfs_trans_commit(xfs_trans_t *, uint flags);
void		xfs_trans_cancel(xfs_trans_t *, int);
int		xfs_trans_ail_init(struct xfs_mount *);
void		xfs_trans_ail_destroy(struct xfs_mount *);

extern kmem_zone_t	*xfs_trans_zone;
extern kmem_zone_t	*xfs_log_item_desc_zone;

#endif	

void		xfs_trans_init(struct xfs_mount *);
int		xfs_trans_roll(struct xfs_trans **, struct xfs_inode *);

#endif	
