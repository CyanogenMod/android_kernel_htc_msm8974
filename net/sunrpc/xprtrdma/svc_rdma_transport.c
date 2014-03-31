/*
 * Copyright (c) 2005-2007 Network Appliance, Inc. All rights reserved.
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
 *
 * Author: Tom Tucker <tom@opengridcomputing.com>
 */

#include <linux/sunrpc/svc_xprt.h>
#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/rpc_rdma.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <linux/sunrpc/svc_rdma.h>
#include <linux/export.h>
#include "xprt_rdma.h"

#define RPCDBG_FACILITY	RPCDBG_SVCXPRT

static struct svc_xprt *svc_rdma_create(struct svc_serv *serv,
					struct net *net,
					struct sockaddr *sa, int salen,
					int flags);
static struct svc_xprt *svc_rdma_accept(struct svc_xprt *xprt);
static void svc_rdma_release_rqst(struct svc_rqst *);
static void dto_tasklet_func(unsigned long data);
static void svc_rdma_detach(struct svc_xprt *xprt);
static void svc_rdma_free(struct svc_xprt *xprt);
static int svc_rdma_has_wspace(struct svc_xprt *xprt);
static void rq_cq_reap(struct svcxprt_rdma *xprt);
static void sq_cq_reap(struct svcxprt_rdma *xprt);

static DECLARE_TASKLET(dto_tasklet, dto_tasklet_func, 0UL);
static DEFINE_SPINLOCK(dto_lock);
static LIST_HEAD(dto_xprt_q);

static struct svc_xprt_ops svc_rdma_ops = {
	.xpo_create = svc_rdma_create,
	.xpo_recvfrom = svc_rdma_recvfrom,
	.xpo_sendto = svc_rdma_sendto,
	.xpo_release_rqst = svc_rdma_release_rqst,
	.xpo_detach = svc_rdma_detach,
	.xpo_free = svc_rdma_free,
	.xpo_prep_reply_hdr = svc_rdma_prep_reply_hdr,
	.xpo_has_wspace = svc_rdma_has_wspace,
	.xpo_accept = svc_rdma_accept,
};

struct svc_xprt_class svc_rdma_class = {
	.xcl_name = "rdma",
	.xcl_owner = THIS_MODULE,
	.xcl_ops = &svc_rdma_ops,
	.xcl_max_payload = RPCSVC_MAXPAYLOAD_TCP,
};

struct svc_rdma_op_ctxt *svc_rdma_get_context(struct svcxprt_rdma *xprt)
{
	struct svc_rdma_op_ctxt *ctxt;

	while (1) {
		ctxt = kmem_cache_alloc(svc_rdma_ctxt_cachep, GFP_KERNEL);
		if (ctxt)
			break;
		schedule_timeout_uninterruptible(msecs_to_jiffies(500));
	}
	ctxt->xprt = xprt;
	INIT_LIST_HEAD(&ctxt->dto_q);
	ctxt->count = 0;
	ctxt->frmr = NULL;
	atomic_inc(&xprt->sc_ctxt_used);
	return ctxt;
}

void svc_rdma_unmap_dma(struct svc_rdma_op_ctxt *ctxt)
{
	struct svcxprt_rdma *xprt = ctxt->xprt;
	int i;
	for (i = 0; i < ctxt->count && ctxt->sge[i].length; i++) {
		if (ctxt->sge[i].lkey == xprt->sc_dma_lkey) {
			atomic_dec(&xprt->sc_dma_used);
			ib_dma_unmap_page(xprt->sc_cm_id->device,
					    ctxt->sge[i].addr,
					    ctxt->sge[i].length,
					    ctxt->direction);
		}
	}
}

void svc_rdma_put_context(struct svc_rdma_op_ctxt *ctxt, int free_pages)
{
	struct svcxprt_rdma *xprt;
	int i;

	BUG_ON(!ctxt);
	xprt = ctxt->xprt;
	if (free_pages)
		for (i = 0; i < ctxt->count; i++)
			put_page(ctxt->pages[i]);

	kmem_cache_free(svc_rdma_ctxt_cachep, ctxt);
	atomic_dec(&xprt->sc_ctxt_used);
}

struct svc_rdma_req_map *svc_rdma_get_req_map(void)
{
	struct svc_rdma_req_map *map;
	while (1) {
		map = kmem_cache_alloc(svc_rdma_map_cachep, GFP_KERNEL);
		if (map)
			break;
		schedule_timeout_uninterruptible(msecs_to_jiffies(500));
	}
	map->count = 0;
	map->frmr = NULL;
	return map;
}

void svc_rdma_put_req_map(struct svc_rdma_req_map *map)
{
	kmem_cache_free(svc_rdma_map_cachep, map);
}

static void cq_event_handler(struct ib_event *event, void *context)
{
	struct svc_xprt *xprt = context;
	dprintk("svcrdma: received CQ event id=%d, context=%p\n",
		event->event, context);
	set_bit(XPT_CLOSE, &xprt->xpt_flags);
}

static void qp_event_handler(struct ib_event *event, void *context)
{
	struct svc_xprt *xprt = context;

	switch (event->event) {
	
	case IB_EVENT_PATH_MIG:
	case IB_EVENT_COMM_EST:
	case IB_EVENT_SQ_DRAINED:
	case IB_EVENT_QP_LAST_WQE_REACHED:
		dprintk("svcrdma: QP event %d received for QP=%p\n",
			event->event, event->element.qp);
		break;
	
	case IB_EVENT_PATH_MIG_ERR:
	case IB_EVENT_QP_FATAL:
	case IB_EVENT_QP_REQ_ERR:
	case IB_EVENT_QP_ACCESS_ERR:
	case IB_EVENT_DEVICE_FATAL:
	default:
		dprintk("svcrdma: QP ERROR event %d received for QP=%p, "
			"closing transport\n",
			event->event, event->element.qp);
		set_bit(XPT_CLOSE, &xprt->xpt_flags);
		break;
	}
}

