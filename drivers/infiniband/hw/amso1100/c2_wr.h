/*
 * Copyright (c) 2005 Ammasso, Inc. All rights reserved.
 * Copyright (c) 2005 Open Grid Computing, Inc. All rights reserved.
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
#ifndef _C2_WR_H_
#define _C2_WR_H_

#ifdef CCDEBUG
#define CCWR_MAGIC		0xb07700b0
#endif

#define C2_QP_NO_ATTR_CHANGE 0xFFFFFFFF

#define C2_MAX_PRIVATE_DATA_SIZE 200

enum c2_cq_notification_type {
	C2_CQ_NOTIFICATION_TYPE_NONE = 1,
	C2_CQ_NOTIFICATION_TYPE_NEXT,
	C2_CQ_NOTIFICATION_TYPE_NEXT_SE
};

enum c2_setconfig_cmd {
	C2_CFG_ADD_ADDR = 1,
	C2_CFG_DEL_ADDR = 2,
	C2_CFG_ADD_ROUTE = 3,
	C2_CFG_DEL_ROUTE = 4
};

enum c2_getconfig_cmd {
	C2_GETCONFIG_ROUTES = 1,
	C2_GETCONFIG_ADDRS
};

enum c2wr_ids {
	CCWR_RNIC_OPEN = 1,
	CCWR_RNIC_QUERY,
	CCWR_RNIC_SETCONFIG,
	CCWR_RNIC_GETCONFIG,
	CCWR_RNIC_CLOSE,
	CCWR_CQ_CREATE,
	CCWR_CQ_QUERY,
	CCWR_CQ_MODIFY,
	CCWR_CQ_DESTROY,
	CCWR_QP_CONNECT,
	CCWR_PD_ALLOC,
	CCWR_PD_DEALLOC,
	CCWR_SRQ_CREATE,
	CCWR_SRQ_QUERY,
	CCWR_SRQ_MODIFY,
	CCWR_SRQ_DESTROY,
	CCWR_QP_CREATE,
	CCWR_QP_QUERY,
	CCWR_QP_MODIFY,
	CCWR_QP_DESTROY,
	CCWR_NSMR_STAG_ALLOC,
	CCWR_NSMR_REGISTER,
	CCWR_NSMR_PBL,
	CCWR_STAG_DEALLOC,
	CCWR_NSMR_REREGISTER,
	CCWR_SMR_REGISTER,
	CCWR_MR_QUERY,
	CCWR_MW_ALLOC,
	CCWR_MW_QUERY,
	CCWR_EP_CREATE,
	CCWR_EP_GETOPT,
	CCWR_EP_SETOPT,
	CCWR_EP_DESTROY,
	CCWR_EP_BIND,
	CCWR_EP_CONNECT,
	CCWR_EP_LISTEN,
	CCWR_EP_SHUTDOWN,
	CCWR_EP_LISTEN_CREATE,
	CCWR_EP_LISTEN_DESTROY,
	CCWR_EP_QUERY,
	CCWR_CR_ACCEPT,
	CCWR_CR_REJECT,
	CCWR_CONSOLE,
	CCWR_TERM,
	CCWR_FLASH_INIT,
	CCWR_FLASH,
	CCWR_BUF_ALLOC,
	CCWR_BUF_FREE,
	CCWR_FLASH_WRITE,
	CCWR_INIT,		



	



	CCWR_LAST,

	CCWR_SEND = 1,
	CCWR_SEND_INV,
	CCWR_SEND_SE,
	CCWR_SEND_SE_INV,
	CCWR_RDMA_WRITE,
	CCWR_RDMA_READ,
	CCWR_RDMA_READ_INV,
	CCWR_MW_BIND,
	CCWR_NSMR_FASTREG,
	CCWR_STAG_INVALIDATE,
	CCWR_RECV,
	CCWR_NOP,
	CCWR_UNIMPL,
};
#define RDMA_SEND_OPCODE_FROM_WR_ID(x)   (x+2)

enum c2_wr_type {
	C2_WR_TYPE_SEND = CCWR_SEND,
	C2_WR_TYPE_SEND_SE = CCWR_SEND_SE,
	C2_WR_TYPE_SEND_INV = CCWR_SEND_INV,
	C2_WR_TYPE_SEND_SE_INV = CCWR_SEND_SE_INV,
	C2_WR_TYPE_RDMA_WRITE = CCWR_RDMA_WRITE,
	C2_WR_TYPE_RDMA_READ = CCWR_RDMA_READ,
	C2_WR_TYPE_RDMA_READ_INV_STAG = CCWR_RDMA_READ_INV,
	C2_WR_TYPE_BIND_MW = CCWR_MW_BIND,
	C2_WR_TYPE_FASTREG_NSMR = CCWR_NSMR_FASTREG,
	C2_WR_TYPE_INV_STAG = CCWR_STAG_INVALIDATE,
	C2_WR_TYPE_RECV = CCWR_RECV,
	C2_WR_TYPE_NOP = CCWR_NOP,
};

struct c2_netaddr {
	__be32 ip_addr;
	__be32 netmask;
	u32 mtu;
};

struct c2_route {
	u32 ip_addr;		
	u32 netmask;		
	u32 flags;
	union {
		u32 ipaddr;	
		u8 enaddr[6];
	} nexthop;
};

struct c2_data_addr {
	__be32 stag;
	__be32 length;
	__be64 to;
};

enum c2_mm_flags {
	MEM_REMOTE = 0x0001,	
	MEM_VA_BASED = 0x0002,	
	MEM_PBL_COMPLETE = 0x0004,	
	MEM_LOCAL_READ = 0x0008,	
	MEM_LOCAL_WRITE = 0x0010,	
	MEM_REMOTE_READ = 0x0020,	
	MEM_REMOTE_WRITE = 0x0040,	
	MEM_WINDOW_BIND = 0x0080,	
	MEM_SHARED = 0x0100,	
	MEM_STAG_VALID = 0x0200	
};

enum c2_acf {
	C2_ACF_LOCAL_READ = MEM_LOCAL_READ,
	C2_ACF_LOCAL_WRITE = MEM_LOCAL_WRITE,
	C2_ACF_REMOTE_READ = MEM_REMOTE_READ,
	C2_ACF_REMOTE_WRITE = MEM_REMOTE_WRITE,
	C2_ACF_WINDOW_BIND = MEM_WINDOW_BIND
};

/*
 * Image types of objects written to flash
 */
