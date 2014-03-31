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

#include <linux/io.h>

#include "ipath_verbs.h"
#include "ipath_kernel.h"

#define OP(x) IB_OPCODE_RC_##x

static u32 restart_sge(struct ipath_sge_state *ss, struct ipath_swqe *wqe,
		       u32 psn, u32 pmtu)
{
	u32 len;

	len = ((psn - wqe->psn) & IPATH_PSN_MASK) * pmtu;
	ss->sge = wqe->sg_list[0];
	ss->sg_list = wqe->sg_list + 1;
	ss->num_sge = wqe->wr.num_sge;
	ipath_skip_sge(ss, len);
	return wqe->length - len;
}

static void ipath_init_restart(struct ipath_qp *qp, struct ipath_swqe *wqe)
{
	struct ipath_ibdev *dev;

	qp->s_len = restart_sge(&qp->s_sge, wqe, qp->s_psn,
				ib_mtu_enum_to_int(qp->path_mtu));
	dev = to_idev(qp->ibqp.device);
	spin_lock(&dev->pending_lock);
	if (list_empty(&qp->timerwait))
		list_add_tail(&qp->timerwait,
			      &dev->pending[dev->pending_index]);
	spin_unlock(&dev->pending_lock);
}

static int ipath_make_rc_ack(struct ipath_ibdev *dev, struct ipath_qp *qp,
			     struct ipath_other_headers *ohdr, u32 pmtu)
{
	struct ipath_ack_entry *e;
	u32 hwords;
	u32 len;
	u32 bth0;
	u32 bth2;

	
	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK))
		goto bail;

	
	hwords = 5;

	switch (qp->s_ack_state) {
	case OP(RDMA_READ_RESPONSE_LAST):
	case OP(RDMA_READ_RESPONSE_ONLY):
	case OP(ATOMIC_ACKNOWLEDGE):
		if (++qp->s_tail_ack_queue > IPATH_MAX_RDMA_ATOMIC)
			qp->s_tail_ack_queue = 0;
		
	case OP(SEND_ONLY):
	case OP(ACKNOWLEDGE):
		
		if (qp->r_head_ack_queue == qp->s_tail_ack_queue) {
			if (qp->s_flags & IPATH_S_ACK_PENDING)
				goto normal;
			qp->s_ack_state = OP(ACKNOWLEDGE);
			goto bail;
		}

		e = &qp->s_ack_queue[qp->s_tail_ack_queue];
		if (e->opcode == OP(RDMA_READ_REQUEST)) {
			
			qp->s_ack_rdma_sge = e->rdma_sge;
			qp->s_cur_sge = &qp->s_ack_rdma_sge;
			len = e->rdma_sge.sge.sge_length;
			if (len > pmtu) {
				len = pmtu;
				qp->s_ack_state = OP(RDMA_READ_RESPONSE_FIRST);
			} else {
				qp->s_ack_state = OP(RDMA_READ_RESPONSE_ONLY);
				e->sent = 1;
			}
			ohdr->u.aeth = ipath_compute_aeth(qp);
			hwords++;
			qp->s_ack_rdma_psn = e->psn;
			bth2 = qp->s_ack_rdma_psn++ & IPATH_PSN_MASK;
		} else {
			
			qp->s_cur_sge = NULL;
			len = 0;
			qp->s_ack_state = OP(ATOMIC_ACKNOWLEDGE);
			ohdr->u.at.aeth = ipath_compute_aeth(qp);
			ohdr->u.at.atomic_ack_eth[0] =
				cpu_to_be32(e->atomic_data >> 32);
			ohdr->u.at.atomic_ack_eth[1] =
				cpu_to_be32(e->atomic_data);
			hwords += sizeof(ohdr->u.at) / sizeof(u32);
			bth2 = e->psn;
			e->sent = 1;
		}
		bth0 = qp->s_ack_state << 24;
		break;

	case OP(RDMA_READ_RESPONSE_FIRST):
		qp->s_ack_state = OP(RDMA_READ_RESPONSE_MIDDLE);
		
	case OP(RDMA_READ_RESPONSE_MIDDLE):
		len = qp->s_ack_rdma_sge.sge.sge_length;
		if (len > pmtu)
			len = pmtu;
		else {
			ohdr->u.aeth = ipath_compute_aeth(qp);
			hwords++;
			qp->s_ack_state = OP(RDMA_READ_RESPONSE_LAST);
			qp->s_ack_queue[qp->s_tail_ack_queue].sent = 1;
		}
		bth0 = qp->s_ack_state << 24;
		bth2 = qp->s_ack_rdma_psn++ & IPATH_PSN_MASK;
		break;

	default:
	normal:
		qp->s_ack_state = OP(SEND_ONLY);
		qp->s_flags &= ~IPATH_S_ACK_PENDING;
		qp->s_cur_sge = NULL;
		if (qp->s_nak_state)
			ohdr->u.aeth =
				cpu_to_be32((qp->r_msn & IPATH_MSN_MASK) |
					    (qp->s_nak_state <<
					     IPATH_AETH_CREDIT_SHIFT));
		else
			ohdr->u.aeth = ipath_compute_aeth(qp);
		hwords++;
		len = 0;
		bth0 = OP(ACKNOWLEDGE) << 24;
		bth2 = qp->s_ack_psn & IPATH_PSN_MASK;
	}
	qp->s_hdrwords = hwords;
	qp->s_cur_size = len;
	ipath_make_ruc_header(dev, qp, ohdr, bth0, bth2);
	return 1;

bail:
	return 0;
}

