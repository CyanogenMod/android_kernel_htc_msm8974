/*
  drbd_int.h

  This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

  Copyright (C) 2001-2008, LINBIT Information Technologies GmbH.
  Copyright (C) 1999-2008, Philipp Reisner <philipp.reisner@linbit.com>.
  Copyright (C) 2002-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

  drbd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  drbd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with drbd; see the file COPYING.  If not, write to
  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef _DRBD_INT_H
#define _DRBD_INT_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <linux/ratelimit.h>
#include <linux/tcp.h>
#include <linux/mutex.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <net/tcp.h>
#include <linux/lru_cache.h>
#include <linux/prefetch.h>

#ifdef __CHECKER__
# define __protected_by(x)       __attribute__((require_context(x,1,999,"rdwr")))
# define __protected_read_by(x)  __attribute__((require_context(x,1,999,"read")))
# define __protected_write_by(x) __attribute__((require_context(x,1,999,"write")))
# define __must_hold(x)       __attribute__((context(x,1,1), require_context(x,1,999,"call")))
#else
# define __protected_by(x)
# define __protected_read_by(x)
# define __protected_write_by(x)
# define __must_hold(x)
#endif

#define __no_warn(lock, stmt) do { __acquire(lock); stmt; __release(lock); } while (0)

extern unsigned int minor_count;
extern bool disable_sendpage;
extern bool allow_oos;
extern unsigned int cn_idx;

#ifdef CONFIG_DRBD_FAULT_INJECTION
extern int enable_faults;
extern int fault_rate;
extern int fault_devs;
#endif

extern char usermode_helper[];


#define DRBD_SIG SIGXCPU

#define DRBD_SIGKILL SIGHUP


#define ID_IN_SYNC      (4711ULL)
#define ID_OUT_OF_SYNC  (4712ULL)

#define ID_SYNCER (-1ULL)
#define ID_VACANT 0
#define is_syncer_block_id(id) ((id) == ID_SYNCER)
#define UUID_NEW_BM_OFFSET ((u64)0x0001000000000000ULL)

struct drbd_conf;


#define DEV (disk_to_dev(mdev->vdisk))

#define D_ASSERT(exp)	if (!(exp)) \
	 dev_err(DEV, "ASSERT( " #exp " ) in %s:%d\n", __FILE__, __LINE__)

#define ERR_IF(exp) if (({						\
	int _b = (exp) != 0;						\
	if (_b) dev_err(DEV, "ASSERT FAILED: %s: (%s) in %s:%d\n",	\
			__func__, #exp, __FILE__, __LINE__);		\
	_b;								\
	}))

enum {
	DRBD_FAULT_MD_WR = 0,	
	DRBD_FAULT_MD_RD = 1,	
	DRBD_FAULT_RS_WR = 2,	
	DRBD_FAULT_RS_RD = 3,
	DRBD_FAULT_DT_WR = 4,	
	DRBD_FAULT_DT_RD = 5,
	DRBD_FAULT_DT_RA = 6,	
	DRBD_FAULT_BM_ALLOC = 7,	
	DRBD_FAULT_AL_EE = 8,	
	DRBD_FAULT_RECEIVE = 9, 

	DRBD_FAULT_MAX,
};

extern unsigned int
_drbd_insert_fault(struct drbd_conf *mdev, unsigned int type);

static inline int
drbd_insert_fault(struct drbd_conf *mdev, unsigned int type) {
#ifdef CONFIG_DRBD_FAULT_INJECTION
	return fault_rate &&
		(enable_faults & (1<<type)) &&
		_drbd_insert_fault(mdev, type);
#else
	return 0;
#endif
}

#define div_ceil(A, B) ((A)/(B) + ((A)%(B) ? 1 : 0))
#define div_floor(A, B) ((A)/(B))

#define DRBD_MD_MAGIC (DRBD_MAGIC+4)

extern struct drbd_conf **minor_table;
extern struct ratelimit_state drbd_ratelimit_state;

enum drbd_packets {
	
	P_DATA		      = 0x00,
	P_DATA_REPLY	      = 0x01, 
	P_RS_DATA_REPLY	      = 0x02, 
	P_BARRIER	      = 0x03,
	P_BITMAP	      = 0x04,
	P_BECOME_SYNC_TARGET  = 0x05,
	P_BECOME_SYNC_SOURCE  = 0x06,
	P_UNPLUG_REMOTE	      = 0x07, 
	P_DATA_REQUEST	      = 0x08, 
	P_RS_DATA_REQUEST     = 0x09, 
	P_SYNC_PARAM	      = 0x0a,
	P_PROTOCOL	      = 0x0b,
	P_UUIDS		      = 0x0c,
	P_SIZES		      = 0x0d,
	P_STATE		      = 0x0e,
	P_SYNC_UUID	      = 0x0f,
	P_AUTH_CHALLENGE      = 0x10,
	P_AUTH_RESPONSE	      = 0x11,
	P_STATE_CHG_REQ	      = 0x12,

	
	P_PING		      = 0x13,
	P_PING_ACK	      = 0x14,
	P_RECV_ACK	      = 0x15, 
	P_WRITE_ACK	      = 0x16, 
	P_RS_WRITE_ACK	      = 0x17, 
	P_DISCARD_ACK	      = 0x18, 
	P_NEG_ACK	      = 0x19, 
	P_NEG_DREPLY	      = 0x1a, 
	P_NEG_RS_DREPLY	      = 0x1b, 
	P_BARRIER_ACK	      = 0x1c,
	P_STATE_CHG_REPLY     = 0x1d,

	

	P_OV_REQUEST	      = 0x1e, 
	P_OV_REPLY	      = 0x1f,
	P_OV_RESULT	      = 0x20, 
	P_CSUM_RS_REQUEST     = 0x21, 
	P_RS_IS_IN_SYNC	      = 0x22, 
	P_SYNC_PARAM89	      = 0x23, 
	P_COMPRESSED_BITMAP   = 0x24, 
	
	
	P_DELAY_PROBE         = 0x27, 
	P_OUT_OF_SYNC         = 0x28, 
	P_RS_CANCEL           = 0x29, 

	P_MAX_CMD	      = 0x2A,
	P_MAY_IGNORE	      = 0x100, 
	P_MAX_OPT_CMD	      = 0x101,

	

	P_HAND_SHAKE_M	      = 0xfff1, 
	P_HAND_SHAKE_S	      = 0xfff2, 

	P_HAND_SHAKE	      = 0xfffe	
};

static inline const char *cmdname(enum drbd_packets cmd)
{
	static const char *cmdnames[] = {
		[P_DATA]	        = "Data",
		[P_DATA_REPLY]	        = "DataReply",
		[P_RS_DATA_REPLY]	= "RSDataReply",
		[P_BARRIER]	        = "Barrier",
		[P_BITMAP]	        = "ReportBitMap",
		[P_BECOME_SYNC_TARGET]  = "BecomeSyncTarget",
		[P_BECOME_SYNC_SOURCE]  = "BecomeSyncSource",
		[P_UNPLUG_REMOTE]	= "UnplugRemote",
		[P_DATA_REQUEST]	= "DataRequest",
		[P_RS_DATA_REQUEST]     = "RSDataRequest",
		[P_SYNC_PARAM]	        = "SyncParam",
		[P_SYNC_PARAM89]	= "SyncParam89",
		[P_PROTOCOL]            = "ReportProtocol",
		[P_UUIDS]	        = "ReportUUIDs",
		[P_SIZES]	        = "ReportSizes",
		[P_STATE]	        = "ReportState",
		[P_SYNC_UUID]           = "ReportSyncUUID",
		[P_AUTH_CHALLENGE]      = "AuthChallenge",
		[P_AUTH_RESPONSE]	= "AuthResponse",
		[P_PING]		= "Ping",
		[P_PING_ACK]	        = "PingAck",
		[P_RECV_ACK]	        = "RecvAck",
		[P_WRITE_ACK]	        = "WriteAck",
		[P_RS_WRITE_ACK]	= "RSWriteAck",
		[P_DISCARD_ACK]	        = "DiscardAck",
		[P_NEG_ACK]	        = "NegAck",
		[P_NEG_DREPLY]	        = "NegDReply",
		[P_NEG_RS_DREPLY]	= "NegRSDReply",
		[P_BARRIER_ACK]	        = "BarrierAck",
		[P_STATE_CHG_REQ]       = "StateChgRequest",
		[P_STATE_CHG_REPLY]     = "StateChgReply",
		[P_OV_REQUEST]          = "OVRequest",
		[P_OV_REPLY]            = "OVReply",
		[P_OV_RESULT]           = "OVResult",
		[P_CSUM_RS_REQUEST]     = "CsumRSRequest",
		[P_RS_IS_IN_SYNC]	= "CsumRSIsInSync",
		[P_COMPRESSED_BITMAP]   = "CBitmap",
		[P_DELAY_PROBE]         = "DelayProbe",
		[P_OUT_OF_SYNC]		= "OutOfSync",
		[P_MAX_CMD]	        = NULL,
	};

	if (cmd == P_HAND_SHAKE_M)
		return "HandShakeM";
	if (cmd == P_HAND_SHAKE_S)
		return "HandShakeS";
	if (cmd == P_HAND_SHAKE)
		return "HandShake";
	if (cmd >= P_MAX_CMD)
		return "Unknown";
	return cmdnames[cmd];
}

struct bm_xfer_ctx {
	unsigned long bm_bits;
	unsigned long bm_words;
	
	unsigned long bit_offset;
	unsigned long word_offset;

	
	unsigned packets[2];
	unsigned bytes[2];
};

extern void INFO_bm_xfer_stats(struct drbd_conf *mdev,
		const char *direction, struct bm_xfer_ctx *c);

static inline void bm_xfer_ctx_bit_to_word_offset(struct bm_xfer_ctx *c)
{
#if BITS_PER_LONG == 64
	c->word_offset = c->bit_offset >> 6;
#elif BITS_PER_LONG == 32
	c->word_offset = c->bit_offset >> 5;
	c->word_offset &= ~(1UL);
#else
# error "unsupported BITS_PER_LONG"
#endif
}

#ifndef __packed
#define __packed __attribute__((packed))
#endif

struct p_header80 {
	u32	  magic;
	u16	  command;
	u16	  length;	
	u8	  payload[0];
} __packed;

struct p_header95 {
	u16	  magic;	
	u16	  command;
	u32	  length;	
	u8	  payload[0];
} __packed;

union p_header {
	struct p_header80 h80;
	struct p_header95 h95;
};



#define DP_HARDBARRIER	      1 
#define DP_RW_SYNC	      2 
#define DP_MAY_SET_IN_SYNC    4
#define DP_UNPLUG             8 
#define DP_FUA               16 
#define DP_FLUSH             32 
#define DP_DISCARD           64 

struct p_data {
	union p_header head;
	u64	    sector;    
	u64	    block_id;  
	u32	    seq_num;
	u32	    dp_flags;
} __packed;

struct p_block_ack {
	struct p_header80 head;
	u64	    sector;
	u64	    block_id;
	u32	    blksize;
	u32	    seq_num;
} __packed;


struct p_block_req {
	struct p_header80 head;
	u64 sector;
	u64 block_id;
	u32 blksize;
	u32 pad;	
} __packed;


struct p_handshake {
	struct p_header80 head;	
	u32 protocol_min;
	u32 feature_flags;
	u32 protocol_max;


	u32 _pad;
	u64 reserverd[7];
} __packed;

struct p_barrier {
	struct p_header80 head;
	u32 barrier;	
	u32 pad;	
} __packed;

struct p_barrier_ack {
	struct p_header80 head;
	u32 barrier;
	u32 set_size;
} __packed;

struct p_rs_param {
	struct p_header80 head;
	u32 rate;

	      
	char verify_alg[0];
} __packed;

struct p_rs_param_89 {
	struct p_header80 head;
	u32 rate;
        
	char verify_alg[SHARED_SECRET_MAX];
	char csums_alg[SHARED_SECRET_MAX];
} __packed;

struct p_rs_param_95 {
	struct p_header80 head;
	u32 rate;
	char verify_alg[SHARED_SECRET_MAX];
	char csums_alg[SHARED_SECRET_MAX];
	u32 c_plan_ahead;
	u32 c_delay_target;
	u32 c_fill_target;
	u32 c_max_rate;
} __packed;

enum drbd_conn_flags {
	CF_WANT_LOSE = 1,
	CF_DRY_RUN = 2,
};

struct p_protocol {
	struct p_header80 head;
	u32 protocol;
	u32 after_sb_0p;
	u32 after_sb_1p;
	u32 after_sb_2p;
	u32 conn_flags;
	u32 two_primaries;

              
	char integrity_alg[0];

} __packed;

struct p_uuids {
	struct p_header80 head;
	u64 uuid[UI_EXTENDED_SIZE];
} __packed;

struct p_rs_uuid {
	struct p_header80 head;
	u64	    uuid;
} __packed;

struct p_sizes {
	struct p_header80 head;
	u64	    d_size;  
	u64	    u_size;  
	u64	    c_size;  
	u32	    max_bio_size;  
	u16	    queue_order_type;  
	u16	    dds_flags; 
} __packed;

struct p_state {
	struct p_header80 head;
	u32	    state;
} __packed;

struct p_req_state {
	struct p_header80 head;
	u32	    mask;
	u32	    val;
} __packed;

struct p_req_state_reply {
	struct p_header80 head;
	u32	    retcode;
} __packed;

struct p_drbd06_param {
	u64	  size;
	u32	  state;
	u32	  blksize;
	u32	  protocol;
	u32	  version;
	u32	  gen_cnt[5];
	u32	  bit_map_gen[5];
} __packed;

struct p_discard {
	struct p_header80 head;
	u64	    block_id;
	u32	    seq_num;
	u32	    pad;
} __packed;

struct p_block_desc {
	struct p_header80 head;
	u64 sector;
	u32 blksize;
	u32 pad;	
} __packed;

enum drbd_bitmap_code {
	RLE_VLI_Bits = 2,
};

struct p_compressed_bm {
	struct p_header80 head;
	u8 encoding;

	u8 code[0];
} __packed;

struct p_delay_probe93 {
	struct p_header80 head;
	u32     seq_num; 
	u32     offset;  
} __packed;

static inline enum drbd_bitmap_code
DCBP_get_code(struct p_compressed_bm *p)
{
	return (enum drbd_bitmap_code)(p->encoding & 0x0f);
}

static inline void
DCBP_set_code(struct p_compressed_bm *p, enum drbd_bitmap_code code)
{
	BUG_ON(code & ~0xf);
	p->encoding = (p->encoding & ~0xf) | code;
}

static inline int
DCBP_get_start(struct p_compressed_bm *p)
{
	return (p->encoding & 0x80) != 0;
}

static inline void
DCBP_set_start(struct p_compressed_bm *p, int set)
{
	p->encoding = (p->encoding & ~0x80) | (set ? 0x80 : 0);
}

static inline int
DCBP_get_pad_bits(struct p_compressed_bm *p)
{
	return (p->encoding >> 4) & 0x7;
}

static inline void
DCBP_set_pad_bits(struct p_compressed_bm *p, int n)
{
	BUG_ON(n & ~0x7);
	p->encoding = (p->encoding & (~0x7 << 4)) | (n << 4);
}

#define BM_PACKET_PAYLOAD_BYTES (4096 - sizeof(struct p_header80))
#define BM_PACKET_WORDS (BM_PACKET_PAYLOAD_BYTES/sizeof(long))
#define BM_PACKET_VLI_BYTES_MAX (4096 - sizeof(struct p_compressed_bm))
#if (PAGE_SIZE < 4096)
#error "PAGE_SIZE too small"
#endif

union p_polymorph {
        union p_header           header;
        struct p_handshake       handshake;
        struct p_data            data;
        struct p_block_ack       block_ack;
        struct p_barrier         barrier;
        struct p_barrier_ack     barrier_ack;
        struct p_rs_param_89     rs_param_89;
        struct p_rs_param_95     rs_param_95;
        struct p_protocol        protocol;
        struct p_sizes           sizes;
        struct p_uuids           uuids;
        struct p_state           state;
        struct p_req_state       req_state;
        struct p_req_state_reply req_state_reply;
        struct p_block_req       block_req;
	struct p_delay_probe93   delay_probe93;
	struct p_rs_uuid         rs_uuid;
	struct p_block_desc      block_desc;
} __packed;

enum drbd_thread_state {
	None,
	Running,
	Exiting,
	Restarting
};

struct drbd_thread {
	spinlock_t t_lock;
	struct task_struct *task;
	struct completion stop;
	enum drbd_thread_state t_state;
	int (*function) (struct drbd_thread *);
	struct drbd_conf *mdev;
	int reset_cpu_mask;
};

static inline enum drbd_thread_state get_t_state(struct drbd_thread *thi)
{

	smp_rmb();
	return thi->t_state;
}

struct drbd_work;
typedef int (*drbd_work_cb)(struct drbd_conf *, struct drbd_work *, int cancel);
struct drbd_work {
	struct list_head list;
	drbd_work_cb cb;
};

struct drbd_tl_epoch;
struct drbd_request {
	struct drbd_work w;
	struct drbd_conf *mdev;

	struct bio *private_bio;

	struct hlist_node collision;
	sector_t sector;
	unsigned int size;
	unsigned int epoch; 


	struct list_head tl_requests; 
	struct bio *master_bio;       
	unsigned long rq_state; 
	int seq_num;
	unsigned long start_time;
};

struct drbd_tl_epoch {
	struct drbd_work w;
	struct list_head requests; 
	struct drbd_tl_epoch *next; 
	unsigned int br_number;  
	int n_writes;	
};

struct drbd_request;

/* These Tl_epoch_entries may be in one of 6 lists:
   active_ee .. data packet being written
   sync_ee   .. syncer block being written
   done_ee   .. block written, need to send P_WRITE_ACK
   read_ee   .. [RS]P_DATA_REQUEST being read
*/