#define C2_FLASH_IMG_BITFILE 1
#define C2_FLASH_IMG_OPTION_ROM 2
#define C2_FLASH_IMG_VPD 3

#define C2_MAX_TERMINATE_MESSAGE_SIZE (72)

#define WR_BUILD_STR_LEN 64


struct c2wr_hdr {
	__be32 wqe_count;

	u8 id;
	u8 result;		
	u8 sge_count;		
	u8 flags;		

	u64 context;
#ifdef CCMSGMAGIC
	u32 magic;
	u32 pad;
#endif
} __attribute__((packed));



enum c2_rnic_flags {
	RNIC_IRD_STATIC = 0x0001,
	RNIC_ORD_STATIC = 0x0002,
	RNIC_QP_STATIC = 0x0004,
	RNIC_SRQ_SUPPORTED = 0x0008,
	RNIC_PBL_BLOCK_MODE = 0x0010,
	RNIC_SRQ_MODEL_ARRIVAL = 0x0020,
	RNIC_CQ_OVF_DETECTED = 0x0040,
	RNIC_PRIV_MODE = 0x0080
};

struct c2wr_rnic_open_req {
	struct c2wr_hdr hdr;
	u64 user_context;
	__be16 flags;		
	__be16 port_num;
} __attribute__((packed));

struct c2wr_rnic_open_rep {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
} __attribute__((packed));

union c2wr_rnic_open {
	struct c2wr_rnic_open_req req;
	struct c2wr_rnic_open_rep rep;
} __attribute__((packed));

struct c2wr_rnic_query_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
} __attribute__((packed));

struct c2wr_rnic_query_rep {
	struct c2wr_hdr hdr;
	u64 user_context;
	__be32 vendor_id;
	__be32 part_number;
	__be32 hw_version;
	__be32 fw_ver_major;
	__be32 fw_ver_minor;
	__be32 fw_ver_patch;
	char fw_ver_build_str[WR_BUILD_STR_LEN];
	__be32 max_qps;
	__be32 max_qp_depth;
	u32 max_srq_depth;
	u32 max_send_sgl_depth;
	u32 max_rdma_sgl_depth;
	__be32 max_cqs;
	__be32 max_cq_depth;
	u32 max_cq_event_handlers;
	__be32 max_mrs;
	u32 max_pbl_depth;
	__be32 max_pds;
	__be32 max_global_ird;
	u32 max_global_ord;
	__be32 max_qp_ird;
	__be32 max_qp_ord;
	u32 flags;
	__be32 max_mws;
	u32 pbe_range_low;
	u32 pbe_range_high;
	u32 max_srqs;
	u32 page_size;
} __attribute__((packed));

