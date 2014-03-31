/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010 QLogic Corporation.
 * All rights reserved.
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

#include "qib.h"

#define OP(x) IB_OPCODE_UC_##x

int qib_make_uc_req(struct qib_qp *qp)
{
	struct qib_other_headers *ohdr;
	struct qib_swqe *wqe;
	unsigned long flags;
	u32 hwords;
	u32 bth0;
	u32 len;
	u32 pmtu = qp->pmtu;
	int ret = 0;

	spin_lock_irqsave(&qp->s_lock, flags);

	if (!(ib_qib_state_ops[qp->state] & QIB_PROCESS_SEND_OK)) {
		if (!(ib_qib_state_ops[qp->state] & QIB_FLUSH_SEND))
			goto bail;
		
		if (qp->s_last == qp->s_head)
			goto bail;
		
		if (atomic_read(&qp->s_dma_busy)) {
			qp->s_flags |= QIB_S_WAIT_DMA;
			goto bail;
		}
		wqe = get_swqe_ptr(qp, qp->s_last);
		qib_send_complete(qp, wqe, IB_WC_WR_FLUSH_ERR);
		goto done;
	}

	ohdr = &qp->s_hdr.u.oth;
	if (qp->remote_ah_attr.ah_flags & IB_AH_GRH)
		ohdr = &qp->s_hdr.u.l.oth;

	
	hwords = 5;
	bth0 = 0;

	
	wqe = get_swqe_ptr(qp, qp->s_cur);
	qp->s_wqe = NULL;
	switch (qp->s_state) {
	default:
		if (!(ib_qib_state_ops[qp->state] &
		    QIB_PROCESS_NEXT_SEND_OK))
			goto bail;
		
		if (qp->s_cur == qp->s_head)
			goto bail;
		wqe->psn = qp->s_next_psn;
		qp->s_psn = qp->s_next_psn;
		qp->s_sge.sge = wqe->sg_list[0];
		qp->s_sge.sg_list = wqe->sg_list + 1;
		qp->s_sge.num_sge = wqe->wr.num_sge;
		qp->s_sge.total_len = wqe->length;
		len = wqe->length;
		qp->s_len = len;
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
				bth0 |= IB_BTH_SOLICITED;
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
					bth0 |= IB_BTH_SOLICITED;
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
			bth0 |= IB_BTH_SOLICITED;
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
				bth0 |= IB_BTH_SOLICITED;
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
	qib_make_ruc_header(qp, ohdr, bth0 | (qp->s_state << 24),
			    qp->s_next_psn++ & QIB_PSN_MASK);
done:
	ret = 1;
	goto unlock;

bail:
	qp->s_flags &= ~QIB_S_BUSY;
unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
	return ret;
}

void qib_uc_rcv(struct qib_ibport *ibp, struct qib_ib_header *hdr,
		int has_grh, void *data, u32 tlen, struct qib_qp *qp)
{
	struct qib_other_headers *ohdr;
	u32 opcode;
	u32 hdrsize;
	u32 psn;
	u32 pad;
	struct ib_wc wc;
	u32 pmtu = qp->pmtu;
	struct ib_reth *reth;
	int ret;

	
	if (!has_grh) {
		ohdr = &hdr->u.oth;
		hdrsize = 8 + 12;       
	} else {
		ohdr = &hdr->u.l.oth;
		hdrsize = 8 + 40 + 12;  
	}

	opcode = be32_to_cpu(ohdr->bth[0]);
	if (qib_ruc_check_hdr(ibp, hdr, has_grh, qp, opcode))
		return;

	psn = be32_to_cpu(ohdr->bth[2]);
	opcode >>= 24;

	
	if (unlikely(qib_cmp24(psn, qp->r_psn) != 0)) {
		qp->r_psn = psn;
inv:
		if (qp->r_state == OP(SEND_FIRST) ||
		    qp->r_state == OP(SEND_MIDDLE)) {
			set_bit(QIB_R_REWIND_SGE, &qp->r_aflags);
			qp->r_sge.num_sge = 0;
		} else
			while (qp->r_sge.num_sge) {
				atomic_dec(&qp->r_sge.sge.mr->refcount);
				if (--qp->r_sge.num_sge)
					qp->r_sge.sge = *qp->r_sge.sg_list++;
			}
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
			goto drop;
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

	if (qp->state == IB_QPS_RTR && !(qp->r_flags & QIB_R_COMM_EST)) {
		qp->r_flags |= QIB_R_COMM_EST;
		if (qp->ibqp.event_handler) {
			struct ib_event ev;

			ev.device = qp->ibqp.device;
			ev.element.qp = &qp->ibqp;
			ev.event = IB_EVENT_COMM_EST;
			qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
		}
	}

	
	switch (opcode) {
	case OP(SEND_FIRST):
	case OP(SEND_ONLY):
	case OP(SEND_ONLY_WITH_IMMEDIATE):
send_first:
		if (test_and_clear_bit(QIB_R_REWIND_SGE, &qp->r_aflags))
			qp->r_sge = qp->s_rdma_read_sge;
		else {
			ret = qib_get_rwqe(qp, 0);
			if (ret < 0)
				goto op_err;
			if (!ret)
				goto drop;
			qp->s_rdma_read_sge = qp->r_sge;
		}
		qp->r_rcv_len = 0;
		if (opcode == OP(SEND_ONLY))
			goto no_immediate_data;
		else if (opcode == OP(SEND_ONLY_WITH_IMMEDIATE))
			goto send_last_imm;
		
	case OP(SEND_MIDDLE):
		
		if (unlikely(tlen != (hdrsize + pmtu + 4)))
			goto rewind;
		qp->r_rcv_len += pmtu;
		if (unlikely(qp->r_rcv_len > qp->r_len))
			goto rewind;
		qib_copy_sge(&qp->r_sge, data, pmtu, 0);
		break;

	case OP(SEND_LAST_WITH_IMMEDIATE):
send_last_imm:
		wc.ex.imm_data = ohdr->u.imm_data;
		hdrsize += 4;
		wc.wc_flags = IB_WC_WITH_IMM;
		goto send_last;
	case OP(SEND_LAST):
no_immediate_data:
		wc.ex.imm_data = 0;
		wc.wc_flags = 0;
send_last:
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4)))
			goto rewind;
		
		tlen -= (hdrsize + pad + 4);
		wc.byte_len = tlen + qp->r_rcv_len;
		if (unlikely(wc.byte_len > qp->r_len))
			goto rewind;
		wc.opcode = IB_WC_RECV;
last_imm:
		qib_copy_sge(&qp->r_sge, data, tlen, 0);
		while (qp->s_rdma_read_sge.num_sge) {
			atomic_dec(&qp->s_rdma_read_sge.sge.mr->refcount);
			if (--qp->s_rdma_read_sge.num_sge)
				qp->s_rdma_read_sge.sge =
					*qp->s_rdma_read_sge.sg_list++;
		}
		wc.wr_id = qp->r_wr_id;
		wc.status = IB_WC_SUCCESS;
		wc.qp = &qp->ibqp;
		wc.src_qp = qp->remote_qpn;
		wc.slid = qp->remote_ah_attr.dlid;
		wc.sl = qp->remote_ah_attr.sl;
		
		wc.vendor_err = 0;
		wc.pkey_index = 0;
		wc.dlid_path_bits = 0;
		wc.port_num = 0;
		
		qib_cq_enter(to_icq(qp->ibqp.recv_cq), &wc,
			     (ohdr->bth[0] &
				cpu_to_be32(IB_BTH_SOLICITED)) != 0);
		break;

	case OP(RDMA_WRITE_FIRST):
	case OP(RDMA_WRITE_ONLY):
	case OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE): 
