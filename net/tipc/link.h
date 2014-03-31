/*
 * net/tipc/link.h: Include file for TIPC link code
 *
 * Copyright (c) 1995-2006, Ericsson AB
 * Copyright (c) 2004-2005, 2010-2011, Wind River Systems
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

#ifndef _TIPC_LINK_H
#define _TIPC_LINK_H

#include "log.h"
#include "msg.h"
#include "node.h"

#define PUSH_FAILED   1
#define PUSH_FINISHED 2


#define INVALID_LINK_SEQ 0x10000


#define WORKING_WORKING 560810u
#define WORKING_UNKNOWN 560811u
#define RESET_UNKNOWN   560812u
#define RESET_RESET     560813u


#define MAX_PKT_DEFAULT 1500


struct tipc_link {
	u32 addr;
	char name[TIPC_MAX_LINK_NAME];
	struct tipc_media_addr media_addr;
	struct timer_list timer;
	struct tipc_node *owner;
	struct list_head link_list;

	
	int started;
	u32 checkpoint;
	u32 peer_session;
	u32 peer_bearer_id;
	struct tipc_bearer *b_ptr;
	u32 tolerance;
	u32 continuity_interval;
	u32 abort_limit;
	int state;
	int blocked;
	u32 fsm_msg_cnt;
	struct {
		unchar hdr[INT_H_SIZE];
		unchar body[TIPC_MAX_IF_NAME];
	} proto_msg;
	struct tipc_msg *pmsg;
	u32 priority;
	u32 queue_limit[15];	

	
	u32 exp_msg_count;
	u32 reset_checkpoint;

	
	u32 max_pkt;
	u32 max_pkt_target;
	u32 max_pkt_probes;

	
	u32 out_queue_size;
	struct sk_buff *first_out;
	struct sk_buff *last_out;
	u32 next_out_no;
	u32 last_retransmitted;
	u32 stale_count;

	
	u32 next_in_no;
	u32 deferred_inqueue_sz;
	struct sk_buff *oldest_deferred_in;
	struct sk_buff *newest_deferred_in;
	u32 unacked_window;

	
	struct sk_buff *proto_msg_queue;
	u32 retransm_queue_size;
	u32 retransm_queue_head;
	struct sk_buff *next_out;
	struct list_head waiting_ports;

	
	u32 long_msg_seq_no;
	struct sk_buff *defragm_buf;

	
	struct {
		u32 sent_info;		
		u32 recv_info;		
		u32 sent_states;
		u32 recv_states;
		u32 sent_probes;
		u32 recv_probes;
		u32 sent_nacks;
		u32 recv_nacks;
		u32 sent_acks;
		u32 sent_bundled;
		u32 sent_bundles;
		u32 recv_bundled;
		u32 recv_bundles;
		u32 retransmitted;
		u32 sent_fragmented;
		u32 sent_fragments;
		u32 recv_fragmented;
		u32 recv_fragments;
		u32 link_congs;		
		u32 bearer_congs;
		u32 deferred_recv;
		u32 duplicates;
		u32 max_queue_sz;	
		u32 accu_queue_sz;	
		u32 queue_sz_counts;	
		u32 msg_length_counts;	
		u32 msg_lengths_total;	
		u32 msg_length_profile[7]; 
	} stats;
};

struct tipc_port;

struct tipc_link *tipc_link_create(struct tipc_node *n_ptr,
			      struct tipc_bearer *b_ptr,
			      const struct tipc_media_addr *media_addr);
void tipc_link_delete(struct tipc_link *l_ptr);
void tipc_link_changeover(struct tipc_link *l_ptr);
void tipc_link_send_duplicate(struct tipc_link *l_ptr, struct tipc_link *dest);
void tipc_link_reset_fragments(struct tipc_link *l_ptr);
int tipc_link_is_up(struct tipc_link *l_ptr);
int tipc_link_is_active(struct tipc_link *l_ptr);
u32 tipc_link_push_packet(struct tipc_link *l_ptr);
void tipc_link_stop(struct tipc_link *l_ptr);
struct sk_buff *tipc_link_cmd_config(const void *req_tlv_area, int req_tlv_space, u16 cmd);
struct sk_buff *tipc_link_cmd_show_stats(const void *req_tlv_area, int req_tlv_space);
struct sk_buff *tipc_link_cmd_reset_stats(const void *req_tlv_area, int req_tlv_space);
void tipc_link_reset(struct tipc_link *l_ptr);
int tipc_link_send(struct sk_buff *buf, u32 dest, u32 selector);
void tipc_link_send_names(struct list_head *message_list, u32 dest);
int tipc_link_send_buf(struct tipc_link *l_ptr, struct sk_buff *buf);
u32 tipc_link_get_max_pkt(u32 dest, u32 selector);
int tipc_link_send_sections_fast(struct tipc_port *sender,
				 struct iovec const *msg_sect,
				 const u32 num_sect,
				 unsigned int total_len,
				 u32 destnode);
void tipc_link_recv_bundle(struct sk_buff *buf);
int  tipc_link_recv_fragment(struct sk_buff **pending,
			     struct sk_buff **fb,
			     struct tipc_msg **msg);
void tipc_link_send_proto_msg(struct tipc_link *l_ptr, u32 msg_typ, int prob,
			      u32 gap, u32 tolerance, u32 priority,
			      u32 acked_mtu);
void tipc_link_push_queue(struct tipc_link *l_ptr);
u32 tipc_link_defer_pkt(struct sk_buff **head, struct sk_buff **tail,
		   struct sk_buff *buf);
void tipc_link_wakeup_ports(struct tipc_link *l_ptr, int all);
void tipc_link_set_queue_limits(struct tipc_link *l_ptr, u32 window);
void tipc_link_retransmit(struct tipc_link *l_ptr,
			  struct sk_buff *start, u32 retransmits);


static inline u32 buf_seqno(struct sk_buff *buf)
{
	return msg_seqno(buf_msg(buf));
}

static inline u32 mod(u32 x)
{
	return x & 0xffffu;
}

static inline int between(u32 lower, u32 upper, u32 n)
{
	if ((lower < n) && (n < upper))
		return 1;
	if ((upper < lower) && ((n > lower) || (n < upper)))
		return 1;
	return 0;
}

static inline int less_eq(u32 left, u32 right)
{
	return mod(right - left) < 32768u;
}

static inline int less(u32 left, u32 right)
{
	return less_eq(left, right) && (mod(right) != mod(left));
}

static inline u32 lesser(u32 left, u32 right)
{
	return less_eq(left, right) ? left : right;
}



static inline int link_working_working(struct tipc_link *l_ptr)
{
	return l_ptr->state == WORKING_WORKING;
}

static inline int link_working_unknown(struct tipc_link *l_ptr)
{
	return l_ptr->state == WORKING_UNKNOWN;
}

static inline int link_reset_unknown(struct tipc_link *l_ptr)
{
	return l_ptr->state == RESET_UNKNOWN;
}

static inline int link_reset_reset(struct tipc_link *l_ptr)
{
	return l_ptr->state == RESET_RESET;
}

static inline int link_blocked(struct tipc_link *l_ptr)
{
	return l_ptr->exp_msg_count || l_ptr->blocked;
}

static inline int link_congested(struct tipc_link *l_ptr)
{
	return l_ptr->out_queue_size >= l_ptr->queue_limit[0];
}

#endif
