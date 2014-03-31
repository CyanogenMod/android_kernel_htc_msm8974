/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2009 Silicon Graphics, Inc.  All Rights Reserved.
 */


#ifndef _DRIVERS_MISC_SGIXP_XPC_H
#define _DRIVERS_MISC_SGIXP_XPC_H

#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include "xp.h"

#define _XPC_VERSION(_maj, _min)	(((_maj) << 4) | ((_min) & 0xf))
#define XPC_VERSION_MAJOR(_v)		((_v) >> 4)
#define XPC_VERSION_MINOR(_v)		((_v) & 0xf)

#define XPC_HB_DEFAULT_INTERVAL		5	
#define XPC_HB_CHECK_DEFAULT_INTERVAL	20	

#define XPC_HB_CHECK_THREAD_NAME	"xpc_hb"
#define XPC_HB_CHECK_CPU		0

#define XPC_DISCOVERY_THREAD_NAME	"xpc_discovery"

struct xpc_rsvd_page {
	u64 SAL_signature;	
	u64 SAL_version;	
	short SAL_partid;	
	short max_npartitions;	
	u8 version;
	u8 pad1[3];		
	unsigned long ts_jiffies; 
	union {
		struct {
			unsigned long vars_pa;	
		} sn2;
		struct {
			unsigned long heartbeat_gpa; 
			unsigned long activate_gru_mq_desc_gpa; 
		} uv;
	} sn;
	u64 pad2[9];		
	u64 SAL_nasids_size;	
};

#define XPC_RP_VERSION _XPC_VERSION(3, 0) 


struct xpc_vars_sn2 {
	u8 version;
	u64 heartbeat;
	DECLARE_BITMAP(heartbeating_to_mask, XP_MAX_NPARTITIONS_SN2);
	u64 heartbeat_offline;	
	int activate_IRQ_nasid;
	int activate_IRQ_phys_cpuid;
	unsigned long vars_part_pa;
	unsigned long amos_page_pa;
	struct amo *amos_page;	
};

#define XPC_V_VERSION _XPC_VERSION(3, 1)    

struct xpc_vars_part_sn2 {
	u64 magic;

	unsigned long openclose_args_pa; 
	unsigned long GPs_pa;	

	unsigned long chctl_amo_pa; 

	int notify_IRQ_nasid;	
	int notify_IRQ_phys_cpuid;	

	u8 nchannels;		

	u8 reserved[23];	
};

#define XPC_VP_MAGIC1_SN2 0x0053524156435058L 
#define XPC_VP_MAGIC2_SN2 0x0073726176435058L 


#define XPC_RP_HEADER_SIZE	L1_CACHE_ALIGN(sizeof(struct xpc_rsvd_page))
#define XPC_RP_VARS_SIZE	L1_CACHE_ALIGN(sizeof(struct xpc_vars_sn2))

#define XPC_RP_PART_NASIDS(_rp) ((unsigned long *)((u8 *)(_rp) + \
				 XPC_RP_HEADER_SIZE))
#define XPC_RP_MACH_NASIDS(_rp) (XPC_RP_PART_NASIDS(_rp) + \
				 xpc_nasid_mask_nlongs)
#define XPC_RP_VARS(_rp)	((struct xpc_vars_sn2 *) \
				 (XPC_RP_MACH_NASIDS(_rp) + \
				  xpc_nasid_mask_nlongs))


struct xpc_heartbeat_uv {
	unsigned long value;
	unsigned long offline;	
};

struct xpc_gru_mq_uv {
	void *address;		
	unsigned int order;	
	int irq;		
	int mmr_blade;		
	unsigned long mmr_offset; 
	unsigned long mmr_value; 
	int watchlist_num;	
	void *gru_mq_desc;	
};

struct xpc_activate_mq_msghdr_uv {
	unsigned int gru_msg_hdr; 
	short partid;		
	u8 act_state;		
	u8 type;		
	unsigned long rp_ts_jiffies; 
};

#define XPC_ACTIVATE_MQ_MSG_SYNC_ACT_STATE_UV		0

#define XPC_ACTIVATE_MQ_MSG_ACTIVATE_REQ_UV		1
#define XPC_ACTIVATE_MQ_MSG_DEACTIVATE_REQ_UV		2

