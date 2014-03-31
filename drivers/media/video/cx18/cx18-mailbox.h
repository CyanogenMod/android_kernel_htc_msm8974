/*
 *  cx18 mailbox functions
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *  Copyright (C) 2008  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA
 */

#ifndef _CX18_MAILBOX_H_
#define _CX18_MAILBOX_H_

#define MAX_MB_ARGUMENTS 6
#define CX2341X_MBOX_MAX_DATA 16

#define MB_RESERVED_HANDLE_0 0
#define MB_RESERVED_HANDLE_1 0xFFFFFFFF

#define APU 0
#define CPU 1
#define EPU 2
#define HPU 3

struct cx18;

struct cx18_mdl_ack {
    u32 id;        
    u32 data_used; 
};

struct cx18_mailbox {
    u32       request;
    u32       ack;
    u32       reserved[6];
    u32       cmd;
    
    u32       args[MAX_MB_ARGUMENTS];
    u32       error;
};

struct cx18_stream;

int cx18_api(struct cx18 *cx, u32 cmd, int args, u32 data[]);
int cx18_vapi_result(struct cx18 *cx, u32 data[MAX_MB_ARGUMENTS], u32 cmd,
		int args, ...);
int cx18_vapi(struct cx18 *cx, u32 cmd, int args, ...);
int cx18_api_func(void *priv, u32 cmd, int in, int out,
		u32 data[CX2341X_MBOX_MAX_DATA]);

void cx18_api_epu_cmd_irq(struct cx18 *cx, int rpu);

void cx18_in_work_handler(struct work_struct *work);

#endif
