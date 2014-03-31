/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 La Monte H.P. Yarroll
 *
 * This file is part of the SCTP kernel implementation
 *
 * This module provides the abstraction for an SCTP association.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Xingang Guo           <xingang.guo@intel.com>
 *    Hui Huang             <hui.huang@nokia.com>
 *    Sridhar Samudrala	    <sri@us.ibm.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *    Ryan Layer	    <rmlayer@us.ibm.com>
 *    Kevin Gao             <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/in.h>
#include <net/ipv6.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

static void sctp_assoc_bh_rcv(struct work_struct *work);
static void sctp_assoc_free_asconf_acks(struct sctp_association *asoc);
static void sctp_assoc_free_asconf_queue(struct sctp_association *asoc);

static u32 idr_low = 1;



static struct sctp_association *sctp_association_init(struct sctp_association *asoc,
					  const struct sctp_endpoint *ep,
					  const struct sock *sk,
					  sctp_scope_t scope,
					  gfp_t gfp)
{
	struct sctp_sock *sp;
	int i;
	sctp_paramhdr_t *p;
	int err;

	
	sp = sctp_sk((struct sock *)sk);

	
	asoc->ep = (struct sctp_endpoint *)ep;
	sctp_endpoint_hold(asoc->ep);

	
	asoc->base.sk = (struct sock *)sk;
	sock_hold(asoc->base.sk);

	
	asoc->base.type = SCTP_EP_TYPE_ASSOCIATION;

	
	atomic_set(&asoc->base.refcnt, 1);
	asoc->base.dead = 0;
	asoc->base.malloced = 0;

	
	sctp_bind_addr_init(&asoc->base.bind_addr, ep->base.bind_addr.port);

	asoc->state = SCTP_STATE_CLOSED;

	asoc->cookie_life.tv_sec = sp->assocparams.sasoc_cookie_life / 1000;
	asoc->cookie_life.tv_usec = (sp->assocparams.sasoc_cookie_life % 1000)
					* 1000;
	asoc->frag_point = 0;
	asoc->user_frag = sp->user_frag;

	asoc->max_retrans = sp->assocparams.sasoc_asocmaxrxt;
	asoc->rto_initial = msecs_to_jiffies(sp->rtoinfo.srto_initial);
	asoc->rto_max = msecs_to_jiffies(sp->rtoinfo.srto_max);
	asoc->rto_min = msecs_to_jiffies(sp->rtoinfo.srto_min);

	asoc->overall_error_count = 0;

	asoc->hbinterval = msecs_to_jiffies(sp->hbinterval);

	
	asoc->pathmaxrxt = sp->pathmaxrxt;

	
	asoc->pathmtu = sp->pathmtu;

	
	asoc->sackdelay = msecs_to_jiffies(sp->sackdelay);
	asoc->sackfreq = sp->sackfreq;

	asoc->param_flags = sp->param_flags;

	asoc->max_burst = sp->max_burst;

	
	asoc->timeouts[SCTP_EVENT_TIMEOUT_NONE] = 0;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T1_COOKIE] = asoc->rto_initial;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T1_INIT] = asoc->rto_initial;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T2_SHUTDOWN] = asoc->rto_initial;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T3_RTX] = 0;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T4_RTO] = 0;

	asoc->timeouts[SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD]
		= 5 * asoc->rto_max;

	asoc->timeouts[SCTP_EVENT_TIMEOUT_HEARTBEAT] = 0;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_SACK] = asoc->sackdelay;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_AUTOCLOSE] =
		min_t(unsigned long, sp->autoclose, sctp_max_autoclose) * HZ;

	
	for (i = SCTP_EVENT_TIMEOUT_NONE; i < SCTP_NUM_TIMEOUT_TYPES; ++i)
		setup_timer(&asoc->timers[i], sctp_timer_events[i],
				(unsigned long)asoc);

	asoc->c.sinit_max_instreams = sp->initmsg.sinit_max_instreams;
	asoc->c.sinit_num_ostreams  = sp->initmsg.sinit_num_ostreams;
	asoc->max_init_attempts	= sp->initmsg.sinit_max_attempts;

	asoc->max_init_timeo =
		 msecs_to_jiffies(sp->initmsg.sinit_max_init_timeo);

	asoc->ssnmap = NULL;

	if ((sk->sk_rcvbuf/2) < SCTP_DEFAULT_MINWINDOW)
		asoc->rwnd = SCTP_DEFAULT_MINWINDOW;
	else
		asoc->rwnd = sk->sk_rcvbuf/2;

	asoc->a_rwnd = asoc->rwnd;

	asoc->rwnd_over = 0;
	asoc->rwnd_press = 0;

	
	asoc->peer.rwnd = SCTP_DEFAULT_MAXWINDOW;

	
	asoc->sndbuf_used = 0;

	
	atomic_set(&asoc->rmem_alloc, 0);

	init_waitqueue_head(&asoc->wait);

	asoc->c.my_vtag = sctp_generate_tag(ep);
	asoc->peer.i.init_tag = 0;     
	asoc->c.peer_vtag = 0;
	asoc->c.my_ttag   = 0;
	asoc->c.peer_ttag = 0;
	asoc->c.my_port = ep->base.bind_addr.port;

	asoc->c.initial_tsn = sctp_generate_tsn(ep);

	asoc->next_tsn = asoc->c.initial_tsn;

	asoc->ctsn_ack_point = asoc->next_tsn - 1;
	asoc->adv_peer_ack_point = asoc->ctsn_ack_point;
	asoc->highest_sacked = asoc->ctsn_ack_point;
	asoc->last_cwr_tsn = asoc->ctsn_ack_point;
	asoc->unack_data = 0;

	asoc->addip_serial = asoc->c.initial_tsn;

	INIT_LIST_HEAD(&asoc->addip_chunk_list);
	INIT_LIST_HEAD(&asoc->asconf_ack_list);

	
	INIT_LIST_HEAD(&asoc->peer.transport_addr_list);
	asoc->peer.transport_count = 0;

	asoc->peer.sack_needed = 1;
	asoc->peer.sack_cnt = 0;

	asoc->peer.asconf_capable = 0;
	if (sctp_addip_noauth)
		asoc->peer.asconf_capable = 1;
	asoc->asconf_addr_del_pending = NULL;
	asoc->src_out_of_asoc_ok = 0;
	asoc->new_transport = NULL;

	
	sctp_inq_init(&asoc->base.inqueue);
	sctp_inq_set_th_handler(&asoc->base.inqueue, sctp_assoc_bh_rcv);

	
	sctp_outq_init(asoc, &asoc->outqueue);

	if (!sctp_ulpq_init(&asoc->ulpq, asoc))
		goto fail_init;

	memset(&asoc->peer.tsn_map, 0, sizeof(struct sctp_tsnmap));

	asoc->need_ecne = 0;

	asoc->assoc_id = 0;

	asoc->peer.ipv4_address = 1;
	if (asoc->base.sk->sk_family == PF_INET6)
		asoc->peer.ipv6_address = 1;
	INIT_LIST_HEAD(&asoc->asocs);

	asoc->autoclose = sp->autoclose;

	asoc->default_stream = sp->default_stream;
	asoc->default_ppid = sp->default_ppid;
	asoc->default_flags = sp->default_flags;
	asoc->default_context = sp->default_context;
	asoc->default_timetolive = sp->default_timetolive;
	asoc->default_rcv_context = sp->default_rcv_context;

	
	INIT_LIST_HEAD(&asoc->endpoint_shared_keys);
	err = sctp_auth_asoc_copy_shkeys(ep, asoc, gfp);
	if (err)
		goto fail_init;

	asoc->active_key_id = ep->active_key_id;
	asoc->asoc_shared_key = NULL;

	asoc->default_hmac_id = 0;
	
	if (ep->auth_hmacs_list)
		memcpy(asoc->c.auth_hmacs, ep->auth_hmacs_list,
			ntohs(ep->auth_hmacs_list->param_hdr.length));
	if (ep->auth_chunk_list)
		memcpy(asoc->c.auth_chunks, ep->auth_chunk_list,
			ntohs(ep->auth_chunk_list->param_hdr.length));

	
	p = (sctp_paramhdr_t *)asoc->c.auth_random;
	p->type = SCTP_PARAM_RANDOM;
	p->length = htons(sizeof(sctp_paramhdr_t) + SCTP_AUTH_RANDOM_LENGTH);
	get_random_bytes(p+1, SCTP_AUTH_RANDOM_LENGTH);

	return asoc;

