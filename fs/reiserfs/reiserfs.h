/*
 * Copyright 1996, 1997, 1998 Hans Reiser, see reiserfs/README for licensing and copyright details
 */

#include <linux/reiserfs_fs.h>

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/workqueue.h>
#include <asm/unaligned.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <linux/buffer_head.h>

#define REISERFS_IOC32_UNPACK		_IOW(0xCD, 1, int)
#define REISERFS_IOC32_GETFLAGS		FS_IOC32_GETFLAGS
#define REISERFS_IOC32_SETFLAGS		FS_IOC32_SETFLAGS
#define REISERFS_IOC32_GETVERSION	FS_IOC32_GETVERSION
#define REISERFS_IOC32_SETVERSION	FS_IOC32_SETVERSION

struct reiserfs_journal_list;

typedef enum {
	i_item_key_version_mask = 0x0001,
	i_stat_data_version_mask = 0x0002,
    
	i_pack_on_close_mask = 0x0004,
    
	i_nopack_mask = 0x0008,
	i_link_saved_unlink_mask = 0x0010,
	i_link_saved_truncate_mask = 0x0020,
	i_has_xattr_dir = 0x0040,
	i_data_log = 0x0080,
} reiserfs_inode_flags;

struct reiserfs_inode_info {
	__u32 i_key[4];		
	__u32 i_flags;

	__u32 i_first_direct_byte;	

	
	__u32 i_attrs;

	int i_prealloc_block;	
	int i_prealloc_count;	
	struct list_head i_prealloc_list;	

	unsigned new_packing_locality:1;	

	unsigned int i_trans_id;
	struct reiserfs_journal_list *i_jl;
	atomic_t openers;
	struct mutex tailpack;
#ifdef CONFIG_REISERFS_FS_XATTR
	struct rw_semaphore i_xattr_sem;
#endif
	struct inode vfs_inode;
};

typedef enum {
	reiserfs_attrs_cleared = 0x00000001,
} reiserfs_super_block_flags;

#define sb_block_count(sbp)         (le32_to_cpu((sbp)->s_v1.s_block_count))
#define set_sb_block_count(sbp,v)   ((sbp)->s_v1.s_block_count = cpu_to_le32(v))
#define sb_free_blocks(sbp)         (le32_to_cpu((sbp)->s_v1.s_free_blocks))
#define set_sb_free_blocks(sbp,v)   ((sbp)->s_v1.s_free_blocks = cpu_to_le32(v))
#define sb_root_block(sbp)          (le32_to_cpu((sbp)->s_v1.s_root_block))
#define set_sb_root_block(sbp,v)    ((sbp)->s_v1.s_root_block = cpu_to_le32(v))

#define sb_jp_journal_1st_block(sbp)  \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_1st_block))
#define set_sb_jp_journal_1st_block(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_1st_block = cpu_to_le32(v))
#define sb_jp_journal_dev(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_dev))
#define set_sb_jp_journal_dev(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_dev = cpu_to_le32(v))
#define sb_jp_journal_size(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_size))
#define set_sb_jp_journal_size(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_size = cpu_to_le32(v))
#define sb_jp_journal_trans_max(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_trans_max))
#define set_sb_jp_journal_trans_max(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_trans_max = cpu_to_le32(v))
#define sb_jp_journal_magic(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_magic))
#define set_sb_jp_journal_magic(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_magic = cpu_to_le32(v))
#define sb_jp_journal_max_batch(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_max_batch))
#define set_sb_jp_journal_max_batch(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_max_batch = cpu_to_le32(v))
#define sb_jp_jourmal_max_commit_age(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_journal.jp_journal_max_commit_age))
#define set_sb_jp_journal_max_commit_age(sbp,v) \
              ((sbp)->s_v1.s_journal.jp_journal_max_commit_age = cpu_to_le32(v))

#define sb_blocksize(sbp)          (le16_to_cpu((sbp)->s_v1.s_blocksize))
#define set_sb_blocksize(sbp,v)    ((sbp)->s_v1.s_blocksize = cpu_to_le16(v))
#define sb_oid_maxsize(sbp)        (le16_to_cpu((sbp)->s_v1.s_oid_maxsize))
#define set_sb_oid_maxsize(sbp,v)  ((sbp)->s_v1.s_oid_maxsize = cpu_to_le16(v))
#define sb_oid_cursize(sbp)        (le16_to_cpu((sbp)->s_v1.s_oid_cursize))
#define set_sb_oid_cursize(sbp,v)  ((sbp)->s_v1.s_oid_cursize = cpu_to_le16(v))
#define sb_umount_state(sbp)       (le16_to_cpu((sbp)->s_v1.s_umount_state))
#define set_sb_umount_state(sbp,v) ((sbp)->s_v1.s_umount_state = cpu_to_le16(v))
#define sb_fs_state(sbp)           (le16_to_cpu((sbp)->s_v1.s_fs_state))
#define set_sb_fs_state(sbp,v)     ((sbp)->s_v1.s_fs_state = cpu_to_le16(v))
#define sb_hash_function_code(sbp) \
              (le32_to_cpu((sbp)->s_v1.s_hash_function_code))
#define set_sb_hash_function_code(sbp,v) \
              ((sbp)->s_v1.s_hash_function_code = cpu_to_le32(v))
#define sb_tree_height(sbp)        (le16_to_cpu((sbp)->s_v1.s_tree_height))
#define set_sb_tree_height(sbp,v)  ((sbp)->s_v1.s_tree_height = cpu_to_le16(v))
#define sb_bmap_nr(sbp)            (le16_to_cpu((sbp)->s_v1.s_bmap_nr))
#define set_sb_bmap_nr(sbp,v)      ((sbp)->s_v1.s_bmap_nr = cpu_to_le16(v))
#define sb_version(sbp)            (le16_to_cpu((sbp)->s_v1.s_version))
#define set_sb_version(sbp,v)      ((sbp)->s_v1.s_version = cpu_to_le16(v))

#define sb_mnt_count(sbp)	   (le16_to_cpu((sbp)->s_mnt_count))
#define set_sb_mnt_count(sbp, v)   ((sbp)->s_mnt_count = cpu_to_le16(v))

#define sb_reserved_for_journal(sbp) \
              (le16_to_cpu((sbp)->s_v1.s_reserved_for_journal))
#define set_sb_reserved_for_journal(sbp,v) \
              ((sbp)->s_v1.s_reserved_for_journal = cpu_to_le16(v))



				
#define JOURNAL_BLOCK_SIZE  4096	
#define JOURNAL_MAX_CNODE   1500	
#define JOURNAL_HASH_SIZE 8192
#define JOURNAL_NUM_BITMAPS 5	

struct reiserfs_journal_cnode {
	struct buffer_head *bh;	
	struct super_block *sb;	
	__u32 blocknr;		
	unsigned long state;
	struct reiserfs_journal_list *jlist;	
	struct reiserfs_journal_cnode *next;	
	struct reiserfs_journal_cnode *prev;	
	struct reiserfs_journal_cnode *hprev;	
	struct reiserfs_journal_cnode *hnext;	
};

struct reiserfs_bitmap_node {
	int id;
	char *data;
	struct list_head list;
};

struct reiserfs_list_bitmap {
	struct reiserfs_journal_list *journal_list;
	struct reiserfs_bitmap_node **bitmaps;
};

/*
** one of these for each transaction.  The most important part here is the j_realblock.
** this list of cnodes is used to hash all the blocks in all the commits, to mark all the
** real buffer heads dirty once all the commits hit the disk,
** and to make sure every real block in a transaction is on disk before allowing the log area
** to be overwritten */
struct reiserfs_journal_list {
	unsigned long j_start;
	unsigned long j_state;
	unsigned long j_len;
	atomic_t j_nonzerolen;
	atomic_t j_commit_left;
	atomic_t j_older_commits_done;	
	struct mutex j_commit_mutex;
	unsigned int j_trans_id;
	time_t j_timestamp;
	struct reiserfs_list_bitmap *j_list_bitmap;
	struct buffer_head *j_commit_bh;	
	struct reiserfs_journal_cnode *j_realblock;
	struct reiserfs_journal_cnode *j_freedlist;	
	
	struct list_head j_list;

	
	struct list_head j_working_list;

	
	struct list_head j_tail_bh_list;
	
	struct list_head j_bh_list;
	int j_refcount;
};

struct reiserfs_journal {
	struct buffer_head **j_ap_blocks;	
	struct reiserfs_journal_cnode *j_last;	
	struct reiserfs_journal_cnode *j_first;	

	struct block_device *j_dev_bd;
	fmode_t j_dev_mode;
	int j_1st_reserved_block;	

	unsigned long j_state;
	unsigned int j_trans_id;
	unsigned long j_mount_id;
	unsigned long j_start;	
	unsigned long j_len;	
	unsigned long j_len_alloc;	
	atomic_t j_wcount;	
	unsigned long j_bcount;	
	unsigned long j_first_unflushed_offset;	
	unsigned j_last_flush_trans_id;	
	struct buffer_head *j_header_bh;

	time_t j_trans_start_time;	
	struct mutex j_mutex;
	struct mutex j_flush_mutex;
	wait_queue_head_t j_join_wait;	
	atomic_t j_jlock;	
	int j_list_bitmap_index;	
	int j_must_wait;	
	int j_next_full_flush;	
	int j_next_async_flush;	

	int j_cnode_used;	
	int j_cnode_free;	

	unsigned int j_trans_max;	
	unsigned int j_max_batch;	
	unsigned int j_max_commit_age;	
	unsigned int j_max_trans_age;	
	unsigned int j_default_max_commit_age;	

	struct reiserfs_journal_cnode *j_cnode_free_list;
	struct reiserfs_journal_cnode *j_cnode_free_orig;	

	struct reiserfs_journal_list *j_current_jl;
	int j_free_bitmap_nodes;
	int j_used_bitmap_nodes;

	int j_num_lists;	
	int j_num_work_lists;	

	
	unsigned int j_last_flush_id;

	
	unsigned int j_last_commit_id;

	struct list_head j_bitmap_nodes;
	struct list_head j_dirty_buffers;
	spinlock_t j_dirty_buffers_lock;	

	
	struct list_head j_journal_list;
	
	struct list_head j_working_list;

	struct reiserfs_list_bitmap j_list_bitmap[JOURNAL_NUM_BITMAPS];	
	struct reiserfs_journal_cnode *j_hash_table[JOURNAL_HASH_SIZE];	
	struct reiserfs_journal_cnode *j_list_hash_table[JOURNAL_HASH_SIZE];	
	struct list_head j_prealloc_list;	
	int j_persistent_trans;
	unsigned long j_max_trans_size;
	unsigned long j_max_batch_size;

	int j_errno;

	
	struct delayed_work j_work;
	struct super_block *j_work_sb;
	atomic_t j_async_throttle;
};

enum journal_state_bits {
	J_WRITERS_BLOCKED = 1,	
	J_WRITERS_QUEUED,	
	J_ABORTED,		
};

#define JOURNAL_DESC_MAGIC "ReIsErLB"	

typedef __u32(*hashf_t) (const signed char *, int);

struct reiserfs_bitmap_info {
	__u32 free_count;
};

struct proc_dir_entry;

