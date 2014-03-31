/*
 * net/tipc/bearer.h: Include file for TIPC bearer code
 *
 * Copyright (c) 1996-2006, Ericsson AB
 * Copyright (c) 2005, 2010-2011, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TIPC_BEARER_H
#define _TIPC_BEARER_H

#include "bcast.h"

#define MAX_BEARERS	2
#define MAX_MEDIA	2


#define TIPC_MEDIA_ADDR_SIZE	20
#define TIPC_MEDIA_TYPE_OFFSET	3

#define TIPC_MEDIA_TYPE_ETH	1


struct tipc_media_addr {
	u8 value[TIPC_MEDIA_ADDR_SIZE];
	u8 media_id;
	u8 broadcast;
};

struct tipc_bearer;


struct tipc_media {
	int (*send_msg)(struct sk_buff *buf,
			struct tipc_bearer *b_ptr,
			struct tipc_media_addr *dest);
	int (*enable_bearer)(struct tipc_bearer *b_ptr);
	void (*disable_bearer)(struct tipc_bearer *b_ptr);
	int (*addr2str)(struct tipc_media_addr *a, char *str_buf, int str_size);
	int (*str2addr)(struct tipc_media_addr *a, char *str_buf);
	int (*addr2msg)(struct tipc_media_addr *a, char *msg_area);
	int (*msg2addr)(struct tipc_media_addr *a, char *msg_area);
	struct tipc_media_addr bcast_addr;
	u32 priority;
	u32 tolerance;
	u32 window;
	u32 type_id;
	char name[TIPC_MAX_MEDIA_NAME];
};

struct tipc_bearer {
	void *usr_handle;			
	u32 mtu;				
	int blocked;				
	struct tipc_media_addr addr;		
	char name[TIPC_MAX_BEARER_NAME];
	spinlock_t lock;
	struct tipc_media *media;
	u32 priority;
	u32 window;
	u32 tolerance;
	u32 identity;
	struct tipc_link_req *link_req;
	struct list_head links;
	struct list_head cong_links;
	int active;
	char net_plane;
	struct tipc_node_map nodes;
};

struct tipc_bearer_names {
	char media_name[TIPC_MAX_MEDIA_NAME];
	char if_name[TIPC_MAX_IF_NAME];
};

struct tipc_link;

extern struct tipc_bearer tipc_bearers[];

int tipc_register_media(struct tipc_media *m_ptr);

void tipc_recv_msg(struct sk_buff *buf, struct tipc_bearer *tb_ptr);

int  tipc_block_bearer(const char *name);
void tipc_continue(struct tipc_bearer *tb_ptr);

int tipc_enable_bearer(const char *bearer_name, u32 disc_domain, u32 priority);
int tipc_disable_bearer(const char *name);

int  tipc_eth_media_start(void);
void tipc_eth_media_stop(void);

int tipc_media_set_priority(const char *name, u32 new_value);
int tipc_media_set_window(const char *name, u32 new_value);
void tipc_media_addr_printf(struct print_buf *pb, struct tipc_media_addr *a);
struct sk_buff *tipc_media_get_names(void);

struct sk_buff *tipc_bearer_get_names(void);
void tipc_bearer_add_dest(struct tipc_bearer *b_ptr, u32 dest);
void tipc_bearer_remove_dest(struct tipc_bearer *b_ptr, u32 dest);
void tipc_bearer_schedule(struct tipc_bearer *b_ptr, struct tipc_link *l_ptr);
struct tipc_bearer *tipc_bearer_find(const char *name);
struct tipc_bearer *tipc_bearer_find_interface(const char *if_name);
struct tipc_media *tipc_media_find(const char *name);
int tipc_bearer_resolve_congestion(struct tipc_bearer *b_ptr,
				   struct tipc_link *l_ptr);
int tipc_bearer_congested(struct tipc_bearer *b_ptr, struct tipc_link *l_ptr);
void tipc_bearer_stop(void);
void tipc_bearer_lock_push(struct tipc_bearer *b_ptr);



static inline int tipc_bearer_send(struct tipc_bearer *b_ptr,
				   struct sk_buff *buf,
				   struct tipc_media_addr *dest)
{
	return !b_ptr->media->send_msg(buf, b_ptr, dest);
}

#endif	