union c2wr_rnic_query {
	struct c2wr_rnic_query_req req;
	struct c2wr_rnic_query_rep rep;
} __attribute__((packed));


struct c2wr_rnic_getconfig_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 option;		
	u64 reply_buf;
	u32 reply_buf_len;
} __attribute__((packed)) ;

struct c2wr_rnic_getconfig_rep {
	struct c2wr_hdr hdr;
	u32 option;		
	u32 count_len;		
} __attribute__((packed)) ;

union c2wr_rnic_getconfig {
	struct c2wr_rnic_getconfig_req req;
	struct c2wr_rnic_getconfig_rep rep;
} __attribute__((packed)) ;

struct c2wr_rnic_setconfig_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	__be32 option;		
	
	u8 data[0];
} __attribute__((packed)) ;

struct c2wr_rnic_setconfig_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_rnic_setconfig {
	struct c2wr_rnic_setconfig_req req;
	struct c2wr_rnic_setconfig_rep rep;
} __attribute__((packed)) ;

struct c2wr_rnic_close_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
} __attribute__((packed)) ;

struct c2wr_rnic_close_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_rnic_close {
	struct c2wr_rnic_close_req req;
	struct c2wr_rnic_close_rep rep;
} __attribute__((packed)) ;

struct c2wr_cq_create_req {
	struct c2wr_hdr hdr;
	__be64 shared_ht;
	u64 user_context;
	__be64 msg_pool;
	u32 rnic_handle;
	__be32 msg_size;
	__be32 depth;
} __attribute__((packed)) ;

struct c2wr_cq_create_rep {
	struct c2wr_hdr hdr;
	__be32 mq_index;
	__be32 adapter_shared;
	u32 cq_handle;
} __attribute__((packed)) ;

union c2wr_cq_create {
	struct c2wr_cq_create_req req;
	struct c2wr_cq_create_rep rep;
} __attribute__((packed)) ;

struct c2wr_cq_modify_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 cq_handle;
	u32 new_depth;
	u64 new_msg_pool;
} __attribute__((packed)) ;

struct c2wr_cq_modify_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_cq_modify {
	struct c2wr_cq_modify_req req;
	struct c2wr_cq_modify_rep rep;
} __attribute__((packed)) ;

struct c2wr_cq_destroy_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 cq_handle;
} __attribute__((packed)) ;

struct c2wr_cq_destroy_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_cq_destroy {
	struct c2wr_cq_destroy_req req;
	struct c2wr_cq_destroy_rep rep;
} __attribute__((packed)) ;

struct c2wr_pd_alloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_pd_alloc_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_pd_alloc {
	struct c2wr_pd_alloc_req req;
	struct c2wr_pd_alloc_rep rep;
} __attribute__((packed)) ;

struct c2wr_pd_dealloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_pd_dealloc_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_pd_dealloc {
	struct c2wr_pd_dealloc_req req;
	struct c2wr_pd_dealloc_rep rep;
} __attribute__((packed)) ;

struct c2wr_srq_create_req {
	struct c2wr_hdr hdr;
	u64 shared_ht;
	u64 user_context;
	u32 rnic_handle;
	u32 srq_depth;
	u32 srq_limit;
	u32 sgl_depth;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_srq_create_rep {
	struct c2wr_hdr hdr;
	u32 srq_depth;
	u32 sgl_depth;
	u32 msg_size;
	u32 mq_index;
	u32 mq_start;
	u32 srq_handle;
} __attribute__((packed)) ;

union c2wr_srq_create {
	struct c2wr_srq_create_req req;
	struct c2wr_srq_create_rep rep;
} __attribute__((packed)) ;

struct c2wr_srq_destroy_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 srq_handle;
} __attribute__((packed)) ;

struct c2wr_srq_destroy_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_srq_destroy {
	struct c2wr_srq_destroy_req req;
	struct c2wr_srq_destroy_rep rep;
} __attribute__((packed)) ;

