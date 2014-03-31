/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 *
 * This file is part of the SCTP kernel implementation
 *
 * These functions work with the state functions in sctp_sm_statefuns.c
 * to implement that state operations.  These functions implement the
 * steps which require modifying existing data structures.
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
 *    Jon Grimm             <jgrimm@austin.ibm.com>
 *    Hui Huang		    <hui.huang@nokia.com>
 *    Dajiang Zhang	    <dajiang.zhang@nokia.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *    Sridhar Samudrala	    <sri@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/gfp.h>
#include <net/sock.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

static int sctp_cmd_interpreter(sctp_event_t event_type,
				sctp_subtype_t subtype,
				sctp_state_t state,
				struct sctp_endpoint *ep,
				struct sctp_association *asoc,
				void *event_arg,
				sctp_disposition_t status,
				sctp_cmd_seq_t *commands,
				gfp_t gfp);
static int sctp_side_effects(sctp_event_t event_type, sctp_subtype_t subtype,
			     sctp_state_t state,
			     struct sctp_endpoint *ep,
			     struct sctp_association *asoc,
			     void *event_arg,
			     sctp_disposition_t status,
			     sctp_cmd_seq_t *commands,
			     gfp_t gfp);


static void sctp_do_ecn_ce_work(struct sctp_association *asoc,
				__u32 lowest_tsn)
{
	

	asoc->last_ecne_tsn = lowest_tsn;
	asoc->need_ecne = 1;
}

static struct sctp_chunk *sctp_do_ecn_ecne_work(struct sctp_association *asoc,
					   __u32 lowest_tsn,
					   struct sctp_chunk *chunk)
{
	struct sctp_chunk *repl;


	if (TSN_lt(asoc->last_cwr_tsn, lowest_tsn)) {
		struct sctp_transport *transport;

		transport = sctp_assoc_lookup_tsn(asoc, lowest_tsn);

		
		if (transport)
			sctp_transport_lower_cwnd(transport,
						  SCTP_LOWER_CWND_ECNE);
		asoc->last_cwr_tsn = lowest_tsn;
	}

	repl = sctp_make_cwr(asoc, asoc->last_cwr_tsn, chunk);

	return repl;
}

static void sctp_do_ecn_cwr_work(struct sctp_association *asoc,
				 __u32 lowest_tsn)
{
	asoc->need_ecne = 0;
}

static int sctp_gen_sack(struct sctp_association *asoc, int force,
			 sctp_cmd_seq_t *commands)
{
	__u32 ctsn, max_tsn_seen;
	struct sctp_chunk *sack;
	struct sctp_transport *trans = asoc->peer.last_data_from;
	int error = 0;

	if (force ||
	    (!trans && (asoc->param_flags & SPP_SACKDELAY_DISABLE)) ||
	    (trans && (trans->param_flags & SPP_SACKDELAY_DISABLE)))
		asoc->peer.sack_needed = 1;

	ctsn = sctp_tsnmap_get_ctsn(&asoc->peer.tsn_map);
	max_tsn_seen = sctp_tsnmap_get_max_tsn_seen(&asoc->peer.tsn_map);

	if (max_tsn_seen != ctsn)
		asoc->peer.sack_needed = 1;

	if (!asoc->peer.sack_needed) {
		asoc->peer.sack_cnt++;

		if (trans) {
			
			if (asoc->peer.sack_cnt >= trans->sackfreq - 1)
				asoc->peer.sack_needed = 1;

			asoc->timeouts[SCTP_EVENT_TIMEOUT_SACK] =
				trans->sackdelay;
		} else {
			
			if (asoc->peer.sack_cnt >= asoc->sackfreq - 1)
				asoc->peer.sack_needed = 1;

			asoc->timeouts[SCTP_EVENT_TIMEOUT_SACK] =
				asoc->sackdelay;
		}

		
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_RESTART,
				SCTP_TO(SCTP_EVENT_TIMEOUT_SACK));
	} else {
		asoc->a_rwnd = asoc->rwnd;
		sack = sctp_make_sack(asoc);
		if (!sack)
			goto nomem;

		asoc->peer.sack_needed = 0;
		asoc->peer.sack_cnt = 0;

		sctp_add_cmd_sf(commands, SCTP_CMD_REPLY, SCTP_CHUNK(sack));

		
		sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_STOP,
				SCTP_TO(SCTP_EVENT_TIMEOUT_SACK));
	}

	return error;
nomem:
	error = -ENOMEM;
	return error;
}

void sctp_generate_t3_rtx_event(unsigned long peer)
{
	int error;
	struct sctp_transport *transport = (struct sctp_transport *) peer;
	struct sctp_association *asoc = transport->asoc;

	

	sctp_bh_lock_sock(asoc->base.sk);
	if (sock_owned_by_user(asoc->base.sk)) {
		SCTP_DEBUG_PRINTK("%s:Sock is busy.\n", __func__);

		
		if (!mod_timer(&transport->T3_rtx_timer, jiffies + (HZ/20)))
			sctp_transport_hold(transport);
		goto out_unlock;
	}

	if (transport->dead)
		goto out_unlock;

	
	error = sctp_do_sm(SCTP_EVENT_T_TIMEOUT,
			   SCTP_ST_TIMEOUT(SCTP_EVENT_TIMEOUT_T3_RTX),
			   asoc->state,
			   asoc->ep, asoc,
			   transport, GFP_ATOMIC);

	if (error)
		asoc->base.sk->sk_err = -error;

out_unlock:
	sctp_bh_unlock_sock(asoc->base.sk);
	sctp_transport_put(transport);
}