fail_init:
	sctp_endpoint_put(asoc->ep);
	sock_put(asoc->base.sk);
	return NULL;
}

struct sctp_association *sctp_association_new(const struct sctp_endpoint *ep,
					 const struct sock *sk,
					 sctp_scope_t scope,
					 gfp_t gfp)
{
	struct sctp_association *asoc;

	asoc = t_new(struct sctp_association, gfp);
	if (!asoc)
		goto fail;

	if (!sctp_association_init(asoc, ep, sk, scope, gfp))
		goto fail_init;

	asoc->base.malloced = 1;
	SCTP_DBG_OBJCNT_INC(assoc);
	SCTP_DEBUG_PRINTK("Created asoc %p\n", asoc);

	return asoc;

fail_init:
	kfree(asoc);
fail:
	return NULL;
}

void sctp_association_free(struct sctp_association *asoc)
{
	struct sock *sk = asoc->base.sk;
	struct sctp_transport *transport;
	struct list_head *pos, *temp;
	int i;

	if (!asoc->temp) {
		list_del(&asoc->asocs);

		if (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING))
			sk->sk_ack_backlog--;
	}

	asoc->base.dead = 1;

	
	sctp_outq_free(&asoc->outqueue);

	
	sctp_ulpq_free(&asoc->ulpq);

	
	sctp_inq_free(&asoc->base.inqueue);

	sctp_tsnmap_free(&asoc->peer.tsn_map);

	
	sctp_ssnmap_free(asoc->ssnmap);

	
	sctp_bind_addr_free(&asoc->base.bind_addr);

	for (i = SCTP_EVENT_TIMEOUT_NONE; i < SCTP_NUM_TIMEOUT_TYPES; ++i) {
		if (timer_pending(&asoc->timers[i]) &&
		    del_timer(&asoc->timers[i]))
			sctp_association_put(asoc);
	}

	
	kfree(asoc->peer.cookie);
	kfree(asoc->peer.peer_random);
	kfree(asoc->peer.peer_chunks);
	kfree(asoc->peer.peer_hmacs);

	
	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		transport = list_entry(pos, struct sctp_transport, transports);
		list_del(pos);
		sctp_transport_free(transport);
	}

	asoc->peer.transport_count = 0;

	sctp_asconf_queue_teardown(asoc);

	
	if (asoc->asconf_addr_del_pending != NULL)
		kfree(asoc->asconf_addr_del_pending);

	
	sctp_auth_destroy_keys(&asoc->endpoint_shared_keys);

	
	sctp_auth_key_put(asoc->asoc_shared_key);

	sctp_association_put(asoc);
}

