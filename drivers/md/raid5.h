#ifndef _RAID5_H
#define _RAID5_H

#include <linux/raid/xor.h>
#include <linux/dmaengine.h>

/*
 *
 * Each stripe contains one buffer per device.  Each buffer can be in
 * one of a number of states stored in "flags".  Changes between
 * these states happen *almost* exclusively under the protection of the
 * STRIPE_ACTIVE flag.  Some very specific changes can happen in bi_end_io, and
 * these are not protected by STRIPE_ACTIVE.
 *
 * The flag bits that are used to represent these states are:
 *   R5_UPTODATE and R5_LOCKED
 *
 * State Empty == !UPTODATE, !LOCK
 *        We have no data, and there is no active request
 * State Want == !UPTODATE, LOCK
 *        A read request is being submitted for this block
 * State Dirty == UPTODATE, LOCK
 *        Some new data is in this buffer, and it is being written out
 * State Clean == UPTODATE, !LOCK
 *        We have valid data which is the same as on disc
 *
 * The possible state transitions are:
 *
 *  Empty -> Want   - on read or write to get old data for  parity calc
 *  Empty -> Dirty  - on compute_parity to satisfy write/sync request.
 *  Empty -> Clean  - on compute_block when computing a block for failed drive
 *  Want  -> Empty  - on failed read
 *  Want  -> Clean  - on successful completion of read request
 *  Dirty -> Clean  - on successful completion of write request
 *  Dirty -> Clean  - on failed write
 *  Clean -> Dirty  - on compute_parity to satisfy write/sync (RECONSTRUCT or RMW)
 *
 * The Want->Empty, Want->Clean, Dirty->Clean, transitions
 * all happen in b_end_io at interrupt time.
 * Each sets the Uptodate bit before releasing the Lock bit.
 * This leaves one multi-stage transition:
 *    Want->Dirty->Clean
 * This is safe because thinking that a Clean buffer is actually dirty
 * will at worst delay some action, and the stripe will be scheduled
 * for attention after the transition is complete.
 *
 * There is one possibility that is not covered by these states.  That
 * is if one drive has failed and there is a spare being rebuilt.  We
 * can't distinguish between a clean block that has been generated
 * from parity calculations, and a clean block that has been
 * successfully written to the spare ( or to parity when resyncing).
 * To distingush these states we have a stripe bit STRIPE_INSYNC that
 * is set whenever a write is scheduled to the spare, or to the parity
 * disc if there is no spare.  A sync request clears this bit, and
 * when we find it set with no buffers locked, we know the sync is
 * complete.
 *
 * Buffers for the md device that arrive via make_request are attached
 * to the appropriate stripe in one of two lists linked on b_reqnext.
 * One list (bh_read) for read requests, one (bh_write) for write.
 * There should never be more than one buffer on the two lists
 * together, but we are not guaranteed of that so we allow for more.
 *
 * If a buffer is on the read list when the associated cache buffer is
 * Uptodate, the data is copied into the read buffer and it's b_end_io
 * routine is called.  This may happen in the end_request routine only
 * if the buffer has just successfully been read.  end_request should
 * remove the buffers from the list and then set the Uptodate bit on
 * the buffer.  Other threads may do this only if they first check
 * that the Uptodate bit is set.  Once they have checked that they may
 * take buffers off the read queue.
 *
 * When a buffer on the write list is committed for write it is copied
 * into the cache buffer, which is then marked dirty, and moved onto a
 * third list, the written list (bh_written).  Once both the parity
 * block and the cached buffer are successfully written, any buffer on
 * a written list can be returned with b_end_io.
 *
 * The write list and read list both act as fifos.  The read list,
 * write list and written list are protected by the device_lock.
 * The device_lock is only for list manipulations and will only be
 * held for a very short time.  It can be claimed from interrupts.
 *
 *
 * Stripes in the stripe cache can be on one of two lists (or on
 * neither).  The "inactive_list" contains stripes which are not
 * currently being used for any request.  They can freely be reused
 * for another stripe.  The "handle_list" contains stripes that need
 * to be handled in some way.  Both of these are fifo queues.  Each
 * stripe is also (potentially) linked to a hash bucket in the hash
 * table so that it can be found by sector number.  Stripes that are
 * not hashed must be on the inactive_list, and will normally be at
 * the front.  All stripes start life this way.
 *
 * The inactive_list, handle_list and hash bucket lists are all protected by the
 * device_lock.
 *  - stripes have a reference counter. If count==0, they are on a list.
 *  - If a stripe might need handling, STRIPE_HANDLE is set.
 *  - When refcount reaches zero, then if STRIPE_HANDLE it is put on
 *    handle_list else inactive_list
 *
 * This, combined with the fact that STRIPE_HANDLE is only ever
 * cleared while a stripe has a non-zero count means that if the
 * refcount is 0 and STRIPE_HANDLE is set, then it is on the
 * handle_list and if recount is 0 and STRIPE_HANDLE is not set, then
 * the stripe is on inactive_list.
 *
 * The possible transitions are:
 *  activate an unhashed/inactive stripe (get_active_stripe())
 *     lockdev check-hash unlink-stripe cnt++ clean-stripe hash-stripe unlockdev
 *  activate a hashed, possibly active stripe (get_active_stripe())
 *     lockdev check-hash if(!cnt++)unlink-stripe unlockdev
 *  attach a request to an active stripe (add_stripe_bh())
 *     lockdev attach-buffer unlockdev
 *  handle a stripe (handle_stripe())
 *     setSTRIPE_ACTIVE,  clrSTRIPE_HANDLE ...
 *		(lockdev check-buffers unlockdev) ..
 *		change-state ..
 *		record io/ops needed clearSTRIPE_ACTIVE schedule io/ops
 *  release an active stripe (release_stripe())
 *     lockdev if (!--cnt) { if  STRIPE_HANDLE, add to handle_list else add to inactive-list } unlockdev
 *
 * The refcount counts each thread that have activated the stripe,
 * plus raid5d if it is handling it, plus one for each active request
 * on a cached buffer, and plus one if the stripe is undergoing stripe
 * operations.
 *
 * The stripe operations are:
 * -copying data between the stripe cache and user application buffers
 * -computing blocks to save a disk access, or to recover a missing block
 * -updating the parity on a write operation (reconstruct write and
 *  read-modify-write)
 * -checking parity correctness
 * -running i/o to disk
 * These operations are carried out by raid5_run_ops which uses the async_tx
 * api to (optionally) offload operations to dedicated hardware engines.
 * When requesting an operation handle_stripe sets the pending bit for the
 * operation and increments the count.  raid5_run_ops is then run whenever
 * the count is non-zero.
 * There are some critical dependencies between the operations that prevent some
 * from being requested while another is in flight.
 * 1/ Parity check operations destroy the in cache version of the parity block,
 *    so we prevent parity dependent operations like writes and compute_blocks
 *    from starting while a check is in progress.  Some dma engines can perform
 *    the check without damaging the parity block, in these cases the parity
 *    block is re-marked up to date (assuming the check was successful) and is
 *    not re-read from disk.
 * 2/ When a write operation is requested we immediately lock the affected
 *    blocks, and mark them as not up to date.  This causes new read requests
 *    to be held off, as well as parity checks and compute block operations.
 * 3/ Once a compute block operation has been requested handle_stripe treats
 *    that block as if it is up to date.  raid5_run_ops guaruntees that any
 *    operation that is dependent on the compute block result is initiated after
 *    the compute block completes.
 */