static void sctp_generate_timeout_event(struct sctp_association *asoc,
					sctp_event_timeout_t timeout_type)
{
	int error = 0;

	sctp_bh_lock_sock(asoc->base.sk);
	if (sock_owned_by_user(asoc->base.sk)) {
		SCTP_DEBUG_PRINTK("%s:Sock is busy: timer %d\n",
				  __func__,
				  timeout_type);

		
		if (!mod_timer(&asoc->timers[timeout_type], jiffies + (HZ/20)))
			sctp_association_hold(asoc);
		goto out_unlock;
	}

	if (asoc->base.dead)
		goto out_unlock;

	
	error = sctp_do_sm(SCTP_EVENT_T_TIMEOUT,
			   SCTP_ST_TIMEOUT(timeout_type),
			   asoc->state, asoc->ep, asoc,
			   (void *)timeout_type, GFP_ATOMIC);

	if (error)
		asoc->base.sk->sk_err = -error;

out_unlock:
	sctp_bh_unlock_sock(asoc->base.sk);
	sctp_association_put(asoc);
}

static void sctp_generate_t1_cookie_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_T1_COOKIE);
}

static void sctp_generate_t1_init_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_T1_INIT);
}

static void sctp_generate_t2_shutdown_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_T2_SHUTDOWN);
}

static void sctp_generate_t4_rto_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_T4_RTO);
}

static void sctp_generate_t5_shutdown_guard_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *)data;
	sctp_generate_timeout_event(asoc,
				    SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD);

} 

static void sctp_generate_autoclose_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_AUTOCLOSE);
}

void sctp_generate_heartbeat_event(unsigned long data)
{
	int error = 0;
	struct sctp_transport *transport = (struct sctp_transport *) data;
	struct sctp_association *asoc = transport->asoc;

	sctp_bh_lock_sock(asoc->base.sk);
	if (sock_owned_by_user(asoc->base.sk)) {
		SCTP_DEBUG_PRINTK("%s:Sock is busy.\n", __func__);

		
		if (!mod_timer(&transport->hb_timer, jiffies + (HZ/20)))
			sctp_transport_hold(transport);
		goto out_unlock;
	}

	if (transport->dead)
		goto out_unlock;

	error = sctp_do_sm(SCTP_EVENT_T_TIMEOUT,
			   SCTP_ST_TIMEOUT(SCTP_EVENT_TIMEOUT_HEARTBEAT),
			   asoc->state, asoc->ep, asoc,
			   transport, GFP_ATOMIC);

	 if (error)
		 asoc->base.sk->sk_err = -error;

out_unlock:
	sctp_bh_unlock_sock(asoc->base.sk);
	sctp_transport_put(transport);
}

void sctp_generate_proto_unreach_event(unsigned long data)
{
	struct sctp_transport *transport = (struct sctp_transport *) data;
	struct sctp_association *asoc = transport->asoc;
	
	sctp_bh_lock_sock(asoc->base.sk);
	if (sock_owned_by_user(asoc->base.sk)) {
		SCTP_DEBUG_PRINTK("%s:Sock is busy.\n", __func__);

		
		if (!mod_timer(&transport->proto_unreach_timer,
				jiffies + (HZ/20)))
			sctp_association_hold(asoc);
		goto out_unlock;
	}

	if (asoc->base.dead)
		goto out_unlock;

	sctp_do_sm(SCTP_EVENT_T_OTHER,
		   SCTP_ST_OTHER(SCTP_EVENT_ICMP_PROTO_UNREACH),
		   asoc->state, asoc->ep, asoc, transport, GFP_ATOMIC);

out_unlock:
	sctp_bh_unlock_sock(asoc->base.sk);
	sctp_association_put(asoc);
}


static void sctp_generate_sack_event(unsigned long data)
{
	struct sctp_association *asoc = (struct sctp_association *) data;
	sctp_generate_timeout_event(asoc, SCTP_EVENT_TIMEOUT_SACK);
}

sctp_timer_event_t *sctp_timer_events[SCTP_NUM_TIMEOUT_TYPES] = {
	NULL,
	sctp_generate_t1_cookie_event,
	sctp_generate_t1_init_event,
	sctp_generate_t2_shutdown_event,
	NULL,
	sctp_generate_t4_rto_event,
	sctp_generate_t5_shutdown_guard_event,
	NULL,
	sctp_generate_sack_event,
	sctp_generate_autoclose_event,
};


static void sctp_do_8_2_transport_strike(struct sctp_association *asoc,
					 struct sctp_transport *transport,
					 int is_hb)
{
	if (!is_hb) {
		asoc->overall_error_count++;
		if (transport->state != SCTP_INACTIVE)
			transport->error_count++;
	 } else if (transport->hb_sent) {
		if (transport->state != SCTP_UNCONFIRMED)
			asoc->overall_error_count++;
		if (transport->state != SCTP_INACTIVE)
			transport->error_count++;
	}

	if (transport->state != SCTP_INACTIVE &&
	    (transport->error_count > transport->pathmaxrxt)) {
		SCTP_DEBUG_PRINTK_IPADDR("transport_strike:association %p",
					 " transport IP: port:%d failed.\n",
					 asoc,
					 (&transport->ipaddr),
					 ntohs(transport->ipaddr.v4.sin_port));
		sctp_assoc_control_transport(asoc, transport,
					     SCTP_TRANSPORT_DOWN,
					     SCTP_FAILED_THRESHOLD);
	}

	if (!is_hb || transport->hb_sent) {
		transport->rto = min((transport->rto * 2), transport->asoc->rto_max);
	}
}

