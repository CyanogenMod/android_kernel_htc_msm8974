/*
 * linux/include/linux/jbd.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD_H
#define _LINUX_JBD_H

#ifndef __KERNEL__
#include "jfs_compat.h"
#define JFS_DEBUG
#define jfs_debug jbd_debug
#else

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/journal-head.h>
#include <linux/stddef.h>
#include <linux/bit_spinlock.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/lockdep.h>
#include <linux/slab.h>

#define journal_oom_retry 1

#undef JBD_PARANOID_IOFAIL

#define JBD_DEFAULT_MAX_COMMIT_AGE 5

#ifdef CONFIG_JBD_DEBUG
#define JBD_EXPENSIVE_CHECKING
extern u8 journal_enable_debug;

#define jbd_debug(n, f, a...)						\
	do {								\
		if ((n) <= journal_enable_debug) {			\
			printk (KERN_DEBUG "(%s, %d): %s: ",		\
				__FILE__, __LINE__, __func__);	\
			printk (f, ## a);				\
		}							\
	} while (0)
#else
#define jbd_debug(f, a...)	
#endif

static inline void *jbd_alloc(size_t size, gfp_t flags)
{
	return (void *)__get_free_pages(flags, get_order(size));
}

static inline void jbd_free(void *ptr, size_t size)
{
	free_pages((unsigned long)ptr, get_order(size));
};

#define JFS_MIN_JOURNAL_BLOCKS 1024


typedef struct handle_s		handle_t;	


typedef struct journal_s	journal_t;	
#endif


#define JFS_MAGIC_NUMBER 0xc03b3998U 



#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK_V1	3
#define JFS_SUPERBLOCK_V2	4
#define JFS_REVOKE_BLOCK	5

typedef struct journal_header_s
{
	__be32		h_magic;
	__be32		h_blocktype;
	__be32		h_sequence;
} journal_header_t;


typedef struct journal_block_tag_s
{
	__be32		t_blocknr;	
	__be32		t_flags;	
} journal_block_tag_t;

typedef struct journal_revoke_header_s
{
	journal_header_t r_header;
	__be32		 r_count;	
} journal_revoke_header_t;


#define JFS_FLAG_ESCAPE		1	
#define JFS_FLAG_SAME_UUID	2	
#define JFS_FLAG_DELETED	4	
#define JFS_FLAG_LAST_TAG	8	


typedef struct journal_superblock_s
{
	journal_header_t s_header;

	
	__be32	s_blocksize;		
	__be32	s_maxlen;		
	__be32	s_first;		

	
	__be32	s_sequence;		
	__be32	s_start;		

	
	__be32	s_errno;

	
	__be32	s_feature_compat;	
	__be32	s_feature_incompat;	
	__be32	s_feature_ro_compat;	
	__u8	s_uuid[16];		

	__be32	s_nr_users;		

	__be32	s_dynsuper;		

	__be32	s_max_transaction;	
	__be32	s_max_trans_data;	

	__u32	s_padding[44];

	__u8	s_users[16*48];		
} journal_superblock_t;

#define JFS_HAS_COMPAT_FEATURE(j,mask)					\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_compat & cpu_to_be32((mask))))
#define JFS_HAS_RO_COMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_ro_compat & cpu_to_be32((mask))))
#define JFS_HAS_INCOMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_incompat & cpu_to_be32((mask))))

#define JFS_FEATURE_INCOMPAT_REVOKE	0x00000001

#define JFS_KNOWN_COMPAT_FEATURES	0
#define JFS_KNOWN_ROCOMPAT_FEATURES	0
#define JFS_KNOWN_INCOMPAT_FEATURES	JFS_FEATURE_INCOMPAT_REVOKE

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/jbd_common.h>

#define J_ASSERT(assert)	BUG_ON(!(assert))

#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)

#if defined(JBD_PARANOID_IOFAIL)
#define J_EXPECT(expr, why...)		J_ASSERT(expr)
#define J_EXPECT_BH(bh, expr, why...)	J_ASSERT_BH(bh, expr)
#define J_EXPECT_JH(jh, expr, why...)	J_ASSERT_JH(jh, expr)
#else
#define __journal_expect(expr, why...)					     \
	({								     \
		int val = (expr);					     \
		if (!val) {						     \
			printk(KERN_ERR					     \
				"EXT3-fs unexpected failure: %s;\n",# expr); \
			printk(KERN_ERR why "\n");			     \
		}							     \
		val;							     \
	})
#define J_EXPECT(expr, why...)		__journal_expect(expr, ## why)
#define J_EXPECT_BH(bh, expr, why...)	__journal_expect(expr, ## why)
#define J_EXPECT_JH(jh, expr, why...)	__journal_expect(expr, ## why)
#endif

struct jbd_revoke_table_s;

struct handle_s
{
	
	transaction_t		*h_transaction;

	
	int			h_buffer_credits;

	
	int			h_ref;

	
	
	int			h_err;

	
	unsigned int	h_sync:		1;	
	unsigned int	h_jdata:	1;	
	unsigned int	h_aborted:	1;	

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	h_lockdep_map;
#endif
};




struct transaction_s
{
	
	journal_t		*t_journal;

	
	tid_t			t_tid;

	enum {
		T_RUNNING,
		T_LOCKED,
		T_FLUSH,
		T_COMMIT,
		T_COMMIT_RECORD,
		T_FINISHED
	}			t_state;

	unsigned int		t_log_start;

	
	int			t_nr_buffers;

	struct journal_head	*t_reserved_list;

	struct journal_head	*t_locked_list;

	struct journal_head	*t_buffers;

	struct journal_head	*t_sync_datalist;

	struct journal_head	*t_forget;

	struct journal_head	*t_checkpoint_list;

	struct journal_head	*t_checkpoint_io_list;

	struct journal_head	*t_iobuf_list;

	struct journal_head	*t_shadow_list;

	/*
	 * Doubly-linked circular list of control buffers being written to the
	 * log. [j_list_lock]
	 */
	struct journal_head	*t_log_list;

	spinlock_t		t_handle_lock;

	int			t_updates;

	int			t_outstanding_credits;

	transaction_t		*t_cpnext, *t_cpprev;

	unsigned long		t_expires;

	ktime_t			t_start_time;

	int t_handle_count;

	unsigned int t_synchronous_commit:1;
};


