/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010 QLogic Corporation.
 * All rights reserved.
 * Copyright (c) 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef QIB_VERBS_H
#define QIB_VERBS_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kref.h>
#include <linux/workqueue.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_user_verbs.h>

struct qib_ctxtdata;
struct qib_pportdata;
struct qib_devdata;
struct qib_verbs_txreq;

#define QIB_MAX_RDMA_ATOMIC     16
#define QIB_GUIDS_PER_PORT	5

#define QPN_MAX                 (1 << 24)
#define QPNMAP_ENTRIES          (QPN_MAX / PAGE_SIZE / BITS_PER_BYTE)

#define QIB_UVERBS_ABI_VERSION       2

#define IB_CQ_NONE      (IB_CQ_NEXT_COMP + 1)

#define IB_SEQ_NAK	(3 << 29)

#define IB_RNR_NAK                      0x20
#define IB_NAK_PSN_ERROR                0x60
#define IB_NAK_INVALID_REQUEST          0x61
#define IB_NAK_REMOTE_ACCESS_ERROR      0x62
#define IB_NAK_REMOTE_OPERATIONAL_ERROR 0x63
#define IB_NAK_INVALID_RD_REQUEST       0x64

#define QIB_POST_SEND_OK                0x01
#define QIB_POST_RECV_OK                0x02
#define QIB_PROCESS_RECV_OK             0x04
#define QIB_PROCESS_SEND_OK             0x08
#define QIB_PROCESS_NEXT_SEND_OK        0x10
#define QIB_FLUSH_SEND			0x20
#define QIB_FLUSH_RECV			0x40
#define QIB_PROCESS_OR_FLUSH_SEND \
	(QIB_PROCESS_SEND_OK | QIB_FLUSH_SEND)

#define IB_PMA_SAMPLE_STATUS_DONE       0x00
#define IB_PMA_SAMPLE_STATUS_STARTED    0x01
#define IB_PMA_SAMPLE_STATUS_RUNNING    0x02

#define IB_PMA_PORT_XMIT_DATA   cpu_to_be16(0x0001)
#define IB_PMA_PORT_RCV_DATA    cpu_to_be16(0x0002)
#define IB_PMA_PORT_XMIT_PKTS   cpu_to_be16(0x0003)
#define IB_PMA_PORT_RCV_PKTS    cpu_to_be16(0x0004)
#define IB_PMA_PORT_XMIT_WAIT   cpu_to_be16(0x0005)

#define QIB_VENDOR_IPG		cpu_to_be16(0xFFA0)

#define IB_BTH_REQ_ACK		(1 << 31)
#define IB_BTH_SOLICITED	(1 << 23)
#define IB_BTH_MIG_REQ		(1 << 22)

#define IB_PORT_OTHER_LOCAL_CHANGES_SUP (1 << 26)

#define IB_GRH_VERSION		6
#define IB_GRH_VERSION_MASK	0xF
#define IB_GRH_VERSION_SHIFT	28
#define IB_GRH_TCLASS_MASK	0xFF
#define IB_GRH_TCLASS_SHIFT	20
#define IB_GRH_FLOW_MASK	0xFFFFF
#define IB_GRH_FLOW_SHIFT	0
#define IB_GRH_NEXT_HDR		0x1B

#define IB_DEFAULT_GID_PREFIX	cpu_to_be64(0xfe80000000000000ULL)

#define IB_VL_VL0       1
#define IB_VL_VL0_1     2
#define IB_VL_VL0_3     3
#define IB_VL_VL0_7     4
#define IB_VL_VL0_14    5

static inline int qib_num_vls(int vls)
{
	switch (vls) {
	default:
	case IB_VL_VL0:
		return 1;
	case IB_VL_VL0_1:
		return 2;
	case IB_VL_VL0_3:
		return 4;
	case IB_VL_VL0_7:
		return 8;
	case IB_VL_VL0_14:
		return 15;
	}
}

