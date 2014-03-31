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

#include "ipath_verbs.h"
#include "ipath_kernel.h"

#define OP(x) IB_OPCODE_UC_##x

int ipath_make_uc_req(struct ipath_qp *qp)
{
	struct ipath_other_headers *ohdr;
	struct ipath_swqe *wqe;
	unsigned long flags;
	u32 hwords;
	u32 bth0;
	u32 len;
	u32 pmtu = ib_mtu_enum_to_int(qp->path_mtu);
	int ret = 0;

	spin_lock_irqsave(&qp->s_lock, flags);

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

	ohdr = &qp->s_hdr.u.oth;
	if (qp->remote_ah_attr.ah_flags & IB_AH_GRH)
		ohdr = &qp->s_hdr.u.l.oth;

	
	hwords = 5;
	bth0 = 1 << 22; 

	
	wqe = get_swqe_ptr(qp, qp->s_cur);
	qp->s_wqe = NULL;
	switch (qp->s_state) {
	default:
		if (!(ib_ipath_state_ops[qp->state] &
		    IPATH_PROCESS_NEXT_SEND_OK))
			goto bail;
		
		if (qp->s_cur == qp->s_head)
			goto bail;
		qp->s_psn = wqe->psn = qp->s_next_psn;
		qp->s_sge.sge = wqe->sg_list[0];
		qp->s_sge.sg_list = wqe->sg_list + 1;
		qp->s_sge.num_sge = wqe->wr.num_sge;
		qp->s_len = len = wqe->length;
		switch (wqe->wr.opcode) {
		case IB_WR_SEND:
		case IB_WR_SEND_WITH_IMM:
			if (len > pmtu) {
				qp->s_state = OP(SEND_FIRST);
				len = pmtu;
				break;
			}
			if (wqe->wr.opcode == IB_WR_SEND)
				qp->s_state = OP(SEND_ONLY);
			else {
				qp->s_state =
					OP(SEND_ONLY_WITH_IMMEDIATE);
				
				ohdr->u.imm_data = wqe->wr.ex.imm_data;
				hwords += 1;
			}
			if (wqe->wr.send_flags & IB_SEND_SOLICITED)
				bth0 |= 1 << 23;
			qp->s_wqe = wqe;
			if (++qp->s_cur >= qp->s_size)
				qp->s_cur = 0;
			break;

		case IB_WR_RDMA_WRITE:
		case IB_WR_RDMA_WRITE_WITH_IMM:
			ohdr->u.rc.reth.vaddr =
				cpu_to_be64(wqe->wr.wr.rdma.remote_addr);
			ohdr->u.rc.reth.rkey =
				cpu_to_be32(wqe->wr.wr.rdma.rkey);
			ohdr->u.rc.reth.length = cpu_to_be32(len);
			hwords += sizeof(struct ib_reth) / 4;
			if (len > pmtu) {
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
			qp->s_wqe = wqe;
			if (++qp->s_cur >= qp->s_size)
				qp->s_cur = 0;
			break;

		default:
			goto bail;
		}
		break;

	case OP(SEND_FIRST):
		qp->s_state = OP(SEND_MIDDLE);
		
	case OP(SEND_MIDDLE):
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
		qp->s_wqe = wqe;
		if (++qp->s_cur >= qp->s_size)
			qp->s_cur = 0;
		break;

	case OP(RDMA_WRITE_FIRST):
		qp->s_state = OP(RDMA_WRITE_MIDDLE);
		
	case OP(RDMA_WRITE_MIDDLE):
		len = qp->s_len;
		if (len > pmtu) {
			len = pmtu;
			break;
		}
		if (wqe->wr.opcode == IB_WR_RDMA_WRITE)
			qp->s_state = OP(RDMA_WRITE_LAST);
		else {
			qp->s_state =
				OP(RDMA_WRITE_LAST_WITH_IMMEDIATE);
			
			ohdr->u.imm_data = wqe->wr.ex.imm_data;
			hwords += 1;
			if (wqe->wr.send_flags & IB_SEND_SOLICITED)
				bth0 |= 1 << 23;
		}
		qp->s_wqe = wqe;
		if (++qp->s_cur >= qp->s_size)
			qp->s_cur = 0;
		break;
	}
	qp->s_len -= len;
	qp->s_hdrwords = hwords;
	qp->s_cur_sge = &qp->s_sge;
	qp->s_cur_size = len;
	ipath_make_ruc_header(to_idev(qp->ibqp.device),
			      qp, ohdr, bth0 | (qp->s_state << 24),
			      qp->s_next_psn++ & IPATH_PSN_MASK);
done:
	ret = 1;
	goto unlock;

bail:
	qp->s_flags &= ~IPATH_S_BUSY;
unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
	return ret;
}

void ipath_uc_rcv(struct ipath_ibdev *dev, struct ipath_ib_header *hdr,
		  int has_grh, void *data, u32 tlen, struct ipath_qp *qp)
{
	struct ipath_other_headers *ohdr;
	int opcode;
	u32 hdrsize;
	u32 psn;
	u32 pad;
	struct ib_wc wc;
	u32 pmtu = ib_mtu_enum_to_int(qp->path_mtu);
	struct ib_reth *reth;
	int header_in_data;

	
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

	memset(&wc, 0, sizeof wc);

	
	if (unlikely(ipath_cmp24(psn, qp->r_psn) != 0)) {
		qp->r_psn = psn;
	inv:
		qp->r_state = OP(SEND_LAST);
		switch (opcode) {
		case OP(SEND_FIRST):
		case OP(SEND_ONLY):
		case OP(SEND_ONLY_WITH_IMMEDIATE):
			goto send_first;

		case OP(RDMA_WRITE_FIRST):
		case OP(RDMA_WRITE_ONLY):
		case OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE):
			goto rdma_first;

		default:
			dev->n_pkt_drops++;
			goto done;
		}
	}

	
	switch (qp->r_state) {
	case OP(SEND_FIRST):
	case OP(SEND_MIDDLE):
		if (opcode == OP(SEND_MIDDLE) ||
		    opcode == OP(SEND_LAST) ||
		    opcode == OP(SEND_LAST_WITH_IMMEDIATE))
			break;
		goto inv;

	case OP(RDMA_WRITE_FIRST):
	case OP(RDMA_WRITE_MIDDLE):
		if (opcode == OP(RDMA_WRITE_MIDDLE) ||
		    opcode == OP(RDMA_WRITE_LAST) ||
		    opcode == OP(RDMA_WRITE_LAST_WITH_IMMEDIATE))
			break;
		goto inv;

	default:
		if (opcode == OP(SEND_FIRST) ||
		    opcode == OP(SEND_ONLY) ||
		    opcode == OP(SEND_ONLY_WITH_IMMEDIATE) ||
		    opcode == OP(RDMA_WRITE_FIRST) ||
		    opcode == OP(RDMA_WRITE_ONLY) ||
		    opcode == OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE))
			break;
		goto inv;
	}

	
	switch (opcode) {
	case OP(SEND_FIRST):
	case OP(SEND_ONLY):
	case OP(SEND_ONLY_WITH_IMMEDIATE):
	send_first:
		if (qp->r_flags & IPATH_R_REUSE_SGE) {
			qp->r_flags &= ~IPATH_R_REUSE_SGE;
			qp->r_sge = qp->s_rdma_read_sge;
		} else if (!ipath_get_rwqe(qp, 0)) {
			dev->n_pkt_drops++;
			goto done;
		}
		
		qp->s_rdma_read_sge = qp->r_sge;
		qp->r_rcv_len = 0;
		if (opcode == OP(SEND_ONLY))
			goto send_last;
		else if (opcode == OP(SEND_ONLY_WITH_IMMEDIATE))
			goto send_last_imm;
		
	case OP(SEND_MIDDLE):
		
		if (unlikely(tlen != (hdrsize + pmtu + 4))) {
			qp->r_flags |= IPATH_R_REUSE_SGE;
			dev->n_pkt_drops++;
			goto done;
		}
		qp->r_rcv_len += pmtu;
		if (unlikely(qp->r_rcv_len > qp->r_len)) {
			qp->r_flags |= IPATH_R_REUSE_SGE;
			dev->n_pkt_drops++;
			goto done;
		}
		ipath_copy_sge(&qp->r_sge, data, pmtu);
		break;

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
	send_last:
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4))) {
			qp->r_flags |= IPATH_R_REUSE_SGE;
			dev->n_pkt_drops++;
			goto done;
		}
		
		tlen -= (hdrsize + pad + 4);
		wc.byte_len = tlen + qp->r_rcv_len;
		if (unlikely(wc.byte_len > qp->r_len)) {
			qp->r_flags |= IPATH_R_REUSE_SGE;
			dev->n_pkt_drops++;
			goto done;
		}
		wc.opcode = IB_WC_RECV;
	last_imm:
		ipath_copy_sge(&qp->r_sge, data, tlen);
		wc.wr_id = qp->r_wr_id;
		wc.status = IB_WC_SUCCESS;
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
	rdma_first:
		
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

			
			ok = ipath_rkey_ok(qp, &qp->r_sge, qp->r_len,
					   vaddr, rkey,
					   IB_ACCESS_REMOTE_WRITE);
			if (unlikely(!ok)) {
				dev->n_pkt_drops++;
				goto done;
			}
		} else {
			qp->r_sge.sg_list = NULL;
			qp->r_sge.sge.mr = NULL;
			qp->r_sge.sge.vaddr = NULL;
			qp->r_sge.sge.length = 0;
			qp->r_sge.sge.sge_length = 0;
		}
		if (unlikely(!(qp->qp_access_flags &
			       IB_ACCESS_REMOTE_WRITE))) {
			dev->n_pkt_drops++;
			goto done;
		}
		if (opcode == OP(RDMA_WRITE_ONLY))
			goto rdma_last;
		else if (opcode == OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE))
			goto rdma_last_imm;
		
	case OP(RDMA_WRITE_MIDDLE):
		
		if (unlikely(tlen != (hdrsize + pmtu + 4))) {
			dev->n_pkt_drops++;
			goto done;
		}
		qp->r_rcv_len += pmtu;
		if (unlikely(qp->r_rcv_len > qp->r_len)) {
			dev->n_pkt_drops++;
			goto done;
		}
		ipath_copy_sge(&qp->r_sge, data, pmtu);
		break;

	case OP(RDMA_WRITE_LAST_WITH_IMMEDIATE):
	rdma_last_imm:
		if (header_in_data) {
			wc.ex.imm_data = *(__be32 *) data;
			data += sizeof(__be32);
		} else {
			
			wc.ex.imm_data = ohdr->u.imm_data;
		}
		hdrsize += 4;
		wc.wc_flags = IB_WC_WITH_IMM;

		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4))) {
			dev->n_pkt_drops++;
			goto done;
		}
		
		tlen -= (hdrsize + pad + 4);
		if (unlikely(tlen + qp->r_rcv_len != qp->r_len)) {
			dev->n_pkt_drops++;
			goto done;
		}
		if (qp->r_flags & IPATH_R_REUSE_SGE)
			qp->r_flags &= ~IPATH_R_REUSE_SGE;
		else if (!ipath_get_rwqe(qp, 1)) {
			dev->n_pkt_drops++;
			goto done;
		}
		wc.byte_len = qp->r_len;
		wc.opcode = IB_WC_RECV_RDMA_WITH_IMM;
		goto last_imm;

	case OP(RDMA_WRITE_LAST):
	rdma_last:
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4))) {
			dev->n_pkt_drops++;
			goto done;
		}
		
		tlen -= (hdrsize + pad + 4);
		if (unlikely(tlen + qp->r_rcv_len != qp->r_len)) {
			dev->n_pkt_drops++;
			goto done;
		}
		ipath_copy_sge(&qp->r_sge, data, tlen);
		break;

	default:
		
		dev->n_pkt_drops++;
		goto done;
	}
	qp->r_psn++;
	qp->r_state = opcode;
done:
	return;
}