enum check_states {
	check_state_idle = 0,
	check_state_run, 
	check_state_run_q, 
	check_state_run_pq, 
	check_state_check_result,
	check_state_compute_run, 
	check_state_compute_result,
};

enum reconstruct_states {
	reconstruct_state_idle = 0,
	reconstruct_state_prexor_drain_run,	
	reconstruct_state_drain_run,		
	reconstruct_state_run,			
	reconstruct_state_prexor_drain_result,
	reconstruct_state_drain_result,
	reconstruct_state_result,
};

struct stripe_head {
	struct hlist_node	hash;
	struct list_head	lru;	      
	struct r5conf		*raid_conf;
	short			generation;	
	sector_t		sector;		
	short			pd_idx;		
	short			qd_idx;		
	short			ddf_layout;
	unsigned long		state;		
	atomic_t		count;	      
	int			bm_seq;	
	int			disks;		
	enum check_states	check_state;
	enum reconstruct_states reconstruct_state;
	struct stripe_operations {
		int 		     target, target2;
		enum sum_check_flags zero_sum_result;
		#ifdef CONFIG_MULTICORE_RAID456
		unsigned long	     request;
		wait_queue_head_t    wait_for_ops;
		#endif
	} ops;
	struct r5dev {
		struct bio	req, rreq;
		struct bio_vec	vec, rvec;
		struct page	*page;
		struct bio	*toread, *read, *towrite, *written;
		sector_t	sector;			
		unsigned long	flags;
	} dev[1]; 
};

struct stripe_head_state {
	int syncing, expanding, expanded, replacing;
	int locked, uptodate, to_read, to_write, failed, written;
	int to_fill, compute, req_compute, non_overwrite;
	int failed_num[2];
	int p_failed, q_failed;
	int dec_preread_active;
	unsigned long ops_request;

	struct bio *return_bi;
	struct md_rdev *blocked_rdev;
	int handle_bad_blocks;
};

enum r5dev_flags {
	R5_UPTODATE,	
	R5_LOCKED,	
	R5_DOUBLE_LOCKED,
	R5_OVERWRITE,	
	R5_Insync,	
	R5_Wantread,	
	R5_Wantwrite,
	R5_Overlap,	
	R5_ReadError,	
	R5_ReWrite,	