struct drbd_epoch {
	struct list_head list;
	unsigned int barrier_nr;
	atomic_t epoch_size; 
	atomic_t active;     
	unsigned long flags;
};

enum {
	DE_HAVE_BARRIER_NUMBER,
};

enum epoch_event {
	EV_PUT,
	EV_GOT_BARRIER_NR,
	EV_BECAME_LAST,
	EV_CLEANUP = 32, 
};

struct drbd_wq_barrier {
	struct drbd_work w;
	struct completion done;
};

struct digest_info {
	int digest_size;
	void *digest;
};

struct drbd_epoch_entry {
	struct drbd_work w;
	struct hlist_node collision;
	struct drbd_epoch *epoch; 
	struct drbd_conf *mdev;
	struct page *pages;
	atomic_t pending_bios;
	unsigned int size;
	
	unsigned long flags;
	sector_t sector;
	union {
		u64 block_id;
		struct digest_info *digest;
	};
};

enum {
	__EE_CALL_AL_COMPLETE_IO,
	__EE_MAY_SET_IN_SYNC,

	__EE_RESUBMITTED,

	__EE_WAS_ERROR,

	
	__EE_HAS_DIGEST,
};
#define EE_CALL_AL_COMPLETE_IO (1<<__EE_CALL_AL_COMPLETE_IO)
#define EE_MAY_SET_IN_SYNC     (1<<__EE_MAY_SET_IN_SYNC)
#define	EE_RESUBMITTED         (1<<__EE_RESUBMITTED)
#define EE_WAS_ERROR           (1<<__EE_WAS_ERROR)
#define EE_HAS_DIGEST          (1<<__EE_HAS_DIGEST)

