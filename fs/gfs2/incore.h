/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2008 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#ifndef __INCORE_DOT_H__
#define __INCORE_DOT_H__

#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/workqueue.h>
#include <linux/dlm.h>
#include <linux/buffer_head.h>
#include <linux/rcupdate.h>
#include <linux/rculist_bl.h>
#include <linux/completion.h>
#include <linux/rbtree.h>
#include <linux/ktime.h>
#include <linux/percpu.h>

#define DIO_WAIT	0x00000010
#define DIO_METADATA	0x00000020

struct gfs2_log_operations;
struct gfs2_log_element;
struct gfs2_holder;
struct gfs2_glock;
struct gfs2_quota_data;
struct gfs2_trans;
struct gfs2_ail;
struct gfs2_jdesc;
struct gfs2_sbd;
struct lm_lockops;

typedef void (*gfs2_glop_bh_t) (struct gfs2_glock *gl, unsigned int ret);

struct gfs2_log_header_host {
	u64 lh_sequence;	
	u32 lh_flags;		
	u32 lh_tail;		
	u32 lh_blkno;
	u32 lh_hash;
};


struct gfs2_log_operations {
	void (*lo_add) (struct gfs2_sbd *sdp, struct gfs2_log_element *le);
	void (*lo_before_commit) (struct gfs2_sbd *sdp);
	void (*lo_after_commit) (struct gfs2_sbd *sdp, struct gfs2_ail *ai);
	void (*lo_before_scan) (struct gfs2_jdesc *jd,
				struct gfs2_log_header_host *head, int pass);
	int (*lo_scan_elements) (struct gfs2_jdesc *jd, unsigned int start,
				 struct gfs2_log_descriptor *ld, __be64 *ptr,
				 int pass);
	void (*lo_after_scan) (struct gfs2_jdesc *jd, int error, int pass);
	const char *lo_name;
};

struct gfs2_log_element {
	struct list_head le_list;
	const struct gfs2_log_operations *le_ops;
};

#define GBF_FULL 1

struct gfs2_bitmap {
	struct buffer_head *bi_bh;
	char *bi_clone;
	unsigned long bi_flags;
	u32 bi_offset;
	u32 bi_start;
	u32 bi_len;
};

struct gfs2_rgrpd {
	struct rb_node rd_node;		
	struct gfs2_glock *rd_gl;	
	u64 rd_addr;			
	u64 rd_data0;			
	u32 rd_length;			
	u32 rd_data;			
	u32 rd_bitbytes;		
	u32 rd_free;
	u32 rd_free_clone;
	u32 rd_dinodes;
	u64 rd_igeneration;
	struct gfs2_bitmap *rd_bits;
	struct gfs2_sbd *rd_sbd;
	u32 rd_last_alloc;
	u32 rd_flags;
#define GFS2_RDF_CHECK		0x10000000 
#define GFS2_RDF_UPTODATE	0x20000000 
#define GFS2_RDF_ERROR		0x40000000 
#define GFS2_RDF_MASK		0xf0000000 
};

enum gfs2_state_bits {
	BH_Pinned = BH_PrivateStart,
	BH_Escaped = BH_PrivateStart + 1,
	BH_Zeronew = BH_PrivateStart + 2,
};

BUFFER_FNS(Pinned, pinned)
TAS_BUFFER_FNS(Pinned, pinned)
BUFFER_FNS(Escaped, escaped)
TAS_BUFFER_FNS(Escaped, escaped)
BUFFER_FNS(Zeronew, zeronew)
TAS_BUFFER_FNS(Zeronew, zeronew)

struct gfs2_bufdata {
	struct buffer_head *bd_bh;
	struct gfs2_glock *bd_gl;

	union {
		struct list_head list_tr;
		u64 blkno;
	} u;
#define bd_list_tr u.list_tr
#define bd_blkno u.blkno

	struct gfs2_log_element bd_le;

	struct gfs2_ail *bd_ail;
	struct list_head bd_ail_st_list;
	struct list_head bd_ail_gl_list;
};


#define GDLM_STRNAME_BYTES	25
#define GDLM_LVB_SIZE		32


enum {
	DFL_BLOCK_LOCKS		= 0,
	DFL_NO_DLM_OPS		= 1,
	DFL_FIRST_MOUNT		= 2,
	DFL_FIRST_MOUNT_DONE	= 3,
	DFL_MOUNT_DONE		= 4,
	DFL_UNMOUNT		= 5,
	DFL_DLM_RECOVERY	= 6,
};

