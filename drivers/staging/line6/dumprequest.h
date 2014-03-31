/*
 * Line6 Linux USB driver - 0.9.1beta
 *
 * Copyright (C) 2004-2010 Markus Grabner (grabner@icg.tugraz.at)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

#ifndef DUMPREQUEST_H
#define DUMPREQUEST_H

#include <linux/usb.h>
#include <linux/wait.h>
#include <sound/core.h>

enum {
	LINE6_DUMP_NONE,
	LINE6_DUMP_CURRENT
};

struct line6_dump_reqbuf {
	unsigned char *buffer;

	size_t length;
};

struct line6_dump_request {
	wait_queue_head_t wait;

	int in_progress;

	struct line6_dump_reqbuf reqbufs[1];
};

extern void line6_dump_finished(struct line6_dump_request *l6dr);
extern int line6_dump_request_async(struct line6_dump_request *l6dr,
				    struct usb_line6 *line6, int num, int dest);
extern void line6_dump_started(struct line6_dump_request *l6dr, int dest);
extern void line6_dumpreq_destruct(struct line6_dump_request *l6dr);
extern void line6_dumpreq_destructbuf(struct line6_dump_request *l6dr, int num);
extern int line6_dumpreq_init(struct line6_dump_request *l6dr, const void *buf,
			      size_t len);
extern int line6_dumpreq_initbuf(struct line6_dump_request *l6dr,
				 const void *buf, size_t len, int num);
extern void line6_invalidate_current(struct line6_dump_request *l6dr);
extern void line6_dump_wait(struct line6_dump_request *l6dr);
extern int line6_dump_wait_interruptible(struct line6_dump_request *l6dr);
extern int line6_dump_wait_timeout(struct line6_dump_request *l6dr,
				   long timeout);

#endif
