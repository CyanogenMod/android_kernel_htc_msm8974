/*
 * Copyright (c) 2006 - 2009 Mellanox Technology Inc.  All rights reserved.
 * Copyright (C) 2009 - 2010 Bart Van Assche <bvanassche@acm.org>.
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
 *
 */

#ifndef IB_SRPT_H
#define IB_SRPT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>

#include <scsi/srp.h>

#include "ib_dm_mad.h"

#define SRP_SERVICE_NAME_PREFIX		"SRP.T10:"

enum {
	SRP_PROTOCOL = 0x0108,
	SRP_PROTOCOL_VERSION = 0x0001,
	SRP_IO_SUBCLASS = 0x609e,
	SRP_SEND_TO_IOC = 0x01,
	SRP_SEND_FROM_IOC = 0x02,
	SRP_RDMA_READ_FROM_IOC = 0x08,
	SRP_RDMA_WRITE_FROM_IOC = 0x20,

	SRP_MTCH_ACTION = 0x03, 
	SRP_LOSOLNT = 0x10, 
	SRP_CRSOLNT = 0x20, 
	SRP_AESOLNT = 0x40, 

	SRP_SCSOLNT = 0x02, 
	SRP_UCSOLNT = 0x04, 

	SRP_SOLNT = 0x01, 

	
	SRP_TSK_MGMT_SUCCESS = 0x00,
	SRP_TSK_MGMT_FUNC_NOT_SUPP = 0x04,
	SRP_TSK_MGMT_FAILED = 0x05,

	
	SRP_CMD_SIMPLE_Q = 0x0,
	SRP_CMD_HEAD_OF_Q = 0x1,
	SRP_CMD_ORDERED_Q = 0x2,
	SRP_CMD_ACA = 0x4,

	SRP_LOGIN_RSP_MULTICHAN_NO_CHAN = 0x0,
	SRP_LOGIN_RSP_MULTICHAN_TERMINATED = 0x1,
	SRP_LOGIN_RSP_MULTICHAN_MAINTAINED = 0x2,

	SRPT_DEF_SG_TABLESIZE = 128,
	SRPT_DEF_SG_PER_WQE = 16,

	MIN_SRPT_SQ_SIZE = 16,
	DEF_SRPT_SQ_SIZE = 4096,
	SRPT_RQ_SIZE = 128,
	MIN_SRPT_SRQ_SIZE = 4,
	DEFAULT_SRPT_SRQ_SIZE = 4095,
	MAX_SRPT_SRQ_SIZE = 65535,
	MAX_SRPT_RDMA_SIZE = 1U << 24,
	MAX_SRPT_RSP_SIZE = 1024,

	MIN_MAX_REQ_SIZE = 996,
	DEFAULT_MAX_REQ_SIZE
		= sizeof(struct srp_cmd)
		+ sizeof(struct srp_indirect_buf)
		+ 128 * sizeof(struct srp_direct_buf),

	MIN_MAX_RSP_SIZE = sizeof(struct srp_rsp) + 4,
	DEFAULT_MAX_RSP_SIZE = 256, 

	DEFAULT_MAX_RDMA_SIZE = 65536,
};

enum srpt_opcode {
	SRPT_RECV,
	SRPT_SEND,
	SRPT_RDMA_MID,
	SRPT_RDMA_ABORT,
	SRPT_RDMA_READ_LAST,
	SRPT_RDMA_WRITE_LAST,
};

static inline u64 encode_wr_id(u8 opcode, u32 idx)
{
	return ((u64)opcode << 32) | idx;
}
static inline enum srpt_opcode opcode_from_wr_id(u64 wr_id)
{
	return wr_id >> 32;
}
static inline u32 idx_from_wr_id(u64 wr_id)
{
	return (u32)wr_id;
}

struct rdma_iu {
	u64		raddr;
	u32		rkey;
	struct ib_sge	*sge;
	u32		sge_cnt;
	int		mem_id;
};