	R5_Expanded,	
	R5_Wantcompute,	
	R5_Wantfill,	
	R5_Wantdrain,	
	R5_WantFUA,	
	R5_WriteError,	
	R5_MadeGood,	
	R5_ReadRepl,	
	R5_MadeGoodRepl,
	R5_NeedReplace,	
	R5_WantReplace, 
};

enum {
	STRIPE_ACTIVE,
	STRIPE_HANDLE,
	STRIPE_SYNC_REQUESTED,
	STRIPE_SYNCING,
	STRIPE_INSYNC,
	STRIPE_PREREAD_ACTIVE,
	STRIPE_DELAYED,
	STRIPE_DEGRADED,
	STRIPE_BIT_DELAY,
	STRIPE_EXPANDING,
	STRIPE_EXPAND_SOURCE,
	STRIPE_EXPAND_READY,
	STRIPE_IO_STARTED,	
	STRIPE_FULL_WRITE,	/* all blocks are set to be overwritten */
	STRIPE_BIOFILL_RUN,
	STRIPE_COMPUTE_RUN,
	STRIPE_OPS_REQ_PENDING,
};

enum {
	STRIPE_OP_BIOFILL,
	STRIPE_OP_COMPUTE_BLK,
	STRIPE_OP_PREXOR,
	STRIPE_OP_BIODRAIN,
	STRIPE_OP_RECONSTRUCT,
	STRIPE_OP_CHECK,
};


struct disk_info {
	struct md_rdev	*rdev, *replacement;
};

struct r5conf {
	struct hlist_head	*stripe_hashtbl;
	struct mddev		*mddev;
	int			chunk_sectors;
	int			level, algorithm;
	int			max_degraded;
	int			raid_disks;
	int			max_nr_stripes;

	sector_t		reshape_progress;
	sector_t		reshape_safe;
	int			previous_raid_disks;
	int			prev_chunk_sectors;
	int			prev_algo;
	short			generation; 
	unsigned long		reshape_checkpoint; 

	struct list_head	handle_list; 
	struct list_head	hold_list; 
	struct list_head	delayed_list; 
	struct list_head	bitmap_list; 
	struct bio		*retry_read_aligned; 
	struct bio		*retry_read_aligned_list; 
	atomic_t		preread_active_stripes; 
	atomic_t		active_aligned_reads;
	atomic_t		pending_full_writes; 
	int			bypass_count; 
	int			bypass_threshold; 
	struct list_head	*last_hold; 

	atomic_t		reshape_stripes; 
	int			active_name;
	char			cache_name[2][32];
	struct kmem_cache		*slab_cache; 

	int			seq_flush, seq_write;
	int			quiesce;

	int			fullsync;  
	int			recovery_disabled;
	
	struct raid5_percpu {
		struct page	*spare_page; 
		void		*scribble;   
	} __percpu *percpu;
	size_t			scribble_len; 
#ifdef CONFIG_HOTPLUG_CPU
	struct notifier_block	cpu_notify;
#endif

	atomic_t		active_stripes;
	struct list_head	inactive_list;
	wait_queue_head_t	wait_for_stripe;
	wait_queue_head_t	wait_for_overlap;
	int			inactive_blocked;	
	int			pool_size; 
	spinlock_t		device_lock;
	struct disk_info	*disks;

	struct md_thread	*thread;
};

#define ALGORITHM_LEFT_ASYMMETRIC	0 
#define ALGORITHM_RIGHT_ASYMMETRIC	1 
#define ALGORITHM_LEFT_SYMMETRIC	2 
#define ALGORITHM_RIGHT_SYMMETRIC	3 

#define ALGORITHM_PARITY_0		4 
#define ALGORITHM_PARITY_N		5 


#define ALGORITHM_ROTATING_ZERO_RESTART	8 
#define ALGORITHM_ROTATING_N_RESTART	9 
#define ALGORITHM_ROTATING_N_CONTINUE	10 


#define ALGORITHM_LEFT_ASYMMETRIC_6	16
#define ALGORITHM_RIGHT_ASYMMETRIC_6	17
#define ALGORITHM_LEFT_SYMMETRIC_6	18
#define ALGORITHM_RIGHT_SYMMETRIC_6	19
#define ALGORITHM_PARITY_0_6		20
#define ALGORITHM_PARITY_N_6		ALGORITHM_PARITY_N

static inline int algorithm_valid_raid5(int layout)
{
	return (layout >= 0) &&
		(layout <= 5);
}
static inline int algorithm_valid_raid6(int layout)
{
	return (layout >= 0 && layout <= 5)
		||
		(layout >= 8 && layout <= 10)
		||
		(layout >= 16 && layout <= 20);
}

static inline int algorithm_is_DDF(int layout)
{
	return layout >= 8 && layout <= 10;
}

extern int md_raid5_congested(struct mddev *mddev, int bits);
extern void md_raid5_kick_device(struct r5conf *conf);
extern int raid5_set_cache_size(struct mddev *mddev, int size);
#endif
