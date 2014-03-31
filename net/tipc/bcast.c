/*
 * net/tipc/bcast.c: TIPC broadcast code
 *
 * Copyright (c) 2004-2006, Ericsson AB
 * Copyright (c) 2004, Intel Corporation.
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
#include "link.h"
#include "port.h"
#include "bcast.h"
#include "name_distr.h"

#define MAX_PKT_DEFAULT_MCAST 1500	

#define BCLINK_WIN_DEFAULT 20		


struct tipc_bcbearer_pair {
	struct tipc_bearer *primary;
	struct tipc_bearer *secondary;
};


struct tipc_bcbearer {
	struct tipc_bearer bearer;
	struct tipc_media media;
	struct tipc_bcbearer_pair bpairs[MAX_BEARERS];
	struct tipc_bcbearer_pair bpairs_temp[TIPC_MAX_LINK_PRI + 1];
	struct tipc_node_map remains;
	struct tipc_node_map remains_new;
};


struct tipc_bclink {
	struct tipc_link link;
	struct tipc_node node;
	struct tipc_node_map bcast_nodes;
	struct tipc_node *retransmit_to;
};

static struct tipc_bcbearer bcast_bearer;
static struct tipc_bclink bcast_link;

static struct tipc_bcbearer *bcbearer = &bcast_bearer;
static struct tipc_bclink *bclink = &bcast_link;
static struct tipc_link *bcl = &bcast_link.link;

static DEFINE_SPINLOCK(bc_lock);

const char tipc_bclink_name[] = "broadcast-link";

static void tipc_nmap_diff(struct tipc_node_map *nm_a,
			   struct tipc_node_map *nm_b,
			   struct tipc_node_map *nm_diff);

static u32 bcbuf_acks(struct sk_buff *buf)
{
	return (u32)(unsigned long)TIPC_SKB_CB(buf)->handle;
}

static void bcbuf_set_acks(struct sk_buff *buf, u32 acks)
{
	TIPC_SKB_CB(buf)->handle = (void *)(unsigned long)acks;
}

static void bcbuf_decr_acks(struct sk_buff *buf)
{
	bcbuf_set_acks(buf, bcbuf_acks(buf) - 1);
}

void tipc_bclink_add_node(u32 addr)
{
	spin_lock_bh(&bc_lock);
	tipc_nmap_add(&bclink->bcast_nodes, addr);
	spin_unlock_bh(&bc_lock);
}

void tipc_bclink_remove_node(u32 addr)
{
	spin_lock_bh(&bc_lock);
	tipc_nmap_remove(&bclink->bcast_nodes, addr);
	spin_unlock_bh(&bc_lock);
}

static void bclink_set_last_sent(void)
{
	if (bcl->next_out)
		bcl->fsm_msg_cnt = mod(buf_seqno(bcl->next_out) - 1);
	else
		bcl->fsm_msg_cnt = mod(bcl->next_out_no - 1);
}

u32 tipc_bclink_get_last_sent(void)
{
	return bcl->fsm_msg_cnt;
}

static void bclink_update_last_sent(struct tipc_node *node, u32 seqno)
{
	node->bclink.last_sent = less_eq(node->bclink.last_sent, seqno) ?
						seqno : node->bclink.last_sent;
}



struct tipc_node *tipc_bclink_retransmit_to(void)
{
	return bclink->retransmit_to;
}


static void bclink_retransmit_pkt(u32 after, u32 to)
{
	struct sk_buff *buf;

	buf = bcl->first_out;
	while (buf && less_eq(buf_seqno(buf), after))
		buf = buf->next;
	tipc_link_retransmit(bcl, buf, mod(to - after));
}


void tipc_bclink_acknowledge(struct tipc_node *n_ptr, u32 acked)
{
	struct sk_buff *crs;
	struct sk_buff *next;
	unsigned int released = 0;

	spin_lock_bh(&bc_lock);

	
	crs = bcl->first_out;
	if (!crs)
		goto exit;

	
	if (acked == INVALID_LINK_SEQ) {
		if (bclink->bcast_nodes.count)
			acked = bcl->fsm_msg_cnt;
		else
			acked = bcl->next_out_no;
	} else {
		if (less(acked, buf_seqno(crs)) ||
		    less(bcl->fsm_msg_cnt, acked) ||
		    less_eq(acked, n_ptr->bclink.acked))
			goto exit;
	}

	
	while (crs && less_eq(buf_seqno(crs), n_ptr->bclink.acked))
		crs = crs->next;

	

	while (crs && less_eq(buf_seqno(crs), acked)) {
		next = crs->next;

		if (crs != bcl->next_out)
			bcbuf_decr_acks(crs);
		else {
			bcbuf_set_acks(crs, 0);
			bcl->next_out = next;
			bclink_set_last_sent();
		}

		if (bcbuf_acks(crs) == 0) {
			bcl->first_out = next;
			bcl->out_queue_size--;
			kfree_skb(crs);
			released = 1;
		}
		crs = next;
	}
	n_ptr->bclink.acked = acked;

	

	if (unlikely(bcl->next_out)) {
		tipc_link_push_queue(bcl);
		bclink_set_last_sent();
	}
	if (unlikely(released && !list_empty(&bcl->waiting_ports)))
		tipc_link_wakeup_ports(bcl, 0);
exit:
	spin_unlock_bh(&bc_lock);
}


void tipc_bclink_update_link_state(struct tipc_node *n_ptr, u32 last_sent)
{
	struct sk_buff *buf;

	

	if (less_eq(last_sent, n_ptr->bclink.last_in))
		return;

	

	bclink_update_last_sent(n_ptr, last_sent);

	if (n_ptr->bclink.last_sent == n_ptr->bclink.last_in)
		return;

	

	if ((++n_ptr->bclink.oos_state) == 1) {
		if (n_ptr->bclink.deferred_size < (TIPC_MIN_LINK_WIN / 2))
			return;
		n_ptr->bclink.oos_state++;
	}

	

	if (n_ptr->bclink.oos_state & 0x1)
		return;

	

	buf = tipc_buf_acquire(INT_H_SIZE);
	if (buf) {
		struct tipc_msg *msg = buf_msg(buf);

		tipc_msg_init(msg, BCAST_PROTOCOL, STATE_MSG,
			      INT_H_SIZE, n_ptr->addr);
		msg_set_non_seq(msg, 1);
		msg_set_mc_netid(msg, tipc_net_id);
		msg_set_bcast_ack(msg, n_ptr->bclink.last_in);
		msg_set_bcgap_after(msg, n_ptr->bclink.last_in);
		msg_set_bcgap_to(msg, n_ptr->bclink.deferred_head
				 ? buf_seqno(n_ptr->bclink.deferred_head) - 1
				 : n_ptr->bclink.last_sent);

		spin_lock_bh(&bc_lock);
		tipc_bearer_send(&bcbearer->bearer, buf, NULL);
		bcl->stats.sent_nacks++;
		spin_unlock_bh(&bc_lock);
		kfree_skb(buf);

		n_ptr->bclink.oos_state++;
	}
}


static void bclink_peek_nack(struct tipc_msg *msg)
{
	struct tipc_node *n_ptr = tipc_node_find(msg_destnode(msg));

	if (unlikely(!n_ptr))
		return;

	tipc_node_lock(n_ptr);

	if (n_ptr->bclink.supported &&
	    (n_ptr->bclink.last_in != n_ptr->bclink.last_sent) &&
	    (n_ptr->bclink.last_in == msg_bcgap_after(msg)))
		n_ptr->bclink.oos_state = 2;

	tipc_node_unlock(n_ptr);
}


int tipc_bclink_send_msg(struct sk_buff *buf)
{
	int res;

	spin_lock_bh(&bc_lock);

	if (!bclink->bcast_nodes.count) {
		res = msg_data_sz(buf_msg(buf));
		kfree_skb(buf);
		goto exit;
	}

	res = tipc_link_send_buf(bcl, buf);
	if (likely(res >= 0)) {
		bclink_set_last_sent();
		bcl->stats.queue_sz_counts++;
		bcl->stats.accu_queue_sz += bcl->out_queue_size;
	}
exit:
	spin_unlock_bh(&bc_lock);
	return res;
}


static void bclink_accept_pkt(struct tipc_node *node, u32 seqno)
{
	bclink_update_last_sent(node, seqno);
	node->bclink.last_in = seqno;
	node->bclink.oos_state = 0;
	bcl->stats.recv_info++;


	if (((seqno - tipc_own_addr) % TIPC_MIN_LINK_WIN) == 0) {
		tipc_link_send_proto_msg(
			node->active_links[node->addr & 1],
			STATE_MSG, 0, 0, 0, 0, 0);
		bcl->stats.sent_acks++;
	}
}


void tipc_bclink_recv_pkt(struct sk_buff *buf)
{
	struct tipc_msg *msg = buf_msg(buf);
	struct tipc_node *node;
	u32 next_in;
	u32 seqno;
	int deferred;

	

	if (msg_mc_netid(msg) != tipc_net_id)
		goto exit;

	node = tipc_node_find(msg_prevnode(msg));
	if (unlikely(!node))
		goto exit;

	tipc_node_lock(node);
	if (unlikely(!node->bclink.supported))
		goto unlock;

	

	if (unlikely(msg_user(msg) == BCAST_PROTOCOL)) {
		if (msg_type(msg) != STATE_MSG)
			goto unlock;
		if (msg_destnode(msg) == tipc_own_addr) {
			tipc_bclink_acknowledge(node, msg_bcast_ack(msg));
			tipc_node_unlock(node);
			spin_lock_bh(&bc_lock);
			bcl->stats.recv_nacks++;
			bclink->retransmit_to = node;
			bclink_retransmit_pkt(msg_bcgap_after(msg),
					      msg_bcgap_to(msg));
			spin_unlock_bh(&bc_lock);
		} else {
			tipc_node_unlock(node);
			bclink_peek_nack(msg);
		}
		goto exit;
	}

	

	seqno = msg_seqno(msg);
	next_in = mod(node->bclink.last_in + 1);

	if (likely(seqno == next_in)) {
receive:
		

		if (likely(msg_isdata(msg))) {
			spin_lock_bh(&bc_lock);
			bclink_accept_pkt(node, seqno);
			spin_unlock_bh(&bc_lock);
			tipc_node_unlock(node);
			if (likely(msg_mcast(msg)))
				tipc_port_recv_mcast(buf, NULL);
			else
				kfree_skb(buf);
		} else if (msg_user(msg) == MSG_BUNDLER) {
			spin_lock_bh(&bc_lock);
			bclink_accept_pkt(node, seqno);
			bcl->stats.recv_bundles++;
			bcl->stats.recv_bundled += msg_msgcnt(msg);
			spin_unlock_bh(&bc_lock);
			tipc_node_unlock(node);
			tipc_link_recv_bundle(buf);
		} else if (msg_user(msg) == MSG_FRAGMENTER) {
			int ret = tipc_link_recv_fragment(&node->bclink.defragm,
						      &buf, &msg);
			if (ret < 0)
				goto unlock;
			spin_lock_bh(&bc_lock);
			bclink_accept_pkt(node, seqno);
			bcl->stats.recv_fragments++;
			if (ret > 0)
				bcl->stats.recv_fragmented++;
			spin_unlock_bh(&bc_lock);
			tipc_node_unlock(node);
			tipc_net_route_msg(buf);
		} else if (msg_user(msg) == NAME_DISTRIBUTOR) {
			spin_lock_bh(&bc_lock);
			bclink_accept_pkt(node, seqno);
			spin_unlock_bh(&bc_lock);
			tipc_node_unlock(node);
			tipc_named_recv(buf);
		} else {
			spin_lock_bh(&bc_lock);
			bclink_accept_pkt(node, seqno);
			spin_unlock_bh(&bc_lock);
			tipc_node_unlock(node);
			kfree_skb(buf);
		}
		buf = NULL;

		

		tipc_node_lock(node);
		if (unlikely(!tipc_node_is_up(node)))
			goto unlock;

		if (node->bclink.last_in == node->bclink.last_sent)
			goto unlock;

		if (!node->bclink.deferred_head) {
			node->bclink.oos_state = 1;
			goto unlock;
		}

		msg = buf_msg(node->bclink.deferred_head);
		seqno = msg_seqno(msg);
		next_in = mod(next_in + 1);
		if (seqno != next_in)
			goto unlock;

		

		buf = node->bclink.deferred_head;
		node->bclink.deferred_head = buf->next;
		node->bclink.deferred_size--;
		goto receive;
	}

	

	if (less(next_in, seqno)) {
		deferred = tipc_link_defer_pkt(&node->bclink.deferred_head,
					       &node->bclink.deferred_tail,
					       buf);
		node->bclink.deferred_size += deferred;
		bclink_update_last_sent(node, seqno);
		buf = NULL;
	} else
		deferred = 0;

	spin_lock_bh(&bc_lock);

	if (deferred)
		bcl->stats.deferred_recv++;
	else
		bcl->stats.duplicates++;

	spin_unlock_bh(&bc_lock);

unlock:
	tipc_node_unlock(node);
exit:
	kfree_skb(buf);
}

u32 tipc_bclink_acks_missing(struct tipc_node *n_ptr)
{
	return (n_ptr->bclink.supported &&
		(tipc_bclink_get_last_sent() != n_ptr->bclink.acked));
}



static int tipc_bcbearer_send(struct sk_buff *buf,
			      struct tipc_bearer *unused1,
			      struct tipc_media_addr *unused2)
{
	int bp_index;


	if (likely(!msg_non_seq(buf_msg(buf)))) {
		struct tipc_msg *msg;

		bcbuf_set_acks(buf, bclink->bcast_nodes.count);
		msg = buf_msg(buf);
		msg_set_non_seq(msg, 1);
		msg_set_mc_netid(msg, tipc_net_id);
		bcl->stats.sent_info++;

		if (WARN_ON(!bclink->bcast_nodes.count)) {
			dump_stack();
			return 0;
		}
	}

	

	bcbearer->remains = bclink->bcast_nodes;

	for (bp_index = 0; bp_index < MAX_BEARERS; bp_index++) {
		struct tipc_bearer *p = bcbearer->bpairs[bp_index].primary;
		struct tipc_bearer *s = bcbearer->bpairs[bp_index].secondary;

		if (!p)
			break;	

		tipc_nmap_diff(&bcbearer->remains, &p->nodes, &bcbearer->remains_new);
		if (bcbearer->remains_new.count == bcbearer->remains.count)
			continue;	

		if (p->blocked ||
		    p->media->send_msg(buf, p, &p->media->bcast_addr)) {
			
			if (!s || s->blocked ||
			    s->media->send_msg(buf, s,
					       &s->media->bcast_addr)) {
				
				continue;
			}
		}

		if (s) {
			bcbearer->bpairs[bp_index].primary = s;
			bcbearer->bpairs[bp_index].secondary = p;
		}

		if (bcbearer->remains_new.count == 0)
			break;	

		bcbearer->remains = bcbearer->remains_new;
	}

	return 0;
}


void tipc_bcbearer_sort(void)
{
	struct tipc_bcbearer_pair *bp_temp = bcbearer->bpairs_temp;
	struct tipc_bcbearer_pair *bp_curr;
	int b_index;
	int pri;

	spin_lock_bh(&bc_lock);

	

	memset(bp_temp, 0, sizeof(bcbearer->bpairs_temp));

	for (b_index = 0; b_index < MAX_BEARERS; b_index++) {
		struct tipc_bearer *b = &tipc_bearers[b_index];

		if (!b->active || !b->nodes.count)
			continue;

		if (!bp_temp[b->priority].primary)
			bp_temp[b->priority].primary = b;
		else
			bp_temp[b->priority].secondary = b;
	}

	

	bp_curr = bcbearer->bpairs;
	memset(bcbearer->bpairs, 0, sizeof(bcbearer->bpairs));

	for (pri = TIPC_MAX_LINK_PRI; pri >= 0; pri--) {

		if (!bp_temp[pri].primary)
			continue;

		bp_curr->primary = bp_temp[pri].primary;

		if (bp_temp[pri].secondary) {
			if (tipc_nmap_equal(&bp_temp[pri].primary->nodes,
					    &bp_temp[pri].secondary->nodes)) {
				bp_curr->secondary = bp_temp[pri].secondary;
			} else {
				bp_curr++;
				bp_curr->primary = bp_temp[pri].secondary;
			}
		}

		bp_curr++;
	}

	spin_unlock_bh(&bc_lock);
}


int tipc_bclink_stats(char *buf, const u32 buf_size)
{
	struct print_buf pb;

	if (!bcl)
		return 0;

	tipc_printbuf_init(&pb, buf, buf_size);

	spin_lock_bh(&bc_lock);

	tipc_printf(&pb, "Link <%s>\n"
			 "  Window:%u packets\n",
		    bcl->name, bcl->queue_limit[0]);
	tipc_printf(&pb, "  RX packets:%u fragments:%u/%u bundles:%u/%u\n",
		    bcl->stats.recv_info,
		    bcl->stats.recv_fragments,
		    bcl->stats.recv_fragmented,
		    bcl->stats.recv_bundles,
		    bcl->stats.recv_bundled);
	tipc_printf(&pb, "  TX packets:%u fragments:%u/%u bundles:%u/%u\n",
		    bcl->stats.sent_info,
		    bcl->stats.sent_fragments,
		    bcl->stats.sent_fragmented,
		    bcl->stats.sent_bundles,
		    bcl->stats.sent_bundled);
	tipc_printf(&pb, "  RX naks:%u defs:%u dups:%u\n",
		    bcl->stats.recv_nacks,
		    bcl->stats.deferred_recv,
		    bcl->stats.duplicates);
	tipc_printf(&pb, "  TX naks:%u acks:%u dups:%u\n",
		    bcl->stats.sent_nacks,
		    bcl->stats.sent_acks,
		    bcl->stats.retransmitted);
	tipc_printf(&pb, "  Congestion bearer:%u link:%u  Send queue max:%u avg:%u\n",
		    bcl->stats.bearer_congs,
		    bcl->stats.link_congs,
		    bcl->stats.max_queue_sz,
		    bcl->stats.queue_sz_counts
		    ? (bcl->stats.accu_queue_sz / bcl->stats.queue_sz_counts)
		    : 0);

	spin_unlock_bh(&bc_lock);
	return tipc_printbuf_validate(&pb);
}

int tipc_bclink_reset_stats(void)
{
	if (!bcl)
		return -ENOPROTOOPT;

	spin_lock_bh(&bc_lock);
	memset(&bcl->stats, 0, sizeof(bcl->stats));
	spin_unlock_bh(&bc_lock);
	return 0;
}

int tipc_bclink_set_queue_limits(u32 limit)
{
	if (!bcl)
		return -ENOPROTOOPT;
	if ((limit < TIPC_MIN_LINK_WIN) || (limit > TIPC_MAX_LINK_WIN))
		return -EINVAL;

	spin_lock_bh(&bc_lock);
	tipc_link_set_queue_limits(bcl, limit);
	spin_unlock_bh(&bc_lock);
	return 0;
}

void tipc_bclink_init(void)
{
	INIT_LIST_HEAD(&bcbearer->bearer.cong_links);
	bcbearer->bearer.media = &bcbearer->media;
	bcbearer->media.send_msg = tipc_bcbearer_send;
	sprintf(bcbearer->media.name, "tipc-broadcast");

	INIT_LIST_HEAD(&bcl->waiting_ports);
	bcl->next_out_no = 1;
	spin_lock_init(&bclink->node.lock);
	bcl->owner = &bclink->node;
	bcl->max_pkt = MAX_PKT_DEFAULT_MCAST;
	tipc_link_set_queue_limits(bcl, BCLINK_WIN_DEFAULT);
	bcl->b_ptr = &bcbearer->bearer;
	bcl->state = WORKING_WORKING;
	strlcpy(bcl->name, tipc_bclink_name, TIPC_MAX_LINK_NAME);
}

void tipc_bclink_stop(void)
{
	spin_lock_bh(&bc_lock);
	tipc_link_stop(bcl);
	spin_unlock_bh(&bc_lock);

	memset(bclink, 0, sizeof(*bclink));
	memset(bcbearer, 0, sizeof(*bcbearer));
}



void tipc_nmap_add(struct tipc_node_map *nm_ptr, u32 node)
{
	int n = tipc_node(node);
	int w = n / WSIZE;
	u32 mask = (1 << (n % WSIZE));

	if ((nm_ptr->map[w] & mask) == 0) {
		nm_ptr->count++;
		nm_ptr->map[w] |= mask;
	}
}


void tipc_nmap_remove(struct tipc_node_map *nm_ptr, u32 node)
{
	int n = tipc_node(node);
	int w = n / WSIZE;
	u32 mask = (1 << (n % WSIZE));

	if ((nm_ptr->map[w] & mask) != 0) {
		nm_ptr->map[w] &= ~mask;
		nm_ptr->count--;
	}
}


static void tipc_nmap_diff(struct tipc_node_map *nm_a,
			   struct tipc_node_map *nm_b,
			   struct tipc_node_map *nm_diff)
{
	int stop = ARRAY_SIZE(nm_a->map);
	int w;
	int b;
	u32 map;

	memset(nm_diff, 0, sizeof(*nm_diff));
	for (w = 0; w < stop; w++) {
		map = nm_a->map[w] ^ (nm_a->map[w] & nm_b->map[w]);
		nm_diff->map[w] = map;
		if (map != 0) {
			for (b = 0 ; b < WSIZE; b++) {
				if (map & (1 << b))
					nm_diff->count++;
			}
		}
	}
}


void tipc_port_list_add(struct tipc_port_list *pl_ptr, u32 port)
{
	struct tipc_port_list *item = pl_ptr;
	int i;
	int item_sz = PLSIZE;
	int cnt = pl_ptr->count;

	for (; ; cnt -= item_sz, item = item->next) {
		if (cnt < PLSIZE)
			item_sz = cnt;
		for (i = 0; i < item_sz; i++)
			if (item->ports[i] == port)
				return;
		if (i < PLSIZE) {
			item->ports[i] = port;
			pl_ptr->count++;
			return;
		}
		if (!item->next) {
			item->next = kmalloc(sizeof(*item), GFP_ATOMIC);
			if (!item->next) {
				warn("Incomplete multicast delivery, no memory\n");
				return;
			}
			item->next->next = NULL;
		}
	}
}


void tipc_port_list_free(struct tipc_port_list *pl_ptr)
{
	struct tipc_port_list *item;
	struct tipc_port_list *next;

	for (item = pl_ptr->next; item; item = next) {
		next = item->next;
		kfree(item);
	}
}

