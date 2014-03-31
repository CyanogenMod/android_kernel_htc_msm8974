/*
 * Copyright (c) 2006, 2007 QLogic Corporation. All rights reserved.
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "ipath_verbs.h"

void ipath_cq_enter(struct ipath_cq *cq, struct ib_wc *entry, int solicited)
{
	struct ipath_cq_wc *wc;
	unsigned long flags;
	u32 head;
	u32 next;

	spin_lock_irqsave(&cq->lock, flags);

	wc = cq->queue;
	head = wc->head;
	if (head >= (unsigned) cq->ibcq.cqe) {
		head = cq->ibcq.cqe;
		next = 0;
	} else
		next = head + 1;
	if (unlikely(next == wc->tail)) {
		spin_unlock_irqrestore(&cq->lock, flags);
		if (cq->ibcq.event_handler) {
			struct ib_event ev;

			ev.device = cq->ibcq.device;
			ev.element.cq = &cq->ibcq;
			ev.event = IB_EVENT_CQ_ERR;
			cq->ibcq.event_handler(&ev, cq->ibcq.cq_context);
		}
		return;
	}
	if (cq->ip) {
		wc->uqueue[head].wr_id = entry->wr_id;
		wc->uqueue[head].status = entry->status;
		wc->uqueue[head].opcode = entry->opcode;
		wc->uqueue[head].vendor_err = entry->vendor_err;
		wc->uqueue[head].byte_len = entry->byte_len;
		wc->uqueue[head].ex.imm_data = (__u32 __force) entry->ex.imm_data;
		wc->uqueue[head].qp_num = entry->qp->qp_num;
		wc->uqueue[head].src_qp = entry->src_qp;
		wc->uqueue[head].wc_flags = entry->wc_flags;
		wc->uqueue[head].pkey_index = entry->pkey_index;
		wc->uqueue[head].slid = entry->slid;
		wc->uqueue[head].sl = entry->sl;
		wc->uqueue[head].dlid_path_bits = entry->dlid_path_bits;
		wc->uqueue[head].port_num = entry->port_num;
		/* Make sure entry is written before the head index. */
		smp_wmb();
	} else
		wc->kqueue[head] = *entry;
	wc->head = next;

	if (cq->notify == IB_CQ_NEXT_COMP ||
	    (cq->notify == IB_CQ_SOLICITED && solicited)) {
		cq->notify = IB_CQ_NONE;
		cq->triggered++;
		tasklet_hi_schedule(&cq->comptask);
	}

	spin_unlock_irqrestore(&cq->lock, flags);

	if (entry->status != IB_WC_SUCCESS)
		to_idev(cq->ibcq.device)->n_wqe_errs++;
}

int ipath_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *entry)
{
	struct ipath_cq *cq = to_icq(ibcq);
	struct ipath_cq_wc *wc;
	unsigned long flags;
	int npolled;
	u32 tail;

	
	if (cq->ip) {
		npolled = -EINVAL;
		goto bail;
	}

	spin_lock_irqsave(&cq->lock, flags);

	wc = cq->queue;
	tail = wc->tail;
	if (tail > (u32) cq->ibcq.cqe)
		tail = (u32) cq->ibcq.cqe;
	for (npolled = 0; npolled < num_entries; ++npolled, ++entry) {
		if (tail == wc->head)
			break;
		
		*entry = wc->kqueue[tail];
		if (tail >= cq->ibcq.cqe)
			tail = 0;
		else
			tail++;
	}
	wc->tail = tail;

	spin_unlock_irqrestore(&cq->lock, flags);

bail:
	return npolled;
}

static void send_complete(unsigned long data)
{
	struct ipath_cq *cq = (struct ipath_cq *)data;

	for (;;) {
		u8 triggered = cq->triggered;

		cq->ibcq.comp_handler(&cq->ibcq, cq->ibcq.cq_context);

		if (cq->triggered == triggered)
			return;
	}
}

struct ib_cq *ipath_create_cq(struct ib_device *ibdev, int entries, int comp_vector,
			      struct ib_ucontext *context,
			      struct ib_udata *udata)
{
	struct ipath_ibdev *dev = to_idev(ibdev);
	struct ipath_cq *cq;
	struct ipath_cq_wc *wc;
	struct ib_cq *ret;
	u32 sz;

	if (entries < 1 || entries > ib_ipath_max_cqes) {
		ret = ERR_PTR(-EINVAL);
		goto done;
	}

	
	cq = kmalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq) {
		ret = ERR_PTR(-ENOMEM);
		goto done;
	}

	sz = sizeof(*wc);
	if (udata && udata->outlen >= sizeof(__u64))
		sz += sizeof(struct ib_uverbs_wc) * (entries + 1);
	else
		sz += sizeof(struct ib_wc) * (entries + 1);
	wc = vmalloc_user(sz);
	if (!wc) {
		ret = ERR_PTR(-ENOMEM);
		goto bail_cq;
	}

	if (udata && udata->outlen >= sizeof(__u64)) {
		int err;

		cq->ip = ipath_create_mmap_info(dev, sz, context, wc);
		if (!cq->ip) {
			ret = ERR_PTR(-ENOMEM);
			goto bail_wc;
		}

		err = ib_copy_to_udata(udata, &cq->ip->offset,
				       sizeof(cq->ip->offset));
		if (err) {
			ret = ERR_PTR(err);
			goto bail_ip;
		}
	} else
		cq->ip = NULL;

	spin_lock(&dev->n_cqs_lock);
	if (dev->n_cqs_allocated == ib_ipath_max_cqs) {
		spin_unlock(&dev->n_cqs_lock);
		ret = ERR_PTR(-ENOMEM);
		goto bail_ip;
	}

	dev->n_cqs_allocated++;
	spin_unlock(&dev->n_cqs_lock);

	if (cq->ip) {
		spin_lock_irq(&dev->pending_lock);
		list_add(&cq->ip->pending_mmaps, &dev->pending_mmaps);
		spin_unlock_irq(&dev->pending_lock);
	}

	cq->ibcq.cqe = entries;
	cq->notify = IB_CQ_NONE;
	cq->triggered = 0;
	spin_lock_init(&cq->lock);
	tasklet_init(&cq->comptask, send_complete, (unsigned long)cq);
	wc->head = 0;
	wc->tail = 0;
	cq->queue = wc;

	ret = &cq->ibcq;

	goto done;

