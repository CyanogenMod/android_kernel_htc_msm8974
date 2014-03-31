/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
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
#ifndef	__XFS_LOG_PRIV_H__
#define __XFS_LOG_PRIV_H__

struct xfs_buf;
struct log;
struct xlog_ticket;
struct xfs_mount;


#define XLOG_MIN_ICLOGS		2
#define XLOG_MAX_ICLOGS		8
#define XLOG_HEADER_MAGIC_NUM	0xFEEDbabe	
#define XLOG_VERSION_1		1
#define XLOG_VERSION_2		2		
#define XLOG_VERSION_OKBITS	(XLOG_VERSION_1 | XLOG_VERSION_2)
#define XLOG_MIN_RECORD_BSIZE	(16*1024)	
#define XLOG_BIG_RECORD_BSIZE	(32*1024)	
#define XLOG_MAX_RECORD_BSIZE	(256*1024)
#define XLOG_HEADER_CYCLE_SIZE	(32*1024)	
#define XLOG_MIN_RECORD_BSHIFT	14		
#define XLOG_BIG_RECORD_BSHIFT	15		
#define XLOG_MAX_RECORD_BSHIFT	18		
#define XLOG_BTOLSUNIT(log, b)  (((b)+(log)->l_mp->m_sb.sb_logsunit-1) / \
                                 (log)->l_mp->m_sb.sb_logsunit)
#define XLOG_LSUNITTOB(log, su) ((su) * (log)->l_mp->m_sb.sb_logsunit)

#define XLOG_HEADER_SIZE	512

#define XLOG_REC_SHIFT(log) \
	BTOBB(1 << (xfs_sb_version_haslogv2(&log->l_mp->m_sb) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))
#define XLOG_TOTAL_REC_SHIFT(log) \
	BTOBB(XLOG_MAX_ICLOGS << (xfs_sb_version_haslogv2(&log->l_mp->m_sb) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))

static inline xfs_lsn_t xlog_assign_lsn(uint cycle, uint block)
{
	return ((xfs_lsn_t)cycle << 32) | block;
}

static inline uint xlog_get_cycle(char *ptr)
{
	if (be32_to_cpu(*(__be32 *)ptr) == XLOG_HEADER_MAGIC_NUM)
		return be32_to_cpu(*((__be32 *)ptr + 1));
	else
		return be32_to_cpu(*(__be32 *)ptr);
}

#define BLK_AVG(blk1, blk2)	((blk1+blk2) >> 1)

#ifdef __KERNEL__

static inline uint xlog_get_client_id(__be32 i)
{
	return be32_to_cpu(i) >> 24;
}

#define XLOG_STATE_ACTIVE    0x0001 /* Current IC log being written to */
#define XLOG_STATE_WANT_SYNC 0x0002 
#define XLOG_STATE_SYNCING   0x0004 
#define XLOG_STATE_DONE_SYNC 0x0008 
#define XLOG_STATE_DO_CALLBACK \
			     0x0010 
#define XLOG_STATE_CALLBACK  0x0020 
#define XLOG_STATE_DIRTY     0x0040 
#define XLOG_STATE_IOERROR   0x0080 
#define XLOG_STATE_ALL	     0x7FFF 
#define XLOG_STATE_NOTUSED   0x8000 
#endif	

/*
 * Flags to log operation header
 *
 * The first write of a new transaction will be preceded with a start
 * record, XLOG_START_TRANS.  Once a transaction is committed, a commit
 * record is written, XLOG_COMMIT_TRANS.  If a single region can not fit into
 * the remainder of the current active in-core log, it is split up into
 * multiple regions.  Each partial region will be marked with a
 * XLOG_CONTINUE_TRANS until the last one, which gets marked with XLOG_END_TRANS.
 *
 */
#define XLOG_START_TRANS	0x01	
#define XLOG_COMMIT_TRANS	0x02	
#define XLOG_CONTINUE_TRANS	0x04	
#define XLOG_WAS_CONT_TRANS	0x08	
#define XLOG_END_TRANS		0x10	
#define XLOG_UNMOUNT_TRANS	0x20	