#define XPC_ACTIVATE_MQ_MSG_CHCTL_CLOSEREQUEST_UV	3
#define XPC_ACTIVATE_MQ_MSG_CHCTL_CLOSEREPLY_UV		4
#define XPC_ACTIVATE_MQ_MSG_CHCTL_OPENREQUEST_UV	5
#define XPC_ACTIVATE_MQ_MSG_CHCTL_OPENREPLY_UV		6
#define XPC_ACTIVATE_MQ_MSG_CHCTL_OPENCOMPLETE_UV	7

#define XPC_ACTIVATE_MQ_MSG_MARK_ENGAGED_UV		8
#define XPC_ACTIVATE_MQ_MSG_MARK_DISENGAGED_UV		9

struct xpc_activate_mq_msg_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
};

struct xpc_activate_mq_msg_activate_req_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	unsigned long rp_gpa;
	unsigned long heartbeat_gpa;
	unsigned long activate_gru_mq_desc_gpa;
};

struct xpc_activate_mq_msg_deactivate_req_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	enum xp_retval reason;
};

struct xpc_activate_mq_msg_chctl_closerequest_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	short ch_number;
	enum xp_retval reason;
};

struct xpc_activate_mq_msg_chctl_closereply_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	short ch_number;
};

struct xpc_activate_mq_msg_chctl_openrequest_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	short ch_number;
	short entry_size;	
	short local_nentries;	
};

struct xpc_activate_mq_msg_chctl_openreply_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	short ch_number;
	short remote_nentries;	
	short local_nentries;	
	unsigned long notify_gru_mq_desc_gpa;
};

struct xpc_activate_mq_msg_chctl_opencomplete_uv {
	struct xpc_activate_mq_msghdr_uv hdr;
	short ch_number;
};

#define XPC_PACK_ARGS(_arg1, _arg2) \
			((((u64)_arg1) & 0xffffffff) | \
			((((u64)_arg2) & 0xffffffff) << 32))

#define XPC_UNPACK_ARG1(_args)	(((u64)_args) & 0xffffffff)
#define XPC_UNPACK_ARG2(_args)	((((u64)_args) >> 32) & 0xffffffff)

struct xpc_gp_sn2 {
	s64 get;		
	s64 put;		
};

#define XPC_GP_SIZE \
		L1_CACHE_ALIGN(sizeof(struct xpc_gp_sn2) * XPC_MAX_NCHANNELS)

struct xpc_openclose_args {
	u16 reason;		
	u16 entry_size;		
	u16 remote_nentries;	
	u16 local_nentries;	
	unsigned long local_msgqueue_pa; 
};

#define XPC_OPENCLOSE_ARGS_SIZE \
	      L1_CACHE_ALIGN(sizeof(struct xpc_openclose_args) * \
	      XPC_MAX_NCHANNELS)



struct xpc_fifo_entry_uv {
	struct xpc_fifo_entry_uv *next;
};

struct xpc_fifo_head_uv {
	struct xpc_fifo_entry_uv *first;
	struct xpc_fifo_entry_uv *last;
	spinlock_t lock;
	int n_entries;
};

struct xpc_msg_sn2 {
	u8 flags;		
	u8 reserved[7];		
	s64 number;		

	u64 payload;		
};


#define	XPC_M_SN2_DONE		0x01	
#define	XPC_M_SN2_READY		0x02	
#define	XPC_M_SN2_INTERRUPT	0x04	


struct xpc_notify_mq_msghdr_uv {
	union {
		unsigned int gru_msg_hdr;	
		struct xpc_fifo_entry_uv next;	
	} u;
	short partid;		
	u8 ch_number;		
	u8 size;		
	unsigned int msg_slot_number;	
};

struct xpc_notify_mq_msg_uv {
	struct xpc_notify_mq_msghdr_uv hdr;
	unsigned long payload;
};

struct xpc_notify_sn2 {
	u8 type;		

	
	xpc_notify_func func;	
	void *key;		
};


#define	XPC_N_CALL	0x01	

struct xpc_send_msg_slot_uv {
	struct xpc_fifo_entry_uv next;
	unsigned int msg_slot_number;
	xpc_notify_func func;	
	void *key;		
};