static void dto_tasklet_func(unsigned long data)
{
	struct svcxprt_rdma *xprt;
	unsigned long flags;

	spin_lock_irqsave(&dto_lock, flags);
	while (!list_empty(&dto_xprt_q)) {
		xprt = list_entry(dto_xprt_q.next,
				  struct svcxprt_rdma, sc_dto_q);
		list_del_init(&xprt->sc_dto_q);
		spin_unlock_irqrestore(&dto_lock, flags);

		rq_cq_reap(xprt);
		sq_cq_reap(xprt);

		svc_xprt_put(&xprt->sc_xprt);
		spin_lock_irqsave(&dto_lock, flags);
	}
	spin_unlock_irqrestore(&dto_lock, flags);
}

static void rq_comp_handler(struct ib_cq *cq, void *cq_context)
{
	struct svcxprt_rdma *xprt = cq_context;
	unsigned long flags;

	
	if (atomic_read(&xprt->sc_xprt.xpt_ref.refcount)==0)
		return;

	set_bit(RDMAXPRT_RQ_PENDING, &xprt->sc_flags);

	spin_lock_irqsave(&dto_lock, flags);
	if (list_empty(&xprt->sc_dto_q)) {
		svc_xprt_get(&xprt->sc_xprt);
		list_add_tail(&xprt->sc_dto_q, &dto_xprt_q);
	}
	spin_unlock_irqrestore(&dto_lock, flags);

	
	tasklet_schedule(&dto_tasklet);
}

static void rq_cq_reap(struct svcxprt_rdma *xprt)
{
	int ret;
	struct ib_wc wc;
	struct svc_rdma_op_ctxt *ctxt = NULL;

	if (!test_and_clear_bit(RDMAXPRT_RQ_PENDING, &xprt->sc_flags))
		return;

	ib_req_notify_cq(xprt->sc_rq_cq, IB_CQ_NEXT_COMP);
	atomic_inc(&rdma_stat_rq_poll);

	while ((ret = ib_poll_cq(xprt->sc_rq_cq, 1, &wc)) > 0) {
		ctxt = (struct svc_rdma_op_ctxt *)(unsigned long)wc.wr_id;
		ctxt->wc_status = wc.status;
		ctxt->byte_len = wc.byte_len;
		svc_rdma_unmap_dma(ctxt);
		if (wc.status != IB_WC_SUCCESS) {
			
			dprintk("svcrdma: transport closing putting ctxt %p\n", ctxt);
			set_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags);
			svc_rdma_put_context(ctxt, 1);
			svc_xprt_put(&xprt->sc_xprt);
			continue;
		}
		spin_lock_bh(&xprt->sc_rq_dto_lock);
		list_add_tail(&ctxt->dto_q, &xprt->sc_rq_dto_q);
		spin_unlock_bh(&xprt->sc_rq_dto_lock);
		svc_xprt_put(&xprt->sc_xprt);
	}

	if (ctxt)
		atomic_inc(&rdma_stat_rq_prod);

	set_bit(XPT_DATA, &xprt->sc_xprt.xpt_flags);
	if (!test_bit(RDMAXPRT_CONN_PENDING, &xprt->sc_flags))
		svc_xprt_enqueue(&xprt->sc_xprt);
}

static void process_context(struct svcxprt_rdma *xprt,
			    struct svc_rdma_op_ctxt *ctxt)
{
	svc_rdma_unmap_dma(ctxt);

	switch (ctxt->wr_op) {
	case IB_WR_SEND:
		if (test_bit(RDMACTXT_F_FAST_UNREG, &ctxt->flags))
			svc_rdma_put_frmr(xprt, ctxt->frmr);
		svc_rdma_put_context(ctxt, 1);
		break;

	case IB_WR_RDMA_WRITE:
		svc_rdma_put_context(ctxt, 0);
		break;

	case IB_WR_RDMA_READ:
	case IB_WR_RDMA_READ_WITH_INV:
		if (test_bit(RDMACTXT_F_LAST_CTXT, &ctxt->flags)) {
			struct svc_rdma_op_ctxt *read_hdr = ctxt->read_hdr;
			BUG_ON(!read_hdr);
			if (test_bit(RDMACTXT_F_FAST_UNREG, &ctxt->flags))
				svc_rdma_put_frmr(xprt, ctxt->frmr);
			spin_lock_bh(&xprt->sc_rq_dto_lock);
			set_bit(XPT_DATA, &xprt->sc_xprt.xpt_flags);
			list_add_tail(&read_hdr->dto_q,
				      &xprt->sc_read_complete_q);
			spin_unlock_bh(&xprt->sc_rq_dto_lock);
			svc_xprt_enqueue(&xprt->sc_xprt);
		}
		svc_rdma_put_context(ctxt, 0);
		break;

	default:
		printk(KERN_ERR "svcrdma: unexpected completion type, "
		       "opcode=%d\n",
		       ctxt->wr_op);
		break;
	}
}