struct ib_reth {
	__be64 vaddr;
	__be32 rkey;
	__be32 length;
} __attribute__ ((packed));

struct ib_atomic_eth {
	__be32 vaddr[2];        
	__be32 rkey;
	__be64 swap_data;
	__be64 compare_data;
} __attribute__ ((packed));

struct qib_other_headers {
	__be32 bth[3];
	union {
		struct {
			__be32 deth[2];
			__be32 imm_data;
		} ud;
		struct {
			struct ib_reth reth;
			__be32 imm_data;
		} rc;
		struct {
			__be32 aeth;
			__be32 atomic_ack_eth[2];
		} at;
		__be32 imm_data;
		__be32 aeth;
		struct ib_atomic_eth atomic_eth;
	} u;
} __attribute__ ((packed));

struct qib_ib_header {
	__be16 lrh[4];
	union {
		struct {
			struct ib_grh grh;
			struct qib_other_headers oth;
		} l;
		struct qib_other_headers oth;
	} u;
} __attribute__ ((packed));

struct qib_pio_header {
	__le32 pbc[2];
	struct qib_ib_header hdr;
} __attribute__ ((packed));

struct qib_mcast_qp {
	struct list_head list;
	struct qib_qp *qp;
};

struct qib_mcast {
	struct rb_node rb_node;
	union ib_gid mgid;
	struct list_head qp_list;
	wait_queue_head_t wait;
	atomic_t refcount;
	int n_attached;
};

struct qib_pd {
	struct ib_pd ibpd;
	int user;               
};

struct qib_ah {
	struct ib_ah ibah;
	struct ib_ah_attr attr;
	atomic_t refcount;
};

struct qib_mmap_info {
	struct list_head pending_mmaps;
	struct ib_ucontext *context;
	void *obj;
	__u64 offset;
	struct kref ref;
	unsigned size;
};

struct qib_cq_wc {
	u32 head;               
	u32 tail;               
	union {
		
		struct ib_uverbs_wc uqueue[0];
		struct ib_wc kqueue[0];
	};
};

struct qib_cq {
	struct ib_cq ibcq;
	struct work_struct comptask;
	spinlock_t lock; 
	u8 notify;
	u8 triggered;
	struct qib_cq_wc *queue;
	struct qib_mmap_info *ip;
};

struct qib_seg {
	void *vaddr;
	size_t length;
};

#define QIB_SEGSZ     (PAGE_SIZE / sizeof(struct qib_seg))

struct qib_segarray {
	struct qib_seg segs[QIB_SEGSZ];
};

struct qib_mregion {
	struct ib_pd *pd;       
	u64 user_base;          
	u64 iova;               
	size_t length;
	u32 lkey;
	u32 offset;             
	int access_flags;
	u32 max_segs;           
	u32 mapsz;              
	u8  page_shift;         
	atomic_t refcount;
	struct qib_segarray *map[0];    
};

struct qib_sge {
	struct qib_mregion *mr;
	void *vaddr;            
	u32 sge_length;         
	u32 length;             
	u16 m;                  
	u16 n;                  
};

struct qib_mr {
	struct ib_mr ibmr;
	struct ib_umem *umem;
	struct qib_mregion mr;  
};

struct qib_swqe {
	struct ib_send_wr wr;   
	u32 psn;                
	u32 lpsn;               
	u32 ssn;                
	u32 length;             
	struct qib_sge sg_list[0];
};

struct qib_rwqe {
	u64 wr_id;
	u8 num_sge;
	struct ib_sge sg_list[0];
};

struct qib_rwq {
	u32 head;               
	u32 tail;               
	struct qib_rwqe wq[0];
};

struct qib_rq {
	struct qib_rwq *wq;
	spinlock_t lock; 
	u32 size;               
	u8 max_sge;
};

struct qib_srq {
	struct ib_srq ibsrq;
	struct qib_rq rq;
	struct qib_mmap_info *ip;
	
