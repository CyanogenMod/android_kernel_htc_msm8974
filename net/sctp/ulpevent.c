/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Nokia, Inc.
 * Copyright (c) 2001 La Monte H.P. Yarroll
 *
 * These functions manipulate an sctp event.   The struct ulpevent is used
 * to carry notifications and data to the ULP (sockets).
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
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Sridhar Samudrala     <sri@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <net/sctp/structs.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

static void sctp_ulpevent_receive_data(struct sctp_ulpevent *event,
				       struct sctp_association *asoc);
static void sctp_ulpevent_release_data(struct sctp_ulpevent *event);
static void sctp_ulpevent_release_frag_data(struct sctp_ulpevent *event);


SCTP_STATIC void sctp_ulpevent_init(struct sctp_ulpevent *event,
				    int msg_flags,
				    unsigned int len)
{
	memset(event, 0, sizeof(struct sctp_ulpevent));
	event->msg_flags = msg_flags;
	event->rmem_len = len;
}

SCTP_STATIC struct sctp_ulpevent *sctp_ulpevent_new(int size, int msg_flags,
						    gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sk_buff *skb;

	skb = alloc_skb(size, gfp);
	if (!skb)
		goto fail;

	event = sctp_skb2event(skb);
	sctp_ulpevent_init(event, msg_flags, skb->truesize);

	return event;

fail:
	return NULL;
}

int sctp_ulpevent_is_notification(const struct sctp_ulpevent *event)
{
	return MSG_NOTIFICATION == (event->msg_flags & MSG_NOTIFICATION);
}

static inline void sctp_ulpevent_set_owner(struct sctp_ulpevent *event,
					   const struct sctp_association *asoc)
{
	struct sk_buff *skb;

	sctp_association_hold((struct sctp_association *)asoc);
	skb = sctp_event2skb(event);
	event->asoc = (struct sctp_association *)asoc;
	atomic_add(event->rmem_len, &event->asoc->rmem_alloc);
	sctp_skb_set_owner_r(skb, asoc->base.sk);
}

static inline void sctp_ulpevent_release_owner(struct sctp_ulpevent *event)
{
	struct sctp_association *asoc = event->asoc;

	atomic_sub(event->rmem_len, &asoc->rmem_alloc);
	sctp_association_put(asoc);
}

