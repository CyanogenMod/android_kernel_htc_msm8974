/*
 * Copyright (c) 2005-2006 Network Appliance, Inc. All rights reserved.
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

#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/rpc_rdma.h>
#include <linux/spinlock.h>
#include <asm/unaligned.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <linux/sunrpc/svc_rdma.h>

#define RPCDBG_FACILITY	RPCDBG_SVCXPRT

static void rdma_build_arg_xdr(struct svc_rqst *rqstp,
			       struct svc_rdma_op_ctxt *ctxt,
			       u32 byte_count)
{
	struct page *page;
	u32 bc;
	int sge_no;

	
	page = ctxt->pages[0];
	put_page(rqstp->rq_pages[0]);
	rqstp->rq_pages[0] = page;

	
	rqstp->rq_arg.head[0].iov_base = page_address(page);
	rqstp->rq_arg.head[0].iov_len = min(byte_count, ctxt->sge[0].length);
	rqstp->rq_arg.len = byte_count;
	rqstp->rq_arg.buflen = byte_count;

	
	bc = byte_count - rqstp->rq_arg.head[0].iov_len;

	
	rqstp->rq_arg.page_len = bc;
	rqstp->rq_arg.page_base = 0;
	rqstp->rq_arg.pages = &rqstp->rq_pages[1];
	sge_no = 1;
	while (bc && sge_no < ctxt->count) {
		page = ctxt->pages[sge_no];
		put_page(rqstp->rq_pages[sge_no]);
		rqstp->rq_pages[sge_no] = page;
		bc -= min(bc, ctxt->sge[sge_no].length);
		rqstp->rq_arg.buflen += ctxt->sge[sge_no].length;
		sge_no++;
	}
	rqstp->rq_respages = &rqstp->rq_pages[sge_no];

	BUG_ON(bc && (sge_no == ctxt->count));
	BUG_ON((rqstp->rq_arg.head[0].iov_len + rqstp->rq_arg.page_len)
	       != byte_count);
	BUG_ON(rqstp->rq_arg.len != byte_count);

	
	bc = sge_no;
	while (sge_no < ctxt->count) {
		page = ctxt->pages[sge_no++];
		put_page(page);
	}
	ctxt->count = bc;

	
	rqstp->rq_arg.tail[0].iov_base = NULL;
	rqstp->rq_arg.tail[0].iov_len = 0;
}

static int map_read_chunks(struct svcxprt_rdma *xprt,
			   struct svc_rqst *rqstp,
			   struct svc_rdma_op_ctxt *head,
			   struct rpcrdma_msg *rmsgp,
			   struct svc_rdma_req_map *rpl_map,
			   struct svc_rdma_req_map *chl_map,
			   int ch_count,
			   int byte_count)
{
	int sge_no;
	int sge_bytes;
	int page_off;
	int page_no;
	int ch_bytes;
	int ch_no;
	struct rpcrdma_read_chunk *ch;

	sge_no = 0;
	page_no = 0;
	page_off = 0;
	ch = (struct rpcrdma_read_chunk *)&rmsgp->rm_body.rm_chunks[0];
	ch_no = 0;
	ch_bytes = ntohl(ch->rc_target.rs_length);
	head->arg.head[0] = rqstp->rq_arg.head[0];
	head->arg.tail[0] = rqstp->rq_arg.tail[0];
	head->arg.pages = &head->pages[head->count];
	head->hdr_count = head->count; 
	head->arg.page_base = 0;
	head->arg.page_len = ch_bytes;
	head->arg.len = rqstp->rq_arg.len + ch_bytes;
	head->arg.buflen = rqstp->rq_arg.buflen + ch_bytes;
	head->count++;
	chl_map->ch[0].start = 0;
	while (byte_count) {
		rpl_map->sge[sge_no].iov_base =
			page_address(rqstp->rq_arg.pages[page_no]) + page_off;
		sge_bytes = min_t(int, PAGE_SIZE-page_off, ch_bytes);
		rpl_map->sge[sge_no].iov_len = sge_bytes;
		head->arg.pages[page_no] = rqstp->rq_arg.pages[page_no];
		rqstp->rq_respages = &rqstp->rq_arg.pages[page_no+1];

		byte_count -= sge_bytes;
		ch_bytes -= sge_bytes;
		sge_no++;
		if (ch_bytes == 0) {
			chl_map->ch[ch_no].count =
				sge_no - chl_map->ch[ch_no].start;
			ch_no++;
			ch++;
			chl_map->ch[ch_no].start = sge_no;
			ch_bytes = ntohl(ch->rc_target.rs_length);
			
			if (byte_count) {
				head->arg.page_len += ch_bytes;
				head->arg.len += ch_bytes;
				head->arg.buflen += ch_bytes;
			}
		}
		if ((sge_bytes + page_off) == PAGE_SIZE) {
			page_no++;
			page_off = 0;
			if (byte_count)
				head->count++;
		} else
			page_off += sge_bytes;
	}
	BUG_ON(byte_count != 0);
	return sge_no;
}

static int fast_reg_read_chunks(struct svcxprt_rdma *xprt,
				struct svc_rqst *rqstp,
				struct svc_rdma_op_ctxt *head,
				struct rpcrdma_msg *rmsgp,
				struct svc_rdma_req_map *rpl_map,
				struct svc_rdma_req_map *chl_map,
				int ch_count,
				int byte_count)
{
	int page_no;
	int ch_no;
	u32 offset;
	struct rpcrdma_read_chunk *ch;
	struct svc_rdma_fastreg_mr *frmr;
	int ret = 0;

	frmr = svc_rdma_get_frmr(xprt);
	if (IS_ERR(frmr))
		return -ENOMEM;

	head->frmr = frmr;
	head->arg.head[0] = rqstp->rq_arg.head[0];
	head->arg.tail[0] = rqstp->rq_arg.tail[0];
	head->arg.pages = &head->pages[head->count];
	head->hdr_count = head->count; 
	head->arg.page_base = 0;
	head->arg.page_len = byte_count;
	head->arg.len = rqstp->rq_arg.len + byte_count;
	head->arg.buflen = rqstp->rq_arg.buflen + byte_count;

	
	frmr->kva = page_address(rqstp->rq_arg.pages[0]);
	frmr->direction = DMA_FROM_DEVICE;
	frmr->access_flags = (IB_ACCESS_LOCAL_WRITE|IB_ACCESS_REMOTE_WRITE);
	frmr->map_len = byte_count;
	frmr->page_list_len = PAGE_ALIGN(byte_count) >> PAGE_SHIFT;
	for (page_no = 0; page_no < frmr->page_list_len; page_no++) {
		frmr->page_list->page_list[page_no] =
			ib_dma_map_page(xprt->sc_cm_id->device,
					rqstp->rq_arg.pages[page_no], 0,
					PAGE_SIZE, DMA_FROM_DEVICE);
		if (ib_dma_mapping_error(xprt->sc_cm_id->device,
					 frmr->page_list->page_list[page_no]))
			goto fatal_err;
		atomic_inc(&xprt->sc_dma_used);
		head->arg.pages[page_no] = rqstp->rq_arg.pages[page_no];
	}
	head->count += page_no;

	
	rqstp->rq_respages = &rqstp->rq_arg.pages[page_no];

	
	offset = 0;
	ch = (struct rpcrdma_read_chunk *)&rmsgp->rm_body.rm_chunks[0];
	for (ch_no = 0; ch_no < ch_count; ch_no++) {
		int len = ntohl(ch->rc_target.rs_length);
		rpl_map->sge[ch_no].iov_base = frmr->kva + offset;
		rpl_map->sge[ch_no].iov_len = len;
		chl_map->ch[ch_no].count = 1;
		chl_map->ch[ch_no].start = ch_no;
		offset += len;
		ch++;
	}

	ret = svc_rdma_fastreg(xprt, frmr);
	if (ret)
		goto fatal_err;

	return ch_no;

 fatal_err:
	printk("svcrdma: error fast registering xdr for xprt %p", xprt);
	svc_rdma_put_frmr(xprt, frmr);
	return -EIO;
}

static int rdma_set_ctxt_sge(struct svcxprt_rdma *xprt,
			     struct svc_rdma_op_ctxt *ctxt,
			     struct svc_rdma_fastreg_mr *frmr,
			     struct kvec *vec,
			     u64 *sgl_offset,
			     int count)
{
	int i;
	unsigned long off;

	ctxt->count = count;
	ctxt->direction = DMA_FROM_DEVICE;
	for (i = 0; i < count; i++) {
		ctxt->sge[i].length = 0; 
		if (!frmr) {
			BUG_ON(!virt_to_page(vec[i].iov_base));
			off = (unsigned long)vec[i].iov_base & ~PAGE_MASK;
			ctxt->sge[i].addr =
				ib_dma_map_page(xprt->sc_cm_id->device,
						virt_to_page(vec[i].iov_base),
						off,
						vec[i].iov_len,
						DMA_FROM_DEVICE);
			if (ib_dma_mapping_error(xprt->sc_cm_id->device,
						 ctxt->sge[i].addr))
				return -EINVAL;
			ctxt->sge[i].lkey = xprt->sc_dma_lkey;
			atomic_inc(&xprt->sc_dma_used);
		} else {
			ctxt->sge[i].addr = (unsigned long)vec[i].iov_base;
			ctxt->sge[i].lkey = frmr->mr->lkey;
		}
		ctxt->sge[i].length = vec[i].iov_len;
		*sgl_offset = *sgl_offset + vec[i].iov_len;
	}
	return 0;
}

static int rdma_read_max_sge(struct svcxprt_rdma *xprt, int sge_count)
{
	if ((rdma_node_get_transport(xprt->sc_cm_id->device->node_type) ==
	     RDMA_TRANSPORT_IWARP) &&
	    sge_count > 1)
		return 1;
	else
		return min_t(int, sge_count, xprt->sc_max_sge);
}

static int rdma_read_xdr(struct svcxprt_rdma *xprt,
			 struct rpcrdma_msg *rmsgp,
			 struct svc_rqst *rqstp,
			 struct svc_rdma_op_ctxt *hdr_ctxt)
{
	struct ib_send_wr read_wr;
	struct ib_send_wr inv_wr;
	int err = 0;
	int ch_no;
	int ch_count;
	int byte_count;
	int sge_count;
	u64 sgl_offset;
	struct rpcrdma_read_chunk *ch;
	struct svc_rdma_op_ctxt *ctxt = NULL;
	struct svc_rdma_req_map *rpl_map;
	struct svc_rdma_req_map *chl_map;

	
	ch = svc_rdma_get_read_chunk(rmsgp);
	if (!ch)
		return 0;

	svc_rdma_rcl_chunk_counts(ch, &ch_count, &byte_count);
	if (ch_count > RPCSVC_MAXPAGES)
		return -EINVAL;

	
	rpl_map = svc_rdma_get_req_map();
	chl_map = svc_rdma_get_req_map();

	if (!xprt->sc_frmr_pg_list_len)
		sge_count = map_read_chunks(xprt, rqstp, hdr_ctxt, rmsgp,
					    rpl_map, chl_map, ch_count,
					    byte_count);
	else
		sge_count = fast_reg_read_chunks(xprt, rqstp, hdr_ctxt, rmsgp,
						 rpl_map, chl_map, ch_count,
						 byte_count);
	if (sge_count < 0) {
		err = -EIO;
		goto out;
	}

	sgl_offset = 0;
	ch_no = 0;

	for (ch = (struct rpcrdma_read_chunk *)&rmsgp->rm_body.rm_chunks[0];
	     ch->rc_discrim != 0; ch++, ch_no++) {
		u64 rs_offset;
next_sge:
		ctxt = svc_rdma_get_context(xprt);
		ctxt->direction = DMA_FROM_DEVICE;
		ctxt->frmr = hdr_ctxt->frmr;
		ctxt->read_hdr = NULL;
		clear_bit(RDMACTXT_F_LAST_CTXT, &ctxt->flags);
		clear_bit(RDMACTXT_F_FAST_UNREG, &ctxt->flags);

		
		memset(&read_wr, 0, sizeof read_wr);
		read_wr.wr_id = (unsigned long)ctxt;
		read_wr.opcode = IB_WR_RDMA_READ;
		ctxt->wr_op = read_wr.opcode;
		read_wr.send_flags = IB_SEND_SIGNALED;
		read_wr.wr.rdma.rkey = ntohl(ch->rc_target.rs_handle);
		xdr_decode_hyper((__be32 *)&ch->rc_target.rs_offset,
				 &rs_offset);
		read_wr.wr.rdma.remote_addr = rs_offset + sgl_offset;
		read_wr.sg_list = ctxt->sge;
		read_wr.num_sge =
			rdma_read_max_sge(xprt, chl_map->ch[ch_no].count);
		err = rdma_set_ctxt_sge(xprt, ctxt, hdr_ctxt->frmr,
					&rpl_map->sge[chl_map->ch[ch_no].start],
					&sgl_offset,
					read_wr.num_sge);
		if (err) {
			svc_rdma_unmap_dma(ctxt);
			svc_rdma_put_context(ctxt, 0);
			goto out;
		}
		if (((ch+1)->rc_discrim == 0) &&
		    (read_wr.num_sge == chl_map->ch[ch_no].count)) {
			set_bit(RDMACTXT_F_LAST_CTXT, &ctxt->flags);
			if (hdr_ctxt->frmr) {
				set_bit(RDMACTXT_F_FAST_UNREG, &ctxt->flags);
				if (xprt->sc_dev_caps &
				    SVCRDMA_DEVCAP_READ_W_INV) {
					read_wr.opcode =
						IB_WR_RDMA_READ_WITH_INV;
					ctxt->wr_op = read_wr.opcode;
					read_wr.ex.invalidate_rkey =
						ctxt->frmr->mr->lkey;
				} else {
					
					memset(&inv_wr, 0, sizeof inv_wr);
					inv_wr.opcode = IB_WR_LOCAL_INV;
					inv_wr.send_flags = IB_SEND_SIGNALED;
					inv_wr.ex.invalidate_rkey =
						hdr_ctxt->frmr->mr->lkey;
					read_wr.next = &inv_wr;
				}
			}
			ctxt->read_hdr = hdr_ctxt;
		}
		
		err = svc_rdma_send(xprt, &read_wr);
		if (err) {
			printk(KERN_ERR "svcrdma: Error %d posting RDMA_READ\n",
			       err);
			set_bit(XPT_CLOSE, &xprt->sc_xprt.xpt_flags);
			svc_rdma_unmap_dma(ctxt);
			svc_rdma_put_context(ctxt, 0);
			goto out;
		}
		atomic_inc(&rdma_stat_read);

		if (read_wr.num_sge < chl_map->ch[ch_no].count) {
			chl_map->ch[ch_no].count -= read_wr.num_sge;
			chl_map->ch[ch_no].start += read_wr.num_sge;
			goto next_sge;
		}
		sgl_offset = 0;
		err = 1;
	}

 out:
	svc_rdma_put_req_map(rpl_map);
	svc_rdma_put_req_map(chl_map);

	
	for (ch_no = 0; &rqstp->rq_pages[ch_no] < rqstp->rq_respages; ch_no++)
		rqstp->rq_pages[ch_no] = NULL;

	while (rqstp->rq_resused)
		rqstp->rq_respages[--rqstp->rq_resused] = NULL;

	return err;
}

static int rdma_read_complete(struct svc_rqst *rqstp,
			      struct svc_rdma_op_ctxt *head)
{
	int page_no;
	int ret;

	BUG_ON(!head);

	
	for (page_no = 0; page_no < head->count; page_no++) {
		put_page(rqstp->rq_pages[page_no]);
		rqstp->rq_pages[page_no] = head->pages[page_no];
	}
	
	rqstp->rq_arg.pages = &rqstp->rq_pages[head->hdr_count];
	rqstp->rq_arg.page_len = head->arg.page_len;
	rqstp->rq_arg.page_base = head->arg.page_base;

	
	rqstp->rq_respages = &rqstp->rq_arg.pages[page_no];
	rqstp->rq_resused = 0;

	
	rqstp->rq_arg.head[0] = head->arg.head[0];
	rqstp->rq_arg.tail[0] = head->arg.tail[0];
	rqstp->rq_arg.len = head->arg.len;
	rqstp->rq_arg.buflen = head->arg.buflen;

	
	svc_rdma_put_context(head, 0);

	
	rqstp->rq_prot = IPPROTO_MAX;
	svc_xprt_copy_addrs(rqstp, rqstp->rq_xprt);

	ret = rqstp->rq_arg.head[0].iov_len
		+ rqstp->rq_arg.page_len
		+ rqstp->rq_arg.tail[0].iov_len;
	dprintk("svcrdma: deferred read ret=%d, rq_arg.len =%d, "
		"rq_arg.head[0].iov_base=%p, rq_arg.head[0].iov_len = %zd\n",
		ret, rqstp->rq_arg.len,	rqstp->rq_arg.head[0].iov_base,
		rqstp->rq_arg.head[0].iov_len);

	return ret;
}

int svc_rdma_recvfrom(struct svc_rqst *rqstp)
{
	struct svc_xprt *xprt = rqstp->rq_xprt;
	struct svcxprt_rdma *rdma_xprt =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	struct svc_rdma_op_ctxt *ctxt = NULL;
	struct rpcrdma_msg *rmsgp;
	int ret = 0;
	int len;

	dprintk("svcrdma: rqstp=%p\n", rqstp);

	spin_lock_bh(&rdma_xprt->sc_rq_dto_lock);
	if (!list_empty(&rdma_xprt->sc_read_complete_q)) {
		ctxt = list_entry(rdma_xprt->sc_read_complete_q.next,
				  struct svc_rdma_op_ctxt,
				  dto_q);
		list_del_init(&ctxt->dto_q);
	}
	if (ctxt) {
		spin_unlock_bh(&rdma_xprt->sc_rq_dto_lock);
		return rdma_read_complete(rqstp, ctxt);
	}

	if (!list_empty(&rdma_xprt->sc_rq_dto_q)) {
		ctxt = list_entry(rdma_xprt->sc_rq_dto_q.next,
				  struct svc_rdma_op_ctxt,
				  dto_q);
		list_del_init(&ctxt->dto_q);
	} else {
		atomic_inc(&rdma_stat_rq_starve);
		clear_bit(XPT_DATA, &xprt->xpt_flags);
		ctxt = NULL;
	}
	spin_unlock_bh(&rdma_xprt->sc_rq_dto_lock);
	if (!ctxt) {
		if (test_bit(XPT_CLOSE, &xprt->xpt_flags))
			goto close_out;

		BUG_ON(ret);
		goto out;
	}
	dprintk("svcrdma: processing ctxt=%p on xprt=%p, rqstp=%p, status=%d\n",
		ctxt, rdma_xprt, rqstp, ctxt->wc_status);
	BUG_ON(ctxt->wc_status != IB_WC_SUCCESS);
	atomic_inc(&rdma_stat_recv);

	
	rdma_build_arg_xdr(rqstp, ctxt, ctxt->byte_len);

	
	len = svc_rdma_xdr_decode_req(&rmsgp, rqstp);
	rqstp->rq_xprt_hlen = len;

	
	if (len < 0) {
		if (len == -ENOSYS)
			svc_rdma_send_error(rdma_xprt, rmsgp, ERR_VERS);
		goto close_out;
	}

	
	ret = rdma_read_xdr(rdma_xprt, rmsgp, rqstp, ctxt);
	if (ret > 0) {
		
		goto defer;
	}
	if (ret < 0) {
		
		svc_rdma_put_context(ctxt, 1);
		return 0;
	}

	ret = rqstp->rq_arg.head[0].iov_len
		+ rqstp->rq_arg.page_len
		+ rqstp->rq_arg.tail[0].iov_len;
	svc_rdma_put_context(ctxt, 0);
 out:
	dprintk("svcrdma: ret = %d, rq_arg.len =%d, "
		"rq_arg.head[0].iov_base=%p, rq_arg.head[0].iov_len = %zd\n",
		ret, rqstp->rq_arg.len,
		rqstp->rq_arg.head[0].iov_base,
		rqstp->rq_arg.head[0].iov_len);
	rqstp->rq_prot = IPPROTO_MAX;
	svc_xprt_copy_addrs(rqstp, xprt);
	return ret;

 close_out:
	if (ctxt)
		svc_rdma_put_context(ctxt, 1);
	dprintk("svcrdma: transport %p is closing\n", xprt);
	set_bit(XPT_CLOSE, &xprt->xpt_flags);
defer:
	return 0;
}
