/*
 *	uvc_video.c  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include <media/v4l2-dev.h>

#include "uvc.h"
#include "uvc_queue.h"


static int
uvc_video_encode_header(struct uvc_video *video, struct uvc_buffer *buf,
		u8 *data, int len)
{
	data[0] = 2;
	data[1] = UVC_STREAM_EOH | video->fid;

	if (buf->buf.bytesused - video->queue.buf_used <= len - 2)
		data[1] |= UVC_STREAM_EOF;

	return 2;
}

static int
uvc_video_encode_data(struct uvc_video *video, struct uvc_buffer *buf,
		u8 *data, int len)
{
	struct uvc_video_queue *queue = &video->queue;
	unsigned int nbytes;
	void *mem;

	
	mem = queue->mem + buf->buf.m.offset + queue->buf_used;
	nbytes = min((unsigned int)len, buf->buf.bytesused - queue->buf_used);

	memcpy(data, mem, nbytes);
	queue->buf_used += nbytes;

	return nbytes;
}

static void
uvc_video_encode_bulk(struct usb_request *req, struct uvc_video *video,
		struct uvc_buffer *buf)
{
	void *mem = req->buf;
	int len = video->req_size;
	int ret;

	
	if (video->payload_size == 0) {
		ret = uvc_video_encode_header(video, buf, mem, len);
		video->payload_size += ret;
		mem += ret;
		len -= ret;
	}

	
	len = min((int)(video->max_payload_size - video->payload_size), len);
	ret = uvc_video_encode_data(video, buf, mem, len);

	video->payload_size += ret;
	len -= ret;

	req->length = video->req_size - len;
	req->zero = video->payload_size == video->max_payload_size;

	if (buf->buf.bytesused == video->queue.buf_used) {
		video->queue.buf_used = 0;
		buf->state = UVC_BUF_STATE_DONE;
		uvc_queue_next_buffer(&video->queue, buf);
		video->fid ^= UVC_STREAM_FID;

		video->payload_size = 0;
	}

	if (video->payload_size == video->max_payload_size ||
	    buf->buf.bytesused == video->queue.buf_used)
		video->payload_size = 0;
}

static void
uvc_video_encode_isoc(struct usb_request *req, struct uvc_video *video,
		struct uvc_buffer *buf)
{
	void *mem = req->buf;
	int len = video->req_size;
	int ret;

	
	ret = uvc_video_encode_header(video, buf, mem, len);
	mem += ret;
	len -= ret;

	
	ret = uvc_video_encode_data(video, buf, mem, len);
	len -= ret;

	req->length = video->req_size - len;

	if (buf->buf.bytesused == video->queue.buf_used) {
		video->queue.buf_used = 0;
		buf->state = UVC_BUF_STATE_DONE;
		uvc_queue_next_buffer(&video->queue, buf);
		video->fid ^= UVC_STREAM_FID;
	}
}


static void
uvc_video_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct uvc_video *video = req->context;
	struct uvc_buffer *buf;
	unsigned long flags;
	int ret;

	switch (req->status) {
	case 0:
		break;

	case -ESHUTDOWN:
		printk(KERN_INFO "VS request cancelled.\n");
		goto requeue;

	default:
		printk(KERN_INFO "VS request completed with status %d.\n",
			req->status);
		goto requeue;
	}

	spin_lock_irqsave(&video->queue.irqlock, flags);
	buf = uvc_queue_head(&video->queue);
	if (buf == NULL) {
		spin_unlock_irqrestore(&video->queue.irqlock, flags);
		goto requeue;
	}

	video->encode(req, video, buf);

	if ((ret = usb_ep_queue(ep, req, GFP_ATOMIC)) < 0) {
		printk(KERN_INFO "Failed to queue request (%d).\n", ret);
		usb_ep_set_halt(ep);
		spin_unlock_irqrestore(&video->queue.irqlock, flags);
		goto requeue;
	}
	spin_unlock_irqrestore(&video->queue.irqlock, flags);

	return;

requeue:
	spin_lock_irqsave(&video->req_lock, flags);
	list_add_tail(&req->list, &video->req_free);
	spin_unlock_irqrestore(&video->req_lock, flags);
}

static int
uvc_video_free_requests(struct uvc_video *video)
{
	unsigned int i;

	for (i = 0; i < UVC_NUM_REQUESTS; ++i) {
		if (video->req[i]) {
			usb_ep_free_request(video->ep, video->req[i]);
			video->req[i] = NULL;
		}

		if (video->req_buffer[i]) {
			kfree(video->req_buffer[i]);
			video->req_buffer[i] = NULL;
		}
	}

	INIT_LIST_HEAD(&video->req_free);
	video->req_size = 0;
	return 0;
}

static int
uvc_video_alloc_requests(struct uvc_video *video)
{
	unsigned int i;
	int ret = -ENOMEM;

	BUG_ON(video->req_size);

	for (i = 0; i < UVC_NUM_REQUESTS; ++i) {
		video->req_buffer[i] = kmalloc(video->ep->maxpacket, GFP_KERNEL);
		if (video->req_buffer[i] == NULL)
			goto error;

		video->req[i] = usb_ep_alloc_request(video->ep, GFP_KERNEL);
		if (video->req[i] == NULL)
			goto error;

		video->req[i]->buf = video->req_buffer[i];
		video->req[i]->length = 0;
		video->req[i]->dma = DMA_ADDR_INVALID;
		video->req[i]->complete = uvc_video_complete;
		video->req[i]->context = video;

		list_add_tail(&video->req[i]->list, &video->req_free);
	}

	video->req_size = video->ep->maxpacket;
	return 0;

error:
	uvc_video_free_requests(video);
	return ret;
}


static int
uvc_video_pump(struct uvc_video *video)
{
	struct usb_request *req;
	struct uvc_buffer *buf;
	unsigned long flags;
	int ret;


	while (1) {
		spin_lock_irqsave(&video->req_lock, flags);
		if (list_empty(&video->req_free)) {
			spin_unlock_irqrestore(&video->req_lock, flags);
			return 0;
		}
		req = list_first_entry(&video->req_free, struct usb_request,
					list);
		list_del(&req->list);
		spin_unlock_irqrestore(&video->req_lock, flags);

		spin_lock_irqsave(&video->queue.irqlock, flags);
		buf = uvc_queue_head(&video->queue);
		if (buf == NULL) {
			spin_unlock_irqrestore(&video->queue.irqlock, flags);
			break;
		}

		video->encode(req, video, buf);

		
		if ((ret = usb_ep_queue(video->ep, req, GFP_KERNEL)) < 0) {
			printk(KERN_INFO "Failed to queue request (%d)\n", ret);
			usb_ep_set_halt(video->ep);
			spin_unlock_irqrestore(&video->queue.irqlock, flags);
			break;
		}
		spin_unlock_irqrestore(&video->queue.irqlock, flags);
	}

	spin_lock_irqsave(&video->req_lock, flags);
	list_add_tail(&req->list, &video->req_free);
	spin_unlock_irqrestore(&video->req_lock, flags);
	return 0;
}

static int
uvc_video_enable(struct uvc_video *video, int enable)
{
	unsigned int i;
	int ret;

	if (video->ep == NULL) {
		printk(KERN_INFO "Video enable failed, device is "
			"uninitialized.\n");
		return -ENODEV;
	}

	if (!enable) {
		for (i = 0; i < UVC_NUM_REQUESTS; ++i)
			usb_ep_dequeue(video->ep, video->req[i]);

		uvc_video_free_requests(video);
		uvc_queue_enable(&video->queue, 0);
		return 0;
	}

	if ((ret = uvc_queue_enable(&video->queue, 1)) < 0)
		return ret;

	if ((ret = uvc_video_alloc_requests(video)) < 0)
		return ret;

	if (video->max_payload_size) {
		video->encode = uvc_video_encode_bulk;
		video->payload_size = 0;
	} else
		video->encode = uvc_video_encode_isoc;

	return uvc_video_pump(video);
}

static int
uvc_video_init(struct uvc_video *video)
{
	INIT_LIST_HEAD(&video->req_free);
	spin_lock_init(&video->req_lock);

	video->fcc = V4L2_PIX_FMT_YUYV;
	video->bpp = 16;
	video->width = 320;
	video->height = 240;
	video->imagesize = 320 * 240 * 2;

	
	uvc_queue_init(&video->queue, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	return 0;
}