static void sctp_association_destroy(struct sctp_association *asoc)
{
	SCTP_ASSERT(asoc->base.dead, "Assoc is not dead", return);

	sctp_endpoint_put(asoc->ep);
	sock_put(asoc->base.sk);

	if (asoc->assoc_id != 0) {
		spin_lock_bh(&sctp_assocs_id_lock);
		idr_remove(&sctp_assocs_id, asoc->assoc_id);
		spin_unlock_bh(&sctp_assocs_id_lock);
	}

	WARN_ON(atomic_read(&asoc->rmem_alloc));

	if (asoc->base.malloced) {
		kfree(asoc);
		SCTP_DBG_OBJCNT_DEC(assoc);
	}
}

void sctp_assoc_set_primary(struct sctp_association *asoc,
			    struct sctp_transport *transport)
{
	int changeover = 0;

	if (asoc->peer.primary_path != NULL &&
	    asoc->peer.primary_path != transport)
		changeover = 1 ;

	asoc->peer.primary_path = transport;

	
	memcpy(&asoc->peer.primary_addr, &transport->ipaddr,
	       sizeof(union sctp_addr));

	if ((transport->state == SCTP_ACTIVE) ||
	    (transport->state == SCTP_UNKNOWN))
		asoc->peer.active_path = transport;

	if (!asoc->outqueue.outstanding_bytes && !asoc->outqueue.out_qlen)
		return;

	if (transport->cacc.changeover_active)
		transport->cacc.cycling_changeover = changeover;

	transport->cacc.changeover_active = changeover;

	transport->cacc.next_tsn_at_change = asoc->next_tsn;
}

void sctp_assoc_rm_peer(struct sctp_association *asoc,
			struct sctp_transport *peer)
{
	struct list_head	*pos;
	struct sctp_transport	*transport;

	SCTP_DEBUG_PRINTK_IPADDR("sctp_assoc_rm_peer:association %p addr: ",
				 " port: %d\n",
				 asoc,
				 (&peer->ipaddr),
				 ntohs(peer->ipaddr.v4.sin_port));

	if (asoc->peer.retran_path == peer)
		sctp_assoc_update_retran_path(asoc);

	
	list_del(&peer->transports);

	
	pos = asoc->peer.transport_addr_list.next;
	transport = list_entry(pos, struct sctp_transport, transports);

	
	if (asoc->peer.primary_path == peer)
		sctp_assoc_set_primary(asoc, transport);
	if (asoc->peer.active_path == peer)
		asoc->peer.active_path = transport;
	if (asoc->peer.retran_path == peer)
		asoc->peer.retran_path = transport;
	if (asoc->peer.last_data_from == peer)
		asoc->peer.last_data_from = transport;

	if (asoc->init_last_sent_to == peer)
		asoc->init_last_sent_to = NULL;

	if (asoc->shutdown_last_sent_to == peer)
		asoc->shutdown_last_sent_to = NULL;

	if (asoc->addip_last_asconf &&
	    asoc->addip_last_asconf->transport == peer)
		asoc->addip_last_asconf->transport = NULL;

	if (!list_empty(&peer->transmitted)) {
		struct sctp_transport *active = asoc->peer.active_path;
		struct sctp_chunk *ch;

		
		list_for_each_entry(ch, &peer->transmitted,
					transmitted_list) {
			ch->transport = NULL;
			ch->rtt_in_progress = 0;
		}

		list_splice_tail_init(&peer->transmitted,
					&active->transmitted);

		if (!timer_pending(&active->T3_rtx_timer))
			if (!mod_timer(&active->T3_rtx_timer,
					jiffies + active->rto))
				sctp_transport_hold(active);
	}

	asoc->peer.transport_count--;

	sctp_transport_free(peer);
}