struct xpc_channel_sn2 {
	struct xpc_openclose_args *local_openclose_args; 
					     

	void *local_msgqueue_base;	
	struct xpc_msg_sn2 *local_msgqueue;	
	void *remote_msgqueue_base;	
	struct xpc_msg_sn2 *remote_msgqueue; 
					   
	unsigned long remote_msgqueue_pa; 
					  

	struct xpc_notify_sn2 *notify_queue;

	

	struct xpc_gp_sn2 *local_GP;	
	struct xpc_gp_sn2 remote_GP;	
	struct xpc_gp_sn2 w_local_GP;	
	struct xpc_gp_sn2 w_remote_GP;	
	s64 next_msg_to_pull;	

	struct mutex msg_to_pull_mutex;	
};

struct xpc_channel_uv {
	void *cached_notify_gru_mq_desc; 
					 

	struct xpc_send_msg_slot_uv *send_msg_slots;
	void *recv_msg_slots;	
				

	struct xpc_fifo_head_uv msg_slot_free_list;
	struct xpc_fifo_head_uv recv_msg_list;	
};

struct xpc_channel {
	short partid;		
	spinlock_t lock;	
	unsigned int flags;	

	enum xp_retval reason;	
	int reason_line;	

	u16 number;		

	u16 entry_size;		
	u16 local_nentries;	
	u16 remote_nentries;	

	atomic_t references;	

	atomic_t n_on_msg_allocate_wq;	
	wait_queue_head_t msg_allocate_wq;	

	u8 delayed_chctl_flags;	
				

	atomic_t n_to_notify;	

	xpc_channel_func func;	
	void *key;		

	struct completion wdisconnect_wait;    

	

	atomic_t kthreads_assigned;	
	u32 kthreads_assigned_limit;	
	atomic_t kthreads_idle;	
	u32 kthreads_idle_limit;	
	atomic_t kthreads_active;	

	wait_queue_head_t idle_wq;	

	union {
		struct xpc_channel_sn2 sn2;
		struct xpc_channel_uv uv;
	} sn;

} ____cacheline_aligned;


#define	XPC_C_WASCONNECTED	0x00000001	

#define XPC_C_ROPENCOMPLETE	0x00000002    
#define XPC_C_OPENCOMPLETE	0x00000004     
#define	XPC_C_ROPENREPLY	0x00000008	
#define	XPC_C_OPENREPLY		0x00000010	
#define	XPC_C_ROPENREQUEST	0x00000020     
#define	XPC_C_OPENREQUEST	0x00000040	

#define	XPC_C_SETUP		0x00000080 
#define	XPC_C_CONNECTEDCALLOUT	0x00000100     
#define	XPC_C_CONNECTEDCALLOUT_MADE \
				0x00000200     
#define	XPC_C_CONNECTED		0x00000400	
#define	XPC_C_CONNECTING	0x00000800	

#define	XPC_C_RCLOSEREPLY	0x00001000	
#define	XPC_C_CLOSEREPLY	0x00002000	
#define	XPC_C_RCLOSEREQUEST	0x00004000    
#define	XPC_C_CLOSEREQUEST	0x00008000     

#define	XPC_C_DISCONNECTED	0x00010000	
#define	XPC_C_DISCONNECTING	0x00020000   
#define	XPC_C_DISCONNECTINGCALLOUT \
				0x00040000 
#define	XPC_C_DISCONNECTINGCALLOUT_MADE \
				0x00080000 
#define	XPC_C_WDISCONNECT	0x00100000  


union xpc_channel_ctl_flags {
	u64 all_flags;
	u8 flags[XPC_MAX_NCHANNELS];
};

#define	XPC_CHCTL_CLOSEREQUEST	0x01
#define	XPC_CHCTL_CLOSEREPLY	0x02
#define	XPC_CHCTL_OPENREQUEST	0x04
#define	XPC_CHCTL_OPENREPLY	0x08
#define XPC_CHCTL_OPENCOMPLETE	0x10
#define	XPC_CHCTL_MSGREQUEST	0x20