enum c2wr_qp_flags {
	QP_RDMA_READ = 0x00000001,	
	QP_RDMA_WRITE = 0x00000002,	
	QP_MW_BIND = 0x00000004,	
	QP_ZERO_STAG = 0x00000008,	
	QP_REMOTE_TERMINATION = 0x00000010,	
	QP_RDMA_READ_RESPONSE = 0x00000020	
	    
};

struct c2wr_qp_create_req {
	struct c2wr_hdr hdr;
	__be64 shared_sq_ht;
	__be64 shared_rq_ht;
	u64 user_context;
	u32 rnic_handle;
	u32 sq_cq_handle;
	u32 rq_cq_handle;
	__be32 sq_depth;
	__be32 rq_depth;
	u32 srq_handle;
	u32 srq_limit;
	__be32 flags;		
	__be32 send_sgl_depth;
	__be32 recv_sgl_depth;
	__be32 rdma_write_sgl_depth;
	__be32 ord;
	__be32 ird;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_qp_create_rep {
	struct c2wr_hdr hdr;
	__be32 sq_depth;
	__be32 rq_depth;
	u32 send_sgl_depth;
	u32 recv_sgl_depth;
	u32 rdma_write_sgl_depth;
	u32 ord;
	u32 ird;
	__be32 sq_msg_size;
	__be32 sq_mq_index;
	__be32 sq_mq_start;
	__be32 rq_msg_size;
	__be32 rq_mq_index;
	__be32 rq_mq_start;
	u32 qp_handle;
} __attribute__((packed)) ;

union c2wr_qp_create {
	struct c2wr_qp_create_req req;
	struct c2wr_qp_create_rep rep;
} __attribute__((packed)) ;

struct c2wr_qp_query_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 qp_handle;
} __attribute__((packed)) ;

struct c2wr_qp_query_rep {
	struct c2wr_hdr hdr;
	u64 user_context;
	u32 rnic_handle;
	u32 sq_depth;
	u32 rq_depth;
	u32 send_sgl_depth;
	u32 rdma_write_sgl_depth;
	u32 recv_sgl_depth;
	u32 ord;
	u32 ird;
	u16 qp_state;
	u16 flags;		
	u32 qp_id;
	u32 local_addr;
	u32 remote_addr;
	u16 local_port;
	u16 remote_port;
	u32 terminate_msg_length;	
	u8 data[0];
	
} __attribute__((packed)) ;

union c2wr_qp_query {
	struct c2wr_qp_query_req req;
	struct c2wr_qp_query_rep rep;
} __attribute__((packed)) ;

struct c2wr_qp_modify_req {
	struct c2wr_hdr hdr;
	u64 stream_msg;
	u32 stream_msg_length;
	u32 rnic_handle;
	u32 qp_handle;
	__be32 next_qp_state;
	__be32 ord;
	__be32 ird;
	__be32 sq_depth;
	__be32 rq_depth;
	u32 llp_ep_handle;
} __attribute__((packed)) ;

struct c2wr_qp_modify_rep {
	struct c2wr_hdr hdr;
	u32 ord;
	u32 ird;
	u32 sq_depth;
	u32 rq_depth;
	u32 sq_msg_size;
	u32 sq_mq_index;
	u32 sq_mq_start;
	u32 rq_msg_size;
	u32 rq_mq_index;
	u32 rq_mq_start;
} __attribute__((packed)) ;

union c2wr_qp_modify {
	struct c2wr_qp_modify_req req;
	struct c2wr_qp_modify_rep rep;
} __attribute__((packed)) ;

struct c2wr_qp_destroy_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 qp_handle;
} __attribute__((packed)) ;

struct c2wr_qp_destroy_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_qp_destroy {
	struct c2wr_qp_destroy_req req;
	struct c2wr_qp_destroy_rep rep;
} __attribute__((packed)) ;

struct c2wr_qp_connect_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 qp_handle;
	__be32 remote_addr;
	__be16 remote_port;
	u16 pad;
	__be32 private_data_length;
	u8 private_data[0];	
} __attribute__((packed)) ;

struct c2wr_qp_connect {
	struct c2wr_qp_connect_req req;
	
} __attribute__((packed)) ;



struct c2wr_nsmr_stag_alloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 pbl_depth;
	u32 pd_id;
	u32 flags;
} __attribute__((packed)) ;

struct c2wr_nsmr_stag_alloc_rep {
	struct c2wr_hdr hdr;
	u32 pbl_depth;
	u32 stag_index;
} __attribute__((packed)) ;

