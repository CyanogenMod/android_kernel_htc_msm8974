/*
 * Copyright (c) 2006, 2007, 2008 QLogic Corporation. All rights reserved.
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

#ifndef IPATH_VERBS_H
#define IPATH_VERBS_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kref.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_user_verbs.h>

#include "ipath_kernel.h"

#define IPATH_MAX_RDMA_ATOMIC	4

#define QPN_MAX                 (1 << 24)
#define QPNMAP_ENTRIES          (QPN_MAX / PAGE_SIZE / BITS_PER_BYTE)

#define IPATH_UVERBS_ABI_VERSION       2

#define IB_CQ_NONE	(IB_CQ_NEXT_COMP + 1)

#define IB_RNR_NAK			0x20
#define IB_NAK_PSN_ERROR		0x60
#define IB_NAK_INVALID_REQUEST		0x61
#define IB_NAK_REMOTE_ACCESS_ERROR	0x62
#define IB_NAK_REMOTE_OPERATIONAL_ERROR 0x63
#define IB_NAK_INVALID_RD_REQUEST	0x64

#define IPATH_POST_SEND_OK		0x01
#define IPATH_POST_RECV_OK		0x02
#define IPATH_PROCESS_RECV_OK		0x04
#define IPATH_PROCESS_SEND_OK		0x08
#define IPATH_PROCESS_NEXT_SEND_OK	0x10
#define IPATH_FLUSH_SEND		0x20
#define IPATH_FLUSH_RECV		0x40
#define IPATH_PROCESS_OR_FLUSH_SEND \
	(IPATH_PROCESS_SEND_OK | IPATH_FLUSH_SEND)

#define IB_PMA_SAMPLE_STATUS_DONE	0x00
#define IB_PMA_SAMPLE_STATUS_STARTED	0x01
#define IB_PMA_SAMPLE_STATUS_RUNNING	0x02

#define IB_PMA_PORT_XMIT_DATA	cpu_to_be16(0x0001)
#define IB_PMA_PORT_RCV_DATA	cpu_to_be16(0x0002)
#define IB_PMA_PORT_XMIT_PKTS	cpu_to_be16(0x0003)
#define IB_PMA_PORT_RCV_PKTS	cpu_to_be16(0x0004)
#define IB_PMA_PORT_XMIT_WAIT	cpu_to_be16(0x0005)

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

struct ipath_other_headers {
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

struct ipath_ib_header {
	__be16 lrh[4];
	union {
		struct {
			struct ib_grh grh;
			struct ipath_other_headers oth;
		} l;
		struct ipath_other_headers oth;
	} u;
} __attribute__ ((packed));

struct ipath_pio_header {
	__le32 pbc[2];
	struct ipath_ib_header hdr;
} __attribute__ ((packed));

struct ipath_mcast_qp {
	struct list_head list;
	struct ipath_qp *qp;
};

struct ipath_mcast {
	struct rb_node rb_node;
	union ib_gid mgid;
	struct list_head qp_list;
	wait_queue_head_t wait;
	atomic_t refcount;
	int n_attached;
};

struct ipath_pd {
	struct ib_pd ibpd;
	int user;		
};

struct ipath_ah {
	struct ib_ah ibah;
	struct ib_ah_attr attr;
};

struct ipath_mmap_info {
	struct list_head pending_mmaps;
	struct ib_ucontext *context;
	void *obj;
	__u64 offset;
	struct kref ref;
	unsigned size;
};

struct ipath_cq_wc {
	u32 head;		
	u32 tail;		
	union {
		
		struct ib_uverbs_wc uqueue[0];
		struct ib_wc kqueue[0];
	};
};

struct ipath_cq {
	struct ib_cq ibcq;
	struct tasklet_struct comptask;
	spinlock_t lock;
	u8 notify;
	u8 triggered;
	struct ipath_cq_wc *queue;
	struct ipath_mmap_info *ip;
};

struct ipath_seg {
	void *vaddr;
	size_t length;
};

#define IPATH_SEGSZ     (PAGE_SIZE / sizeof (struct ipath_seg))

struct ipath_segarray {
	struct ipath_seg segs[IPATH_SEGSZ];
};

struct ipath_mregion {
	struct ib_pd *pd;	
	u64 user_base;		
	u64 iova;		
	size_t length;
	u32 lkey;
	u32 offset;		
	int access_flags;
	u32 max_segs;		
	u32 mapsz;		
	struct ipath_segarray *map[0];	
};

struct ipath_sge {
	struct ipath_mregion *mr;
	void *vaddr;		
	u32 sge_length;		
	u32 length;		
	u16 m;			
	u16 n;			
};

struct ipath_mr {
	struct ib_mr ibmr;
	struct ib_umem *umem;
	struct ipath_mregion mr;	
};

struct ipath_swqe {
	struct ib_send_wr wr;	
	u32 psn;		
	u32 lpsn;		
	u32 ssn;		
	u32 length;		
	struct ipath_sge sg_list[0];
};

struct ipath_rwqe {
	u64 wr_id;
	u8 num_sge;
	struct ib_sge sg_list[0];
};

struct ipath_rwq {
	u32 head;		
	u32 tail;		
	struct ipath_rwqe wq[0];
};

struct ipath_rq {
	struct ipath_rwq *wq;
	spinlock_t lock;
	u32 size;		
	u8 max_sge;
};

struct ipath_srq {
	struct ib_srq ibsrq;
	struct ipath_rq rq;
	struct ipath_mmap_info *ip;
	
	u32 limit;
};

struct ipath_sge_state {
	struct ipath_sge *sg_list;      
	struct ipath_sge sge;   
	u8 num_sge;
	u8 static_rate;
};

struct ipath_ack_entry {
	u8 opcode;
	u8 sent;
	u32 psn;
	union {
		struct ipath_sge_state rdma_sge;
		u64 atomic_data;
	};
};

struct ipath_qp {
	struct ib_qp ibqp;
	struct ipath_qp *next;		
	struct ipath_qp *timer_next;	
	struct ipath_qp *pio_next;	
	struct list_head piowait;	
	struct list_head timerwait;	
	struct ib_ah_attr remote_ah_attr;
	struct ipath_ib_header s_hdr;	
	atomic_t refcount;
	wait_queue_head_t wait;
	wait_queue_head_t wait_dma;
	struct tasklet_struct s_task;
	struct ipath_mmap_info *ip;
	struct ipath_sge_state *s_cur_sge;
	struct ipath_verbs_txreq *s_tx;
	struct ipath_sge_state s_sge;	
	struct ipath_ack_entry s_ack_queue[IPATH_MAX_RDMA_ATOMIC + 1];
	struct ipath_sge_state s_ack_rdma_sge;
	struct ipath_sge_state s_rdma_read_sge;
	struct ipath_sge_state r_sge;	
	spinlock_t s_lock;
	atomic_t s_dma_busy;
	u16 s_pkt_delay;
	u16 s_hdrwords;		
	u32 s_cur_size;		
	u32 s_len;		
	u32 s_rdma_read_len;	
	u32 s_next_psn;		
	u32 s_last_psn;		
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
	u8 s_max_rd_atomic;	
	u8 s_num_rd_atomic;	
	u8 s_tail_ack_queue;	
	u8 s_flags;
	u8 s_dmult;
	u8 s_draining;
	u8 timeout;		
	enum ib_mtu path_mtu;
	u32 remote_qpn;
	u32 qkey;		
	u32 s_size;		
	u32 s_head;		
	u32 s_tail;		
	u32 s_cur;		
	u32 s_last;		
	u32 s_ssn;		
	u32 s_lsn;		
	struct ipath_swqe *s_wq;	
	struct ipath_swqe *s_wqe;
	struct ipath_sge *r_ud_sg_list;
	struct ipath_rq r_rq;		
	struct ipath_sge r_sg_list[0];	
};

#define IPATH_R_WRID_VALID	0

#define IPATH_R_REUSE_SGE	0x01
#define IPATH_R_RDMAR_SEQ	0x02

#define IPATH_S_SIGNAL_REQ_WR	0x01
#define IPATH_S_FENCE_PENDING	0x02
#define IPATH_S_RDMAR_PENDING	0x04
#define IPATH_S_ACK_PENDING	0x08
#define IPATH_S_BUSY		0x10
#define IPATH_S_WAITING		0x20
#define IPATH_S_WAIT_SSN_CREDIT	0x40
#define IPATH_S_WAIT_DMA	0x80

#define IPATH_S_ANY_WAIT (IPATH_S_FENCE_PENDING | IPATH_S_RDMAR_PENDING | \
	IPATH_S_WAITING | IPATH_S_WAIT_SSN_CREDIT | IPATH_S_WAIT_DMA)

#define IPATH_PSN_CREDIT	512

static inline struct ipath_swqe *get_swqe_ptr(struct ipath_qp *qp,
					      unsigned n)
{
	return (struct ipath_swqe *)((char *)qp->s_wq +
				     (sizeof(struct ipath_swqe) +
				      qp->s_max_sge *
				      sizeof(struct ipath_sge)) * n);
}

static inline struct ipath_rwqe *get_rwqe_ptr(struct ipath_rq *rq,
					      unsigned n)
{
	return (struct ipath_rwqe *)
		((char *) rq->wq->wq +
		 (sizeof(struct ipath_rwqe) +
		  rq->max_sge * sizeof(struct ib_sge)) * n);
}

struct qpn_map {
	atomic_t n_free;
	void *page;
};

struct ipath_qp_table {
	spinlock_t lock;
	u32 last;		
	u32 max;		
	u32 nmaps;		
	struct ipath_qp **table;
	
	struct qpn_map map[QPNMAP_ENTRIES];
};

struct ipath_lkey_table {
	spinlock_t lock;
	u32 next;		
	u32 gen;		
	u32 max;		
	struct ipath_mregion **table;
};

struct ipath_opcode_stats {
	u64 n_packets;		
	u64 n_bytes;		
};

struct ipath_ibdev {
	struct ib_device ibdev;
	struct ipath_devdata *dd;
	struct list_head pending_mmaps;
	spinlock_t mmap_offset_lock;
	u32 mmap_offset;
	int ib_unit;		
	u16 sm_lid;		
	u8 sm_sl;
	u8 mkeyprot;
	
	unsigned long mkey_lease_timeout;

	
	struct ipath_qp_table qp_table;
	struct ipath_lkey_table lk_table;
	struct list_head pending[3];	
	struct list_head piowait;	
	struct list_head txreq_free;
	void *txreq_bufs;
	
	struct list_head rnrwait;
	spinlock_t pending_lock;
	__be64 sys_image_guid;	
	__be64 gid_prefix;	
	__be64 mkey;

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

	u64 ipath_sword;	
	u64 ipath_rword;	
	u64 ipath_spkts;	
	u64 ipath_rpkts;	
	
	u64 ipath_xmit_wait;
	u64 rcv_errors;		
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
	u32 z_pkey_violations;			
	u32 z_local_link_integrity_errors;	
	u32 z_excessive_buffer_overrun_errors;	
	u32 z_vl15_dropped;			
	u32 n_rc_resends;
	u32 n_rc_acks;
	u32 n_rc_qacks;
	u32 n_seq_naks;
	u32 n_rdma_seq;
	u32 n_rnr_naks;
	u32 n_other_naks;
	u32 n_timeouts;
	u32 n_pkt_drops;
	u32 n_vl15_dropped;
	u32 n_wqe_errs;
	u32 n_rdma_dup_busy;
	u32 n_piowait;
	u32 n_unaligned;
	u32 port_cap_flags;
	u32 pma_sample_start;
	u32 pma_sample_interval;
	__be16 pma_counter_select[5];
	u16 pma_tag;
	u16 qkey_violations;
	u16 mkey_violations;
	u16 mkey_lease_period;
	u16 pending_index;	
	u8 pma_sample_status;
	u8 subnet_timeout;
	u8 vl_high_limit;
	struct ipath_opcode_stats opstats[128];
};

struct ipath_verbs_counters {
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

struct ipath_verbs_txreq {
	struct ipath_qp         *qp;
	struct ipath_swqe       *wqe;
	u32                      map_len;
	u32                      len;
	struct ipath_sge_state  *ss;
	struct ipath_pio_header  hdr;
	struct ipath_sdma_txreq  txreq;
};

static inline struct ipath_mr *to_imr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct ipath_mr, ibmr);
}

static inline struct ipath_pd *to_ipd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct ipath_pd, ibpd);
}

static inline struct ipath_ah *to_iah(struct ib_ah *ibah)
{
	return container_of(ibah, struct ipath_ah, ibah);
}

static inline struct ipath_cq *to_icq(struct ib_cq *ibcq)
{
	return container_of(ibcq, struct ipath_cq, ibcq);
}

static inline struct ipath_srq *to_isrq(struct ib_srq *ibsrq)
{
	return container_of(ibsrq, struct ipath_srq, ibsrq);
}

static inline struct ipath_qp *to_iqp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct ipath_qp, ibqp);
}

static inline struct ipath_ibdev *to_idev(struct ib_device *ibdev)
{
	return container_of(ibdev, struct ipath_ibdev, ibdev);
}

static inline void ipath_schedule_send(struct ipath_qp *qp)
{
	if (qp->s_flags & IPATH_S_ANY_WAIT)
		qp->s_flags &= ~IPATH_S_ANY_WAIT;
	if (!(qp->s_flags & IPATH_S_BUSY))
		tasklet_hi_schedule(&qp->s_task);
}

int ipath_process_mad(struct ib_device *ibdev,
		      int mad_flags,
		      u8 port_num,
		      struct ib_wc *in_wc,
		      struct ib_grh *in_grh,
		      struct ib_mad *in_mad, struct ib_mad *out_mad);

static inline int ipath_cmp24(u32 a, u32 b)
{
	return (((int) a) - ((int) b)) << 8;
}

struct ipath_mcast *ipath_mcast_find(union ib_gid *mgid);

int ipath_snapshot_counters(struct ipath_devdata *dd, u64 *swords,
			    u64 *rwords, u64 *spkts, u64 *rpkts,
			    u64 *xmit_wait);

int ipath_get_counters(struct ipath_devdata *dd,
		       struct ipath_verbs_counters *cntrs);

int ipath_multicast_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid);

int ipath_multicast_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid);

int ipath_mcast_tree_empty(void);

__be32 ipath_compute_aeth(struct ipath_qp *qp);

struct ipath_qp *ipath_lookup_qpn(struct ipath_qp_table *qpt, u32 qpn);

struct ib_qp *ipath_create_qp(struct ib_pd *ibpd,
			      struct ib_qp_init_attr *init_attr,
			      struct ib_udata *udata);

int ipath_destroy_qp(struct ib_qp *ibqp);

int ipath_error_qp(struct ipath_qp *qp, enum ib_wc_status err);

int ipath_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		    int attr_mask, struct ib_udata *udata);

int ipath_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		   int attr_mask, struct ib_qp_init_attr *init_attr);

unsigned ipath_free_all_qps(struct ipath_qp_table *qpt);

int ipath_init_qp_table(struct ipath_ibdev *idev, int size);

void ipath_get_credit(struct ipath_qp *qp, u32 aeth);

unsigned ipath_ib_rate_to_mult(enum ib_rate rate);

int ipath_verbs_send(struct ipath_qp *qp, struct ipath_ib_header *hdr,
		     u32 hdrwords, struct ipath_sge_state *ss, u32 len);

void ipath_copy_sge(struct ipath_sge_state *ss, void *data, u32 length);

void ipath_skip_sge(struct ipath_sge_state *ss, u32 length);

void ipath_uc_rcv(struct ipath_ibdev *dev, struct ipath_ib_header *hdr,
		  int has_grh, void *data, u32 tlen, struct ipath_qp *qp);

void ipath_rc_rcv(struct ipath_ibdev *dev, struct ipath_ib_header *hdr,
		  int has_grh, void *data, u32 tlen, struct ipath_qp *qp);

void ipath_restart_rc(struct ipath_qp *qp, u32 psn);

void ipath_rc_error(struct ipath_qp *qp, enum ib_wc_status err);

int ipath_post_ud_send(struct ipath_qp *qp, struct ib_send_wr *wr);

void ipath_ud_rcv(struct ipath_ibdev *dev, struct ipath_ib_header *hdr,
		  int has_grh, void *data, u32 tlen, struct ipath_qp *qp);

int ipath_alloc_lkey(struct ipath_lkey_table *rkt,
		     struct ipath_mregion *mr);

void ipath_free_lkey(struct ipath_lkey_table *rkt, u32 lkey);

int ipath_lkey_ok(struct ipath_qp *qp, struct ipath_sge *isge,
		  struct ib_sge *sge, int acc);

int ipath_rkey_ok(struct ipath_qp *qp, struct ipath_sge_state *ss,
		  u32 len, u64 vaddr, u32 rkey, int acc);

int ipath_post_srq_receive(struct ib_srq *ibsrq, struct ib_recv_wr *wr,
			   struct ib_recv_wr **bad_wr);

struct ib_srq *ipath_create_srq(struct ib_pd *ibpd,
				struct ib_srq_init_attr *srq_init_attr,
				struct ib_udata *udata);

int ipath_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr,
		     enum ib_srq_attr_mask attr_mask,
		     struct ib_udata *udata);

int ipath_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr);

int ipath_destroy_srq(struct ib_srq *ibsrq);

void ipath_cq_enter(struct ipath_cq *cq, struct ib_wc *entry, int sig);

int ipath_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *entry);

struct ib_cq *ipath_create_cq(struct ib_device *ibdev, int entries, int comp_vector,
			      struct ib_ucontext *context,
			      struct ib_udata *udata);

int ipath_destroy_cq(struct ib_cq *ibcq);

int ipath_req_notify_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags notify_flags);

int ipath_resize_cq(struct ib_cq *ibcq, int cqe, struct ib_udata *udata);

struct ib_mr *ipath_get_dma_mr(struct ib_pd *pd, int acc);

struct ib_mr *ipath_reg_phys_mr(struct ib_pd *pd,
				struct ib_phys_buf *buffer_list,
				int num_phys_buf, int acc, u64 *iova_start);

struct ib_mr *ipath_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				u64 virt_addr, int mr_access_flags,
				struct ib_udata *udata);

int ipath_dereg_mr(struct ib_mr *ibmr);

struct ib_fmr *ipath_alloc_fmr(struct ib_pd *pd, int mr_access_flags,
			       struct ib_fmr_attr *fmr_attr);

int ipath_map_phys_fmr(struct ib_fmr *ibfmr, u64 * page_list,
		       int list_len, u64 iova);

int ipath_unmap_fmr(struct list_head *fmr_list);

int ipath_dealloc_fmr(struct ib_fmr *ibfmr);

void ipath_release_mmap_info(struct kref *ref);

struct ipath_mmap_info *ipath_create_mmap_info(struct ipath_ibdev *dev,
					       u32 size,
					       struct ib_ucontext *context,
					       void *obj);

void ipath_update_mmap_info(struct ipath_ibdev *dev,
			    struct ipath_mmap_info *ip,
			    u32 size, void *obj);

int ipath_mmap(struct ib_ucontext *context, struct vm_area_struct *vma);

void ipath_insert_rnr_queue(struct ipath_qp *qp);

int ipath_init_sge(struct ipath_qp *qp, struct ipath_rwqe *wqe,
		   u32 *lengthp, struct ipath_sge_state *ss);

int ipath_get_rwqe(struct ipath_qp *qp, int wr_id_only);

u32 ipath_make_grh(struct ipath_ibdev *dev, struct ib_grh *hdr,
		   struct ib_global_route *grh, u32 hwords, u32 nwords);

void ipath_make_ruc_header(struct ipath_ibdev *dev, struct ipath_qp *qp,
			   struct ipath_other_headers *ohdr,
			   u32 bth0, u32 bth2);

void ipath_do_send(unsigned long data);

void ipath_send_complete(struct ipath_qp *qp, struct ipath_swqe *wqe,
			 enum ib_wc_status status);

int ipath_make_rc_req(struct ipath_qp *qp);

int ipath_make_uc_req(struct ipath_qp *qp);

int ipath_make_ud_req(struct ipath_qp *qp);

int ipath_register_ib_device(struct ipath_devdata *);

void ipath_unregister_ib_device(struct ipath_ibdev *);

void ipath_ib_rcv(struct ipath_ibdev *, void *, void *, u32);

int ipath_ib_piobufavail(struct ipath_ibdev *);

unsigned ipath_get_npkeys(struct ipath_devdata *);

u32 ipath_get_cr_errpkey(struct ipath_devdata *);

unsigned ipath_get_pkey(struct ipath_devdata *, unsigned);

extern const enum ib_wc_opcode ib_ipath_wc_opcode[];

extern const u8 ipath_cvt_physportstate[];
#define IB_PHYSPORTSTATE_SLEEP 1
#define IB_PHYSPORTSTATE_POLL 2
#define IB_PHYSPORTSTATE_DISABLED 3
#define IB_PHYSPORTSTATE_CFG_TRAIN 4
#define IB_PHYSPORTSTATE_LINKUP 5
#define IB_PHYSPORTSTATE_LINK_ERR_RECOVER 6

extern const int ib_ipath_state_ops[];

extern unsigned int ib_ipath_lkey_table_size;

extern unsigned int ib_ipath_max_cqes;

extern unsigned int ib_ipath_max_cqs;

extern unsigned int ib_ipath_max_qp_wrs;

extern unsigned int ib_ipath_max_qps;

extern unsigned int ib_ipath_max_sges;

extern unsigned int ib_ipath_max_mcast_grps;

extern unsigned int ib_ipath_max_mcast_qp_attached;

extern unsigned int ib_ipath_max_srqs;

extern unsigned int ib_ipath_max_srq_sges;

extern unsigned int ib_ipath_max_srq_wrs;

extern const u32 ib_ipath_rnr_table[];

extern struct ib_dma_mapping_ops ipath_dma_mapping_ops;

#endif				