static void sctp_cmd_init_failed(sctp_cmd_seq_t *commands,
				 struct sctp_association *asoc,
				 unsigned error)
{
	struct sctp_ulpevent *event;

	event = sctp_ulpevent_make_assoc_change(asoc,0, SCTP_CANT_STR_ASSOC,
						(__u16)error, 0, 0, NULL,
						GFP_ATOMIC);

	if (event)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(event));

	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_CLOSED));

	
	asoc->outqueue.error = error;
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());
}

static void sctp_cmd_assoc_failed(sctp_cmd_seq_t *commands,
				  struct sctp_association *asoc,
				  sctp_event_t event_type,
				  sctp_subtype_t subtype,
				  struct sctp_chunk *chunk,
				  unsigned error)
{
	struct sctp_ulpevent *event;

	
	sctp_ulpq_abort_pd(&asoc->ulpq, GFP_ATOMIC);

	if (event_type == SCTP_EVENT_T_CHUNK && subtype.chunk == SCTP_CID_ABORT)
		event = sctp_ulpevent_make_assoc_change(asoc, 0, SCTP_COMM_LOST,
						(__u16)error, 0, 0, chunk,
						GFP_ATOMIC);
	else
		event = sctp_ulpevent_make_assoc_change(asoc, 0, SCTP_COMM_LOST,
						(__u16)error, 0, 0, NULL,
						GFP_ATOMIC);
	if (event)
		sctp_add_cmd_sf(commands, SCTP_CMD_EVENT_ULP,
				SCTP_ULPEVENT(event));

	sctp_add_cmd_sf(commands, SCTP_CMD_NEW_STATE,
			SCTP_STATE(SCTP_STATE_CLOSED));

	
	asoc->outqueue.error = error;
	sctp_add_cmd_sf(commands, SCTP_CMD_DELETE_TCB, SCTP_NULL());
}

static int sctp_cmd_process_init(sctp_cmd_seq_t *commands,
				 struct sctp_association *asoc,
				 struct sctp_chunk *chunk,
				 sctp_init_chunk_t *peer_init,
				 gfp_t gfp)
{
	int error;

	if (!sctp_process_init(asoc, chunk, sctp_source(chunk), peer_init, gfp))
		error = -ENOMEM;
	else
		error = 0;

	return error;
}

static void sctp_cmd_hb_timers_start(sctp_cmd_seq_t *cmds,
				     struct sctp_association *asoc)
{
	struct sctp_transport *t;

	list_for_each_entry(t, &asoc->peer.transport_addr_list, transports) {

		if (!mod_timer(&t->hb_timer, sctp_transport_timeout(t)))
			sctp_transport_hold(t);
	}
}

static void sctp_cmd_hb_timers_stop(sctp_cmd_seq_t *cmds,
				    struct sctp_association *asoc)
{
	struct sctp_transport *t;

	

	list_for_each_entry(t, &asoc->peer.transport_addr_list,
			transports) {
		if (del_timer(&t->hb_timer))
			sctp_transport_put(t);
	}
}

static void sctp_cmd_t3_rtx_timers_stop(sctp_cmd_seq_t *cmds,
					struct sctp_association *asoc)
{
	struct sctp_transport *t;

	list_for_each_entry(t, &asoc->peer.transport_addr_list,
			transports) {
		if (timer_pending(&t->T3_rtx_timer) &&
		    del_timer(&t->T3_rtx_timer)) {
			sctp_transport_put(t);
		}
	}
}


static void sctp_cmd_hb_timer_update(sctp_cmd_seq_t *cmds,
				     struct sctp_transport *t)
{
	
	if (!mod_timer(&t->hb_timer, sctp_transport_timeout(t)))
		sctp_transport_hold(t);
}

static void sctp_cmd_transport_on(sctp_cmd_seq_t *cmds,
				  struct sctp_association *asoc,
				  struct sctp_transport *t,
				  struct sctp_chunk *chunk)
{
	sctp_sender_hb_info_t *hbinfo;
	int was_unconfirmed = 0;

	t->error_count = 0;

	if (t->asoc->state != SCTP_STATE_SHUTDOWN_PENDING)
		t->asoc->overall_error_count = 0;

	t->hb_sent = 0;

	if ((t->state == SCTP_INACTIVE) || (t->state == SCTP_UNCONFIRMED)) {
		was_unconfirmed = 1;
		sctp_assoc_control_transport(asoc, t, SCTP_TRANSPORT_UP,
					     SCTP_HEARTBEAT_SUCCESS);
	}

	if (t->rto_pending == 0)
		t->rto_pending = 1;

	hbinfo = (sctp_sender_hb_info_t *) chunk->skb->data;
	sctp_transport_update_rto(t, (jiffies - hbinfo->sent_at));

	
	if (!mod_timer(&t->hb_timer, sctp_transport_timeout(t)))
		sctp_transport_hold(t);

	if (was_unconfirmed && asoc->peer.transport_count == 1)
		sctp_transport_immediate_rtx(t);
}


static int sctp_cmd_process_sack(sctp_cmd_seq_t *cmds,
				 struct sctp_association *asoc,
				 struct sctp_sackhdr *sackh)
{
	int err = 0;

	if (sctp_outq_sack(&asoc->outqueue, sackh)) {
		
		err = sctp_do_sm(SCTP_EVENT_T_OTHER,
				 SCTP_ST_OTHER(SCTP_EVENT_NO_PENDING_TSN),
				 asoc->state, asoc->ep, asoc, NULL,
				 GFP_ATOMIC);
	}