rdma_first:
		if (unlikely(!(qp->qp_access_flags &
			       IB_ACCESS_REMOTE_WRITE))) {
			goto drop;
		}
		reth = &ohdr->u.rc.reth;
		hdrsize += sizeof(*reth);
		qp->r_len = be32_to_cpu(reth->length);
		qp->r_rcv_len = 0;
		qp->r_sge.sg_list = NULL;
		if (qp->r_len != 0) {
			u32 rkey = be32_to_cpu(reth->rkey);
			u64 vaddr = be64_to_cpu(reth->vaddr);
			int ok;

			
			ok = qib_rkey_ok(qp, &qp->r_sge.sge, qp->r_len,
					 vaddr, rkey, IB_ACCESS_REMOTE_WRITE);
			if (unlikely(!ok))
				goto drop;
			qp->r_sge.num_sge = 1;
		} else {
			qp->r_sge.num_sge = 0;
			qp->r_sge.sge.mr = NULL;
			qp->r_sge.sge.vaddr = NULL;
			qp->r_sge.sge.length = 0;
			qp->r_sge.sge.sge_length = 0;
		}
		if (opcode == OP(RDMA_WRITE_ONLY))
			goto rdma_last;
		else if (opcode == OP(RDMA_WRITE_ONLY_WITH_IMMEDIATE)) {
			wc.ex.imm_data = ohdr->u.rc.imm_data;
			goto rdma_last_imm;
		}
		
	case OP(RDMA_WRITE_MIDDLE):
		
		if (unlikely(tlen != (hdrsize + pmtu + 4)))
			goto drop;
		qp->r_rcv_len += pmtu;
		if (unlikely(qp->r_rcv_len > qp->r_len))
			goto drop;
		qib_copy_sge(&qp->r_sge, data, pmtu, 1);
		break;

	case OP(RDMA_WRITE_LAST_WITH_IMMEDIATE):
		wc.ex.imm_data = ohdr->u.imm_data;