#if defined( CONFIG_PROC_FS ) && defined( CONFIG_REISERFS_PROC_INFO )
typedef unsigned long int stat_cnt_t;
typedef struct reiserfs_proc_info_data {
	spinlock_t lock;
	int exiting;
	int max_hash_collisions;

	stat_cnt_t breads;
	stat_cnt_t bread_miss;
	stat_cnt_t search_by_key;
	stat_cnt_t search_by_key_fs_changed;
	stat_cnt_t search_by_key_restarted;

	stat_cnt_t insert_item_restarted;
	stat_cnt_t paste_into_item_restarted;
	stat_cnt_t cut_from_item_restarted;
	stat_cnt_t delete_solid_item_restarted;
	stat_cnt_t delete_item_restarted;

	stat_cnt_t leaked_oid;
	stat_cnt_t leaves_removable;

	
	stat_cnt_t balance_at[5];	
	
	stat_cnt_t sbk_read_at[5];	
	stat_cnt_t sbk_fs_changed[5];
	stat_cnt_t sbk_restarted[5];
	stat_cnt_t items_at[5];	
	stat_cnt_t free_at[5];	
	stat_cnt_t can_node_be_removed[5];	
	long int lnum[5];	
	long int rnum[5];	
	long int lbytes[5];	
	long int rbytes[5];	
	stat_cnt_t get_neighbors[5];
	stat_cnt_t get_neighbors_restart[5];
	stat_cnt_t need_l_neighbor[5];
	stat_cnt_t need_r_neighbor[5];

	stat_cnt_t free_block;
	struct __scan_bitmap_stats {
		stat_cnt_t call;
		stat_cnt_t wait;
		stat_cnt_t bmap;
		stat_cnt_t retry;
		stat_cnt_t in_journal_hint;
		stat_cnt_t in_journal_nohint;
		stat_cnt_t stolen;
	} scan_bitmap;
	struct __journal_stats {
		stat_cnt_t in_journal;
		stat_cnt_t in_journal_bitmap;
		stat_cnt_t in_journal_reusable;
		stat_cnt_t lock_journal;
		stat_cnt_t lock_journal_wait;
		stat_cnt_t journal_being;
		stat_cnt_t journal_relock_writers;
		stat_cnt_t journal_relock_wcount;
		stat_cnt_t mark_dirty;
		stat_cnt_t mark_dirty_already;
		stat_cnt_t mark_dirty_notjournal;
		stat_cnt_t restore_prepared;
		stat_cnt_t prepare;
		stat_cnt_t prepare_retry;
	} journal;
} reiserfs_proc_info_data_t;
#else
typedef struct reiserfs_proc_info_data {
} reiserfs_proc_info_data_t;
#endif

struct reiserfs_sb_info {
	struct buffer_head *s_sbh;	
	struct reiserfs_super_block *s_rs;	
	struct reiserfs_bitmap_info *s_ap_bitmap;
	struct reiserfs_journal *s_journal;	
	unsigned short s_mount_state;	

	
	struct mutex lock;
	
	struct task_struct *lock_owner;
	
	int lock_depth;

	
	void (*end_io_handler) (struct buffer_head *, int);
	hashf_t s_hash_function;	
	unsigned long s_mount_opt;	

	struct {		
		unsigned long bits;	
		unsigned long large_file_size;	
		int border;	
		int preallocmin;	
		int preallocsize;	
	} s_alloc_options;

	
	wait_queue_head_t s_wait;
	
	atomic_t s_generation_counter;	
	
	unsigned long s_properties;	

	
	int s_disk_reads;
	int s_disk_writes;
	int s_fix_nodes;
	int s_do_balance;
	int s_unneeded_left_neighbor;
	int s_good_search_by_key_reada;
	int s_bmaps;
	int s_bmaps_without_search;
	int s_direct2indirect;
	int s_indirect2direct;
	int s_is_unlinked_ok;
	reiserfs_proc_info_data_t s_proc_info_data;
	struct proc_dir_entry *procdir;
	int reserved_blocks;	
	spinlock_t bitmap_lock;	
	struct dentry *priv_root;	
	struct dentry *xattr_root;	
	int j_errno;
#ifdef CONFIG_QUOTA
	char *s_qf_names[MAXQUOTAS];
	int s_jquota_fmt;
#endif
	char *s_jdev;		
#ifdef CONFIG_REISERFS_CHECK

	struct tree_balance *cur_tb;	
#endif
};

#define REISERFS_3_5 0
#define REISERFS_3_6 1
#define REISERFS_OLD_FORMAT 2

enum reiserfs_mount_options {
	REISERFS_LARGETAIL,	
	REISERFS_SMALLTAIL,	
	REPLAYONLY,		
	REISERFS_CONVERT,	

	FORCE_TEA_HASH,		
	FORCE_RUPASOV_HASH,	
	FORCE_R5_HASH,		
	FORCE_HASH_DETECT,	

	REISERFS_DATA_LOG,
	REISERFS_DATA_ORDERED,
	REISERFS_DATA_WRITEBACK,


	REISERFS_NO_BORDER,
	REISERFS_NO_UNHASHED_RELOCATION,
	REISERFS_HASHED_RELOCATION,
	REISERFS_ATTRS,
	REISERFS_XATTRS_USER,
	REISERFS_POSIXACL,
	REISERFS_EXPOSE_PRIVROOT,
	REISERFS_BARRIER_NONE,
	REISERFS_BARRIER_FLUSH,

	
	REISERFS_ERROR_PANIC,
	REISERFS_ERROR_RO,
	REISERFS_ERROR_CONTINUE,

	REISERFS_USRQUOTA,	
	REISERFS_GRPQUOTA,	

	REISERFS_TEST1,
	REISERFS_TEST2,
	REISERFS_TEST3,
	REISERFS_TEST4,
	REISERFS_UNSUPPORTED_OPT,
};

#define reiserfs_r5_hash(s) (REISERFS_SB(s)->s_mount_opt & (1 << FORCE_R5_HASH))
#define reiserfs_rupasov_hash(s) (REISERFS_SB(s)->s_mount_opt & (1 << FORCE_RUPASOV_HASH))
#define reiserfs_tea_hash(s) (REISERFS_SB(s)->s_mount_opt & (1 << FORCE_TEA_HASH))
#define reiserfs_hash_detect(s) (REISERFS_SB(s)->s_mount_opt & (1 << FORCE_HASH_DETECT))
#define reiserfs_no_border(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_NO_BORDER))
#define reiserfs_no_unhashed_relocation(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_NO_UNHASHED_RELOCATION))
#define reiserfs_hashed_relocation(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_HASHED_RELOCATION))
#define reiserfs_test4(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_TEST4))

#define have_large_tails(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_LARGETAIL))
#define have_small_tails(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_SMALLTAIL))
#define replay_only(s) (REISERFS_SB(s)->s_mount_opt & (1 << REPLAYONLY))
#define reiserfs_attrs(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_ATTRS))
#define old_format_only(s) (REISERFS_SB(s)->s_properties & (1 << REISERFS_3_5))
#define convert_reiserfs(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_CONVERT))
#define reiserfs_data_log(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_DATA_LOG))
#define reiserfs_data_ordered(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_DATA_ORDERED))
#define reiserfs_data_writeback(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_DATA_WRITEBACK))
#define reiserfs_xattrs_user(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_XATTRS_USER))
#define reiserfs_posixacl(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_POSIXACL))
#define reiserfs_expose_privroot(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_EXPOSE_PRIVROOT))
#define reiserfs_xattrs_optional(s) (reiserfs_xattrs_user(s) || reiserfs_posixacl(s))
#define reiserfs_barrier_none(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_BARRIER_NONE))
#define reiserfs_barrier_flush(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_BARRIER_FLUSH))

#define reiserfs_error_panic(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_ERROR_PANIC))
#define reiserfs_error_ro(s) (REISERFS_SB(s)->s_mount_opt & (1 << REISERFS_ERROR_RO))

void reiserfs_file_buffer(struct buffer_head *bh, int list);
extern struct file_system_type reiserfs_fs_type;
int reiserfs_resize(struct super_block *, unsigned long);

#define CARRY_ON                0
#define SCHEDULE_OCCURRED       1

#define SB_BUFFER_WITH_SB(s) (REISERFS_SB(s)->s_sbh)
#define SB_JOURNAL(s) (REISERFS_SB(s)->s_journal)
#define SB_JOURNAL_1st_RESERVED_BLOCK(s) (SB_JOURNAL(s)->j_1st_reserved_block)
#define SB_JOURNAL_LEN_FREE(s) (SB_JOURNAL(s)->j_journal_len_free)
#define SB_AP_BITMAP(s) (REISERFS_SB(s)->s_ap_bitmap)

#define SB_DISK_JOURNAL_HEAD(s) (SB_JOURNAL(s)->j_header_bh->)

static inline char *reiserfs_bdevname(struct super_block *s)
{
	return (s == NULL) ? "Null superblock" : s->s_id;
}

#define reiserfs_is_journal_aborted(journal) (unlikely (__reiserfs_is_journal_aborted (journal)))
static inline int __reiserfs_is_journal_aborted(struct reiserfs_journal
						*journal)
{
	return test_bit(J_ABORTED, &journal->j_state);
}

void reiserfs_write_lock(struct super_block *s);
void reiserfs_write_unlock(struct super_block *s);
int reiserfs_write_lock_once(struct super_block *s);
void reiserfs_write_unlock_once(struct super_block *s, int lock_depth);

#ifdef CONFIG_REISERFS_CHECK
void reiserfs_lock_check_recursive(struct super_block *s);
#else
static inline void reiserfs_lock_check_recursive(struct super_block *s) { }
#endif

static inline void reiserfs_mutex_lock_safe(struct mutex *m,
			       struct super_block *s)
{
	reiserfs_lock_check_recursive(s);
	reiserfs_write_unlock(s);
	mutex_lock(m);
	reiserfs_write_lock(s);
}

static inline void
reiserfs_mutex_lock_nested_safe(struct mutex *m, unsigned int subclass,
			       struct super_block *s)
{
	reiserfs_lock_check_recursive(s);
	reiserfs_write_unlock(s);
	mutex_lock_nested(m, subclass);
	reiserfs_write_lock(s);
}

static inline void
reiserfs_down_read_safe(struct rw_semaphore *sem, struct super_block *s)
{
	reiserfs_lock_check_recursive(s);
	reiserfs_write_unlock(s);
	down_read(sem);
	reiserfs_write_lock(s);
}

static inline void reiserfs_cond_resched(struct super_block *s)
{
	if (need_resched()) {
		reiserfs_write_unlock(s);
		schedule();
		reiserfs_write_lock(s);
	}
}

struct fid;


#define USE_INODE_GENERATION_COUNTER

#define REISERFS_PREALLOCATE
#define DISPLACE_NEW_PACKING_LOCALITIES
#define PREALLOCATION_SIZE 9

#define _ROUND_UP(x,n) (((x)+(n)-1u) & ~((n)-1u))

#define ROUND_UP(x) _ROUND_UP(x,8LL)

#define REISERFS_DEBUG_CODE 5	

void __reiserfs_warning(struct super_block *s, const char *id,
			 const char *func, const char *fmt, ...);
#define reiserfs_warning(s, id, fmt, args...) \
	 __reiserfs_warning(s, id, __func__, fmt, ##args)

#define __RASSERT(cond, scond, format, args...)			\
do {									\
	if (!(cond))							\
		reiserfs_panic(NULL, "assertion failure", "(" #cond ") at " \
			       __FILE__ ":%i:%s: " format "\n",		\
			       in_interrupt() ? -1 : task_pid_nr(current), \
			       __LINE__, __func__ , ##args);		\
} while (0)

#define RASSERT(cond, format, args...) __RASSERT(cond, #cond, format, ##args)

#if defined( CONFIG_REISERFS_CHECK )
#define RFALSE(cond, format, args...) __RASSERT(!(cond), "!(" #cond ")", format, ##args)
#else
#define RFALSE( cond, format, args... ) do {;} while( 0 )
#endif

#define CONSTF __attribute_const__


/*
 * Structure of super block on disk, a version of which in RAM is often accessed as REISERFS_SB(s)->s_rs
 * the version in RAM is part of a larger structure containing fields never written to disk.
 */
#define UNSET_HASH 0		
		     
#define TEA_HASH  1
#define YURA_HASH 2
#define R5_HASH   3
#define DEFAULT_HASH R5_HASH

struct journal_params {
	__le32 jp_journal_1st_block;	
	__le32 jp_journal_dev;	
	__le32 jp_journal_size;	
	__le32 jp_journal_trans_max;	
	__le32 jp_journal_magic;	
	__le32 jp_journal_max_batch;	
	__le32 jp_journal_max_commit_age;	
	__le32 jp_journal_max_trans_age;	
};

struct reiserfs_super_block_v1 {
	__le32 s_block_count;	
	__le32 s_free_blocks;	
	__le32 s_root_block;	
	struct journal_params s_journal;
	__le16 s_blocksize;	
	__le16 s_oid_maxsize;	
	__le16 s_oid_cursize;	
	__le16 s_umount_state;	
	char s_magic[10];	
	__le16 s_fs_state;	
	__le32 s_hash_function_code;	
	__le16 s_tree_height;	
	__le16 s_bmap_nr;	
	__le16 s_version;	
	__le16 s_reserved_for_journal;	
} __attribute__ ((__packed__));

#define SB_SIZE_V1 (sizeof(struct reiserfs_super_block_v1))

struct reiserfs_super_block {
	struct reiserfs_super_block_v1 s_v1;
	__le32 s_inode_generation;
	__le32 s_flags;		
	unsigned char s_uuid[16];	
	unsigned char s_label[16];	
	__le16 s_mnt_count;		
	__le16 s_max_mnt_count;		
	__le32 s_lastcheck;		
	__le32 s_check_interval;	
	char s_unused[76];	
} __attribute__ ((__packed__));

#define SB_SIZE (sizeof(struct reiserfs_super_block))

#define REISERFS_VERSION_1 0
#define REISERFS_VERSION_2 2

#define SB_DISK_SUPER_BLOCK(s) (REISERFS_SB(s)->s_rs)
#define SB_V1_DISK_SUPER_BLOCK(s) (&(SB_DISK_SUPER_BLOCK(s)->s_v1))
#define SB_BLOCKSIZE(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_blocksize))
#define SB_BLOCK_COUNT(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_block_count))
#define SB_FREE_BLOCKS(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_free_blocks))
#define SB_REISERFS_MAGIC(s) \
        (SB_V1_DISK_SUPER_BLOCK(s)->s_magic)
#define SB_ROOT_BLOCK(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_root_block))
#define SB_TREE_HEIGHT(s) \
        le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_tree_height))