int ipath_make_rc_req(struct ipath_qp *qp)
{
	struct ipath_ibdev *dev = to_idev(qp->ibqp.device);
	struct ipath_other_headers *ohdr;
	struct ipath_sge_state *ss;
	struct ipath_swqe *wqe;
	u32 hwords;
	u32 len;
	u32 bth0;
	u32 bth2;
	u32 pmtu = ib_mtu_enum_to_int(qp->path_mtu);
	char newreq;
	unsigned long flags;
	int ret = 0;

	ohdr = &qp->s_hdr.u.oth;
	if (qp->remote_ah_attr.ah_flags & IB_AH_GRH)
		ohdr = &qp->s_hdr.u.l.oth;

	spin_lock_irqsave(&qp->s_lock, flags);

	
	if ((qp->r_head_ack_queue != qp->s_tail_ack_queue ||
	     (qp->s_flags & IPATH_S_ACK_PENDING) ||
	     qp->s_ack_state != OP(ACKNOWLEDGE)) &&
	    ipath_make_rc_ack(dev, qp, ohdr, pmtu))
		goto done;

	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_SEND_OK)) {
		if (!(ib_ipath_state_ops[qp->state] & IPATH_FLUSH_SEND))
			goto bail;
		
		if (qp->s_last == qp->s_head)
			goto bail;
		
		if (atomic_read(&qp->s_dma_busy)) {
			qp->s_flags |= IPATH_S_WAIT_DMA;
			goto bail;
		}
		wqe = get_swqe_ptr(qp, qp->s_last);
		ipath_send_complete(qp, wqe, IB_WC_WR_FLUSH_ERR);
		goto done;
	}

	
	if (qp->s_rnr_timeout) {
		qp->s_flags |= IPATH_S_WAITING;
		goto bail;
	}

	
	hwords = 5;
	bth0 = 1 << 22; 

	
	wqe = get_swqe_ptr(qp, qp->s_cur);
	switch (qp->s_state) {
	default:
		if (!(ib_ipath_state_ops[qp->state] &
		    IPATH_PROCESS_NEXT_SEND_OK))
			goto bail;
		newreq = 0;
		if (qp->s_cur == qp->s_tail) {
			
			if (qp->s_tail == qp->s_head)
				goto bail;
			if ((wqe->wr.send_flags & IB_SEND_FENCE) &&
			    qp->s_num_rd_atomic) {
				qp->s_flags |= IPATH_S_FENCE_PENDING;
				goto bail;
			}
			wqe->psn = qp->s_next_psn;
			newreq = 1;
		}
		len = wqe->length;
		ss = &qp->s_sge;
		bth2 = 0;
		switch (wqe->wr.opcode) {
		case IB_WR_SEND:
		case IB_WR_SEND_WITH_IMM:
			
			if (qp->s_lsn != (u32) -1 &&
			    ipath_cmp24(wqe->ssn, qp->s_lsn + 1) > 0) {
				qp->s_flags |= IPATH_S_WAIT_SSN_CREDIT;
				goto bail;
			}
			wqe->lpsn = wqe->psn;
			if (len > pmtu) {
				wqe->lpsn += (len - 1) / pmtu;
				qp->s_state = OP(SEND_FIRST);
				len = pmtu;
				break;
			}
			if (wqe->wr.opcode == IB_WR_SEND)
				qp->s_state = OP(SEND_ONLY);
			else {
				qp->s_state = OP(SEND_ONLY_WITH_IMMEDIATE);
				
				ohdr->u.imm_data = wqe->wr.ex.imm_data;
				hwords += 1;
			}
			if (wqe->wr.send_flags & IB_SEND_SOLICITED)
				bth0 |= 1 << 23;
			bth2 = 1 << 31;	
			if (++qp->s_cur == qp->s_size)
				qp->s_cur = 0;
			break;

		case IB_WR_RDMA_WRITE:
			if (newreq && qp->s_lsn != (u32) -1)
				qp->s_lsn++;
			
		case IB_WR_RDMA_WRITE_WITH_IMM:
			
			if (qp->s_lsn != (u32) -1 &&
			    ipath_cmp24(wqe->ssn, qp->s_lsn + 1) > 0) {
				qp->s_flags |= IPATH_S_WAIT_SSN_CREDIT;
				goto bail;
			}
			ohdr->u.rc.reth.vaddr =
				cpu_to_be64(wqe->wr.wr.rdma.remote_addr);
			ohdr->u.rc.reth.rkey =
				cpu_to_be32(wqe->wr.wr.rdma.rkey);
			ohdr->u.rc.reth.length = cpu_to_be32(len);
			hwords += sizeof(struct ib_reth) / sizeof(u32);
			wqe->lpsn = wqe->psn;
			if (len > pmtu) {
				wqe->lpsn += (len - 1) / pmtu;
				qp->s_state = OP(RDMA_WRITE_FIRST);
				len = pmtu;
				break;
			}
			if (wqe->wr.opcode == IB_WR_RDMA_WRITE)
				qp->s_state = OP(RDMA_WRITE_ONLY);
			else {
				qp->s_state =
					OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE);
				
				ohdr->u.rc.imm_data = wqe->wr.ex.imm_data;
				hwords += 1;
				if (wqe->wr.send_flags & IB_SEND_SOLICITED)
					bth0 |= 1 << 23;
			}
			bth2 = 1 << 31;	
			if (++qp->s_cur == qp->s_size)
				qp->s_cur = 0;
			break;

		case IB_WR_RDMA_READ:
			if (newreq) {
				if (qp->s_num_rd_atomic >=
				    qp->s_max_rd_atomic) {
					qp->s_flags |= IPATH_S_RDMAR_PENDING;
					goto bail;
				}
				qp->s_num_rd_atomic++;
				if (qp->s_lsn != (u32) -1)
					qp->s_lsn++;
				if (len > pmtu)
					qp->s_next_psn += (len - 1) / pmtu;
				wqe->lpsn = qp->s_next_psn++;
			}
			ohdr->u.rc.reth.vaddr =
				cpu_to_be64(wqe->wr.wr.rdma.remote_addr);
			ohdr->u.rc.reth.rkey =
				cpu_to_be32(wqe->wr.wr.rdma.rkey);
			ohdr->u.rc.reth.length = cpu_to_be32(len);
			qp->s_state = OP(RDMA_READ_REQUEST);
			hwords += sizeof(ohdr->u.rc.reth) / sizeof(u32);
			ss = NULL;
			len = 0;
			if (++qp->s_cur == qp->s_size)
				qp->s_cur = 0;
			break;

		case IB_WR_ATOMIC_CMP_AND_SWP:
		case IB_WR_ATOMIC_FETCH_AND_ADD:
			if (newreq) {
				if (qp->s_num_rd_atomic >=
				    qp->s_max_rd_atomic) {
					qp->s_flags |= IPATH_S_RDMAR_PENDING;
					goto bail;
				}
				qp->s_num_rd_atomic++;
				if (qp->s_lsn != (u32) -1)
					qp->s_lsn++;
				wqe->lpsn = wqe->psn;
			}
			if (wqe->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP) {
				qp->s_state = OP(COMPARE_SWAP);
				ohdr->u.atomic_eth.swap_data = cpu_to_be64(
					wqe->wr.wr.atomic.swap);
				ohdr->u.atomic_eth.compare_data = cpu_to_be64(
					wqe->wr.wr.atomic.compare_add);
			} else {
				qp->s_state = OP(FETCH_ADD);
				ohdr->u.atomic_eth.swap_data = cpu_to_be64(
					wqe->wr.wr.atomic.compare_add);
				ohdr->u.atomic_eth.compare_data = 0;
			}
			ohdr->u.atomic_eth.vaddr[0] = cpu_to_be32(
				wqe->wr.wr.atomic.remote_addr >> 32);
			ohdr->u.atomic_eth.vaddr[1] = cpu_to_be32(
				wqe->wr.wr.atomic.remote_addr);
			ohdr->u.atomic_eth.rkey = cpu_to_be32(
				wqe->wr.wr.atomic.rkey);
			hwords += sizeof(struct ib_atomic_eth) / sizeof(u32);
			ss = NULL;
			len = 0;
			if (++qp->s_cur == qp->s_size)
				qp->s_cur = 0;
			break;

		default:
			goto bail;
		}
		qp->s_sge.sge = wqe->sg_list[0];
		qp->s_sge.sg_list = wqe->sg_list + 1;
		qp->s_sge.num_sge = wqe->wr.num_sge;
		qp->s_len = wqe->length;
		if (newreq) {
			qp->s_tail++;
			if (qp->s_tail >= qp->s_size)
				qp->s_tail = 0;
		}
		bth2 |= qp->s_psn & IPATH_PSN_MASK;
		if (wqe->wr.opcode == IB_WR_RDMA_READ)
			qp->s_psn = wqe->lpsn + 1;
		else {
			qp->s_psn++;
			if (ipath_cmp24(qp->s_psn, qp->s_next_psn) > 0)
				qp->s_next_psn = qp->s_psn;
		}
		spin_lock(&dev->pending_lock);
		if (list_empty(&qp->timerwait))
			list_add_tail(&qp->timerwait,
				      &dev->pending[dev->pending_index]);
		spin_unlock(&dev->pending_lock);
		break;

	case OP(RDMA_READ_RESPONSE_FIRST):
		ipath_init_restart(qp, wqe);
		
	case OP(SEND_FIRST):
		qp->s_state = OP(SEND_MIDDLE);
		
	case OP(SEND_MIDDLE):
		bth2 = qp->s_psn++ & IPATH_PSN_MASK;
		if (ipath_cmp24(qp->s_psn, qp->s_next_psn) > 0)
			qp->s_next_psn = qp->s_psn;
		ss = &qp->s_sge;
		len = qp->s_len;
		if (len > pmtu) {
			len = pmtu;
			break;
		}
		if (wqe->wr.opcode == IB_WR_SEND)
			qp->s_state = OP(SEND_LAST);
		else {
			qp->s_state = OP(SEND_LAST_WITH_IMMEDIATE);
			
			ohdr->u.imm_data = wqe->wr.ex.imm_data;
			hwords += 1;
		}
		if (wqe->wr.send_flags & IB_SEND_SOLICITED)
			bth0 |= 1 << 23;
		bth2 |= 1 << 31;	
		qp->s_cur++;
		if (qp->s_cur >= qp->s_size)
			qp->s_cur = 0;
		break;

	case OP(RDMA_READ_RESPONSE_LAST):
		ipath_init_restart(qp, wqe);
		
	case OP(RDMA_WRITE_FIRST):
		qp->s_state = OP(RDMA_WRITE_MIDDLE);
		
	case OP(RDMA_WRITE_MIDDLE):
		bth2 = qp->s_psn++ & IPATH_PSN_MASK;
		if (ipath_cmp24(qp->s_psn, qp->s_next_psn) > 0)
			qp->s_next_psn = qp->s_psn;
		ss = &qp->s_sge;
		len = qp->s_len;
		if (len > pmtu) {
			len = pmtu;
			break;
		}
		if (wqe->wr.opcode == IB_WR_RDMA_WRITE)
			qp->s_state = OP(RDMA_WRITE_LAST);
		else {
			qp->s_state = OP(RDMA_WRITE_LAST_WITH_IMMEDIATE);
			
			ohdr->u.imm_data = wqe->wr.ex.imm_data;
			hwords += 1;
			if (wqe->wr.send_flags & IB_SEND_SOLICITED)
				bth0 |= 1 << 23;
		}
		bth2 |= 1 << 31;	
		qp->s_cur++;
		if (qp->s_cur >= qp->s_size)
			qp->s_cur = 0;
		break;

	case OP(RDMA_READ_RESPONSE_MIDDLE):
		ipath_init_restart(qp, wqe);
		len = ((qp->s_psn - wqe->psn) & IPATH_PSN_MASK) * pmtu;
		ohdr->u.rc.reth.vaddr =
			cpu_to_be64(wqe->wr.wr.rdma.remote_addr + len);
		ohdr->u.rc.reth.rkey =
			cpu_to_be32(wqe->wr.wr.rdma.rkey);
		ohdr->u.rc.reth.length = cpu_to_be32(qp->s_len);
		qp->s_state = OP(RDMA_READ_REQUEST);
		hwords += sizeof(ohdr->u.rc.reth) / sizeof(u32);
		bth2 = qp->s_psn & IPATH_PSN_MASK;
		qp->s_psn = wqe->lpsn + 1;
		ss = NULL;
		len = 0;
		qp->s_cur++;
		if (qp->s_cur == qp->s_size)
			qp->s_cur = 0;
		break;
	}
	if (ipath_cmp24(qp->s_psn, qp->s_last_psn + IPATH_PSN_CREDIT - 1) >= 0)
		bth2 |= 1 << 31;	
	qp->s_len -= len;
	qp->s_hdrwords = hwords;
	qp->s_cur_sge = ss;
	qp->s_cur_size = len;
	ipath_make_ruc_header(dev, qp, ohdr, bth0 | (qp->s_state << 24), bth2);