static void sq_cq_reap(struct svcxprt_rdma *xprt)
{
	struct svc_rdma_op_ctxt *ctxt = NULL;
	struct ib_wc wc;
	struct ib_cq *cq = xprt->sc_sq_cq;
	int ret;

	if (!test_and_clear_bit(RDMAXPRT_SQ_PENDING, &xprt->sc_flags))
		return;

	ib_req_notify_cq(xprt->sc_sq_cq, IB_CQ_NEXT_COMP);
	atomic_inc(&rdma_stat_sq_poll);
	while ((ret = ib_poll_cq(cq, 1, &wc)) > 0) {
		if (wc.status != IB_WC_SUCCESS)
			
			set_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags);

		
		atomic_dec(&xprt->sc_sq_count);
		wake_up(&xprt->sc_send_wait);

		ctxt = (struct svc_rdma_op_ctxt *)(unsigned long)wc.wr_id;
		if (ctxt)
			process_context(xprt, ctxt);

		svc_xprt_put(&xprt->sc_xprt);
	}

	if (ctxt)
		atomic_inc(&rdma_stat_sq_prod);
}

static void sq_comp_handler(struct ib_cq *cq, void *cq_context)
{
	struct svcxprt_rdma *xprt = cq_context;
	unsigned long flags;

	
	if (atomic_read(&xprt->sc_xprt.xpt_ref.refcount)==0)
		return;

	set_bit(RDMAXPRT_SQ_PENDING, &xprt->sc_flags);

	spin_lock_irqsave(&dto_lock, flags);
	if (list_empty(&xprt->sc_dto_q)) {
		svc_xprt_get(&xprt->sc_xprt);
		list_add_tail(&xprt->sc_dto_q, &dto_xprt_q);
	}
	spin_unlock_irqrestore(&dto_lock, flags);

	
	tasklet_schedule(&dto_tasklet);
}

static struct svcxprt_rdma *rdma_create_xprt(struct svc_serv *serv,
					     int listener)
{
	struct svcxprt_rdma *cma_xprt = kzalloc(sizeof *cma_xprt, GFP_KERNEL);

	if (!cma_xprt)
		return NULL;
	svc_xprt_init(&init_net, &svc_rdma_class, &cma_xprt->sc_xprt, serv);
	INIT_LIST_HEAD(&cma_xprt->sc_accept_q);
	INIT_LIST_HEAD(&cma_xprt->sc_dto_q);
	INIT_LIST_HEAD(&cma_xprt->sc_rq_dto_q);
	INIT_LIST_HEAD(&cma_xprt->sc_read_complete_q);
	INIT_LIST_HEAD(&cma_xprt->sc_frmr_q);
	init_waitqueue_head(&cma_xprt->sc_send_wait);

	spin_lock_init(&cma_xprt->sc_lock);
	spin_lock_init(&cma_xprt->sc_rq_dto_lock);
	spin_lock_init(&cma_xprt->sc_frmr_q_lock);

	cma_xprt->sc_ord = svcrdma_ord;

	cma_xprt->sc_max_req_size = svcrdma_max_req_size;
	cma_xprt->sc_max_requests = svcrdma_max_requests;
	cma_xprt->sc_sq_depth = svcrdma_max_requests * RPCRDMA_SQ_DEPTH_MULT;
	atomic_set(&cma_xprt->sc_sq_count, 0);
	atomic_set(&cma_xprt->sc_ctxt_used, 0);

	if (listener)
		set_bit(XPT_LISTENER, &cma_xprt->sc_xprt.xpt_flags);

	return cma_xprt;
}

struct page *svc_rdma_get_page(void)
{
	struct page *page;

	while ((page = alloc_page(GFP_KERNEL)) == NULL) {
		
		printk(KERN_INFO "svcrdma: out of memory...retrying in 1000 "
		       "jiffies.\n");
		schedule_timeout_uninterruptible(msecs_to_jiffies(1000));
	}
	return page;
}

int svc_rdma_post_recv(struct svcxprt_rdma *xprt)
{
	struct ib_recv_wr recv_wr, *bad_recv_wr;
	struct svc_rdma_op_ctxt *ctxt;
	struct page *page;
	dma_addr_t pa;
	int sge_no;
	int buflen;
	int ret;

	ctxt = svc_rdma_get_context(xprt);
	buflen = 0;
	ctxt->direction = DMA_FROM_DEVICE;
	for (sge_no = 0; buflen < xprt->sc_max_req_size; sge_no++) {
		BUG_ON(sge_no >= xprt->sc_max_sge);
		page = svc_rdma_get_page();
		ctxt->pages[sge_no] = page;
		pa = ib_dma_map_page(xprt->sc_cm_id->device,
				     page, 0, PAGE_SIZE,
				     DMA_FROM_DEVICE);
		if (ib_dma_mapping_error(xprt->sc_cm_id->device, pa))
			goto err_put_ctxt;
		atomic_inc(&xprt->sc_dma_used);
		ctxt->sge[sge_no].addr = pa;
		ctxt->sge[sge_no].length = PAGE_SIZE;
		ctxt->sge[sge_no].lkey = xprt->sc_dma_lkey;
		ctxt->count = sge_no + 1;
		buflen += PAGE_SIZE;
	}
	recv_wr.next = NULL;
	recv_wr.sg_list = &ctxt->sge[0];
	recv_wr.num_sge = ctxt->count;
	recv_wr.wr_id = (u64)(unsigned long)ctxt;

	svc_xprt_get(&xprt->sc_xprt);
	ret = ib_post_recv(xprt->sc_qp, &recv_wr, &bad_recv_wr);
	if (ret) {
		svc_rdma_unmap_dma(ctxt);
		svc_rdma_put_context(ctxt, 1);
		svc_xprt_put(&xprt->sc_xprt);
	}
	return ret;

 err_put_ctxt:
	svc_rdma_unmap_dma(ctxt);
	svc_rdma_put_context(ctxt, 1);
	return -ENOMEM;
}

