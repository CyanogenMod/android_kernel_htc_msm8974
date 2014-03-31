/*
 * Copyright (c) 2003-2007 Network Appliance, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of the Network Appliance, Inc. nor the names of
 *      its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_SUNRPC_XPRT_RDMA_H
#define _LINUX_SUNRPC_XPRT_RDMA_H

#include <linux/wait.h> 		
#include <linux/spinlock.h> 		
#include <linux/atomic.h>			

#include <rdma/rdma_cm.h>		
#include <rdma/ib_verbs.h>		

#include <linux/sunrpc/clnt.h> 		
#include <linux/sunrpc/rpc_rdma.h> 	
#include <linux/sunrpc/xprtrdma.h> 	

#define RDMA_RESOLVE_TIMEOUT	(5000)	
#define RDMA_CONNECT_RETRY_MAX	(2)	

struct rpcrdma_ia {
	struct rdma_cm_id 	*ri_id;
	struct ib_pd		*ri_pd;
	struct ib_mr		*ri_bind_mem;
	u32			ri_dma_lkey;
	int			ri_have_dma_lkey;
	struct completion	ri_done;
	int			ri_async_rc;
	enum rpcrdma_memreg	ri_memreg_strategy;
};


struct rpcrdma_ep {
	atomic_t		rep_cqcount;
	int			rep_cqinit;
	int			rep_connected;
	struct rpcrdma_ia	*rep_ia;
	struct ib_cq		*rep_cq;
	struct ib_qp_init_attr	rep_attr;
	wait_queue_head_t 	rep_connect_wait;
	struct ib_sge		rep_pad;	
	struct ib_mr		*rep_pad_mr;	
	void			(*rep_func)(struct rpcrdma_ep *);
	struct rpc_xprt		*rep_xprt;	
	struct rdma_conn_param	rep_remote_cma;
	struct sockaddr_storage	rep_remote_addr;
};

#define INIT_CQCOUNT(ep) atomic_set(&(ep)->rep_cqcount, (ep)->rep_cqinit)
#define DECR_CQCOUNT(ep) atomic_sub_return(1, &(ep)->rep_cqcount)


#define RPCRDMA_MAX_DATA_SEGS	(64)	
#define RPCRDMA_MAX_SEGS 	(RPCRDMA_MAX_DATA_SEGS + 2) 
#define MAX_RPCRDMAHDR	(\
	 \
	sizeof(struct rpcrdma_msg) + (2 * sizeof(u32)) + \
	(sizeof(struct rpcrdma_read_chunk) * RPCRDMA_MAX_SEGS) + sizeof(u32))

struct rpcrdma_buffer;

struct rpcrdma_rep {
	unsigned int	rr_len;		
	struct rpcrdma_buffer *rr_buffer; 
	struct rpc_xprt	*rr_xprt;	
	void (*rr_func)(struct rpcrdma_rep *);
	struct list_head rr_list;	
	wait_queue_head_t rr_unbind;	
	struct ib_sge	rr_iov;		
	struct ib_mr	*rr_handle;	
	char	rr_base[MAX_RPCRDMAHDR]; 
};


struct rpcrdma_mr_seg {		
	union {				
		struct ib_mr	*rl_mr;		
		struct rpcrdma_mw {		
			union {
				struct ib_mw	*mw;
				struct ib_fmr	*fmr;
				struct {
					struct ib_fast_reg_page_list *fr_pgl;
					struct ib_mr *fr_mr;
					enum { FRMR_IS_INVALID, FRMR_IS_VALID  } state;
				} frmr;
			} r;
			struct list_head mw_list;
		} *rl_mw;
	} mr_chunk;
	u64		mr_base;	
	u32		mr_rkey;	
	u32		mr_len;		
	int		mr_nsegs;	
	enum dma_data_direction	mr_dir;	
	dma_addr_t	mr_dma;		
	size_t		mr_dmalen;	
	struct page	*mr_page;	
	char		*mr_offset;	
};

struct rpcrdma_req {
	size_t 		rl_size;	
	unsigned int	rl_niovs;	
	unsigned int	rl_nchunks;	
	unsigned int	rl_connect_cookie;	
	struct rpcrdma_buffer *rl_buffer; 
	struct rpcrdma_rep	*rl_reply;
	struct rpcrdma_mr_seg rl_segments[RPCRDMA_MAX_SEGS];
	struct ib_sge	rl_send_iov[4];	
	struct ib_sge	rl_iov;		
	struct ib_mr	*rl_handle;	
	char		rl_base[MAX_RPCRDMAHDR]; 
	__u32 		rl_xdr_buf[0];	
};
#define rpcr_to_rdmar(r) \
	container_of((r)->rq_buffer, struct rpcrdma_req, rl_xdr_buf[0])

struct rpcrdma_buffer {
	spinlock_t	rb_lock;	
	atomic_t	rb_credits;	
	unsigned long	rb_cwndscale;	
	int		rb_max_requests;
	struct list_head rb_mws;	
	int		rb_send_index;
	struct rpcrdma_req	**rb_send_bufs;
	int		rb_recv_index;
	struct rpcrdma_rep	**rb_recv_bufs;
	char		*rb_pool;
};
#define rdmab_to_ia(b) (&container_of((b), struct rpcrdma_xprt, rx_buf)->rx_ia)

struct rpcrdma_create_data_internal {
	struct sockaddr_storage	addr;	
	unsigned int	max_requests;	
	unsigned int	rsize;		
	unsigned int	wsize;		
	unsigned int	inline_rsize;	
	unsigned int	inline_wsize;	
	unsigned int	padding;	
};

#define RPCRDMA_INLINE_READ_THRESHOLD(rq) \
	(rpcx_to_rdmad(rq->rq_task->tk_xprt).inline_rsize)

#define RPCRDMA_INLINE_WRITE_THRESHOLD(rq)\
	(rpcx_to_rdmad(rq->rq_task->tk_xprt).inline_wsize)

#define RPCRDMA_INLINE_PAD_VALUE(rq)\
	rpcx_to_rdmad(rq->rq_task->tk_xprt).padding

struct rpcrdma_stats {
	unsigned long		read_chunk_count;
	unsigned long		write_chunk_count;
	unsigned long		reply_chunk_count;

	unsigned long long	total_rdma_request;
	unsigned long long	total_rdma_reply;

	unsigned long long	pullup_copy_count;
	unsigned long long	fixup_copy_count;
	unsigned long		hardway_register_count;
	unsigned long		failed_marshal_count;
	unsigned long		bad_reply_count;
};

struct rpcrdma_xprt {
	struct rpc_xprt		xprt;
	struct rpcrdma_ia	rx_ia;
	struct rpcrdma_ep	rx_ep;
	struct rpcrdma_buffer	rx_buf;
	struct rpcrdma_create_data_internal rx_data;
	struct delayed_work	rdma_connect;
	struct rpcrdma_stats	rx_stats;
};

#define rpcx_to_rdmax(x) container_of(x, struct rpcrdma_xprt, xprt)
#define rpcx_to_rdmad(x) (rpcx_to_rdmax(x)->rx_data)

extern int xprt_rdma_pad_optimize;

int rpcrdma_ia_open(struct rpcrdma_xprt *, struct sockaddr *, int);
void rpcrdma_ia_close(struct rpcrdma_ia *);

int rpcrdma_ep_create(struct rpcrdma_ep *, struct rpcrdma_ia *,
				struct rpcrdma_create_data_internal *);
int rpcrdma_ep_destroy(struct rpcrdma_ep *, struct rpcrdma_ia *);
int rpcrdma_ep_connect(struct rpcrdma_ep *, struct rpcrdma_ia *);
int rpcrdma_ep_disconnect(struct rpcrdma_ep *, struct rpcrdma_ia *);

int rpcrdma_ep_post(struct rpcrdma_ia *, struct rpcrdma_ep *,
				struct rpcrdma_req *);
int rpcrdma_ep_post_recv(struct rpcrdma_ia *, struct rpcrdma_ep *,
				struct rpcrdma_rep *);

int rpcrdma_buffer_create(struct rpcrdma_buffer *, struct rpcrdma_ep *,
				struct rpcrdma_ia *,
				struct rpcrdma_create_data_internal *);
void rpcrdma_buffer_destroy(struct rpcrdma_buffer *);

struct rpcrdma_req *rpcrdma_buffer_get(struct rpcrdma_buffer *);
void rpcrdma_buffer_put(struct rpcrdma_req *);
void rpcrdma_recv_buffer_get(struct rpcrdma_req *);
void rpcrdma_recv_buffer_put(struct rpcrdma_rep *);

int rpcrdma_register_internal(struct rpcrdma_ia *, void *, int,
				struct ib_mr **, struct ib_sge *);
int rpcrdma_deregister_internal(struct rpcrdma_ia *,
				struct ib_mr *, struct ib_sge *);

int rpcrdma_register_external(struct rpcrdma_mr_seg *,
				int, int, struct rpcrdma_xprt *);
int rpcrdma_deregister_external(struct rpcrdma_mr_seg *,
				struct rpcrdma_xprt *, void *);

void rpcrdma_conn_func(struct rpcrdma_ep *);
void rpcrdma_reply_handler(struct rpcrdma_rep *);

int rpcrdma_marshal_req(struct rpc_rqst *);

extern struct kmem_cache *svc_rdma_map_cachep;
extern struct kmem_cache *svc_rdma_ctxt_cachep;
extern struct workqueue_struct *svc_rdma_wq;

#endif				