	u32 limit;
};

struct qib_sge_state {
	struct qib_sge *sg_list;      
	struct qib_sge sge;   
	u32 total_len;
	u8 num_sge;
};

struct qib_ack_entry {
	u8 opcode;
	u8 sent;
	u32 psn;
	u32 lpsn;
	union {
		struct qib_sge rdma_sge;
		u64 atomic_data;
	};
};

struct qib_qp {
	struct ib_qp ibqp;
	struct qib_qp *next;            
	struct qib_qp *timer_next;      
	struct list_head iowait;        
	struct list_head rspwait;       
	struct ib_ah_attr remote_ah_attr;
	struct ib_ah_attr alt_ah_attr;
	struct qib_ib_header s_hdr;     
	atomic_t refcount;
	wait_queue_head_t wait;
	wait_queue_head_t wait_dma;
	struct timer_list s_timer;
	struct work_struct s_work;
	struct qib_mmap_info *ip;
	struct qib_sge_state *s_cur_sge;
	struct qib_verbs_txreq *s_tx;
	struct qib_mregion *s_rdma_mr;
	struct qib_sge_state s_sge;     
	struct qib_ack_entry s_ack_queue[QIB_MAX_RDMA_ATOMIC + 1];
	struct qib_sge_state s_ack_rdma_sge;
	struct qib_sge_state s_rdma_read_sge;
	struct qib_sge_state r_sge;     
	spinlock_t r_lock;      
	spinlock_t s_lock;
	atomic_t s_dma_busy;
	u32 s_flags;
	u32 s_cur_size;         
	u32 s_len;              
	u32 s_rdma_read_len;    
	u32 s_next_psn;         
	u32 s_last_psn;         
	u32 s_sending_psn;      
	u32 s_sending_hpsn;     
	u32 s_psn;              
	u32 s_ack_rdma_psn;     
	u32 s_ack_psn;          
	u32 s_rnr_timeout;      
	u32 r_ack_psn;          
	u64 r_wr_id;            
	unsigned long r_aflags;
	u32 r_len;              
	u32 r_rcv_len;          
	u32 r_psn;              
	u32 r_msn;              
	u16 s_hdrwords;         
	u16 s_rdma_ack_cnt;
	u8 state;               
	u8 s_state;             
	u8 s_ack_state;         
	u8 s_nak_state;         
	u8 r_state;             
	u8 r_nak_state;         
	u8 r_min_rnr_timer;     
	u8 r_flags;
	u8 r_max_rd_atomic;     
	u8 r_head_ack_queue;    
	u8 qp_access_flags;
	u8 s_max_sge;           
	u8 s_retry_cnt;         
	u8 s_rnr_retry_cnt;
	u8 s_retry;             
	u8 s_rnr_retry;         
	u8 s_pkey_index;        
	u8 s_alt_pkey_index;    
	u8 s_max_rd_atomic;     
	u8 s_num_rd_atomic;     
	u8 s_tail_ack_queue;    
	u8 s_srate;
	u8 s_draining;
	u8 s_mig_state;
	u8 timeout;             
	u8 alt_timeout;         
	u8 port_num;
	enum ib_mtu path_mtu;
	u32 pmtu;		
	u32 remote_qpn;
	u32 qkey;               
	u32 s_size;             
	u32 s_head;             
	u32 s_tail;             
	u32 s_cur;              
	u32 s_acked;            
	u32 s_last;             
	u32 s_ssn;              
	u32 s_lsn;              
	unsigned long timeout_jiffies;  
	struct qib_swqe *s_wq;  
	struct qib_swqe *s_wqe;
	struct qib_rq r_rq;             
	struct qib_sge r_sg_list[0];    
};

#define QIB_R_WRID_VALID        0
#define QIB_R_REWIND_SGE        1

#define QIB_R_REUSE_SGE 0x01
#define QIB_R_RDMAR_SEQ 0x02
#define QIB_R_RSP_NAK   0x04
#define QIB_R_RSP_SEND  0x08
#define QIB_R_COMM_EST  0x10