#define SB_REISERFS_STATE(s) \
        le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_umount_state))
#define SB_VERSION(s) le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_version))
#define SB_BMAP_NR(s) le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_bmap_nr))

#define PUT_SB_BLOCK_COUNT(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_block_count = cpu_to_le32(val); } while (0)
#define PUT_SB_FREE_BLOCKS(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_free_blocks = cpu_to_le32(val); } while (0)
#define PUT_SB_ROOT_BLOCK(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_root_block = cpu_to_le32(val); } while (0)
#define PUT_SB_TREE_HEIGHT(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_tree_height = cpu_to_le16(val); } while (0)
#define PUT_SB_REISERFS_STATE(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_umount_state = cpu_to_le16(val); } while (0)
#define PUT_SB_VERSION(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_version = cpu_to_le16(val); } while (0)
#define PUT_SB_BMAP_NR(s, val) \
   do { SB_V1_DISK_SUPER_BLOCK(s)->s_bmap_nr = cpu_to_le16 (val); } while (0)

#define SB_ONDISK_JP(s) (&SB_V1_DISK_SUPER_BLOCK(s)->s_journal)
#define SB_ONDISK_JOURNAL_SIZE(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_size))
#define SB_ONDISK_JOURNAL_1st_BLOCK(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_1st_block))
#define SB_ONDISK_JOURNAL_DEVICE(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_dev))
#define SB_ONDISK_RESERVED_FOR_JOURNAL(s) \
         le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_reserved_for_journal))

#define is_block_in_log_or_reserved_area(s, block) \
         block >= SB_JOURNAL_1st_RESERVED_BLOCK(s) \
         && block < SB_JOURNAL_1st_RESERVED_BLOCK(s) +  \
         ((!is_reiserfs_jr(SB_DISK_SUPER_BLOCK(s)) ? \
         SB_ONDISK_JOURNAL_SIZE(s) + 1 : SB_ONDISK_RESERVED_FOR_JOURNAL(s)))

int is_reiserfs_3_5(struct reiserfs_super_block *rs);
int is_reiserfs_3_6(struct reiserfs_super_block *rs);
int is_reiserfs_jr(struct reiserfs_super_block *rs);

#define REISERFS_DISK_OFFSET_IN_BYTES (64 * 1024)
#define REISERFS_FIRST_BLOCK unused_define
#define REISERFS_JOURNAL_OFFSET_IN_BYTES REISERFS_DISK_OFFSET_IN_BYTES

#define REISERFS_OLD_DISK_OFFSET_IN_BYTES (8 * 1024)

#define CARRY_ON      0
#define REPEAT_SEARCH -1
#define IO_ERROR      -2
#define NO_DISK_SPACE -3
#define NO_BALANCING_NEEDED  (-4)
#define NO_MORE_UNUSED_CONTIGUOUS_BLOCKS (-5)
#define QUOTA_EXCEEDED -6

typedef __u32 b_blocknr_t;
typedef __le32 unp_t;

struct unfm_nodeinfo {
	unp_t unfm_nodenum;
	unsigned short unfm_freespace;
};

#define KEY_FORMAT_3_5 0
#define KEY_FORMAT_3_6 1

#define STAT_DATA_V1 0
#define STAT_DATA_V2 1

static inline struct reiserfs_inode_info *REISERFS_I(const struct inode *inode)
{
	return container_of(inode, struct reiserfs_inode_info, vfs_inode);
}

