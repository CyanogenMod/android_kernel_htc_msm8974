/*
 * Memory-to-memory device framework for Video for Linux 2 and videobuf.
 *
 * Helper functions for devices that use videobuf buffers for both their
 * source and destination.
 *
 * Copyright (c) 2009-2010 Samsung Electronics Co., Ltd.
 * Pawel Osciak, <pawel@osciak.com>
 * Marek Szyprowski, <m.szyprowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <media/videobuf2-core.h>
#include <media/v4l2-mem2mem.h>

MODULE_DESCRIPTION("Mem to mem device framework for videobuf");
MODULE_AUTHOR("Pawel Osciak, <pawel@osciak.com>");
MODULE_LICENSE("GPL");

static bool debug;
module_param(debug, bool, 0644);

#define dprintk(fmt, arg...)						\
	do {								\
		if (debug)						\
			printk(KERN_DEBUG "%s: " fmt, __func__, ## arg);\
	} while (0)


#define TRANS_QUEUED		(1 << 0)
#define TRANS_RUNNING		(1 << 1)


#define DST_QUEUE_OFF_BASE	(1 << 30)


struct v4l2_m2m_dev {
	struct v4l2_m2m_ctx	*curr_ctx;

	struct list_head	job_queue;
	spinlock_t		job_spinlock;

	struct v4l2_m2m_ops	*m2m_ops;
};

static struct v4l2_m2m_queue_ctx *get_queue_ctx(struct v4l2_m2m_ctx *m2m_ctx,
						enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &m2m_ctx->out_q_ctx;
	else
		return &m2m_ctx->cap_q_ctx;
}

struct vb2_queue *v4l2_m2m_get_vq(struct v4l2_m2m_ctx *m2m_ctx,
				       enum v4l2_buf_type type)
{
	struct v4l2_m2m_queue_ctx *q_ctx;

	q_ctx = get_queue_ctx(m2m_ctx, type);
	if (!q_ctx)
		return NULL;

	return &q_ctx->q;
}
EXPORT_SYMBOL(v4l2_m2m_get_vq);

void *v4l2_m2m_next_buf(struct v4l2_m2m_queue_ctx *q_ctx)
{
	struct v4l2_m2m_buffer *b = NULL;
	unsigned long flags;

	spin_lock_irqsave(&q_ctx->rdy_spinlock, flags);

	if (list_empty(&q_ctx->rdy_queue)) {
		spin_unlock_irqrestore(&q_ctx->rdy_spinlock, flags);
		return NULL;
	}

	b = list_entry(q_ctx->rdy_queue.next, struct v4l2_m2m_buffer, list);
	spin_unlock_irqrestore(&q_ctx->rdy_spinlock, flags);
	return &b->vb;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_next_buf);

void *v4l2_m2m_buf_remove(struct v4l2_m2m_queue_ctx *q_ctx)
{
	struct v4l2_m2m_buffer *b = NULL;
	unsigned long flags;

	spin_lock_irqsave(&q_ctx->rdy_spinlock, flags);
	if (list_empty(&q_ctx->rdy_queue)) {
		spin_unlock_irqrestore(&q_ctx->rdy_spinlock, flags);
		return NULL;
	}
	b = list_entry(q_ctx->rdy_queue.next, struct v4l2_m2m_buffer, list);
	list_del(&b->list);
	q_ctx->num_rdy--;
	spin_unlock_irqrestore(&q_ctx->rdy_spinlock, flags);

	return &b->vb;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_remove);


void *v4l2_m2m_get_curr_priv(struct v4l2_m2m_dev *m2m_dev)
{
	unsigned long flags;
	void *ret = NULL;

	spin_lock_irqsave(&m2m_dev->job_spinlock, flags);
	if (m2m_dev->curr_ctx)
		ret = m2m_dev->curr_ctx->priv;
	spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);

	return ret;
}
EXPORT_SYMBOL(v4l2_m2m_get_curr_priv);

static void v4l2_m2m_try_run(struct v4l2_m2m_dev *m2m_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&m2m_dev->job_spinlock, flags);
	if (NULL != m2m_dev->curr_ctx) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
		dprintk("Another instance is running, won't run now\n");
		return;
	}

	if (list_empty(&m2m_dev->job_queue)) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
		dprintk("No job pending\n");
		return;
	}

	m2m_dev->curr_ctx = list_entry(m2m_dev->job_queue.next,
				   struct v4l2_m2m_ctx, queue);
	m2m_dev->curr_ctx->job_flags |= TRANS_RUNNING;
	spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);

	m2m_dev->m2m_ops->device_run(m2m_dev->curr_ctx->priv);
}

static void v4l2_m2m_try_schedule(struct v4l2_m2m_ctx *m2m_ctx)
{
	struct v4l2_m2m_dev *m2m_dev;
	unsigned long flags_job, flags;

	m2m_dev = m2m_ctx->m2m_dev;
	dprintk("Trying to schedule a job for m2m_ctx: %p\n", m2m_ctx);

	if (!m2m_ctx->out_q_ctx.q.streaming
	    || !m2m_ctx->cap_q_ctx.q.streaming) {
		dprintk("Streaming needs to be on for both queues\n");
		return;
	}

	spin_lock_irqsave(&m2m_dev->job_spinlock, flags_job);
	if (m2m_ctx->job_flags & TRANS_QUEUED) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags_job);
		dprintk("On job queue already\n");
		return;
	}

	spin_lock_irqsave(&m2m_ctx->out_q_ctx.rdy_spinlock, flags);
	if (list_empty(&m2m_ctx->out_q_ctx.rdy_queue)) {
		spin_unlock_irqrestore(&m2m_ctx->out_q_ctx.rdy_spinlock, flags);
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags_job);
		dprintk("No input buffers available\n");
		return;
	}
	if (list_empty(&m2m_ctx->cap_q_ctx.rdy_queue)) {
		spin_unlock_irqrestore(&m2m_ctx->out_q_ctx.rdy_spinlock, flags);
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags_job);
		dprintk("No output buffers available\n");
		return;
	}
	spin_unlock_irqrestore(&m2m_ctx->out_q_ctx.rdy_spinlock, flags);

	if (m2m_dev->m2m_ops->job_ready
		&& (!m2m_dev->m2m_ops->job_ready(m2m_ctx->priv))) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags_job);
		dprintk("Driver not ready\n");
		return;
	}

	list_add_tail(&m2m_ctx->queue, &m2m_dev->job_queue);
	m2m_ctx->job_flags |= TRANS_QUEUED;

	spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags_job);

	v4l2_m2m_try_run(m2m_dev);
}

void v4l2_m2m_job_finish(struct v4l2_m2m_dev *m2m_dev,
			 struct v4l2_m2m_ctx *m2m_ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&m2m_dev->job_spinlock, flags);
	if (!m2m_dev->curr_ctx || m2m_dev->curr_ctx != m2m_ctx) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
		dprintk("Called by an instance not currently running\n");
		return;
	}

	list_del(&m2m_dev->curr_ctx->queue);
	m2m_dev->curr_ctx->job_flags &= ~(TRANS_QUEUED | TRANS_RUNNING);
	wake_up(&m2m_dev->curr_ctx->finished);
	m2m_dev->curr_ctx = NULL;

	spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);

	v4l2_m2m_try_schedule(m2m_ctx);
	v4l2_m2m_try_run(m2m_dev);
}
EXPORT_SYMBOL(v4l2_m2m_job_finish);

int v4l2_m2m_reqbufs(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		     struct v4l2_requestbuffers *reqbufs)
{
	struct vb2_queue *vq;

	vq = v4l2_m2m_get_vq(m2m_ctx, reqbufs->type);
	return vb2_reqbufs(vq, reqbufs);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_reqbufs);

int v4l2_m2m_querybuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		      struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret = 0;
	unsigned int i;

	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	ret = vb2_querybuf(vq, buf);

	
	if (buf->memory == V4L2_MEMORY_MMAP && !V4L2_TYPE_IS_OUTPUT(vq->type)) {
		if (V4L2_TYPE_IS_MULTIPLANAR(vq->type)) {
			for (i = 0; i < buf->length; ++i)
				buf->m.planes[i].m.mem_offset
					+= DST_QUEUE_OFF_BASE;
		} else {
			buf->m.offset += DST_QUEUE_OFF_BASE;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_querybuf);

int v4l2_m2m_qbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		  struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret;

	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	ret = vb2_qbuf(vq, buf);
	if (!ret)
		v4l2_m2m_try_schedule(m2m_ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_qbuf);

int v4l2_m2m_dqbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		   struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;

	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	return vb2_dqbuf(vq, buf, file->f_flags & O_NONBLOCK);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_dqbuf);

int v4l2_m2m_streamon(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		      enum v4l2_buf_type type)
{
	struct vb2_queue *vq;
	int ret;

	vq = v4l2_m2m_get_vq(m2m_ctx, type);
	ret = vb2_streamon(vq, type);
	if (!ret)
		v4l2_m2m_try_schedule(m2m_ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_streamon);

int v4l2_m2m_streamoff(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		       enum v4l2_buf_type type)
{
	struct vb2_queue *vq;

	vq = v4l2_m2m_get_vq(m2m_ctx, type);
	return vb2_streamoff(vq, type);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_streamoff);

unsigned int v4l2_m2m_poll(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
			   struct poll_table_struct *wait)
{
	struct vb2_queue *src_q, *dst_q;
	struct vb2_buffer *src_vb = NULL, *dst_vb = NULL;
	unsigned int rc = 0;
	unsigned long flags;

	src_q = v4l2_m2m_get_src_vq(m2m_ctx);
	dst_q = v4l2_m2m_get_dst_vq(m2m_ctx);

	if ((!src_q->streaming || list_empty(&src_q->queued_list))
		&& (!dst_q->streaming || list_empty(&dst_q->queued_list))) {
		rc = POLLERR;
		goto end;
	}

	if (m2m_ctx->m2m_dev->m2m_ops->unlock)
		m2m_ctx->m2m_dev->m2m_ops->unlock(m2m_ctx->priv);

	poll_wait(file, &src_q->done_wq, wait);
	poll_wait(file, &dst_q->done_wq, wait);

	if (m2m_ctx->m2m_dev->m2m_ops->lock)
		m2m_ctx->m2m_dev->m2m_ops->lock(m2m_ctx->priv);

	spin_lock_irqsave(&src_q->done_lock, flags);
	if (!list_empty(&src_q->done_list))
		src_vb = list_first_entry(&src_q->done_list, struct vb2_buffer,
						done_entry);
	if (src_vb && (src_vb->state == VB2_BUF_STATE_DONE
			|| src_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&src_q->done_lock, flags);

	spin_lock_irqsave(&dst_q->done_lock, flags);
	if (!list_empty(&dst_q->done_list))
		dst_vb = list_first_entry(&dst_q->done_list, struct vb2_buffer,
						done_entry);
	if (dst_vb && (dst_vb->state == VB2_BUF_STATE_DONE
			|| dst_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&dst_q->done_lock, flags);

end:
	return rc;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_poll);

int v4l2_m2m_mmap(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
			 struct vm_area_struct *vma)
{
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	struct vb2_queue *vq;

	if (offset < DST_QUEUE_OFF_BASE) {
		vq = v4l2_m2m_get_src_vq(m2m_ctx);
	} else {
		vq = v4l2_m2m_get_dst_vq(m2m_ctx);
		vma->vm_pgoff -= (DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
	}

	return vb2_mmap(vq, vma);
}
EXPORT_SYMBOL(v4l2_m2m_mmap);

struct v4l2_m2m_dev *v4l2_m2m_init(struct v4l2_m2m_ops *m2m_ops)
{
	struct v4l2_m2m_dev *m2m_dev;

	if (!m2m_ops)
		return ERR_PTR(-EINVAL);

	BUG_ON(!m2m_ops->device_run);
	BUG_ON(!m2m_ops->job_abort);

	m2m_dev = kzalloc(sizeof *m2m_dev, GFP_KERNEL);
	if (!m2m_dev)
		return ERR_PTR(-ENOMEM);

	m2m_dev->curr_ctx = NULL;
	m2m_dev->m2m_ops = m2m_ops;
	INIT_LIST_HEAD(&m2m_dev->job_queue);
	spin_lock_init(&m2m_dev->job_spinlock);

	return m2m_dev;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_init);

void v4l2_m2m_release(struct v4l2_m2m_dev *m2m_dev)
{
	kfree(m2m_dev);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_release);

struct v4l2_m2m_ctx *v4l2_m2m_ctx_init(struct v4l2_m2m_dev *m2m_dev,
		void *drv_priv,
		int (*queue_init)(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq))
{
	struct v4l2_m2m_ctx *m2m_ctx;
	struct v4l2_m2m_queue_ctx *out_q_ctx, *cap_q_ctx;
	int ret;

	m2m_ctx = kzalloc(sizeof *m2m_ctx, GFP_KERNEL);
	if (!m2m_ctx)
		return ERR_PTR(-ENOMEM);

	m2m_ctx->priv = drv_priv;
	m2m_ctx->m2m_dev = m2m_dev;
	init_waitqueue_head(&m2m_ctx->finished);

	out_q_ctx = &m2m_ctx->out_q_ctx;
	cap_q_ctx = &m2m_ctx->cap_q_ctx;

	INIT_LIST_HEAD(&out_q_ctx->rdy_queue);
	INIT_LIST_HEAD(&cap_q_ctx->rdy_queue);
	spin_lock_init(&out_q_ctx->rdy_spinlock);
	spin_lock_init(&cap_q_ctx->rdy_spinlock);

	INIT_LIST_HEAD(&m2m_ctx->queue);

	ret = queue_init(drv_priv, &out_q_ctx->q, &cap_q_ctx->q);

	if (ret)
		goto err;

	return m2m_ctx;
err:
	kfree(m2m_ctx);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_ctx_init);

void v4l2_m2m_ctx_release(struct v4l2_m2m_ctx *m2m_ctx)
{
	struct v4l2_m2m_dev *m2m_dev;
	unsigned long flags;

	m2m_dev = m2m_ctx->m2m_dev;

	spin_lock_irqsave(&m2m_dev->job_spinlock, flags);
	if (m2m_ctx->job_flags & TRANS_RUNNING) {
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
		m2m_dev->m2m_ops->job_abort(m2m_ctx->priv);
		dprintk("m2m_ctx %p running, will wait to complete", m2m_ctx);
		wait_event(m2m_ctx->finished, !(m2m_ctx->job_flags & TRANS_RUNNING));
	} else if (m2m_ctx->job_flags & TRANS_QUEUED) {
		list_del(&m2m_ctx->queue);
		m2m_ctx->job_flags &= ~(TRANS_QUEUED | TRANS_RUNNING);
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
		dprintk("m2m_ctx: %p had been on queue and was removed\n",
			m2m_ctx);
	} else {
		
		spin_unlock_irqrestore(&m2m_dev->job_spinlock, flags);
	}

	vb2_queue_release(&m2m_ctx->cap_q_ctx.q);
	vb2_queue_release(&m2m_ctx->out_q_ctx.q);

	kfree(m2m_ctx);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_ctx_release);

void v4l2_m2m_buf_queue(struct v4l2_m2m_ctx *m2m_ctx, struct vb2_buffer *vb)
{
	struct v4l2_m2m_buffer *b = container_of(vb, struct v4l2_m2m_buffer, vb);
	struct v4l2_m2m_queue_ctx *q_ctx;
	unsigned long flags;

	q_ctx = get_queue_ctx(m2m_ctx, vb->vb2_queue->type);
	if (!q_ctx)
		return;

	spin_lock_irqsave(&q_ctx->rdy_spinlock, flags);
	list_add_tail(&b->list, &q_ctx->rdy_queue);
	q_ctx->num_rdy++;
	spin_unlock_irqrestore(&q_ctx->rdy_spinlock, flags);
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_queue);