#define XPC_OPENCLOSE_CHCTL_FLAGS \
			(XPC_CHCTL_CLOSEREQUEST | XPC_CHCTL_CLOSEREPLY | \
			 XPC_CHCTL_OPENREQUEST | XPC_CHCTL_OPENREPLY | \
			 XPC_CHCTL_OPENCOMPLETE)
#define XPC_MSG_CHCTL_FLAGS	XPC_CHCTL_MSGREQUEST

static inline int
xpc_any_openclose_chctl_flags_set(union xpc_channel_ctl_flags *chctl)
{
	int ch_number;

	for (ch_number = 0; ch_number < XPC_MAX_NCHANNELS; ch_number++) {
		if (chctl->flags[ch_number] & XPC_OPENCLOSE_CHCTL_FLAGS)
			return 1;
	}
	return 0;
}

static inline int
xpc_any_msg_chctl_flags_set(union xpc_channel_ctl_flags *chctl)
{
	int ch_number;

	for (ch_number = 0; ch_number < XPC_MAX_NCHANNELS; ch_number++) {
		if (chctl->flags[ch_number] & XPC_MSG_CHCTL_FLAGS)
			return 1;
	}
	return 0;
}


struct xpc_partition_sn2 {
	unsigned long remote_amos_page_pa; 
	int activate_IRQ_nasid;	
	int activate_IRQ_phys_cpuid;	

	unsigned long remote_vars_pa;	
	unsigned long remote_vars_part_pa; 
	u8 remote_vars_version;	

	void *local_GPs_base;	
	struct xpc_gp_sn2 *local_GPs;	
	void *remote_GPs_base;	
	struct xpc_gp_sn2 *remote_GPs;	
					
	unsigned long remote_GPs_pa; 
				     

	void *local_openclose_args_base;   
	struct xpc_openclose_args *local_openclose_args;      
	unsigned long remote_openclose_args_pa;	

	int notify_IRQ_nasid;	
	int notify_IRQ_phys_cpuid;	
	char notify_IRQ_owner[8];	

	struct amo *remote_chctl_amo_va; 
	struct amo *local_chctl_amo_va;	

	struct timer_list dropped_notify_IRQ_timer;	
};

struct xpc_partition_uv {
	unsigned long heartbeat_gpa; 
	struct xpc_heartbeat_uv cached_heartbeat; 
						  
	unsigned long activate_gru_mq_desc_gpa;	
						
						
	void *cached_activate_gru_mq_desc; 
					   
	struct mutex cached_activate_gru_mq_desc_mutex;
	spinlock_t flags_lock;	
	unsigned int flags;	
	u8 remote_act_state;	
	u8 act_state_req;	
	enum xp_retval reason;	
};


#define XPC_P_CACHED_ACTIVATE_GRU_MQ_DESC_UV	0x00000001
#define XPC_P_ENGAGED_UV			0x00000002


#define XPC_P_ASR_ACTIVATE_UV		0x01
#define XPC_P_ASR_REACTIVATE_UV		0x02
#define XPC_P_ASR_DEACTIVATE_UV		0x03

struct xpc_partition {

	

	u8 remote_rp_version;	
	unsigned long remote_rp_ts_jiffies; 
	unsigned long remote_rp_pa;	
	u64 last_heartbeat;	
	u32 activate_IRQ_rcvd;	
	spinlock_t act_lock;	
	u8 act_state;		
	enum xp_retval reason;	
	int reason_line;	

	unsigned long disengage_timeout;	
	struct timer_list disengage_timer;

	

	u8 setup_state;		
	wait_queue_head_t teardown_wq;	
	atomic_t references;	

	u8 nchannels;		
	atomic_t nchannels_active;  
	atomic_t nchannels_engaged;  
	struct xpc_channel *channels;	

	

	union xpc_channel_ctl_flags chctl; 
	spinlock_t chctl_lock;	

	void *remote_openclose_args_base;  
	struct xpc_openclose_args *remote_openclose_args; 
							  

	

	atomic_t channel_mgr_requests;	
	wait_queue_head_t channel_mgr_wq;	

	union {
		struct xpc_partition_sn2 sn2;
		struct xpc_partition_uv uv;
	} sn;

} ____cacheline_aligned;