#define QIB_S_SIGNAL_REQ_WR	0x0001
#define QIB_S_BUSY		0x0002
#define QIB_S_TIMER		0x0004
#define QIB_S_RESP_PENDING	0x0008
#define QIB_S_ACK_PENDING	0x0010
#define QIB_S_WAIT_FENCE	0x0020
#define QIB_S_WAIT_RDMAR	0x0040
#define QIB_S_WAIT_RNR		0x0080
#define QIB_S_WAIT_SSN_CREDIT	0x0100
#define QIB_S_WAIT_DMA		0x0200
#define QIB_S_WAIT_PIO		0x0400
#define QIB_S_WAIT_TX		0x0800
#define QIB_S_WAIT_DMA_DESC	0x1000
#define QIB_S_WAIT_KMEM		0x2000
#define QIB_S_WAIT_PSN		0x4000
#define QIB_S_WAIT_ACK		0x8000
#define QIB_S_SEND_ONE		0x10000
#define QIB_S_UNLIMITED_CREDIT	0x20000

#define QIB_S_ANY_WAIT_IO (QIB_S_WAIT_PIO | QIB_S_WAIT_TX | \
	QIB_S_WAIT_DMA_DESC | QIB_S_WAIT_KMEM)

#define QIB_S_ANY_WAIT_SEND (QIB_S_WAIT_FENCE | QIB_S_WAIT_RDMAR | \
	QIB_S_WAIT_RNR | QIB_S_WAIT_SSN_CREDIT | QIB_S_WAIT_DMA | \
	QIB_S_WAIT_PSN | QIB_S_WAIT_ACK)

#define QIB_S_ANY_WAIT (QIB_S_ANY_WAIT_IO | QIB_S_ANY_WAIT_SEND)

#define QIB_PSN_CREDIT  16

static inline struct qib_swqe *get_swqe_ptr(struct qib_qp *qp,
					      unsigned n)
{
	return (struct qib_swqe *)((char *)qp->s_wq +
				     (sizeof(struct qib_swqe) +
				      qp->s_max_sge *
				      sizeof(struct qib_sge)) * n);
}

static inline struct qib_rwqe *get_rwqe_ptr(struct qib_rq *rq, unsigned n)
{
	return (struct qib_rwqe *)
		((char *) rq->wq->wq +
		 (sizeof(struct qib_rwqe) +
		  rq->max_sge * sizeof(struct ib_sge)) * n);
}

struct qpn_map {
	void *page;
};

struct qib_qpn_table {
	spinlock_t lock; 
	unsigned flags;         
	u32 last;               
	u32 nmaps;              
	u16 limit;
	u16 mask;
	
	struct qpn_map map[QPNMAP_ENTRIES];
};

struct qib_lkey_table {
	spinlock_t lock; 
	u32 next;               
	u32 gen;                
	u32 max;                
	struct qib_mregion **table;
};

struct qib_opcode_stats {
	u64 n_packets;          
	u64 n_bytes;            
};