struct sctp_transport *sctp_assoc_add_peer(struct sctp_association *asoc,
					   const union sctp_addr *addr,
					   const gfp_t gfp,
					   const int peer_state)
{
	struct sctp_transport *peer;
	struct sctp_sock *sp;
	unsigned short port;

	sp = sctp_sk(asoc->base.sk);

	
	port = ntohs(addr->v4.sin_port);

	SCTP_DEBUG_PRINTK_IPADDR("sctp_assoc_add_peer:association %p addr: ",
				 " port: %d state:%d\n",
				 asoc,
				 addr,
				 port,
				 peer_state);

	
	if (0 == asoc->peer.port)
		asoc->peer.port = port;

	
	peer = sctp_assoc_lookup_paddr(asoc, addr);
	if (peer) {
		if (peer->state == SCTP_UNKNOWN) {
			peer->state = SCTP_ACTIVE;
		}
		return peer;
	}

	peer = sctp_transport_new(addr, gfp);
	if (!peer)
		return NULL;

	sctp_transport_set_owner(peer, asoc);

	peer->hbinterval = asoc->hbinterval;

	
	peer->pathmaxrxt = asoc->pathmaxrxt;

	peer->sackdelay = asoc->sackdelay;
	peer->sackfreq = asoc->sackfreq;

	peer->param_flags = asoc->param_flags;

	sctp_transport_route(peer, NULL, sp);

	
	if (peer->param_flags & SPP_PMTUD_DISABLE) {
		if (asoc->pathmtu)
			peer->pathmtu = asoc->pathmtu;
		else
			peer->pathmtu = SCTP_DEFAULT_MAXSEGMENT;
	}

	if (asoc->pathmtu)
		asoc->pathmtu = min_t(int, peer->pathmtu, asoc->pathmtu);
	else
		asoc->pathmtu = peer->pathmtu;

	SCTP_DEBUG_PRINTK("sctp_assoc_add_peer:association %p PMTU set to "
			  "%d\n", asoc, asoc->pathmtu);
	peer->pmtu_pending = 0;

	asoc->frag_point = sctp_frag_point(asoc, asoc->pathmtu);

	sctp_packet_init(&peer->packet, peer, asoc->base.bind_addr.port,
			 asoc->peer.port);

	peer->cwnd = min(4*asoc->pathmtu, max_t(__u32, 2*asoc->pathmtu, 4380));

	peer->ssthresh = SCTP_DEFAULT_MAXWINDOW;

	peer->partial_bytes_acked = 0;
	peer->flight_size = 0;
	peer->burst_limited = 0;

	
	peer->rto = asoc->rto_initial;

	
	peer->state = peer_state;

	
	list_add_tail(&peer->transports, &asoc->peer.transport_addr_list);
	asoc->peer.transport_count++;

	
	if (!asoc->peer.primary_path) {
		sctp_assoc_set_primary(asoc, peer);
		asoc->peer.retran_path = peer;
	}

	if (asoc->peer.active_path == asoc->peer.retran_path &&
	    peer->state != SCTP_UNCONFIRMED) {
		asoc->peer.retran_path = peer;
	}

	return peer;
}

void sctp_assoc_del_peer(struct sctp_association *asoc,
			 const union sctp_addr *addr)
{
	struct list_head	*pos;
	struct list_head	*temp;
	struct sctp_transport	*transport;

	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		transport = list_entry(pos, struct sctp_transport, transports);
		if (sctp_cmp_addr_exact(addr, &transport->ipaddr)) {
			
			sctp_assoc_rm_peer(asoc, transport);
			break;
		}
	}
}

struct sctp_transport *sctp_assoc_lookup_paddr(
					const struct sctp_association *asoc,
					const union sctp_addr *address)
{
	struct sctp_transport *t;

	

	list_for_each_entry(t, &asoc->peer.transport_addr_list,
			transports) {
		if (sctp_cmp_addr_exact(address, &t->ipaddr))
			return t;
	}

	return NULL;
}

void sctp_assoc_del_nonprimary_peers(struct sctp_association *asoc,
				     struct sctp_transport *primary)
{
	struct sctp_transport	*temp;
	struct sctp_transport	*t;

	list_for_each_entry_safe(t, temp, &asoc->peer.transport_addr_list,
				 transports) {
		
		if (t != primary)
			sctp_assoc_rm_peer(asoc, t);
	}
}