done:
	ret = 1;
	goto unlock;

bail:
	qp->s_flags &= ~IPATH_S_BUSY;
unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
	return ret;
}

static void send_rc_ack(struct ipath_qp *qp)
{
	struct ipath_ibdev *dev = to_idev(qp->ibqp.device);
	struct ipath_devdata *dd;
	u16 lrh0;
	u32 bth0;
	u32 hwords;
	u32 __iomem *piobuf;
	struct ipath_ib_header hdr;
	struct ipath_other_headers *ohdr;
	unsigned long flags;

	spin_lock_irqsave(&qp->s_lock, flags);

	
	if (qp->r_head_ack_queue != qp->s_tail_ack_queue ||
	    (qp->s_flags & IPATH_S_ACK_PENDING) ||
	    qp->s_ack_state != OP(ACKNOWLEDGE))
		goto queue_ack;

	spin_unlock_irqrestore(&qp->s_lock, flags);

	
	dd = dev->dd;
	if (!(dd->ipath_flags & IPATH_LINKACTIVE))
		goto done;

	piobuf = ipath_getpiobuf(dd, 0, NULL);
	if (!piobuf) {
		spin_lock_irqsave(&qp->s_lock, flags);
		goto queue_ack;
	}

	
	ohdr = &hdr.u.oth;
	lrh0 = IPATH_LRH_BTH;
	
	hwords = 6;
	if (unlikely(qp->remote_ah_attr.ah_flags & IB_AH_GRH)) {
		hwords += ipath_make_grh(dev, &hdr.u.l.grh,
					 &qp->remote_ah_attr.grh,
					 hwords, 0);
		ohdr = &hdr.u.l.oth;
		lrh0 = IPATH_LRH_GRH;
	}
	
	bth0 = ipath_get_pkey(dd, qp->s_pkey_index) |
		(OP(ACKNOWLEDGE) << 24) | (1 << 22);
	if (qp->r_nak_state)
		ohdr->u.aeth = cpu_to_be32((qp->r_msn & IPATH_MSN_MASK) |
					    (qp->r_nak_state <<
					     IPATH_AETH_CREDIT_SHIFT));
	else
		ohdr->u.aeth = ipath_compute_aeth(qp);
	lrh0 |= qp->remote_ah_attr.sl << 4;
	hdr.lrh[0] = cpu_to_be16(lrh0);
	hdr.lrh[1] = cpu_to_be16(qp->remote_ah_attr.dlid);
	hdr.lrh[2] = cpu_to_be16(hwords + SIZE_OF_CRC);
	hdr.lrh[3] = cpu_to_be16(dd->ipath_lid |
				 qp->remote_ah_attr.src_path_bits);
	ohdr->bth[0] = cpu_to_be32(bth0);
	ohdr->bth[1] = cpu_to_be32(qp->remote_qpn);
	ohdr->bth[2] = cpu_to_be32(qp->r_ack_psn & IPATH_PSN_MASK);

	writeq(hwords + 1, piobuf);

	if (dd->ipath_flags & IPATH_PIO_FLUSH_WC) {
		u32 *hdrp = (u32 *) &hdr;

		ipath_flush_wc();
		__iowrite32_copy(piobuf + 2, hdrp, hwords - 1);
		ipath_flush_wc();
		__raw_writel(hdrp[hwords - 1], piobuf + hwords + 1);
	} else
		__iowrite32_copy(piobuf + 2, (u32 *) &hdr, hwords);

	ipath_flush_wc();

	dev->n_unicast_xmit++;
	goto done;

queue_ack:
	if (ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK) {
		dev->n_rc_qacks++;
		qp->s_flags |= IPATH_S_ACK_PENDING;
		qp->s_nak_state = qp->r_nak_state;
		qp->s_ack_psn = qp->r_ack_psn;

		
		ipath_schedule_send(qp);
	}
	spin_unlock_irqrestore(&qp->s_lock, flags);
done:
	return;
}

