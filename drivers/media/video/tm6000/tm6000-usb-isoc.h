/*
 *  tm6000-buf.c - driver for TM5600/TM6000/TM6010 USB video capture devices
 *
 *  Copyright (C) 2006-2007 Mauro Carvalho Chehab <mchehab@infradead.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/videodev2.h>

#define TM6000_URB_MSG_LEN 180

struct usb_isoc_ctl {
		
	int				max_pkt_size;

		
	int				num_bufs;

		
	struct urb			**urb;

		
	char				**transfer_buffer;

		
	u8				cmd;
	int				pos, size, pktsize;

		
	int				vfield, field;

		
	u32				tmp_buf;
	int				tmp_buf_len;

		
	struct tm6000_buffer		*buf;
};