static void handle_connect_req(struct rdma_cm_id *new_cma_id, size_t client_ird)
{
	struct svcxprt_rdma *listen_xprt = new_cma_id->context;
	struct svcxprt_rdma *newxprt;
	struct sockaddr *sa;

	
	newxprt = rdma_create_xprt(listen_xprt->sc_xprt.xpt_server, 0);
	if (!newxprt) {
		dprintk("svcrdma: failed to create new transport\n");
		return;
	}
	newxprt->sc_cm_id = new_cma_id;
	new_cma_id->context = newxprt;
	dprintk("svcrdma: Creating newxprt=%p, cm_id=%p, listenxprt=%p\n",
		newxprt, newxprt->sc_cm_id, listen_xprt);

	
	newxprt->sc_ord = client_ird;

	
	sa = (struct sockaddr *)&newxprt->sc_cm_id->route.addr.dst_addr;
	svc_xprt_set_remote(&newxprt->sc_xprt, sa, svc_addr_len(sa));
	sa = (struct sockaddr *)&newxprt->sc_cm_id->route.addr.src_addr;
	svc_xprt_set_local(&newxprt->sc_xprt, sa, svc_addr_len(sa));

	spin_lock_bh(&listen_xprt->sc_lock);
	list_add_tail(&newxprt->sc_accept_q, &listen_xprt->sc_accept_q);
	spin_unlock_bh(&listen_xprt->sc_lock);

	set_bit(XPT_CONN, &listen_xprt->sc_xprt.xpt_flags);
	svc_xprt_enqueue(&listen_xprt->sc_xprt);
}

static int rdma_listen_handler(struct rdma_cm_id *cma_id,
			       struct rdma_cm_event *event)
{
	struct svcxprt_rdma *xprt = cma_id->context;
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		dprintk("svcrdma: Connect request on cma_id=%p, xprt = %p, "
			"event=%d\n", cma_id, cma_id->context, event->event);
		handle_connect_req(cma_id,
				   event->param.conn.initiator_depth);
		break;

	case RDMA_CM_EVENT_ESTABLISHED:
		
		dprintk("svcrdma: Connection completed on LISTEN xprt=%p, "
			"cm_id=%p\n", xprt, cma_id);
		break;

	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		dprintk("svcrdma: Device removal xprt=%p, cm_id=%p\n",
			xprt, cma_id);
		if (xprt)
			set_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags);
		break;

	default:
		dprintk("svcrdma: Unexpected event on listening endpoint %p, "
			"event=%d\n", cma_id, event->event);
		break;
	}

	return ret;
}

static int rdma_cma_handler(struct rdma_cm_id *cma_id,
			    struct rdma_cm_event *event)
{
	struct svc_xprt *xprt = cma_id->context;
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	switch (event->event) {
	case RDMA_CM_EVENT_ESTABLISHED:
		
		svc_xprt_get(xprt);
		dprintk("svcrdma: Connection completed on DTO xprt=%p, "
			"cm_id=%p\n", xprt, cma_id);
		clear_bit(RDMAXPRT_CONN_PENDING, &rdma->sc_flags);
		svc_xprt_enqueue(xprt);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		dprintk("svcrdma: Disconnect on DTO xprt=%p, cm_id=%p\n",
			xprt, cma_id);
		if (xprt) {
			set_bit(XPT_CLOSE, &xprt->xpt_flags);
			svc_xprt_enqueue(xprt);
			svc_xprt_put(xprt);
		}
		break;
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		dprintk("svcrdma: Device removal cma_id=%p, xprt = %p, "
			"event=%d\n", cma_id, xprt, event->event);
		if (xprt) {
			set_bit(XPT_CLOSE, &xprt->xpt_flags);
			svc_xprt_enqueue(xprt);
		}
		break;
	default:
		dprintk("svcrdma: Unexpected event on DTO endpoint %p, "
			"event=%d\n", cma_id, event->event);
		break;
	}
	return 0;
}

static struct svc_xprt *svc_rdma_create(struct svc_serv *serv,
					struct net *net,
					struct sockaddr *sa, int salen,
					int flags)
{
	struct rdma_cm_id *listen_id;
	struct svcxprt_rdma *cma_xprt;
	struct svc_xprt *xprt;
	int ret;

	dprintk("svcrdma: Creating RDMA socket\n");
	if (sa->sa_family != AF_INET) {
		dprintk("svcrdma: Address family %d is not supported.\n", sa->sa_family);
		return ERR_PTR(-EAFNOSUPPORT);
	}
	cma_xprt = rdma_create_xprt(serv, 1);
	if (!cma_xprt)
		return ERR_PTR(-ENOMEM);
	xprt = &cma_xprt->sc_xprt;

	listen_id = rdma_create_id(rdma_listen_handler, cma_xprt, RDMA_PS_TCP,
				   IB_QPT_RC);
	if (IS_ERR(listen_id)) {
		ret = PTR_ERR(listen_id);
		dprintk("svcrdma: rdma_create_id failed = %d\n", ret);
		goto err0;
	}

	ret = rdma_bind_addr(listen_id, sa);
	if (ret) {
		dprintk("svcrdma: rdma_bind_addr failed = %d\n", ret);
		goto err1;
	}
	cma_xprt->sc_cm_id = listen_id;

	ret = rdma_listen(listen_id, RPCRDMA_LISTEN_BACKLOG);
	if (ret) {
		dprintk("svcrdma: rdma_listen failed = %d\n", ret);
		goto err1;
	}

	sa = (struct sockaddr *)&cma_xprt->sc_cm_id->route.addr.src_addr;
	svc_xprt_set_local(&cma_xprt->sc_xprt, sa, salen);

	return &cma_xprt->sc_xprt;

 err1:
	rdma_destroy_id(listen_id);
 err0:
	kfree(cma_xprt);
	return ERR_PTR(ret);
}