void sctp_assoc_control_transport(struct sctp_association *asoc,
				  struct sctp_transport *transport,
				  sctp_transport_cmd_t command,
				  sctp_sn_error_t error)
{
	struct sctp_transport *t = NULL;
	struct sctp_transport *first;
	struct sctp_transport *second;
	struct sctp_ulpevent *event;
	struct sockaddr_storage addr;
	int spc_state = 0;

	
	switch (command) {
	case SCTP_TRANSPORT_UP:
		if (SCTP_UNCONFIRMED == transport->state &&
		    SCTP_HEARTBEAT_SUCCESS == error)
			spc_state = SCTP_ADDR_CONFIRMED;
		else
			spc_state = SCTP_ADDR_AVAILABLE;
		transport->state = SCTP_ACTIVE;
		break;

	case SCTP_TRANSPORT_DOWN:
		if (transport->state != SCTP_UNCONFIRMED)
			transport->state = SCTP_INACTIVE;
		else {
			dst_release(transport->dst);
			transport->dst = NULL;
		}

		spc_state = SCTP_ADDR_UNREACHABLE;
		break;

	default:
		return;
	}

	memset(&addr, 0, sizeof(struct sockaddr_storage));
	memcpy(&addr, &transport->ipaddr, transport->af_specific->sockaddr_len);
	event = sctp_ulpevent_make_peer_addr_change(asoc, &addr,
				0, spc_state, error, GFP_ATOMIC);
	if (event)
		sctp_ulpq_tail_event(&asoc->ulpq, event);

	

	first = NULL; second = NULL;

	list_for_each_entry(t, &asoc->peer.transport_addr_list,
			transports) {

		if ((t->state == SCTP_INACTIVE) ||
		    (t->state == SCTP_UNCONFIRMED))
			continue;
		if (!first || t->last_time_heard > first->last_time_heard) {
			second = first;
			first = t;
		}
		if (!second || t->last_time_heard > second->last_time_heard)
			second = t;
	}

	if (((asoc->peer.primary_path->state == SCTP_ACTIVE) ||
	     (asoc->peer.primary_path->state == SCTP_UNKNOWN)) &&
	    first != asoc->peer.primary_path) {
		second = first;
		first = asoc->peer.primary_path;
	}

	if (!first) {
		first = asoc->peer.primary_path;
		second = asoc->peer.primary_path;
	}

	
	asoc->peer.active_path = first;
	asoc->peer.retran_path = second;
}

void sctp_association_hold(struct sctp_association *asoc)
{
	atomic_inc(&asoc->base.refcnt);
}

void sctp_association_put(struct sctp_association *asoc)
{
	if (atomic_dec_and_test(&asoc->base.refcnt))
		sctp_association_destroy(asoc);
}

__u32 sctp_association_get_next_tsn(struct sctp_association *asoc)
{
	__u32 retval = asoc->next_tsn;
	asoc->next_tsn++;
	asoc->unack_data++;

	return retval;
}

int sctp_cmp_addr_exact(const union sctp_addr *ss1,
			const union sctp_addr *ss2)
{
	struct sctp_af *af;

	af = sctp_get_af_specific(ss1->sa.sa_family);
	if (unlikely(!af))
		return 0;

	return af->cmp_addr(ss1, ss2);
}

struct sctp_chunk *sctp_get_ecne_prepend(struct sctp_association *asoc)
{
	struct sctp_chunk *chunk;

	if (asoc->need_ecne)
		chunk = sctp_make_ecne(asoc, asoc->last_ecne_tsn);
	else
		chunk = NULL;

	return chunk;
}

struct sctp_transport *sctp_assoc_lookup_tsn(struct sctp_association *asoc,
					     __u32 tsn)
{
	struct sctp_transport *active;
	struct sctp_transport *match;
	struct sctp_transport *transport;
	struct sctp_chunk *chunk;
	__be32 key = htonl(tsn);

	match = NULL;



	active = asoc->peer.active_path;

	list_for_each_entry(chunk, &active->transmitted,
			transmitted_list) {

		if (key == chunk->subh.data_hdr->tsn) {
			match = active;
			goto out;
		}
	}

	
	list_for_each_entry(transport, &asoc->peer.transport_addr_list,
			transports) {

		if (transport == active)
			break;
		list_for_each_entry(chunk, &transport->transmitted,
				transmitted_list) {
			if (key == chunk->subh.data_hdr->tsn) {
				match = transport;
				goto out;
			}
		}
	}
out:
	return match;
}

struct sctp_transport *sctp_assoc_is_match(struct sctp_association *asoc,
					   const union sctp_addr *laddr,
					   const union sctp_addr *paddr)
{
	struct sctp_transport *transport;

	if ((htons(asoc->base.bind_addr.port) == laddr->v4.sin_port) &&
	    (htons(asoc->peer.port) == paddr->v4.sin_port)) {
		transport = sctp_assoc_lookup_paddr(asoc, paddr);
		if (!transport)
			goto out;

		if (sctp_bind_addr_match(&asoc->base.bind_addr, laddr,
					 sctp_sk(asoc->base.sk)))
			goto out;
	}
	transport = NULL;

out:
	return transport;
}