struct qib_ibport {
	struct qib_qp *qp0;
	struct qib_qp *qp1;
	struct ib_mad_agent *send_agent;	
	struct qib_ah *sm_ah;
	struct qib_ah *smi_ah;
	struct rb_root mcast_tree;
	spinlock_t lock;		

	
	unsigned long mkey_lease_timeout;
	unsigned long trap_timeout;
	__be64 gid_prefix;      
	__be64 mkey;
	__be64 guids[QIB_GUIDS_PER_PORT	- 1];	
	u64 tid;		
	u64 n_unicast_xmit;     
	u64 n_unicast_rcv;      
	u64 n_multicast_xmit;   
	u64 n_multicast_rcv;    
	u64 z_symbol_error_counter;             
	u64 z_link_error_recovery_counter;      
	u64 z_link_downed_counter;              
	u64 z_port_rcv_errors;                  
	u64 z_port_rcv_remphys_errors;          
	u64 z_port_xmit_discards;               
	u64 z_port_xmit_data;                   
	u64 z_port_rcv_data;                    
	u64 z_port_xmit_packets;                
	u64 z_port_rcv_packets;                 
	u32 z_local_link_integrity_errors;      
	u32 z_excessive_buffer_overrun_errors;  
	u32 z_vl15_dropped;                     
	u32 n_rc_resends;
	u32 n_rc_acks;
	u32 n_rc_qacks;
	u32 n_rc_delayed_comp;
	u32 n_seq_naks;
	u32 n_rdma_seq;
	u32 n_rnr_naks;
	u32 n_other_naks;
	u32 n_loop_pkts;
	u32 n_pkt_drops;
	u32 n_vl15_dropped;
	u32 n_rc_timeouts;
	u32 n_dmawait;
	u32 n_unaligned;
	u32 n_rc_dupreq;
	u32 n_rc_seqnak;
	u32 port_cap_flags;
	u32 pma_sample_start;
	u32 pma_sample_interval;
	__be16 pma_counter_select[5];
	u16 pma_tag;
	u16 pkey_violations;
	u16 qkey_violations;
	u16 mkey_violations;
	u16 mkey_lease_period;
	u16 sm_lid;
	u16 repress_traps;
	u8 sm_sl;
	u8 mkeyprot;
	u8 subnet_timeout;
	u8 vl_high_limit;
	u8 sl_to_vl[16];

	struct qib_opcode_stats opstats[128];
};

struct qib_ibdev {
	struct ib_device ibdev;
	struct list_head pending_mmaps;
	spinlock_t mmap_offset_lock; 
	u32 mmap_offset;
	struct qib_mregion *dma_mr;

	
	struct qib_qpn_table qpn_table;
	struct qib_lkey_table lk_table;
	struct list_head piowait;       
	struct list_head dmawait;	
	struct list_head txwait;        
	struct list_head memwait;       
	struct list_head txreq_free;
	struct timer_list mem_timer;
	struct qib_qp **qp_table;
	struct qib_pio_header *pio_hdrs;
	dma_addr_t pio_hdrs_phys;
	
	spinlock_t pending_lock; 
	u32 qp_table_size; 
	u32 qp_rnd; 
	spinlock_t qpt_lock;

	u32 n_piowait;
	u32 n_txwait;

	u32 n_pds_allocated;    
	spinlock_t n_pds_lock;
	u32 n_ahs_allocated;    
	spinlock_t n_ahs_lock;
	u32 n_cqs_allocated;    
	spinlock_t n_cqs_lock;
	u32 n_qps_allocated;    
	spinlock_t n_qps_lock;
	u32 n_srqs_allocated;   
	spinlock_t n_srqs_lock;
	u32 n_mcast_grps_allocated; 
	spinlock_t n_mcast_grps_lock;
};

struct qib_verbs_counters {
	u64 symbol_error_counter;
	u64 link_error_recovery_counter;
	u64 link_downed_counter;
	u64 port_rcv_errors;
	u64 port_rcv_remphys_errors;
	u64 port_xmit_discards;
	u64 port_xmit_data;
	u64 port_rcv_data;
	u64 port_xmit_packets;
	u64 port_rcv_packets;
	u32 local_link_integrity_errors;
	u32 excessive_buffer_overrun_errors;
	u32 vl15_dropped;
};

static inline struct qib_mr *to_imr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct qib_mr, ibmr);
}

static inline struct qib_pd *to_ipd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct qib_pd, ibpd);
}

static inline struct qib_ah *to_iah(struct ib_ah *ibah)
{
	return container_of(ibah, struct qib_ah, ibah);
}

static inline struct qib_cq *to_icq(struct ib_cq *ibcq)
{
	return container_of(ibcq, struct qib_cq, ibcq);
}