union c2wr_nsmr_stag_alloc {
	struct c2wr_nsmr_stag_alloc_req req;
	struct c2wr_nsmr_stag_alloc_rep rep;
} __attribute__((packed)) ;

struct c2wr_nsmr_register_req {
	struct c2wr_hdr hdr;
	__be64 va;
	u32 rnic_handle;
	__be16 flags;
	u8 stag_key;
	u8 pad;
	u32 pd_id;
	__be32 pbl_depth;
	__be32 pbe_size;
	__be32 fbo;
	__be32 length;
	__be32 addrs_length;
	
	__be64 paddrs[0];
} __attribute__((packed)) ;

struct c2wr_nsmr_register_rep {
	struct c2wr_hdr hdr;
	u32 pbl_depth;
	__be32 stag_index;
} __attribute__((packed)) ;

union c2wr_nsmr_register {
	struct c2wr_nsmr_register_req req;
	struct c2wr_nsmr_register_rep rep;
} __attribute__((packed)) ;

struct c2wr_nsmr_pbl_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	__be32 flags;
	__be32 stag_index;
	__be32 addrs_length;
	
	__be64 paddrs[0];
} __attribute__((packed)) ;

struct c2wr_nsmr_pbl_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_nsmr_pbl {
	struct c2wr_nsmr_pbl_req req;
	struct c2wr_nsmr_pbl_rep rep;
} __attribute__((packed)) ;

struct c2wr_mr_query_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 stag_index;
} __attribute__((packed)) ;

struct c2wr_mr_query_rep {
	struct c2wr_hdr hdr;
	u8 stag_key;
	u8 pad[3];
	u32 pd_id;
	u32 flags;
	u32 pbl_depth;
} __attribute__((packed)) ;

union c2wr_mr_query {
	struct c2wr_mr_query_req req;
	struct c2wr_mr_query_rep rep;
} __attribute__((packed)) ;

struct c2wr_mw_query_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 stag_index;
} __attribute__((packed)) ;

struct c2wr_mw_query_rep {
	struct c2wr_hdr hdr;
	u8 stag_key;
	u8 pad[3];
	u32 pd_id;
	u32 flags;
} __attribute__((packed)) ;

union c2wr_mw_query {
	struct c2wr_mw_query_req req;
	struct c2wr_mw_query_rep rep;
} __attribute__((packed)) ;


struct c2wr_stag_dealloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	__be32 stag_index;
} __attribute__((packed)) ;

struct c2wr_stag_dealloc_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed)) ;

union c2wr_stag_dealloc {
	struct c2wr_stag_dealloc_req req;
	struct c2wr_stag_dealloc_rep rep;
} __attribute__((packed)) ;

struct c2wr_nsmr_reregister_req {
	struct c2wr_hdr hdr;
	u64 va;
	u32 rnic_handle;
	u16 flags;
	u8 stag_key;
	u8 pad;
	u32 stag_index;
	u32 pd_id;
	u32 pbl_depth;
	u32 pbe_size;
	u32 fbo;
	u32 length;
	u32 addrs_length;
	u32 pad1;
	
	u64 paddrs[0];
} __attribute__((packed)) ;

struct c2wr_nsmr_reregister_rep {
	struct c2wr_hdr hdr;
	u32 pbl_depth;
	u32 stag_index;
} __attribute__((packed)) ;

union c2wr_nsmr_reregister {
	struct c2wr_nsmr_reregister_req req;
	struct c2wr_nsmr_reregister_rep rep;
} __attribute__((packed)) ;

struct c2wr_smr_register_req {
	struct c2wr_hdr hdr;
	u64 va;
	u32 rnic_handle;
	u16 flags;
	u8 stag_key;
	u8 pad;
	u32 stag_index;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_smr_register_rep {
	struct c2wr_hdr hdr;
	u32 stag_index;
} __attribute__((packed)) ;

union c2wr_smr_register {
	struct c2wr_smr_register_req req;
	struct c2wr_smr_register_rep rep;
} __attribute__((packed)) ;

struct c2wr_mw_alloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 pd_id;
} __attribute__((packed)) ;

struct c2wr_mw_alloc_rep {
	struct c2wr_hdr hdr;
	u32 stag_index;
} __attribute__((packed)) ;

union c2wr_mw_alloc {
	struct c2wr_mw_alloc_req req;
	struct c2wr_mw_alloc_rep rep;
} __attribute__((packed)) ;