enum srpt_command_state {
	SRPT_STATE_NEW		 = 0,
	SRPT_STATE_NEED_DATA	 = 1,
	SRPT_STATE_DATA_IN	 = 2,
	SRPT_STATE_CMD_RSP_SENT	 = 3,
	SRPT_STATE_MGMT		 = 4,
	SRPT_STATE_MGMT_RSP_SENT = 5,
	SRPT_STATE_DONE		 = 6,
};

struct srpt_ioctx {
	void			*buf;
	dma_addr_t		dma;
	uint32_t		index;
};

struct srpt_recv_ioctx {
	struct srpt_ioctx	ioctx;
	struct list_head	wait_list;
};

struct srpt_send_ioctx {
	struct srpt_ioctx	ioctx;
	struct srpt_rdma_ch	*ch;
	struct kref		 kref;
	struct rdma_iu		*rdma_ius;
	struct srp_direct_buf	*rbufs;
	struct srp_direct_buf	single_rbuf;
	struct scatterlist	*sg;
	struct list_head	free_list;
	spinlock_t		spinlock;
	enum srpt_command_state	state;
	bool			rdma_aborted;
	struct se_cmd		cmd;
	struct completion	tx_done;
	u64			tag;
	int			sg_cnt;
	int			mapped_sg_count;
	u16			n_rdma_ius;
	u8			n_rdma;
	u8			n_rbuf;
	bool			queue_status_only;
	u8			sense_data[SCSI_SENSE_BUFFERSIZE];
};

enum rdma_ch_state {
	CH_CONNECTING,
	CH_LIVE,
	CH_DISCONNECTING,
	CH_DRAINING,
	CH_RELEASING
};

struct srpt_rdma_ch {
	wait_queue_head_t	wait_queue;
	struct task_struct	*thread;
	struct ib_cm_id		*cm_id;
	struct ib_qp		*qp;
	struct ib_cq		*cq;
	int			rq_size;
	u32			rsp_size;
	atomic_t		sq_wr_avail;
	struct srpt_port	*sport;
	u8			i_port_id[16];
	u8			t_port_id[16];
	int			max_ti_iu_len;
	atomic_t		req_lim;
	atomic_t		req_lim_delta;
	spinlock_t		spinlock;
	struct list_head	free_list;
	enum rdma_ch_state	state;
	struct srpt_send_ioctx	**ioctx_ring;
	struct ib_wc		wc[16];
	struct list_head	list;
	struct list_head	cmd_wait_list;
	struct se_session	*sess;
	u8			sess_name[36];
	struct work_struct	release_work;
	struct completion	*release_done;
};

struct srpt_port_attrib {
	u32			srp_max_rdma_size;
	u32			srp_max_rsp_size;
	u32			srp_sq_size;
};

struct srpt_port {
	struct srpt_device	*sdev;
	struct ib_mad_agent	*mad_agent;
	bool			enabled;
	u8			port_guid[64];
	u8			port;
	u16			sm_lid;
	u16			lid;
	union ib_gid		gid;
	spinlock_t		port_acl_lock;
	struct work_struct	work;
	struct se_portal_group	port_tpg_1;
	struct se_wwn		port_wwn;
	struct list_head	port_acl_list;
	struct srpt_port_attrib port_attrib;
};

struct srpt_device {
	struct ib_device	*device;
	struct ib_pd		*pd;
	struct ib_mr		*mr;
	struct ib_srq		*srq;
	struct ib_cm_id		*cm_id;
	struct ib_device_attr	dev_attr;
	int			srq_size;
	struct srpt_recv_ioctx	**ioctx_ring;
	struct list_head	rch_list;
	wait_queue_head_t	ch_releaseQ;
	spinlock_t		spinlock;
	struct srpt_port	port[2];
	struct ib_event_handler	event_handler;
	struct list_head	list;
};

struct srpt_node_acl {
	u8			i_port_id[16];
	struct srpt_port	*sport;
	struct se_node_acl	nacl;
	struct list_head	list;
};


enum {
	SCSI_TRANSPORTID_PROTOCOLID_SRP	= 4,
};

struct spc_rdma_transport_id {
	uint8_t protocol_identifier;
	uint8_t reserved[7];
	uint8_t i_port_id[16];
};

#endif				
