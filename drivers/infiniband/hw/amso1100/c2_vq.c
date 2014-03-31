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
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "c2_vq.h"
#include "c2_provider.h"


int vq_init(struct c2_dev *c2dev)
{
	sprintf(c2dev->vq_cache_name, "c2-vq:dev%c",
		(char) ('0' + c2dev->devnum));
	c2dev->host_msg_cache =
	    kmem_cache_create(c2dev->vq_cache_name, c2dev->rep_vq.msg_size, 0,
			      SLAB_HWCACHE_ALIGN, NULL);
	if (c2dev->host_msg_cache == NULL) {
		return -ENOMEM;
	}
	return 0;
}

void vq_term(struct c2_dev *c2dev)
{
	kmem_cache_destroy(c2dev->host_msg_cache);
}

struct c2_vq_req *vq_req_alloc(struct c2_dev *c2dev)
{
	struct c2_vq_req *r;

	r = kmalloc(sizeof(struct c2_vq_req), GFP_KERNEL);
	if (r) {
		init_waitqueue_head(&r->wait_object);
		r->reply_msg = 0;
		r->event = 0;
		r->cm_id = NULL;
		r->qp = NULL;
		atomic_set(&r->refcnt, 1);
		atomic_set(&r->reply_ready, 0);
	}
	return r;
}


void vq_req_free(struct c2_dev *c2dev, struct c2_vq_req *r)
{
	r->reply_msg = 0;
	if (atomic_dec_and_test(&r->refcnt)) {
		kfree(r);
	}
}

void vq_req_get(struct c2_dev *c2dev, struct c2_vq_req *r)
{
	atomic_inc(&r->refcnt);
}


void vq_req_put(struct c2_dev *c2dev, struct c2_vq_req *r)
{
	if (atomic_dec_and_test(&r->refcnt)) {
		if (r->reply_msg != 0)
			vq_repbuf_free(c2dev,
				       (void *) (unsigned long) r->reply_msg);
		kfree(r);
	}
}


void *vq_repbuf_alloc(struct c2_dev *c2dev)
{
	return kmem_cache_alloc(c2dev->host_msg_cache, GFP_ATOMIC);
}

int vq_send_wr(struct c2_dev *c2dev, union c2wr *wr)
{
	void *msg;
	wait_queue_t __wait;

	spin_lock(&c2dev->vqlock);

	msg = c2_mq_alloc(&c2dev->req_vq);

	while (msg == NULL) {
		pr_debug("%s:%d no available msg in VQ, waiting...\n",
		       __func__, __LINE__);
		init_waitqueue_entry(&__wait, current);
		add_wait_queue(&c2dev->req_vq_wo, &__wait);
		spin_unlock(&c2dev->vqlock);
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (!c2_mq_full(&c2dev->req_vq)) {
				break;
			}
			if (!signal_pending(current)) {
				schedule_timeout(1 * HZ);	
				continue;
			}
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&c2dev->req_vq_wo, &__wait);
			return -EINTR;
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&c2dev->req_vq_wo, &__wait);
		spin_lock(&c2dev->vqlock);
		msg = c2_mq_alloc(&c2dev->req_vq);
	}

	memcpy(msg, wr, c2dev->req_vq.msg_size);

	c2_mq_produce(&c2dev->req_vq);

	spin_unlock(&c2dev->vqlock);
	return 0;
}


int vq_wait_for_reply(struct c2_dev *c2dev, struct c2_vq_req *req)
{
	if (!wait_event_timeout(req->wait_object,
				atomic_read(&req->reply_ready),
				60*HZ))
		return -ETIMEDOUT;

	return 0;
}

void vq_repbuf_free(struct c2_dev *c2dev, void *reply)
{
	kmem_cache_free(c2dev->host_msg_cache, reply);
}