static inline struct reiserfs_sb_info *REISERFS_SB(const struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline __u32 reiserfs_bmap_count(struct super_block *sb)
{
	return (SB_BLOCK_COUNT(sb) - 1) / (sb->s_blocksize * 8) + 1;
}

static inline int bmap_would_wrap(unsigned bmap_nr)
{
	return bmap_nr > ((1LL << 16) - 1);
}

#define get_inode_item_key_version( inode )                                    \
    ((REISERFS_I(inode)->i_flags & i_item_key_version_mask) ? KEY_FORMAT_3_6 : KEY_FORMAT_3_5)

#define set_inode_item_key_version( inode, version )                           \
         ({ if((version)==KEY_FORMAT_3_6)                                      \
                REISERFS_I(inode)->i_flags |= i_item_key_version_mask;      \
            else                                                               \
                REISERFS_I(inode)->i_flags &= ~i_item_key_version_mask; })

#define get_inode_sd_version(inode)                                            \
    ((REISERFS_I(inode)->i_flags & i_stat_data_version_mask) ? STAT_DATA_V2 : STAT_DATA_V1)

#define set_inode_sd_version(inode, version)                                   \
         ({ if((version)==STAT_DATA_V2)                                        \
                REISERFS_I(inode)->i_flags |= i_stat_data_version_mask;     \
            else                                                               \
                REISERFS_I(inode)->i_flags &= ~i_stat_data_version_mask; })

#define STORE_TAIL_IN_UNFM_S1(n_file_size,n_tail_size,n_block_size) \
(\
  (!(n_tail_size)) || \
  (((n_tail_size) > MAX_DIRECT_ITEM_LEN(n_block_size)) || \
   ( (n_file_size) >= (n_block_size) * 4 ) || \
   ( ( (n_file_size) >= (n_block_size) * 3 ) && \
     ( (n_tail_size) >=   (MAX_DIRECT_ITEM_LEN(n_block_size))/4) ) || \
   ( ( (n_file_size) >= (n_block_size) * 2 ) && \
     ( (n_tail_size) >=   (MAX_DIRECT_ITEM_LEN(n_block_size))/2) ) || \
   ( ( (n_file_size) >= (n_block_size) ) && \
     ( (n_tail_size) >=   (MAX_DIRECT_ITEM_LEN(n_block_size) * 3)/4) ) ) \
)

#define STORE_TAIL_IN_UNFM_S2(n_file_size,n_tail_size,n_block_size) \
(\
  (!(n_tail_size)) || \
  (((n_file_size) > MAX_DIRECT_ITEM_LEN(n_block_size)) ) \
)

#define REISERFS_VALID_FS    1
#define REISERFS_ERROR_FS    2

#define TYPE_STAT_DATA 0
#define TYPE_INDIRECT 1
#define TYPE_DIRECT 2
#define TYPE_DIRENTRY 3
#define TYPE_MAXTYPE 3
#define TYPE_ANY 15		


struct offset_v1 {
	__le32 k_offset;
	__le32 k_uniqueness;
} __attribute__ ((__packed__));

struct offset_v2 {
	__le64 v;
} __attribute__ ((__packed__));

static inline __u16 offset_v2_k_type(const struct offset_v2 *v2)
{
	__u8 type = le64_to_cpu(v2->v) >> 60;
	return (type <= TYPE_MAXTYPE) ? type : TYPE_ANY;
}

static inline void set_offset_v2_k_type(struct offset_v2 *v2, int type)
{
	v2->v =
	    (v2->v & cpu_to_le64(~0ULL >> 4)) | cpu_to_le64((__u64) type << 60);
}

static inline loff_t offset_v2_k_offset(const struct offset_v2 *v2)
{
	return le64_to_cpu(v2->v) & (~0ULL >> 4);
}

static inline void set_offset_v2_k_offset(struct offset_v2 *v2, loff_t offset)
{
	offset &= (~0ULL >> 4);
	v2->v = (v2->v & cpu_to_le64(15ULL << 60)) | cpu_to_le64(offset);
}

struct reiserfs_key {
	__le32 k_dir_id;	
	__le32 k_objectid;	
	union {
		struct offset_v1 k_offset_v1;
		struct offset_v2 k_offset_v2;
	} __attribute__ ((__packed__)) u;
} __attribute__ ((__packed__));

struct in_core_key {
	__u32 k_dir_id;		
	__u32 k_objectid;	
	__u64 k_offset;
	__u8 k_type;
};

struct cpu_key {
	struct in_core_key on_disk_key;
	int version;
	int key_length;		
};

#define REISERFS_FULL_KEY_LEN     4
#define REISERFS_SHORT_KEY_LEN    2

#define FIRST_GREATER 1
#define SECOND_GREATER -1
#define KEYS_IDENTICAL 0
#define KEY_FOUND 1
#define KEY_NOT_FOUND 0

#define KEY_SIZE (sizeof(struct reiserfs_key))
#define SHORT_KEY_SIZE (sizeof (__u32) + sizeof (__u32))

#define ITEM_FOUND 1
#define ITEM_NOT_FOUND 0
#define ENTRY_FOUND 1
#define ENTRY_NOT_FOUND 0
#define DIRECTORY_NOT_FOUND -1
#define REGULAR_FILE_FOUND -2
#define DIRECTORY_FOUND -3
#define BYTE_FOUND 1
#define BYTE_NOT_FOUND 0
#define FILE_NOT_FOUND -1

#define POSITION_FOUND 1
#define POSITION_NOT_FOUND 0

#define NAME_FOUND 1
#define NAME_NOT_FOUND 0
#define GOTO_PREVIOUS_ITEM 2
#define NAME_FOUND_INVISIBLE 3


struct item_head {
	struct reiserfs_key ih_key;
	union {
		__le16 ih_free_space_reserved;
		__le16 ih_entry_count;
	} __attribute__ ((__packed__)) u;
	__le16 ih_item_len;	
	__le16 ih_item_location;	
	__le16 ih_version;	
} __attribute__ ((__packed__));
#define IH_SIZE (sizeof(struct item_head))

#define ih_free_space(ih)            le16_to_cpu((ih)->u.ih_free_space_reserved)
#define ih_version(ih)               le16_to_cpu((ih)->ih_version)
#define ih_entry_count(ih)           le16_to_cpu((ih)->u.ih_entry_count)
#define ih_location(ih)              le16_to_cpu((ih)->ih_item_location)
#define ih_item_len(ih)              le16_to_cpu((ih)->ih_item_len)

#define put_ih_free_space(ih, val)   do { (ih)->u.ih_free_space_reserved = cpu_to_le16(val); } while(0)
#define put_ih_version(ih, val)      do { (ih)->ih_version = cpu_to_le16(val); } while (0)
#define put_ih_entry_count(ih, val)  do { (ih)->u.ih_entry_count = cpu_to_le16(val); } while (0)
#define put_ih_location(ih, val)     do { (ih)->ih_item_location = cpu_to_le16(val); } while (0)
#define put_ih_item_len(ih, val)     do { (ih)->ih_item_len = cpu_to_le16(val); } while (0)

#define unreachable_item(ih) (ih_version(ih) & (1 << 15))

#define get_ih_free_space(ih) (ih_version (ih) == KEY_FORMAT_3_6 ? 0 : ih_free_space (ih))
#define set_ih_free_space(ih,val) put_ih_free_space((ih), ((ih_version(ih) == KEY_FORMAT_3_6) ? 0 : (val)))

#define get_block_num(p, i) get_unaligned_le32((p) + (i))
#define put_block_num(p, i, v) put_unaligned_le32((v), (p) + (i))

#define V1_SD_UNIQUENESS 0
#define V1_INDIRECT_UNIQUENESS 0xfffffffe
#define V1_DIRECT_UNIQUENESS 0xffffffff
#define V1_DIRENTRY_UNIQUENESS 500
#define V1_ANY_UNIQUENESS 555	

static inline int uniqueness2type(__u32 uniqueness) CONSTF;
static inline int uniqueness2type(__u32 uniqueness)
{
	switch ((int)uniqueness) {
	case V1_SD_UNIQUENESS:
		return TYPE_STAT_DATA;
	case V1_INDIRECT_UNIQUENESS:
		return TYPE_INDIRECT;
	case V1_DIRECT_UNIQUENESS:
		return TYPE_DIRECT;
	case V1_DIRENTRY_UNIQUENESS:
		return TYPE_DIRENTRY;
	case V1_ANY_UNIQUENESS:
	default:
		return TYPE_ANY;
	}
}

static inline __u32 type2uniqueness(int type) CONSTF;
static inline __u32 type2uniqueness(int type)
{
	switch (type) {
	case TYPE_STAT_DATA:
		return V1_SD_UNIQUENESS;
	case TYPE_INDIRECT:
		return V1_INDIRECT_UNIQUENESS;
	case TYPE_DIRECT:
		return V1_DIRECT_UNIQUENESS;
	case TYPE_DIRENTRY:
		return V1_DIRENTRY_UNIQUENESS;
	case TYPE_ANY:
	default:
		return V1_ANY_UNIQUENESS;
	}
}

static inline loff_t le_key_k_offset(int version,
				     const struct reiserfs_key *key)
{
	return (version == KEY_FORMAT_3_5) ?
	    le32_to_cpu(key->u.k_offset_v1.k_offset) :
	    offset_v2_k_offset(&(key->u.k_offset_v2));
}

static inline loff_t le_ih_k_offset(const struct item_head *ih)
{
	return le_key_k_offset(ih_version(ih), &(ih->ih_key));
}

static inline loff_t le_key_k_type(int version, const struct reiserfs_key *key)
{
	return (version == KEY_FORMAT_3_5) ?
	    uniqueness2type(le32_to_cpu(key->u.k_offset_v1.k_uniqueness)) :
	    offset_v2_k_type(&(key->u.k_offset_v2));
}

static inline loff_t le_ih_k_type(const struct item_head *ih)
{
	return le_key_k_type(ih_version(ih), &(ih->ih_key));
}

static inline void set_le_key_k_offset(int version, struct reiserfs_key *key,
				       loff_t offset)
{
	(version == KEY_FORMAT_3_5) ? (void)(key->u.k_offset_v1.k_offset = cpu_to_le32(offset)) :	
	    (void)(set_offset_v2_k_offset(&(key->u.k_offset_v2), offset));
}

static inline void set_le_ih_k_offset(struct item_head *ih, loff_t offset)
{
	set_le_key_k_offset(ih_version(ih), &(ih->ih_key), offset);
}

static inline void set_le_key_k_type(int version, struct reiserfs_key *key,
				     int type)
{
	(version == KEY_FORMAT_3_5) ?
	    (void)(key->u.k_offset_v1.k_uniqueness =
		   cpu_to_le32(type2uniqueness(type)))
	    : (void)(set_offset_v2_k_type(&(key->u.k_offset_v2), type));
}

static inline void set_le_ih_k_type(struct item_head *ih, int type)
{
	set_le_key_k_type(ih_version(ih), &(ih->ih_key), type);
}

static inline int is_direntry_le_key(int version, struct reiserfs_key *key)
{
	return le_key_k_type(version, key) == TYPE_DIRENTRY;
}

static inline int is_direct_le_key(int version, struct reiserfs_key *key)
{
	return le_key_k_type(version, key) == TYPE_DIRECT;
}

static inline int is_indirect_le_key(int version, struct reiserfs_key *key)
{
	return le_key_k_type(version, key) == TYPE_INDIRECT;
}

static inline int is_statdata_le_key(int version, struct reiserfs_key *key)
{
	return le_key_k_type(version, key) == TYPE_STAT_DATA;
}

static inline int is_direntry_le_ih(struct item_head *ih)
{
	return is_direntry_le_key(ih_version(ih), &ih->ih_key);
}

static inline int is_direct_le_ih(struct item_head *ih)
{
	return is_direct_le_key(ih_version(ih), &ih->ih_key);
}

static inline int is_indirect_le_ih(struct item_head *ih)
{
	return is_indirect_le_key(ih_version(ih), &ih->ih_key);
}

static inline int is_statdata_le_ih(struct item_head *ih)
{
	return is_statdata_le_key(ih_version(ih), &ih->ih_key);
}

static inline loff_t cpu_key_k_offset(const struct cpu_key *key)
{
	return key->on_disk_key.k_offset;
}

static inline loff_t cpu_key_k_type(const struct cpu_key *key)
{
	return key->on_disk_key.k_type;
}

static inline void set_cpu_key_k_offset(struct cpu_key *key, loff_t offset)
{
	key->on_disk_key.k_offset = offset;
}

static inline void set_cpu_key_k_type(struct cpu_key *key, int type)
{
	key->on_disk_key.k_type = type;
}

static inline void cpu_key_k_offset_dec(struct cpu_key *key)
{
	key->on_disk_key.k_offset--;
}

#define is_direntry_cpu_key(key) (cpu_key_k_type (key) == TYPE_DIRENTRY)
#define is_direct_cpu_key(key) (cpu_key_k_type (key) == TYPE_DIRECT)
#define is_indirect_cpu_key(key) (cpu_key_k_type (key) == TYPE_INDIRECT)
#define is_statdata_cpu_key(key) (cpu_key_k_type (key) == TYPE_STAT_DATA)

#define is_direntry_cpu_ih(ih) (is_direntry_cpu_key (&((ih)->ih_key)))
#define is_direct_cpu_ih(ih) (is_direct_cpu_key (&((ih)->ih_key)))
#define is_indirect_cpu_ih(ih) (is_indirect_cpu_key (&((ih)->ih_key)))
#define is_statdata_cpu_ih(ih) (is_statdata_cpu_key (&((ih)->ih_key)))

#define I_K_KEY_IN_ITEM(ih, key, n_blocksize) \
    (!COMP_SHORT_KEYS(ih, key) && \
	  I_OFF_BYTE_IN_ITEM(ih, k_offset(key), n_blocksize))

#define MAX_ITEM_LEN(block_size) (block_size - BLKH_SIZE - IH_SIZE)
#define MIN_ITEM_LEN 1

#define REISERFS_ROOT_OBJECTID 2
#define REISERFS_ROOT_PARENT_OBJECTID 1

extern struct reiserfs_key root_key;


struct block_head {
	__le16 blk_level;	
	__le16 blk_nr_item;	
	__le16 blk_free_space;	
	__le16 blk_reserved;
	
	struct reiserfs_key blk_right_delim_key;	
};

#define BLKH_SIZE                     (sizeof(struct block_head))
#define blkh_level(p_blkh)            (le16_to_cpu((p_blkh)->blk_level))
#define blkh_nr_item(p_blkh)          (le16_to_cpu((p_blkh)->blk_nr_item))
#define blkh_free_space(p_blkh)       (le16_to_cpu((p_blkh)->blk_free_space))
#define blkh_reserved(p_blkh)         (le16_to_cpu((p_blkh)->blk_reserved))
#define set_blkh_level(p_blkh,val)    ((p_blkh)->blk_level = cpu_to_le16(val))
#define set_blkh_nr_item(p_blkh,val)  ((p_blkh)->blk_nr_item = cpu_to_le16(val))
#define set_blkh_free_space(p_blkh,val) ((p_blkh)->blk_free_space = cpu_to_le16(val))
#define set_blkh_reserved(p_blkh,val) ((p_blkh)->blk_reserved = cpu_to_le16(val))
#define blkh_right_delim_key(p_blkh)  ((p_blkh)->blk_right_delim_key)
#define set_blkh_right_delim_key(p_blkh,val)  ((p_blkh)->blk_right_delim_key = val)


#define FREE_LEVEL 0		

#define DISK_LEAF_NODE_LEVEL  1	

#define B_BLK_HEAD(bh)			((struct block_head *)((bh)->b_data))
#define B_NR_ITEMS(bh)			(blkh_nr_item(B_BLK_HEAD(bh)))
#define B_LEVEL(bh)			(blkh_level(B_BLK_HEAD(bh)))
#define B_FREE_SPACE(bh)		(blkh_free_space(B_BLK_HEAD(bh)))

#define PUT_B_NR_ITEMS(bh, val)		do { set_blkh_nr_item(B_BLK_HEAD(bh), val); } while (0)
#define PUT_B_LEVEL(bh, val)		do { set_blkh_level(B_BLK_HEAD(bh), val); } while (0)
#define PUT_B_FREE_SPACE(bh, val)	do { set_blkh_free_space(B_BLK_HEAD(bh), val); } while (0)

#define B_PRIGHT_DELIM_KEY(bh)		(&(blk_right_delim_key(B_BLK_HEAD(bh))))

#define B_IS_ITEMS_LEVEL(bh)		(B_LEVEL(bh) == DISK_LEAF_NODE_LEVEL)

#define B_IS_KEYS_LEVEL(bh)      (B_LEVEL(bh) > DISK_LEAF_NODE_LEVEL \
					    && B_LEVEL(bh) <= MAX_HEIGHT)


struct stat_data_v1 {
	__le16 sd_mode;		
	__le16 sd_nlink;	
	__le16 sd_uid;		
	__le16 sd_gid;		
	__le32 sd_size;		
	__le32 sd_atime;	
	__le32 sd_mtime;	
	__le32 sd_ctime;	
	union {
		__le32 sd_rdev;
		__le32 sd_blocks;	
	} __attribute__ ((__packed__)) u;
	__le32 sd_first_direct_byte;	
} __attribute__ ((__packed__));

#define SD_V1_SIZE              (sizeof(struct stat_data_v1))
#define stat_data_v1(ih)        (ih_version (ih) == KEY_FORMAT_3_5)
#define sd_v1_mode(sdp)         (le16_to_cpu((sdp)->sd_mode))
#define set_sd_v1_mode(sdp,v)   ((sdp)->sd_mode = cpu_to_le16(v))
#define sd_v1_nlink(sdp)        (le16_to_cpu((sdp)->sd_nlink))
#define set_sd_v1_nlink(sdp,v)  ((sdp)->sd_nlink = cpu_to_le16(v))
#define sd_v1_uid(sdp)          (le16_to_cpu((sdp)->sd_uid))
#define set_sd_v1_uid(sdp,v)    ((sdp)->sd_uid = cpu_to_le16(v))
#define sd_v1_gid(sdp)          (le16_to_cpu((sdp)->sd_gid))
#define set_sd_v1_gid(sdp,v)    ((sdp)->sd_gid = cpu_to_le16(v))
#define sd_v1_size(sdp)         (le32_to_cpu((sdp)->sd_size))
#define set_sd_v1_size(sdp,v)   ((sdp)->sd_size = cpu_to_le32(v))
#define sd_v1_atime(sdp)        (le32_to_cpu((sdp)->sd_atime))
#define set_sd_v1_atime(sdp,v)  ((sdp)->sd_atime = cpu_to_le32(v))
#define sd_v1_mtime(sdp)        (le32_to_cpu((sdp)->sd_mtime))
#define set_sd_v1_mtime(sdp,v)  ((sdp)->sd_mtime = cpu_to_le32(v))
#define sd_v1_ctime(sdp)        (le32_to_cpu((sdp)->sd_ctime))
#define set_sd_v1_ctime(sdp,v)  ((sdp)->sd_ctime = cpu_to_le32(v))
#define sd_v1_rdev(sdp)         (le32_to_cpu((sdp)->u.sd_rdev))
#define set_sd_v1_rdev(sdp,v)   ((sdp)->u.sd_rdev = cpu_to_le32(v))
#define sd_v1_blocks(sdp)       (le32_to_cpu((sdp)->u.sd_blocks))
#define set_sd_v1_blocks(sdp,v) ((sdp)->u.sd_blocks = cpu_to_le32(v))
#define sd_v1_first_direct_byte(sdp) \
                                (le32_to_cpu((sdp)->sd_first_direct_byte))
#define set_sd_v1_first_direct_byte(sdp,v) \
                                ((sdp)->sd_first_direct_byte = cpu_to_le32(v))


#define REISERFS_IMMUTABLE_FL FS_IMMUTABLE_FL
#define REISERFS_APPEND_FL    FS_APPEND_FL
#define REISERFS_SYNC_FL      FS_SYNC_FL
#define REISERFS_NOATIME_FL   FS_NOATIME_FL
#define REISERFS_NODUMP_FL    FS_NODUMP_FL
#define REISERFS_SECRM_FL     FS_SECRM_FL
#define REISERFS_UNRM_FL      FS_UNRM_FL
#define REISERFS_COMPR_FL     FS_COMPR_FL
#define REISERFS_NOTAIL_FL    FS_NOTAIL_FL