static void reset_psn(struct ipath_qp *qp, u32 psn)
{
	u32 n = qp->s_last;
	struct ipath_swqe *wqe = get_swqe_ptr(qp, n);
	u32 opcode;

	qp->s_cur = n;

	if (ipath_cmp24(psn, wqe->psn) <= 0) {
		qp->s_state = OP(SEND_LAST);
		goto done;
	}

	
	opcode = wqe->wr.opcode;
	for (;;) {
		int diff;

		if (++n == qp->s_size)
			n = 0;
		if (n == qp->s_tail)
			break;
		wqe = get_swqe_ptr(qp, n);
		diff = ipath_cmp24(psn, wqe->psn);
		if (diff < 0)
			break;
		qp->s_cur = n;
		if (diff == 0) {
			qp->s_state = OP(SEND_LAST);
			goto done;
		}
		opcode = wqe->wr.opcode;
	}

	switch (opcode) {
	case IB_WR_SEND:
	case IB_WR_SEND_WITH_IMM:
		qp->s_state = OP(RDMA_READ_RESPONSE_FIRST);
		break;

	case IB_WR_RDMA_WRITE:
	case IB_WR_RDMA_WRITE_WITH_IMM:
		qp->s_state = OP(RDMA_READ_RESPONSE_LAST);
		break;

	case IB_WR_RDMA_READ:
		qp->s_state = OP(RDMA_READ_RESPONSE_MIDDLE);
		break;

	default:
		qp->s_state = OP(SEND_LAST);
	}
done:
	qp->s_psn = psn;
}

void ipath_restart_rc(struct ipath_qp *qp, u32 psn)
{
	struct ipath_swqe *wqe = get_swqe_ptr(qp, qp->s_last);
	struct ipath_ibdev *dev;

	if (qp->s_retry == 0) {
		ipath_send_complete(qp, wqe, IB_WC_RETRY_EXC_ERR);
		ipath_error_qp(qp, IB_WC_WR_FLUSH_ERR);
		goto bail;
	}
	qp->s_retry--;

	dev = to_idev(qp->ibqp.device);
	spin_lock(&dev->pending_lock);
	if (!list_empty(&qp->timerwait))
		list_del_init(&qp->timerwait);
	if (!list_empty(&qp->piowait))
		list_del_init(&qp->piowait);
	spin_unlock(&dev->pending_lock);

	if (wqe->wr.opcode == IB_WR_RDMA_READ)
		dev->n_rc_resends++;
	else
		dev->n_rc_resends += (qp->s_psn - psn) & IPATH_PSN_MASK;

	reset_psn(qp, psn);
	ipath_schedule_send(qp);

bail:
	return;
}

static inline void update_last_psn(struct ipath_qp *qp, u32 psn)
{
	qp->s_last_psn = psn;
}