struct lm_lockname {
	u64 ln_number;
	unsigned int ln_type;
};

#define lm_name_equal(name1, name2) \
        (((name1)->ln_number == (name2)->ln_number) && \
         ((name1)->ln_type == (name2)->ln_type))


struct gfs2_glock_operations {
	void (*go_xmote_th) (struct gfs2_glock *gl);
	int (*go_xmote_bh) (struct gfs2_glock *gl, struct gfs2_holder *gh);
	void (*go_inval) (struct gfs2_glock *gl, int flags);
	int (*go_demote_ok) (const struct gfs2_glock *gl);
	int (*go_lock) (struct gfs2_holder *gh);
	void (*go_unlock) (struct gfs2_holder *gh);
	int (*go_dump)(struct seq_file *seq, const struct gfs2_glock *gl);
	void (*go_callback) (struct gfs2_glock *gl);
	const int go_type;
	const unsigned long go_flags;
#define GLOF_ASPACE 1
};

enum {
	GFS2_LKS_SRTT = 0,	
	GFS2_LKS_SRTTVAR = 1,	
	GFS2_LKS_SRTTB = 2,	
	GFS2_LKS_SRTTVARB = 3,	
	GFS2_LKS_SIRT = 4,	
	GFS2_LKS_SIRTVAR = 5,	
	GFS2_LKS_DCOUNT = 6,	
	GFS2_LKS_QCOUNT = 7,	
	GFS2_NR_LKSTATS
};

struct gfs2_lkstats {
	s64 stats[GFS2_NR_LKSTATS];
};

enum {
	
	HIF_HOLDER		= 6,  
	HIF_FIRST		= 7,
	HIF_WAIT		= 10,
};

struct gfs2_holder {
	struct list_head gh_list;

	struct gfs2_glock *gh_gl;
	struct pid *gh_owner_pid;
	unsigned int gh_state;
	unsigned gh_flags;

	int gh_error;
	unsigned long gh_iflags; 
	unsigned long gh_ip;
};

enum {
	GLF_LOCK			= 1,
	GLF_DEMOTE			= 3,
	GLF_PENDING_DEMOTE		= 4,
	GLF_DEMOTE_IN_PROGRESS		= 5,
	GLF_DIRTY			= 6,
	GLF_LFLUSH			= 7,
	GLF_INVALIDATE_IN_PROGRESS	= 8,
	GLF_REPLY_PENDING		= 9,
	GLF_INITIAL			= 10,
	GLF_FROZEN			= 11,
	GLF_QUEUED			= 12,
	GLF_LRU				= 13,
	GLF_OBJECT			= 14, 
	GLF_BLOCKING			= 15,
};

struct gfs2_glock {
	struct hlist_bl_node gl_list;
	struct gfs2_sbd *gl_sbd;
	unsigned long gl_flags;		
	struct lm_lockname gl_name;
	atomic_t gl_ref;

	spinlock_t gl_spin;

	
	unsigned int gl_state:2,	
		     gl_target:2,	
		     gl_demote_state:2,	
		     gl_req:2,		
		     gl_reply:8;	

	unsigned int gl_hash;
	unsigned long gl_demote_time; 
	long gl_hold_time;
	struct list_head gl_holders;

	const struct gfs2_glock_operations *gl_ops;
	ktime_t gl_dstamp;
	struct gfs2_lkstats gl_stats;
	struct dlm_lksb gl_lksb;
	char gl_lvb[32];
	unsigned long gl_tchange;
	void *gl_object;

	struct list_head gl_lru;
	struct list_head gl_ail_list;
	atomic_t gl_ail_count;
	atomic_t gl_revokes;
	struct delayed_work gl_work;
	struct work_struct gl_delete;
	struct rcu_head gl_rcu;
};

#define GFS2_MIN_LVB_SIZE 32	

struct gfs2_qadata { 
	
	struct gfs2_quota_data *qa_qd[2*MAXQUOTAS];
	struct gfs2_holder qa_qd_ghs[2*MAXQUOTAS];
	unsigned int qa_qd_num;
};

struct gfs2_blkreserv {
	u32 rs_requested; 
	struct gfs2_holder rs_rgd_gh; 
};

enum {
	GIF_INVALID		= 0,
	GIF_QD_LOCKED		= 1,
	GIF_ALLOC_FAILED	= 2,
	GIF_SW_PAGED		= 3,
};