#ifdef __KERNEL__
#define XLOG_TIC_INITED		0x1	
#define XLOG_TIC_PERM_RESERV	0x2	

#define XLOG_TIC_FLAGS \
	{ XLOG_TIC_INITED,	"XLOG_TIC_INITED" }, \
	{ XLOG_TIC_PERM_RESERV,	"XLOG_TIC_PERM_RESERV" }

#endif	

#define XLOG_UNMOUNT_TYPE	0x556e	

#define XLOG_CHKSUM_MISMATCH	0x1	
#define XLOG_ACTIVE_RECOVERY	0x2	
#define	XLOG_RECOVERY_NEEDED	0x4	
#define XLOG_IO_ERROR		0x8	
#define XLOG_TAIL_WARN		0x10	

typedef __uint32_t xlog_tid_t;

#ifdef __KERNEL__

#define XLOG_STATE_COVER_IDLE	0
#define XLOG_STATE_COVER_NEED	1
#define XLOG_STATE_COVER_DONE	2
#define XLOG_STATE_COVER_NEED2	3
#define XLOG_STATE_COVER_DONE2	4

#define XLOG_COVER_OPS		5


 
#define XLOG_TIC_LEN_MAX	15

typedef struct xlog_res {
	uint	r_len;	
	uint	r_type;	
} xlog_res_t;

typedef struct xlog_ticket {
	struct list_head   t_queue;	 
	struct task_struct *t_task;	 
	xlog_tid_t	   t_tid;	 
	atomic_t	   t_ref;	 
	int		   t_curr_res;	 
	int		   t_unit_res;	 
	char		   t_ocnt;	 
	char		   t_cnt;	 
	char		   t_clientid;	 
	char		   t_flags;	 
	uint		   t_trans_type; 

        
	uint		   t_res_num;                    
	uint		   t_res_num_ophdrs;		 
	uint		   t_res_arr_sum;		 
	uint		   t_res_o_flow;		 
	xlog_res_t	   t_res_arr[XLOG_TIC_LEN_MAX];   
} xlog_ticket_t;

#endif


typedef struct xlog_op_header {
	__be32	   oh_tid;	
	__be32	   oh_len;	
	__u8	   oh_clientid;	
	__u8	   oh_flags;	
	__u16	   oh_res2;	
} xlog_op_header_t;


#define XLOG_FMT_UNKNOWN  0
#define XLOG_FMT_LINUX_LE 1
#define XLOG_FMT_LINUX_BE 2
#define XLOG_FMT_IRIX_BE  3

#ifdef XFS_NATIVE_HOST
#define XLOG_FMT XLOG_FMT_LINUX_BE
#else
#define XLOG_FMT XLOG_FMT_LINUX_LE
#endif

typedef struct xlog_rec_header {
	__be32	  h_magicno;	
	__be32	  h_cycle;	
	__be32	  h_version;	
	__be32	  h_len;	
	__be64	  h_lsn;	
	__be64	  h_tail_lsn;	
	__be32	  h_chksum;	
	__be32	  h_prev_block; 
	__be32	  h_num_logops;	
	__be32	  h_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE];
	
	__be32    h_fmt;        
	uuid_t	  h_fs_uuid;    
	__be32	  h_size;	
} xlog_rec_header_t;

typedef struct xlog_rec_ext_header {
	__be32	  xh_cycle;	
	__be32	  xh_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE]; 
} xlog_rec_ext_header_t;

#ifdef __KERNEL__

typedef union xlog_in_core2 {
	xlog_rec_header_t	hic_header;
	xlog_rec_ext_header_t	hic_xheader;
	char			hic_sector[XLOG_HEADER_SIZE];
} xlog_in_core_2_t;