static int do_rc_ack(struct ipath_qp *qp, u32 aeth, u32 psn, int opcode,
		     u64 val)
{
	struct ipath_ibdev *dev = to_idev(qp->ibqp.device);
	struct ib_wc wc;
	enum ib_wc_status status;
	struct ipath_swqe *wqe;
	int ret = 0;
	u32 ack_psn;
	int diff;

	spin_lock(&dev->pending_lock);
	if (!list_empty(&qp->timerwait))
		list_del_init(&qp->timerwait);
	spin_unlock(&dev->pending_lock);

	ack_psn = psn;
	if (aeth >> 29)
		ack_psn--;
	wqe = get_swqe_ptr(qp, qp->s_last);

	while ((diff = ipath_cmp24(ack_psn, wqe->lpsn)) >= 0) {
		if (wqe->wr.opcode == IB_WR_RDMA_READ &&
		    opcode == OP(RDMA_READ_RESPONSE_ONLY) &&
		    diff == 0) {
			ret = 1;
			goto bail;
		}
		if ((wqe->wr.opcode == IB_WR_RDMA_READ &&
		     (opcode != OP(RDMA_READ_RESPONSE_LAST) || diff != 0)) ||
		    ((wqe->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP ||
		      wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD) &&
		     (opcode != OP(ATOMIC_ACKNOWLEDGE) || diff != 0))) {
			update_last_psn(qp, wqe->psn - 1);
			
			ipath_restart_rc(qp, wqe->psn);
			goto bail;
		}
		if (wqe->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP ||
		    wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD)
			*(u64 *) wqe->sg_list[0].vaddr = val;
		if (qp->s_num_rd_atomic &&
		    (wqe->wr.opcode == IB_WR_RDMA_READ ||
		     wqe->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP ||
		     wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD)) {
			qp->s_num_rd_atomic--;
			
			if (((qp->s_flags & IPATH_S_FENCE_PENDING) &&
			     !qp->s_num_rd_atomic) ||
			    qp->s_flags & IPATH_S_RDMAR_PENDING)
				ipath_schedule_send(qp);
		}
		
		if (!(qp->s_flags & IPATH_S_SIGNAL_REQ_WR) ||
		    (wqe->wr.send_flags & IB_SEND_SIGNALED)) {
			memset(&wc, 0, sizeof wc);
			wc.wr_id = wqe->wr.wr_id;
			wc.status = IB_WC_SUCCESS;
			wc.opcode = ib_ipath_wc_opcode[wqe->wr.opcode];
			wc.byte_len = wqe->length;
			wc.qp = &qp->ibqp;
			wc.src_qp = qp->remote_qpn;
			wc.slid = qp->remote_ah_attr.dlid;
			wc.sl = qp->remote_ah_attr.sl;
			ipath_cq_enter(to_icq(qp->ibqp.send_cq), &wc, 0);
		}
		qp->s_retry = qp->s_retry_cnt;
		if (qp->s_last == qp->s_cur) {
			if (++qp->s_cur >= qp->s_size)
				qp->s_cur = 0;
			qp->s_last = qp->s_cur;
			if (qp->s_last == qp->s_tail)
				break;
			wqe = get_swqe_ptr(qp, qp->s_cur);
			qp->s_state = OP(SEND_LAST);
			qp->s_psn = wqe->psn;
		} else {
			if (++qp->s_last >= qp->s_size)
				qp->s_last = 0;
			if (qp->state == IB_QPS_SQD && qp->s_last == qp->s_cur)
				qp->s_draining = 0;
			if (qp->s_last == qp->s_tail)
				break;
			wqe = get_swqe_ptr(qp, qp->s_last);
		}
	}

	switch (aeth >> 29) {
	case 0:		
		dev->n_rc_acks++;
		
		if (qp->s_last != qp->s_tail) {
			spin_lock(&dev->pending_lock);
			if (list_empty(&qp->timerwait))
				list_add_tail(&qp->timerwait,
					&dev->pending[dev->pending_index]);
			spin_unlock(&dev->pending_lock);
			if (ipath_cmp24(qp->s_psn, psn) <= 0) {
				reset_psn(qp, psn + 1);
				ipath_schedule_send(qp);
			}
		} else if (ipath_cmp24(qp->s_psn, psn) <= 0) {
			qp->s_state = OP(SEND_LAST);
			qp->s_psn = psn + 1;
		}
		ipath_get_credit(qp, aeth);
		qp->s_rnr_retry = qp->s_rnr_retry_cnt;
		qp->s_retry = qp->s_retry_cnt;
		update_last_psn(qp, psn);
		ret = 1;
		goto bail;

	case 1:		
		dev->n_rnr_naks++;
		if (qp->s_last == qp->s_tail)
			goto bail;
		if (qp->s_rnr_retry == 0) {
			status = IB_WC_RNR_RETRY_EXC_ERR;
			goto class_b;
		}
		if (qp->s_rnr_retry_cnt < 7)
			qp->s_rnr_retry--;

		
		update_last_psn(qp, psn - 1);

		if (wqe->wr.opcode == IB_WR_RDMA_READ)
			dev->n_rc_resends++;
		else
			dev->n_rc_resends +=
				(qp->s_psn - psn) & IPATH_PSN_MASK;

		reset_psn(qp, psn);

		qp->s_rnr_timeout =
			ib_ipath_rnr_table[(aeth >> IPATH_AETH_CREDIT_SHIFT) &
					   IPATH_AETH_CREDIT_MASK];
		ipath_insert_rnr_queue(qp);
		ipath_schedule_send(qp);
		goto bail;

	case 3:		
		if (qp->s_last == qp->s_tail)
			goto bail;
		
		update_last_psn(qp, psn - 1);
		switch ((aeth >> IPATH_AETH_CREDIT_SHIFT) &
			IPATH_AETH_CREDIT_MASK) {
		case 0:	
			dev->n_seq_naks++;
			ipath_restart_rc(qp, psn);
			break;

		case 1:	
			status = IB_WC_REM_INV_REQ_ERR;
			dev->n_other_naks++;
			goto class_b;

		case 2:	
			status = IB_WC_REM_ACCESS_ERR;
			dev->n_other_naks++;
			goto class_b;

		case 3:	
			status = IB_WC_REM_OP_ERR;
			dev->n_other_naks++;
		class_b:
			ipath_send_complete(qp, wqe, status);
			ipath_error_qp(qp, IB_WC_WR_FLUSH_ERR);
			break;

		default:
			
			goto reserved;
		}
		qp->s_rnr_retry = qp->s_rnr_retry_cnt;
		goto bail;

	default:		
	reserved:
		
		goto bail;
	}

bail:
	return ret;
}

static inline void ipath_rc_rcv_resp(struct ipath_ibdev *dev,
				     struct ipath_other_headers *ohdr,
				     void *data, u32 tlen,
				     struct ipath_qp *qp,
				     u32 opcode,
				     u32 psn, u32 hdrsize, u32 pmtu,
				     int header_in_data)
{
	struct ipath_swqe *wqe;
	enum ib_wc_status status;
	unsigned long flags;
	int diff;
	u32 pad;
	u32 aeth;
	u64 val;

	spin_lock_irqsave(&qp->s_lock, flags);

	
	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK))
		goto ack_done;

	
	if (ipath_cmp24(psn, qp->s_next_psn) >= 0)
		goto ack_done;

	
	diff = ipath_cmp24(psn, qp->s_last_psn);
	if (unlikely(diff <= 0)) {
		
		if (diff == 0 && opcode == OP(ACKNOWLEDGE)) {
			if (!header_in_data)
				aeth = be32_to_cpu(ohdr->u.aeth);
			else {
				aeth = be32_to_cpu(((__be32 *) data)[0]);
				data += sizeof(__be32);
			}
			if ((aeth >> 29) == 0)
				ipath_get_credit(qp, aeth);
		}
		goto ack_done;
	}

	if (unlikely(qp->s_last == qp->s_tail))
		goto ack_done;
	wqe = get_swqe_ptr(qp, qp->s_last);
	status = IB_WC_SUCCESS;

	switch (opcode) {
	case OP(ACKNOWLEDGE):
	case OP(ATOMIC_ACKNOWLEDGE):
	case OP(RDMA_READ_RESPONSE_FIRST):
		if (!header_in_data)
			aeth = be32_to_cpu(ohdr->u.aeth);
		else {
			aeth = be32_to_cpu(((__be32 *) data)[0]);
			data += sizeof(__be32);
		}
		if (opcode == OP(ATOMIC_ACKNOWLEDGE)) {
			if (!header_in_data) {
				__be32 *p = ohdr->u.at.atomic_ack_eth;

				val = ((u64) be32_to_cpu(p[0]) << 32) |
					be32_to_cpu(p[1]);
			} else
				val = be64_to_cpu(((__be64 *) data)[0]);
		} else
			val = 0;
		if (!do_rc_ack(qp, aeth, psn, opcode, val) ||
		    opcode != OP(RDMA_READ_RESPONSE_FIRST))
			goto ack_done;
		hdrsize += 4;
		wqe = get_swqe_ptr(qp, qp->s_last);
		if (unlikely(wqe->wr.opcode != IB_WR_RDMA_READ))
			goto ack_op_err;
		qp->r_flags &= ~IPATH_R_RDMAR_SEQ;
		qp->s_rdma_read_len = restart_sge(&qp->s_rdma_read_sge,
						  wqe, psn, pmtu);
		goto read_middle;

	case OP(RDMA_READ_RESPONSE_MIDDLE):
		
		if (unlikely(ipath_cmp24(psn, qp->s_last_psn + 1))) {
			dev->n_rdma_seq++;
			if (qp->r_flags & IPATH_R_RDMAR_SEQ)
				goto ack_done;
			qp->r_flags |= IPATH_R_RDMAR_SEQ;
			ipath_restart_rc(qp, qp->s_last_psn + 1);
			goto ack_done;
		}
		if (unlikely(wqe->wr.opcode != IB_WR_RDMA_READ))
			goto ack_op_err;
	read_middle:
		if (unlikely(tlen != (hdrsize + pmtu + 4)))
			goto ack_len_err;
		if (unlikely(pmtu >= qp->s_rdma_read_len))
			goto ack_len_err;

		
		spin_lock(&dev->pending_lock);
		if (qp->s_rnr_timeout == 0 && !list_empty(&qp->timerwait))
			list_move_tail(&qp->timerwait,
				       &dev->pending[dev->pending_index]);
		spin_unlock(&dev->pending_lock);

		if (opcode == OP(RDMA_READ_RESPONSE_MIDDLE))
			qp->s_retry = qp->s_retry_cnt;

		qp->s_rdma_read_len -= pmtu;
		update_last_psn(qp, psn);
		spin_unlock_irqrestore(&qp->s_lock, flags);
		ipath_copy_sge(&qp->s_rdma_read_sge, data, pmtu);
		goto bail;

	case OP(RDMA_READ_RESPONSE_ONLY):
		if (!header_in_data)
			aeth = be32_to_cpu(ohdr->u.aeth);
		else
			aeth = be32_to_cpu(((__be32 *) data)[0]);
		if (!do_rc_ack(qp, aeth, psn, opcode, 0))
			goto ack_done;
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		if (unlikely(tlen < (hdrsize + pad + 8)))
			goto ack_len_err;
		wqe = get_swqe_ptr(qp, qp->s_last);
		qp->s_rdma_read_len = restart_sge(&qp->s_rdma_read_sge,
						  wqe, psn, pmtu);
		goto read_last;

	case OP(RDMA_READ_RESPONSE_LAST):
		
		if (unlikely(ipath_cmp24(psn, qp->s_last_psn + 1))) {
			dev->n_rdma_seq++;
			if (qp->r_flags & IPATH_R_RDMAR_SEQ)
				goto ack_done;
			qp->r_flags |= IPATH_R_RDMAR_SEQ;
			ipath_restart_rc(qp, qp->s_last_psn + 1);
			goto ack_done;
		}
		if (unlikely(wqe->wr.opcode != IB_WR_RDMA_READ))
			goto ack_op_err;
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		if (unlikely(tlen <= (hdrsize + pad + 8)))
			goto ack_len_err;
	read_last:
		tlen -= hdrsize + pad + 8;
		if (unlikely(tlen != qp->s_rdma_read_len))
			goto ack_len_err;
		if (!header_in_data)
			aeth = be32_to_cpu(ohdr->u.aeth);
		else {
			aeth = be32_to_cpu(((__be32 *) data)[0]);
			data += sizeof(__be32);
		}
		ipath_copy_sge(&qp->s_rdma_read_sge, data, tlen);
		(void) do_rc_ack(qp, aeth, psn,
				 OP(RDMA_READ_RESPONSE_LAST), 0);
		goto ack_done;
	}