enum {
	CREATE_BARRIER,		
	SIGNAL_ASENDER,		
	SEND_PING,		

	UNPLUG_QUEUED,		
	UNPLUG_REMOTE,		
	MD_DIRTY,		
	DISCARD_CONCURRENT,	
	USE_DEGR_WFC_T,		
	CLUSTER_ST_CHANGE,	
	CL_ST_CHG_SUCCESS,
	CL_ST_CHG_FAIL,
	CRASHED_PRIMARY,	
	NO_BARRIER_SUPP,	
	CONSIDER_RESYNC,

	MD_NO_FUA,		
	SUSPEND_IO,		
	BITMAP_IO,		
	BITMAP_IO_QUEUED,       
	GO_DISKLESS,		
	WAS_IO_ERROR,		
	RESYNC_AFTER_NEG,       
	NET_CONGESTED,		

	CONFIG_PENDING,		
	DEVICE_DYING,		
	RESIZE_PENDING,		
	CONN_DRY_RUN,		
	GOT_PING_ACK,		
	NEW_CUR_UUID,		
	AL_SUSPENDED,		
	AHEAD_TO_SYNC_SOURCE,   
};

struct drbd_bitmap; 

enum bm_flag {
	
	BM_P_VMALLOCED = 0x10000, 

	
	BM_LOCKED_MASK = 0x7,

	
	BM_DONT_CLEAR = 0x1,
	BM_DONT_SET   = 0x2,
	BM_DONT_TEST  = 0x4,

	
	BM_LOCKED_TEST_ALLOWED = 0x3,

	BM_LOCKED_SET_ALLOWED = 0x1,

	
};



struct drbd_work_queue {
	struct list_head q;
	struct semaphore s; 
	spinlock_t q_lock;  
};

struct drbd_socket {
	struct drbd_work_queue work;
	struct mutex mutex;
	struct socket    *socket;
	union p_polymorph sbuf;
	union p_polymorph rbuf;
};

struct drbd_md {
	u64 md_offset;		

	u64 la_size_sect;	
	u64 uuid[UI_SIZE];
	u64 device_uuid;
	u32 flags;
	u32 md_size_sect;

	s32 al_offset;	
	s32 bm_offset;	

};

#define NL_PACKET(name, number, fields) struct name { fields };
#define NL_INTEGER(pn,pr,member) int member;
#define NL_INT64(pn,pr,member) __u64 member;
#define NL_BIT(pn,pr,member)   unsigned member:1;
#define NL_STRING(pn,pr,member,len) unsigned char member[len]; int member ## _len;
#include <linux/drbd_nl.h>

struct drbd_backing_dev {
	struct block_device *backing_bdev;
	struct block_device *md_bdev;
	struct drbd_md md;
	struct disk_conf dc; 
	sector_t known_size; 
};

struct drbd_md_io {
	struct drbd_conf *mdev;
	struct completion event;
	int error;
};

struct bm_io_work {
	struct drbd_work w;
	char *why;
	enum bm_flag flags;
	int (*io_fn)(struct drbd_conf *mdev);
	void (*done)(struct drbd_conf *mdev, int rv);
};

enum write_ordering_e {
	WO_none,
	WO_drain_io,
	WO_bdev_flush,
};

struct fifo_buffer {
	int *values;
	unsigned int head_index;
	unsigned int size;
};

struct drbd_conf {
	
	unsigned long flags;

	
	struct net_conf *net_conf; 
	struct syncer_conf sync_conf;
	struct drbd_backing_dev *ldev __protected_by(local);

	sector_t p_size;     
	struct request_queue *rq_queue;
	struct block_device *this_bdev;
	struct gendisk	    *vdisk;

	struct drbd_socket data; 
	struct drbd_socket meta; 
	int agreed_pro_version;  
	unsigned long last_received; 
	unsigned int ko_count;
	struct drbd_work  resync_work,
			  unplug_work,
			  go_diskless,
			  md_sync_work,
			  start_resync_work;
	struct timer_list resync_timer;
	struct timer_list md_sync_timer;
	struct timer_list start_resync_timer;
	struct timer_list request_timer;
#ifdef DRBD_DEBUG_MD_SYNC
	struct {
		unsigned int line;
		const char* func;
	} last_md_mark_dirty;
#endif

	
	union drbd_state new_state_tmp;

	union drbd_state state;
	wait_queue_head_t misc_wait;
	wait_queue_head_t state_wait;  
	wait_queue_head_t net_cnt_wait;
	unsigned int send_cnt;
	unsigned int recv_cnt;
	unsigned int read_cnt;
	unsigned int writ_cnt;
	unsigned int al_writ_cnt;
	unsigned int bm_writ_cnt;
	atomic_t ap_bio_cnt;	 
	atomic_t ap_pending_cnt; 
	atomic_t rs_pending_cnt; 
	atomic_t unacked_cnt;	 
	atomic_t local_cnt;	 
	atomic_t net_cnt;	 
	spinlock_t req_lock;
	struct drbd_tl_epoch *unused_spare_tle; 
	struct drbd_tl_epoch *newest_tle;
	struct drbd_tl_epoch *oldest_tle;
	struct list_head out_of_sequence_requests;
	struct hlist_head *tl_hash;
	unsigned int tl_hash_s;

	
	unsigned long rs_total;
	
	unsigned long rs_failed;
	
	unsigned long rs_start;
	
	unsigned long rs_paused;
	
	unsigned long rs_same_csum;
#define DRBD_SYNC_MARKS 8
#define DRBD_SYNC_MARK_STEP (3*HZ)
	
	unsigned long rs_mark_left[DRBD_SYNC_MARKS];
	
	unsigned long rs_mark_time[DRBD_SYNC_MARKS];
	
	int rs_last_mark;

	
	sector_t ov_start_sector;
	
	sector_t ov_position;
	
	sector_t ov_last_oos_start;
	
	sector_t ov_last_oos_size;
	unsigned long ov_left; 
	struct crypto_hash *csums_tfm;
	struct crypto_hash *verify_tfm;

	struct drbd_thread receiver;
	struct drbd_thread worker;
	struct drbd_thread asender;
	struct drbd_bitmap *bitmap;
	unsigned long bm_resync_fo; 

	
	struct lru_cache *resync;
	
	unsigned int resync_locked;
	
	unsigned int resync_wenr;

	int open_cnt;
	u64 *p_uuid;
	struct drbd_epoch *current_epoch;
	spinlock_t epoch_lock;
	unsigned int epochs;
	enum write_ordering_e write_ordering;
	struct list_head active_ee; /* IO in progress (P_DATA gets written to disk) */
	struct list_head sync_ee;   /* IO in progress (P_RS_DATA_REPLY gets written to disk) */
	struct list_head done_ee;   
	struct list_head read_ee;   
	struct list_head net_ee;    
	struct hlist_head *ee_hash; 
	unsigned int ee_hash_s;

	
	struct drbd_epoch_entry *last_write_w_barrier;

	int next_barrier_nr;
	struct hlist_head *app_reads_hash; 
	struct list_head resync_reads;
	atomic_t pp_in_use;		
	atomic_t pp_in_use_by_net;	
	wait_queue_head_t ee_wait;
	struct page *md_io_page;	
	struct page *md_io_tmpp;	
	struct mutex md_io_mutex;	
	spinlock_t al_lock;
	wait_queue_head_t al_wait;
	struct lru_cache *act_log;	
	unsigned int al_tr_number;
	int al_tr_cycle;
	int al_tr_pos;   
	struct crypto_hash *cram_hmac_tfm;
	struct crypto_hash *integrity_w_tfm; 
	struct crypto_hash *integrity_r_tfm; 
	void *int_dig_out;
	void *int_dig_in;
	void *int_dig_vv;
	wait_queue_head_t seq_wait;
	atomic_t packet_seq;
	unsigned int peer_seq;
	spinlock_t peer_seq_lock;
	unsigned int minor;
	unsigned long comm_bm_set; 
	cpumask_var_t cpu_mask;
	struct bm_io_work bm_io_work;
	u64 ed_uuid; 
	struct mutex state_mutex;
	char congestion_reason;  
	atomic_t rs_sect_in; 
	atomic_t rs_sect_ev; 
	int rs_last_sect_ev; 
	int rs_last_events;  
	int c_sync_rate; 
	struct fifo_buffer rs_plan_s; 
	int rs_in_flight; 
	int rs_planed;    
	atomic_t ap_in_flight; 
	int peer_max_bio_size;
	int local_max_bio_size;
};