static struct svc_rdma_fastreg_mr *rdma_alloc_frmr(struct svcxprt_rdma *xprt)
{
	struct ib_mr *mr;
	struct ib_fast_reg_page_list *pl;
	struct svc_rdma_fastreg_mr *frmr;

	frmr = kmalloc(sizeof(*frmr), GFP_KERNEL);
	if (!frmr)
		goto err;

	mr = ib_alloc_fast_reg_mr(xprt->sc_pd, RPCSVC_MAXPAGES);
	if (IS_ERR(mr))
		goto err_free_frmr;

	pl = ib_alloc_fast_reg_page_list(xprt->sc_cm_id->device,
					 RPCSVC_MAXPAGES);
	if (IS_ERR(pl))
		goto err_free_mr;

	frmr->mr = mr;
	frmr->page_list = pl;
	INIT_LIST_HEAD(&frmr->frmr_list);
	return frmr;

 err_free_mr:
	ib_dereg_mr(mr);
 err_free_frmr:
	kfree(frmr);
 err:
	return ERR_PTR(-ENOMEM);
}

static void rdma_dealloc_frmr_q(struct svcxprt_rdma *xprt)
{
	struct svc_rdma_fastreg_mr *frmr;

	while (!list_empty(&xprt->sc_frmr_q)) {
		frmr = list_entry(xprt->sc_frmr_q.next,
				  struct svc_rdma_fastreg_mr, frmr_list);
		list_del_init(&frmr->frmr_list);
		ib_dereg_mr(frmr->mr);
		ib_free_fast_reg_page_list(frmr->page_list);
		kfree(frmr);
	}
}

struct svc_rdma_fastreg_mr *svc_rdma_get_frmr(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_fastreg_mr *frmr = NULL;

	spin_lock_bh(&rdma->sc_frmr_q_lock);
	if (!list_empty(&rdma->sc_frmr_q)) {
		frmr = list_entry(rdma->sc_frmr_q.next,
				  struct svc_rdma_fastreg_mr, frmr_list);
		list_del_init(&frmr->frmr_list);
		frmr->map_len = 0;
		frmr->page_list_len = 0;
	}
	spin_unlock_bh(&rdma->sc_frmr_q_lock);
	if (frmr)
		return frmr;

	return rdma_alloc_frmr(rdma);
}

static void frmr_unmap_dma(struct svcxprt_rdma *xprt,
			   struct svc_rdma_fastreg_mr *frmr)
{
	int page_no;
	for (page_no = 0; page_no < frmr->page_list_len; page_no++) {
		dma_addr_t addr = frmr->page_list->page_list[page_no];
		if (ib_dma_mapping_error(frmr->mr->device, addr))
			continue;
		atomic_dec(&xprt->sc_dma_used);
		ib_dma_unmap_page(frmr->mr->device, addr, PAGE_SIZE,
				  frmr->direction);
	}
}

void svc_rdma_put_frmr(struct svcxprt_rdma *rdma,
		       struct svc_rdma_fastreg_mr *frmr)
{
	if (frmr) {
		frmr_unmap_dma(rdma, frmr);
		spin_lock_bh(&rdma->sc_frmr_q_lock);
		BUG_ON(!list_empty(&frmr->frmr_list));
		list_add(&frmr->frmr_list, &rdma->sc_frmr_q);
		spin_unlock_bh(&rdma->sc_frmr_q_lock);
	}
}

static struct svc_xprt *svc_rdma_accept(struct svc_xprt *xprt)
{
	struct svcxprt_rdma *listen_rdma;
	struct svcxprt_rdma *newxprt = NULL;
	struct rdma_conn_param conn_param;
	struct ib_qp_init_attr qp_attr;
	struct ib_device_attr devattr;
	int uninitialized_var(dma_mr_acc);
	int need_dma_mr;
	int ret;
	int i;

	listen_rdma = container_of(xprt, struct svcxprt_rdma, sc_xprt);
	clear_bit(XPT_CONN, &xprt->xpt_flags);
	
	spin_lock_bh(&listen_rdma->sc_lock);
	if (!list_empty(&listen_rdma->sc_accept_q)) {
		newxprt = list_entry(listen_rdma->sc_accept_q.next,
				     struct svcxprt_rdma, sc_accept_q);
		list_del_init(&newxprt->sc_accept_q);
	}
	if (!list_empty(&listen_rdma->sc_accept_q))
		set_bit(XPT_CONN, &listen_rdma->sc_xprt.xpt_flags);
	spin_unlock_bh(&listen_rdma->sc_lock);
	if (!newxprt)
		return NULL;

	dprintk("svcrdma: newxprt from accept queue = %p, cm_id=%p\n",
		newxprt, newxprt->sc_cm_id);

	ret = ib_query_device(newxprt->sc_cm_id->device, &devattr);
	if (ret) {
		dprintk("svcrdma: could not query device attributes on "
			"device %p, rc=%d\n", newxprt->sc_cm_id->device, ret);
		goto errout;
	}

