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

#include <linux/sched.h>
#include <linux/spinlock.h>

#include "ipath_verbs.h"
#include "ipath_kernel.h"

const u32 ib_ipath_rnr_table[32] = {
	656,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	1,			
	2,			
	2,			
	3,			
	4,			
	6,			
	8,			
	11,			
	16,			
	21,			
	31,			
	41,			
	62,			
	82,			
	123,			
	164,			
	246,			
	328,			
	492			
};

void ipath_insert_rnr_queue(struct ipath_qp *qp)
{
	struct ipath_ibdev *dev = to_idev(qp->ibqp.device);

	
	spin_lock(&dev->pending_lock);
	if (list_empty(&dev->rnrwait))
		list_add(&qp->timerwait, &dev->rnrwait);
	else {
		struct list_head *l = &dev->rnrwait;
		struct ipath_qp *nqp = list_entry(l->next, struct ipath_qp,
						  timerwait);

		while (qp->s_rnr_timeout >= nqp->s_rnr_timeout) {
			qp->s_rnr_timeout -= nqp->s_rnr_timeout;
			l = l->next;
			if (l->next == &dev->rnrwait) {
				nqp = NULL;
				break;
			}
			nqp = list_entry(l->next, struct ipath_qp,
					 timerwait);
		}
		if (nqp)
			nqp->s_rnr_timeout -= qp->s_rnr_timeout;
		list_add(&qp->timerwait, l);
	}
	spin_unlock(&dev->pending_lock);
}

int ipath_init_sge(struct ipath_qp *qp, struct ipath_rwqe *wqe,
		   u32 *lengthp, struct ipath_sge_state *ss)
{
	int i, j, ret;
	struct ib_wc wc;

	*lengthp = 0;
	for (i = j = 0; i < wqe->num_sge; i++) {
		if (wqe->sg_list[i].length == 0)
			continue;
		
		if (!ipath_lkey_ok(qp, j ? &ss->sg_list[j - 1] : &ss->sge,
				   &wqe->sg_list[i], IB_ACCESS_LOCAL_WRITE))
			goto bad_lkey;
		*lengthp += wqe->sg_list[i].length;
		j++;
	}
	ss->num_sge = j;
	ret = 1;
	goto bail;

bad_lkey:
	memset(&wc, 0, sizeof(wc));
	wc.wr_id = wqe->wr_id;
	wc.status = IB_WC_LOC_PROT_ERR;
	wc.opcode = IB_WC_RECV;
	wc.qp = &qp->ibqp;
	
	ipath_cq_enter(to_icq(qp->ibqp.recv_cq), &wc, 1);
	ret = 0;
bail:
	return ret;
}

int ipath_get_rwqe(struct ipath_qp *qp, int wr_id_only)
{
	unsigned long flags;
	struct ipath_rq *rq;
	struct ipath_rwq *wq;
	struct ipath_srq *srq;
	struct ipath_rwqe *wqe;
	void (*handler)(struct ib_event *, void *);
	u32 tail;
	int ret;

	if (qp->ibqp.srq) {
		srq = to_isrq(qp->ibqp.srq);
		handler = srq->ibsrq.event_handler;
		rq = &srq->rq;
	} else {
		srq = NULL;
		handler = NULL;
		rq = &qp->r_rq;
	}

	spin_lock_irqsave(&rq->lock, flags);
	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK)) {
		ret = 0;
		goto unlock;
	}

	wq = rq->wq;
	tail = wq->tail;
	
	if (tail >= rq->size)
		tail = 0;
	do {
		if (unlikely(tail == wq->head)) {
			ret = 0;
			goto unlock;
		}
		
		smp_rmb();
		wqe = get_rwqe_ptr(rq, tail);
		if (++tail >= rq->size)
			tail = 0;
		if (wr_id_only)
			break;
		qp->r_sge.sg_list = qp->r_sg_list;
	} while (!ipath_init_sge(qp, wqe, &qp->r_len, &qp->r_sge));
	qp->r_wr_id = wqe->wr_id;
	wq->tail = tail;

	ret = 1;
	set_bit(IPATH_R_WRID_VALID, &qp->r_aflags);
	if (handler) {
		u32 n;

		n = wq->head;
		if (n >= rq->size)
			n = 0;
		if (n < tail)
			n += rq->size - tail;
		else
			n -= tail;
		if (n < srq->limit) {
			struct ib_event ev;

			srq->limit = 0;
			spin_unlock_irqrestore(&rq->lock, flags);
			ev.device = qp->ibqp.device;
			ev.element.srq = qp->ibqp.srq;
			ev.event = IB_EVENT_SRQ_LIMIT_REACHED;
			handler(&ev, srq->ibsrq.srq_context);
			goto bail;
		}
	}