static void sctp_assoc_bh_rcv(struct work_struct *work)
{
	struct sctp_association *asoc =
		container_of(work, struct sctp_association,
			     base.inqueue.immediate);
	struct sctp_endpoint *ep;
	struct sctp_chunk *chunk;
	struct sctp_inq *inqueue;
	int state;
	sctp_subtype_t subtype;
	int error = 0;

	
	ep = asoc->ep;

	inqueue = &asoc->base.inqueue;
	sctp_association_hold(asoc);
	while (NULL != (chunk = sctp_inq_pop(inqueue))) {
		state = asoc->state;
		subtype = SCTP_ST_CHUNK(chunk->chunk_hdr->type);

		if (sctp_auth_recv_cid(subtype.chunk, asoc) && !chunk->auth)
			continue;

		if (sctp_chunk_is_data(chunk))
			asoc->peer.last_data_from = chunk->transport;
		else
			SCTP_INC_STATS(SCTP_MIB_INCTRLCHUNKS);

		if (chunk->transport)
			chunk->transport->last_time_heard = jiffies;

		
		error = sctp_do_sm(SCTP_EVENT_T_CHUNK, subtype,
				   state, ep, asoc, chunk, GFP_ATOMIC);

		if (asoc->base.dead)
			break;

		
		if (error && chunk)
			chunk->pdiscard = 1;
	}
	sctp_association_put(asoc);
}

void sctp_assoc_migrate(struct sctp_association *assoc, struct sock *newsk)
{
	struct sctp_sock *newsp = sctp_sk(newsk);
	struct sock *oldsk = assoc->base.sk;

	list_del_init(&assoc->asocs);

	
	if (sctp_style(oldsk, TCP))
		oldsk->sk_ack_backlog--;

	
	sctp_endpoint_put(assoc->ep);
	sock_put(assoc->base.sk);

	
	assoc->ep = newsp->ep;
	sctp_endpoint_hold(assoc->ep);

	
	assoc->base.sk = newsk;
	sock_hold(assoc->base.sk);

	
	sctp_endpoint_add_asoc(newsp->ep, assoc);
}

void sctp_assoc_update(struct sctp_association *asoc,
		       struct sctp_association *new)
{
	struct sctp_transport *trans;
	struct list_head *pos, *temp;

	
	asoc->c = new->c;
	asoc->peer.rwnd = new->peer.rwnd;
	asoc->peer.sack_needed = new->peer.sack_needed;
	asoc->peer.i = new->peer.i;
	sctp_tsnmap_init(&asoc->peer.tsn_map, SCTP_TSN_MAP_INITIAL,
			 asoc->peer.i.initial_tsn, GFP_ATOMIC);

	
	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		trans = list_entry(pos, struct sctp_transport, transports);
		if (!sctp_assoc_lookup_paddr(new, &trans->ipaddr)) {
			sctp_assoc_rm_peer(asoc, trans);
			continue;
		}

		if (asoc->state >= SCTP_STATE_ESTABLISHED)
			sctp_transport_reset(trans);
	}

	if (asoc->state >= SCTP_STATE_ESTABLISHED) {
		asoc->next_tsn = new->next_tsn;
		asoc->ctsn_ack_point = new->ctsn_ack_point;
		asoc->adv_peer_ack_point = new->adv_peer_ack_point;

		sctp_ssnmap_clear(asoc->ssnmap);

		sctp_ulpq_flush(&asoc->ulpq);

		asoc->overall_error_count = 0;

	} else {
		
		list_for_each_entry(trans, &new->peer.transport_addr_list,
				transports) {
			if (!sctp_assoc_lookup_paddr(asoc, &trans->ipaddr))
				sctp_assoc_add_peer(asoc, &trans->ipaddr,
						    GFP_ATOMIC, trans->state);
		}

		asoc->ctsn_ack_point = asoc->next_tsn - 1;
		asoc->adv_peer_ack_point = asoc->ctsn_ack_point;
		if (!asoc->ssnmap) {
			
			asoc->ssnmap = new->ssnmap;
			new->ssnmap = NULL;
		}

		if (!asoc->assoc_id) {
			sctp_assoc_set_id(asoc, GFP_ATOMIC);
		}
	}

	kfree(asoc->peer.peer_random);
	asoc->peer.peer_random = new->peer.peer_random;
	new->peer.peer_random = NULL;

	kfree(asoc->peer.peer_chunks);
	asoc->peer.peer_chunks = new->peer.peer_chunks;
	new->peer.peer_chunks = NULL;

	kfree(asoc->peer.peer_hmacs);
	asoc->peer.peer_hmacs = new->peer.peer_hmacs;
	new->peer.peer_hmacs = NULL;

	sctp_auth_key_put(asoc->asoc_shared_key);
	sctp_auth_asoc_init_active_key(asoc, GFP_ATOMIC);
}