struct sctp_ulpevent  *sctp_ulpevent_make_assoc_change(
	const struct sctp_association *asoc,
	__u16 flags, __u16 state, __u16 error, __u16 outbound,
	__u16 inbound, struct sctp_chunk *chunk, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_assoc_change *sac;
	struct sk_buff *skb;

	if (chunk) {
		skb = skb_copy_expand(chunk->skb,
				      sizeof(struct sctp_assoc_change), 0, gfp);

		if (!skb)
			goto fail;

		
		event = sctp_skb2event(skb);
		sctp_ulpevent_init(event, MSG_NOTIFICATION, skb->truesize);

		
		sac = (struct sctp_assoc_change *)
			skb_push(skb, sizeof(struct sctp_assoc_change));

		
		skb_trim(skb, sizeof(struct sctp_assoc_change) +
			 ntohs(chunk->chunk_hdr->length) -
			 sizeof(sctp_chunkhdr_t));
	} else {
		event = sctp_ulpevent_new(sizeof(struct sctp_assoc_change),
				  MSG_NOTIFICATION, gfp);
		if (!event)
			goto fail;

		skb = sctp_event2skb(event);
		sac = (struct sctp_assoc_change *) skb_put(skb,
					sizeof(struct sctp_assoc_change));
	}

	sac->sac_type = SCTP_ASSOC_CHANGE;

	sac->sac_state = state;

	sac->sac_flags = 0;

	sac->sac_length = skb->len;

	sac->sac_error = error;

	sac->sac_outbound_streams = outbound;
	sac->sac_inbound_streams = inbound;

	sctp_ulpevent_set_owner(event, asoc);
	sac->sac_assoc_id = sctp_assoc2id(asoc);

	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_peer_addr_change(
	const struct sctp_association *asoc,
	const struct sockaddr_storage *aaddr,
	int flags, int state, int error, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_paddr_change  *spc;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_paddr_change),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		goto fail;

	skb = sctp_event2skb(event);
	spc = (struct sctp_paddr_change *)
		skb_put(skb, sizeof(struct sctp_paddr_change));

	spc->spc_type = SCTP_PEER_ADDR_CHANGE;

	spc->spc_length = sizeof(struct sctp_paddr_change);

	spc->spc_flags = 0;

	spc->spc_state = state;

	spc->spc_error = error;

	sctp_ulpevent_set_owner(event, asoc);
	spc->spc_assoc_id = sctp_assoc2id(asoc);

	memcpy(&spc->spc_aaddr, aaddr, sizeof(struct sockaddr_storage));

	
	sctp_get_pf_specific(asoc->base.sk->sk_family)->addr_v4map(
					sctp_sk(asoc->base.sk),
					(union sctp_addr *)&spc->spc_aaddr);

	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_remote_error(
	const struct sctp_association *asoc, struct sctp_chunk *chunk,
	__u16 flags, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_remote_error *sre;
	struct sk_buff *skb;
	sctp_errhdr_t *ch;
	__be16 cause;
	int elen;

	ch = (sctp_errhdr_t *)(chunk->skb->data);
	cause = ch->cause;
	elen = WORD_ROUND(ntohs(ch->length)) - sizeof(sctp_errhdr_t);

	
	skb_pull(chunk->skb, sizeof(sctp_errhdr_t));

	skb = skb_copy_expand(chunk->skb, sizeof(struct sctp_remote_error),
			      0, gfp);

	
	skb_pull(chunk->skb, elen);
	if (!skb)
		goto fail;

	
	event = sctp_skb2event(skb);
	sctp_ulpevent_init(event, MSG_NOTIFICATION, skb->truesize);

	sre = (struct sctp_remote_error *)
		skb_push(skb, sizeof(struct sctp_remote_error));

	
	skb_trim(skb, sizeof(struct sctp_remote_error) + elen);

	sre->sre_type = SCTP_REMOTE_ERROR;

	sre->sre_flags = 0;

	sre->sre_length = skb->len;

	sre->sre_error = cause;

	sctp_ulpevent_set_owner(event, asoc);
	sre->sre_assoc_id = sctp_assoc2id(asoc);

	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_send_failed(
	const struct sctp_association *asoc, struct sctp_chunk *chunk,
	__u16 flags, __u32 error, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_send_failed *ssf;
	struct sk_buff *skb;

	
	int len = ntohs(chunk->chunk_hdr->length);

	
	skb = skb_copy_expand(chunk->skb,
			      sizeof(struct sctp_send_failed), 
			      0,                               
			      gfp);
	if (!skb)
		goto fail;

	
	skb_pull(skb, sizeof(struct sctp_data_chunk));
	len -= sizeof(struct sctp_data_chunk);

	
	event = sctp_skb2event(skb);
	sctp_ulpevent_init(event, MSG_NOTIFICATION, skb->truesize);

	ssf = (struct sctp_send_failed *)
		skb_push(skb, sizeof(struct sctp_send_failed));

	ssf->ssf_type = SCTP_SEND_FAILED;

	ssf->ssf_flags = flags;

	ssf->ssf_length = sizeof(struct sctp_send_failed) + len;
	skb_trim(skb, ssf->ssf_length);

	ssf->ssf_error = error;

	memcpy(&ssf->ssf_info, &chunk->sinfo, sizeof(struct sctp_sndrcvinfo));

	ssf->ssf_info.sinfo_flags = chunk->chunk_hdr->flags;

	sctp_ulpevent_set_owner(event, asoc);
	ssf->ssf_assoc_id = sctp_assoc2id(asoc);
	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_shutdown_event(
	const struct sctp_association *asoc,
	__u16 flags, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_shutdown_event *sse;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_shutdown_event),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		goto fail;

	skb = sctp_event2skb(event);
	sse = (struct sctp_shutdown_event *)
		skb_put(skb, sizeof(struct sctp_shutdown_event));

	sse->sse_type = SCTP_SHUTDOWN_EVENT;

	sse->sse_flags = 0;

	sse->sse_length = sizeof(struct sctp_shutdown_event);

	sctp_ulpevent_set_owner(event, asoc);
	sse->sse_assoc_id = sctp_assoc2id(asoc);

	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_adaptation_indication(
	const struct sctp_association *asoc, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_adaptation_event *sai;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_adaptation_event),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		goto fail;

	skb = sctp_event2skb(event);
	sai = (struct sctp_adaptation_event *)
		skb_put(skb, sizeof(struct sctp_adaptation_event));

	sai->sai_type = SCTP_ADAPTATION_INDICATION;
	sai->sai_flags = 0;
	sai->sai_length = sizeof(struct sctp_adaptation_event);
	sai->sai_adaptation_ind = asoc->peer.adaptation_ind;
	sctp_ulpevent_set_owner(event, asoc);
	sai->sai_assoc_id = sctp_assoc2id(asoc);

	return event;

fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_rcvmsg(struct sctp_association *asoc,
						struct sctp_chunk *chunk,
						gfp_t gfp)
{
	struct sctp_ulpevent *event = NULL;
	struct sk_buff *skb;
	size_t padding, len;
	int rx_count;

	if (asoc->ep->rcvbuf_policy)
		rx_count = atomic_read(&asoc->rmem_alloc);
	else
		rx_count = atomic_read(&asoc->base.sk->sk_rmem_alloc);

	if (rx_count >= asoc->base.sk->sk_rcvbuf) {

		if ((asoc->base.sk->sk_userlocks & SOCK_RCVBUF_LOCK) ||
		    (!sk_rmem_schedule(asoc->base.sk, chunk->skb->truesize)))
			goto fail;
	}

	
	skb = skb_clone(chunk->skb, gfp);
	if (!skb)
		goto fail;

	if (sctp_tsnmap_mark(&asoc->peer.tsn_map,
			     ntohl(chunk->subh.data_hdr->tsn)))
		goto fail_mark;

	len = ntohs(chunk->chunk_hdr->length);
	padding = WORD_ROUND(len) - len;

	
	skb_trim(skb, chunk->chunk_end - padding - skb->data);

	
	event = sctp_skb2event(skb);

	sctp_ulpevent_init(event, 0, skb->len + sizeof(struct sk_buff));

	sctp_ulpevent_receive_data(event, asoc);

	event->stream = ntohs(chunk->subh.data_hdr->stream);
	event->ssn = ntohs(chunk->subh.data_hdr->ssn);
	event->ppid = chunk->subh.data_hdr->ppid;
	if (chunk->chunk_hdr->flags & SCTP_DATA_UNORDERED) {
		event->flags |= SCTP_UNORDERED;
		event->cumtsn = sctp_tsnmap_get_ctsn(&asoc->peer.tsn_map);
	}
	event->tsn = ntohl(chunk->subh.data_hdr->tsn);
	event->msg_flags |= chunk->chunk_hdr->flags;
	event->iif = sctp_chunk_iif(chunk);

	return event;

fail_mark:
	kfree_skb(skb);
fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_pdapi(
	const struct sctp_association *asoc, __u32 indication,
	gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_pdapi_event *pd;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_pdapi_event),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		goto fail;

	skb = sctp_event2skb(event);
	pd = (struct sctp_pdapi_event *)
		skb_put(skb, sizeof(struct sctp_pdapi_event));

	pd->pdapi_type = SCTP_PARTIAL_DELIVERY_EVENT;
	pd->pdapi_flags = 0;

	pd->pdapi_length = sizeof(struct sctp_pdapi_event);

	pd->pdapi_indication = indication;

	sctp_ulpevent_set_owner(event, asoc);
	pd->pdapi_assoc_id = sctp_assoc2id(asoc);

	return event;
fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_authkey(
	const struct sctp_association *asoc, __u16 key_id,
	__u32 indication, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_authkey_event *ak;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_authkey_event),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		goto fail;

	skb = sctp_event2skb(event);
	ak = (struct sctp_authkey_event *)
		skb_put(skb, sizeof(struct sctp_authkey_event));

	ak->auth_type = SCTP_AUTHENTICATION_EVENT;
	ak->auth_flags = 0;
	ak->auth_length = sizeof(struct sctp_authkey_event);

	ak->auth_keynumber = key_id;
	ak->auth_altkeynumber = 0;
	ak->auth_indication = indication;

	sctp_ulpevent_set_owner(event, asoc);
	ak->auth_assoc_id = sctp_assoc2id(asoc);

	return event;
fail:
	return NULL;
}

struct sctp_ulpevent *sctp_ulpevent_make_sender_dry_event(
	const struct sctp_association *asoc, gfp_t gfp)
{
	struct sctp_ulpevent *event;
	struct sctp_sender_dry_event *sdry;
	struct sk_buff *skb;

	event = sctp_ulpevent_new(sizeof(struct sctp_sender_dry_event),
				  MSG_NOTIFICATION, gfp);
	if (!event)
		return NULL;

	skb = sctp_event2skb(event);
	sdry = (struct sctp_sender_dry_event *)
		skb_put(skb, sizeof(struct sctp_sender_dry_event));

	sdry->sender_dry_type = SCTP_SENDER_DRY_EVENT;
	sdry->sender_dry_flags = 0;
	sdry->sender_dry_length = sizeof(struct sctp_sender_dry_event);
	sctp_ulpevent_set_owner(event, asoc);
	sdry->sender_dry_assoc_id = sctp_assoc2id(asoc);

	return event;
}

__u16 sctp_ulpevent_get_notification_type(const struct sctp_ulpevent *event)
{
	union sctp_notification *notification;
	struct sk_buff *skb;

	skb = sctp_event2skb(event);
	notification = (union sctp_notification *) skb->data;
	return notification->sn_header.sn_type;
}

void sctp_ulpevent_read_sndrcvinfo(const struct sctp_ulpevent *event,
				   struct msghdr *msghdr)
{
	struct sctp_sndrcvinfo sinfo;

	if (sctp_ulpevent_is_notification(event))
		return;

	sinfo.sinfo_stream = event->stream;
	sinfo.sinfo_ssn = event->ssn;
	sinfo.sinfo_ppid = event->ppid;
	sinfo.sinfo_flags = event->flags;
	sinfo.sinfo_tsn = event->tsn;
	sinfo.sinfo_cumtsn = event->cumtsn;
	sinfo.sinfo_assoc_id = sctp_assoc2id(event->asoc);

	
	sinfo.sinfo_context = event->asoc->default_rcv_context;

	
	sinfo.sinfo_timetolive = 0;

	put_cmsg(msghdr, IPPROTO_SCTP, SCTP_SNDRCV,
		 sizeof(struct sctp_sndrcvinfo), (void *)&sinfo);
}

static void sctp_ulpevent_receive_data(struct sctp_ulpevent *event,
				       struct sctp_association *asoc)
{
	struct sk_buff *skb, *frag;

	skb = sctp_event2skb(event);
	
	sctp_ulpevent_set_owner(event, asoc);
	sctp_assoc_rwnd_decrease(asoc, skb_headlen(skb));

	if (!skb->data_len)
		return;

	skb_walk_frags(skb, frag)
		sctp_ulpevent_receive_data(sctp_skb2event(frag), asoc);
}

static void sctp_ulpevent_release_data(struct sctp_ulpevent *event)
{
	struct sk_buff *skb, *frag;
	unsigned int	len;


	skb = sctp_event2skb(event);
	len = skb->len;

	if (!skb->data_len)
		goto done;

	
	skb_walk_frags(skb, frag) {
		sctp_ulpevent_release_frag_data(sctp_skb2event(frag));
	}

done:
	sctp_assoc_rwnd_increase(event->asoc, len);
	sctp_ulpevent_release_owner(event);
}

static void sctp_ulpevent_release_frag_data(struct sctp_ulpevent *event)
{
	struct sk_buff *skb, *frag;

	skb = sctp_event2skb(event);

	if (!skb->data_len)
		goto done;

	
	skb_walk_frags(skb, frag) {
		sctp_ulpevent_release_frag_data(sctp_skb2event(frag));
	}

done:
	sctp_ulpevent_release_owner(event);
}

void sctp_ulpevent_free(struct sctp_ulpevent *event)
{
	if (sctp_ulpevent_is_notification(event))
		sctp_ulpevent_release_owner(event);
	else
		sctp_ulpevent_release_data(event);

	kfree_skb(sctp_event2skb(event));
}

unsigned int sctp_queue_purge_ulpevents(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	unsigned int data_unread = 0;

	while ((skb = skb_dequeue(list)) != NULL) {
		struct sctp_ulpevent *event = sctp_skb2event(skb);

		if (!sctp_ulpevent_is_notification(event))
			data_unread += skb->len;

		sctp_ulpevent_free(event);
	}

	return data_unread;
}