unlock:
	spin_unlock_irqrestore(&rq->lock, flags);
bail:
	return ret;
}

static void ipath_ruc_loopback(struct ipath_qp *sqp)
{
	struct ipath_ibdev *dev = to_idev(sqp->ibqp.device);
	struct ipath_qp *qp;
	struct ipath_swqe *wqe;
	struct ipath_sge *sge;
	unsigned long flags;
	struct ib_wc wc;
	u64 sdata;
	atomic64_t *maddr;
	enum ib_wc_status send_status;

	qp = ipath_lookup_qpn(&dev->qp_table, sqp->remote_qpn);

	spin_lock_irqsave(&sqp->s_lock, flags);

	
	if ((sqp->s_flags & (IPATH_S_BUSY | IPATH_S_ANY_WAIT)) ||
	    !(ib_ipath_state_ops[sqp->state] & IPATH_PROCESS_OR_FLUSH_SEND))
		goto unlock;

	sqp->s_flags |= IPATH_S_BUSY;

again:
	if (sqp->s_last == sqp->s_head)
		goto clr_busy;
	wqe = get_swqe_ptr(sqp, sqp->s_last);

	
	if (!(ib_ipath_state_ops[sqp->state] & IPATH_PROCESS_NEXT_SEND_OK)) {
		if (!(ib_ipath_state_ops[sqp->state] & IPATH_FLUSH_SEND))
			goto clr_busy;
		
		send_status = IB_WC_WR_FLUSH_ERR;
		goto flush_send;
	}

	if (sqp->s_last == sqp->s_cur) {
		if (++sqp->s_cur >= sqp->s_size)
			sqp->s_cur = 0;
	}
	spin_unlock_irqrestore(&sqp->s_lock, flags);

	if (!qp || !(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK)) {
		dev->n_pkt_drops++;
		if (sqp->ibqp.qp_type == IB_QPT_RC)
			send_status = IB_WC_RETRY_EXC_ERR;
		else
			send_status = IB_WC_SUCCESS;
		goto serr;
	}

	memset(&wc, 0, sizeof wc);
	send_status = IB_WC_SUCCESS;

	sqp->s_sge.sge = wqe->sg_list[0];
	sqp->s_sge.sg_list = wqe->sg_list + 1;
	sqp->s_sge.num_sge = wqe->wr.num_sge;
	sqp->s_len = wqe->length;
	switch (wqe->wr.opcode) {
	case IB_WR_SEND_WITH_IMM:
		wc.wc_flags = IB_WC_WITH_IMM;
		wc.ex.imm_data = wqe->wr.ex.imm_data;
		
	case IB_WR_SEND:
		if (!ipath_get_rwqe(qp, 0))
			goto rnr_nak;
		break;

	case IB_WR_RDMA_WRITE_WITH_IMM:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_WRITE)))
			goto inv_err;
		wc.wc_flags = IB_WC_WITH_IMM;
		wc.ex.imm_data = wqe->wr.ex.imm_data;
		if (!ipath_get_rwqe(qp, 1))
			goto rnr_nak;
		
	case IB_WR_RDMA_WRITE:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_WRITE)))
			goto inv_err;
		if (wqe->length == 0)
			break;
		if (unlikely(!ipath_rkey_ok(qp, &qp->r_sge, wqe->length,
					    wqe->wr.wr.rdma.remote_addr,
					    wqe->wr.wr.rdma.rkey,
					    IB_ACCESS_REMOTE_WRITE)))
			goto acc_err;
		break;

	case IB_WR_RDMA_READ:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_READ)))
			goto inv_err;
		if (unlikely(!ipath_rkey_ok(qp, &sqp->s_sge, wqe->length,
					    wqe->wr.wr.rdma.remote_addr,
					    wqe->wr.wr.rdma.rkey,
					    IB_ACCESS_REMOTE_READ)))
			goto acc_err;
		qp->r_sge.sge = wqe->sg_list[0];
		qp->r_sge.sg_list = wqe->sg_list + 1;
		qp->r_sge.num_sge = wqe->wr.num_sge;
		break;

	case IB_WR_ATOMIC_CMP_AND_SWP:
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_ATOMIC)))
			goto inv_err;
		if (unlikely(!ipath_rkey_ok(qp, &qp->r_sge, sizeof(u64),
					    wqe->wr.wr.atomic.remote_addr,
					    wqe->wr.wr.atomic.rkey,
					    IB_ACCESS_REMOTE_ATOMIC)))
			goto acc_err;
		
		maddr = (atomic64_t *) qp->r_sge.sge.vaddr;
		sdata = wqe->wr.wr.atomic.compare_add;
		*(u64 *) sqp->s_sge.sge.vaddr =
			(wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD) ?
			(u64) atomic64_add_return(sdata, maddr) - sdata :
			(u64) cmpxchg((u64 *) qp->r_sge.sge.vaddr,
				      sdata, wqe->wr.wr.atomic.swap);
		goto send_comp;

	default:
		send_status = IB_WC_LOC_QP_OP_ERR;
		goto serr;
	}

	sge = &sqp->s_sge.sge;
	while (sqp->s_len) {
		u32 len = sqp->s_len;

		if (len > sge->length)
			len = sge->length;
		if (len > sge->sge_length)
			len = sge->sge_length;
		BUG_ON(len == 0);
		ipath_copy_sge(&qp->r_sge, sge->vaddr, len);
		sge->vaddr += len;
		sge->length -= len;
		sge->sge_length -= len;
		if (sge->sge_length == 0) {
			if (--sqp->s_sge.num_sge)
				*sge = *sqp->s_sge.sg_list++;
		} else if (sge->length == 0 && sge->mr != NULL) {
			if (++sge->n >= IPATH_SEGSZ) {
				if (++sge->m >= sge->mr->mapsz)
					break;
				sge->n = 0;
			}
			sge->vaddr =
				sge->mr->map[sge->m]->segs[sge->n].vaddr;
			sge->length =
				sge->mr->map[sge->m]->segs[sge->n].length;
		}
		sqp->s_len -= len;
	}

	if (!test_and_clear_bit(IPATH_R_WRID_VALID, &qp->r_aflags))
		goto send_comp;

	if (wqe->wr.opcode == IB_WR_RDMA_WRITE_WITH_IMM)
		wc.opcode = IB_WC_RECV_RDMA_WITH_IMM;
	else
		wc.opcode = IB_WC_RECV;
	wc.wr_id = qp->r_wr_id;
	wc.status = IB_WC_SUCCESS;
	wc.byte_len = wqe->length;
	wc.qp = &qp->ibqp;
	wc.src_qp = qp->remote_qpn;
	wc.slid = qp->remote_ah_attr.dlid;
	wc.sl = qp->remote_ah_attr.sl;
	wc.port_num = 1;
	
	ipath_cq_enter(to_icq(qp->ibqp.recv_cq), &wc,
		       wqe->wr.send_flags & IB_SEND_SOLICITED);