void sctp_assoc_update_retran_path(struct sctp_association *asoc)
{
	struct sctp_transport *t, *next;
	struct list_head *head = &asoc->peer.transport_addr_list;
	struct list_head *pos;

	if (asoc->peer.transport_count == 1)
		return;

	
	t = asoc->peer.retran_path;
	pos = &t->transports;
	next = NULL;

	while (1) {
		
		if (pos->next == head)
			pos = head->next;
		else
			pos = pos->next;

		t = list_entry(pos, struct sctp_transport, transports);

		if (t == asoc->peer.retran_path) {
			t = next;
			break;
		}

		

		if ((t->state == SCTP_ACTIVE) ||
		    (t->state == SCTP_UNKNOWN)) {
			break;
		} else {
			if (t->state != SCTP_UNCONFIRMED && !next)
				next = t;
		}
	}

	if (t)
		asoc->peer.retran_path = t;
	else
		t = asoc->peer.retran_path;

	SCTP_DEBUG_PRINTK_IPADDR("sctp_assoc_update_retran_path:association"
				 " %p addr: ",
				 " port: %d\n",
				 asoc,
				 (&t->ipaddr),
				 ntohs(t->ipaddr.v4.sin_port));
}

struct sctp_transport *sctp_assoc_choose_alter_transport(
	struct sctp_association *asoc, struct sctp_transport *last_sent_to)
{
	if (!last_sent_to)
		return asoc->peer.active_path;
	else {
		if (last_sent_to == asoc->peer.retran_path)
			sctp_assoc_update_retran_path(asoc);
		return asoc->peer.retran_path;
	}
}

void sctp_assoc_sync_pmtu(struct sctp_association *asoc)
{
	struct sctp_transport *t;
	__u32 pmtu = 0;

	if (!asoc)
		return;

	
	list_for_each_entry(t, &asoc->peer.transport_addr_list,
				transports) {
		if (t->pmtu_pending && t->dst) {
			sctp_transport_update_pmtu(t, dst_mtu(t->dst));
			t->pmtu_pending = 0;
		}
		if (!pmtu || (t->pathmtu < pmtu))
			pmtu = t->pathmtu;
	}

	if (pmtu) {
		asoc->pathmtu = pmtu;
		asoc->frag_point = sctp_frag_point(asoc, pmtu);
	}

	SCTP_DEBUG_PRINTK("%s: asoc:%p, pmtu:%d, frag_point:%d\n",
			  __func__, asoc, asoc->pathmtu, asoc->frag_point);
}

static inline int sctp_peer_needs_update(struct sctp_association *asoc)
{
	switch (asoc->state) {
	case SCTP_STATE_ESTABLISHED:
	case SCTP_STATE_SHUTDOWN_PENDING:
	case SCTP_STATE_SHUTDOWN_RECEIVED:
	case SCTP_STATE_SHUTDOWN_SENT:
		if ((asoc->rwnd > asoc->a_rwnd) &&
		    ((asoc->rwnd - asoc->a_rwnd) >= max_t(__u32,
			   (asoc->base.sk->sk_rcvbuf >> sctp_rwnd_upd_shift),
			   asoc->pathmtu)))
			return 1;
		break;
	default:
		break;
	}
	return 0;
}

void sctp_assoc_rwnd_increase(struct sctp_association *asoc, unsigned len)
{
	struct sctp_chunk *sack;
	struct timer_list *timer;

	if (asoc->rwnd_over) {
		if (asoc->rwnd_over >= len) {
			asoc->rwnd_over -= len;
		} else {
			asoc->rwnd += (len - asoc->rwnd_over);
			asoc->rwnd_over = 0;
		}
	} else {
		asoc->rwnd += len;
	}

	if (asoc->rwnd_press && asoc->rwnd >= asoc->rwnd_press) {
		int change = min(asoc->pathmtu, asoc->rwnd_press);
		asoc->rwnd += change;
		asoc->rwnd_press -= change;
	}

	SCTP_DEBUG_PRINTK("%s: asoc %p rwnd increased by %d to (%u, %u) "
			  "- %u\n", __func__, asoc, len, asoc->rwnd,
			  asoc->rwnd_over, asoc->a_rwnd);

	if (sctp_peer_needs_update(asoc)) {
		asoc->a_rwnd = asoc->rwnd;
		SCTP_DEBUG_PRINTK("%s: Sending window update SACK- asoc: %p "
				  "rwnd: %u a_rwnd: %u\n", __func__,
				  asoc, asoc->rwnd, asoc->a_rwnd);
		sack = sctp_make_sack(asoc);
		if (!sack)
			return;

		asoc->peer.sack_needed = 0;

		sctp_outq_tail(&asoc->outqueue, sack);

		
		timer = &asoc->timers[SCTP_EVENT_TIMEOUT_SACK];
		if (timer_pending(timer) && del_timer(timer))
			sctp_association_put(asoc);
	}
}

void sctp_assoc_rwnd_decrease(struct sctp_association *asoc, unsigned len)
{
	int rx_count;
	int over = 0;

	SCTP_ASSERT(asoc->rwnd, "rwnd zero", return);
	SCTP_ASSERT(!asoc->rwnd_over, "rwnd_over not zero", return);

	if (asoc->ep->rcvbuf_policy)
		rx_count = atomic_read(&asoc->rmem_alloc);
	else
		rx_count = atomic_read(&asoc->base.sk->sk_rmem_alloc);

	if (rx_count >= asoc->base.sk->sk_rcvbuf)
		over = 1;

	if (asoc->rwnd >= len) {
		asoc->rwnd -= len;
		if (over) {
			asoc->rwnd_press += asoc->rwnd;
			asoc->rwnd = 0;
		}
	} else {
		asoc->rwnd_over = len - asoc->rwnd;
		asoc->rwnd = 0;
	}
	SCTP_DEBUG_PRINTK("%s: asoc %p rwnd decreased by %d to (%u, %u, %u)\n",
			  __func__, asoc, len, asoc->rwnd,
			  asoc->rwnd_over, asoc->rwnd_press);
}