struct journal_s
{
	
	unsigned long		j_flags;

	int			j_errno;

	
	struct buffer_head	*j_sb_buffer;
	journal_superblock_t	*j_superblock;

	
	int			j_format_version;

	spinlock_t		j_state_lock;

	int			j_barrier_count;

	transaction_t		*j_running_transaction;

	transaction_t		*j_committing_transaction;

	transaction_t		*j_checkpoint_transactions;

	wait_queue_head_t	j_wait_transaction_locked;

	
	wait_queue_head_t	j_wait_logspace;

	
	wait_queue_head_t	j_wait_done_commit;

	
	wait_queue_head_t	j_wait_checkpoint;

	
	wait_queue_head_t	j_wait_commit;

	
	wait_queue_head_t	j_wait_updates;

	
	struct mutex		j_checkpoint_mutex;

	unsigned int		j_head;

	unsigned int		j_tail;

	unsigned int		j_free;

	unsigned int		j_first;
	unsigned int		j_last;

	struct block_device	*j_dev;
	int			j_blocksize;
	unsigned int		j_blk_offset;

	struct block_device	*j_fs_dev;

	
	unsigned int		j_maxlen;

	spinlock_t		j_list_lock;

	
	
	
	struct inode		*j_inode;

	tid_t			j_tail_sequence;

	tid_t			j_transaction_sequence;

	tid_t			j_commit_sequence;

	tid_t			j_commit_request;

	__u8			j_uuid[16];

	
	struct task_struct	*j_task;

	int			j_max_transaction_buffers;

	unsigned long		j_commit_interval;

	
	struct timer_list	j_commit_timer;

	spinlock_t		j_revoke_lock;
	struct jbd_revoke_table_s *j_revoke;
	struct jbd_revoke_table_s *j_revoke_table[2];

	struct buffer_head	**j_wbuf;
	int			j_wbufsize;

	pid_t			j_last_sync_writer;

	u64			j_average_commit_time;

	void *j_private;
};

#define JFS_UNMOUNT	0x001	
#define JFS_ABORT	0x002	
#define JFS_ACK_ERR	0x004	
#define JFS_FLUSHED	0x008	
#define JFS_LOADED	0x010	
#define JFS_BARRIER	0x020	
#define JFS_ABORT_ON_SYNCDATA_ERR	0x040  


extern void journal_unfile_buffer(journal_t *, struct journal_head *);
extern void __journal_unfile_buffer(struct journal_head *);
extern void __journal_refile_buffer(struct journal_head *);
extern void journal_refile_buffer(journal_t *, struct journal_head *);
extern void __journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_free_buffer(struct journal_head *bh);
extern void journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_clean_data_list(transaction_t *transaction);

extern struct journal_head * journal_get_descriptor_buffer(journal_t *);
int journal_next_log_block(journal_t *, unsigned int *);

extern void journal_commit_transaction(journal_t *);

int __journal_clean_checkpoint_list(journal_t *journal);
int __journal_remove_checkpoint(struct journal_head *);
void __journal_insert_checkpoint(struct journal_head *, transaction_t *);

extern int
journal_write_metadata_buffer(transaction_t	  *transaction,
			      struct journal_head  *jh_in,
			      struct journal_head **jh_out,
			      unsigned int blocknr);

extern void		__wait_on_journal (journal_t *);


static inline handle_t *journal_current_handle(void)
{
	return current->journal_info;
}