send_comp:
	spin_lock_irqsave(&sqp->s_lock, flags);
flush_send:
	sqp->s_rnr_retry = sqp->s_rnr_retry_cnt;
	ipath_send_complete(sqp, wqe, send_status);
	goto again;

rnr_nak:
	
	if (qp->ibqp.qp_type == IB_QPT_UC)
		goto send_comp;
	if (sqp->s_rnr_retry == 0) {
		send_status = IB_WC_RNR_RETRY_EXC_ERR;
		goto serr;
	}
	if (sqp->s_rnr_retry_cnt < 7)
		sqp->s_rnr_retry--;
	spin_lock_irqsave(&sqp->s_lock, flags);
	if (!(ib_ipath_state_ops[sqp->state] & IPATH_PROCESS_RECV_OK))
		goto clr_busy;
	sqp->s_flags |= IPATH_S_WAITING;
	dev->n_rnr_naks++;
	sqp->s_rnr_timeout = ib_ipath_rnr_table[qp->r_min_rnr_timer];
	ipath_insert_rnr_queue(sqp);
	goto clr_busy;

inv_err:
	send_status = IB_WC_REM_INV_REQ_ERR;
	wc.status = IB_WC_LOC_QP_OP_ERR;
	goto err;

acc_err:
	send_status = IB_WC_REM_ACCESS_ERR;
	wc.status = IB_WC_LOC_PROT_ERR;