int sctp_assoc_set_bind_addr_from_ep(struct sctp_association *asoc,
				     sctp_scope_t scope, gfp_t gfp)
{
	int flags;

	flags = (PF_INET6 == asoc->base.sk->sk_family) ? SCTP_ADDR6_ALLOWED : 0;
	if (asoc->peer.ipv4_address)
		flags |= SCTP_ADDR4_PEERSUPP;
	if (asoc->peer.ipv6_address)
		flags |= SCTP_ADDR6_PEERSUPP;

	return sctp_bind_addr_copy(&asoc->base.bind_addr,
				   &asoc->ep->base.bind_addr,
				   scope, gfp, flags);
}

int sctp_assoc_set_bind_addr_from_cookie(struct sctp_association *asoc,
					 struct sctp_cookie *cookie,
					 gfp_t gfp)
{
	int var_size2 = ntohs(cookie->peer_init->chunk_hdr.length);
	int var_size3 = cookie->raw_addr_list_len;
	__u8 *raw = (__u8 *)cookie->peer_init + var_size2;

	return sctp_raw_to_bind_addrs(&asoc->base.bind_addr, raw, var_size3,
				      asoc->ep->base.bind_addr.port, gfp);
}

int sctp_assoc_lookup_laddr(struct sctp_association *asoc,
			    const union sctp_addr *laddr)
{
	int found = 0;

	if ((asoc->base.bind_addr.port == ntohs(laddr->v4.sin_port)) &&
	    sctp_bind_addr_match(&asoc->base.bind_addr, laddr,
				 sctp_sk(asoc->base.sk)))
		found = 1;

	return found;
}

int sctp_assoc_set_id(struct sctp_association *asoc, gfp_t gfp)
{
	int assoc_id;
	int error = 0;

	
	if (asoc->assoc_id)
		return error;
retry:
	if (unlikely(!idr_pre_get(&sctp_assocs_id, gfp)))
		return -ENOMEM;

	spin_lock_bh(&sctp_assocs_id_lock);
	error = idr_get_new_above(&sctp_assocs_id, (void *)asoc,
				    idr_low, &assoc_id);
	if (!error) {
		idr_low = assoc_id + 1;
		if (idr_low == INT_MAX)
			idr_low = 1;
	}
	spin_unlock_bh(&sctp_assocs_id_lock);
	if (error == -EAGAIN)
		goto retry;
	else if (error)
		return error;

	asoc->assoc_id = (sctp_assoc_t) assoc_id;
	return error;
}

static void sctp_assoc_free_asconf_queue(struct sctp_association *asoc)
{
	struct sctp_chunk *asconf;
	struct sctp_chunk *tmp;

	list_for_each_entry_safe(asconf, tmp, &asoc->addip_chunk_list, list) {
		list_del_init(&asconf->list);
		sctp_chunk_free(asconf);
	}
}

static void sctp_assoc_free_asconf_acks(struct sctp_association *asoc)
{
	struct sctp_chunk *ack;
	struct sctp_chunk *tmp;

	list_for_each_entry_safe(ack, tmp, &asoc->asconf_ack_list,
				transmitted_list) {
		list_del_init(&ack->transmitted_list);
		sctp_chunk_free(ack);
	}
}

void sctp_assoc_clean_asconf_ack_cache(const struct sctp_association *asoc)
{
	struct sctp_chunk *ack;
	struct sctp_chunk *tmp;

	list_for_each_entry_safe(ack, tmp, &asoc->asconf_ack_list,
				transmitted_list) {
		if (ack->subh.addip_hdr->serial ==
				htonl(asoc->peer.addip_serial))
			break;

		list_del_init(&ack->transmitted_list);
		sctp_chunk_free(ack);
	}
}

struct sctp_chunk *sctp_assoc_lookup_asconf_ack(
					const struct sctp_association *asoc,
					__be32 serial)
{
	struct sctp_chunk *ack;

	list_for_each_entry(ack, &asoc->asconf_ack_list, transmitted_list) {
		if (ack->subh.addip_hdr->serial == serial) {
			sctp_chunk_hold(ack);
			return ack;
		}
	}

	return NULL;
}

void sctp_asconf_queue_teardown(struct sctp_association *asoc)
{
	
	sctp_assoc_free_asconf_acks(asoc);

	
	sctp_assoc_free_asconf_queue(asoc);

	
	if (asoc->addip_last_asconf)
		sctp_chunk_free(asoc->addip_last_asconf);
}
