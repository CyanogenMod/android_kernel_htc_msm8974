/*
 * net/tipc/net.c: TIPC network routing code
 *
 * Copyright (c) 1995-2006, Ericsson AB
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

#include "core.h"
#include "net.h"
#include "name_distr.h"
#include "subscr.h"
#include "port.h"
#include "node.h"
#include "config.h"


DEFINE_RWLOCK(tipc_net_lock);

static void net_route_named_msg(struct sk_buff *buf)
{
	struct tipc_msg *msg = buf_msg(buf);
	u32 dnode;
	u32 dport;

	if (!msg_named(msg)) {
		kfree_skb(buf);
		return;
	}

	dnode = addr_domain(msg_lookup_scope(msg));
	dport = tipc_nametbl_translate(msg_nametype(msg), msg_nameinst(msg), &dnode);
	if (dport) {
		msg_set_destnode(msg, dnode);
		msg_set_destport(msg, dport);
		tipc_net_route_msg(buf);
		return;
	}
	tipc_reject_msg(buf, TIPC_ERR_NO_NAME);
}

void tipc_net_route_msg(struct sk_buff *buf)
{
	struct tipc_msg *msg;
	u32 dnode;

	if (!buf)
		return;
	msg = buf_msg(buf);

	
	dnode = msg_short(msg) ? tipc_own_addr : msg_destnode(msg);
	if (tipc_in_scope(dnode, tipc_own_addr)) {
		if (msg_isdata(msg)) {
			if (msg_mcast(msg))
				tipc_port_recv_mcast(buf, NULL);
			else if (msg_destport(msg))
				tipc_port_recv_msg(buf);
			else
				net_route_named_msg(buf);
			return;
		}
		switch (msg_user(msg)) {
		case NAME_DISTRIBUTOR:
			tipc_named_recv(buf);
			break;
		case CONN_MANAGER:
			tipc_port_recv_proto_msg(buf);
			break;
		default:
			kfree_skb(buf);
		}
		return;
	}

	
	skb_trim(buf, msg_size(msg));
	tipc_link_send(buf, dnode, msg_link_selector(msg));
}

int tipc_net_start(u32 addr)
{
	char addr_string[16];

	tipc_subscr_stop();
	tipc_cfg_stop();

	tipc_own_addr = addr;
	tipc_named_reinit();
	tipc_port_reinit();

	tipc_bclink_init();

	tipc_k_signal((Handler)tipc_subscr_start, 0);
	tipc_k_signal((Handler)tipc_cfg_init, 0);

	info("Started in network mode\n");
	info("Own node address %s, network identity %u\n",
	     tipc_addr_string_fill(addr_string, tipc_own_addr), tipc_net_id);
	return 0;
}

void tipc_net_stop(void)
{
	struct tipc_node *node, *t_node;

	if (!tipc_own_addr)
		return;
	write_lock_bh(&tipc_net_lock);
	tipc_bearer_stop();
	tipc_bclink_stop();
	list_for_each_entry_safe(node, t_node, &tipc_node_list, list)
		tipc_node_delete(node);
	write_unlock_bh(&tipc_net_lock);
	info("Left network mode\n");
}