/*
 * - A log record header is 512 bytes.  There is plenty of room to grow the
 *	xlog_rec_header_t into the reserved space.
 * - ic_data follows, so a write to disk can start at the beginning of
 *	the iclog.
 * - ic_forcewait is used to implement synchronous forcing of the iclog to disk.
 * - ic_next is the pointer to the next iclog in the ring.
 * - ic_bp is a pointer to the buffer used to write this incore log to disk.
 * - ic_log is a pointer back to the global log structure.
 * - ic_callback is a linked list of callback function/argument pairs to be
 *	called after an iclog finishes writing.
 * - ic_size is the full size of the header plus data.
 * - ic_offset is the current number of bytes written to in this iclog.
 * - ic_refcnt is bumped when someone is writing to the log.
 * - ic_state is the state of the iclog.
 *
 * Because of cacheline contention on large machines, we need to separate
 * various resources onto different cachelines. To start with, make the
 * structure cacheline aligned. The following fields can be contended on
 * by independent processes:
 *
 *	- ic_callback_*
 *	- ic_refcnt
 *	- fields protected by the global l_icloglock
 *
 * so we need to ensure that these fields are located in separate cachelines.
 * We'll put all the read-only and l_icloglock fields in the first cacheline,
 * and move everything else out to subsequent cachelines.
 */
typedef struct xlog_in_core {
	wait_queue_head_t	ic_force_wait;
	wait_queue_head_t	ic_write_wait;
	struct xlog_in_core	*ic_next;
	struct xlog_in_core	*ic_prev;
	struct xfs_buf		*ic_bp;
	struct log		*ic_log;
	int			ic_size;
	int			ic_offset;
	int			ic_bwritecnt;
	unsigned short		ic_state;
	char			*ic_datap;	

	
	spinlock_t		ic_callback_lock ____cacheline_aligned_in_smp;
	xfs_log_callback_t	*ic_callback;
	xfs_log_callback_t	**ic_callback_tail;

	
	atomic_t		ic_refcnt ____cacheline_aligned_in_smp;
	xlog_in_core_2_t	*ic_data;
#define ic_header	ic_data->hic_header
} xlog_in_core_t;

struct xfs_cil;

struct xfs_cil_ctx {
	struct xfs_cil		*cil;
	xfs_lsn_t		sequence;	
	xfs_lsn_t		start_lsn;	
	xfs_lsn_t		commit_lsn;	
	struct xlog_ticket	*ticket;	
	int			nvecs;		
	int			space_used;	
	struct list_head	busy_extents;	
	struct xfs_log_vec	*lv_chain;	
	xfs_log_callback_t	log_cb;		
	struct list_head	committing;	
};

/*
 * Committed Item List structure
 *
 * This structure is used to track log items that have been committed but not
 * yet written into the log. It is used only when the delayed logging mount
 * option is enabled.
 *
 * This structure tracks the list of committing checkpoint contexts so
 * we can avoid the problem of having to hold out new transactions during a
 * flush until we have a the commit record LSN of the checkpoint. We can
 * traverse the list of committing contexts in xlog_cil_push_lsn() to find a
 * sequence match and extract the commit LSN directly from there. If the
 * checkpoint is still in the process of committing, we can block waiting for
 * the commit LSN to be determined as well. This should make synchronous
 * operations almost as efficient as the old logging methods.
 */
struct xfs_cil {
	struct log		*xc_log;
	struct list_head	xc_cil;
	spinlock_t		xc_cil_lock;
	struct xfs_cil_ctx	*xc_ctx;
	struct rw_semaphore	xc_ctx_lock;
	struct list_head	xc_committing;
	wait_queue_head_t	xc_commit_wait;
	xfs_lsn_t		xc_current_sequence;
};

#define XLOG_CIL_SPACE_LIMIT(log)	(log->l_logsize >> 3)
#define XLOG_CIL_HARD_SPACE_LIMIT(log)	(3 * (log->l_logsize >> 4))

struct xlog_grant_head {
	spinlock_t		lock ____cacheline_aligned_in_smp;
	struct list_head	waiters;
	atomic64_t		grant;
};

