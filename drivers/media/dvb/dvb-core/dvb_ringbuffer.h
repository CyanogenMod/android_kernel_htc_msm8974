/*
 *
 * dvb_ringbuffer.h: ring buffer implementation for the dvb driver
 *
 * Copyright (C) 2003 Oliver Endriss
 * Copyright (C) 2004 Andrew de Quincey
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * based on code originally found in av7110.c & dvb_ci.c:
 * Copyright (C) 1999-2003 Ralph Metzler & Marcus Metzler
 *                         for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _DVB_RINGBUFFER_H_
#define _DVB_RINGBUFFER_H_

#include <linux/spinlock.h>
#include <linux/wait.h>

struct dvb_ringbuffer {
	u8               *data;
	ssize_t           size;
	ssize_t           pread;
	ssize_t           pwrite;
	int               error;

	wait_queue_head_t queue;
	spinlock_t        lock;
};

#define DVB_RINGBUFFER_PKTHDRSIZE 3



extern void dvb_ringbuffer_init(struct dvb_ringbuffer *rbuf, void *data, size_t len);

extern int dvb_ringbuffer_empty(struct dvb_ringbuffer *rbuf);

extern ssize_t dvb_ringbuffer_free(struct dvb_ringbuffer *rbuf);

extern ssize_t dvb_ringbuffer_avail(struct dvb_ringbuffer *rbuf);


extern void dvb_ringbuffer_reset(struct dvb_ringbuffer *rbuf);


extern void dvb_ringbuffer_flush(struct dvb_ringbuffer *rbuf);

extern void dvb_ringbuffer_flush_spinlock_wakeup(struct dvb_ringbuffer *rbuf);

#define DVB_RINGBUFFER_PEEK(rbuf,offs)	\
			(rbuf)->data[((rbuf)->pread+(offs))%(rbuf)->size]

#define DVB_RINGBUFFER_SKIP(rbuf,num)	\
			(rbuf)->pread=((rbuf)->pread+(num))%(rbuf)->size

#define DVB_RINGBUFFER_PUSH(rbuf, num)	\
			((rbuf)->pwrite = (((rbuf)->pwrite+(num))%(rbuf)->size))

extern ssize_t dvb_ringbuffer_read_user(struct dvb_ringbuffer *rbuf,
				   u8 __user *buf, size_t len);
extern void dvb_ringbuffer_read(struct dvb_ringbuffer *rbuf,
				   u8 *buf, size_t len);


#define DVB_RINGBUFFER_WRITE_BYTE(rbuf,byte)	\
			{ (rbuf)->data[(rbuf)->pwrite]=(byte); \
			(rbuf)->pwrite=((rbuf)->pwrite+1)%(rbuf)->size; }
extern ssize_t dvb_ringbuffer_write(struct dvb_ringbuffer *rbuf, const u8 *buf,
				    size_t len);

extern ssize_t dvb_ringbuffer_write_user(struct dvb_ringbuffer *rbuf,
					const u8 *buf, size_t len);

/**
 * Write a packet into the ringbuffer.
 *
 * <rbuf> Ringbuffer to write to.
 * <buf> Buffer to write.
 * <len> Length of buffer (currently limited to 65535 bytes max).
 * returns Number of bytes written, or -EFAULT, -ENOMEM, -EVINAL.
 */
extern ssize_t dvb_ringbuffer_pkt_write(struct dvb_ringbuffer *rbuf, u8* buf,
					size_t len);

extern ssize_t dvb_ringbuffer_pkt_read_user(struct dvb_ringbuffer *rbuf, size_t idx,
				       int offset, u8 __user *buf, size_t len);
extern ssize_t dvb_ringbuffer_pkt_read(struct dvb_ringbuffer *rbuf, size_t idx,
				       int offset, u8 *buf, size_t len);

extern void dvb_ringbuffer_pkt_dispose(struct dvb_ringbuffer *rbuf, size_t idx);

extern ssize_t dvb_ringbuffer_pkt_next(struct dvb_ringbuffer *rbuf, size_t idx, size_t* pktlen);


/**
 * Start a new packet that will be written directly by the user to the packet buffer.
 * The function only writes the header of the packet into the packet buffer,
 * and the packet is in pending state (can't be read by the reader) until it is
 * closed using dvb_ringbuffer_pkt_close. You must write the data into the
 * packet buffer using dvb_ringbuffer_write followed by
 * dvb_ringbuffer_pkt_close.
 *
 * <rbuf> Ringbuffer concerned.
 * <len> Size of the packet's data
 * returns Index of the packet's header that was started.
 */
extern ssize_t dvb_ringbuffer_pkt_start(struct dvb_ringbuffer *rbuf,
						size_t len);

extern int dvb_ringbuffer_pkt_close(struct dvb_ringbuffer *rbuf, ssize_t idx);


#endif 