struct c2wr_user_hdr {
	struct c2wr_hdr hdr;		
} __attribute__((packed)) ;

enum c2_qp_state {
	C2_QP_STATE_IDLE = 0x01,
	C2_QP_STATE_CONNECTING = 0x02,
	C2_QP_STATE_RTS = 0x04,
	C2_QP_STATE_CLOSING = 0x08,
	C2_QP_STATE_TERMINATE = 0x10,
	C2_QP_STATE_ERROR = 0x20,
};

struct c2wr_ce {
	struct c2wr_hdr hdr;		
	u64 qp_user_context;	
	u32 qp_state;		
	u32 handle;		
	__be32 bytes_rcvd;		
	u32 stag;
} __attribute__((packed)) ;


enum {
	SQ_SIGNALED = 0x01,
	SQ_READ_FENCE = 0x02,
	SQ_FENCE = 0x04,
};

struct c2_sq_hdr {
	struct c2wr_user_hdr user_hdr;
} __attribute__((packed));

struct c2_rq_hdr {
	struct c2wr_user_hdr user_hdr;
} __attribute__((packed));

struct c2wr_send_req {
	struct c2_sq_hdr sq_hdr;
	__be32 sge_len;
	__be32 remote_stag;
	u8 data[0];		
} __attribute__((packed));

union c2wr_send {
	struct c2wr_send_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_rdma_write_req {
	struct c2_sq_hdr sq_hdr;
	__be64 remote_to;
	__be32 remote_stag;
	__be32 sge_len;
	u8 data[0];		
} __attribute__((packed));

union c2wr_rdma_write {
	struct c2wr_rdma_write_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_rdma_read_req {
	struct c2_sq_hdr sq_hdr;
	__be64 local_to;
	__be64 remote_to;
	__be32 local_stag;
	__be32 remote_stag;
	__be32 length;
} __attribute__((packed));

union c2wr_rdma_read {
	struct c2wr_rdma_read_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_mw_bind_req {
	struct c2_sq_hdr sq_hdr;
	u64 va;
	u8 stag_key;
	u8 pad[3];
	u32 mw_stag_index;
	u32 mr_stag_index;
	u32 length;
	u32 flags;
} __attribute__((packed));

union c2wr_mw_bind {
	struct c2wr_mw_bind_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_nsmr_fastreg_req {
	struct c2_sq_hdr sq_hdr;
	u64 va;
	u8 stag_key;
	u8 pad[3];
	u32 stag_index;
	u32 pbe_size;
	u32 fbo;
	u32 length;
	u32 addrs_length;
	
