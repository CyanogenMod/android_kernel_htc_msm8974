/*
 *      uvc_isight.c  --  USB Video Class driver - iSight support
 *
 *	Copyright (C) 2006-2007
 *		Ivan N. Zlatev <contact@i-nz.net>
 *	Copyright (C) 2008-2009
 *		Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/mm.h>

#include "uvcvideo.h"


static int isight_decode(struct uvc_video_queue *queue, struct uvc_buffer *buf,
		const __u8 *data, unsigned int len)
{
	static const __u8 hdr[] = {
		0x11, 0x22, 0x33, 0x44,
		0xde, 0xad, 0xbe, 0xef,
		0xde, 0xad, 0xfa, 0xce
	};

	unsigned int maxlen, nbytes;
	__u8 *mem;
	int is_header = 0;

	if (buf == NULL)
		return 0;

	if ((len >= 14 && memcmp(&data[2], hdr, 12) == 0) ||
	    (len >= 15 && memcmp(&data[3], hdr, 12) == 0)) {
		uvc_trace(UVC_TRACE_FRAME, "iSight header found\n");
		is_header = 1;
	}

	
	if (buf->state != UVC_BUF_STATE_ACTIVE) {
		if (!is_header) {
			uvc_trace(UVC_TRACE_FRAME, "Dropping packet (out of "
				  "sync).\n");
			return 0;
		}

		buf->state = UVC_BUF_STATE_ACTIVE;
	}

	if (is_header && buf->bytesused != 0) {
		buf->state = UVC_BUF_STATE_DONE;
		return -EAGAIN;
	}

	if (!is_header) {
		maxlen = buf->length - buf->bytesused;
		mem = buf->mem + buf->bytesused;
		nbytes = min(len, maxlen);
		memcpy(mem, data, nbytes);
		buf->bytesused += nbytes;

		if (len > maxlen || buf->bytesused == buf->length) {
			uvc_trace(UVC_TRACE_FRAME, "Frame complete "
				  "(overflow).\n");
			buf->state = UVC_BUF_STATE_DONE;
		}
	}

	return 0;
}

void uvc_video_decode_isight(struct urb *urb, struct uvc_streaming *stream,
		struct uvc_buffer *buf)
{
	int ret, i;

	for (i = 0; i < urb->number_of_packets; ++i) {
		if (urb->iso_frame_desc[i].status < 0) {
			uvc_trace(UVC_TRACE_FRAME, "USB isochronous frame "
				  "lost (%d).\n",
				  urb->iso_frame_desc[i].status);
		}

		do {
			ret = isight_decode(&stream->queue, buf,
					urb->transfer_buffer +
					urb->iso_frame_desc[i].offset,
					urb->iso_frame_desc[i].actual_length);

			if (buf == NULL)
				break;

			if (buf->state == UVC_BUF_STATE_DONE ||
			    buf->state == UVC_BUF_STATE_ERROR)
				buf = uvc_queue_next_buffer(&stream->queue,
							buf);
		} while (ret == -EAGAIN);
	}
}