ack_op_err:
	status = IB_WC_LOC_QP_OP_ERR;
	goto ack_err;

ack_len_err:
	status = IB_WC_LOC_LEN_ERR;
ack_err:
	ipath_send_complete(qp, wqe, status);
	ipath_error_qp(qp, IB_WC_WR_FLUSH_ERR);
ack_done:
	spin_unlock_irqrestore(&qp->s_lock, flags);
bail:
	return;
}

static inline int ipath_rc_rcv_error(struct ipath_ibdev *dev,
				     struct ipath_other_headers *ohdr,
				     void *data,
				     struct ipath_qp *qp,
				     u32 opcode,
				     u32 psn,
				     int diff,
				     int header_in_data)
{
	struct ipath_ack_entry *e;
	u8 i, prev;
	int old_req;
	unsigned long flags;

	if (diff > 0) {
		if (!qp->r_nak_state) {
			qp->r_nak_state = IB_NAK_PSN_ERROR;
			
			qp->r_ack_psn = qp->r_psn;
			goto send_ack;
		}
		goto done;
	}

	psn &= IPATH_PSN_MASK;
	e = NULL;
	old_req = 1;

	spin_lock_irqsave(&qp->s_lock, flags);
	
	if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK))
		goto unlock_done;

	for (i = qp->r_head_ack_queue; ; i = prev) {
		if (i == qp->s_tail_ack_queue)
			old_req = 0;
		if (i)
			prev = i - 1;
		else
			prev = IPATH_MAX_RDMA_ATOMIC;
		if (prev == qp->r_head_ack_queue) {
			e = NULL;
			break;
		}
		e = &qp->s_ack_queue[prev];
		if (!e->opcode) {
			e = NULL;
			break;
		}
		if (ipath_cmp24(psn, e->psn) >= 0) {
			if (prev == qp->s_tail_ack_queue)
				old_req = 0;
			break;
		}
	}
	switch (opcode) {
	case OP(RDMA_READ_REQUEST): {
		struct ib_reth *reth;
		u32 offset;
		u32 len;

		if (!e || e->opcode != OP(RDMA_READ_REQUEST) || old_req)
			goto unlock_done;
		
		if (!header_in_data)
			reth = &ohdr->u.rc.reth;
		else {
			reth = (struct ib_reth *)data;
			data += sizeof(*reth);
		}
		offset = ((psn - e->psn) & IPATH_PSN_MASK) *
			ib_mtu_enum_to_int(qp->path_mtu);
		len = be32_to_cpu(reth->length);
		if (unlikely(offset + len > e->rdma_sge.sge.sge_length))
			goto unlock_done;
		if (len != 0) {
			u32 rkey = be32_to_cpu(reth->rkey);
			u64 vaddr = be64_to_cpu(reth->vaddr);
			int ok;

			ok = ipath_rkey_ok(qp, &e->rdma_sge,
					   len, vaddr, rkey,
					   IB_ACCESS_REMOTE_READ);
			if (unlikely(!ok))
				goto unlock_done;
		} else {
			e->rdma_sge.sg_list = NULL;
			e->rdma_sge.num_sge = 0;
			e->rdma_sge.sge.mr = NULL;
			e->rdma_sge.sge.vaddr = NULL;
			e->rdma_sge.sge.length = 0;
			e->rdma_sge.sge.sge_length = 0;
		}
		e->psn = psn;
		qp->s_ack_state = OP(ACKNOWLEDGE);
		qp->s_tail_ack_queue = prev;
		break;
	}

	case OP(COMPARE_SWAP):
	case OP(FETCH_ADD): {
		if (!e || e->opcode != (u8) opcode || old_req)
			goto unlock_done;
		qp->s_ack_state = OP(ACKNOWLEDGE);
		qp->s_tail_ack_queue = prev;
		break;
	}

	default:
		if (old_req)
			goto unlock_done;
		if (i == qp->r_head_ack_queue) {
			spin_unlock_irqrestore(&qp->s_lock, flags);
			qp->r_nak_state = 0;
			qp->r_ack_psn = qp->r_psn - 1;
			goto send_ack;
		}
		if (qp->r_head_ack_queue == qp->s_tail_ack_queue &&
		    !(qp->s_flags & IPATH_S_ACK_PENDING) &&
		    qp->s_ack_state == OP(ACKNOWLEDGE)) {
			spin_unlock_irqrestore(&qp->s_lock, flags);
			qp->r_nak_state = 0;
			qp->r_ack_psn = qp->s_ack_queue[i].psn - 1;
			goto send_ack;
		}
		qp->s_ack_state = OP(ACKNOWLEDGE);
		qp->s_tail_ack_queue = i;
		break;
	}
	qp->r_nak_state = 0;
	ipath_schedule_send(qp);