typedef struct log {
	
	struct xfs_mount	*l_mp;	        
	struct xfs_ail		*l_ailp;	
	struct xfs_cil		*l_cilp;	
	struct xfs_buf		*l_xbuf;        
	struct xfs_buftarg	*l_targ;        
	uint			l_flags;
	uint			l_quotaoffs_flag; 
	struct list_head	*l_buf_cancel_table;
	int			l_iclog_hsize;  
	int			l_iclog_heads;  
	uint			l_sectBBsize;   
	int			l_iclog_size;	
	int			l_iclog_size_log; 
	int			l_iclog_bufs;	
	xfs_daddr_t		l_logBBstart;   
	int			l_logsize;      
	int			l_logBBsize;    

	
	wait_queue_head_t	l_flush_wait ____cacheline_aligned_in_smp;
						
	int			l_covered_state;
	xlog_in_core_t		*l_iclog;       
	spinlock_t		l_icloglock;    
	int			l_curr_cycle;   
	int			l_prev_cycle;   
	int			l_curr_block;   
	int			l_prev_block;   

	
	atomic64_t		l_last_sync_lsn ____cacheline_aligned_in_smp;
	
	atomic64_t		l_tail_lsn ____cacheline_aligned_in_smp;

	struct xlog_grant_head	l_reserve_head;
	struct xlog_grant_head	l_write_head;

	
#ifdef DEBUG
	char			*l_iclog_bak[XLOG_MAX_ICLOGS];
#endif

} xlog_t;

#define XLOG_BUF_CANCEL_BUCKET(log, blkno) \
	((log)->l_buf_cancel_table + ((__uint64_t)blkno % XLOG_BC_TABLE_SIZE))

#define XLOG_FORCED_SHUTDOWN(log)	((log)->l_flags & XLOG_IO_ERROR)

extern int	 xlog_recover(xlog_t *log);
extern int	 xlog_recover_finish(xlog_t *log);
extern void	 xlog_pack_data(xlog_t *log, xlog_in_core_t *iclog, int);

extern kmem_zone_t *xfs_log_ticket_zone;
struct xlog_ticket *xlog_ticket_alloc(struct log *log, int unit_bytes,
				int count, char client, bool permanent,
				int alloc_flags);


static inline void
xlog_write_adv_cnt(void **ptr, int *len, int *off, size_t bytes)
{
	*ptr += bytes;
	*len -= bytes;
	*off += bytes;
}

void	xlog_print_tic_res(struct xfs_mount *mp, struct xlog_ticket *ticket);
int	xlog_write(struct log *log, struct xfs_log_vec *log_vector,
				struct xlog_ticket *tic, xfs_lsn_t *start_lsn,
				xlog_in_core_t **commit_iclog, uint flags);

static inline void
xlog_crack_atomic_lsn(atomic64_t *lsn, uint *cycle, uint *block)
{
	xfs_lsn_t val = atomic64_read(lsn);

	*cycle = CYCLE_LSN(val);
	*block = BLOCK_LSN(val);
}

static inline void
xlog_assign_atomic_lsn(atomic64_t *lsn, uint cycle, uint block)
{
	atomic64_set(lsn, xlog_assign_lsn(cycle, block));
}

static inline void
xlog_crack_grant_head_val(int64_t val, int *cycle, int *space)
{
	*cycle = val >> 32;
	*space = val & 0xffffffff;
}

static inline void
xlog_crack_grant_head(atomic64_t *head, int *cycle, int *space)
{
	xlog_crack_grant_head_val(atomic64_read(head), cycle, space);
}

static inline int64_t
xlog_assign_grant_head_val(int cycle, int space)
{
	return ((int64_t)cycle << 32) | space;
}

static inline void
xlog_assign_grant_head(atomic64_t *head, int cycle, int space)
{
	atomic64_set(head, xlog_assign_grant_head_val(cycle, space));
}

int	xlog_cil_init(struct log *log);
void	xlog_cil_init_post_recovery(struct log *log);
void	xlog_cil_destroy(struct log *log);

xfs_lsn_t xlog_cil_force_lsn(struct log *log, xfs_lsn_t sequence);

static inline void
xlog_cil_force(struct log *log)
{
	xlog_cil_force_lsn(log, log->l_cilp->xc_current_sequence);
}

#define XLOG_UNMOUNT_REC_TYPE	(-1U)

static inline void xlog_wait(wait_queue_head_t *wq, spinlock_t *lock)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue_exclusive(wq, &wait);
	__set_current_state(TASK_UNINTERRUPTIBLE);
	spin_unlock(lock);
	schedule();
	remove_wait_queue(wq, &wait);
}
#endif	

#endif	