extern handle_t *journal_start(journal_t *, int nblocks);
extern int	 journal_restart (handle_t *, int nblocks);
extern int	 journal_extend (handle_t *, int nblocks);
extern int	 journal_get_write_access(handle_t *, struct buffer_head *);
extern int	 journal_get_create_access (handle_t *, struct buffer_head *);
extern int	 journal_get_undo_access(handle_t *, struct buffer_head *);
extern int	 journal_dirty_data (handle_t *, struct buffer_head *);
extern int	 journal_dirty_metadata (handle_t *, struct buffer_head *);
extern void	 journal_release_buffer (handle_t *, struct buffer_head *);
extern int	 journal_forget (handle_t *, struct buffer_head *);
extern void	 journal_sync_buffer (struct buffer_head *);
extern void	 journal_invalidatepage(journal_t *,
				struct page *, unsigned long);
extern int	 journal_try_to_free_buffers(journal_t *, struct page *, gfp_t);
extern int	 journal_stop(handle_t *);
extern int	 journal_flush (journal_t *);
extern void	 journal_lock_updates (journal_t *);
extern void	 journal_unlock_updates (journal_t *);

extern journal_t * journal_init_dev(struct block_device *bdev,
				struct block_device *fs_dev,
				int start, int len, int bsize);
extern journal_t * journal_init_inode (struct inode *);
extern int	   journal_update_format (journal_t *);
extern int	   journal_check_used_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_check_available_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_set_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_create     (journal_t *);
extern int	   journal_load       (journal_t *journal);
extern int	   journal_destroy    (journal_t *);
extern int	   journal_recover    (journal_t *journal);
extern int	   journal_wipe       (journal_t *, int);
extern int	   journal_skip_recovery	(journal_t *);
extern void	   journal_update_superblock	(journal_t *, int);
extern void	   journal_abort      (journal_t *, int);
extern int	   journal_errno      (journal_t *);
extern void	   journal_ack_err    (journal_t *);
extern int	   journal_clear_err  (journal_t *);
extern int	   journal_bmap(journal_t *, unsigned int, unsigned int *);
extern int	   journal_force_commit(journal_t *);

struct journal_head *journal_add_journal_head(struct buffer_head *bh);
struct journal_head *journal_grab_journal_head(struct buffer_head *bh);
void journal_put_journal_head(struct journal_head *jh);

extern struct kmem_cache *jbd_handle_cache;

static inline handle_t *jbd_alloc_handle(gfp_t gfp_flags)
{
	return kmem_cache_alloc(jbd_handle_cache, gfp_flags);
}

static inline void jbd_free_handle(handle_t *handle)
{
	kmem_cache_free(jbd_handle_cache, handle);
}

#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   journal_init_revoke(journal_t *, int);
extern void	   journal_destroy_revoke_caches(void);
extern int	   journal_init_revoke_caches(void);

extern void	   journal_destroy_revoke(journal_t *);
extern int	   journal_revoke (handle_t *,
				unsigned int, struct buffer_head *);
extern int	   journal_cancel_revoke(handle_t *, struct journal_head *);
extern void	   journal_write_revoke_records(journal_t *,
						transaction_t *, int);

extern int	journal_set_revoke(journal_t *, unsigned int, tid_t);
extern int	journal_test_revoke(journal_t *, unsigned int, tid_t);
extern void	journal_clear_revoke(journal_t *);
extern void	journal_switch_revoke_table(journal_t *journal);
extern void	journal_clear_buffer_revoked_flags(journal_t *journal);


int __log_space_left(journal_t *); 
int log_start_commit(journal_t *journal, tid_t tid);
int __log_start_commit(journal_t *journal, tid_t tid);
int journal_start_commit(journal_t *journal, tid_t *tid);
int journal_force_commit_nested(journal_t *journal);
int log_wait_commit(journal_t *journal, tid_t tid);
int log_do_checkpoint(journal_t *journal);
int journal_trans_will_send_data_barrier(journal_t *journal, tid_t tid);

void __log_wait_for_space(journal_t *journal);
extern void	__journal_drop_transaction(journal_t *, transaction_t *);
extern int	cleanup_journal_tail(journal_t *);


#define jbd_ENOSYS() \
do {								           \
	printk (KERN_ERR "JBD unimplemented function %s\n", __func__); \
	current->state = TASK_UNINTERRUPTIBLE;			           \
	schedule();						           \
} while (1)


static inline int is_journal_aborted(journal_t *journal)
{
	return journal->j_flags & JFS_ABORT;
}

static inline int is_handle_aborted(handle_t *handle)
{
	if (handle->h_aborted)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void journal_abort_handle(handle_t *handle)
{
	handle->h_aborted = 1;
}

#endif 


static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int journal_blocks_per_page(struct inode *inode);

static inline int jbd_space_needed(journal_t *journal)
{
	int nblocks = journal->j_max_transaction_buffers;
	if (journal->j_committing_transaction)
		nblocks += journal->j_committing_transaction->
					t_outstanding_credits;
	return nblocks;
}


#define BJ_None		0	
#define BJ_SyncData	1	
#define BJ_Metadata	2	
#define BJ_Forget	3	
#define BJ_IO		4	
#define BJ_Shadow	5	
#define BJ_LogCtl	6	
#define BJ_Reserved	7	
#define BJ_Locked	8	
#define BJ_Types	9

extern int jbd_blocks_per_page(struct inode *inode);

#ifdef __KERNEL__

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	

#endif	