static inline struct drbd_conf *minor_to_mdev(unsigned int minor)
{
	struct drbd_conf *mdev;

	mdev = minor < minor_count ? minor_table[minor] : NULL;

	return mdev;
}

static inline unsigned int mdev_to_minor(struct drbd_conf *mdev)
{
	return mdev->minor;
}

static inline int drbd_get_data_sock(struct drbd_conf *mdev)
{
	mutex_lock(&mdev->data.mutex);
	if (unlikely(mdev->data.socket == NULL)) {
		mutex_unlock(&mdev->data.mutex);
		return 0;
	}
	return 1;
}

static inline void drbd_put_data_sock(struct drbd_conf *mdev)
{
	mutex_unlock(&mdev->data.mutex);
}



enum chg_state_flags {
	CS_HARD	= 1,
	CS_VERBOSE = 2,
	CS_WAIT_COMPLETE = 4,
	CS_SERIALIZE    = 8,
	CS_ORDERED      = CS_WAIT_COMPLETE + CS_SERIALIZE,
};

enum dds_flags {
	DDSF_FORCED    = 1,
	DDSF_NO_RESYNC = 2, 
};

extern void drbd_init_set_defaults(struct drbd_conf *mdev);
extern enum drbd_state_rv drbd_change_state(struct drbd_conf *mdev,
					    enum chg_state_flags f,
					    union drbd_state mask,
					    union drbd_state val);
extern void drbd_force_state(struct drbd_conf *, union drbd_state,
			union drbd_state);
extern enum drbd_state_rv _drbd_request_state(struct drbd_conf *,
					      union drbd_state,
					      union drbd_state,
					      enum chg_state_flags);
extern enum drbd_state_rv __drbd_set_state(struct drbd_conf *, union drbd_state,
					   enum chg_state_flags,
					   struct completion *done);
extern void print_st_err(struct drbd_conf *, union drbd_state,
			union drbd_state, int);
extern int  drbd_thread_start(struct drbd_thread *thi);
extern void _drbd_thread_stop(struct drbd_thread *thi, int restart, int wait);
#ifdef CONFIG_SMP
extern void drbd_thread_current_set_cpu(struct drbd_conf *mdev);
extern void drbd_calc_cpu_mask(struct drbd_conf *mdev);
#else
#define drbd_thread_current_set_cpu(A) ({})
#define drbd_calc_cpu_mask(A) ({})
#endif
extern void drbd_free_resources(struct drbd_conf *mdev);
extern void tl_release(struct drbd_conf *mdev, unsigned int barrier_nr,
		       unsigned int set_size);
extern void tl_clear(struct drbd_conf *mdev);
extern void _tl_add_barrier(struct drbd_conf *, struct drbd_tl_epoch *);
extern void drbd_free_sock(struct drbd_conf *mdev);
extern int drbd_send(struct drbd_conf *mdev, struct socket *sock,
			void *buf, size_t size, unsigned msg_flags);
extern int drbd_send_protocol(struct drbd_conf *mdev);
extern int drbd_send_uuids(struct drbd_conf *mdev);
extern int drbd_send_uuids_skip_initial_sync(struct drbd_conf *mdev);
extern int drbd_gen_and_send_sync_uuid(struct drbd_conf *mdev);
extern int drbd_send_sizes(struct drbd_conf *mdev, int trigger_reply, enum dds_flags flags);
extern int _drbd_send_state(struct drbd_conf *mdev);
extern int drbd_send_state(struct drbd_conf *mdev);
extern int _drbd_send_cmd(struct drbd_conf *mdev, struct socket *sock,
			enum drbd_packets cmd, struct p_header80 *h,
			size_t size, unsigned msg_flags);
#define USE_DATA_SOCKET 1
#define USE_META_SOCKET 0
extern int drbd_send_cmd(struct drbd_conf *mdev, int use_data_socket,
			enum drbd_packets cmd, struct p_header80 *h,
			size_t size);
extern int drbd_send_cmd2(struct drbd_conf *mdev, enum drbd_packets cmd,
			char *data, size_t size);
extern int drbd_send_sync_param(struct drbd_conf *mdev, struct syncer_conf *sc);
extern int drbd_send_b_ack(struct drbd_conf *mdev, u32 barrier_nr,
			u32 set_size);
extern int drbd_send_ack(struct drbd_conf *mdev, enum drbd_packets cmd,
			struct drbd_epoch_entry *e);
extern int drbd_send_ack_rp(struct drbd_conf *mdev, enum drbd_packets cmd,
			struct p_block_req *rp);
extern int drbd_send_ack_dp(struct drbd_conf *mdev, enum drbd_packets cmd,
			struct p_data *dp, int data_size);
extern int drbd_send_ack_ex(struct drbd_conf *mdev, enum drbd_packets cmd,
			    sector_t sector, int blksize, u64 block_id);
extern int drbd_send_oos(struct drbd_conf *mdev, struct drbd_request *req);
extern int drbd_send_block(struct drbd_conf *mdev, enum drbd_packets cmd,
			   struct drbd_epoch_entry *e);
extern int drbd_send_dblock(struct drbd_conf *mdev, struct drbd_request *req);
extern int drbd_send_drequest(struct drbd_conf *mdev, int cmd,
			      sector_t sector, int size, u64 block_id);
extern int drbd_send_drequest_csum(struct drbd_conf *mdev,
				   sector_t sector,int size,
				   void *digest, int digest_size,
				   enum drbd_packets cmd);
extern int drbd_send_ov_request(struct drbd_conf *mdev,sector_t sector,int size);

extern int drbd_send_bitmap(struct drbd_conf *mdev);
extern int _drbd_send_bitmap(struct drbd_conf *mdev);
extern int drbd_send_sr_reply(struct drbd_conf *mdev, enum drbd_state_rv retcode);
extern void drbd_free_bc(struct drbd_backing_dev *ldev);
extern void drbd_mdev_cleanup(struct drbd_conf *mdev);
void drbd_print_uuids(struct drbd_conf *mdev, const char *text);

extern void drbd_md_sync(struct drbd_conf *mdev);
extern int  drbd_md_read(struct drbd_conf *mdev, struct drbd_backing_dev *bdev);
extern void drbd_uuid_set(struct drbd_conf *mdev, int idx, u64 val) __must_hold(local);
extern void _drbd_uuid_set(struct drbd_conf *mdev, int idx, u64 val) __must_hold(local);
extern void drbd_uuid_new_current(struct drbd_conf *mdev) __must_hold(local);
extern void _drbd_uuid_new_current(struct drbd_conf *mdev) __must_hold(local);
extern void drbd_uuid_set_bm(struct drbd_conf *mdev, u64 val) __must_hold(local);
extern void drbd_md_set_flag(struct drbd_conf *mdev, int flags) __must_hold(local);
extern void drbd_md_clear_flag(struct drbd_conf *mdev, int flags)__must_hold(local);
extern int drbd_md_test_flag(struct drbd_backing_dev *, int);
#ifndef DRBD_DEBUG_MD_SYNC
extern void drbd_md_mark_dirty(struct drbd_conf *mdev);
#else
#define drbd_md_mark_dirty(m)	drbd_md_mark_dirty_(m, __LINE__ , __func__ )
extern void drbd_md_mark_dirty_(struct drbd_conf *mdev,
		unsigned int line, const char *func);
#endif
extern void drbd_queue_bitmap_io(struct drbd_conf *mdev,
				 int (*io_fn)(struct drbd_conf *),
				 void (*done)(struct drbd_conf *, int),
				 char *why, enum bm_flag flags);
extern int drbd_bitmap_io(struct drbd_conf *mdev,
		int (*io_fn)(struct drbd_conf *),
		char *why, enum bm_flag flags);
extern int drbd_bmio_set_n_write(struct drbd_conf *mdev);
extern int drbd_bmio_clear_n_write(struct drbd_conf *mdev);
extern void drbd_go_diskless(struct drbd_conf *mdev);
extern void drbd_ldev_destroy(struct drbd_conf *mdev);



#define MD_RESERVED_SECT (128LU << 11)  
#define MD_AL_OFFSET 8	    
#define MD_AL_MAX_SIZE 64   
#define MD_BM_OFFSET (MD_AL_OFFSET + MD_AL_MAX_SIZE)

#define MD_SECTOR_SHIFT	 9
#define MD_SECTOR_SIZE	 (1<<MD_SECTOR_SHIFT)

#define AL_EXTENTS_PT ((MD_SECTOR_SIZE-12)/8-1) 
#define AL_EXTENT_SHIFT 22		 
#define AL_EXTENT_SIZE (1<<AL_EXTENT_SHIFT)