#define REISERFS_INHERIT_MASK ( REISERFS_IMMUTABLE_FL |	\
				REISERFS_SYNC_FL |	\
				REISERFS_NOATIME_FL |	\
				REISERFS_NODUMP_FL |	\
				REISERFS_SECRM_FL |	\
				REISERFS_COMPR_FL |	\
				REISERFS_NOTAIL_FL )

struct stat_data {
	__le16 sd_mode;		
	__le16 sd_attrs;	
	__le32 sd_nlink;	
	__le64 sd_size;		
	__le32 sd_uid;		
	__le32 sd_gid;		
	__le32 sd_atime;	
	__le32 sd_mtime;	
	__le32 sd_ctime;	
	__le32 sd_blocks;
	union {
		__le32 sd_rdev;
		__le32 sd_generation;
		
	} __attribute__ ((__packed__)) u;
} __attribute__ ((__packed__));
#define SD_SIZE (sizeof(struct stat_data))
#define SD_V2_SIZE              SD_SIZE
#define stat_data_v2(ih)        (ih_version (ih) == KEY_FORMAT_3_6)
#define sd_v2_mode(sdp)         (le16_to_cpu((sdp)->sd_mode))
#define set_sd_v2_mode(sdp,v)   ((sdp)->sd_mode = cpu_to_le16(v))
#define sd_v2_nlink(sdp)        (le32_to_cpu((sdp)->sd_nlink))
#define set_sd_v2_nlink(sdp,v)  ((sdp)->sd_nlink = cpu_to_le32(v))
#define sd_v2_size(sdp)         (le64_to_cpu((sdp)->sd_size))
#define set_sd_v2_size(sdp,v)   ((sdp)->sd_size = cpu_to_le64(v))
#define sd_v2_uid(sdp)          (le32_to_cpu((sdp)->sd_uid))
#define set_sd_v2_uid(sdp,v)    ((sdp)->sd_uid = cpu_to_le32(v))
#define sd_v2_gid(sdp)          (le32_to_cpu((sdp)->sd_gid))
#define set_sd_v2_gid(sdp,v)    ((sdp)->sd_gid = cpu_to_le32(v))
#define sd_v2_atime(sdp)        (le32_to_cpu((sdp)->sd_atime))
#define set_sd_v2_atime(sdp,v)  ((sdp)->sd_atime = cpu_to_le32(v))
#define sd_v2_mtime(sdp)        (le32_to_cpu((sdp)->sd_mtime))
#define set_sd_v2_mtime(sdp,v)  ((sdp)->sd_mtime = cpu_to_le32(v))
#define sd_v2_ctime(sdp)        (le32_to_cpu((sdp)->sd_ctime))
#define set_sd_v2_ctime(sdp,v)  ((sdp)->sd_ctime = cpu_to_le32(v))
#define sd_v2_blocks(sdp)       (le32_to_cpu((sdp)->sd_blocks))
#define set_sd_v2_blocks(sdp,v) ((sdp)->sd_blocks = cpu_to_le32(v))
#define sd_v2_rdev(sdp)         (le32_to_cpu((sdp)->u.sd_rdev))
#define set_sd_v2_rdev(sdp,v)   ((sdp)->u.sd_rdev = cpu_to_le32(v))
#define sd_v2_generation(sdp)   (le32_to_cpu((sdp)->u.sd_generation))
#define set_sd_v2_generation(sdp,v) ((sdp)->u.sd_generation = cpu_to_le32(v))
#define sd_v2_attrs(sdp)         (le16_to_cpu((sdp)->sd_attrs))
#define set_sd_v2_attrs(sdp,v)   ((sdp)->sd_attrs = cpu_to_le16(v))

#define SD_OFFSET  0
#define SD_UNIQUENESS 0
#define DOT_OFFSET 1
#define DOT_DOT_OFFSET 2
#define DIRENTRY_UNIQUENESS 500

#define FIRST_ITEM_OFFSET 1



struct reiserfs_de_head {
	__le32 deh_offset;	
	__le32 deh_dir_id;	
	__le32 deh_objectid;	
	__le16 deh_location;	
	__le16 deh_state;	
} __attribute__ ((__packed__));
#define DEH_SIZE                  sizeof(struct reiserfs_de_head)
#define deh_offset(p_deh)         (le32_to_cpu((p_deh)->deh_offset))
#define deh_dir_id(p_deh)         (le32_to_cpu((p_deh)->deh_dir_id))
#define deh_objectid(p_deh)       (le32_to_cpu((p_deh)->deh_objectid))
#define deh_location(p_deh)       (le16_to_cpu((p_deh)->deh_location))
#define deh_state(p_deh)          (le16_to_cpu((p_deh)->deh_state))

#define put_deh_offset(p_deh,v)   ((p_deh)->deh_offset = cpu_to_le32((v)))
#define put_deh_dir_id(p_deh,v)   ((p_deh)->deh_dir_id = cpu_to_le32((v)))
#define put_deh_objectid(p_deh,v) ((p_deh)->deh_objectid = cpu_to_le32((v)))
#define put_deh_location(p_deh,v) ((p_deh)->deh_location = cpu_to_le16((v)))
#define put_deh_state(p_deh,v)    ((p_deh)->deh_state = cpu_to_le16((v)))

#define EMPTY_DIR_SIZE \
(DEH_SIZE * 2 + ROUND_UP (strlen (".")) + ROUND_UP (strlen ("..")))

#define EMPTY_DIR_SIZE_V1 (DEH_SIZE * 2 + 3)

#define DEH_Statdata 0		
#define DEH_Visible 2

#if BITS_PER_LONG == 64 || defined(__s390__) || defined(__hppa__)
#   define ADDR_UNALIGNED_BITS  (3)
#endif

#ifdef ADDR_UNALIGNED_BITS

#   define aligned_address(addr)           ((void *)((long)(addr) & ~((1UL << ADDR_UNALIGNED_BITS) - 1)))
#   define unaligned_offset(addr)          (((int)((long)(addr) & ((1 << ADDR_UNALIGNED_BITS) - 1))) << 3)

#   define set_bit_unaligned(nr, addr)	\
	__test_and_set_bit_le((nr) + unaligned_offset(addr), aligned_address(addr))
#   define clear_bit_unaligned(nr, addr)	\
	__test_and_clear_bit_le((nr) + unaligned_offset(addr), aligned_address(addr))
#   define test_bit_unaligned(nr, addr)	\
	test_bit_le((nr) + unaligned_offset(addr), aligned_address(addr))

#else

#   define set_bit_unaligned(nr, addr)	__test_and_set_bit_le(nr, addr)
#   define clear_bit_unaligned(nr, addr)	__test_and_clear_bit_le(nr, addr)
#   define test_bit_unaligned(nr, addr)	test_bit_le(nr, addr)

#endif

#define mark_de_with_sd(deh)        set_bit_unaligned (DEH_Statdata, &((deh)->deh_state))
#define mark_de_without_sd(deh)     clear_bit_unaligned (DEH_Statdata, &((deh)->deh_state))
#define mark_de_visible(deh)	    set_bit_unaligned (DEH_Visible, &((deh)->deh_state))
#define mark_de_hidden(deh)	    clear_bit_unaligned (DEH_Visible, &((deh)->deh_state))

#define de_with_sd(deh)		    test_bit_unaligned (DEH_Statdata, &((deh)->deh_state))
#define de_visible(deh)	    	    test_bit_unaligned (DEH_Visible, &((deh)->deh_state))
#define de_hidden(deh)	    	    !test_bit_unaligned (DEH_Visible, &((deh)->deh_state))

extern void make_empty_dir_item_v1(char *body, __le32 dirid, __le32 objid,
				   __le32 par_dirid, __le32 par_objid);
extern void make_empty_dir_item(char *body, __le32 dirid, __le32 objid,
				__le32 par_dirid, __le32 par_objid);

 
#define B_I_PITEM(bh,ih) ( (bh)->b_data + ih_location(ih) )
#define B_I_DEH(bh,ih) ((struct reiserfs_de_head *)(B_I_PITEM(bh,ih)))

static inline int entry_length(const struct buffer_head *bh,
			       const struct item_head *ih, int pos_in_item)
{
	struct reiserfs_de_head *deh;

	deh = B_I_DEH(bh, ih) + pos_in_item;
	if (pos_in_item)
		return deh_location(deh - 1) - deh_location(deh);

	return ih_item_len(ih) - deh_location(deh);
}

#define I_ENTRY_COUNT(ih) (ih_entry_count((ih)))

#define B_I_E_NAME(bh,ih,entry_num) ((char *)(bh->b_data + ih_location(ih) + deh_location(B_I_DEH(bh,ih)+(entry_num))))

#define REISERFS_MAX_NAME(block_size) 255

struct reiserfs_dir_entry {
	struct buffer_head *de_bh;
	int de_item_num;
	struct item_head *de_ih;
	int de_entry_num;
	struct reiserfs_de_head *de_deh;
	int de_entrylen;
	int de_namelen;
	char *de_name;
	unsigned long *de_gen_number_bit_string;

	__u32 de_dir_id;
	__u32 de_objectid;

	struct cpu_key de_entry_key;
};


#define B_I_DEH_ENTRY_FILE_NAME(bh,ih,deh) (B_I_PITEM (bh, ih) + deh_location(deh))

#define I_DEH_N_ENTRY_FILE_NAME_LENGTH(ih,deh,entry_num) \
(I_DEH_N_ENTRY_LENGTH (ih, deh, entry_num) - (de_with_sd (deh) ? SD_SIZE : 0))

#define GET_HASH_VALUE(offset) ((offset) & 0x7fffff80LL)
#define GET_GENERATION_NUMBER(offset) ((offset) & 0x7fLL)
#define MAX_GENERATION_NUMBER  127

#define SET_GENERATION_NUMBER(offset,gen_number) (GET_HASH_VALUE(offset)|(gen_number))


struct disk_child {
	__le32 dc_block_number;	
	__le16 dc_size;		
	__le16 dc_reserved;
};

#define DC_SIZE (sizeof(struct disk_child))
#define dc_block_number(dc_p)	(le32_to_cpu((dc_p)->dc_block_number))
#define dc_size(dc_p)		(le16_to_cpu((dc_p)->dc_size))
#define put_dc_block_number(dc_p, val)   do { (dc_p)->dc_block_number = cpu_to_le32(val); } while(0)
#define put_dc_size(dc_p, val)   do { (dc_p)->dc_size = cpu_to_le16(val); } while(0)

#define B_N_CHILD(bh, n_pos)  ((struct disk_child *)\
((bh)->b_data + BLKH_SIZE + B_NR_ITEMS(bh) * KEY_SIZE + DC_SIZE * (n_pos)))

#define B_N_CHILD_NUM(bh, n_pos) (dc_block_number(B_N_CHILD(bh, n_pos)))
#define PUT_B_N_CHILD_NUM(bh, n_pos, val) \
				(put_dc_block_number(B_N_CHILD(bh, n_pos), val))

 
 
#define MAX_CHILD_SIZE(bh) ((int)( (bh)->b_size - BLKH_SIZE ))

#define B_CHILD_SIZE(cur) (MAX_CHILD_SIZE(cur)-(B_FREE_SPACE(cur)))

#define MAX_NR_KEY(bh) ( (MAX_CHILD_SIZE(bh)-DC_SIZE)/(KEY_SIZE+DC_SIZE) )
#define MIN_NR_KEY(bh)    (MAX_NR_KEY(bh)/2)



struct path_element {
	struct buffer_head *pe_buffer;	
	int pe_position;	
	
};

#define MAX_HEIGHT 5		
#define EXTENDED_MAX_HEIGHT         7	
#define FIRST_PATH_ELEMENT_OFFSET   2	

#define ILLEGAL_PATH_ELEMENT_OFFSET 1	
#define MAX_FEB_SIZE 6		


#define PATH_READA	0x1	
#define PATH_READA_BACK 0x2	

struct treepath {
	int path_length;	
	int reada;
	struct path_element path_elements[EXTENDED_MAX_HEIGHT];	
	int pos_in_item;
};

#define pos_in_item(path) ((path)->pos_in_item)

#define INITIALIZE_PATH(var) \
struct treepath var = {.path_length = ILLEGAL_PATH_ELEMENT_OFFSET, .reada = 0,}

#define PATH_OFFSET_PELEMENT(path, n_offset)  ((path)->path_elements + (n_offset))

#define PATH_OFFSET_PBUFFER(path, n_offset)   (PATH_OFFSET_PELEMENT(path, n_offset)->pe_buffer)

