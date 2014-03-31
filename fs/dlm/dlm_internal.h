/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2011 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __DLM_INTERNAL_DOT_H__
#define __DLM_INTERNAL_DOT_H__


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/socket.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/kref.h>
#include <linux/kernel.h>
#include <linux/jhash.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <asm/uaccess.h>

#include <linux/dlm.h>
#include "config.h"


#define DLM_INBUF_LEN		148

struct dlm_ls;
struct dlm_lkb;
struct dlm_rsb;
struct dlm_member;
struct dlm_rsbtable;
struct dlm_dirtable;
struct dlm_direntry;
struct dlm_recover;
struct dlm_header;
struct dlm_message;
struct dlm_rcom;
struct dlm_mhandle;

#define log_print(fmt, args...) \
	printk(KERN_ERR "dlm: "fmt"\n" , ##args)
#define log_error(ls, fmt, args...) \
	printk(KERN_ERR "dlm: %s: " fmt "\n", (ls)->ls_name , ##args)

#define log_debug(ls, fmt, args...) \
do { \
	if (dlm_config.ci_log_debug) \
		printk(KERN_DEBUG "dlm: %s: " fmt "\n", \
		       (ls)->ls_name , ##args); \
} while (0)

#define DLM_ASSERT(x, do) \
{ \
  if (!(x)) \
  { \
    printk(KERN_ERR "\nDLM:  Assertion failed on line %d of file %s\n" \
               "DLM:  assertion:  \"%s\"\n" \
               "DLM:  time = %lu\n", \
               __LINE__, __FILE__, #x, jiffies); \
    {do} \
    printk("\n"); \
    BUG(); \
    panic("DLM:  Record message above and reboot.\n"); \
  } \
}


struct dlm_direntry {
	struct list_head	list;
	uint32_t		master_nodeid;
	uint16_t		length;
	char			name[1];
};

struct dlm_dirtable {
	struct list_head	list;
	spinlock_t		lock;
};

struct dlm_rsbtable {
	struct rb_root		keep;
	struct rb_root		toss;
	spinlock_t		lock;
};



struct dlm_member {
	struct list_head	list;
	int			nodeid;
	int			weight;
	int			slot;
	int			slot_prev;
	int			comm_seq;
	uint32_t		generation;
};


struct dlm_recover {
	struct list_head	list;
	struct dlm_config_node	*nodes;
	int			nodes_count;
	uint64_t		seq;
};


struct dlm_args {
	uint32_t		flags;
	void			(*astfn) (void *astparam);
	void			*astparam;
	void			(*bastfn) (void *astparam, int mode);
	int			mode;
	struct dlm_lksb		*lksb;
	unsigned long		timeout;
};




#define DLM_LKSTS_WAITING	1
#define DLM_LKSTS_GRANTED	2
#define DLM_LKSTS_CONVERT	3


#define DLM_IFL_MSTCPY		0x00010000
#define DLM_IFL_RESEND		0x00020000
#define DLM_IFL_DEAD		0x00040000
#define DLM_IFL_OVERLAP_UNLOCK  0x00080000
#define DLM_IFL_OVERLAP_CANCEL  0x00100000
#define DLM_IFL_ENDOFLIFE	0x00200000
#define DLM_IFL_WATCH_TIMEWARN	0x00400000
#define DLM_IFL_TIMEOUT_CANCEL	0x00800000
#define DLM_IFL_DEADLOCK_CANCEL	0x01000000
#define DLM_IFL_STUB_MS		0x02000000 
#define DLM_IFL_USER		0x00000001
#define DLM_IFL_ORPHAN		0x00000002

#define DLM_CALLBACKS_SIZE	6

#define DLM_CB_CAST		0x00000001
#define DLM_CB_BAST		0x00000002
#define DLM_CB_SKIP		0x00000004

struct dlm_callback {
	uint64_t		seq;
	uint32_t		flags;		
	int			sb_status;	
	uint8_t			sb_flags;	
	int8_t			mode; 
};

struct dlm_lkb {
	struct dlm_rsb		*lkb_resource;	
	struct kref		lkb_ref;
	int			lkb_nodeid;	
	int			lkb_ownpid;	
	uint32_t		lkb_id;		
	uint32_t		lkb_remid;	
	uint32_t		lkb_exflags;	
	uint32_t		lkb_sbflags;	
	uint32_t		lkb_flags;	
	uint32_t		lkb_lvbseq;	

	int8_t			lkb_status;     
	int8_t			lkb_rqmode;	
	int8_t			lkb_grmode;	
	int8_t			lkb_highbast;	

	int8_t			lkb_wait_type;	
	int8_t			lkb_wait_count;
	int			lkb_wait_nodeid; 

	struct list_head	lkb_statequeue;	
	struct list_head	lkb_rsb_lookup;	
	struct list_head	lkb_wait_reply;	
	struct list_head	lkb_ownqueue;	
	struct list_head	lkb_time_list;
	ktime_t			lkb_timestamp;
	ktime_t			lkb_wait_time;
	unsigned long		lkb_timeout_cs;

	struct mutex		lkb_cb_mutex;
	struct work_struct	lkb_cb_work;
	struct list_head	lkb_cb_list; 
	struct dlm_callback	lkb_callbacks[DLM_CALLBACKS_SIZE];
	struct dlm_callback	lkb_last_cast;
	struct dlm_callback	lkb_last_bast;
	ktime_t			lkb_last_cast_time;	
	ktime_t			lkb_last_bast_time;	

	char			*lkb_lvbptr;
	struct dlm_lksb		*lkb_lksb;      
	void			(*lkb_astfn) (void *astparam);
	void			(*lkb_bastfn) (void *astparam, int mode);
	union {
		void			*lkb_astparam;	
		struct dlm_user_args	*lkb_ua;
	};
};


struct dlm_rsb {
	struct dlm_ls		*res_ls;	
	struct kref		res_ref;
	struct mutex		res_mutex;
	unsigned long		res_flags;
	int			res_length;	
	int			res_nodeid;
	uint32_t                res_lvbseq;
	uint32_t		res_hash;
	uint32_t		res_bucket;	
	unsigned long		res_toss_time;
	uint32_t		res_first_lkid;
	struct list_head	res_lookup;	
	union {
		struct list_head	res_hashchain;
		struct rb_node		res_hashnode;	
	};
	struct list_head	res_grantqueue;
	struct list_head	res_convertqueue;
	struct list_head	res_waitqueue;

	struct list_head	res_root_list;	    
	struct list_head	res_recover_list;   
	int			res_recover_locks_count;

	char			*res_lvbptr;
	char			res_name[DLM_RESNAME_MAXLEN+1];
};


#define R_MASTER		1	
#define R_CREATE		2	


enum rsb_flags {
	RSB_MASTER_UNCERTAIN,
	RSB_VALNOTVALID,
	RSB_VALNOTVALID_PREV,
	RSB_NEW_MASTER,
	RSB_NEW_MASTER2,
	RSB_RECOVER_CONVERT,
	RSB_LOCKS_PURGED,
};

static inline void rsb_set_flag(struct dlm_rsb *r, enum rsb_flags flag)
{
	__set_bit(flag, &r->res_flags);
}

static inline void rsb_clear_flag(struct dlm_rsb *r, enum rsb_flags flag)
{
	__clear_bit(flag, &r->res_flags);
}

static inline int rsb_flag(struct dlm_rsb *r, enum rsb_flags flag)
{
	return test_bit(flag, &r->res_flags);
}



#define DLM_HEADER_MAJOR	0x00030000
#define DLM_HEADER_MINOR	0x00000001

#define DLM_HEADER_SLOTS	0x00000001

#define DLM_MSG			1
#define DLM_RCOM		2

struct dlm_header {
	uint32_t		h_version;
	uint32_t		h_lockspace;
	uint32_t		h_nodeid;	
	uint16_t		h_length;
	uint8_t			h_cmd;		
	uint8_t			h_pad;
};


#define DLM_MSG_REQUEST		1
#define DLM_MSG_CONVERT		2
#define DLM_MSG_UNLOCK		3
#define DLM_MSG_CANCEL		4
#define DLM_MSG_REQUEST_REPLY	5
#define DLM_MSG_CONVERT_REPLY	6
#define DLM_MSG_UNLOCK_REPLY	7
#define DLM_MSG_CANCEL_REPLY	8
#define DLM_MSG_GRANT		9
#define DLM_MSG_BAST		10
#define DLM_MSG_LOOKUP		11
#define DLM_MSG_REMOVE		12
#define DLM_MSG_LOOKUP_REPLY	13
#define DLM_MSG_PURGE		14

struct dlm_message {
	struct dlm_header	m_header;
	uint32_t		m_type;		
	uint32_t		m_nodeid;
	uint32_t		m_pid;
	uint32_t		m_lkid;		
	uint32_t		m_remid;	
	uint32_t		m_parent_lkid;
	uint32_t		m_parent_remid;
	uint32_t		m_exflags;
	uint32_t		m_sbflags;
	uint32_t		m_flags;
	uint32_t		m_lvbseq;
	uint32_t		m_hash;
	int			m_status;
	int			m_grmode;
	int			m_rqmode;
	int			m_bastmode;
	int			m_asts;
	int			m_result;	
	char			m_extra[0];	
};


#define DLM_RS_NODES		0x00000001
#define DLM_RS_NODES_ALL	0x00000002
#define DLM_RS_DIR		0x00000004
#define DLM_RS_DIR_ALL		0x00000008
#define DLM_RS_LOCKS		0x00000010
#define DLM_RS_LOCKS_ALL	0x00000020
#define DLM_RS_DONE		0x00000040
#define DLM_RS_DONE_ALL		0x00000080

#define DLM_RCOM_STATUS		1
#define DLM_RCOM_NAMES		2
#define DLM_RCOM_LOOKUP		3
#define DLM_RCOM_LOCK		4
#define DLM_RCOM_STATUS_REPLY	5
#define DLM_RCOM_NAMES_REPLY	6
#define DLM_RCOM_LOOKUP_REPLY	7
#define DLM_RCOM_LOCK_REPLY	8

struct dlm_rcom {
	struct dlm_header	rc_header;
	uint32_t		rc_type;	
	int			rc_result;	
	uint64_t		rc_id;		
	uint64_t		rc_seq;		
	uint64_t		rc_seq_reply;	
	char			rc_buf[0];
};

union dlm_packet {
	struct dlm_header	header;		
	struct dlm_message	message;
	struct dlm_rcom		rcom;
};

#define DLM_RSF_NEED_SLOTS	0x00000001

struct rcom_status {
	__le32			rs_flags;
	__le32			rs_unused1;
	__le64			rs_unused2;
};

struct rcom_config {
	__le32			rf_lvblen;
	__le32			rf_lsflags;

	
	__le32			rf_flags;
	__le16			rf_our_slot;
	__le16			rf_num_slots;
	__le32			rf_generation;
	__le32			rf_unused1;
	__le64			rf_unused2;
};

struct rcom_slot {
	__le32			ro_nodeid;
	__le16			ro_slot;
	__le16			ro_unused1;
	__le64			ro_unused2;
};

struct rcom_lock {
	__le32			rl_ownpid;
	__le32			rl_lkid;
	__le32			rl_remid;
	__le32			rl_parent_lkid;
	__le32			rl_parent_remid;
	__le32			rl_exflags;
	__le32			rl_flags;
	__le32			rl_lvbseq;
	__le32			rl_result;
	int8_t			rl_rqmode;
	int8_t			rl_grmode;
	int8_t			rl_status;
	int8_t			rl_asts;
	__le16			rl_wait_type;
	__le16			rl_namelen;
	char			rl_name[DLM_RESNAME_MAXLEN];
	char			rl_lvb[0];
};

struct dlm_ls {
	struct list_head	ls_list;	
	dlm_lockspace_t		*ls_local_handle;
	uint32_t		ls_global_id;	
	uint32_t		ls_generation;
	uint32_t		ls_exflags;
	int			ls_lvblen;
	int			ls_count;	
	int			ls_create_count; 
	unsigned long		ls_flags;	
	unsigned long		ls_scan_time;
	struct kobject		ls_kobj;

	struct idr		ls_lkbidr;
	spinlock_t		ls_lkbidr_spin;

	struct dlm_rsbtable	*ls_rsbtbl;
	uint32_t		ls_rsbtbl_size;

	struct dlm_dirtable	*ls_dirtbl;
	uint32_t		ls_dirtbl_size;

	struct mutex		ls_waiters_mutex;
	struct list_head	ls_waiters;	

	struct mutex		ls_orphans_mutex;
	struct list_head	ls_orphans;

	struct mutex		ls_timeout_mutex;
	struct list_head	ls_timeout;

	spinlock_t		ls_new_rsb_spin;
	int			ls_new_rsb_count;
	struct list_head	ls_new_rsb;	

	struct list_head	ls_nodes;	
	struct list_head	ls_nodes_gone;	
	int			ls_num_nodes;	
	int			ls_low_nodeid;
	int			ls_total_weight;
	int			*ls_node_array;

	int			ls_slot;
	int			ls_num_slots;
	int			ls_slots_size;
	struct dlm_slot		*ls_slots;

	struct dlm_rsb		ls_stub_rsb;	
	struct dlm_lkb		ls_stub_lkb;	
	struct dlm_message	ls_stub_ms;	

	struct dentry		*ls_debug_rsb_dentry; 
	struct dentry		*ls_debug_waiters_dentry; 
	struct dentry		*ls_debug_locks_dentry; 
	struct dentry		*ls_debug_all_dentry; 

	wait_queue_head_t	ls_uevent_wait;	
	int			ls_uevent_result;
	struct completion	ls_members_done;
	int			ls_members_result;

	struct miscdevice       ls_device;

	struct workqueue_struct	*ls_callback_wq;

	

	struct mutex		ls_cb_mutex;
	struct list_head	ls_cb_delay; 
	struct timer_list	ls_timer;
	struct task_struct	*ls_recoverd_task;
	struct mutex		ls_recoverd_active;
	spinlock_t		ls_recover_lock;
	unsigned long		ls_recover_begin; 
	uint32_t		ls_recover_status; 
	uint64_t		ls_recover_seq;
	struct dlm_recover	*ls_recover_args;
	struct rw_semaphore	ls_in_recovery;	
	struct rw_semaphore	ls_recv_active;	
	struct list_head	ls_requestqueue;
	struct mutex		ls_requestqueue_mutex;
	struct dlm_rcom		*ls_recover_buf;
	int			ls_recover_nodeid; 
	uint64_t		ls_rcom_seq;
	spinlock_t		ls_rcom_spin;
	struct list_head	ls_recover_list;
	spinlock_t		ls_recover_list_lock;
	int			ls_recover_list_count;
	wait_queue_head_t	ls_wait_general;
	struct mutex		ls_clear_proc_locks;

	struct list_head	ls_root_list;	
	struct rw_semaphore	ls_root_sem;	

	const struct dlm_lockspace_ops *ls_ops;
	void			*ls_ops_arg;

	int			ls_namelen;
	char			ls_name[1];
};

#define LSFL_WORK		0
#define LSFL_RUNNING		1
#define LSFL_RECOVERY_STOP	2
#define LSFL_RCOM_READY		3
#define LSFL_RCOM_WAIT		4
#define LSFL_UEVENT_WAIT	5
#define LSFL_TIMEWARN		6
#define LSFL_CB_DELAY		7


struct dlm_user_args {
	struct dlm_user_proc	*proc; 
	struct dlm_lksb		lksb;
	struct dlm_lksb __user	*user_lksb;
	void __user		*castparam;
	void __user		*castaddr;
	void __user		*bastparam;
	void __user		*bastaddr;
	uint64_t		xid;
};

#define DLM_PROC_FLAGS_CLOSING 1
#define DLM_PROC_FLAGS_COMPAT  2


struct dlm_user_proc {
	dlm_lockspace_t		*lockspace;
	unsigned long		flags; 
	struct list_head	asts;
	spinlock_t		asts_spin;
	struct list_head	locks;
	spinlock_t		locks_spin;
	struct list_head	unlocking;
	wait_queue_head_t	wait;
};

static inline int dlm_locking_stopped(struct dlm_ls *ls)
{
	return !test_bit(LSFL_RUNNING, &ls->ls_flags);
}

static inline int dlm_recovery_stopped(struct dlm_ls *ls)
{
	return test_bit(LSFL_RECOVERY_STOP, &ls->ls_flags);
}

static inline int dlm_no_directory(struct dlm_ls *ls)
{
	return (ls->ls_exflags & DLM_LSFL_NODIR) ? 1 : 0;
}

int dlm_netlink_init(void);
void dlm_netlink_exit(void);
void dlm_timeout_warn(struct dlm_lkb *lkb);
int dlm_plock_init(void);
void dlm_plock_exit(void);

#ifdef CONFIG_DLM_DEBUG
int dlm_register_debugfs(void);
void dlm_unregister_debugfs(void);
int dlm_create_debug_file(struct dlm_ls *ls);
void dlm_delete_debug_file(struct dlm_ls *ls);
#else
static inline int dlm_register_debugfs(void) { return 0; }
static inline void dlm_unregister_debugfs(void) { }
static inline int dlm_create_debug_file(struct dlm_ls *ls) { return 0; }
static inline void dlm_delete_debug_file(struct dlm_ls *ls) { }
#endif

#endif				