	u64 paddrs[0];
} __attribute__((packed));

union c2wr_nsmr_fastreg {
	struct c2wr_nsmr_fastreg_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_stag_invalidate_req {
	struct c2_sq_hdr sq_hdr;
	u8 stag_key;
	u8 pad[3];
	u32 stag_index;
} __attribute__((packed));

union c2wr_stag_invalidate {
	struct c2wr_stag_invalidate_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

union c2wr_sqwr {
	struct c2_sq_hdr sq_hdr;
	struct c2wr_send_req send;
	struct c2wr_send_req send_se;
	struct c2wr_send_req send_inv;
	struct c2wr_send_req send_se_inv;
	struct c2wr_rdma_write_req rdma_write;
	struct c2wr_rdma_read_req rdma_read;
	struct c2wr_mw_bind_req mw_bind;
	struct c2wr_nsmr_fastreg_req nsmr_fastreg;
	struct c2wr_stag_invalidate_req stag_inv;
} __attribute__((packed));


struct c2wr_rqwr {
	struct c2_rq_hdr rq_hdr;
	u8 data[0];		
} __attribute__((packed));

union c2wr_recv {
	struct c2wr_rqwr req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_ae_hdr {
	struct c2wr_hdr hdr;
	u64 user_context;	
	__be32 resource_type;	
	__be32 resource;	
	__be32 qp_state;	
} __attribute__((packed));

struct c2wr_ae_active_connect_results {
	struct c2wr_ae_hdr ae_hdr;
	__be32 laddr;
	__be32 raddr;
	__be16 lport;
	__be16 rport;
	__be32 private_data_length;
	u8 private_data[0];	
} __attribute__((packed));

struct c2wr_ae_connection_request {
	struct c2wr_ae_hdr ae_hdr;
	u32 cr_handle;		
	__be32 laddr;
	__be32 raddr;
	__be16 lport;
	__be16 rport;
	__be32 private_data_length;
	u8 private_data[0];	
} __attribute__((packed));

union c2wr_ae {
	struct c2wr_ae_hdr ae_generic;
	struct c2wr_ae_active_connect_results ae_active_connect_results;
	struct c2wr_ae_connection_request ae_connection_request;
} __attribute__((packed));

struct c2wr_init_req {
	struct c2wr_hdr hdr;
	__be64 hint_count;
	__be64 q0_host_shared;
	__be64 q1_host_shared;
	__be64 q1_host_msg_pool;
	__be64 q2_host_shared;
	__be64 q2_host_msg_pool;
} __attribute__((packed));

struct c2wr_init_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed));

union c2wr_init {
	struct c2wr_init_req req;
	struct c2wr_init_rep rep;
} __attribute__((packed));


struct c2wr_flash_init_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
} __attribute__((packed));

struct c2wr_flash_init_rep {
	struct c2wr_hdr hdr;
	u32 adapter_flash_buf_offset;
	u32 adapter_flash_len;
} __attribute__((packed));

union c2wr_flash_init {
	struct c2wr_flash_init_req req;
	struct c2wr_flash_init_rep rep;
} __attribute__((packed));

struct c2wr_flash_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 len;
} __attribute__((packed));

struct c2wr_flash_rep {
	struct c2wr_hdr hdr;
	u32 status;
} __attribute__((packed));

union c2wr_flash {
	struct c2wr_flash_req req;
	struct c2wr_flash_rep rep;
} __attribute__((packed));

struct c2wr_buf_alloc_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 size;
} __attribute__((packed));

struct c2wr_buf_alloc_rep {
	struct c2wr_hdr hdr;
	u32 offset;		
	u32 size;		
} __attribute__((packed));

union c2wr_buf_alloc {
	struct c2wr_buf_alloc_req req;
	struct c2wr_buf_alloc_rep rep;
} __attribute__((packed));

struct c2wr_buf_free_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 offset;		
	u32 size;		
} __attribute__((packed));

struct c2wr_buf_free_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed));

union c2wr_buf_free {
	struct c2wr_buf_free_req req;
	struct c2wr_ce rep;
} __attribute__((packed));

struct c2wr_flash_write_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 offset;
	u32 size;
	u32 type;
	u32 flags;
} __attribute__((packed));

struct c2wr_flash_write_rep {
	struct c2wr_hdr hdr;
	u32 status;
} __attribute__((packed));

union c2wr_flash_write {
	struct c2wr_flash_write_req req;
	struct c2wr_flash_write_rep rep;
} __attribute__((packed));


struct c2wr_ep_listen_create_req {
	struct c2wr_hdr hdr;
	u64 user_context;	
	u32 rnic_handle;
	__be32 local_addr;		
	__be16 local_port;		
	u16 pad;
	__be32 backlog;		
} __attribute__((packed));

struct c2wr_ep_listen_create_rep {
	struct c2wr_hdr hdr;
	u32 ep_handle;		
	u16 local_port;		
	u16 pad;
} __attribute__((packed));

union c2wr_ep_listen_create {
	struct c2wr_ep_listen_create_req req;
	struct c2wr_ep_listen_create_rep rep;
} __attribute__((packed));

struct c2wr_ep_listen_destroy_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 ep_handle;
} __attribute__((packed));

struct c2wr_ep_listen_destroy_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed));

union c2wr_ep_listen_destroy {
	struct c2wr_ep_listen_destroy_req req;
	struct c2wr_ep_listen_destroy_rep rep;
} __attribute__((packed));

struct c2wr_ep_query_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 ep_handle;
} __attribute__((packed));

struct c2wr_ep_query_rep {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 local_addr;
	u32 remote_addr;
	u16 local_port;
	u16 remote_port;
} __attribute__((packed));

union c2wr_ep_query {
	struct c2wr_ep_query_req req;
	struct c2wr_ep_query_rep rep;
} __attribute__((packed));


struct c2wr_cr_accept_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 qp_handle;		
	u32 ep_handle;		
	__be32 private_data_length;
	u8 private_data[0];	
} __attribute__((packed));

struct c2wr_cr_accept_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed));

union c2wr_cr_accept {
	struct c2wr_cr_accept_req req;
	struct c2wr_cr_accept_rep rep;
} __attribute__((packed));