	newxprt->sc_max_sge = min((size_t)devattr.max_sge,
				  (size_t)RPCSVC_MAXPAGES);
	newxprt->sc_max_requests = min((size_t)devattr.max_qp_wr,
				   (size_t)svcrdma_max_requests);
	newxprt->sc_sq_depth = RPCRDMA_SQ_DEPTH_MULT * newxprt->sc_max_requests;

	newxprt->sc_ord = min_t(size_t, devattr.max_qp_rd_atom, newxprt->sc_ord);
	newxprt->sc_ord = min_t(size_t,	svcrdma_ord, newxprt->sc_ord);

	newxprt->sc_pd = ib_alloc_pd(newxprt->sc_cm_id->device);
	if (IS_ERR(newxprt->sc_pd)) {
		dprintk("svcrdma: error creating PD for connect request\n");
		goto errout;
	}
	newxprt->sc_sq_cq = ib_create_cq(newxprt->sc_cm_id->device,
					 sq_comp_handler,
					 cq_event_handler,
					 newxprt,
					 newxprt->sc_sq_depth,
					 0);
	if (IS_ERR(newxprt->sc_sq_cq)) {
		dprintk("svcrdma: error creating SQ CQ for connect request\n");
		goto errout;
	}
	newxprt->sc_rq_cq = ib_create_cq(newxprt->sc_cm_id->device,
					 rq_comp_handler,
					 cq_event_handler,
					 newxprt,
					 newxprt->sc_max_requests,
					 0);
	if (IS_ERR(newxprt->sc_rq_cq)) {
		dprintk("svcrdma: error creating RQ CQ for connect request\n");
		goto errout;
	}

	memset(&qp_attr, 0, sizeof qp_attr);
	qp_attr.event_handler = qp_event_handler;
	qp_attr.qp_context = &newxprt->sc_xprt;
	qp_attr.cap.max_send_wr = newxprt->sc_sq_depth;
	qp_attr.cap.max_recv_wr = newxprt->sc_max_requests;
	qp_attr.cap.max_send_sge = newxprt->sc_max_sge;
	qp_attr.cap.max_recv_sge = newxprt->sc_max_sge;
	qp_attr.sq_sig_type = IB_SIGNAL_REQ_WR;
	qp_attr.qp_type = IB_QPT_RC;
	qp_attr.send_cq = newxprt->sc_sq_cq;
	qp_attr.recv_cq = newxprt->sc_rq_cq;
	dprintk("svcrdma: newxprt->sc_cm_id=%p, newxprt->sc_pd=%p\n"
		"    cm_id->device=%p, sc_pd->device=%p\n"
		"    cap.max_send_wr = %d\n"
		"    cap.max_recv_wr = %d\n"
		"    cap.max_send_sge = %d\n"
		"    cap.max_recv_sge = %d\n",
		newxprt->sc_cm_id, newxprt->sc_pd,
		newxprt->sc_cm_id->device, newxprt->sc_pd->device,
		qp_attr.cap.max_send_wr,
		qp_attr.cap.max_recv_wr,
		qp_attr.cap.max_send_sge,
		qp_attr.cap.max_recv_sge);

	ret = rdma_create_qp(newxprt->sc_cm_id, newxprt->sc_pd, &qp_attr);
	if (ret) {
		qp_attr.cap.max_send_sge -= 2;
		qp_attr.cap.max_recv_sge -= 2;
		ret = rdma_create_qp(newxprt->sc_cm_id, newxprt->sc_pd,
				     &qp_attr);
		if (ret) {
			dprintk("svcrdma: failed to create QP, ret=%d\n", ret);
			goto errout;
		}
		newxprt->sc_max_sge = qp_attr.cap.max_send_sge;
		newxprt->sc_max_sge = qp_attr.cap.max_recv_sge;
		newxprt->sc_sq_depth = qp_attr.cap.max_send_wr;
		newxprt->sc_max_requests = qp_attr.cap.max_recv_wr;
	}
	newxprt->sc_qp = newxprt->sc_cm_id->qp;

	if (devattr.device_cap_flags & IB_DEVICE_MEM_MGT_EXTENSIONS) {
		newxprt->sc_frmr_pg_list_len =
			devattr.max_fast_reg_page_list_len;
		newxprt->sc_dev_caps |= SVCRDMA_DEVCAP_FAST_REG;
	}

	switch (rdma_node_get_transport(newxprt->sc_cm_id->device->node_type)) {
	case RDMA_TRANSPORT_IWARP:
		newxprt->sc_dev_caps |= SVCRDMA_DEVCAP_READ_W_INV;
		if (!(newxprt->sc_dev_caps & SVCRDMA_DEVCAP_FAST_REG)) {
			need_dma_mr = 1;
			dma_mr_acc =
				(IB_ACCESS_LOCAL_WRITE |
				 IB_ACCESS_REMOTE_WRITE);
		} else if (!(devattr.device_cap_flags & IB_DEVICE_LOCAL_DMA_LKEY)) {
			need_dma_mr = 1;
			dma_mr_acc = IB_ACCESS_LOCAL_WRITE;
		} else
			need_dma_mr = 0;
		break;
	case RDMA_TRANSPORT_IB:
		if (!(devattr.device_cap_flags & IB_DEVICE_LOCAL_DMA_LKEY)) {
			need_dma_mr = 1;
			dma_mr_acc = IB_ACCESS_LOCAL_WRITE;
		} else
			need_dma_mr = 0;
		break;
	default:
		goto errout;
	}

	
	if (need_dma_mr) {
		
		newxprt->sc_phys_mr =
			ib_get_dma_mr(newxprt->sc_pd, dma_mr_acc);
		if (IS_ERR(newxprt->sc_phys_mr)) {
			dprintk("svcrdma: Failed to create DMA MR ret=%d\n",
				ret);
			goto errout;
		}
		newxprt->sc_dma_lkey = newxprt->sc_phys_mr->lkey;
	} else
		newxprt->sc_dma_lkey =
			newxprt->sc_cm_id->device->local_dma_lkey;

	
	for (i = 0; i < newxprt->sc_max_requests; i++) {
		ret = svc_rdma_post_recv(newxprt);
		if (ret) {
			dprintk("svcrdma: failure posting receive buffers\n");
			goto errout;
		}
	}

	
	newxprt->sc_cm_id->event_handler = rdma_cma_handler;