	return err;
}

static void sctp_cmd_setup_t2(sctp_cmd_seq_t *cmds,
			      struct sctp_association *asoc,
			      struct sctp_chunk *chunk)
{
	struct sctp_transport *t;

	if (chunk->transport)
		t = chunk->transport;
	else {
		t = sctp_assoc_choose_alter_transport(asoc,
					      asoc->shutdown_last_sent_to);
		chunk->transport = t;
	}
	asoc->shutdown_last_sent_to = t;
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T2_SHUTDOWN] = t->rto;
}

static void sctp_cmd_new_state(sctp_cmd_seq_t *cmds,
			       struct sctp_association *asoc,
			       sctp_state_t state)
{
	struct sock *sk = asoc->base.sk;

	asoc->state = state;

	SCTP_DEBUG_PRINTK("sctp_cmd_new_state: asoc %p[%s]\n",
			  asoc, sctp_state_tbl[state]);

	if (sctp_style(sk, TCP)) {
		if (sctp_state(asoc, ESTABLISHED) && sctp_sstate(sk, CLOSED))
			sk->sk_state = SCTP_SS_ESTABLISHED;

		
		if (sctp_state(asoc, SHUTDOWN_RECEIVED) &&
		    sctp_sstate(sk, ESTABLISHED))
			sk->sk_shutdown |= RCV_SHUTDOWN;
	}

	if (sctp_state(asoc, COOKIE_WAIT)) {
		asoc->timeouts[SCTP_EVENT_TIMEOUT_T1_INIT] =
						asoc->rto_initial;
		asoc->timeouts[SCTP_EVENT_TIMEOUT_T1_COOKIE] =
						asoc->rto_initial;
	}

	if (sctp_state(asoc, ESTABLISHED) ||
	    sctp_state(asoc, CLOSED) ||
	    sctp_state(asoc, SHUTDOWN_RECEIVED)) {
		if (waitqueue_active(&asoc->wait))
			wake_up_interruptible(&asoc->wait);

		if (!sctp_style(sk, UDP))
			sk->sk_state_change(sk);
	}
}

static void sctp_cmd_delete_tcb(sctp_cmd_seq_t *cmds,
				struct sctp_association *asoc)
{
	struct sock *sk = asoc->base.sk;

	if (sctp_style(sk, TCP) && sctp_sstate(sk, LISTENING) &&
	    (!asoc->temp) && (sk->sk_shutdown != SHUTDOWN_MASK))
		return;

	sctp_unhash_established(asoc);
	sctp_association_free(asoc);
}

static void sctp_cmd_setup_t4(sctp_cmd_seq_t *cmds,
				struct sctp_association *asoc,
				struct sctp_chunk *chunk)
{
	struct sctp_transport *t;

	t = sctp_assoc_choose_alter_transport(asoc, chunk->transport);
	asoc->timeouts[SCTP_EVENT_TIMEOUT_T4_RTO] = t->rto;
	chunk->transport = t;
}

static void sctp_cmd_process_operr(sctp_cmd_seq_t *cmds,
				   struct sctp_association *asoc,
				   struct sctp_chunk *chunk)
{
	struct sctp_errhdr *err_hdr;
	struct sctp_ulpevent *ev;

	while (chunk->chunk_end > chunk->skb->data) {
		err_hdr = (struct sctp_errhdr *)(chunk->skb->data);

		ev = sctp_ulpevent_make_remote_error(asoc, chunk, 0,
						     GFP_ATOMIC);
		if (!ev)
			return;

		sctp_ulpq_tail_event(&asoc->ulpq, ev);

		switch (err_hdr->cause) {
		case SCTP_ERROR_UNKNOWN_CHUNK:
		{
			sctp_chunkhdr_t *unk_chunk_hdr;

			unk_chunk_hdr = (sctp_chunkhdr_t *)err_hdr->variable;
			switch (unk_chunk_hdr->type) {
			case SCTP_CID_ASCONF:
				if (asoc->peer.asconf_capable == 0)
					break;

				asoc->peer.asconf_capable = 0;
				sctp_add_cmd_sf(cmds, SCTP_CMD_TIMER_STOP,
					SCTP_TO(SCTP_EVENT_TIMEOUT_T4_RTO));
				break;
			default:
				break;
			}
			break;
		}
		default:
			break;
		}
	}
}

static void sctp_cmd_process_fwdtsn(struct sctp_ulpq *ulpq,
				    struct sctp_chunk *chunk)
{
	struct sctp_fwdtsn_skip *skip;
	
	sctp_walk_fwdtsn(skip, chunk) {
		sctp_ulpq_skip(ulpq, ntohs(skip->stream), ntohs(skip->ssn));
	}
}

static void sctp_cmd_del_non_primary(struct sctp_association *asoc)
{
	struct sctp_transport *t;
	struct list_head *pos;
	struct list_head *temp;

	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		t = list_entry(pos, struct sctp_transport, transports);
		if (!sctp_cmp_addr_exact(&t->ipaddr,
					 &asoc->peer.primary_addr)) {
			sctp_assoc_del_peer(asoc, &t->ipaddr);
		}
	}
}

static void sctp_cmd_set_sk_err(struct sctp_association *asoc, int error)
{
	struct sock *sk = asoc->base.sk;

	if (!sctp_style(sk, UDP))
		sk->sk_err = error;
}