err:
	
	ipath_rc_error(qp, wc.status);

serr:
	spin_lock_irqsave(&sqp->s_lock, flags);
	ipath_send_complete(sqp, wqe, send_status);
	if (sqp->ibqp.qp_type == IB_QPT_RC) {
		int lastwqe = ipath_error_qp(sqp, IB_WC_WR_FLUSH_ERR);

		sqp->s_flags &= ~IPATH_S_BUSY;
		spin_unlock_irqrestore(&sqp->s_lock, flags);
		if (lastwqe) {
			struct ib_event ev;

			ev.device = sqp->ibqp.device;
			ev.element.qp = &sqp->ibqp;
			ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
			sqp->ibqp.event_handler(&ev, sqp->ibqp.qp_context);
		}
		goto done;
	}
clr_busy:
	sqp->s_flags &= ~IPATH_S_BUSY;
unlock:
	spin_unlock_irqrestore(&sqp->s_lock, flags);
done:
	if (qp && atomic_dec_and_test(&qp->refcount))
		wake_up(&qp->wait);
}

static void want_buffer(struct ipath_devdata *dd, struct ipath_qp *qp)
{
	if (!(dd->ipath_flags & IPATH_HAS_SEND_DMA) ||
	    qp->ibqp.qp_type == IB_QPT_SMI) {
		unsigned long flags;

		spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
		dd->ipath_sendctrl |= INFINIPATH_S_PIOINTBUFAVAIL;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
				 dd->ipath_sendctrl);
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
		spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
	}
}

static int ipath_no_bufs_available(struct ipath_qp *qp,
				    struct ipath_ibdev *dev)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&qp->s_lock, flags);
	if (ib_ipath_state_ops[qp->state] & IPATH_PROCESS_SEND_OK) {
		dev->n_piowait++;
		qp->s_flags |= IPATH_S_WAITING;
		qp->s_flags &= ~IPATH_S_BUSY;
		spin_lock(&dev->pending_lock);
		if (list_empty(&qp->piowait))
			list_add_tail(&qp->piowait, &dev->piowait);
		spin_unlock(&dev->pending_lock);
	} else
		ret = 0;
	spin_unlock_irqrestore(&qp->s_lock, flags);
	if (ret)
		want_buffer(dev->dd, qp);
	return ret;
}

u32 ipath_make_grh(struct ipath_ibdev *dev, struct ib_grh *hdr,
		   struct ib_global_route *grh, u32 hwords, u32 nwords)
{
	hdr->version_tclass_flow =
		cpu_to_be32((6 << 28) |
			    (grh->traffic_class << 20) |
			    grh->flow_label);
	hdr->paylen = cpu_to_be16((hwords - 2 + nwords + SIZE_OF_CRC) << 2);
	
	hdr->next_hdr = 0x1B;
	hdr->hop_limit = grh->hop_limit;
	
	hdr->sgid.global.subnet_prefix = dev->gid_prefix;
	hdr->sgid.global.interface_id = dev->dd->ipath_guid;
	hdr->dgid = grh->dgid;

	
	return sizeof(struct ib_grh) / sizeof(u32);
}