static inline struct qib_srq *to_isrq(struct ib_srq *ibsrq)
{
	return container_of(ibsrq, struct qib_srq, ibsrq);
}

static inline struct qib_qp *to_iqp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct qib_qp, ibqp);
}

static inline struct qib_ibdev *to_idev(struct ib_device *ibdev)
{
	return container_of(ibdev, struct qib_ibdev, ibdev);
}

static inline int qib_send_ok(struct qib_qp *qp)
{
	return !(qp->s_flags & (QIB_S_BUSY | QIB_S_ANY_WAIT_IO)) &&
		(qp->s_hdrwords || (qp->s_flags & QIB_S_RESP_PENDING) ||
		 !(qp->s_flags & QIB_S_ANY_WAIT_SEND));
}

extern struct workqueue_struct *qib_cq_wq;

static inline void qib_schedule_send(struct qib_qp *qp)
{
	if (qib_send_ok(qp))
		queue_work(ib_wq, &qp->s_work);
}

static inline int qib_pkey_ok(u16 pkey1, u16 pkey2)
{
	u16 p1 = pkey1 & 0x7FFF;
	u16 p2 = pkey2 & 0x7FFF;

	return p1 && p1 == p2 && ((__s16)pkey1 < 0 || (__s16)pkey2 < 0);
}

void qib_bad_pqkey(struct qib_ibport *ibp, __be16 trap_num, u32 key, u32 sl,
		   u32 qp1, u32 qp2, __be16 lid1, __be16 lid2);
void qib_cap_mask_chg(struct qib_ibport *ibp);
void qib_sys_guid_chg(struct qib_ibport *ibp);
void qib_node_desc_chg(struct qib_ibport *ibp);
int qib_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
		    struct ib_wc *in_wc, struct ib_grh *in_grh,
		    struct ib_mad *in_mad, struct ib_mad *out_mad);
int qib_create_agents(struct qib_ibdev *dev);
void qib_free_agents(struct qib_ibdev *dev);

static inline int qib_cmp24(u32 a, u32 b)
{
	return (((int) a) - ((int) b)) << 8;
}

struct qib_mcast *qib_mcast_find(struct qib_ibport *ibp, union ib_gid *mgid);

int qib_snapshot_counters(struct qib_pportdata *ppd, u64 *swords,
			  u64 *rwords, u64 *spkts, u64 *rpkts,
			  u64 *xmit_wait);

int qib_get_counters(struct qib_pportdata *ppd,
		     struct qib_verbs_counters *cntrs);

int qib_multicast_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid);

int qib_multicast_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid);

int qib_mcast_tree_empty(struct qib_ibport *ibp);

__be32 qib_compute_aeth(struct qib_qp *qp);

struct qib_qp *qib_lookup_qpn(struct qib_ibport *ibp, u32 qpn);

struct ib_qp *qib_create_qp(struct ib_pd *ibpd,
			    struct ib_qp_init_attr *init_attr,
			    struct ib_udata *udata);

int qib_destroy_qp(struct ib_qp *ibqp);

int qib_error_qp(struct qib_qp *qp, enum ib_wc_status err);

int qib_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		  int attr_mask, struct ib_udata *udata);

int qib_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		 int attr_mask, struct ib_qp_init_attr *init_attr);

unsigned qib_free_all_qps(struct qib_devdata *dd);

void qib_init_qpn_table(struct qib_devdata *dd, struct qib_qpn_table *qpt);

void qib_free_qpn_table(struct qib_qpn_table *qpt);

void qib_get_credit(struct qib_qp *qp, u32 aeth);

unsigned qib_pkt_delay(u32 plen, u8 snd_mult, u8 rcv_mult);

void qib_verbs_sdma_desc_avail(struct qib_pportdata *ppd, unsigned avail);

void qib_put_txreq(struct qib_verbs_txreq *tx);