static void sctp_cmd_assoc_change(sctp_cmd_seq_t *commands,
				 struct sctp_association *asoc,
				 u8 state)
{
	struct sctp_ulpevent *ev;

	ev = sctp_ulpevent_make_assoc_change(asoc, 0, state, 0,
					    asoc->c.sinit_num_ostreams,
					    asoc->c.sinit_max_instreams,
					    NULL, GFP_ATOMIC);
	if (ev)
		sctp_ulpq_tail_event(&asoc->ulpq, ev);
}

static void sctp_cmd_adaptation_ind(sctp_cmd_seq_t *commands,
				    struct sctp_association *asoc)
{
	struct sctp_ulpevent *ev;

	ev = sctp_ulpevent_make_adaptation_indication(asoc, GFP_ATOMIC);

	if (ev)
		sctp_ulpq_tail_event(&asoc->ulpq, ev);
}


static void sctp_cmd_t1_timer_update(struct sctp_association *asoc,
				    sctp_event_timeout_t timer,
				    char *name)
{
	struct sctp_transport *t;

	t = asoc->init_last_sent_to;
	asoc->init_err_counter++;

	if (t->init_sent_count > (asoc->init_cycle + 1)) {
		asoc->timeouts[timer] *= 2;
		if (asoc->timeouts[timer] > asoc->max_init_timeo) {
			asoc->timeouts[timer] = asoc->max_init_timeo;
		}
		asoc->init_cycle++;
		SCTP_DEBUG_PRINTK(
			"T1 %s Timeout adjustment"
			" init_err_counter: %d"
			" cycle: %d"
			" timeout: %ld\n",
			name,
			asoc->init_err_counter,
			asoc->init_cycle,
			asoc->timeouts[timer]);
	}

}

static int sctp_cmd_send_msg(struct sctp_association *asoc,
				struct sctp_datamsg *msg)
{
	struct sctp_chunk *chunk;
	int error = 0;

	list_for_each_entry(chunk, &msg->chunks, frag_list) {
		error = sctp_outq_tail(&asoc->outqueue, chunk);
		if (error)
			break;
	}

	return error;
}


static void sctp_cmd_send_asconf(struct sctp_association *asoc)
{
	if (!list_empty(&asoc->addip_chunk_list)) {
		struct list_head *entry = asoc->addip_chunk_list.next;
		struct sctp_chunk *asconf = list_entry(entry,
						struct sctp_chunk, list);
		list_del_init(entry);

		
		sctp_chunk_hold(asconf);
		if (sctp_primitive_ASCONF(asoc, asconf))
			sctp_chunk_free(asconf);
		else
			asoc->addip_last_asconf = asconf;
	}
}


#define DEBUG_PRE \
	SCTP_DEBUG_PRINTK("sctp_do_sm prefn: " \
			  "ep %p, %s, %s, asoc %p[%s], %s\n", \
			  ep, sctp_evttype_tbl[event_type], \
			  (*debug_fn)(subtype), asoc, \
			  sctp_state_tbl[state], state_fn->name)

#define DEBUG_POST \
	SCTP_DEBUG_PRINTK("sctp_do_sm postfn: " \
			  "asoc %p, status: %s\n", \
			  asoc, sctp_status_tbl[status])

#define DEBUG_POST_SFX \
	SCTP_DEBUG_PRINTK("sctp_do_sm post sfx: error %d, asoc %p[%s]\n", \
			  error, asoc, \
			  sctp_state_tbl[(asoc && sctp_id2assoc(ep->base.sk, \
			  sctp_assoc2id(asoc)))?asoc->state:SCTP_STATE_CLOSED])

int sctp_do_sm(sctp_event_t event_type, sctp_subtype_t subtype,
	       sctp_state_t state,
	       struct sctp_endpoint *ep,
	       struct sctp_association *asoc,
	       void *event_arg,
	       gfp_t gfp)
{
	sctp_cmd_seq_t commands;
	const sctp_sm_table_entry_t *state_fn;
	sctp_disposition_t status;
	int error = 0;
	typedef const char *(printfn_t)(sctp_subtype_t);

	static printfn_t *table[] = {
		NULL, sctp_cname, sctp_tname, sctp_oname, sctp_pname,
	};
	printfn_t *debug_fn  __attribute__ ((unused)) = table[event_type];

	state_fn = sctp_sm_lookup_event(event_type, state, subtype);

	sctp_init_cmd_seq(&commands);

	DEBUG_PRE;
	status = (*state_fn->fn)(ep, asoc, subtype, event_arg, &commands);
	DEBUG_POST;

	error = sctp_side_effects(event_type, subtype, state,
				  ep, asoc, event_arg, status,
				  &commands, gfp);
	DEBUG_POST_SFX;

	return error;
}

#undef DEBUG_PRE
#undef DEBUG_POST

static int sctp_side_effects(sctp_event_t event_type, sctp_subtype_t subtype,
			     sctp_state_t state,
			     struct sctp_endpoint *ep,
			     struct sctp_association *asoc,
			     void *event_arg,
			     sctp_disposition_t status,
			     sctp_cmd_seq_t *commands,
			     gfp_t gfp)
{
	int error;

	if (0 != (error = sctp_cmd_interpreter(event_type, subtype, state,
					       ep, asoc,
					       event_arg, status,
					       commands, gfp)))
		goto bail;

	switch (status) {
	case SCTP_DISPOSITION_DISCARD:
		SCTP_DEBUG_PRINTK("Ignored sctp protocol event - state %d, "
				  "event_type %d, event_id %d\n",
				  state, event_type, subtype.chunk);
		break;

	case SCTP_DISPOSITION_NOMEM:
		error = -ENOMEM;
		break;

	case SCTP_DISPOSITION_DELETE_TCB:
		
		break;

	case SCTP_DISPOSITION_CONSUME:
	case SCTP_DISPOSITION_ABORT:
		break;

	case SCTP_DISPOSITION_VIOLATION:
		if (net_ratelimit())
			pr_err("protocol violation state %d chunkid %d\n",
			       state, subtype.chunk);
		break;

	case SCTP_DISPOSITION_NOT_IMPL:
		pr_warn("unimplemented feature in state %d, event_type %d, event_id %d\n",
			state, event_type, subtype.chunk);
		break;

	case SCTP_DISPOSITION_BUG:
		pr_err("bug in state %d, event_type %d, event_id %d\n",
		       state, event_type, subtype.chunk);
		BUG();
		break;

	default:
		pr_err("impossible disposition %d in state %d, event_type %d, event_id %d\n",
		       status, state, event_type, subtype.chunk);
		BUG();
		break;
	}

bail:
	return error;
}