	ib_req_notify_cq(newxprt->sc_sq_cq, IB_CQ_NEXT_COMP);
	ib_req_notify_cq(newxprt->sc_rq_cq, IB_CQ_NEXT_COMP);

	
	set_bit(RDMAXPRT_CONN_PENDING, &newxprt->sc_flags);
	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 0;
	conn_param.initiator_depth = newxprt->sc_ord;
	ret = rdma_accept(newxprt->sc_cm_id, &conn_param);
	if (ret) {
		dprintk("svcrdma: failed to accept new connection, ret=%d\n",
		       ret);
		goto errout;
	}

	dprintk("svcrdma: new connection %p accepted with the following "
		"attributes:\n"
		"    local_ip        : %pI4\n"
		"    local_port	     : %d\n"
		"    remote_ip       : %pI4\n"
		"    remote_port     : %d\n"
		"    max_sge         : %d\n"
		"    sq_depth        : %d\n"
		"    max_requests    : %d\n"
		"    ord             : %d\n",
		newxprt,
		&((struct sockaddr_in *)&newxprt->sc_cm_id->
			 route.addr.src_addr)->sin_addr.s_addr,
		ntohs(((struct sockaddr_in *)&newxprt->sc_cm_id->
		       route.addr.src_addr)->sin_port),
		&((struct sockaddr_in *)&newxprt->sc_cm_id->
			 route.addr.dst_addr)->sin_addr.s_addr,
		ntohs(((struct sockaddr_in *)&newxprt->sc_cm_id->
		       route.addr.dst_addr)->sin_port),
		newxprt->sc_max_sge,
		newxprt->sc_sq_depth,
		newxprt->sc_max_requests,
		newxprt->sc_ord);

	return &newxprt->sc_xprt;

 errout:
	dprintk("svcrdma: failure accepting new connection rc=%d.\n", ret);
	
	svc_xprt_get(&newxprt->sc_xprt);
	if (newxprt->sc_qp && !IS_ERR(newxprt->sc_qp))
		ib_destroy_qp(newxprt->sc_qp);
	rdma_destroy_id(newxprt->sc_cm_id);
	
	svc_xprt_put(&newxprt->sc_xprt);
	return NULL;
}

static void svc_rdma_release_rqst(struct svc_rqst *rqstp)
{
}

static void svc_rdma_detach(struct svc_xprt *xprt)
{
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	dprintk("svc: svc_rdma_detach(%p)\n", xprt);

	
	rdma_disconnect(rdma->sc_cm_id);
}

static void __svc_rdma_free(struct work_struct *work)
{
	struct svcxprt_rdma *rdma =
		container_of(work, struct svcxprt_rdma, sc_work);
	dprintk("svcrdma: svc_rdma_free(%p)\n", rdma);

	
	BUG_ON(atomic_read(&rdma->sc_xprt.xpt_ref.refcount) != 0);

	while (!list_empty(&rdma->sc_read_complete_q)) {
		struct svc_rdma_op_ctxt *ctxt;
		ctxt = list_entry(rdma->sc_read_complete_q.next,
				  struct svc_rdma_op_ctxt,
				  dto_q);
		list_del_init(&ctxt->dto_q);
		svc_rdma_put_context(ctxt, 1);
	}

	
	while (!list_empty(&rdma->sc_rq_dto_q)) {
		struct svc_rdma_op_ctxt *ctxt;
		ctxt = list_entry(rdma->sc_rq_dto_q.next,
				  struct svc_rdma_op_ctxt,
				  dto_q);
		list_del_init(&ctxt->dto_q);
		svc_rdma_put_context(ctxt, 1);
	}

	
	WARN_ON(atomic_read(&rdma->sc_ctxt_used) != 0);
	WARN_ON(atomic_read(&rdma->sc_dma_used) != 0);

	
	rdma_dealloc_frmr_q(rdma);

	
	if (rdma->sc_qp && !IS_ERR(rdma->sc_qp))
		ib_destroy_qp(rdma->sc_qp);

	if (rdma->sc_sq_cq && !IS_ERR(rdma->sc_sq_cq))
		ib_destroy_cq(rdma->sc_sq_cq);

	if (rdma->sc_rq_cq && !IS_ERR(rdma->sc_rq_cq))
		ib_destroy_cq(rdma->sc_rq_cq);

	if (rdma->sc_phys_mr && !IS_ERR(rdma->sc_phys_mr))
		ib_dereg_mr(rdma->sc_phys_mr);

	if (rdma->sc_pd && !IS_ERR(rdma->sc_pd))
		ib_dealloc_pd(rdma->sc_pd);

	
	rdma_destroy_id(rdma->sc_cm_id);

	kfree(rdma);
}