int qib_verbs_send(struct qib_qp *qp, struct qib_ib_header *hdr,
		   u32 hdrwords, struct qib_sge_state *ss, u32 len);

void qib_copy_sge(struct qib_sge_state *ss, void *data, u32 length,
		  int release);

void qib_skip_sge(struct qib_sge_state *ss, u32 length, int release);

void qib_uc_rcv(struct qib_ibport *ibp, struct qib_ib_header *hdr,
		int has_grh, void *data, u32 tlen, struct qib_qp *qp);

void qib_rc_rcv(struct qib_ctxtdata *rcd, struct qib_ib_header *hdr,
		int has_grh, void *data, u32 tlen, struct qib_qp *qp);

int qib_check_ah(struct ib_device *ibdev, struct ib_ah_attr *ah_attr);

void qib_rc_rnr_retry(unsigned long arg);

void qib_rc_send_complete(struct qib_qp *qp, struct qib_ib_header *hdr);

void qib_rc_error(struct qib_qp *qp, enum ib_wc_status err);

int qib_post_ud_send(struct qib_qp *qp, struct ib_send_wr *wr);

void qib_ud_rcv(struct qib_ibport *ibp, struct qib_ib_header *hdr,
		int has_grh, void *data, u32 tlen, struct qib_qp *qp);

int qib_alloc_lkey(struct qib_lkey_table *rkt, struct qib_mregion *mr);

int qib_free_lkey(struct qib_ibdev *dev, struct qib_mregion *mr);

int qib_lkey_ok(struct qib_lkey_table *rkt, struct qib_pd *pd,
		struct qib_sge *isge, struct ib_sge *sge, int acc);

int qib_rkey_ok(struct qib_qp *qp, struct qib_sge *sge,
		u32 len, u64 vaddr, u32 rkey, int acc);

int qib_post_srq_receive(struct ib_srq *ibsrq, struct ib_recv_wr *wr,
			 struct ib_recv_wr **bad_wr);

struct ib_srq *qib_create_srq(struct ib_pd *ibpd,
			      struct ib_srq_init_attr *srq_init_attr,
			      struct ib_udata *udata);

int qib_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr,
		   enum ib_srq_attr_mask attr_mask,
		   struct ib_udata *udata);

int qib_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr);

int qib_destroy_srq(struct ib_srq *ibsrq);

void qib_cq_enter(struct qib_cq *cq, struct ib_wc *entry, int sig);

int qib_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *entry);

struct ib_cq *qib_create_cq(struct ib_device *ibdev, int entries,
			    int comp_vector, struct ib_ucontext *context,
			    struct ib_udata *udata);

int qib_destroy_cq(struct ib_cq *ibcq);

int qib_req_notify_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags notify_flags);

int qib_resize_cq(struct ib_cq *ibcq, int cqe, struct ib_udata *udata);

struct ib_mr *qib_get_dma_mr(struct ib_pd *pd, int acc);

struct ib_mr *qib_reg_phys_mr(struct ib_pd *pd,
			      struct ib_phys_buf *buffer_list,
			      int num_phys_buf, int acc, u64 *iova_start);

struct ib_mr *qib_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
			      u64 virt_addr, int mr_access_flags,
			      struct ib_udata *udata);

int qib_dereg_mr(struct ib_mr *ibmr);

struct ib_mr *qib_alloc_fast_reg_mr(struct ib_pd *pd, int max_page_list_len);

struct ib_fast_reg_page_list *qib_alloc_fast_reg_page_list(
				struct ib_device *ibdev, int page_list_len);

void qib_free_fast_reg_page_list(struct ib_fast_reg_page_list *pl);

int qib_fast_reg_mr(struct qib_qp *qp, struct ib_send_wr *wr);

struct ib_fmr *qib_alloc_fmr(struct ib_pd *pd, int mr_access_flags,
			     struct ib_fmr_attr *fmr_attr);

int qib_map_phys_fmr(struct ib_fmr *ibfmr, u64 *page_list,
		     int list_len, u64 iova);

