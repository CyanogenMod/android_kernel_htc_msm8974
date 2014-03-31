/*
 * Memory-to-memory device framework for Video for Linux 2.
 *
 * Helper functions for devices that use memory buffers for both source
 * and destination.
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 * Pawel Osciak, <pawel@osciak.com>
 * Marek Szyprowski, <m.szyprowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#ifndef _MEDIA_V4L2_MEM2MEM_H
#define _MEDIA_V4L2_MEM2MEM_H

#include <media/videobuf2-core.h>

struct v4l2_m2m_ops {
	void (*device_run)(void *priv);
	int (*job_ready)(void *priv);
	void (*job_abort)(void *priv);
	void (*lock)(void *priv);
	void (*unlock)(void *priv);
};

struct v4l2_m2m_dev;

struct v4l2_m2m_queue_ctx {
	struct vb2_queue	q;

	struct list_head	rdy_queue;
	spinlock_t		rdy_spinlock;
	u8			num_rdy;
};

struct v4l2_m2m_ctx {
	struct v4l2_m2m_dev		*m2m_dev;

	
	struct v4l2_m2m_queue_ctx	cap_q_ctx;

	
	struct v4l2_m2m_queue_ctx	out_q_ctx;

	
	struct list_head		queue;
	unsigned long			job_flags;
	wait_queue_head_t		finished;

	
	void				*priv;
};

struct v4l2_m2m_buffer {
	struct vb2_buffer	vb;
	struct list_head	list;
};

void *v4l2_m2m_get_curr_priv(struct v4l2_m2m_dev *m2m_dev);

struct vb2_queue *v4l2_m2m_get_vq(struct v4l2_m2m_ctx *m2m_ctx,
				       enum v4l2_buf_type type);

void v4l2_m2m_job_finish(struct v4l2_m2m_dev *m2m_dev,
			 struct v4l2_m2m_ctx *m2m_ctx);

static inline void
v4l2_m2m_buf_done(struct vb2_buffer *buf, enum vb2_buffer_state state)
{
	vb2_buffer_done(buf, state);
}

int v4l2_m2m_reqbufs(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		     struct v4l2_requestbuffers *reqbufs);

int v4l2_m2m_querybuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		      struct v4l2_buffer *buf);

int v4l2_m2m_qbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		  struct v4l2_buffer *buf);
int v4l2_m2m_dqbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		   struct v4l2_buffer *buf);

int v4l2_m2m_streamon(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		      enum v4l2_buf_type type);
int v4l2_m2m_streamoff(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		       enum v4l2_buf_type type);

unsigned int v4l2_m2m_poll(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
			   struct poll_table_struct *wait);

int v4l2_m2m_mmap(struct file *file, struct v4l2_m2m_ctx *m2m_ctx,
		  struct vm_area_struct *vma);

struct v4l2_m2m_dev *v4l2_m2m_init(struct v4l2_m2m_ops *m2m_ops);
void v4l2_m2m_release(struct v4l2_m2m_dev *m2m_dev);

struct v4l2_m2m_ctx *v4l2_m2m_ctx_init(struct v4l2_m2m_dev *m2m_dev,
		void *drv_priv,
		int (*queue_init)(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq));

void v4l2_m2m_ctx_release(struct v4l2_m2m_ctx *m2m_ctx);

void v4l2_m2m_buf_queue(struct v4l2_m2m_ctx *m2m_ctx, struct vb2_buffer *vb);

static inline
unsigned int v4l2_m2m_num_src_bufs_ready(struct v4l2_m2m_ctx *m2m_ctx)
{
	return m2m_ctx->cap_q_ctx.num_rdy;
}

static inline
unsigned int v4l2_m2m_num_dst_bufs_ready(struct v4l2_m2m_ctx *m2m_ctx)
{
	return m2m_ctx->out_q_ctx.num_rdy;
}

void *v4l2_m2m_next_buf(struct v4l2_m2m_queue_ctx *q_ctx);

static inline void *v4l2_m2m_next_src_buf(struct v4l2_m2m_ctx *m2m_ctx)
{
	return v4l2_m2m_next_buf(&m2m_ctx->out_q_ctx);
}

static inline void *v4l2_m2m_next_dst_buf(struct v4l2_m2m_ctx *m2m_ctx)
{
	return v4l2_m2m_next_buf(&m2m_ctx->cap_q_ctx);
}

static inline
struct vb2_queue *v4l2_m2m_get_src_vq(struct v4l2_m2m_ctx *m2m_ctx)
{
	return &m2m_ctx->out_q_ctx.q;
}

static inline
struct vb2_queue *v4l2_m2m_get_dst_vq(struct v4l2_m2m_ctx *m2m_ctx)
{
	return &m2m_ctx->cap_q_ctx.q;
}

void *v4l2_m2m_buf_remove(struct v4l2_m2m_queue_ctx *q_ctx);

static inline void *v4l2_m2m_src_buf_remove(struct v4l2_m2m_ctx *m2m_ctx)
{
	return v4l2_m2m_buf_remove(&m2m_ctx->out_q_ctx);
}

static inline void *v4l2_m2m_dst_buf_remove(struct v4l2_m2m_ctx *m2m_ctx)
{
	return v4l2_m2m_buf_remove(&m2m_ctx->cap_q_ctx);
}

#endif 