struct gfs2_inode {
	struct inode i_inode;
	u64 i_no_addr;
	u64 i_no_formal_ino;
	u64 i_generation;
	u64 i_eattr;
	unsigned long i_flags;		
	struct gfs2_glock *i_gl; 
	struct gfs2_holder i_iopen_gh;
	struct gfs2_holder i_gh; 
	struct gfs2_qadata *i_qadata; 
	struct gfs2_blkreserv *i_res; 
	struct gfs2_rgrpd *i_rgd;
	u64 i_goal;	
	struct rw_semaphore i_rw_mutex;
	struct list_head i_trunc_list;
	__be64 *i_hash_cache;
	u32 i_entries;
	u32 i_diskflags;
	u8 i_height;
	u8 i_depth;
};

static inline struct gfs2_inode *GFS2_I(struct inode *inode)
{
	return container_of(inode, struct gfs2_inode, i_inode);
}

static inline struct gfs2_sbd *GFS2_SB(const struct inode *inode)
{
	return inode->i_sb->s_fs_info;
}

struct gfs2_file {
	struct mutex f_fl_mutex;
	struct gfs2_holder f_fl_gh;
};

struct gfs2_revoke_replay {
	struct list_head rr_list;
	u64 rr_blkno;
	unsigned int rr_where;
};

enum {
	QDF_USER		= 0,
	QDF_CHANGE		= 1,
	QDF_LOCKED		= 2,
	QDF_REFRESH		= 3,
};

struct gfs2_quota_data {
	struct list_head qd_list;
	struct list_head qd_reclaim;

	atomic_t qd_count;

	u32 qd_id;
	unsigned long qd_flags;		

	s64 qd_change;
	s64 qd_change_sync;

	unsigned int qd_slot;
	unsigned int qd_slot_count;

	struct buffer_head *qd_bh;
	struct gfs2_quota_change *qd_bh_qc;
	unsigned int qd_bh_count;

	struct gfs2_glock *qd_gl;
	struct gfs2_quota_lvb qd_qb;

	u64 qd_sync_gen;
	unsigned long qd_last_warn;
};

struct gfs2_trans {
	unsigned long tr_ip;

	unsigned int tr_blocks;
	unsigned int tr_revokes;
	unsigned int tr_reserved;

	struct gfs2_holder tr_t_gh;

	int tr_touched;

	unsigned int tr_num_buf;
	unsigned int tr_num_buf_new;
	unsigned int tr_num_databuf_new;
	unsigned int tr_num_buf_rm;
	unsigned int tr_num_databuf_rm;
	struct list_head tr_list_buf;

	unsigned int tr_num_revoke;
	unsigned int tr_num_revoke_rm;
};

struct gfs2_ail {
	struct list_head ai_list;

	unsigned int ai_first;
	struct list_head ai_ail1_list;
	struct list_head ai_ail2_list;
};

struct gfs2_journal_extent {
	struct list_head extent_list;

	unsigned int lblock; 
	u64 dblock; 
	u64 blocks;
};

struct gfs2_jdesc {
	struct list_head jd_list;
	struct list_head extent_list;
	struct work_struct jd_work;
	struct inode *jd_inode;
	unsigned long jd_flags;
#define JDF_RECOVERY 1
	unsigned int jd_jid;
	unsigned int jd_blocks;
	int jd_recover_error;
};

struct gfs2_statfs_change_host {
	s64 sc_total;
	s64 sc_free;
	s64 sc_dinodes;
};

#define GFS2_QUOTA_DEFAULT	GFS2_QUOTA_OFF
#define GFS2_QUOTA_OFF		0
#define GFS2_QUOTA_ACCOUNT	1
#define GFS2_QUOTA_ON		2

#define GFS2_DATA_DEFAULT	GFS2_DATA_ORDERED
#define GFS2_DATA_WRITEBACK	1
#define GFS2_DATA_ORDERED	2

#define GFS2_ERRORS_DEFAULT     GFS2_ERRORS_WITHDRAW
#define GFS2_ERRORS_WITHDRAW    0
#define GFS2_ERRORS_CONTINUE    1 
#define GFS2_ERRORS_RO          2 
#define GFS2_ERRORS_PANIC       3