bail_ip:
	kfree(cq->ip);
bail_wc:
	vfree(wc);
bail_cq:
	kfree(cq);
done:
	return ret;
}

int ipath_destroy_cq(struct ib_cq *ibcq)
{
	struct ipath_ibdev *dev = to_idev(ibcq->device);
	struct ipath_cq *cq = to_icq(ibcq);

	tasklet_kill(&cq->comptask);
	spin_lock(&dev->n_cqs_lock);
	dev->n_cqs_allocated--;
	spin_unlock(&dev->n_cqs_lock);
	if (cq->ip)
		kref_put(&cq->ip->ref, ipath_release_mmap_info);
	else
		vfree(cq->queue);
	kfree(cq);

	return 0;
}

int ipath_req_notify_cq(struct ib_cq *ibcq, enum ib_cq_notify_flags notify_flags)
{
	struct ipath_cq *cq = to_icq(ibcq);
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&cq->lock, flags);
	if (cq->notify != IB_CQ_NEXT_COMP)
		cq->notify = notify_flags & IB_CQ_SOLICITED_MASK;

	if ((notify_flags & IB_CQ_REPORT_MISSED_EVENTS) &&
	    cq->queue->head != cq->queue->tail)
		ret = 1;

	spin_unlock_irqrestore(&cq->lock, flags);

	return ret;
}

int ipath_resize_cq(struct ib_cq *ibcq, int cqe, struct ib_udata *udata)
{
	struct ipath_cq *cq = to_icq(ibcq);
	struct ipath_cq_wc *old_wc;
	struct ipath_cq_wc *wc;
	u32 head, tail, n;
	int ret;
	u32 sz;

	if (cqe < 1 || cqe > ib_ipath_max_cqes) {
		ret = -EINVAL;
		goto bail;
	}

	sz = sizeof(*wc);
	if (udata && udata->outlen >= sizeof(__u64))
		sz += sizeof(struct ib_uverbs_wc) * (cqe + 1);
	else
		sz += sizeof(struct ib_wc) * (cqe + 1);
	wc = vmalloc_user(sz);
	if (!wc) {
		ret = -ENOMEM;
		goto bail;
	}

	
	if (udata && udata->outlen >= sizeof(__u64)) {
		__u64 offset = 0;

		ret = ib_copy_to_udata(udata, &offset, sizeof(offset));
		if (ret)
			goto bail_free;
	}

	spin_lock_irq(&cq->lock);
	old_wc = cq->queue;
	head = old_wc->head;
	if (head > (u32) cq->ibcq.cqe)
		head = (u32) cq->ibcq.cqe;
	tail = old_wc->tail;
	if (tail > (u32) cq->ibcq.cqe)
		tail = (u32) cq->ibcq.cqe;
	if (head < tail)
		n = cq->ibcq.cqe + 1 + head - tail;
	else
		n = head - tail;
	if (unlikely((u32)cqe < n)) {
		ret = -EINVAL;
		goto bail_unlock;
	}
	for (n = 0; tail != head; n++) {
		if (cq->ip)
			wc->uqueue[n] = old_wc->uqueue[tail];
		else
			wc->kqueue[n] = old_wc->kqueue[tail];
		if (tail == (u32) cq->ibcq.cqe)
			tail = 0;
		else
			tail++;
	}
	cq->ibcq.cqe = cqe;
	wc->head = n;
	wc->tail = 0;
	cq->queue = wc;
	spin_unlock_irq(&cq->lock);

	vfree(old_wc);

	if (cq->ip) {
		struct ipath_ibdev *dev = to_idev(ibcq->device);
		struct ipath_mmap_info *ip = cq->ip;

		ipath_update_mmap_info(dev, ip, sz, wc);

		if (udata && udata->outlen >= sizeof(__u64)) {
			ret = ib_copy_to_udata(udata, &ip->offset,
					       sizeof(ip->offset));
			if (ret)
				goto bail;
		}

		spin_lock_irq(&dev->pending_lock);
		if (list_empty(&ip->pending_mmaps))
			list_add(&ip->pending_mmaps, &dev->pending_mmaps);
		spin_unlock_irq(&dev->pending_lock);
	}

	ret = 0;
	goto bail;

bail_unlock:
	spin_unlock_irq(&cq->lock);
bail_free:
	vfree(wc);
bail:
	return ret;
}