void ipath_make_ruc_header(struct ipath_ibdev *dev, struct ipath_qp *qp,
			   struct ipath_other_headers *ohdr,
			   u32 bth0, u32 bth2)
{
	u16 lrh0;
	u32 nwords;
	u32 extra_bytes;

	
	extra_bytes = -qp->s_cur_size & 3;
	nwords = (qp->s_cur_size + extra_bytes) >> 2;
	lrh0 = IPATH_LRH_BTH;
	if (unlikely(qp->remote_ah_attr.ah_flags & IB_AH_GRH)) {
		qp->s_hdrwords += ipath_make_grh(dev, &qp->s_hdr.u.l.grh,
						 &qp->remote_ah_attr.grh,
						 qp->s_hdrwords, nwords);
		lrh0 = IPATH_LRH_GRH;
	}
	lrh0 |= qp->remote_ah_attr.sl << 4;
	qp->s_hdr.lrh[0] = cpu_to_be16(lrh0);
	qp->s_hdr.lrh[1] = cpu_to_be16(qp->remote_ah_attr.dlid);
	qp->s_hdr.lrh[2] = cpu_to_be16(qp->s_hdrwords + nwords + SIZE_OF_CRC);
	qp->s_hdr.lrh[3] = cpu_to_be16(dev->dd->ipath_lid |
				       qp->remote_ah_attr.src_path_bits);
	bth0 |= ipath_get_pkey(dev->dd, qp->s_pkey_index);
	bth0 |= extra_bytes << 20;
	ohdr->bth[0] = cpu_to_be32(bth0 | (1 << 22));
	ohdr->bth[1] = cpu_to_be32(qp->remote_qpn);
	ohdr->bth[2] = cpu_to_be32(bth2);
}

void ipath_do_send(unsigned long data)
{
	struct ipath_qp *qp = (struct ipath_qp *)data;
	struct ipath_ibdev *dev = to_idev(qp->ibqp.device);
	int (*make_req)(struct ipath_qp *qp);
	unsigned long flags;

	if ((qp->ibqp.qp_type == IB_QPT_RC ||
	     qp->ibqp.qp_type == IB_QPT_UC) &&
	    qp->remote_ah_attr.dlid == dev->dd->ipath_lid) {
		ipath_ruc_loopback(qp);
		goto bail;
	}

	if (qp->ibqp.qp_type == IB_QPT_RC)
	       make_req = ipath_make_rc_req;
	else if (qp->ibqp.qp_type == IB_QPT_UC)
	       make_req = ipath_make_uc_req;
	else
	       make_req = ipath_make_ud_req;

	spin_lock_irqsave(&qp->s_lock, flags);

	
	if ((qp->s_flags & (IPATH_S_BUSY | IPATH_S_ANY_WAIT)) ||
	    !(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_OR_FLUSH_SEND)) {
		spin_unlock_irqrestore(&qp->s_lock, flags);
		goto bail;
	}

	qp->s_flags |= IPATH_S_BUSY;

	spin_unlock_irqrestore(&qp->s_lock, flags);

again:
	
	if (qp->s_hdrwords != 0) {
		if (ipath_verbs_send(qp, &qp->s_hdr, qp->s_hdrwords,
				     qp->s_cur_sge, qp->s_cur_size)) {
			if (ipath_no_bufs_available(qp, dev))
				goto bail;
		}
		dev->n_unicast_xmit++;
		
		qp->s_hdrwords = 0;
	}

	if (make_req(qp))
		goto again;

bail:;
}

void ipath_send_complete(struct ipath_qp *qp, struct ipath_swqe *wqe,
			 enum ib_wc_status status)
{
	u32 old_last, last;

	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_OR_FLUSH_SEND))
		return;

	
	if (!(qp->s_flags & IPATH_S_SIGNAL_REQ_WR) ||
	    (wqe->wr.send_flags & IB_SEND_SIGNALED) ||
	    status != IB_WC_SUCCESS) {
		struct ib_wc wc;

		memset(&wc, 0, sizeof wc);
		wc.wr_id = wqe->wr.wr_id;
		wc.status = status;
		wc.opcode = ib_ipath_wc_opcode[wqe->wr.opcode];
		wc.qp = &qp->ibqp;
		if (status == IB_WC_SUCCESS)
			wc.byte_len = wqe->length;
		ipath_cq_enter(to_icq(qp->ibqp.send_cq), &wc,
			       status != IB_WC_SUCCESS);
	}

	old_last = last = qp->s_last;
	if (++last >= qp->s_size)
		last = 0;
	qp->s_last = last;
	if (qp->s_cur == old_last)
		qp->s_cur = last;
	if (qp->s_tail == old_last)
		qp->s_tail = last;
	if (qp->state == IB_QPS_SQD && last == qp->s_cur)
		qp->s_draining = 0;
}