int qib_unmap_fmr(struct list_head *fmr_list);

int qib_dealloc_fmr(struct ib_fmr *ibfmr);

void qib_release_mmap_info(struct kref *ref);

struct qib_mmap_info *qib_create_mmap_info(struct qib_ibdev *dev, u32 size,
					   struct ib_ucontext *context,
					   void *obj);

void qib_update_mmap_info(struct qib_ibdev *dev, struct qib_mmap_info *ip,
			  u32 size, void *obj);

int qib_mmap(struct ib_ucontext *context, struct vm_area_struct *vma);

int qib_get_rwqe(struct qib_qp *qp, int wr_id_only);

void qib_migrate_qp(struct qib_qp *qp);

int qib_ruc_check_hdr(struct qib_ibport *ibp, struct qib_ib_header *hdr,
		      int has_grh, struct qib_qp *qp, u32 bth0);

u32 qib_make_grh(struct qib_ibport *ibp, struct ib_grh *hdr,
		 struct ib_global_route *grh, u32 hwords, u32 nwords);

void qib_make_ruc_header(struct qib_qp *qp, struct qib_other_headers *ohdr,
			 u32 bth0, u32 bth2);

void qib_do_send(struct work_struct *work);

void qib_send_complete(struct qib_qp *qp, struct qib_swqe *wqe,
		       enum ib_wc_status status);

void qib_send_rc_ack(struct qib_qp *qp);

int qib_make_rc_req(struct qib_qp *qp);

int qib_make_uc_req(struct qib_qp *qp);

int qib_make_ud_req(struct qib_qp *qp);

int qib_register_ib_device(struct qib_devdata *);

void qib_unregister_ib_device(struct qib_devdata *);

void qib_ib_rcv(struct qib_ctxtdata *, void *, void *, u32);

void qib_ib_piobufavail(struct qib_devdata *);

unsigned qib_get_npkeys(struct qib_devdata *);

unsigned qib_get_pkey(struct qib_ibport *, unsigned);

extern const enum ib_wc_opcode ib_qib_wc_opcode[];

#define IB_PHYSPORTSTATE_SLEEP 1
#define IB_PHYSPORTSTATE_POLL 2
#define IB_PHYSPORTSTATE_DISABLED 3
#define IB_PHYSPORTSTATE_CFG_TRAIN 4
#define IB_PHYSPORTSTATE_LINKUP 5
#define IB_PHYSPORTSTATE_LINK_ERR_RECOVER 6
#define IB_PHYSPORTSTATE_CFG_DEBOUNCE 8
#define IB_PHYSPORTSTATE_CFG_IDLE 0xB
#define IB_PHYSPORTSTATE_RECOVERY_RETRAIN 0xC
#define IB_PHYSPORTSTATE_RECOVERY_WAITRMT 0xE
#define IB_PHYSPORTSTATE_RECOVERY_IDLE 0xF
#define IB_PHYSPORTSTATE_CFG_ENH 0x10
#define IB_PHYSPORTSTATE_CFG_WAIT_ENH 0x13

extern const int ib_qib_state_ops[];

extern __be64 ib_qib_sys_image_guid;    

extern unsigned int ib_qib_lkey_table_size;

extern unsigned int ib_qib_max_cqes;

extern unsigned int ib_qib_max_cqs;

extern unsigned int ib_qib_max_qp_wrs;

extern unsigned int ib_qib_max_qps;

extern unsigned int ib_qib_max_sges;

extern unsigned int ib_qib_max_mcast_grps;

extern unsigned int ib_qib_max_mcast_qp_attached;

extern unsigned int ib_qib_max_srqs;

extern unsigned int ib_qib_max_srq_sges;

extern unsigned int ib_qib_max_srq_wrs;

extern const u32 ib_qib_rnr_table[];

extern struct ib_dma_mapping_ops qib_dma_mapping_ops;

#endif                          