#define PATH_OFFSET_POSITION(path, n_offset) (PATH_OFFSET_PELEMENT(path, n_offset)->pe_position)

#define PATH_PLAST_BUFFER(path) (PATH_OFFSET_PBUFFER((path), (path)->path_length))
#define PATH_LAST_POSITION(path) (PATH_OFFSET_POSITION((path), (path)->path_length))

#define PATH_PITEM_HEAD(path)    B_N_PITEM_HEAD(PATH_PLAST_BUFFER(path), PATH_LAST_POSITION(path))

#define PATH_H_PBUFFER(path, h) PATH_OFFSET_PBUFFER (path, path->path_length - (h))	
#define PATH_H_PPARENT(path, h) PATH_H_PBUFFER (path, (h) + 1)	
#define PATH_H_POSITION(path, h) PATH_OFFSET_POSITION (path, path->path_length - (h))
#define PATH_H_B_ITEM_ORDER(path, h) PATH_H_POSITION(path, h + 1)	

#define PATH_H_PATH_OFFSET(path, n_h) ((path)->path_length - (n_h))

#define get_last_bh(path) PATH_PLAST_BUFFER(path)
#define get_ih(path) PATH_PITEM_HEAD(path)
#define get_item_pos(path) PATH_LAST_POSITION(path)
#define get_item(path) ((void *)B_N_PITEM(PATH_PLAST_BUFFER(path), PATH_LAST_POSITION (path)))
#define item_moved(ih,path) comp_items(ih, path)
#define path_changed(ih,path) comp_items (ih, path)


#define UNFM_P_SIZE (sizeof(unp_t))
#define UNFM_P_SHIFT 2

#define INODE_PKEY(inode) ((struct reiserfs_key *)(REISERFS_I(inode)->i_key))

#define MAX_UL_INT 0xffffffff
#define MAX_INT    0x7ffffff
#define MAX_US_INT 0xffff

#define U32_MAX (~(__u32)0)

static inline loff_t max_reiserfs_offset(struct inode *inode)
{
	if (get_inode_item_key_version(inode) == KEY_FORMAT_3_5)
		return (loff_t) U32_MAX;

	return (loff_t) ((~(__u64) 0) >> 4);
}

#define MAX_KEY_OBJECTID	MAX_UL_INT

#define MAX_B_NUM  MAX_UL_INT
#define MAX_FC_NUM MAX_US_INT

#define REISERFS_LINK_MAX (MAX_US_INT - 1000)

#define REISERFS_KERNEL_MEM		0	
#define REISERFS_USER_MEM		1	

#define fs_generation(s) (REISERFS_SB(s)->s_generation_counter)
#define get_generation(s) atomic_read (&fs_generation(s))
#define FILESYSTEM_CHANGED_TB(tb)  (get_generation((tb)->tb_sb) != (tb)->fs_gen)
#define __fs_changed(gen,s) (gen != get_generation (s))
#define fs_changed(gen,s)		\
({					\
	reiserfs_cond_resched(s);	\
	__fs_changed(gen, s);		\
})


#define VI_TYPE_LEFT_MERGEABLE 1
#define VI_TYPE_RIGHT_MERGEABLE 2

struct virtual_item {
	int vi_index;		
	unsigned short vi_type;	
	unsigned short vi_item_len;	
	struct item_head *vi_ih;
	const char *vi_item;	
	const void *vi_new_data;	
	void *vi_uarea;		
};

struct virtual_node {
	char *vn_free_ptr;	
	unsigned short vn_nr_item;	
	short vn_size;		
	short vn_mode;		
	short vn_affected_item_num;
	short vn_pos_in_item;
	struct item_head *vn_ins_ih;	
	const void *vn_data;
	struct virtual_item *vn_vi;	
};

struct direntry_uarea {
	int flags;
	__u16 entry_count;
	__u16 entry_sizes[1];
} __attribute__ ((__packed__));



#define MAX_FREE_BLOCK 7	

#define MAX_AMOUNT_NEEDED 2

struct tree_balance {
	int tb_mode;
	int need_balance_dirty;
	struct super_block *tb_sb;
	struct reiserfs_transaction_handle *transaction_handle;
	struct treepath *tb_path;
	struct buffer_head *L[MAX_HEIGHT];	
	struct buffer_head *R[MAX_HEIGHT];	
	struct buffer_head *FL[MAX_HEIGHT];	
	struct buffer_head *FR[MAX_HEIGHT];	
	struct buffer_head *CFL[MAX_HEIGHT];	
	struct buffer_head *CFR[MAX_HEIGHT];	

	struct buffer_head *FEB[MAX_FEB_SIZE];	
	struct buffer_head *used[MAX_FEB_SIZE];
	struct buffer_head *thrown[MAX_FEB_SIZE];
	int lnum[MAX_HEIGHT];	
	int rnum[MAX_HEIGHT];	
	int lkey[MAX_HEIGHT];	
	int rkey[MAX_HEIGHT];	
	int insert_size[MAX_HEIGHT];	
	int blknum[MAX_HEIGHT];	

	
	int cur_blknum;		
	int s0num;		
	int s1num;		
	int s2num;		
	int lbytes;		
	
	
	int rbytes;		
	
	
	int s1bytes;		
	
	int s2bytes;
	struct buffer_head *buf_to_free[MAX_FREE_BLOCK];	
	char *vn_buf;		
	int vn_buf_size;	
	struct virtual_node *tb_vn;	

	int fs_gen;		
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	struct in_core_key key;	
#endif
};


#define M_INSERT	'i'
#define M_PASTE		'p'
#define M_DELETE	'd'
#define M_CUT 		'c'

#define M_INTERNAL	'n'

#define M_SKIP_BALANCING 		's'
#define M_CONVERT	'v'

#define LEAF_FROM_S_TO_L 0
#define LEAF_FROM_S_TO_R 1
#define LEAF_FROM_R_TO_L 2
#define LEAF_FROM_L_TO_R 3
#define LEAF_FROM_S_TO_SNEW 4

#define FIRST_TO_LAST 0
#define LAST_TO_FIRST 1

struct buffer_info {
	struct tree_balance *tb;
	struct buffer_head *bi_bh;
	struct buffer_head *bi_parent;
	int bi_position;
};

static inline struct super_block *sb_from_tb(struct tree_balance *tb)
{
	return tb ? tb->tb_sb : NULL;
}

static inline struct super_block *sb_from_bi(struct buffer_info *bi)
{
	return bi ? sb_from_tb(bi->tb) : NULL;
}


struct item_operations {
	int (*bytes_number) (struct item_head * ih, int block_size);
	void (*decrement_key) (struct cpu_key *);
	int (*is_left_mergeable) (struct reiserfs_key * ih,
				  unsigned long bsize);
	void (*print_item) (struct item_head *, char *item);
	void (*check_item) (struct item_head *, char *item);

	int (*create_vi) (struct virtual_node * vn, struct virtual_item * vi,
			  int is_affected, int insert_size);
	int (*check_left) (struct virtual_item * vi, int free,
			   int start_skip, int end_skip);
	int (*check_right) (struct virtual_item * vi, int free);
	int (*part_size) (struct virtual_item * vi, int from, int to);
	int (*unit_num) (struct virtual_item * vi);
	void (*print_vi) (struct virtual_item * vi);
};

extern struct item_operations *item_ops[TYPE_ANY + 1];

#define op_bytes_number(ih,bsize)                    item_ops[le_ih_k_type (ih)]->bytes_number (ih, bsize)
#define op_is_left_mergeable(key,bsize)              item_ops[le_key_k_type (le_key_version (key), key)]->is_left_mergeable (key, bsize)
#define op_print_item(ih,item)                       item_ops[le_ih_k_type (ih)]->print_item (ih, item)
#define op_check_item(ih,item)                       item_ops[le_ih_k_type (ih)]->check_item (ih, item)
#define op_create_vi(vn,vi,is_affected,insert_size)  item_ops[le_ih_k_type ((vi)->vi_ih)]->create_vi (vn,vi,is_affected,insert_size)
#define op_check_left(vi,free,start_skip,end_skip) item_ops[(vi)->vi_index]->check_left (vi, free, start_skip, end_skip)
#define op_check_right(vi,free)                      item_ops[(vi)->vi_index]->check_right (vi, free)
#define op_part_size(vi,from,to)                     item_ops[(vi)->vi_index]->part_size (vi, from, to)
#define op_unit_num(vi)				     item_ops[(vi)->vi_index]->unit_num (vi)
#define op_print_vi(vi)                              item_ops[(vi)->vi_index]->print_vi (vi)

#define COMP_SHORT_KEYS comp_short_keys

#define I_UNFM_NUM(ih)	(ih_item_len(ih) / UNFM_P_SIZE)

#define I_POS_UNFM_SIZE(ih,pos,size) (((pos) == I_UNFM_NUM(ih) - 1 ) ? (size) - ih_free_space(ih) : (size))


#define B_N_PITEM_HEAD(bh,item_num) ( (struct item_head * )((bh)->b_data + BLKH_SIZE) + (item_num) )

#define B_N_PDELIM_KEY(bh,item_num) ( (struct reiserfs_key * )((bh)->b_data + BLKH_SIZE) + (item_num) )

#define B_N_PKEY(bh,item_num) ( &(B_N_PITEM_HEAD(bh,item_num)->ih_key) )

#define B_N_PITEM(bh,item_num) ( (bh)->b_data + ih_location(B_N_PITEM_HEAD((bh),(item_num))))

#define B_N_STAT_DATA(bh,nr) \
( (struct stat_data *)((bh)->b_data + ih_location(B_N_PITEM_HEAD((bh),(nr))) ) )

    

#define B_I_STAT_DATA(bh, ih) ( (struct stat_data * )((bh)->b_data + ih_location(ih)) )

#define MAX_DIRECT_ITEM_LEN(size) ((size) - BLKH_SIZE - 2*IH_SIZE - SD_SIZE - UNFM_P_SIZE)

#define B_I_POS_UNFM_POINTER(bh,ih,pos) le32_to_cpu(*(((unp_t *)B_I_PITEM(bh,ih)) + (pos)))
#define PUT_B_I_POS_UNFM_POINTER(bh,ih,pos, val) do {*(((unp_t *)B_I_PITEM(bh,ih)) + (pos)) = cpu_to_le32(val); } while (0)

struct reiserfs_iget_args {
	__u32 objectid;
	__u32 dirid;
};


#define get_journal_desc_magic(bh) (bh->b_data + bh->b_size - 12)

#define journal_trans_half(blocksize) \
	((blocksize - sizeof (struct reiserfs_journal_desc) + sizeof (__u32) - 12) / sizeof (__u32))


/* first block written in a commit.  */
struct reiserfs_journal_desc {
	__le32 j_trans_id;	
	__le32 j_len;		
	__le32 j_mount_id;	
	__le32 j_realblock[1];	
};

#define get_desc_trans_id(d)   le32_to_cpu((d)->j_trans_id)
#define get_desc_trans_len(d)  le32_to_cpu((d)->j_len)
#define get_desc_mount_id(d)   le32_to_cpu((d)->j_mount_id)

#define set_desc_trans_id(d,val)       do { (d)->j_trans_id = cpu_to_le32 (val); } while (0)
#define set_desc_trans_len(d,val)      do { (d)->j_len = cpu_to_le32 (val); } while (0)
#define set_desc_mount_id(d,val)       do { (d)->j_mount_id = cpu_to_le32 (val); } while (0)

/* last block written in a commit */
struct reiserfs_journal_commit {
	__le32 j_trans_id;	
	__le32 j_len;		
	__le32 j_realblock[1];	
};

#define get_commit_trans_id(c) le32_to_cpu((c)->j_trans_id)
#define get_commit_trans_len(c)        le32_to_cpu((c)->j_len)
#define get_commit_mount_id(c) le32_to_cpu((c)->j_mount_id)

#define set_commit_trans_id(c,val)     do { (c)->j_trans_id = cpu_to_le32 (val); } while (0)
#define set_commit_trans_len(c,val)    do { (c)->j_len = cpu_to_le32 (val); } while (0)

/* this header block gets written whenever a transaction is considered fully flushed, and is more recent than the
** last fully flushed transaction.  fully flushed means all the log blocks and all the real blocks are on disk,
** and this transaction does not need to be replayed.
*/
struct reiserfs_journal_header {
	__le32 j_last_flush_trans_id;	
	__le32 j_first_unflushed_offset;	
	__le32 j_mount_id;
	 struct journal_params jh_journal;
};