rdma_last_imm:
		hdrsize += 4;
		wc.wc_flags = IB_WC_WITH_IMM;

		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4)))
			goto drop;
		
		tlen -= (hdrsize + pad + 4);
		if (unlikely(tlen + qp->r_rcv_len != qp->r_len))
			goto drop;
		if (test_and_clear_bit(QIB_R_REWIND_SGE, &qp->r_aflags))
			while (qp->s_rdma_read_sge.num_sge) {
				atomic_dec(&qp->s_rdma_read_sge.sge.mr->
					   refcount);
				if (--qp->s_rdma_read_sge.num_sge)
					qp->s_rdma_read_sge.sge =
						*qp->s_rdma_read_sge.sg_list++;
			}
		else {
			ret = qib_get_rwqe(qp, 1);
			if (ret < 0)
				goto op_err;
			if (!ret)
				goto drop;
		}
		wc.byte_len = qp->r_len;
		wc.opcode = IB_WC_RECV_RDMA_WITH_IMM;
		goto last_imm;

	case OP(RDMA_WRITE_LAST):
rdma_last:
		
		pad = (be32_to_cpu(ohdr->bth[0]) >> 20) & 3;
		
		
		if (unlikely(tlen < (hdrsize + pad + 4)))
			goto drop;
		
		tlen -= (hdrsize + pad + 4);
		if (unlikely(tlen + qp->r_rcv_len != qp->r_len))
			goto drop;
		qib_copy_sge(&qp->r_sge, data, tlen, 1);
		while (qp->r_sge.num_sge) {
			atomic_dec(&qp->r_sge.sge.mr->refcount);
			if (--qp->r_sge.num_sge)
				qp->r_sge.sge = *qp->r_sge.sg_list++;
		}
		break;

	default:
		
		goto drop;
	}
	qp->r_psn++;
	qp->r_state = opcode;
	return;

rewind:
	set_bit(QIB_R_REWIND_SGE, &qp->r_aflags);
	qp->r_sge.num_sge = 0;
drop:
	ibp->n_pkt_drops++;
	return;

op_err:
	qib_rc_error(qp, IB_WC_LOC_QP_OP_ERR);
	return;

}