#if BITS_PER_LONG == 32
#define LN2_BPL 5
#define cpu_to_lel(A) cpu_to_le32(A)
#define lel_to_cpu(A) le32_to_cpu(A)
#elif BITS_PER_LONG == 64
#define LN2_BPL 6
#define cpu_to_lel(A) cpu_to_le64(A)
#define lel_to_cpu(A) le64_to_cpu(A)
#else
#error "LN2 of BITS_PER_LONG unknown!"
#endif

struct bm_extent {
	int rs_left; 
	int rs_failed; 
	unsigned long flags;
	struct lc_element lce;
};

#define BME_NO_WRITES  0  
#define BME_LOCKED     1  
#define BME_PRIORITY   2  


#define SLEEP_TIME (HZ/10)

#define BM_BLOCK_SHIFT  12			 
#define BM_BLOCK_SIZE	 (1<<BM_BLOCK_SHIFT)
#define BM_EXT_SHIFT	 (BM_BLOCK_SHIFT + MD_SECTOR_SHIFT + 3)  
#define BM_EXT_SIZE	 (1<<BM_EXT_SHIFT)

#if (BM_EXT_SHIFT != 24) || (BM_BLOCK_SHIFT != 12)
#error "HAVE YOU FIXED drbdmeta AS WELL??"
#endif

#define BM_SECT_TO_BIT(x)   ((x)>>(BM_BLOCK_SHIFT-9))
#define BM_BIT_TO_SECT(x)   ((sector_t)(x)<<(BM_BLOCK_SHIFT-9))
#define BM_SECT_PER_BIT     BM_BIT_TO_SECT(1)

#define Bit2KB(bits) ((bits)<<(BM_BLOCK_SHIFT-10))

#define BM_SECT_TO_EXT(x)   ((x)>>(BM_EXT_SHIFT-9))

#define BM_EXT_TO_SECT(x)   ((sector_t)(x) << (BM_EXT_SHIFT-9))
#define BM_SECT_PER_EXT     BM_EXT_TO_SECT(1)

#define AL_EXT_PER_BM_SECT  (1 << (BM_EXT_SHIFT - AL_EXTENT_SHIFT))
#define BM_WORDS_PER_AL_EXT (1 << (AL_EXTENT_SHIFT-BM_BLOCK_SHIFT-LN2_BPL))

#define BM_BLOCKS_PER_BM_EXT_B (BM_EXT_SHIFT - BM_BLOCK_SHIFT)
#define BM_BLOCKS_PER_BM_EXT_MASK  ((1<<BM_BLOCKS_PER_BM_EXT_B) - 1)


#define DRBD_MAX_SECTORS_32 (0xffffffffLU)
#define DRBD_MAX_SECTORS_BM \
	  ((MD_RESERVED_SECT - MD_BM_OFFSET) * (1LL<<(BM_EXT_SHIFT-9)))
#if DRBD_MAX_SECTORS_BM < DRBD_MAX_SECTORS_32
#define DRBD_MAX_SECTORS      DRBD_MAX_SECTORS_BM
#define DRBD_MAX_SECTORS_FLEX DRBD_MAX_SECTORS_BM
#elif !defined(CONFIG_LBDAF) && BITS_PER_LONG == 32
#define DRBD_MAX_SECTORS      DRBD_MAX_SECTORS_32
#define DRBD_MAX_SECTORS_FLEX DRBD_MAX_SECTORS_32
#else
#define DRBD_MAX_SECTORS      DRBD_MAX_SECTORS_BM
#if BITS_PER_LONG == 32
#define DRBD_MAX_SECTORS_FLEX BM_BIT_TO_SECT(0xffff7fff)
#else
#define DRBD_MAX_SECTORS_FLEX (1UL << 51)
#endif
#endif

#define HT_SHIFT 8
#define DRBD_MAX_BIO_SIZE (1U<<(9+HT_SHIFT))
#define DRBD_MAX_BIO_SIZE_SAFE (1 << 12)       

#define DRBD_MAX_SIZE_H80_PACKET (1 << 15) 

#define APP_R_HSIZE 15

extern int  drbd_bm_init(struct drbd_conf *mdev);
extern int  drbd_bm_resize(struct drbd_conf *mdev, sector_t sectors, int set_new_bits);
extern void drbd_bm_cleanup(struct drbd_conf *mdev);
extern void drbd_bm_set_all(struct drbd_conf *mdev);
extern void drbd_bm_clear_all(struct drbd_conf *mdev);
extern int  drbd_bm_set_bits(
		struct drbd_conf *mdev, unsigned long s, unsigned long e);
extern int  drbd_bm_clear_bits(
		struct drbd_conf *mdev, unsigned long s, unsigned long e);
extern int drbd_bm_count_bits(
	struct drbd_conf *mdev, const unsigned long s, const unsigned long e);
extern void _drbd_bm_set_bits(struct drbd_conf *mdev,
		const unsigned long s, const unsigned long e);
extern int  drbd_bm_test_bit(struct drbd_conf *mdev, unsigned long bitnr);
extern int  drbd_bm_e_weight(struct drbd_conf *mdev, unsigned long enr);
extern int  drbd_bm_write_page(struct drbd_conf *mdev, unsigned int idx) __must_hold(local);
extern int  drbd_bm_read(struct drbd_conf *mdev) __must_hold(local);
extern int  drbd_bm_write(struct drbd_conf *mdev) __must_hold(local);
extern unsigned long drbd_bm_ALe_set_all(struct drbd_conf *mdev,
		unsigned long al_enr);
extern size_t	     drbd_bm_words(struct drbd_conf *mdev);
extern unsigned long drbd_bm_bits(struct drbd_conf *mdev);
extern sector_t      drbd_bm_capacity(struct drbd_conf *mdev);

#define DRBD_END_OF_BITMAP	(~(unsigned long)0)
extern unsigned long drbd_bm_find_next(struct drbd_conf *mdev, unsigned long bm_fo);
extern unsigned long _drbd_bm_find_next(struct drbd_conf *mdev, unsigned long bm_fo);
extern unsigned long _drbd_bm_find_next_zero(struct drbd_conf *mdev, unsigned long bm_fo);
extern unsigned long _drbd_bm_total_weight(struct drbd_conf *mdev);
extern unsigned long drbd_bm_total_weight(struct drbd_conf *mdev);
extern int drbd_bm_rs_done(struct drbd_conf *mdev);
extern void drbd_bm_merge_lel(struct drbd_conf *mdev, size_t offset,
		size_t number, unsigned long *buffer);
extern void drbd_bm_get_lel(struct drbd_conf *mdev, size_t offset,
		size_t number, unsigned long *buffer);

extern void drbd_bm_lock(struct drbd_conf *mdev, char *why, enum bm_flag flags);
extern void drbd_bm_unlock(struct drbd_conf *mdev);

extern struct kmem_cache *drbd_request_cache;
extern struct kmem_cache *drbd_ee_cache;	
extern struct kmem_cache *drbd_bm_ext_cache;	
extern struct kmem_cache *drbd_al_ext_cache;	
extern mempool_t *drbd_request_mempool;
extern mempool_t *drbd_ee_mempool;

extern struct page *drbd_pp_pool; 
extern spinlock_t   drbd_pp_lock;
extern int	    drbd_pp_vacant;
extern wait_queue_head_t drbd_pp_wait;

extern rwlock_t global_state_lock;

extern struct drbd_conf *drbd_new_device(unsigned int minor);
extern void drbd_free_mdev(struct drbd_conf *mdev);

extern int proc_details;

extern void drbd_make_request(struct request_queue *q, struct bio *bio);
extern int drbd_read_remote(struct drbd_conf *mdev, struct drbd_request *req);
extern int drbd_merge_bvec(struct request_queue *q, struct bvec_merge_data *bvm, struct bio_vec *bvec);
extern int is_valid_ar_handle(struct drbd_request *, sector_t);


extern void drbd_suspend_io(struct drbd_conf *mdev);
extern void drbd_resume_io(struct drbd_conf *mdev);
extern char *ppsize(char *buf, unsigned long long size);
extern sector_t drbd_new_dev_size(struct drbd_conf *, struct drbd_backing_dev *, int);
enum determine_dev_size { dev_size_error = -1, unchanged = 0, shrunk = 1, grew = 2 };
extern enum determine_dev_size drbd_determine_dev_size(struct drbd_conf *, enum dds_flags) __must_hold(local);
extern void resync_after_online_grow(struct drbd_conf *);
extern void drbd_reconsider_max_bio_size(struct drbd_conf *mdev);
extern enum drbd_state_rv drbd_set_role(struct drbd_conf *mdev,
					enum drbd_role new_role,
					int force);
extern enum drbd_disk_state drbd_try_outdate_peer(struct drbd_conf *mdev);
extern void drbd_try_outdate_peer_async(struct drbd_conf *mdev);
extern int drbd_khelper(struct drbd_conf *mdev, char *cmd);

extern int drbd_worker(struct drbd_thread *thi);
extern int drbd_alter_sa(struct drbd_conf *mdev, int na);
extern void drbd_start_resync(struct drbd_conf *mdev, enum drbd_conns side);
extern void resume_next_sg(struct drbd_conf *mdev);
extern void suspend_other_sg(struct drbd_conf *mdev);
extern int drbd_resync_finished(struct drbd_conf *mdev);
extern int drbd_md_sync_page_io(struct drbd_conf *mdev,
		struct drbd_backing_dev *bdev, sector_t sector, int rw);