#define JOURNAL_BLOCK_COUNT 8192	
#define JOURNAL_TRANS_MAX_DEFAULT 1024	
#define JOURNAL_TRANS_MIN_DEFAULT 256
#define JOURNAL_MAX_BATCH_DEFAULT   900	
#define JOURNAL_MIN_RATIO 2
#define JOURNAL_MAX_COMMIT_AGE 30
#define JOURNAL_MAX_TRANS_AGE 30
#define JOURNAL_PER_BALANCE_CNT (3 * (MAX_HEIGHT-2) + 9)
#define JOURNAL_BLOCKS_PER_OBJECT(sb)  (JOURNAL_PER_BALANCE_CNT * 3 + \
					 2 * (REISERFS_QUOTA_INIT_BLOCKS(sb) + \
					      REISERFS_QUOTA_TRANS_BLOCKS(sb)))

#ifdef CONFIG_QUOTA
#define REISERFS_QUOTA_OPTS ((1 << REISERFS_USRQUOTA) | (1 << REISERFS_GRPQUOTA))
#define REISERFS_QUOTA_TRANS_BLOCKS(s) (REISERFS_SB(s)->s_mount_opt & REISERFS_QUOTA_OPTS ? 2 : 0)
#define REISERFS_QUOTA_INIT_BLOCKS(s) (REISERFS_SB(s)->s_mount_opt & REISERFS_QUOTA_OPTS ? \
(DQUOT_INIT_ALLOC*(JOURNAL_PER_BALANCE_CNT+2)+DQUOT_INIT_REWRITE+1) : 0)
#define REISERFS_QUOTA_DEL_BLOCKS(s) (REISERFS_SB(s)->s_mount_opt & REISERFS_QUOTA_OPTS ? \
(DQUOT_DEL_ALLOC*(JOURNAL_PER_BALANCE_CNT+2)+DQUOT_DEL_REWRITE+1) : 0)
#else
#define REISERFS_QUOTA_TRANS_BLOCKS(s) 0
#define REISERFS_QUOTA_INIT_BLOCKS(s) 0
#define REISERFS_QUOTA_DEL_BLOCKS(s) 0
#endif

#define REISERFS_MIN_BITMAP_NODES 10
#define REISERFS_MAX_BITMAP_NODES 100

#define JBH_HASH_SHIFT 13	
#define JBH_HASH_MASK 8191

#define _jhashfn(sb,block)	\
	(((unsigned long)sb>>L1_CACHE_SHIFT) ^ \
	 (((block)<<(JBH_HASH_SHIFT - 6)) ^ ((block) >> 13) ^ ((block) << (JBH_HASH_SHIFT - 12))))
#define journal_hash(t,sb,block) ((t)[_jhashfn((sb),(block)) & JBH_HASH_MASK])

#define journal_find_get_block(s, block) __find_get_block(SB_JOURNAL(s)->j_dev_bd, block, s->s_blocksize)
#define journal_getblk(s, block) __getblk(SB_JOURNAL(s)->j_dev_bd, block, s->s_blocksize)
#define journal_bread(s, block) __bread(SB_JOURNAL(s)->j_dev_bd, block, s->s_blocksize)

enum reiserfs_bh_state_bits {
	BH_JDirty = BH_PrivateStart,	
	BH_JDirty_wait,
	BH_JNew,		/* disk block was taken off free list before
				 * being in a finished transaction, or
				 * written to disk. Can be reused immed. */
	BH_JPrepared,
	BH_JRestore_dirty,
	BH_JTest,		
};

BUFFER_FNS(JDirty, journaled);
TAS_BUFFER_FNS(JDirty, journaled);
BUFFER_FNS(JDirty_wait, journal_dirty);
TAS_BUFFER_FNS(JDirty_wait, journal_dirty);
BUFFER_FNS(JNew, journal_new);
TAS_BUFFER_FNS(JNew, journal_new);
BUFFER_FNS(JPrepared, journal_prepared);
TAS_BUFFER_FNS(JPrepared, journal_prepared);
BUFFER_FNS(JRestore_dirty, journal_restore_dirty);
TAS_BUFFER_FNS(JRestore_dirty, journal_restore_dirty);
BUFFER_FNS(JTest, journal_test);
TAS_BUFFER_FNS(JTest, journal_test);

struct reiserfs_transaction_handle {
	struct super_block *t_super;	
	int t_refcount;
	int t_blocks_logged;	
	int t_blocks_allocated;	
	unsigned int t_trans_id;	
	void *t_handle_save;	
	unsigned displace_new_blocks:1;	
	struct list_head t_list;
};

struct reiserfs_jh {
	struct reiserfs_journal_list *jl;
	struct buffer_head *bh;
	struct list_head list;
};

void reiserfs_free_jh(struct buffer_head *bh);
int reiserfs_add_tail_list(struct inode *inode, struct buffer_head *bh);
int reiserfs_add_ordered_list(struct inode *inode, struct buffer_head *bh);
int journal_mark_dirty(struct reiserfs_transaction_handle *,
		       struct super_block *, struct buffer_head *bh);

static inline int reiserfs_file_data_log(struct inode *inode)
{
	if (reiserfs_data_log(inode->i_sb) ||
	    (REISERFS_I(inode)->i_flags & i_data_log))
		return 1;
	return 0;
}

static inline int reiserfs_transaction_running(struct super_block *s)
{
	struct reiserfs_transaction_handle *th = current->journal_info;
	if (th && th->t_super == s)
		return 1;
	if (th && th->t_super == NULL)
		BUG();
	return 0;
}

static inline int reiserfs_transaction_free_space(struct reiserfs_transaction_handle *th)
{
	return th->t_blocks_allocated - th->t_blocks_logged;
}

struct reiserfs_transaction_handle *reiserfs_persistent_transaction(struct
								    super_block
								    *,
								    int count);
int reiserfs_end_persistent_transaction(struct reiserfs_transaction_handle *);
int reiserfs_commit_page(struct inode *inode, struct page *page,
			 unsigned from, unsigned to);
int reiserfs_flush_old_commits(struct super_block *);
int reiserfs_commit_for_inode(struct inode *);
int reiserfs_inode_needs_commit(struct inode *);
void reiserfs_update_inode_transaction(struct inode *);
void reiserfs_wait_on_write_block(struct super_block *s);
void reiserfs_block_writes(struct reiserfs_transaction_handle *th);
void reiserfs_allow_writes(struct super_block *s);
void reiserfs_check_lock_depth(struct super_block *s, char *caller);
int reiserfs_prepare_for_journal(struct super_block *, struct buffer_head *bh,
				 int wait);
void reiserfs_restore_prepared_buffer(struct super_block *,
				      struct buffer_head *bh);
int journal_init(struct super_block *, const char *j_dev_name, int old_format,
		 unsigned int);
int journal_release(struct reiserfs_transaction_handle *, struct super_block *);
int journal_release_error(struct reiserfs_transaction_handle *,
			  struct super_block *);
int journal_end(struct reiserfs_transaction_handle *, struct super_block *,
		unsigned long);
int journal_end_sync(struct reiserfs_transaction_handle *, struct super_block *,
		     unsigned long);
int journal_mark_freed(struct reiserfs_transaction_handle *,
		       struct super_block *, b_blocknr_t blocknr);
int journal_transaction_should_end(struct reiserfs_transaction_handle *, int);
int reiserfs_in_journal(struct super_block *sb, unsigned int bmap_nr,
			 int bit_nr, int searchall, b_blocknr_t *next);
int journal_begin(struct reiserfs_transaction_handle *,
		  struct super_block *sb, unsigned long);
int journal_join_abort(struct reiserfs_transaction_handle *,
		       struct super_block *sb, unsigned long);
void reiserfs_abort_journal(struct super_block *sb, int errno);
void reiserfs_abort(struct super_block *sb, int errno, const char *fmt, ...);
int reiserfs_allocate_list_bitmaps(struct super_block *s,
				   struct reiserfs_list_bitmap *, unsigned int);

void add_save_link(struct reiserfs_transaction_handle *th,
		   struct inode *inode, int truncate);
int remove_save_link(struct inode *inode, int truncate);

__u32 reiserfs_get_unused_objectid(struct reiserfs_transaction_handle *th);
void reiserfs_release_objectid(struct reiserfs_transaction_handle *th,
			       __u32 objectid_to_release);
int reiserfs_convert_objectid_map_v1(struct super_block *);

int B_IS_IN_TREE(const struct buffer_head *);
extern void copy_item_head(struct item_head *to,
			   const struct item_head *from);

extern int comp_short_keys(const struct reiserfs_key *le_key,
			   const struct cpu_key *cpu_key);
extern void le_key2cpu_key(struct cpu_key *to, const struct reiserfs_key *from);

extern int comp_le_keys(const struct reiserfs_key *,
			const struct reiserfs_key *);
extern int comp_short_le_keys(const struct reiserfs_key *,
			      const struct reiserfs_key *);

static inline int le_key_version(const struct reiserfs_key *key)
{
	int type;

	type = offset_v2_k_type(&(key->u.k_offset_v2));
	if (type != TYPE_DIRECT && type != TYPE_INDIRECT
	    && type != TYPE_DIRENTRY)
		return KEY_FORMAT_3_5;

	return KEY_FORMAT_3_6;

}

static inline void copy_key(struct reiserfs_key *to,
			    const struct reiserfs_key *from)
{
	memcpy(to, from, KEY_SIZE);
}

int comp_items(const struct item_head *stored_ih, const struct treepath *path);
const struct reiserfs_key *get_rkey(const struct treepath *chk_path,
				    const struct super_block *sb);
int search_by_key(struct super_block *, const struct cpu_key *,
		  struct treepath *, int);
#define search_item(s,key,path) search_by_key (s, key, path, DISK_LEAF_NODE_LEVEL)
int search_for_position_by_key(struct super_block *sb,
			       const struct cpu_key *cpu_key,
			       struct treepath *search_path);
extern void decrement_bcount(struct buffer_head *bh);
void decrement_counters_in_path(struct treepath *search_path);
void pathrelse(struct treepath *search_path);
int reiserfs_check_path(struct treepath *p);
void pathrelse_and_restore(struct super_block *s, struct treepath *search_path);

int reiserfs_insert_item(struct reiserfs_transaction_handle *th,
			 struct treepath *path,
			 const struct cpu_key *key,
			 struct item_head *ih,
			 struct inode *inode, const char *body);

int reiserfs_paste_into_item(struct reiserfs_transaction_handle *th,
			     struct treepath *path,
			     const struct cpu_key *key,
			     struct inode *inode,
			     const char *body, int paste_size);

int reiserfs_cut_from_item(struct reiserfs_transaction_handle *th,
			   struct treepath *path,
			   struct cpu_key *key,
			   struct inode *inode,
			   struct page *page, loff_t new_file_size);

int reiserfs_delete_item(struct reiserfs_transaction_handle *th,
			 struct treepath *path,
			 const struct cpu_key *key,
			 struct inode *inode, struct buffer_head *un_bh);

void reiserfs_delete_solid_item(struct reiserfs_transaction_handle *th,
				struct inode *inode, struct reiserfs_key *key);
int reiserfs_delete_object(struct reiserfs_transaction_handle *th,
			   struct inode *inode);
int reiserfs_do_truncate(struct reiserfs_transaction_handle *th,
			 struct inode *inode, struct page *,
			 int update_timestamps);

#define i_block_size(inode) ((inode)->i_sb->s_blocksize)
#define file_size(inode) ((inode)->i_size)
#define tail_size(inode) (file_size (inode) & (i_block_size (inode) - 1))

#define tail_has_to_be_packed(inode) (have_large_tails ((inode)->i_sb)?\
!STORE_TAIL_IN_UNFM_S1(file_size (inode), tail_size(inode), inode->i_sb->s_blocksize):have_small_tails ((inode)->i_sb)?!STORE_TAIL_IN_UNFM_S2(file_size (inode), tail_size(inode), inode->i_sb->s_blocksize):0 )

void padd_item(char *item, int total_length, int length);

#define GET_BLOCK_NO_CREATE 0	
#define GET_BLOCK_CREATE 1	
#define GET_BLOCK_NO_HOLE 2	
#define GET_BLOCK_READ_DIRECT 4	
#define GET_BLOCK_NO_IMUX     8	
#define GET_BLOCK_NO_DANGLE   16	

void reiserfs_read_locked_inode(struct inode *inode,
				struct reiserfs_iget_args *args);