struct  c2wr_cr_reject_req {
	struct c2wr_hdr hdr;
	u32 rnic_handle;
	u32 ep_handle;		
} __attribute__((packed));

struct  c2wr_cr_reject_rep {
	struct c2wr_hdr hdr;
} __attribute__((packed));

union c2wr_cr_reject {
	struct c2wr_cr_reject_req req;
	struct c2wr_cr_reject_rep rep;
} __attribute__((packed));


struct c2wr_console_req {
	struct c2wr_hdr hdr;		
	u64 reply_buf;		
	u32 reply_buf_len;	
	u8 command[0];		
	
} __attribute__((packed));

enum c2_console_flags {
	CONS_REPLY_TRUNCATED = 0x00000001	
} __attribute__((packed));

struct c2wr_console_rep {
	struct c2wr_hdr hdr;		
	u32 flags;
} __attribute__((packed));

union c2wr_console {
	struct c2wr_console_req req;
	struct c2wr_console_rep rep;
} __attribute__((packed));


union c2wr {
	struct c2wr_hdr hdr;
	struct c2wr_user_hdr user_hdr;
	union c2wr_rnic_open rnic_open;
	union c2wr_rnic_query rnic_query;
	union c2wr_rnic_getconfig rnic_getconfig;
	union c2wr_rnic_setconfig rnic_setconfig;
	union c2wr_rnic_close rnic_close;
	union c2wr_cq_create cq_create;
	union c2wr_cq_modify cq_modify;
	union c2wr_cq_destroy cq_destroy;
	union c2wr_pd_alloc pd_alloc;
	union c2wr_pd_dealloc pd_dealloc;
	union c2wr_srq_create srq_create;
	union c2wr_srq_destroy srq_destroy;
	union c2wr_qp_create qp_create;
	union c2wr_qp_query qp_query;
	union c2wr_qp_modify qp_modify;
	union c2wr_qp_destroy qp_destroy;
	struct c2wr_qp_connect qp_connect;
	union c2wr_nsmr_stag_alloc nsmr_stag_alloc;
	union c2wr_nsmr_register nsmr_register;
	union c2wr_nsmr_pbl nsmr_pbl;
	union c2wr_mr_query mr_query;
	union c2wr_mw_query mw_query;
	union c2wr_stag_dealloc stag_dealloc;
	union c2wr_sqwr sqwr;
	struct c2wr_rqwr rqwr;
	struct c2wr_ce ce;
	union c2wr_ae ae;
	union c2wr_init init;
	union c2wr_ep_listen_create ep_listen_create;
	union c2wr_ep_listen_destroy ep_listen_destroy;
	union c2wr_cr_accept cr_accept;
	union c2wr_cr_reject cr_reject;
	union c2wr_console console;
	union c2wr_flash_init flash_init;
	union c2wr_flash flash;
	union c2wr_buf_alloc buf_alloc;
	union c2wr_buf_free buf_free;
	union c2wr_flash_write flash_write;
} __attribute__((packed));


static __inline__ u8 c2_wr_get_id(void *wr)
{
	return ((struct c2wr_hdr *) wr)->id;
}
static __inline__ void c2_wr_set_id(void *wr, u8 id)
{
	((struct c2wr_hdr *) wr)->id = id;
}
static __inline__ u8 c2_wr_get_result(void *wr)
{
	return ((struct c2wr_hdr *) wr)->result;
}
static __inline__ void c2_wr_set_result(void *wr, u8 result)
{
	((struct c2wr_hdr *) wr)->result = result;
}
static __inline__ u8 c2_wr_get_flags(void *wr)
{
	return ((struct c2wr_hdr *) wr)->flags;
}
static __inline__ void c2_wr_set_flags(void *wr, u8 flags)
{
	((struct c2wr_hdr *) wr)->flags = flags;
}
static __inline__ u8 c2_wr_get_sge_count(void *wr)
{
	return ((struct c2wr_hdr *) wr)->sge_count;
}
static __inline__ void c2_wr_set_sge_count(void *wr, u8 sge_count)
{
	((struct c2wr_hdr *) wr)->sge_count = sge_count;
}
static __inline__ __be32 c2_wr_get_wqe_count(void *wr)
{
	return ((struct c2wr_hdr *) wr)->wqe_count;
}
static __inline__ void c2_wr_set_wqe_count(void *wr, u32 wqe_count)
{
	((struct c2wr_hdr *) wr)->wqe_count = wqe_count;
}

#endif				