struct xpc_arch_operations {
	int (*setup_partitions) (void);
	void (*teardown_partitions) (void);
	void (*process_activate_IRQ_rcvd) (void);
	enum xp_retval (*get_partition_rsvd_page_pa)
		(void *, u64 *, unsigned long *, size_t *);
	int (*setup_rsvd_page) (struct xpc_rsvd_page *);

	void (*allow_hb) (short);
	void (*disallow_hb) (short);
	void (*disallow_all_hbs) (void);
	void (*increment_heartbeat) (void);
	void (*offline_heartbeat) (void);
	void (*online_heartbeat) (void);
	void (*heartbeat_init) (void);
	void (*heartbeat_exit) (void);
	enum xp_retval (*get_remote_heartbeat) (struct xpc_partition *);

	void (*request_partition_activation) (struct xpc_rsvd_page *,
						 unsigned long, int);
	void (*request_partition_reactivation) (struct xpc_partition *);
	void (*request_partition_deactivation) (struct xpc_partition *);
	void (*cancel_partition_deactivation_request) (struct xpc_partition *);
	enum xp_retval (*setup_ch_structures) (struct xpc_partition *);
	void (*teardown_ch_structures) (struct xpc_partition *);

	enum xp_retval (*make_first_contact) (struct xpc_partition *);

	u64 (*get_chctl_all_flags) (struct xpc_partition *);
	void (*send_chctl_closerequest) (struct xpc_channel *, unsigned long *);
	void (*send_chctl_closereply) (struct xpc_channel *, unsigned long *);
	void (*send_chctl_openrequest) (struct xpc_channel *, unsigned long *);
	void (*send_chctl_openreply) (struct xpc_channel *, unsigned long *);
	void (*send_chctl_opencomplete) (struct xpc_channel *, unsigned long *);
	void (*process_msg_chctl_flags) (struct xpc_partition *, int);

	enum xp_retval (*save_remote_msgqueue_pa) (struct xpc_channel *,
						      unsigned long);

	enum xp_retval (*setup_msg_structures) (struct xpc_channel *);
	void (*teardown_msg_structures) (struct xpc_channel *);

	void (*indicate_partition_engaged) (struct xpc_partition *);
	void (*indicate_partition_disengaged) (struct xpc_partition *);
	void (*assume_partition_disengaged) (short);
	int (*partition_engaged) (short);
	int (*any_partition_engaged) (void);

	int (*n_of_deliverable_payloads) (struct xpc_channel *);
	enum xp_retval (*send_payload) (struct xpc_channel *, u32, void *,
					   u16, u8, xpc_notify_func, void *);
	void *(*get_deliverable_payload) (struct xpc_channel *);
	void (*received_payload) (struct xpc_channel *, void *);
	void (*notify_senders_of_disconnect) (struct xpc_channel *);
};


#define	XPC_P_AS_INACTIVE	0x00	
#define XPC_P_AS_ACTIVATION_REQ	0x01	
#define XPC_P_AS_ACTIVATING	0x02	
#define XPC_P_AS_ACTIVE		0x03	
#define XPC_P_AS_DEACTIVATING	0x04	

#define XPC_DEACTIVATE_PARTITION(_p, _reason) \
			xpc_deactivate_partition(__LINE__, (_p), (_reason))


#define XPC_P_SS_UNSET		0x00	
#define XPC_P_SS_SETUP		0x01	
#define XPC_P_SS_WTEARDOWN	0x02	
#define XPC_P_SS_TORNDOWN	0x03	

#define XPC_DROPPED_NOTIFY_IRQ_WAIT_INTERVAL	(0.25 * HZ)

#define XPC_DISENGAGE_DEFAULT_TIMELIMIT		90

#define XPC_DEACTIVATE_PRINTMSG_INTERVAL	10

#define XPC_PARTID(_p)	((short)((_p) - &xpc_partitions[0]))

extern struct xpc_registration xpc_registrations[];