int reiserfs_find_actor(struct inode *inode, void *p);
int reiserfs_init_locked_inode(struct inode *inode, void *p);
void reiserfs_evict_inode(struct inode *inode);
int reiserfs_write_inode(struct inode *inode, struct writeback_control *wbc);
int reiserfs_get_block(struct inode *inode, sector_t block,
		       struct buffer_head *bh_result, int create);
struct dentry *reiserfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
				     int fh_len, int fh_type);
struct dentry *reiserfs_fh_to_parent(struct super_block *sb, struct fid *fid,
				     int fh_len, int fh_type);
int reiserfs_encode_fh(struct dentry *dentry, __u32 * data, int *lenp,
		       int connectable);

int reiserfs_truncate_file(struct inode *, int update_timestamps);
void make_cpu_key(struct cpu_key *cpu_key, struct inode *inode, loff_t offset,
		  int type, int key_length);
void make_le_item_head(struct item_head *ih, const struct cpu_key *key,
		       int version,
		       loff_t offset, int type, int length, int entry_count);
struct inode *reiserfs_iget(struct super_block *s, const struct cpu_key *key);

struct reiserfs_security_handle;
int reiserfs_new_inode(struct reiserfs_transaction_handle *th,
		       struct inode *dir, umode_t mode,
		       const char *symname, loff_t i_size,
		       struct dentry *dentry, struct inode *inode,
		       struct reiserfs_security_handle *security);

void reiserfs_update_sd_size(struct reiserfs_transaction_handle *th,
			     struct inode *inode, loff_t size);

static inline void reiserfs_update_sd(struct reiserfs_transaction_handle *th,
				      struct inode *inode)
{
	reiserfs_update_sd_size(th, inode, inode->i_size);
}

void sd_attrs_to_i_attrs(__u16 sd_attrs, struct inode *inode);
void i_attrs_to_sd_attrs(struct inode *inode, __u16 * sd_attrs);
int reiserfs_setattr(struct dentry *dentry, struct iattr *attr);

int __reiserfs_write_begin(struct page *page, unsigned from, unsigned len);

void set_de_name_and_namelen(struct reiserfs_dir_entry *de);
int search_by_entry_key(struct super_block *sb, const struct cpu_key *key,
			struct treepath *path, struct reiserfs_dir_entry *de);
struct dentry *reiserfs_get_parent(struct dentry *);

#ifdef CONFIG_REISERFS_PROC_INFO
int reiserfs_proc_info_init(struct super_block *sb);
int reiserfs_proc_info_done(struct super_block *sb);
int reiserfs_proc_info_global_init(void);
int reiserfs_proc_info_global_done(void);

#define PROC_EXP( e )   e

#define __PINFO( sb ) REISERFS_SB(sb) -> s_proc_info_data
#define PROC_INFO_MAX( sb, field, value )								\
    __PINFO( sb ).field =												\
        max( REISERFS_SB( sb ) -> s_proc_info_data.field, value )
#define PROC_INFO_INC( sb, field ) ( ++ ( __PINFO( sb ).field ) )
#define PROC_INFO_ADD( sb, field, val ) ( __PINFO( sb ).field += ( val ) )
#define PROC_INFO_BH_STAT( sb, bh, level )							\
    PROC_INFO_INC( sb, sbk_read_at[ ( level ) ] );						\
    PROC_INFO_ADD( sb, free_at[ ( level ) ], B_FREE_SPACE( bh ) );	\
    PROC_INFO_ADD( sb, items_at[ ( level ) ], B_NR_ITEMS( bh ) )
#else
static inline int reiserfs_proc_info_init(struct super_block *sb)
{
	return 0;
}

static inline int reiserfs_proc_info_done(struct super_block *sb)
{
	return 0;
}

static inline int reiserfs_proc_info_global_init(void)
{
	return 0;
}

static inline int reiserfs_proc_info_global_done(void)
{
	return 0;
}

#define PROC_EXP( e )
#define VOID_V ( ( void ) 0 )
#define PROC_INFO_MAX( sb, field, value ) VOID_V
#define PROC_INFO_INC( sb, field ) VOID_V
#define PROC_INFO_ADD( sb, field, val ) VOID_V
#define PROC_INFO_BH_STAT(sb, bh, n_node_level) VOID_V
#endif

extern const struct inode_operations reiserfs_dir_inode_operations;
extern const struct inode_operations reiserfs_symlink_inode_operations;
extern const struct inode_operations reiserfs_special_inode_operations;
extern const struct file_operations reiserfs_dir_operations;
int reiserfs_readdir_dentry(struct dentry *, void *, filldir_t, loff_t *);

int direct2indirect(struct reiserfs_transaction_handle *, struct inode *,
		    struct treepath *, struct buffer_head *, loff_t);
int indirect2direct(struct reiserfs_transaction_handle *, struct inode *,
		    struct page *, struct treepath *, const struct cpu_key *,
		    loff_t, char *);
void reiserfs_unmap_buffer(struct buffer_head *);

extern const struct inode_operations reiserfs_file_inode_operations;
extern const struct file_operations reiserfs_file_operations;
extern const struct address_space_operations reiserfs_address_space_operations;


int fix_nodes(int n_op_mode, struct tree_balance *tb,
	      struct item_head *ins_ih, const void *);
void unfix_nodes(struct tree_balance *);

void __reiserfs_panic(struct super_block *s, const char *id,
		      const char *function, const char *fmt, ...)
    __attribute__ ((noreturn));
#define reiserfs_panic(s, id, fmt, args...) \
	__reiserfs_panic(s, id, __func__, fmt, ##args)
void __reiserfs_error(struct super_block *s, const char *id,
		      const char *function, const char *fmt, ...);
#define reiserfs_error(s, id, fmt, args...) \
	 __reiserfs_error(s, id, __func__, fmt, ##args)
void reiserfs_info(struct super_block *s, const char *fmt, ...);
void reiserfs_debug(struct super_block *s, int level, const char *fmt, ...);
void print_indirect_item(struct buffer_head *bh, int item_num);
void store_print_tb(struct tree_balance *tb);
void print_cur_tb(char *mes);
void print_de(struct reiserfs_dir_entry *de);
void print_bi(struct buffer_info *bi, char *mes);
#define PRINT_LEAF_ITEMS 1	
#define PRINT_DIRECTORY_ITEMS 2	
#define PRINT_DIRECT_ITEMS 4	
void print_block(struct buffer_head *bh, ...);
void print_bmap(struct super_block *s, int silent);
void print_bmap_block(int i, char *data, int size, int silent);
void print_objectid_map(struct super_block *s);
void print_block_head(struct buffer_head *bh, char *mes);
void check_leaf(struct buffer_head *bh);
void check_internal(struct buffer_head *bh);
void print_statistics(struct super_block *s);
char *reiserfs_hashname(int code);

int leaf_move_items(int shift_mode, struct tree_balance *tb, int mov_num,
		    int mov_bytes, struct buffer_head *Snew);
int leaf_shift_left(struct tree_balance *tb, int shift_num, int shift_bytes);
int leaf_shift_right(struct tree_balance *tb, int shift_num, int shift_bytes);
void leaf_delete_items(struct buffer_info *cur_bi, int last_first, int first,
		       int del_num, int del_bytes);
void leaf_insert_into_buf(struct buffer_info *bi, int before,
			  struct item_head *inserted_item_ih,
			  const char *inserted_item_body, int zeros_number);
void leaf_paste_in_buffer(struct buffer_info *bi, int pasted_item_num,
			  int pos_in_item, int paste_size, const char *body,
			  int zeros_number);
void leaf_cut_from_buffer(struct buffer_info *bi, int cut_item_num,
			  int pos_in_item, int cut_size);
void leaf_paste_entries(struct buffer_info *bi, int item_num, int before,
			int new_entry_count, struct reiserfs_de_head *new_dehs,
			const char *records, int paste_size);
int balance_internal(struct tree_balance *, int, int, struct item_head *,
		     struct buffer_head **);

void do_balance_mark_leaf_dirty(struct tree_balance *tb,
				struct buffer_head *bh, int flag);
#define do_balance_mark_internal_dirty do_balance_mark_leaf_dirty
#define do_balance_mark_sb_dirty do_balance_mark_leaf_dirty

void do_balance(struct tree_balance *tb, struct item_head *ih,
		const char *body, int flag);
void reiserfs_invalidate_buffer(struct tree_balance *tb,
				struct buffer_head *bh);

int get_left_neighbor_position(struct tree_balance *tb, int h);
int get_right_neighbor_position(struct tree_balance *tb, int h);
void replace_key(struct tree_balance *tb, struct buffer_head *, int,
		 struct buffer_head *, int);
void make_empty_node(struct buffer_info *);
struct buffer_head *get_FEB(struct tree_balance *);


struct __reiserfs_blocknr_hint {
	struct inode *inode;	
	sector_t block;		
	struct in_core_key key;
	struct treepath *path;	
	struct reiserfs_transaction_handle *th;	
	b_blocknr_t beg, end;
	b_blocknr_t search_start;	
	int prealloc_size;	

	unsigned formatted_node:1;	
	unsigned preallocate:1;
};

typedef struct __reiserfs_blocknr_hint reiserfs_blocknr_hint_t;

int reiserfs_parse_alloc_options(struct super_block *, char *);
void reiserfs_init_alloc_options(struct super_block *s);

__le32 reiserfs_choose_packing(struct inode *dir);

int reiserfs_init_bitmap_cache(struct super_block *sb);
void reiserfs_free_bitmap_cache(struct super_block *sb);
void reiserfs_cache_bitmap_metadata(struct super_block *sb, struct buffer_head *bh, struct reiserfs_bitmap_info *info);
struct buffer_head *reiserfs_read_bitmap_block(struct super_block *sb, unsigned int bitmap);
int is_reusable(struct super_block *s, b_blocknr_t block, int bit_value);
void reiserfs_free_block(struct reiserfs_transaction_handle *th, struct inode *,
			 b_blocknr_t, int for_unformatted);
int reiserfs_allocate_blocknrs(reiserfs_blocknr_hint_t *, b_blocknr_t *, int,
			       int);
static inline int reiserfs_new_form_blocknrs(struct tree_balance *tb,
					     b_blocknr_t * new_blocknrs,
					     int amount_needed)
{
	reiserfs_blocknr_hint_t hint = {
		.th = tb->transaction_handle,
		.path = tb->tb_path,
		.inode = NULL,
		.key = tb->key,
		.block = 0,
		.formatted_node = 1
	};
	return reiserfs_allocate_blocknrs(&hint, new_blocknrs, amount_needed,
					  0);
}

static inline int reiserfs_new_unf_blocknrs(struct reiserfs_transaction_handle
					    *th, struct inode *inode,
					    b_blocknr_t * new_blocknrs,
					    struct treepath *path,
					    sector_t block)
{
	reiserfs_blocknr_hint_t hint = {
		.th = th,
		.path = path,
		.inode = inode,
		.block = block,
		.formatted_node = 0,
		.preallocate = 0
	};
	return reiserfs_allocate_blocknrs(&hint, new_blocknrs, 1, 0);
}

#ifdef REISERFS_PREALLOCATE
static inline int reiserfs_new_unf_blocknrs2(struct reiserfs_transaction_handle
					     *th, struct inode *inode,
					     b_blocknr_t * new_blocknrs,
					     struct treepath *path,
					     sector_t block)
{
	reiserfs_blocknr_hint_t hint = {
		.th = th,
		.path = path,
		.inode = inode,
		.block = block,
		.formatted_node = 0,
		.preallocate = 1
	};
	return reiserfs_allocate_blocknrs(&hint, new_blocknrs, 1, 0);
}

void reiserfs_discard_prealloc(struct reiserfs_transaction_handle *th,
			       struct inode *inode);
void reiserfs_discard_all_prealloc(struct reiserfs_transaction_handle *th);
#endif

__u32 keyed_hash(const signed char *msg, int len);
__u32 yura_hash(const signed char *msg, int len);
__u32 r5_hash(const signed char *msg, int len);

#define reiserfs_set_le_bit		__set_bit_le
#define reiserfs_test_and_set_le_bit	__test_and_set_bit_le
#define reiserfs_clear_le_bit		__clear_bit_le
#define reiserfs_test_and_clear_le_bit	__test_and_clear_bit_le
#define reiserfs_test_le_bit		test_bit_le
#define reiserfs_find_next_zero_le_bit	find_next_zero_bit_le

#define SPARE_SPACE 500

long reiserfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
long reiserfs_compat_ioctl(struct file *filp,
		   unsigned int cmd, unsigned long arg);
int reiserfs_unpack(struct inode *inode, struct file *filp);