unlock_done:
	spin_unlock_irqrestore(&qp->s_lock, flags);
done:
	return 1;

send_ack:
	return 0;
}

void ipath_rc_error(struct ipath_qp *qp, enum ib_wc_status err)
{
	unsigned long flags;
	int lastwqe;

	spin_lock_irqsave(&qp->s_lock, flags);
	lastwqe = ipath_error_qp(qp, err);
	spin_unlock_irqrestore(&qp->s_lock, flags);

	if (lastwqe) {
		struct ib_event ev;

		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
}

static inline void ipath_update_ack_queue(struct ipath_qp *qp, unsigned n)
{
	unsigned next;

	next = n + 1;
	if (next > IPATH_MAX_RDMA_ATOMIC)
		next = 0;
	if (n == qp->s_tail_ack_queue) {
		qp->s_tail_ack_queue = next;
		qp->s_ack_state = OP(ACKNOWLEDGE);
	}
}

void ipath_rc_rcv(struct ipath_ibdev *dev, struct ipath_ib_header *hdr,
		  int has_grh, void *data, u32 tlen, struct ipath_qp *qp)
{
	struct ipath_other_headers *ohdr;
	u32 opcode;
	u32 hdrsize;
	u32 psn;
	u32 pad;
	struct ib_wc wc;
	u32 pmtu = ib_mtu_enum_to_int(qp->path_mtu);
	int diff;
	struct ib_reth *reth;
	int header_in_data;
	unsigned long flags;

	
	if (unlikely(be16_to_cpu(hdr->lrh[3]) != qp->remote_ah_attr.dlid))
		goto done;

	
	if (!has_grh) {
		ohdr = &hdr->u.oth;
		hdrsize = 8 + 12;	
		psn = be32_to_cpu(ohdr->bth[2]);
		header_in_data = 0;
	} else {
		ohdr = &hdr->u.l.oth;
		hdrsize = 8 + 40 + 12;	
		header_in_data = dev->dd->ipath_rcvhdrentsize == 16;
		if (header_in_data) {
			psn = be32_to_cpu(((__be32 *) data)[0]);
			data += sizeof(__be32);
		} else
			psn = be32_to_cpu(ohdr->bth[2]);
	}

	opcode = be32_to_cpu(ohdr->bth[0]) >> 24;
	if (opcode >= OP(RDMA_READ_RESPONSE_FIRST) &&
	    opcode <= OP(ATOMIC_ACKNOWLEDGE)) {
		ipath_rc_rcv_resp(dev, ohdr, data, tlen, qp, opcode, psn,
				  hdrsize, pmtu, header_in_data);
		goto done;
	}

	
	diff = ipath_cmp24(psn, qp->r_psn);
	if (unlikely(diff)) {
		if (ipath_rc_rcv_error(dev, ohdr, data, qp, opcode,
				       psn, diff, header_in_data))
			goto done;
		goto send_ack;
	}

	
	switch (qp->r_state) {
	case OP(SEND_FIRST):
	case OP(SEND_MIDDLE):
		if (opcode == OP(SEND_MIDDLE) ||
		    opcode == OP(SEND_LAST) ||
		    opcode == OP(SEND_LAST_WITH_IMMEDIATE))
			break;
		goto nack_inv;

	case OP(RDMA_WRITE_FIRST):
	case OP(RDMA_WRITE_MIDDLE):
		if (opcode == OP(RDMA_WRITE_MIDDLE) ||
		    opcode == OP(RDMA_WRITE_LAST) ||
		    opcode == OP(RDMA_WRITE_LAST_WITH_IMMEDIATE))
			break;
		goto nack_inv;

	default:
		if (opcode == OP(SEND_MIDDLE) ||
		    opcode == OP(SEND_LAST) ||
		    opcode == OP(SEND_LAST_WITH_IMMEDIATE) ||
		    opcode == OP(RDMA_WRITE_MIDDLE) ||
		    opcode == OP(RDMA_WRITE_LAST) ||
		    opcode == OP(RDMA_WRITE_LAST_WITH_IMMEDIATE))
			goto nack_inv;
		break;
	}

	memset(&wc, 0, sizeof wc);

	
	switch (opcode) {
	case OP(SEND_FIRST):
		if (!ipath_get_rwqe(qp, 0))
			goto rnr_nak;
		qp->r_rcv_len = 0;
		
	case OP(SEND_MIDDLE):
	case OP(RDMA_WRITE_MIDDLE):
	send_middle:
		
		if (unlikely(tlen != (hdrsize + pmtu + 4)))
			goto nack_inv;
		qp->r_rcv_len += pmtu;
		if (unlikely(qp->r_rcv_len > qp->r_len))
			goto nack_inv;
		ipath_copy_sge(&qp->r_sge, data, pmtu);
		break;

	case OP(RDMA_WRITE_LAST_WITH_IMMEDIATE):
		
		if (!ipath_get_rwqe(qp, 1))
			goto rnr_nak;
		goto send_last_imm;

	case OP(SEND_ONLY):
	case OP(SEND_ONLY_WITH_IMMEDIATE):
		if (!ipath_get_rwqe(qp, 0))
			goto rnr_nak;
		qp->r_rcv_len = 0;
		if (opcode == OP(SEND_ONLY))
			goto send_last;
		
	case OP(SEND_LAST_WITH_IMMEDIATE):
	send_last_imm:
		if (header_in_data) {
			wc.ex.imm_data = *(__be32 *) data;
			data += sizeof(__be32);
		} else {
			
			wc.ex.imm_data = ohdr->u.imm_data;
		}
		hdrsize += 4;
		wc.wc_flags = IB_WC_WITH_IMM;
		
	case OP(SEND_LAST):
	case OP(RDMA_WRITE_LAST):
	send_last:
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4)))
			goto nack_inv;
		
		tlen -= (hdrsize + pad + 4);
		wc.byte_len = tlen + qp->r_rcv_len;
		if (unlikely(wc.byte_len > qp->r_len))
			goto nack_inv;
		ipath_copy_sge(&qp->r_sge, data, tlen);
		qp->r_msn++;
		if (!test_and_clear_bit(IPATH_R_WRID_VALID, &qp->r_aflags))
			break;
		wc.wr_id = qp->r_wr_id;
		wc.status = IB_WC_SUCCESS;
		if (opcode == OP(RDMA_WRITE_LAST_WITH_IMMEDIATE) ||
		    opcode == OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE))
			wc.opcode = IB_WC_RECV_RDMA_WITH_IMM;
		else
			wc.opcode = IB_WC_RECV;
		wc.qp = &qp->ibqp;
		wc.src_qp = qp->remote_qpn;
		wc.slid = qp->remote_ah_attr.dlid;
		wc.sl = qp->remote_ah_attr.sl;
		
		ipath_cq_enter(to_icq(qp->ibqp.recv_cq), &wc,
			       (ohdr->bth[0] &
				cpu_to_be32(1 << 23)) != 0);
		break;

	case OP(RDMA_WRITE_FIRST):
	case OP(RDMA_WRITE_ONLY):
	case OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE):
		if (unlikely(!(qp->qp_access_flags &
			       IB_ACCESS_REMOTE_WRITE)))
			goto nack_inv;
		
		
		if (!header_in_data)
			reth = &ohdr->u.rc.reth;
		else {
			reth = (struct ib_reth *)data;
			data += sizeof(*reth);
		}
		hdrsize += sizeof(*reth);
		qp->r_len = be32_to_cpu(reth->length);
		qp->r_rcv_len = 0;
		if (qp->r_len != 0) {
			u32 rkey = be32_to_cpu(reth->rkey);
			u64 vaddr = be64_to_cpu(reth->vaddr);
			int ok;

			
			ok = ipath_rkey_ok(qp, &qp->r_sge,
					   qp->r_len, vaddr, rkey,
					   IB_ACCESS_REMOTE_WRITE);
			if (unlikely(!ok))
				goto nack_acc;
		} else {
			qp->r_sge.sg_list = NULL;
			qp->r_sge.sge.mr = NULL;
			qp->r_sge.sge.vaddr = NULL;
			qp->r_sge.sge.length = 0;
			qp->r_sge.sge.sge_length = 0;
		}
		if (opcode == OP(RDMA_WRITE_FIRST))
			goto send_middle;
		else if (opcode == OP(RDMA_WRITE_ONLY))
			goto send_last;
		if (!ipath_get_rwqe(qp, 1))
			goto rnr_nak;
		goto send_last_imm;

	case OP(RDMA_READ_REQUEST): {
		struct ipath_ack_entry *e;
		u32 len;
		u8 next;

		if (unlikely(!(qp->qp_access_flags &
			       IB_ACCESS_REMOTE_READ)))
			goto nack_inv;
		next = qp->r_head_ack_queue + 1;
		if (next > IPATH_MAX_RDMA_ATOMIC)
			next = 0;
		spin_lock_irqsave(&qp->s_lock, flags);
		
		if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK))
			goto unlock;
		if (unlikely(next == qp->s_tail_ack_queue)) {
			if (!qp->s_ack_queue[next].sent)
				goto nack_inv_unlck;
			ipath_update_ack_queue(qp, next);
		}
		e = &qp->s_ack_queue[qp->r_head_ack_queue];
		
		if (!header_in_data)
			reth = &ohdr->u.rc.reth;
		else {
			reth = (struct ib_reth *)data;
			data += sizeof(*reth);
		}
		len = be32_to_cpu(reth->length);
		if (len) {
			u32 rkey = be32_to_cpu(reth->rkey);
			u64 vaddr = be64_to_cpu(reth->vaddr);
			int ok;

			
			ok = ipath_rkey_ok(qp, &e->rdma_sge, len, vaddr,
					   rkey, IB_ACCESS_REMOTE_READ);
			if (unlikely(!ok))
				goto nack_acc_unlck;
			if (len > pmtu)
				qp->r_psn += (len - 1) / pmtu;
		} else {
			e->rdma_sge.sg_list = NULL;
			e->rdma_sge.num_sge = 0;
			e->rdma_sge.sge.mr = NULL;
			e->rdma_sge.sge.vaddr = NULL;
			e->rdma_sge.sge.length = 0;
			e->rdma_sge.sge.sge_length = 0;
		}
		e->opcode = opcode;
		e->sent = 0;
		e->psn = psn;
		qp->r_msn++;
		qp->r_psn++;
		qp->r_state = opcode;
		qp->r_nak_state = 0;
		qp->r_head_ack_queue = next;

		
		ipath_schedule_send(qp);

		goto unlock;
	}

	case OP(COMPARE_SWAP):
	case OP(FETCH_ADD): {
		struct ib_atomic_eth *ateth;
		struct ipath_ack_entry *e;
		u64 vaddr;
		atomic64_t *maddr;
		u64 sdata;
		u32 rkey;
		u8 next;

		if (unlikely(!(qp->qp_access_flags &
			       IB_ACCESS_REMOTE_ATOMIC)))
			goto nack_inv;
		next = qp->r_head_ack_queue + 1;
		if (next > IPATH_MAX_RDMA_ATOMIC)
			next = 0;
		spin_lock_irqsave(&qp->s_lock, flags);
		
		if (!(ib_ipath_state_ops[qp->state] & IPATH_PROCESS_RECV_OK))
			goto unlock;
		if (unlikely(next == qp->s_tail_ack_queue)) {
			if (!qp->s_ack_queue[next].sent)
				goto nack_inv_unlck;
			ipath_update_ack_queue(qp, next);
		}
		if (!header_in_data)
			ateth = &ohdr->u.atomic_eth;
		else
			ateth = (struct ib_atomic_eth *)data;
		vaddr = ((u64) be32_to_cpu(ateth->vaddr[0]) << 32) |
			be32_to_cpu(ateth->vaddr[1]);
		if (unlikely(vaddr & (sizeof(u64) - 1)))
			goto nack_inv_unlck;
		rkey = be32_to_cpu(ateth->rkey);
		
		if (unlikely(!ipath_rkey_ok(qp, &qp->r_sge,
					    sizeof(u64), vaddr, rkey,
					    IB_ACCESS_REMOTE_ATOMIC)))
			goto nack_acc_unlck;
		
		maddr = (atomic64_t *) qp->r_sge.sge.vaddr;
		sdata = be64_to_cpu(ateth->swap_data);
		e = &qp->s_ack_queue[qp->r_head_ack_queue];
		e->atomic_data = (opcode == OP(FETCH_ADD)) ?
			(u64) atomic64_add_return(sdata, maddr) - sdata :
			(u64) cmpxchg((u64 *) qp->r_sge.sge.vaddr,
				      be64_to_cpu(ateth->compare_data),
				      sdata);
		e->opcode = opcode;
		e->sent = 0;
		e->psn = psn & IPATH_PSN_MASK;
		qp->r_msn++;
		qp->r_psn++;
		qp->r_state = opcode;
		qp->r_nak_state = 0;
		qp->r_head_ack_queue = next;

		
		ipath_schedule_send(qp);

		goto unlock;
	}

	default:
		
		goto nack_inv;
	}
	qp->r_psn++;
	qp->r_state = opcode;
	qp->r_ack_psn = psn;
	qp->r_nak_state = 0;
	
	if (psn & (1 << 31))
		goto send_ack;
	goto done;

rnr_nak:
	qp->r_nak_state = IB_RNR_NAK | qp->r_min_rnr_timer;
	qp->r_ack_psn = qp->r_psn;
	goto send_ack;

nack_inv_unlck:
	spin_unlock_irqrestore(&qp->s_lock, flags);
nack_inv:
	ipath_rc_error(qp, IB_WC_LOC_QP_OP_ERR);
	qp->r_nak_state = IB_NAK_INVALID_REQUEST;
	qp->r_ack_psn = qp->r_psn;
	goto send_ack;

nack_acc_unlck:
	spin_unlock_irqrestore(&qp->s_lock, flags);
nack_acc:
	ipath_rc_error(qp, IB_WC_LOC_PROT_ERR);
	qp->r_nak_state = IB_NAK_REMOTE_ACCESS_ERROR;
	qp->r_ack_psn = qp->r_psn;
send_ack:
	send_rc_ack(qp);
	goto done;

unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
done:
	return;
}