struct gfs2_args {
	char ar_lockproto[GFS2_LOCKNAME_LEN];	
	char ar_locktable[GFS2_LOCKNAME_LEN];	
	char ar_hostdata[GFS2_LOCKNAME_LEN];	
	unsigned int ar_spectator:1;		
	unsigned int ar_localflocks:1;		
	unsigned int ar_debug:1;		
	unsigned int ar_posix_acl:1;		
	unsigned int ar_quota:2;		
	unsigned int ar_suiddir:1;		
	unsigned int ar_data:2;			
	unsigned int ar_meta:1;			
	unsigned int ar_discard:1;		
	unsigned int ar_errors:2;               
	unsigned int ar_nobarrier:1;            
	int ar_commit;				
	int ar_statfs_quantum;			
	int ar_quota_quantum;			
	int ar_statfs_percent;			
};

struct gfs2_tune {
	spinlock_t gt_spin;

	unsigned int gt_logd_secs;

	unsigned int gt_quota_simul_sync; 
	unsigned int gt_quota_warn_period; 
	unsigned int gt_quota_scale_num; 
	unsigned int gt_quota_scale_den; 
	unsigned int gt_quota_quantum; 
	unsigned int gt_new_files_jdata;
	unsigned int gt_max_readahead; 
	unsigned int gt_complain_secs;
	unsigned int gt_statfs_quantum;
	unsigned int gt_statfs_slow;
};

enum {
	SDF_JOURNAL_CHECKED	= 0,
	SDF_JOURNAL_LIVE	= 1,
	SDF_SHUTDOWN		= 2,
	SDF_NOBARRIERS		= 3,
	SDF_NORECOVERY		= 4,
	SDF_DEMOTE		= 5,
	SDF_NOJOURNALID		= 6,
	SDF_RORECOVERY		= 7, 
};

#define GFS2_FSNAME_LEN		256

struct gfs2_inum_host {
	u64 no_formal_ino;
	u64 no_addr;
};

struct gfs2_sb_host {
	u32 sb_magic;
	u32 sb_type;
	u32 sb_format;

	u32 sb_fs_format;
	u32 sb_multihost_format;
	u32 sb_bsize;
	u32 sb_bsize_shift;

	struct gfs2_inum_host sb_master_dir;
	struct gfs2_inum_host sb_root_dir;

	char sb_lockproto[GFS2_LOCKNAME_LEN];
	char sb_locktable[GFS2_LOCKNAME_LEN];
};


struct lm_lockstruct {
	int ls_jid;
	unsigned int ls_first;
	unsigned int ls_nodir;
	const struct lm_lockops *ls_ops;
	dlm_lockspace_t *ls_dlm;

	int ls_recover_jid_done;   
	int ls_recover_jid_status; 

	struct dlm_lksb ls_mounted_lksb; 
	struct dlm_lksb ls_control_lksb; 
	char ls_control_lvb[GDLM_LVB_SIZE]; 
	struct completion ls_sync_wait; 

	spinlock_t ls_recover_spin; 
	unsigned long ls_recover_flags; 
	uint32_t ls_recover_mount; 
	uint32_t ls_recover_start; 
	uint32_t ls_recover_block; 
	uint32_t ls_recover_size; 
	uint32_t *ls_recover_submit; 
	uint32_t *ls_recover_result; 
};

struct gfs2_pcpu_lkstats {
	
	struct gfs2_lkstats lkstats[10];
};

struct gfs2_sbd {
	struct super_block *sd_vfs;
	struct gfs2_pcpu_lkstats __percpu *sd_lkstats;
	struct kobject sd_kobj;
	unsigned long sd_flags;	
	struct gfs2_sb_host sd_sb;

	

	u32 sd_fsb2bb;
	u32 sd_fsb2bb_shift;
	u32 sd_diptrs;	
	u32 sd_inptrs;	
	u32 sd_jbsize;	
	u32 sd_hash_bsize;	
	u32 sd_hash_bsize_shift;
	u32 sd_hash_ptrs;	
	u32 sd_qc_per_block;
	u32 sd_max_dirres;	
	u32 sd_max_height;	
	u64 sd_heightsize[GFS2_MAX_META_HEIGHT + 1];
	u32 sd_max_jheight; 
	u64 sd_jheightsize[GFS2_MAX_META_HEIGHT + 1];

	struct gfs2_args sd_args;	
	struct gfs2_tune sd_tune;	

	

	struct lm_lockstruct sd_lockstruct;
	struct gfs2_holder sd_live_gh;
	struct gfs2_glock *sd_rename_gl;
	struct gfs2_glock *sd_trans_gl;
	wait_queue_head_t sd_glock_wait;
	atomic_t sd_glock_disposal;
	struct completion sd_locking_init;
	struct delayed_work sd_control_work;

	