static int sctp_cmd_interpreter(sctp_event_t event_type,
				sctp_subtype_t subtype,
				sctp_state_t state,
				struct sctp_endpoint *ep,
				struct sctp_association *asoc,
				void *event_arg,
				sctp_disposition_t status,
				sctp_cmd_seq_t *commands,
				gfp_t gfp)
{
	int error = 0;
	int force;
	sctp_cmd_t *cmd;
	struct sctp_chunk *new_obj;
	struct sctp_chunk *chunk = NULL;
	struct sctp_packet *packet;
	struct timer_list *timer;
	unsigned long timeout;
	struct sctp_transport *t;
	struct sctp_sackhdr sackh;
	int local_cork = 0;

	if (SCTP_EVENT_T_TIMEOUT != event_type)
		chunk = event_arg;

	while (NULL != (cmd = sctp_next_cmd(commands))) {
		switch (cmd->verb) {
		case SCTP_CMD_NOP:
			
			break;

		case SCTP_CMD_NEW_ASOC:
			
			if (local_cork) {
				sctp_outq_uncork(&asoc->outqueue);
				local_cork = 0;
			}
			asoc = cmd->obj.ptr;
			
			sctp_endpoint_add_asoc(ep, asoc);
			sctp_hash_established(asoc);
			break;

		case SCTP_CMD_UPDATE_ASSOC:
		       sctp_assoc_update(asoc, cmd->obj.ptr);
		       break;

		case SCTP_CMD_PURGE_OUTQUEUE:
		       sctp_outq_teardown(&asoc->outqueue);
		       break;

		case SCTP_CMD_DELETE_TCB:
			if (local_cork) {
				sctp_outq_uncork(&asoc->outqueue);
				local_cork = 0;
			}
			
			sctp_cmd_delete_tcb(commands, asoc);
			asoc = NULL;
			break;

		case SCTP_CMD_NEW_STATE:
			
			sctp_cmd_new_state(commands, asoc, cmd->obj.state);
			break;

		case SCTP_CMD_REPORT_TSN:
			
			error = sctp_tsnmap_mark(&asoc->peer.tsn_map,
						 cmd->obj.u32);
			break;

		case SCTP_CMD_REPORT_FWDTSN:
			
			sctp_tsnmap_skip(&asoc->peer.tsn_map, cmd->obj.u32);

			
			sctp_ulpq_reasm_flushtsn(&asoc->ulpq, cmd->obj.u32);

			
			sctp_ulpq_abort_pd(&asoc->ulpq, GFP_ATOMIC);
			break;

		case SCTP_CMD_PROCESS_FWDTSN:
			sctp_cmd_process_fwdtsn(&asoc->ulpq, cmd->obj.ptr);
			break;

		case SCTP_CMD_GEN_SACK:
			force = cmd->obj.i32;
			error = sctp_gen_sack(asoc, force, commands);
			break;

		case SCTP_CMD_PROCESS_SACK:
			
			error = sctp_cmd_process_sack(commands, asoc,
						      cmd->obj.ptr);
			break;

		case SCTP_CMD_GEN_INIT_ACK:
			
			new_obj = sctp_make_init_ack(asoc, chunk, GFP_ATOMIC,
						     0);
			if (!new_obj)
				goto nomem;

			sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
					SCTP_CHUNK(new_obj));
			break;

		case SCTP_CMD_PEER_INIT:
			error = sctp_cmd_process_init(commands, asoc, chunk,
						      cmd->obj.ptr, gfp);
			break;

		case SCTP_CMD_GEN_COOKIE_ECHO:
			
			new_obj = sctp_make_cookie_echo(asoc, chunk);
			if (!new_obj) {
				if (cmd->obj.ptr)
					sctp_chunk_free(cmd->obj.ptr);
				goto nomem;
			}
			sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
					SCTP_CHUNK(new_obj));

			if (cmd->obj.ptr)
				sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
						SCTP_CHUNK(cmd->obj.ptr));

			if (new_obj->transport) {
				new_obj->transport->init_sent_count++;
				asoc->init_last_sent_to = new_obj->transport;
			}

			if ((asoc->peer.retran_path !=
			     asoc->peer.primary_path) &&
			    (asoc->init_err_counter > 0)) {
				sctp_add_cmd_sf(commands,
						SCTP_CMD_FORCE_PRIM_RETRAN,
						SCTP_NULL());
			}

			break;

		case SCTP_CMD_GEN_SHUTDOWN:
			asoc->overall_error_count = 0;

			
			new_obj = sctp_make_shutdown(asoc, chunk);
			if (!new_obj)
				goto nomem;
			sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
					SCTP_CHUNK(new_obj));
			break;

		case SCTP_CMD_CHUNK_ULP:
			
			SCTP_DEBUG_PRINTK("sm_sideff: %s %p, %s %p.\n",
					  "chunk_up:", cmd->obj.ptr,
					  "ulpq:", &asoc->ulpq);
			sctp_ulpq_tail_data(&asoc->ulpq, cmd->obj.ptr,
					    GFP_ATOMIC);
			break;

		case SCTP_CMD_EVENT_ULP:
			
			SCTP_DEBUG_PRINTK("sm_sideff: %s %p, %s %p.\n",
					  "event_up:",cmd->obj.ptr,
					  "ulpq:",&asoc->ulpq);
			sctp_ulpq_tail_event(&asoc->ulpq, cmd->obj.ptr);
			break;

		case SCTP_CMD_REPLY:
			
			if (!asoc->outqueue.cork) {
				sctp_outq_cork(&asoc->outqueue);
				local_cork = 1;
			}
			
			error = sctp_outq_tail(&asoc->outqueue, cmd->obj.ptr);
			break;

		case SCTP_CMD_SEND_PKT:
			
			packet = cmd->obj.ptr;
			sctp_packet_transmit(packet);
			sctp_ootb_pkt_free(packet);
			break;

		case SCTP_CMD_T1_RETRAN:
			
			sctp_retransmit(&asoc->outqueue, cmd->obj.transport,
					SCTP_RTXR_T1_RTX);
			break;

		case SCTP_CMD_RETRAN:
			
			sctp_retransmit(&asoc->outqueue, cmd->obj.transport,
					SCTP_RTXR_T3_RTX);
			break;

		case SCTP_CMD_ECN_CE:
			
			sctp_do_ecn_ce_work(asoc, cmd->obj.u32);
			break;

		case SCTP_CMD_ECN_ECNE:
			
			new_obj = sctp_do_ecn_ecne_work(asoc, cmd->obj.u32,
							chunk);
			if (new_obj)
				sctp_add_cmd_sf(commands, SCTP_CMD_REPLY,
						SCTP_CHUNK(new_obj));
			break;

		case SCTP_CMD_ECN_CWR:
			
			sctp_do_ecn_cwr_work(asoc, cmd->obj.u32);
			break;

		case SCTP_CMD_SETUP_T2:
			sctp_cmd_setup_t2(commands, asoc, cmd->obj.ptr);
			break;

		case SCTP_CMD_TIMER_START_ONCE:
			timer = &asoc->timers[cmd->obj.to];

			if (timer_pending(timer))
				break;
			

		case SCTP_CMD_TIMER_START:
			timer = &asoc->timers[cmd->obj.to];
			timeout = asoc->timeouts[cmd->obj.to];
			BUG_ON(!timeout);

			timer->expires = jiffies + timeout;
			sctp_association_hold(asoc);
			add_timer(timer);
			break;

		case SCTP_CMD_TIMER_RESTART:
			timer = &asoc->timers[cmd->obj.to];
			timeout = asoc->timeouts[cmd->obj.to];
			if (!mod_timer(timer, jiffies + timeout))
				sctp_association_hold(asoc);
			break;

		case SCTP_CMD_TIMER_STOP:
			timer = &asoc->timers[cmd->obj.to];
			if (timer_pending(timer) && del_timer(timer))
				sctp_association_put(asoc);
			break;

		case SCTP_CMD_INIT_CHOOSE_TRANSPORT:
			chunk = cmd->obj.ptr;
			t = sctp_assoc_choose_alter_transport(asoc,
						asoc->init_last_sent_to);
			asoc->init_last_sent_to = t;
			chunk->transport = t;
			t->init_sent_count++;
			
			sctp_assoc_set_primary(asoc, t);
			break;

		case SCTP_CMD_INIT_RESTART:
			sctp_cmd_t1_timer_update(asoc,
						SCTP_EVENT_TIMEOUT_T1_INIT,
						"INIT");

			sctp_add_cmd_sf(commands, SCTP_CMD_TIMER_RESTART,
					SCTP_TO(SCTP_EVENT_TIMEOUT_T1_INIT));
			break;

		case SCTP_CMD_COOKIEECHO_RESTART:
			sctp_cmd_t1_timer_update(asoc,
						SCTP_EVENT_TIMEOUT_T1_COOKIE,
						"COOKIE");

			list_for_each_entry(t, &asoc->peer.transport_addr_list,
					transports) {
				sctp_retransmit_mark(&asoc->outqueue, t,
					    SCTP_RTXR_T1_RTX);
			}

			sctp_add_cmd_sf(commands,
					SCTP_CMD_TIMER_RESTART,
					SCTP_TO(SCTP_EVENT_TIMEOUT_T1_COOKIE));
			break;

		case SCTP_CMD_INIT_FAILED:
			sctp_cmd_init_failed(commands, asoc, cmd->obj.err);
			break;

		case SCTP_CMD_ASSOC_FAILED:
			sctp_cmd_assoc_failed(commands, asoc, event_type,
					      subtype, chunk, cmd->obj.err);
			break;

		case SCTP_CMD_INIT_COUNTER_INC:
			asoc->init_err_counter++;
			break;

		case SCTP_CMD_INIT_COUNTER_RESET:
			asoc->init_err_counter = 0;
			asoc->init_cycle = 0;
			list_for_each_entry(t, &asoc->peer.transport_addr_list,
					    transports) {
				t->init_sent_count = 0;
			}
			break;

		case SCTP_CMD_REPORT_DUP:
			sctp_tsnmap_mark_dup(&asoc->peer.tsn_map,
					     cmd->obj.u32);
			break;

		case SCTP_CMD_REPORT_BAD_TAG:
			SCTP_DEBUG_PRINTK("vtag mismatch!\n");
			break;

		case SCTP_CMD_STRIKE:
			
			sctp_do_8_2_transport_strike(asoc, cmd->obj.transport,
						    0);
			break;

		case SCTP_CMD_TRANSPORT_IDLE:
			t = cmd->obj.transport;
			sctp_transport_lower_cwnd(t, SCTP_LOWER_CWND_INACTIVE);
			break;

		case SCTP_CMD_TRANSPORT_HB_SENT:
			t = cmd->obj.transport;
			sctp_do_8_2_transport_strike(asoc, t, 1);
			t->hb_sent = 1;
			break;

		case SCTP_CMD_TRANSPORT_ON:
			t = cmd->obj.transport;
			sctp_cmd_transport_on(commands, asoc, t, chunk);
			break;

		case SCTP_CMD_HB_TIMERS_START:
			sctp_cmd_hb_timers_start(commands, asoc);
			break;

		case SCTP_CMD_HB_TIMER_UPDATE:
			t = cmd->obj.transport;
			sctp_cmd_hb_timer_update(commands, t);
			break;

		case SCTP_CMD_HB_TIMERS_STOP:
			sctp_cmd_hb_timers_stop(commands, asoc);
			break;

		case SCTP_CMD_REPORT_ERROR:
			error = cmd->obj.error;
			break;

		case SCTP_CMD_PROCESS_CTSN:
			
			sackh.cum_tsn_ack = cmd->obj.be32;
			sackh.a_rwnd = asoc->peer.rwnd +
					asoc->outqueue.outstanding_bytes;
			sackh.num_gap_ack_blocks = 0;
			sackh.num_dup_tsns = 0;
			sctp_add_cmd_sf(commands, SCTP_CMD_PROCESS_SACK,
					SCTP_SACKH(&sackh));
			break;

		case SCTP_CMD_DISCARD_PACKET:
			chunk->pdiscard = 1;
			if (asoc) {
				sctp_outq_uncork(&asoc->outqueue);
				local_cork = 0;
			}
			break;

		case SCTP_CMD_RTO_PENDING:
			t = cmd->obj.transport;
			t->rto_pending = 1;
			break;

		case SCTP_CMD_PART_DELIVER:
			sctp_ulpq_partial_delivery(&asoc->ulpq, cmd->obj.ptr,
						   GFP_ATOMIC);
			break;

		case SCTP_CMD_RENEGE:
			sctp_ulpq_renege(&asoc->ulpq, cmd->obj.ptr,
					 GFP_ATOMIC);
			break;

		case SCTP_CMD_SETUP_T4:
			sctp_cmd_setup_t4(commands, asoc, cmd->obj.ptr);
			break;

		case SCTP_CMD_PROCESS_OPERR:
			sctp_cmd_process_operr(commands, asoc, chunk);
			break;
		case SCTP_CMD_CLEAR_INIT_TAG:
			asoc->peer.i.init_tag = 0;
			break;
		case SCTP_CMD_DEL_NON_PRIMARY:
			sctp_cmd_del_non_primary(asoc);
			break;
		case SCTP_CMD_T3_RTX_TIMERS_STOP:
			sctp_cmd_t3_rtx_timers_stop(commands, asoc);
			break;
		case SCTP_CMD_FORCE_PRIM_RETRAN:
			t = asoc->peer.retran_path;
			asoc->peer.retran_path = asoc->peer.primary_path;
			error = sctp_outq_uncork(&asoc->outqueue);
			local_cork = 0;
			asoc->peer.retran_path = t;
			break;
		case SCTP_CMD_SET_SK_ERR:
			sctp_cmd_set_sk_err(asoc, cmd->obj.error);
			break;
		case SCTP_CMD_ASSOC_CHANGE:
			sctp_cmd_assoc_change(commands, asoc,
					      cmd->obj.u8);
			break;
		case SCTP_CMD_ADAPTATION_IND:
			sctp_cmd_adaptation_ind(commands, asoc);
			break;

		case SCTP_CMD_ASSOC_SHKEY:
			error = sctp_auth_asoc_init_active_key(asoc,
						GFP_ATOMIC);
			break;
		case SCTP_CMD_UPDATE_INITTAG:
			asoc->peer.i.init_tag = cmd->obj.u32;
			break;
		case SCTP_CMD_SEND_MSG:
			if (!asoc->outqueue.cork) {
				sctp_outq_cork(&asoc->outqueue);
				local_cork = 1;
			}
			error = sctp_cmd_send_msg(asoc, cmd->obj.msg);
			break;
		case SCTP_CMD_SEND_NEXT_ASCONF:
			sctp_cmd_send_asconf(asoc);
			break;
		case SCTP_CMD_PURGE_ASCONF_QUEUE:
			sctp_asconf_queue_teardown(asoc);
			break;

		case SCTP_CMD_SET_ASOC:
			asoc = cmd->obj.asoc;
			break;

		default:
			pr_warn("Impossible command: %u, %p\n",
				cmd->verb, cmd->obj.ptr);
			break;
		}

		if (error)
			break;
	}

out:
	if (asoc && SCTP_EVENT_T_CHUNK == event_type && chunk) {
		if (chunk->end_of_packet || chunk->singleton)
			error = sctp_outq_uncork(&asoc->outqueue);
	} else if (local_cork)
		error = sctp_outq_uncork(&asoc->outqueue);
	return error;
nomem:
	error = -ENOMEM;
	goto out;
}

