/*
 * helper functions for SG DMA video4linux capture buffers
 *
 * The functions expect the hardware being able to scatter gather
 * (i.e. the buffers are not linear in physical memory, but fragmented
 * into PAGE_SIZE chunks).  They also assume the driver does not need
 * to touch the video data.
 *
 * (c) 2007 Mauro Carvalho Chehab, <mchehab@infradead.org>
 *
 * Highly based on video-buf written originally by:
 * (c) 2001,02 Gerd Knorr <kraxel@bytesex.org>
 * (c) 2006 Mauro Carvalho Chehab, <mchehab@infradead.org>
 * (c) 2006 Ted Walther and John Sokol
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2
 */
#ifndef _VIDEOBUF_DMA_SG_H
#define _VIDEOBUF_DMA_SG_H

#include <media/videobuf-core.h>



struct videobuf_dmabuf {
	u32                 magic;

	
	int                 offset;
	size_t		    size;
	struct page         **pages;

	
	void                *vaddr;

	
	dma_addr_t          bus_addr;

	
	struct scatterlist  *sglist;
	int                 sglen;
	int                 nr_pages;
	int                 direction;
};

struct videobuf_dma_sg_memory {
	u32                 magic;

	
	struct videobuf_dmabuf  dma;
};

void videobuf_dma_init(struct videobuf_dmabuf *dma);
int videobuf_dma_init_user(struct videobuf_dmabuf *dma, int direction,
			   unsigned long data, unsigned long size);
int videobuf_dma_init_kernel(struct videobuf_dmabuf *dma, int direction,
			     int nr_pages);
int videobuf_dma_init_overlay(struct videobuf_dmabuf *dma, int direction,
			      dma_addr_t addr, int nr_pages);
int videobuf_dma_free(struct videobuf_dmabuf *dma);

int videobuf_dma_map(struct device *dev, struct videobuf_dmabuf *dma);
int videobuf_dma_unmap(struct device *dev, struct videobuf_dmabuf *dma);
struct videobuf_dmabuf *videobuf_to_dma(struct videobuf_buffer *buf);

void *videobuf_sg_alloc(size_t size);

void videobuf_queue_sg_init(struct videobuf_queue *q,
			 const struct videobuf_queue_ops *ops,
			 struct device *dev,
			 spinlock_t *irqlock,
			 enum v4l2_buf_type type,
			 enum v4l2_field field,
			 unsigned int msize,
			 void *priv,
			 struct mutex *ext_lock);

#endif 