static void svc_rdma_free(struct svc_xprt *xprt)
{
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	INIT_WORK(&rdma->sc_work, __svc_rdma_free);
	queue_work(svc_rdma_wq, &rdma->sc_work);
}

static int svc_rdma_has_wspace(struct svc_xprt *xprt)
{
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);

	if ((rdma->sc_sq_depth - atomic_read(&rdma->sc_sq_count) < 3))
		return 0;

	if (waitqueue_active(&rdma->sc_send_wait))
		return 0;

	
	return 1;
}

int svc_rdma_fastreg(struct svcxprt_rdma *xprt,
		     struct svc_rdma_fastreg_mr *frmr)
{
	struct ib_send_wr fastreg_wr;
	u8 key;

	
	key = (u8)(frmr->mr->lkey & 0x000000FF);
	ib_update_fast_reg_key(frmr->mr, ++key);

	
	memset(&fastreg_wr, 0, sizeof fastreg_wr);
	fastreg_wr.opcode = IB_WR_FAST_REG_MR;
	fastreg_wr.send_flags = IB_SEND_SIGNALED;
	fastreg_wr.wr.fast_reg.iova_start = (unsigned long)frmr->kva;
	fastreg_wr.wr.fast_reg.page_list = frmr->page_list;
	fastreg_wr.wr.fast_reg.page_list_len = frmr->page_list_len;
	fastreg_wr.wr.fast_reg.page_shift = PAGE_SHIFT;
	fastreg_wr.wr.fast_reg.length = frmr->map_len;
	fastreg_wr.wr.fast_reg.access_flags = frmr->access_flags;
	fastreg_wr.wr.fast_reg.rkey = frmr->mr->lkey;
	return svc_rdma_send(xprt, &fastreg_wr);
}

int svc_rdma_send(struct svcxprt_rdma *xprt, struct ib_send_wr *wr)
{
	struct ib_send_wr *bad_wr, *n_wr;
	int wr_count;
	int i;
	int ret;

	if (test_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags))
		return -ENOTCONN;

	BUG_ON(wr->send_flags != IB_SEND_SIGNALED);
	wr_count = 1;
	for (n_wr = wr->next; n_wr; n_wr = n_wr->next)
		wr_count++;

	
	while (1) {
		spin_lock_bh(&xprt->sc_lock);
		if (xprt->sc_sq_depth < atomic_read(&xprt->sc_sq_count) + wr_count) {
			spin_unlock_bh(&xprt->sc_lock);
			atomic_inc(&rdma_stat_sq_starve);

			
			sq_cq_reap(xprt);

			
			wait_event(xprt->sc_send_wait,
				   atomic_read(&xprt->sc_sq_count) <
				   xprt->sc_sq_depth);
			if (test_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags))
				return -ENOTCONN;
			continue;
		}
		
		for (i = 0; i < wr_count; i++)
			svc_xprt_get(&xprt->sc_xprt);

		
		atomic_add(wr_count, &xprt->sc_sq_count);
		ret = ib_post_send(xprt->sc_qp, wr, &bad_wr);
		if (ret) {
			set_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags);
			atomic_sub(wr_count, &xprt->sc_sq_count);
			for (i = 0; i < wr_count; i ++)
				svc_xprt_put(&xprt->sc_xprt);
			dprintk("svcrdma: failed to post SQ WR rc=%d, "
			       "sc_sq_count=%d, sc_sq_depth=%d\n",
			       ret, atomic_read(&xprt->sc_sq_count),
			       xprt->sc_sq_depth);
		}
		spin_unlock_bh(&xprt->sc_lock);
		if (ret)
			wake_up(&xprt->sc_send_wait);
		break;
	}
	return ret;
}

void svc_rdma_send_error(struct svcxprt_rdma *xprt, struct rpcrdma_msg *rmsgp,
			 enum rpcrdma_errcode err)
{
	struct ib_send_wr err_wr;
	struct page *p;
	struct svc_rdma_op_ctxt *ctxt;
	u32 *va;
	int length;
	int ret;

	p = svc_rdma_get_page();
	va = page_address(p);

	
	length = svc_rdma_xdr_encode_error(xprt, rmsgp, err, va);

	ctxt = svc_rdma_get_context(xprt);
	ctxt->direction = DMA_FROM_DEVICE;
	ctxt->count = 1;
	ctxt->pages[0] = p;

	
	ctxt->sge[0].addr = ib_dma_map_page(xprt->sc_cm_id->device,
					    p, 0, length, DMA_FROM_DEVICE);
	if (ib_dma_mapping_error(xprt->sc_cm_id->device, ctxt->sge[0].addr)) {
		put_page(p);
		svc_rdma_put_context(ctxt, 1);
		return;
	}
	atomic_inc(&xprt->sc_dma_used);
	ctxt->sge[0].lkey = xprt->sc_dma_lkey;
	ctxt->sge[0].length = length;

	
	memset(&err_wr, 0, sizeof err_wr);
	ctxt->wr_op = IB_WR_SEND;
	err_wr.wr_id = (unsigned long)ctxt;
	err_wr.sg_list = ctxt->sge;
	err_wr.num_sge = 1;
	err_wr.opcode = IB_WR_SEND;
	err_wr.send_flags = IB_SEND_SIGNALED;

	
	ret = svc_rdma_send(xprt, &err_wr);
	if (ret) {
		dprintk("svcrdma: Error %d posting send for protocol error\n",
			ret);
		svc_rdma_unmap_dma(ctxt);
		svc_rdma_put_context(ctxt, 1);
	}
}