extern struct device *xpc_part;
extern struct device *xpc_chan;
extern struct xpc_arch_operations xpc_arch_ops;
extern int xpc_disengage_timelimit;
extern int xpc_disengage_timedout;
extern int xpc_activate_IRQ_rcvd;
extern spinlock_t xpc_activate_IRQ_rcvd_lock;
extern wait_queue_head_t xpc_activate_IRQ_wq;
extern void *xpc_kzalloc_cacheline_aligned(size_t, gfp_t, void **);
extern void xpc_activate_partition(struct xpc_partition *);
extern void xpc_activate_kthreads(struct xpc_channel *, int);
extern void xpc_create_kthreads(struct xpc_channel *, int, int);
extern void xpc_disconnect_wait(int);

extern int xpc_init_sn2(void);
extern void xpc_exit_sn2(void);

extern int xpc_init_uv(void);
extern void xpc_exit_uv(void);

extern int xpc_exiting;
extern int xpc_nasid_mask_nlongs;
extern struct xpc_rsvd_page *xpc_rsvd_page;
extern unsigned long *xpc_mach_nasids;
extern struct xpc_partition *xpc_partitions;
extern void *xpc_kmalloc_cacheline_aligned(size_t, gfp_t, void **);
extern int xpc_setup_rsvd_page(void);
extern void xpc_teardown_rsvd_page(void);
extern int xpc_identify_activate_IRQ_sender(void);
extern int xpc_partition_disengaged(struct xpc_partition *);
extern enum xp_retval xpc_mark_partition_active(struct xpc_partition *);
extern void xpc_mark_partition_inactive(struct xpc_partition *);
extern void xpc_discovery(void);
extern enum xp_retval xpc_get_remote_rp(int, unsigned long *,
					struct xpc_rsvd_page *,
					unsigned long *);
extern void xpc_deactivate_partition(const int, struct xpc_partition *,
				     enum xp_retval);
extern enum xp_retval xpc_initiate_partid_to_nasids(short, void *);

extern void xpc_initiate_connect(int);
extern void xpc_initiate_disconnect(int);
extern enum xp_retval xpc_allocate_msg_wait(struct xpc_channel *);
extern enum xp_retval xpc_initiate_send(short, int, u32, void *, u16);
extern enum xp_retval xpc_initiate_send_notify(short, int, u32, void *, u16,
					       xpc_notify_func, void *);
extern void xpc_initiate_received(short, int, void *);
extern void xpc_process_sent_chctl_flags(struct xpc_partition *);
extern void xpc_connected_callout(struct xpc_channel *);
extern void xpc_deliver_payload(struct xpc_channel *);
extern void xpc_disconnect_channel(const int, struct xpc_channel *,
				   enum xp_retval, unsigned long *);
extern void xpc_disconnect_callout(struct xpc_channel *, enum xp_retval);
extern void xpc_partition_going_down(struct xpc_partition *, enum xp_retval);

static inline void
xpc_wakeup_channel_mgr(struct xpc_partition *part)
{
	if (atomic_inc_return(&part->channel_mgr_requests) == 1)
		wake_up(&part->channel_mgr_wq);
}

static inline void
xpc_msgqueue_ref(struct xpc_channel *ch)
{
	atomic_inc(&ch->references);
}

static inline void
xpc_msgqueue_deref(struct xpc_channel *ch)
{
	s32 refs = atomic_dec_return(&ch->references);

	DBUG_ON(refs < 0);
	if (refs == 0)
		xpc_wakeup_channel_mgr(&xpc_partitions[ch->partid]);
}

#define XPC_DISCONNECT_CHANNEL(_ch, _reason, _irqflgs) \
		xpc_disconnect_channel(__LINE__, _ch, _reason, _irqflgs)

static inline void
xpc_part_deref(struct xpc_partition *part)
{
	s32 refs = atomic_dec_return(&part->references);

	DBUG_ON(refs < 0);
	if (refs == 0 && part->setup_state == XPC_P_SS_WTEARDOWN)
		wake_up(&part->teardown_wq);
}

static inline int
xpc_part_ref(struct xpc_partition *part)
{
	int setup;

	atomic_inc(&part->references);
	setup = (part->setup_state == XPC_P_SS_SETUP);
	if (!setup)
		xpc_part_deref(part);

	return setup;
}

#define XPC_SET_REASON(_p, _reason, _line) \
	{ \
		(_p)->reason = _reason; \
		(_p)->reason_line = _line; \
	}

#endif 