extern void drbd_ov_oos_found(struct drbd_conf*, sector_t, int);
extern void drbd_rs_controller_reset(struct drbd_conf *mdev);

static inline void ov_oos_print(struct drbd_conf *mdev)
{
	if (mdev->ov_last_oos_size) {
		dev_err(DEV, "Out of sync: start=%llu, size=%lu (sectors)\n",
		     (unsigned long long)mdev->ov_last_oos_start,
		     (unsigned long)mdev->ov_last_oos_size);
	}
	mdev->ov_last_oos_size=0;
}


extern void drbd_csum_bio(struct drbd_conf *, struct crypto_hash *, struct bio *, void *);
extern void drbd_csum_ee(struct drbd_conf *, struct crypto_hash *, struct drbd_epoch_entry *, void *);
extern int w_req_cancel_conflict(struct drbd_conf *, struct drbd_work *, int);
extern int w_read_retry_remote(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_end_data_req(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_end_rsdata_req(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_end_csum_rs_req(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_end_ov_reply(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_end_ov_req(struct drbd_conf *, struct drbd_work *, int);
extern int w_ov_finished(struct drbd_conf *, struct drbd_work *, int);
extern int w_resync_timer(struct drbd_conf *, struct drbd_work *, int);
extern int w_resume_next_sg(struct drbd_conf *, struct drbd_work *, int);
extern int w_send_write_hint(struct drbd_conf *, struct drbd_work *, int);
extern int w_send_dblock(struct drbd_conf *, struct drbd_work *, int);
extern int w_send_barrier(struct drbd_conf *, struct drbd_work *, int);
extern int w_send_read_req(struct drbd_conf *, struct drbd_work *, int);
extern int w_prev_work_done(struct drbd_conf *, struct drbd_work *, int);
extern int w_e_reissue(struct drbd_conf *, struct drbd_work *, int);
extern int w_restart_disk_io(struct drbd_conf *, struct drbd_work *, int);
extern int w_send_oos(struct drbd_conf *, struct drbd_work *, int);
extern int w_start_resync(struct drbd_conf *, struct drbd_work *, int);

extern void resync_timer_fn(unsigned long data);
extern void start_resync_timer_fn(unsigned long data);

extern int drbd_rs_should_slow_down(struct drbd_conf *mdev, sector_t sector);
extern int drbd_submit_ee(struct drbd_conf *mdev, struct drbd_epoch_entry *e,
		const unsigned rw, const int fault_type);
extern int drbd_release_ee(struct drbd_conf *mdev, struct list_head *list);
extern struct drbd_epoch_entry *drbd_alloc_ee(struct drbd_conf *mdev,
					    u64 id,
					    sector_t sector,
					    unsigned int data_size,
					    gfp_t gfp_mask) __must_hold(local);
extern void drbd_free_some_ee(struct drbd_conf *mdev, struct drbd_epoch_entry *e,
		int is_net);
#define drbd_free_ee(m,e)	drbd_free_some_ee(m, e, 0)
#define drbd_free_net_ee(m,e)	drbd_free_some_ee(m, e, 1)
extern void drbd_wait_ee_list_empty(struct drbd_conf *mdev,
		struct list_head *head);
extern void _drbd_wait_ee_list_empty(struct drbd_conf *mdev,
		struct list_head *head);
extern void drbd_set_recv_tcq(struct drbd_conf *mdev, int tcq_enabled);
extern void _drbd_clear_done_ee(struct drbd_conf *mdev, struct list_head *to_be_freed);
extern void drbd_flush_workqueue(struct drbd_conf *mdev);
extern void drbd_free_tl_hash(struct drbd_conf *mdev);

static inline int drbd_setsockopt(struct socket *sock, int level, int optname,
			char __user *optval, int optlen)
{
	int err;
	if (level == SOL_SOCKET)
		err = sock_setsockopt(sock, level, optname, optval, optlen);
	else
		err = sock->ops->setsockopt(sock, level, optname, optval,
					    optlen);
	return err;
}

static inline void drbd_tcp_cork(struct socket *sock)
{
	int __user val = 1;
	(void) drbd_setsockopt(sock, SOL_TCP, TCP_CORK,
			(char __user *)&val, sizeof(val));
}

static inline void drbd_tcp_uncork(struct socket *sock)
{
	int __user val = 0;
	(void) drbd_setsockopt(sock, SOL_TCP, TCP_CORK,
			(char __user *)&val, sizeof(val));
}

static inline void drbd_tcp_nodelay(struct socket *sock)
{
	int __user val = 1;
	(void) drbd_setsockopt(sock, SOL_TCP, TCP_NODELAY,
			(char __user *)&val, sizeof(val));
}

static inline void drbd_tcp_quickack(struct socket *sock)
{
	int __user val = 2;
	(void) drbd_setsockopt(sock, SOL_TCP, TCP_QUICKACK,
			(char __user *)&val, sizeof(val));
}

void drbd_bump_write_ordering(struct drbd_conf *mdev, enum write_ordering_e wo);

extern struct proc_dir_entry *drbd_proc;
extern const struct file_operations drbd_proc_fops;
extern const char *drbd_conn_str(enum drbd_conns s);
extern const char *drbd_role_str(enum drbd_role s);

extern void drbd_al_begin_io(struct drbd_conf *mdev, sector_t sector);
extern void drbd_al_complete_io(struct drbd_conf *mdev, sector_t sector);
extern void drbd_rs_complete_io(struct drbd_conf *mdev, sector_t sector);
extern int drbd_rs_begin_io(struct drbd_conf *mdev, sector_t sector);
extern int drbd_try_rs_begin_io(struct drbd_conf *mdev, sector_t sector);
extern void drbd_rs_cancel_all(struct drbd_conf *mdev);
extern int drbd_rs_del_all(struct drbd_conf *mdev);
extern void drbd_rs_failed_io(struct drbd_conf *mdev,
		sector_t sector, int size);
extern int drbd_al_read_log(struct drbd_conf *mdev, struct drbd_backing_dev *);
extern void drbd_advance_rs_marks(struct drbd_conf *mdev, unsigned long still_to_go);
extern void __drbd_set_in_sync(struct drbd_conf *mdev, sector_t sector,
		int size, const char *file, const unsigned int line);
#define drbd_set_in_sync(mdev, sector, size) \
	__drbd_set_in_sync(mdev, sector, size, __FILE__, __LINE__)
extern int __drbd_set_out_of_sync(struct drbd_conf *mdev, sector_t sector,
		int size, const char *file, const unsigned int line);
#define drbd_set_out_of_sync(mdev, sector, size) \
	__drbd_set_out_of_sync(mdev, sector, size, __FILE__, __LINE__)
extern void drbd_al_apply_to_bm(struct drbd_conf *mdev);
extern void drbd_al_shrink(struct drbd_conf *mdev);



void drbd_nl_cleanup(void);
int __init drbd_nl_init(void);
void drbd_bcast_state(struct drbd_conf *mdev, union drbd_state);
void drbd_bcast_sync_progress(struct drbd_conf *mdev);
void drbd_bcast_ee(struct drbd_conf *mdev,
		const char *reason, const int dgs,
		const char* seen_hash, const char* calc_hash,
		const struct drbd_epoch_entry* e);


#define role_MASK R_MASK
#define peer_MASK R_MASK
#define disk_MASK D_MASK
#define pdsk_MASK D_MASK
#define conn_MASK C_MASK
#define susp_MASK 1
#define user_isp_MASK 1
#define aftr_isp_MASK 1
#define susp_nod_MASK 1
#define susp_fen_MASK 1

#define NS(T, S) \
	({ union drbd_state mask; mask.i = 0; mask.T = T##_MASK; mask; }), \
	({ union drbd_state val; val.i = 0; val.T = (S); val; })
#define NS2(T1, S1, T2, S2) \
	({ union drbd_state mask; mask.i = 0; mask.T1 = T1##_MASK; \
	  mask.T2 = T2##_MASK; mask; }), \
	({ union drbd_state val; val.i = 0; val.T1 = (S1); \
	  val.T2 = (S2); val; })
#define NS3(T1, S1, T2, S2, T3, S3) \
	({ union drbd_state mask; mask.i = 0; mask.T1 = T1##_MASK; \
	  mask.T2 = T2##_MASK; mask.T3 = T3##_MASK; mask; }), \
	({ union drbd_state val;  val.i = 0; val.T1 = (S1); \
	  val.T2 = (S2); val.T3 = (S3); val; })

#define _NS(D, T, S) \
	D, ({ union drbd_state __ns; __ns.i = D->state.i; __ns.T = (S); __ns; })
#define _NS2(D, T1, S1, T2, S2) \
	D, ({ union drbd_state __ns; __ns.i = D->state.i; __ns.T1 = (S1); \
	__ns.T2 = (S2); __ns; })
#define _NS3(D, T1, S1, T2, S2, T3, S3) \
	D, ({ union drbd_state __ns; __ns.i = D->state.i; __ns.T1 = (S1); \
	__ns.T2 = (S2); __ns.T3 = (S3); __ns; })


static inline struct page *page_chain_next(struct page *page)
{
	return (struct page *)page_private(page);
}
#define page_chain_for_each(page) \
	for (; page && ({ prefetch(page_chain_next(page)); 1; }); \
			page = page_chain_next(page))
#define page_chain_for_each_safe(page, n) \
	for (; page && ({ n = page_chain_next(page); 1; }); page = n)

static inline int drbd_bio_has_active_page(struct bio *bio)
{
	struct bio_vec *bvec;
	int i;

	__bio_for_each_segment(bvec, bio, i, 0) {
		if (page_count(bvec->bv_page) > 1)
			return 1;
	}

	return 0;
}

static inline int drbd_ee_has_active_page(struct drbd_epoch_entry *e)
{
	struct page *page = e->pages;
	page_chain_for_each(page) {
		if (page_count(page) > 1)
			return 1;
	}
	return 0;
}


static inline void drbd_state_lock(struct drbd_conf *mdev)
{
	wait_event(mdev->misc_wait,
		   !test_and_set_bit(CLUSTER_ST_CHANGE, &mdev->flags));
}

static inline void drbd_state_unlock(struct drbd_conf *mdev)
{
	clear_bit(CLUSTER_ST_CHANGE, &mdev->flags);
	wake_up(&mdev->misc_wait);
}

static inline enum drbd_state_rv
_drbd_set_state(struct drbd_conf *mdev, union drbd_state ns,
		enum chg_state_flags flags, struct completion *done)
{
	enum drbd_state_rv rv;

	read_lock(&global_state_lock);
	rv = __drbd_set_state(mdev, ns, flags, done);
	read_unlock(&global_state_lock);

	return rv;
}

static inline int drbd_request_state(struct drbd_conf *mdev,
				     union drbd_state mask,
				     union drbd_state val)
{
	return _drbd_request_state(mdev, mask, val, CS_VERBOSE + CS_ORDERED);
}

#define __drbd_chk_io_error(m,f) __drbd_chk_io_error_(m,f, __func__)
static inline void __drbd_chk_io_error_(struct drbd_conf *mdev, int forcedetach, const char *where)
{
	switch (mdev->ldev->dc.on_io_error) {
	case EP_PASS_ON:
		if (!forcedetach) {
			if (__ratelimit(&drbd_ratelimit_state))
				dev_err(DEV, "Local IO failed in %s.\n", where);
			if (mdev->state.disk > D_INCONSISTENT)
				_drbd_set_state(_NS(mdev, disk, D_INCONSISTENT), CS_HARD, NULL);
			break;
		}
		
	case EP_DETACH:
	case EP_CALL_HELPER:
		set_bit(WAS_IO_ERROR, &mdev->flags);
		if (mdev->state.disk > D_FAILED) {
			_drbd_set_state(_NS(mdev, disk, D_FAILED), CS_HARD, NULL);
			dev_err(DEV,
				"Local IO failed in %s. Detaching...\n", where);
		}
		break;
	}
}

#define drbd_chk_io_error(m,e,f) drbd_chk_io_error_(m,e,f, __func__)
static inline void drbd_chk_io_error_(struct drbd_conf *mdev,
	int error, int forcedetach, const char *where)
{
	if (error) {
		unsigned long flags;
		spin_lock_irqsave(&mdev->req_lock, flags);
		__drbd_chk_io_error_(mdev, forcedetach, where);
		spin_unlock_irqrestore(&mdev->req_lock, flags);
	}
}


static inline sector_t drbd_md_first_sector(struct drbd_backing_dev *bdev)
{
	switch (bdev->dc.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		return bdev->md.md_offset + bdev->md.bm_offset;
	case DRBD_MD_INDEX_FLEX_EXT:
	default:
		return bdev->md.md_offset;
	}
}

static inline sector_t drbd_md_last_sector(struct drbd_backing_dev *bdev)
{
	switch (bdev->dc.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		return bdev->md.md_offset + MD_AL_OFFSET - 1;
	case DRBD_MD_INDEX_FLEX_EXT:
	default:
		return bdev->md.md_offset + bdev->md.md_size_sect;
	}
}

static inline sector_t drbd_get_capacity(struct block_device *bdev)
{
	
	return bdev ? i_size_read(bdev->bd_inode) >> 9 : 0;
}

static inline sector_t drbd_get_max_capacity(struct drbd_backing_dev *bdev)
{
	sector_t s;
	switch (bdev->dc.meta_dev_idx) {
	case DRBD_MD_INDEX_INTERNAL:
	case DRBD_MD_INDEX_FLEX_INT:
		s = drbd_get_capacity(bdev->backing_bdev)
			? min_t(sector_t, DRBD_MAX_SECTORS_FLEX,
					drbd_md_first_sector(bdev))
			: 0;
		break;
	case DRBD_MD_INDEX_FLEX_EXT:
		s = min_t(sector_t, DRBD_MAX_SECTORS_FLEX,
				drbd_get_capacity(bdev->backing_bdev));
		
		s = min_t(sector_t, s,
			BM_EXT_TO_SECT(bdev->md.md_size_sect
				     - bdev->md.bm_offset));
		break;
	default:
		s = min_t(sector_t, DRBD_MAX_SECTORS,
				drbd_get_capacity(bdev->backing_bdev));
	}
	return s;
}

static inline sector_t drbd_md_ss__(struct drbd_conf *mdev,
				    struct drbd_backing_dev *bdev)
{
	switch (bdev->dc.meta_dev_idx) {
	default: 
		return MD_RESERVED_SECT * bdev->dc.meta_dev_idx;
	case DRBD_MD_INDEX_INTERNAL:
		
	case DRBD_MD_INDEX_FLEX_INT:
		if (!bdev->backing_bdev) {
			if (__ratelimit(&drbd_ratelimit_state)) {
				dev_err(DEV, "bdev->backing_bdev==NULL\n");
				dump_stack();
			}
			return 0;
		}
		return (drbd_get_capacity(bdev->backing_bdev) & ~7ULL)
			- MD_AL_OFFSET;
	case DRBD_MD_INDEX_FLEX_EXT:
		return 0;
	}
}

static inline void
drbd_queue_work_front(struct drbd_work_queue *q, struct drbd_work *w)
{
	unsigned long flags;
	spin_lock_irqsave(&q->q_lock, flags);
	list_add(&w->list, &q->q);
	up(&q->s); 
	spin_unlock_irqrestore(&q->q_lock, flags);
}

static inline void
drbd_queue_work(struct drbd_work_queue *q, struct drbd_work *w)
{
	unsigned long flags;
	spin_lock_irqsave(&q->q_lock, flags);
	list_add_tail(&w->list, &q->q);
	up(&q->s); 
	spin_unlock_irqrestore(&q->q_lock, flags);
}

static inline void wake_asender(struct drbd_conf *mdev)
{
	if (test_bit(SIGNAL_ASENDER, &mdev->flags))
		force_sig(DRBD_SIG, mdev->asender.task);
}

static inline void request_ping(struct drbd_conf *mdev)
{
	set_bit(SEND_PING, &mdev->flags);
	wake_asender(mdev);
}

static inline int drbd_send_short_cmd(struct drbd_conf *mdev,
	enum drbd_packets cmd)
{
	struct p_header80 h;
	return drbd_send_cmd(mdev, USE_DATA_SOCKET, cmd, &h, sizeof(h));
}

static inline int drbd_send_ping(struct drbd_conf *mdev)
{
	struct p_header80 h;
	return drbd_send_cmd(mdev, USE_META_SOCKET, P_PING, &h, sizeof(h));
}

static inline int drbd_send_ping_ack(struct drbd_conf *mdev)
{
	struct p_header80 h;
	return drbd_send_cmd(mdev, USE_META_SOCKET, P_PING_ACK, &h, sizeof(h));
}

static inline void drbd_thread_stop(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, false, true);
}

static inline void drbd_thread_stop_nowait(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, false, false);
}

static inline void drbd_thread_restart_nowait(struct drbd_thread *thi)
{
	_drbd_thread_stop(thi, true, false);
}

static inline void inc_ap_pending(struct drbd_conf *mdev)
{
	atomic_inc(&mdev->ap_pending_cnt);
}

#define ERR_IF_CNT_IS_NEGATIVE(which)				\
	if (atomic_read(&mdev->which) < 0)			\
		dev_err(DEV, "in %s:%d: " #which " = %d < 0 !\n",	\
		    __func__ , __LINE__ ,			\
		    atomic_read(&mdev->which))

#define dec_ap_pending(mdev)	do {				\
	typecheck(struct drbd_conf *, mdev);			\
	if (atomic_dec_and_test(&mdev->ap_pending_cnt))		\
		wake_up(&mdev->misc_wait);			\
	ERR_IF_CNT_IS_NEGATIVE(ap_pending_cnt); } while (0)

static inline void inc_rs_pending(struct drbd_conf *mdev)
{
	atomic_inc(&mdev->rs_pending_cnt);
}

#define dec_rs_pending(mdev)	do {				\
	typecheck(struct drbd_conf *, mdev);			\
	atomic_dec(&mdev->rs_pending_cnt);			\
	ERR_IF_CNT_IS_NEGATIVE(rs_pending_cnt); } while (0)

static inline void inc_unacked(struct drbd_conf *mdev)
{
	atomic_inc(&mdev->unacked_cnt);
}

#define dec_unacked(mdev)	do {				\
	typecheck(struct drbd_conf *, mdev);			\
	atomic_dec(&mdev->unacked_cnt);				\
	ERR_IF_CNT_IS_NEGATIVE(unacked_cnt); } while (0)

#define sub_unacked(mdev, n)	do {				\
	typecheck(struct drbd_conf *, mdev);			\
	atomic_sub(n, &mdev->unacked_cnt);			\
	ERR_IF_CNT_IS_NEGATIVE(unacked_cnt); } while (0)


static inline void put_net_conf(struct drbd_conf *mdev)
{
	if (atomic_dec_and_test(&mdev->net_cnt))
		wake_up(&mdev->net_cnt_wait);
}

static inline int get_net_conf(struct drbd_conf *mdev)
{
	int have_net_conf;

	atomic_inc(&mdev->net_cnt);
	have_net_conf = mdev->state.conn >= C_UNCONNECTED;
	if (!have_net_conf)
		put_net_conf(mdev);
	return have_net_conf;
}

#define get_ldev(M) __cond_lock(local, _get_ldev_if_state(M,D_INCONSISTENT))
#define get_ldev_if_state(M,MINS) __cond_lock(local, _get_ldev_if_state(M,MINS))

static inline void put_ldev(struct drbd_conf *mdev)
{
	int i = atomic_dec_return(&mdev->local_cnt);


	__release(local);
	D_ASSERT(i >= 0);
	if (i == 0) {
		if (mdev->state.disk == D_DISKLESS)
			
			drbd_ldev_destroy(mdev);
		if (mdev->state.disk == D_FAILED)
			
			drbd_go_diskless(mdev);
		wake_up(&mdev->misc_wait);
	}
}

#ifndef __CHECKER__
static inline int _get_ldev_if_state(struct drbd_conf *mdev, enum drbd_disk_state mins)
{
	int io_allowed;

	
	if (mdev->state.disk == D_DISKLESS)
		return 0;

	atomic_inc(&mdev->local_cnt);
	io_allowed = (mdev->state.disk >= mins);
	if (!io_allowed)
		put_ldev(mdev);
	return io_allowed;
}
#else
extern int _get_ldev_if_state(struct drbd_conf *mdev, enum drbd_disk_state mins);
#endif

static inline void drbd_get_syncer_progress(struct drbd_conf *mdev,
		unsigned long *bits_left, unsigned int *per_mil_done)
{
	typecheck(unsigned long, mdev->rs_total);


	if (mdev->state.conn == C_VERIFY_S || mdev->state.conn == C_VERIFY_T)
		*bits_left = mdev->ov_left;
	else
		*bits_left = drbd_bm_total_weight(mdev) - mdev->rs_failed;
	if (*bits_left > mdev->rs_total) {
		smp_rmb();
		dev_warn(DEV, "cs:%s rs_left=%lu > rs_total=%lu (rs_failed %lu)\n",
				drbd_conn_str(mdev->state.conn),
				*bits_left, mdev->rs_total, mdev->rs_failed);
		*per_mil_done = 0;
	} else {
		unsigned int shift = mdev->rs_total >= (1ULL << 32) ? 16 : 10;
		unsigned long left = *bits_left >> shift;
		unsigned long total = 1UL + (mdev->rs_total >> shift);
		unsigned long tmp = 1000UL - left * 1000UL/total;
		*per_mil_done = tmp;
	}
}


static inline int drbd_get_max_buffers(struct drbd_conf *mdev)
{
	int mxb = 1000000; 
	if (get_net_conf(mdev)) {
		mxb = mdev->net_conf->max_buffers;
		put_net_conf(mdev);
	}
	return mxb;
}

static inline int drbd_state_is_stable(struct drbd_conf *mdev)
{
	union drbd_state s = mdev->state;


	switch ((enum drbd_conns)s.conn) {
	
	case C_STANDALONE:
	case C_WF_CONNECTION:
	
	case C_CONNECTED:
	case C_SYNC_SOURCE:
	case C_SYNC_TARGET:
	case C_VERIFY_S:
	case C_VERIFY_T:
	case C_PAUSED_SYNC_S:
	case C_PAUSED_SYNC_T:
	case C_AHEAD:
	case C_BEHIND:
		
	case C_DISCONNECTING:
	case C_UNCONNECTED:
	case C_TIMEOUT:
	case C_BROKEN_PIPE:
	case C_NETWORK_FAILURE:
	case C_PROTOCOL_ERROR:
	case C_TEAR_DOWN:
	case C_WF_REPORT_PARAMS:
	case C_STARTING_SYNC_S:
	case C_STARTING_SYNC_T:
		break;

		
	case C_WF_BITMAP_S:
		if (mdev->agreed_pro_version < 96)
			return 0;
		break;

		
	case C_WF_BITMAP_T:
	case C_WF_SYNC_UUID:
	case C_MASK:
		
		return 0;
	}

	switch ((enum drbd_disk_state)s.disk) {
	case D_DISKLESS:
	case D_INCONSISTENT:
	case D_OUTDATED:
	case D_CONSISTENT:
	case D_UP_TO_DATE:
		
		break;

	
	case D_ATTACHING:
	case D_FAILED:
	case D_NEGOTIATING:
	case D_UNKNOWN:
	case D_MASK:
		
		return 0;
	}

	return 1;
}

static inline int is_susp(union drbd_state s)
{
	return s.susp || s.susp_nod || s.susp_fen;
}

static inline bool may_inc_ap_bio(struct drbd_conf *mdev)
{
	int mxb = drbd_get_max_buffers(mdev);

	if (is_susp(mdev->state))
		return false;
	if (test_bit(SUSPEND_IO, &mdev->flags))
		return false;


	
	if (!drbd_state_is_stable(mdev))
		return false;

	if (atomic_read(&mdev->ap_bio_cnt) > mxb)
		return false;
	if (test_bit(BITMAP_IO, &mdev->flags))
		return false;
	return true;
}

static inline bool inc_ap_bio_cond(struct drbd_conf *mdev, int count)
{
	bool rv = false;

	spin_lock_irq(&mdev->req_lock);
	rv = may_inc_ap_bio(mdev);
	if (rv)
		atomic_add(count, &mdev->ap_bio_cnt);
	spin_unlock_irq(&mdev->req_lock);

	return rv;
}

static inline void inc_ap_bio(struct drbd_conf *mdev, int count)
{

	wait_event(mdev->misc_wait, inc_ap_bio_cond(mdev, count));
}

static inline void dec_ap_bio(struct drbd_conf *mdev)
{
	int mxb = drbd_get_max_buffers(mdev);
	int ap_bio = atomic_dec_return(&mdev->ap_bio_cnt);

	D_ASSERT(ap_bio >= 0);
	if (ap_bio < mxb)
		wake_up(&mdev->misc_wait);
	if (ap_bio == 0 && test_bit(BITMAP_IO, &mdev->flags)) {
		if (!test_and_set_bit(BITMAP_IO_QUEUED, &mdev->flags))
			drbd_queue_work(&mdev->data.work, &mdev->bm_io_work.w);
	}
}

static inline int drbd_set_ed_uuid(struct drbd_conf *mdev, u64 val)
{
	int changed = mdev->ed_uuid != val;
	mdev->ed_uuid = val;
	return changed;
}

static inline int seq_cmp(u32 a, u32 b)
{
	return (s32)(a) - (s32)(b);
}
#define seq_lt(a, b) (seq_cmp((a), (b)) < 0)
#define seq_gt(a, b) (seq_cmp((a), (b)) > 0)
#define seq_ge(a, b) (seq_cmp((a), (b)) >= 0)
#define seq_le(a, b) (seq_cmp((a), (b)) <= 0)
#define seq_max(a, b) ((u32)(seq_gt((a), (b)) ? (a) : (b)))

static inline void update_peer_seq(struct drbd_conf *mdev, unsigned int new_seq)
{
	unsigned int m;
	spin_lock(&mdev->peer_seq_lock);
	m = seq_max(mdev->peer_seq, new_seq);
	mdev->peer_seq = m;
	spin_unlock(&mdev->peer_seq_lock);
	if (m == new_seq)
		wake_up(&mdev->seq_wait);
}

static inline void drbd_update_congested(struct drbd_conf *mdev)
{
	struct sock *sk = mdev->data.socket->sk;
	if (sk->sk_wmem_queued > sk->sk_sndbuf * 4 / 5)
		set_bit(NET_CONGESTED, &mdev->flags);
}

static inline int drbd_queue_order_type(struct drbd_conf *mdev)
{
#ifndef QUEUE_ORDERED_NONE
#define QUEUE_ORDERED_NONE 0
#endif
	return QUEUE_ORDERED_NONE;
}

static inline void drbd_md_flush(struct drbd_conf *mdev)
{
	int r;

	if (test_bit(MD_NO_FUA, &mdev->flags))
		return;

	r = blkdev_issue_flush(mdev->ldev->md_bdev, GFP_KERNEL, NULL);
	if (r) {
		set_bit(MD_NO_FUA, &mdev->flags);
		dev_err(DEV, "meta data flush failed with status %d, disabling md-flushes\n", r);
	}
}

#endif