	struct dentry *sd_master_dir;
	struct dentry *sd_root_dir;

	struct inode *sd_jindex;
	struct inode *sd_statfs_inode;
	struct inode *sd_sc_inode;
	struct inode *sd_qc_inode;
	struct inode *sd_rindex;
	struct inode *sd_quota_inode;

	

	spinlock_t sd_statfs_spin;
	struct gfs2_statfs_change_host sd_statfs_master;
	struct gfs2_statfs_change_host sd_statfs_local;
	int sd_statfs_force_sync;

	

	int sd_rindex_uptodate;
	spinlock_t sd_rindex_spin;
	struct rb_root sd_rindex_tree;
	unsigned int sd_rgrps;
	unsigned int sd_max_rg_data;

	

	struct list_head sd_jindex_list;
	spinlock_t sd_jindex_spin;
	struct mutex sd_jindex_mutex;
	unsigned int sd_journals;

	struct gfs2_jdesc *sd_jdesc;
	struct gfs2_holder sd_journal_gh;
	struct gfs2_holder sd_jinode_gh;

	struct gfs2_holder sd_sc_gh;
	struct gfs2_holder sd_qc_gh;

	

	struct task_struct *sd_logd_process;
	struct task_struct *sd_quotad_process;

	

	struct list_head sd_quota_list;
	atomic_t sd_quota_count;
	struct mutex sd_quota_mutex;
	wait_queue_head_t sd_quota_wait;
	struct list_head sd_trunc_list;
	spinlock_t sd_trunc_lock;

	unsigned int sd_quota_slots;
	unsigned int sd_quota_chunks;
	unsigned char **sd_quota_bitmap;

	u64 sd_quota_sync_gen;

	

	spinlock_t sd_log_lock;

	unsigned int sd_log_blks_reserved;
	unsigned int sd_log_commited_buf;
	unsigned int sd_log_commited_databuf;
	int sd_log_commited_revoke;

	atomic_t sd_log_pinned;
	unsigned int sd_log_num_buf;
	unsigned int sd_log_num_revoke;
	unsigned int sd_log_num_rg;
	unsigned int sd_log_num_databuf;

	struct list_head sd_log_le_buf;
	struct list_head sd_log_le_revoke;
	struct list_head sd_log_le_rg;
	struct list_head sd_log_le_databuf;
	struct list_head sd_log_le_ordered;

	atomic_t sd_log_thresh1;
	atomic_t sd_log_thresh2;
	atomic_t sd_log_blks_free;
	wait_queue_head_t sd_log_waitq;
	wait_queue_head_t sd_logd_waitq;

	u64 sd_log_sequence;
	unsigned int sd_log_head;
	unsigned int sd_log_tail;
	int sd_log_idle;

	struct rw_semaphore sd_log_flush_lock;
	atomic_t sd_log_in_flight;
	wait_queue_head_t sd_log_flush_wait;

	unsigned int sd_log_flush_head;
	u64 sd_log_flush_wrapped;

	spinlock_t sd_ail_lock;
	struct list_head sd_ail1_list;
	struct list_head sd_ail2_list;

	

	struct list_head sd_revoke_list;
	unsigned int sd_replay_tail;

	unsigned int sd_found_blocks;
	unsigned int sd_found_revokes;
	unsigned int sd_replayed_blocks;

	

	struct gfs2_holder sd_freeze_gh;
	struct mutex sd_freeze_lock;
	unsigned int sd_freeze_count;

	char sd_fsname[GFS2_FSNAME_LEN];
	char sd_table_name[GFS2_FSNAME_LEN];
	char sd_proto_name[GFS2_FSNAME_LEN];

	

	unsigned long sd_last_warning;
	struct dentry *debugfs_dir;    
	struct dentry *debugfs_dentry_glocks;
	struct dentry *debugfs_dentry_glstats;
	struct dentry *debugfs_dentry_sbstats;
};

static inline void gfs2_glstats_inc(struct gfs2_glock *gl, int which)
{
	gl->gl_stats.stats[which]++;
}

static inline void gfs2_sbstats_inc(const struct gfs2_glock *gl, int which)
{
	const struct gfs2_sbd *sdp = gl->gl_sbd;
	preempt_disable();
	this_cpu_ptr(sdp->sd_lkstats)->lkstats[gl->gl_name.ln_type].stats[which]++;
	preempt_enable();
}

#endif 

