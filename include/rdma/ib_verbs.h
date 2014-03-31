/*
 * Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2004 Infinicon Corporation.  All rights reserved.
 * Copyright (c) 2004 Intel Corporation.  All rights reserved.
 * Copyright (c) 2004 Topspin Corporation.  All rights reserved.
 * Copyright (c) 2004 Voltaire Corporation.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems.  All rights reserved.
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

#if !defined(IB_VERBS_H)
#define IB_VERBS_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>

#include <linux/atomic.h>
#include <asm/uaccess.h>

extern struct workqueue_struct *ib_wq;

union ib_gid {
	u8	raw[16];
	struct {
		__be64	subnet_prefix;
		__be64	interface_id;
	} global;
};

enum rdma_node_type {
	
	RDMA_NODE_IB_CA 	= 1,
	RDMA_NODE_IB_SWITCH,
	RDMA_NODE_IB_ROUTER,
	RDMA_NODE_RNIC
};

enum rdma_transport_type {
	RDMA_TRANSPORT_IB,
	RDMA_TRANSPORT_IWARP
};

enum rdma_transport_type
rdma_node_get_transport(enum rdma_node_type node_type) __attribute_const__;

enum rdma_link_layer {
	IB_LINK_LAYER_UNSPECIFIED,
	IB_LINK_LAYER_INFINIBAND,
	IB_LINK_LAYER_ETHERNET,
};

enum ib_device_cap_flags {
	IB_DEVICE_RESIZE_MAX_WR		= 1,
	IB_DEVICE_BAD_PKEY_CNTR		= (1<<1),
	IB_DEVICE_BAD_QKEY_CNTR		= (1<<2),
	IB_DEVICE_RAW_MULTI		= (1<<3),
	IB_DEVICE_AUTO_PATH_MIG		= (1<<4),
	IB_DEVICE_CHANGE_PHY_PORT	= (1<<5),
	IB_DEVICE_UD_AV_PORT_ENFORCE	= (1<<6),
	IB_DEVICE_CURR_QP_STATE_MOD	= (1<<7),
	IB_DEVICE_SHUTDOWN_PORT		= (1<<8),
	IB_DEVICE_INIT_TYPE		= (1<<9),
	IB_DEVICE_PORT_ACTIVE_EVENT	= (1<<10),
	IB_DEVICE_SYS_IMAGE_GUID	= (1<<11),
	IB_DEVICE_RC_RNR_NAK_GEN	= (1<<12),
	IB_DEVICE_SRQ_RESIZE		= (1<<13),
	IB_DEVICE_N_NOTIFY_CQ		= (1<<14),
	IB_DEVICE_LOCAL_DMA_LKEY	= (1<<15),
	IB_DEVICE_RESERVED		= (1<<16), 
	IB_DEVICE_MEM_WINDOW		= (1<<17),
	IB_DEVICE_UD_IP_CSUM		= (1<<18),
	IB_DEVICE_UD_TSO		= (1<<19),
	IB_DEVICE_XRC			= (1<<20),
	IB_DEVICE_MEM_MGT_EXTENSIONS	= (1<<21),
	IB_DEVICE_BLOCK_MULTICAST_LOOPBACK = (1<<22),
};

enum ib_atomic_cap {
	IB_ATOMIC_NONE,
	IB_ATOMIC_HCA,
	IB_ATOMIC_GLOB
};

struct ib_device_attr {
	u64			fw_ver;
	__be64			sys_image_guid;
	u64			max_mr_size;
	u64			page_size_cap;
	u32			vendor_id;
	u32			vendor_part_id;
	u32			hw_ver;
	int			max_qp;
	int			max_qp_wr;
	int			device_cap_flags;
	int			max_sge;
	int			max_sge_rd;
	int			max_cq;
	int			max_cqe;
	int			max_mr;
	int			max_pd;
	int			max_qp_rd_atom;
	int			max_ee_rd_atom;
	int			max_res_rd_atom;
	int			max_qp_init_rd_atom;
	int			max_ee_init_rd_atom;
	enum ib_atomic_cap	atomic_cap;
	enum ib_atomic_cap	masked_atomic_cap;
	int			max_ee;
	int			max_rdd;
	int			max_mw;
	int			max_raw_ipv6_qp;
	int			max_raw_ethy_qp;
	int			max_mcast_grp;
	int			max_mcast_qp_attach;
	int			max_total_mcast_qp_attach;
	int			max_ah;
	int			max_fmr;
	int			max_map_per_fmr;
	int			max_srq;
	int			max_srq_wr;
	int			max_srq_sge;
	unsigned int		max_fast_reg_page_list_len;
	u16			max_pkeys;
	u8			local_ca_ack_delay;
};

enum ib_mtu {
	IB_MTU_256  = 1,
	IB_MTU_512  = 2,
	IB_MTU_1024 = 3,
	IB_MTU_2048 = 4,
	IB_MTU_4096 = 5
};

static inline int ib_mtu_enum_to_int(enum ib_mtu mtu)
{
	switch (mtu) {
	case IB_MTU_256:  return  256;
	case IB_MTU_512:  return  512;
	case IB_MTU_1024: return 1024;
	case IB_MTU_2048: return 2048;
	case IB_MTU_4096: return 4096;
	default: 	  return -1;
	}
}

enum ib_port_state {
	IB_PORT_NOP		= 0,
	IB_PORT_DOWN		= 1,
	IB_PORT_INIT		= 2,
	IB_PORT_ARMED		= 3,
	IB_PORT_ACTIVE		= 4,
	IB_PORT_ACTIVE_DEFER	= 5
};

enum ib_port_cap_flags {
	IB_PORT_SM				= 1 <<  1,
	IB_PORT_NOTICE_SUP			= 1 <<  2,
	IB_PORT_TRAP_SUP			= 1 <<  3,
	IB_PORT_OPT_IPD_SUP                     = 1 <<  4,
	IB_PORT_AUTO_MIGR_SUP			= 1 <<  5,
	IB_PORT_SL_MAP_SUP			= 1 <<  6,
	IB_PORT_MKEY_NVRAM			= 1 <<  7,
	IB_PORT_PKEY_NVRAM			= 1 <<  8,
	IB_PORT_LED_INFO_SUP			= 1 <<  9,
	IB_PORT_SM_DISABLED			= 1 << 10,
	IB_PORT_SYS_IMAGE_GUID_SUP		= 1 << 11,
	IB_PORT_PKEY_SW_EXT_PORT_TRAP_SUP	= 1 << 12,
	IB_PORT_EXTENDED_SPEEDS_SUP             = 1 << 14,
	IB_PORT_CM_SUP				= 1 << 16,
	IB_PORT_SNMP_TUNNEL_SUP			= 1 << 17,
	IB_PORT_REINIT_SUP			= 1 << 18,
	IB_PORT_DEVICE_MGMT_SUP			= 1 << 19,
	IB_PORT_VENDOR_CLASS_SUP		= 1 << 20,
	IB_PORT_DR_NOTICE_SUP			= 1 << 21,
	IB_PORT_CAP_MASK_NOTICE_SUP		= 1 << 22,
	IB_PORT_BOOT_MGMT_SUP			= 1 << 23,
	IB_PORT_LINK_LATENCY_SUP		= 1 << 24,
	IB_PORT_CLIENT_REG_SUP			= 1 << 25
};

enum ib_port_width {
	IB_WIDTH_1X	= 1,
	IB_WIDTH_4X	= 2,
	IB_WIDTH_8X	= 4,
	IB_WIDTH_12X	= 8
};

static inline int ib_width_enum_to_int(enum ib_port_width width)
{
	switch (width) {
	case IB_WIDTH_1X:  return  1;
	case IB_WIDTH_4X:  return  4;
	case IB_WIDTH_8X:  return  8;
	case IB_WIDTH_12X: return 12;
	default: 	  return -1;
	}
}

enum ib_port_speed {
	IB_SPEED_SDR	= 1,
	IB_SPEED_DDR	= 2,
	IB_SPEED_QDR	= 4,
	IB_SPEED_FDR10	= 8,
	IB_SPEED_FDR	= 16,
	IB_SPEED_EDR	= 32
};

struct ib_protocol_stats {
	
};

struct iw_protocol_stats {
	u64	ipInReceives;
	u64	ipInHdrErrors;
	u64	ipInTooBigErrors;
	u64	ipInNoRoutes;
	u64	ipInAddrErrors;
	u64	ipInUnknownProtos;
	u64	ipInTruncatedPkts;
	u64	ipInDiscards;
	u64	ipInDelivers;
	u64	ipOutForwDatagrams;
	u64	ipOutRequests;
	u64	ipOutDiscards;
	u64	ipOutNoRoutes;
	u64	ipReasmTimeout;
	u64	ipReasmReqds;
	u64	ipReasmOKs;
	u64	ipReasmFails;
	u64	ipFragOKs;
	u64	ipFragFails;
	u64	ipFragCreates;
	u64	ipInMcastPkts;
	u64	ipOutMcastPkts;
	u64	ipInBcastPkts;
	u64	ipOutBcastPkts;

	u64	tcpRtoAlgorithm;
	u64	tcpRtoMin;
	u64	tcpRtoMax;
	u64	tcpMaxConn;
	u64	tcpActiveOpens;
	u64	tcpPassiveOpens;
	u64	tcpAttemptFails;
	u64	tcpEstabResets;
	u64	tcpCurrEstab;
	u64	tcpInSegs;
	u64	tcpOutSegs;
	u64	tcpRetransSegs;
	u64	tcpInErrs;
	u64	tcpOutRsts;
};

union rdma_protocol_stats {
	struct ib_protocol_stats	ib;
	struct iw_protocol_stats	iw;
};

struct ib_port_attr {
	enum ib_port_state	state;
	enum ib_mtu		max_mtu;
	enum ib_mtu		active_mtu;
	int			gid_tbl_len;
	u32			port_cap_flags;
	u32			max_msg_sz;
	u32			bad_pkey_cntr;
	u32			qkey_viol_cntr;
	u16			pkey_tbl_len;
	u16			lid;
	u16			sm_lid;
	u8			lmc;
	u8			max_vl_num;
	u8			sm_sl;
	u8			subnet_timeout;
	u8			init_type_reply;
	u8			active_width;
	u8			active_speed;
	u8                      phys_state;
};

enum ib_device_modify_flags {
	IB_DEVICE_MODIFY_SYS_IMAGE_GUID	= 1 << 0,
	IB_DEVICE_MODIFY_NODE_DESC	= 1 << 1
};

struct ib_device_modify {
	u64	sys_image_guid;
	char	node_desc[64];
};

enum ib_port_modify_flags {
	IB_PORT_SHUTDOWN		= 1,
	IB_PORT_INIT_TYPE		= (1<<2),
	IB_PORT_RESET_QKEY_CNTR		= (1<<3)
};

struct ib_port_modify {
	u32	set_port_cap_mask;
	u32	clr_port_cap_mask;
	u8	init_type;
};

enum ib_event_type {
	IB_EVENT_CQ_ERR,
	IB_EVENT_QP_FATAL,
	IB_EVENT_QP_REQ_ERR,
	IB_EVENT_QP_ACCESS_ERR,
	IB_EVENT_COMM_EST,
	IB_EVENT_SQ_DRAINED,
	IB_EVENT_PATH_MIG,
	IB_EVENT_PATH_MIG_ERR,
	IB_EVENT_DEVICE_FATAL,
	IB_EVENT_PORT_ACTIVE,
	IB_EVENT_PORT_ERR,
	IB_EVENT_LID_CHANGE,
	IB_EVENT_PKEY_CHANGE,
	IB_EVENT_SM_CHANGE,
	IB_EVENT_SRQ_ERR,
	IB_EVENT_SRQ_LIMIT_REACHED,
	IB_EVENT_QP_LAST_WQE_REACHED,
	IB_EVENT_CLIENT_REREGISTER,
	IB_EVENT_GID_CHANGE,
};

struct ib_event {
	struct ib_device	*device;
	union {
		struct ib_cq	*cq;
		struct ib_qp	*qp;
		struct ib_srq	*srq;
		u8		port_num;
	} element;
	enum ib_event_type	event;
};

struct ib_event_handler {
	struct ib_device *device;
	void            (*handler)(struct ib_event_handler *, struct ib_event *);
	struct list_head  list;
};

#define INIT_IB_EVENT_HANDLER(_ptr, _device, _handler)		\
	do {							\
		(_ptr)->device  = _device;			\
		(_ptr)->handler = _handler;			\
		INIT_LIST_HEAD(&(_ptr)->list);			\
	} while (0)

struct ib_global_route {
	union ib_gid	dgid;
	u32		flow_label;
	u8		sgid_index;
	u8		hop_limit;
	u8		traffic_class;
};

struct ib_grh {
	__be32		version_tclass_flow;
	__be16		paylen;
	u8		next_hdr;
	u8		hop_limit;
	union ib_gid	sgid;
	union ib_gid	dgid;
};

enum {
	IB_MULTICAST_QPN = 0xffffff
};

#define IB_LID_PERMISSIVE	cpu_to_be16(0xFFFF)

enum ib_ah_flags {
	IB_AH_GRH	= 1
};

enum ib_rate {
	IB_RATE_PORT_CURRENT = 0,
	IB_RATE_2_5_GBPS = 2,
	IB_RATE_5_GBPS   = 5,
	IB_RATE_10_GBPS  = 3,
	IB_RATE_20_GBPS  = 6,
	IB_RATE_30_GBPS  = 4,
	IB_RATE_40_GBPS  = 7,
	IB_RATE_60_GBPS  = 8,
	IB_RATE_80_GBPS  = 9,
	IB_RATE_120_GBPS = 10,
	IB_RATE_14_GBPS  = 11,
	IB_RATE_56_GBPS  = 12,
	IB_RATE_112_GBPS = 13,
	IB_RATE_168_GBPS = 14,
	IB_RATE_25_GBPS  = 15,
	IB_RATE_100_GBPS = 16,
	IB_RATE_200_GBPS = 17,
	IB_RATE_300_GBPS = 18
};

int ib_rate_to_mult(enum ib_rate rate) __attribute_const__;

int ib_rate_to_mbps(enum ib_rate rate) __attribute_const__;

enum ib_rate mult_to_ib_rate(int mult) __attribute_const__;

struct ib_ah_attr {
	struct ib_global_route	grh;
	u16			dlid;
	u8			sl;
	u8			src_path_bits;
	u8			static_rate;
	u8			ah_flags;
	u8			port_num;
};

enum ib_wc_status {
	IB_WC_SUCCESS,
	IB_WC_LOC_LEN_ERR,
	IB_WC_LOC_QP_OP_ERR,
	IB_WC_LOC_EEC_OP_ERR,
	IB_WC_LOC_PROT_ERR,
	IB_WC_WR_FLUSH_ERR,
	IB_WC_MW_BIND_ERR,
	IB_WC_BAD_RESP_ERR,
	IB_WC_LOC_ACCESS_ERR,
	IB_WC_REM_INV_REQ_ERR,
	IB_WC_REM_ACCESS_ERR,
	IB_WC_REM_OP_ERR,
	IB_WC_RETRY_EXC_ERR,
	IB_WC_RNR_RETRY_EXC_ERR,
	IB_WC_LOC_RDD_VIOL_ERR,
	IB_WC_REM_INV_RD_REQ_ERR,
	IB_WC_REM_ABORT_ERR,
	IB_WC_INV_EECN_ERR,
	IB_WC_INV_EEC_STATE_ERR,
	IB_WC_FATAL_ERR,
	IB_WC_RESP_TIMEOUT_ERR,
	IB_WC_GENERAL_ERR
};

enum ib_wc_opcode {
	IB_WC_SEND,
	IB_WC_RDMA_WRITE,
	IB_WC_RDMA_READ,
	IB_WC_COMP_SWAP,
	IB_WC_FETCH_ADD,
	IB_WC_BIND_MW,
	IB_WC_LSO,
	IB_WC_LOCAL_INV,
	IB_WC_FAST_REG_MR,
	IB_WC_MASKED_COMP_SWAP,
	IB_WC_MASKED_FETCH_ADD,
	IB_WC_RECV			= 1 << 7,
	IB_WC_RECV_RDMA_WITH_IMM
};

enum ib_wc_flags {
	IB_WC_GRH		= 1,
	IB_WC_WITH_IMM		= (1<<1),
	IB_WC_WITH_INVALIDATE	= (1<<2),
	IB_WC_IP_CSUM_OK	= (1<<3),
};

struct ib_wc {
	u64			wr_id;
	enum ib_wc_status	status;
	enum ib_wc_opcode	opcode;
	u32			vendor_err;
	u32			byte_len;
	struct ib_qp	       *qp;
	union {
		__be32		imm_data;
		u32		invalidate_rkey;
	} ex;
	u32			src_qp;
	int			wc_flags;
	u16			pkey_index;
	u16			slid;
	u8			sl;
	u8			dlid_path_bits;
	u8			port_num;	
};

enum ib_cq_notify_flags {
	IB_CQ_SOLICITED			= 1 << 0,
	IB_CQ_NEXT_COMP			= 1 << 1,
	IB_CQ_SOLICITED_MASK		= IB_CQ_SOLICITED | IB_CQ_NEXT_COMP,
	IB_CQ_REPORT_MISSED_EVENTS	= 1 << 2,
};

enum ib_srq_type {
	IB_SRQT_BASIC,
	IB_SRQT_XRC
};

enum ib_srq_attr_mask {
	IB_SRQ_MAX_WR	= 1 << 0,
	IB_SRQ_LIMIT	= 1 << 1,
};

struct ib_srq_attr {
	u32	max_wr;
	u32	max_sge;
	u32	srq_limit;
};

struct ib_srq_init_attr {
	void		      (*event_handler)(struct ib_event *, void *);
	void		       *srq_context;
	struct ib_srq_attr	attr;
	enum ib_srq_type	srq_type;

	union {
		struct {
			struct ib_xrcd *xrcd;
			struct ib_cq   *cq;
		} xrc;
	} ext;
};

struct ib_qp_cap {
	u32	max_send_wr;
	u32	max_recv_wr;
	u32	max_send_sge;
	u32	max_recv_sge;
	u32	max_inline_data;
};

enum ib_sig_type {
	IB_SIGNAL_ALL_WR,
	IB_SIGNAL_REQ_WR
};

enum ib_qp_type {
	IB_QPT_SMI,
	IB_QPT_GSI,

	IB_QPT_RC,
	IB_QPT_UC,
	IB_QPT_UD,
	IB_QPT_RAW_IPV6,
	IB_QPT_RAW_ETHERTYPE,
	
	IB_QPT_XRC_INI = 9,
	IB_QPT_XRC_TGT,
	IB_QPT_MAX
};

enum ib_qp_create_flags {
	IB_QP_CREATE_IPOIB_UD_LSO		= 1 << 0,
	IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK	= 1 << 1,
};

struct ib_qp_init_attr {
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_srq	       *srq;
	struct ib_xrcd	       *xrcd;     
	struct ib_qp_cap	cap;
	enum ib_sig_type	sq_sig_type;
	enum ib_qp_type		qp_type;
	enum ib_qp_create_flags	create_flags;
	u8			port_num; 
};

struct ib_qp_open_attr {
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	u32			qp_num;
	enum ib_qp_type		qp_type;
};

enum ib_rnr_timeout {
	IB_RNR_TIMER_655_36 =  0,
	IB_RNR_TIMER_000_01 =  1,
	IB_RNR_TIMER_000_02 =  2,
	IB_RNR_TIMER_000_03 =  3,
	IB_RNR_TIMER_000_04 =  4,
	IB_RNR_TIMER_000_06 =  5,
	IB_RNR_TIMER_000_08 =  6,
	IB_RNR_TIMER_000_12 =  7,
	IB_RNR_TIMER_000_16 =  8,
	IB_RNR_TIMER_000_24 =  9,
	IB_RNR_TIMER_000_32 = 10,
	IB_RNR_TIMER_000_48 = 11,
	IB_RNR_TIMER_000_64 = 12,
	IB_RNR_TIMER_000_96 = 13,
	IB_RNR_TIMER_001_28 = 14,
	IB_RNR_TIMER_001_92 = 15,
	IB_RNR_TIMER_002_56 = 16,
	IB_RNR_TIMER_003_84 = 17,
	IB_RNR_TIMER_005_12 = 18,
	IB_RNR_TIMER_007_68 = 19,
	IB_RNR_TIMER_010_24 = 20,
	IB_RNR_TIMER_015_36 = 21,
	IB_RNR_TIMER_020_48 = 22,
	IB_RNR_TIMER_030_72 = 23,
	IB_RNR_TIMER_040_96 = 24,
	IB_RNR_TIMER_061_44 = 25,
	IB_RNR_TIMER_081_92 = 26,
	IB_RNR_TIMER_122_88 = 27,
	IB_RNR_TIMER_163_84 = 28,
	IB_RNR_TIMER_245_76 = 29,
	IB_RNR_TIMER_327_68 = 30,
	IB_RNR_TIMER_491_52 = 31
};

enum ib_qp_attr_mask {
	IB_QP_STATE			= 1,
	IB_QP_CUR_STATE			= (1<<1),
	IB_QP_EN_SQD_ASYNC_NOTIFY	= (1<<2),
	IB_QP_ACCESS_FLAGS		= (1<<3),
	IB_QP_PKEY_INDEX		= (1<<4),
	IB_QP_PORT			= (1<<5),
	IB_QP_QKEY			= (1<<6),
	IB_QP_AV			= (1<<7),
	IB_QP_PATH_MTU			= (1<<8),
	IB_QP_TIMEOUT			= (1<<9),
	IB_QP_RETRY_CNT			= (1<<10),
	IB_QP_RNR_RETRY			= (1<<11),
	IB_QP_RQ_PSN			= (1<<12),
	IB_QP_MAX_QP_RD_ATOMIC		= (1<<13),
	IB_QP_ALT_PATH			= (1<<14),
	IB_QP_MIN_RNR_TIMER		= (1<<15),
	IB_QP_SQ_PSN			= (1<<16),
	IB_QP_MAX_DEST_RD_ATOMIC	= (1<<17),
	IB_QP_PATH_MIG_STATE		= (1<<18),
	IB_QP_CAP			= (1<<19),
	IB_QP_DEST_QPN			= (1<<20)
};

enum ib_qp_state {
	IB_QPS_RESET,
	IB_QPS_INIT,
	IB_QPS_RTR,
	IB_QPS_RTS,
	IB_QPS_SQD,
	IB_QPS_SQE,
	IB_QPS_ERR
};

enum ib_mig_state {
	IB_MIG_MIGRATED,
	IB_MIG_REARM,
	IB_MIG_ARMED
};

struct ib_qp_attr {
	enum ib_qp_state	qp_state;
	enum ib_qp_state	cur_qp_state;
	enum ib_mtu		path_mtu;
	enum ib_mig_state	path_mig_state;
	u32			qkey;
	u32			rq_psn;
	u32			sq_psn;
	u32			dest_qp_num;
	int			qp_access_flags;
	struct ib_qp_cap	cap;
	struct ib_ah_attr	ah_attr;
	struct ib_ah_attr	alt_ah_attr;
	u16			pkey_index;
	u16			alt_pkey_index;
	u8			en_sqd_async_notify;
	u8			sq_draining;
	u8			max_rd_atomic;
	u8			max_dest_rd_atomic;
	u8			min_rnr_timer;
	u8			port_num;
	u8			timeout;
	u8			retry_cnt;
	u8			rnr_retry;
	u8			alt_port_num;
	u8			alt_timeout;
};

enum ib_wr_opcode {
	IB_WR_RDMA_WRITE,
	IB_WR_RDMA_WRITE_WITH_IMM,
	IB_WR_SEND,
	IB_WR_SEND_WITH_IMM,
	IB_WR_RDMA_READ,
	IB_WR_ATOMIC_CMP_AND_SWP,
	IB_WR_ATOMIC_FETCH_AND_ADD,
	IB_WR_LSO,
	IB_WR_SEND_WITH_INV,
	IB_WR_RDMA_READ_WITH_INV,
	IB_WR_LOCAL_INV,
	IB_WR_FAST_REG_MR,
	IB_WR_MASKED_ATOMIC_CMP_AND_SWP,
	IB_WR_MASKED_ATOMIC_FETCH_AND_ADD,
};

enum ib_send_flags {
	IB_SEND_FENCE		= 1,
	IB_SEND_SIGNALED	= (1<<1),
	IB_SEND_SOLICITED	= (1<<2),
	IB_SEND_INLINE		= (1<<3),
	IB_SEND_IP_CSUM		= (1<<4)
};

struct ib_sge {
	u64	addr;
	u32	length;
	u32	lkey;
};

struct ib_fast_reg_page_list {
	struct ib_device       *device;
	u64		       *page_list;
	unsigned int		max_page_list_len;
};

struct ib_send_wr {
	struct ib_send_wr      *next;
	u64			wr_id;
	struct ib_sge	       *sg_list;
	int			num_sge;
	enum ib_wr_opcode	opcode;
	int			send_flags;
	union {
		__be32		imm_data;
		u32		invalidate_rkey;
	} ex;
	union {
		struct {
			u64	remote_addr;
			u32	rkey;
		} rdma;
		struct {
			u64	remote_addr;
			u64	compare_add;
			u64	swap;
			u64	compare_add_mask;
			u64	swap_mask;
			u32	rkey;
		} atomic;
		struct {
			struct ib_ah *ah;
			void   *header;
			int     hlen;
			int     mss;
			u32	remote_qpn;
			u32	remote_qkey;
			u16	pkey_index; 
			u8	port_num;   
		} ud;
		struct {
			u64				iova_start;
			struct ib_fast_reg_page_list   *page_list;
			unsigned int			page_shift;
			unsigned int			page_list_len;
			u32				length;
			int				access_flags;
			u32				rkey;
		} fast_reg;
	} wr;
	u32			xrc_remote_srq_num;	
};

struct ib_recv_wr {
	struct ib_recv_wr      *next;
	u64			wr_id;
	struct ib_sge	       *sg_list;
	int			num_sge;
};

enum ib_access_flags {
	IB_ACCESS_LOCAL_WRITE	= 1,
	IB_ACCESS_REMOTE_WRITE	= (1<<1),
	IB_ACCESS_REMOTE_READ	= (1<<2),
	IB_ACCESS_REMOTE_ATOMIC	= (1<<3),
	IB_ACCESS_MW_BIND	= (1<<4)
};

struct ib_phys_buf {
	u64      addr;
	u64      size;
};

struct ib_mr_attr {
	struct ib_pd	*pd;
	u64		device_virt_addr;
	u64		size;
	int		mr_access_flags;
	u32		lkey;
	u32		rkey;
};

enum ib_mr_rereg_flags {
	IB_MR_REREG_TRANS	= 1,
	IB_MR_REREG_PD		= (1<<1),
	IB_MR_REREG_ACCESS	= (1<<2)
};

struct ib_mw_bind {
	struct ib_mr   *mr;
	u64		wr_id;
	u64		addr;
	u32		length;
	int		send_flags;
	int		mw_access_flags;
};

struct ib_fmr_attr {
	int	max_pages;
	int	max_maps;
	u8	page_shift;
};

struct ib_ucontext {
	struct ib_device       *device;
	struct list_head	pd_list;
	struct list_head	mr_list;
	struct list_head	mw_list;
	struct list_head	cq_list;
	struct list_head	qp_list;
	struct list_head	srq_list;
	struct list_head	ah_list;
	struct list_head	xrcd_list;
	int			closing;
};

struct ib_uobject {
	u64			user_handle;	
	struct ib_ucontext     *context;	
	void		       *object;		
	struct list_head	list;		
	int			id;		
	struct kref		ref;
	struct rw_semaphore	mutex;		
	int			live;
};

struct ib_udata {
	void __user *inbuf;
	void __user *outbuf;
	size_t       inlen;
	size_t       outlen;
};

struct ib_pd {
	struct ib_device       *device;
	struct ib_uobject      *uobject;
	atomic_t          	usecnt; 
};

struct ib_xrcd {
	struct ib_device       *device;
	atomic_t		usecnt; 
	struct inode	       *inode;

	struct mutex		tgt_qp_mutex;
	struct list_head	tgt_qp_list;
};

struct ib_ah {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct ib_uobject	*uobject;
};

typedef void (*ib_comp_handler)(struct ib_cq *cq, void *cq_context);

struct ib_cq {
	struct ib_device       *device;
	struct ib_uobject      *uobject;
	ib_comp_handler   	comp_handler;
	void                  (*event_handler)(struct ib_event *, void *);
	void                   *cq_context;
	int               	cqe;
	atomic_t          	usecnt; 
};

struct ib_srq {
	struct ib_device       *device;
	struct ib_pd	       *pd;
	struct ib_uobject      *uobject;
	void		      (*event_handler)(struct ib_event *, void *);
	void		       *srq_context;
	enum ib_srq_type	srq_type;
	atomic_t		usecnt;

	union {
		struct {
			struct ib_xrcd *xrcd;
			struct ib_cq   *cq;
			u32		srq_num;
		} xrc;
	} ext;
};

struct ib_qp {
	struct ib_device       *device;
	struct ib_pd	       *pd;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_srq	       *srq;
	struct ib_xrcd	       *xrcd; 
	struct list_head	xrcd_list;
	atomic_t		usecnt; 
	struct list_head	open_list;
	struct ib_qp           *real_qp;
	struct ib_uobject      *uobject;
	void                  (*event_handler)(struct ib_event *, void *);
	void		       *qp_context;
	u32			qp_num;
	enum ib_qp_type		qp_type;
};

struct ib_mr {
	struct ib_device  *device;
	struct ib_pd	  *pd;
	struct ib_uobject *uobject;
	u32		   lkey;
	u32		   rkey;
	atomic_t	   usecnt; 
};

struct ib_mw {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct ib_uobject	*uobject;
	u32			rkey;
};

struct ib_fmr {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct list_head	list;
	u32			lkey;
	u32			rkey;
};

struct ib_mad;
struct ib_grh;

enum ib_process_mad_flags {
	IB_MAD_IGNORE_MKEY	= 1,
	IB_MAD_IGNORE_BKEY	= 2,
	IB_MAD_IGNORE_ALL	= IB_MAD_IGNORE_MKEY | IB_MAD_IGNORE_BKEY
};

enum ib_mad_result {
	IB_MAD_RESULT_FAILURE  = 0,      
	IB_MAD_RESULT_SUCCESS  = 1 << 0, 
	IB_MAD_RESULT_REPLY    = 1 << 1, 
	IB_MAD_RESULT_CONSUMED = 1 << 2  
};

#define IB_DEVICE_NAME_MAX 64

struct ib_cache {
	rwlock_t                lock;
	struct ib_event_handler event_handler;
	struct ib_pkey_cache  **pkey_cache;
	struct ib_gid_cache   **gid_cache;
	u8                     *lmc_cache;
};

struct ib_dma_mapping_ops {
	int		(*mapping_error)(struct ib_device *dev,
					 u64 dma_addr);
	u64		(*map_single)(struct ib_device *dev,
				      void *ptr, size_t size,
				      enum dma_data_direction direction);
	void		(*unmap_single)(struct ib_device *dev,
					u64 addr, size_t size,
					enum dma_data_direction direction);
	u64		(*map_page)(struct ib_device *dev,
				    struct page *page, unsigned long offset,
				    size_t size,
				    enum dma_data_direction direction);
	void		(*unmap_page)(struct ib_device *dev,
				      u64 addr, size_t size,
				      enum dma_data_direction direction);
	int		(*map_sg)(struct ib_device *dev,
				  struct scatterlist *sg, int nents,
				  enum dma_data_direction direction);
	void		(*unmap_sg)(struct ib_device *dev,
				    struct scatterlist *sg, int nents,
				    enum dma_data_direction direction);
	u64		(*dma_address)(struct ib_device *dev,
				       struct scatterlist *sg);
	unsigned int	(*dma_len)(struct ib_device *dev,
				   struct scatterlist *sg);
	void		(*sync_single_for_cpu)(struct ib_device *dev,
					       u64 dma_handle,
					       size_t size,
					       enum dma_data_direction dir);
	void		(*sync_single_for_device)(struct ib_device *dev,
						  u64 dma_handle,
						  size_t size,
						  enum dma_data_direction dir);
	void		*(*alloc_coherent)(struct ib_device *dev,
					   size_t size,
					   u64 *dma_handle,
					   gfp_t flag);
	void		(*free_coherent)(struct ib_device *dev,
					 size_t size, void *cpu_addr,
					 u64 dma_handle);
};

struct iw_cm_verbs;

struct ib_device {
	struct device                *dma_device;

	char                          name[IB_DEVICE_NAME_MAX];

	struct list_head              event_handler_list;
	spinlock_t                    event_handler_lock;

	spinlock_t                    client_data_lock;
	struct list_head              core_list;
	struct list_head              client_data_list;

	struct ib_cache               cache;
	int                          *pkey_tbl_len;
	int                          *gid_tbl_len;

	int			      num_comp_vectors;

	struct iw_cm_verbs	     *iwcm;

	int		           (*get_protocol_stats)(struct ib_device *device,
							 union rdma_protocol_stats *stats);
	int		           (*query_device)(struct ib_device *device,
						   struct ib_device_attr *device_attr);
	int		           (*query_port)(struct ib_device *device,
						 u8 port_num,
						 struct ib_port_attr *port_attr);
	enum rdma_link_layer	   (*get_link_layer)(struct ib_device *device,
						     u8 port_num);
	int		           (*query_gid)(struct ib_device *device,
						u8 port_num, int index,
						union ib_gid *gid);
	int		           (*query_pkey)(struct ib_device *device,
						 u8 port_num, u16 index, u16 *pkey);
	int		           (*modify_device)(struct ib_device *device,
						    int device_modify_mask,
						    struct ib_device_modify *device_modify);
	int		           (*modify_port)(struct ib_device *device,
						  u8 port_num, int port_modify_mask,
						  struct ib_port_modify *port_modify);
	struct ib_ucontext *       (*alloc_ucontext)(struct ib_device *device,
						     struct ib_udata *udata);
	int                        (*dealloc_ucontext)(struct ib_ucontext *context);
	int                        (*mmap)(struct ib_ucontext *context,
					   struct vm_area_struct *vma);
	struct ib_pd *             (*alloc_pd)(struct ib_device *device,
					       struct ib_ucontext *context,
					       struct ib_udata *udata);
	int                        (*dealloc_pd)(struct ib_pd *pd);
	struct ib_ah *             (*create_ah)(struct ib_pd *pd,
						struct ib_ah_attr *ah_attr);
	int                        (*modify_ah)(struct ib_ah *ah,
						struct ib_ah_attr *ah_attr);
	int                        (*query_ah)(struct ib_ah *ah,
					       struct ib_ah_attr *ah_attr);
	int                        (*destroy_ah)(struct ib_ah *ah);
	struct ib_srq *            (*create_srq)(struct ib_pd *pd,
						 struct ib_srq_init_attr *srq_init_attr,
						 struct ib_udata *udata);
	int                        (*modify_srq)(struct ib_srq *srq,
						 struct ib_srq_attr *srq_attr,
						 enum ib_srq_attr_mask srq_attr_mask,
						 struct ib_udata *udata);
	int                        (*query_srq)(struct ib_srq *srq,
						struct ib_srq_attr *srq_attr);
	int                        (*destroy_srq)(struct ib_srq *srq);
	int                        (*post_srq_recv)(struct ib_srq *srq,
						    struct ib_recv_wr *recv_wr,
						    struct ib_recv_wr **bad_recv_wr);
	struct ib_qp *             (*create_qp)(struct ib_pd *pd,
						struct ib_qp_init_attr *qp_init_attr,
						struct ib_udata *udata);
	int                        (*modify_qp)(struct ib_qp *qp,
						struct ib_qp_attr *qp_attr,
						int qp_attr_mask,
						struct ib_udata *udata);
	int                        (*query_qp)(struct ib_qp *qp,
					       struct ib_qp_attr *qp_attr,
					       int qp_attr_mask,
					       struct ib_qp_init_attr *qp_init_attr);
	int                        (*destroy_qp)(struct ib_qp *qp);
	int                        (*post_send)(struct ib_qp *qp,
						struct ib_send_wr *send_wr,
						struct ib_send_wr **bad_send_wr);
	int                        (*post_recv)(struct ib_qp *qp,
						struct ib_recv_wr *recv_wr,
						struct ib_recv_wr **bad_recv_wr);
	struct ib_cq *             (*create_cq)(struct ib_device *device, int cqe,
						int comp_vector,
						struct ib_ucontext *context,
						struct ib_udata *udata);
	int                        (*modify_cq)(struct ib_cq *cq, u16 cq_count,
						u16 cq_period);
	int                        (*destroy_cq)(struct ib_cq *cq);
	int                        (*resize_cq)(struct ib_cq *cq, int cqe,
						struct ib_udata *udata);
	int                        (*poll_cq)(struct ib_cq *cq, int num_entries,
					      struct ib_wc *wc);
	int                        (*peek_cq)(struct ib_cq *cq, int wc_cnt);
	int                        (*req_notify_cq)(struct ib_cq *cq,
						    enum ib_cq_notify_flags flags);
	int                        (*req_ncomp_notif)(struct ib_cq *cq,
						      int wc_cnt);
	struct ib_mr *             (*get_dma_mr)(struct ib_pd *pd,
						 int mr_access_flags);
	struct ib_mr *             (*reg_phys_mr)(struct ib_pd *pd,
						  struct ib_phys_buf *phys_buf_array,
						  int num_phys_buf,
						  int mr_access_flags,
						  u64 *iova_start);
	struct ib_mr *             (*reg_user_mr)(struct ib_pd *pd,
						  u64 start, u64 length,
						  u64 virt_addr,
						  int mr_access_flags,
						  struct ib_udata *udata);
	int                        (*query_mr)(struct ib_mr *mr,
					       struct ib_mr_attr *mr_attr);
	int                        (*dereg_mr)(struct ib_mr *mr);
	struct ib_mr *		   (*alloc_fast_reg_mr)(struct ib_pd *pd,
					       int max_page_list_len);
	struct ib_fast_reg_page_list * (*alloc_fast_reg_page_list)(struct ib_device *device,
								   int page_list_len);
	void			   (*free_fast_reg_page_list)(struct ib_fast_reg_page_list *page_list);
	int                        (*rereg_phys_mr)(struct ib_mr *mr,
						    int mr_rereg_mask,
						    struct ib_pd *pd,
						    struct ib_phys_buf *phys_buf_array,
						    int num_phys_buf,
						    int mr_access_flags,
						    u64 *iova_start);
	struct ib_mw *             (*alloc_mw)(struct ib_pd *pd);
	int                        (*bind_mw)(struct ib_qp *qp,
					      struct ib_mw *mw,
					      struct ib_mw_bind *mw_bind);
	int                        (*dealloc_mw)(struct ib_mw *mw);
	struct ib_fmr *	           (*alloc_fmr)(struct ib_pd *pd,
						int mr_access_flags,
						struct ib_fmr_attr *fmr_attr);
	int		           (*map_phys_fmr)(struct ib_fmr *fmr,
						   u64 *page_list, int list_len,
						   u64 iova);
	int		           (*unmap_fmr)(struct list_head *fmr_list);
	int		           (*dealloc_fmr)(struct ib_fmr *fmr);
	int                        (*attach_mcast)(struct ib_qp *qp,
						   union ib_gid *gid,
						   u16 lid);
	int                        (*detach_mcast)(struct ib_qp *qp,
						   union ib_gid *gid,
						   u16 lid);
	int                        (*process_mad)(struct ib_device *device,
						  int process_mad_flags,
						  u8 port_num,
						  struct ib_wc *in_wc,
						  struct ib_grh *in_grh,
						  struct ib_mad *in_mad,
						  struct ib_mad *out_mad);
	struct ib_xrcd *	   (*alloc_xrcd)(struct ib_device *device,
						 struct ib_ucontext *ucontext,
						 struct ib_udata *udata);
	int			   (*dealloc_xrcd)(struct ib_xrcd *xrcd);

	struct ib_dma_mapping_ops   *dma_ops;

	struct module               *owner;
	struct device                dev;
	struct kobject               *ports_parent;
	struct list_head             port_list;

	enum {
		IB_DEV_UNINITIALIZED,
		IB_DEV_REGISTERED,
		IB_DEV_UNREGISTERED
	}                            reg_state;

	int			     uverbs_abi_ver;
	u64			     uverbs_cmd_mask;

	char			     node_desc[64];
	__be64			     node_guid;
	u32			     local_dma_lkey;
	u8                           node_type;
	u8                           phys_port_cnt;
};

struct ib_client {
	char  *name;
	void (*add)   (struct ib_device *);
	void (*remove)(struct ib_device *);

	struct list_head list;
};

struct ib_device *ib_alloc_device(size_t size);
void ib_dealloc_device(struct ib_device *device);

int ib_register_device(struct ib_device *device,
		       int (*port_callback)(struct ib_device *,
					    u8, struct kobject *));
void ib_unregister_device(struct ib_device *device);

int ib_register_client   (struct ib_client *client);
void ib_unregister_client(struct ib_client *client);

void *ib_get_client_data(struct ib_device *device, struct ib_client *client);
void  ib_set_client_data(struct ib_device *device, struct ib_client *client,
			 void *data);

static inline int ib_copy_from_udata(void *dest, struct ib_udata *udata, size_t len)
{
	return copy_from_user(dest, udata->inbuf, len) ? -EFAULT : 0;
}

static inline int ib_copy_to_udata(struct ib_udata *udata, void *src, size_t len)
{
	return copy_to_user(udata->outbuf, src, len) ? -EFAULT : 0;
}

int ib_modify_qp_is_ok(enum ib_qp_state cur_state, enum ib_qp_state next_state,
		       enum ib_qp_type type, enum ib_qp_attr_mask mask);

int ib_register_event_handler  (struct ib_event_handler *event_handler);
int ib_unregister_event_handler(struct ib_event_handler *event_handler);
void ib_dispatch_event(struct ib_event *event);

int ib_query_device(struct ib_device *device,
		    struct ib_device_attr *device_attr);

int ib_query_port(struct ib_device *device,
		  u8 port_num, struct ib_port_attr *port_attr);

enum rdma_link_layer rdma_port_get_link_layer(struct ib_device *device,
					       u8 port_num);

int ib_query_gid(struct ib_device *device,
		 u8 port_num, int index, union ib_gid *gid);

int ib_query_pkey(struct ib_device *device,
		  u8 port_num, u16 index, u16 *pkey);

int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify);

int ib_modify_port(struct ib_device *device,
		   u8 port_num, int port_modify_mask,
		   struct ib_port_modify *port_modify);

int ib_find_gid(struct ib_device *device, union ib_gid *gid,
		u8 *port_num, u16 *index);

int ib_find_pkey(struct ib_device *device,
		 u8 port_num, u16 pkey, u16 *index);

struct ib_pd *ib_alloc_pd(struct ib_device *device);

int ib_dealloc_pd(struct ib_pd *pd);

struct ib_ah *ib_create_ah(struct ib_pd *pd, struct ib_ah_attr *ah_attr);

int ib_init_ah_from_wc(struct ib_device *device, u8 port_num, struct ib_wc *wc,
		       struct ib_grh *grh, struct ib_ah_attr *ah_attr);

struct ib_ah *ib_create_ah_from_wc(struct ib_pd *pd, struct ib_wc *wc,
				   struct ib_grh *grh, u8 port_num);

int ib_modify_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);

int ib_query_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr);

int ib_destroy_ah(struct ib_ah *ah);

struct ib_srq *ib_create_srq(struct ib_pd *pd,
			     struct ib_srq_init_attr *srq_init_attr);

int ib_modify_srq(struct ib_srq *srq,
		  struct ib_srq_attr *srq_attr,
		  enum ib_srq_attr_mask srq_attr_mask);

int ib_query_srq(struct ib_srq *srq,
		 struct ib_srq_attr *srq_attr);

int ib_destroy_srq(struct ib_srq *srq);

static inline int ib_post_srq_recv(struct ib_srq *srq,
				   struct ib_recv_wr *recv_wr,
				   struct ib_recv_wr **bad_recv_wr)
{
	return srq->device->post_srq_recv(srq, recv_wr, bad_recv_wr);
}

struct ib_qp *ib_create_qp(struct ib_pd *pd,
			   struct ib_qp_init_attr *qp_init_attr);

int ib_modify_qp(struct ib_qp *qp,
		 struct ib_qp_attr *qp_attr,
		 int qp_attr_mask);

int ib_query_qp(struct ib_qp *qp,
		struct ib_qp_attr *qp_attr,
		int qp_attr_mask,
		struct ib_qp_init_attr *qp_init_attr);

int ib_destroy_qp(struct ib_qp *qp);

struct ib_qp *ib_open_qp(struct ib_xrcd *xrcd,
			 struct ib_qp_open_attr *qp_open_attr);

int ib_close_qp(struct ib_qp *qp);

static inline int ib_post_send(struct ib_qp *qp,
			       struct ib_send_wr *send_wr,
			       struct ib_send_wr **bad_send_wr)
{
	return qp->device->post_send(qp, send_wr, bad_send_wr);
}

static inline int ib_post_recv(struct ib_qp *qp,
			       struct ib_recv_wr *recv_wr,
			       struct ib_recv_wr **bad_recv_wr)
{
	return qp->device->post_recv(qp, recv_wr, bad_recv_wr);
}

struct ib_cq *ib_create_cq(struct ib_device *device,
			   ib_comp_handler comp_handler,
			   void (*event_handler)(struct ib_event *, void *),
			   void *cq_context, int cqe, int comp_vector);

int ib_resize_cq(struct ib_cq *cq, int cqe);

int ib_modify_cq(struct ib_cq *cq, u16 cq_count, u16 cq_period);

int ib_destroy_cq(struct ib_cq *cq);

static inline int ib_poll_cq(struct ib_cq *cq, int num_entries,
			     struct ib_wc *wc)
{
	return cq->device->poll_cq(cq, num_entries, wc);
}

int ib_peek_cq(struct ib_cq *cq, int wc_cnt);

static inline int ib_req_notify_cq(struct ib_cq *cq,
				   enum ib_cq_notify_flags flags)
{
	return cq->device->req_notify_cq(cq, flags);
}

static inline int ib_req_ncomp_notif(struct ib_cq *cq, int wc_cnt)
{
	return cq->device->req_ncomp_notif ?
		cq->device->req_ncomp_notif(cq, wc_cnt) :
		-ENOSYS;
}

struct ib_mr *ib_get_dma_mr(struct ib_pd *pd, int mr_access_flags);

static inline int ib_dma_mapping_error(struct ib_device *dev, u64 dma_addr)
{
	if (dev->dma_ops)
		return dev->dma_ops->mapping_error(dev, dma_addr);
	return dma_mapping_error(dev->dma_device, dma_addr);
}

static inline u64 ib_dma_map_single(struct ib_device *dev,
				    void *cpu_addr, size_t size,
				    enum dma_data_direction direction)
{
	if (dev->dma_ops)
		return dev->dma_ops->map_single(dev, cpu_addr, size, direction);
	return dma_map_single(dev->dma_device, cpu_addr, size, direction);
}

static inline void ib_dma_unmap_single(struct ib_device *dev,
				       u64 addr, size_t size,
				       enum dma_data_direction direction)
{
	if (dev->dma_ops)
		dev->dma_ops->unmap_single(dev, addr, size, direction);
	else
		dma_unmap_single(dev->dma_device, addr, size, direction);
}

static inline u64 ib_dma_map_single_attrs(struct ib_device *dev,
					  void *cpu_addr, size_t size,
					  enum dma_data_direction direction,
					  struct dma_attrs *attrs)
{
	return dma_map_single_attrs(dev->dma_device, cpu_addr, size,
				    direction, attrs);
}

static inline void ib_dma_unmap_single_attrs(struct ib_device *dev,
					     u64 addr, size_t size,
					     enum dma_data_direction direction,
					     struct dma_attrs *attrs)
{
	return dma_unmap_single_attrs(dev->dma_device, addr, size,
				      direction, attrs);
}

static inline u64 ib_dma_map_page(struct ib_device *dev,
				  struct page *page,
				  unsigned long offset,
				  size_t size,
					 enum dma_data_direction direction)
{
	if (dev->dma_ops)
		return dev->dma_ops->map_page(dev, page, offset, size, direction);
	return dma_map_page(dev->dma_device, page, offset, size, direction);
}

static inline void ib_dma_unmap_page(struct ib_device *dev,
				     u64 addr, size_t size,
				     enum dma_data_direction direction)
{
	if (dev->dma_ops)
		dev->dma_ops->unmap_page(dev, addr, size, direction);
	else
		dma_unmap_page(dev->dma_device, addr, size, direction);
}

static inline int ib_dma_map_sg(struct ib_device *dev,
				struct scatterlist *sg, int nents,
				enum dma_data_direction direction)
{
	if (dev->dma_ops)
		return dev->dma_ops->map_sg(dev, sg, nents, direction);
	return dma_map_sg(dev->dma_device, sg, nents, direction);
}

static inline void ib_dma_unmap_sg(struct ib_device *dev,
				   struct scatterlist *sg, int nents,
				   enum dma_data_direction direction)
{
	if (dev->dma_ops)
		dev->dma_ops->unmap_sg(dev, sg, nents, direction);
	else
		dma_unmap_sg(dev->dma_device, sg, nents, direction);
}

static inline int ib_dma_map_sg_attrs(struct ib_device *dev,
				      struct scatterlist *sg, int nents,
				      enum dma_data_direction direction,
				      struct dma_attrs *attrs)
{
	return dma_map_sg_attrs(dev->dma_device, sg, nents, direction, attrs);
}

static inline void ib_dma_unmap_sg_attrs(struct ib_device *dev,
					 struct scatterlist *sg, int nents,
					 enum dma_data_direction direction,
					 struct dma_attrs *attrs)
{
	dma_unmap_sg_attrs(dev->dma_device, sg, nents, direction, attrs);
}
static inline u64 ib_sg_dma_address(struct ib_device *dev,
				    struct scatterlist *sg)
{
	if (dev->dma_ops)
		return dev->dma_ops->dma_address(dev, sg);
	return sg_dma_address(sg);
}

static inline unsigned int ib_sg_dma_len(struct ib_device *dev,
					 struct scatterlist *sg)
{
	if (dev->dma_ops)
		return dev->dma_ops->dma_len(dev, sg);
	return sg_dma_len(sg);
}

static inline void ib_dma_sync_single_for_cpu(struct ib_device *dev,
					      u64 addr,
					      size_t size,
					      enum dma_data_direction dir)
{
	if (dev->dma_ops)
		dev->dma_ops->sync_single_for_cpu(dev, addr, size, dir);
	else
		dma_sync_single_for_cpu(dev->dma_device, addr, size, dir);
}

static inline void ib_dma_sync_single_for_device(struct ib_device *dev,
						 u64 addr,
						 size_t size,
						 enum dma_data_direction dir)
{
	if (dev->dma_ops)
		dev->dma_ops->sync_single_for_device(dev, addr, size, dir);
	else
		dma_sync_single_for_device(dev->dma_device, addr, size, dir);
}

static inline void *ib_dma_alloc_coherent(struct ib_device *dev,
					   size_t size,
					   u64 *dma_handle,
					   gfp_t flag)
{
	if (dev->dma_ops)
		return dev->dma_ops->alloc_coherent(dev, size, dma_handle, flag);
	else {
		dma_addr_t handle;
		void *ret;

		ret = dma_alloc_coherent(dev->dma_device, size, &handle, flag);
		*dma_handle = handle;
		return ret;
	}
}

static inline void ib_dma_free_coherent(struct ib_device *dev,
					size_t size, void *cpu_addr,
					u64 dma_handle)
{
	if (dev->dma_ops)
		dev->dma_ops->free_coherent(dev, size, cpu_addr, dma_handle);
	else
		dma_free_coherent(dev->dma_device, size, cpu_addr, dma_handle);
}

struct ib_mr *ib_reg_phys_mr(struct ib_pd *pd,
			     struct ib_phys_buf *phys_buf_array,
			     int num_phys_buf,
			     int mr_access_flags,
			     u64 *iova_start);

int ib_rereg_phys_mr(struct ib_mr *mr,
		     int mr_rereg_mask,
		     struct ib_pd *pd,
		     struct ib_phys_buf *phys_buf_array,
		     int num_phys_buf,
		     int mr_access_flags,
		     u64 *iova_start);

int ib_query_mr(struct ib_mr *mr, struct ib_mr_attr *mr_attr);

int ib_dereg_mr(struct ib_mr *mr);

struct ib_mr *ib_alloc_fast_reg_mr(struct ib_pd *pd, int max_page_list_len);

struct ib_fast_reg_page_list *ib_alloc_fast_reg_page_list(
				struct ib_device *device, int page_list_len);

void ib_free_fast_reg_page_list(struct ib_fast_reg_page_list *page_list);

static inline void ib_update_fast_reg_key(struct ib_mr *mr, u8 newkey)
{
	mr->lkey = (mr->lkey & 0xffffff00) | newkey;
	mr->rkey = (mr->rkey & 0xffffff00) | newkey;
}

struct ib_mw *ib_alloc_mw(struct ib_pd *pd);

static inline int ib_bind_mw(struct ib_qp *qp,
			     struct ib_mw *mw,
			     struct ib_mw_bind *mw_bind)
{
	
	return mw->device->bind_mw ?
		mw->device->bind_mw(qp, mw, mw_bind) :
		-ENOSYS;
}

int ib_dealloc_mw(struct ib_mw *mw);

struct ib_fmr *ib_alloc_fmr(struct ib_pd *pd,
			    int mr_access_flags,
			    struct ib_fmr_attr *fmr_attr);

static inline int ib_map_phys_fmr(struct ib_fmr *fmr,
				  u64 *page_list, int list_len,
				  u64 iova)
{
	return fmr->device->map_phys_fmr(fmr, page_list, list_len, iova);
}

int ib_unmap_fmr(struct list_head *fmr_list);

int ib_dealloc_fmr(struct ib_fmr *fmr);

int ib_attach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid);

int ib_detach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid);

struct ib_xrcd *ib_alloc_xrcd(struct ib_device *device);

int ib_dealloc_xrcd(struct ib_xrcd *xrcd);

#endif 
