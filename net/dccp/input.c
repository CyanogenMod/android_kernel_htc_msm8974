/*
 *  net/dccp/input.c
 *
 *  An implementation of the DCCP protocol
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/dccp.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include <net/sock.h>

#include "ackvec.h"
#include "ccid.h"
#include "dccp.h"

int sysctl_dccp_sync_ratelimit	__read_mostly = HZ / 8;

static void dccp_enqueue_skb(struct sock *sk, struct sk_buff *skb)
{
	__skb_pull(skb, dccp_hdr(skb)->dccph_doff * 4);
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	skb_set_owner_r(skb, sk);
	sk->sk_data_ready(sk, 0);
}

static void dccp_fin(struct sock *sk, struct sk_buff *skb)
{
	sk->sk_shutdown = SHUTDOWN_MASK;
	sock_set_flag(sk, SOCK_DONE);
	dccp_enqueue_skb(sk, skb);
}

static int dccp_rcv_close(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;

	switch (sk->sk_state) {
	case DCCP_CLOSING:
		if (dccp_sk(sk)->dccps_role != DCCP_ROLE_CLIENT)
			break;
		
	case DCCP_REQUESTING:
	case DCCP_ACTIVE_CLOSEREQ:
		dccp_send_reset(sk, DCCP_RESET_CODE_CLOSED);
		dccp_done(sk);
		break;
	case DCCP_OPEN:
	case DCCP_PARTOPEN:
		
		queued = 1;
		dccp_fin(sk, skb);
		dccp_set_state(sk, DCCP_PASSIVE_CLOSE);
		
	case DCCP_PASSIVE_CLOSE:
		sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_HUP);
	}
	return queued;
}

static int dccp_rcv_closereq(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;

	if (dccp_sk(sk)->dccps_role != DCCP_ROLE_CLIENT) {
		dccp_send_sync(sk, DCCP_SKB_CB(skb)->dccpd_seq, DCCP_PKT_SYNC);
		return queued;
	}

	
	switch (sk->sk_state) {
	case DCCP_REQUESTING:
		dccp_send_close(sk, 0);
		dccp_set_state(sk, DCCP_CLOSING);
		break;
	case DCCP_OPEN:
	case DCCP_PARTOPEN:
		
		queued = 1;
		dccp_fin(sk, skb);
		dccp_set_state(sk, DCCP_PASSIVE_CLOSEREQ);
		
	case DCCP_PASSIVE_CLOSEREQ:
		sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_HUP);
	}
	return queued;
}

static u16 dccp_reset_code_convert(const u8 code)
{
	const u16 error_code[] = {
	[DCCP_RESET_CODE_CLOSED]	     = 0,	
	[DCCP_RESET_CODE_UNSPECIFIED]	     = 0,	
	[DCCP_RESET_CODE_ABORTED]	     = ECONNRESET,

	[DCCP_RESET_CODE_NO_CONNECTION]	     = ECONNREFUSED,
	[DCCP_RESET_CODE_CONNECTION_REFUSED] = ECONNREFUSED,
	[DCCP_RESET_CODE_TOO_BUSY]	     = EUSERS,
	[DCCP_RESET_CODE_AGGRESSION_PENALTY] = EDQUOT,

	[DCCP_RESET_CODE_PACKET_ERROR]	     = ENOMSG,
	[DCCP_RESET_CODE_BAD_INIT_COOKIE]    = EBADR,
	[DCCP_RESET_CODE_BAD_SERVICE_CODE]   = EBADRQC,
	[DCCP_RESET_CODE_OPTION_ERROR]	     = EILSEQ,
	[DCCP_RESET_CODE_MANDATORY_ERROR]    = EOPNOTSUPP,
	};

	return code >= DCCP_MAX_RESET_CODES ? 0 : error_code[code];
}

static void dccp_rcv_reset(struct sock *sk, struct sk_buff *skb)
{
	u16 err = dccp_reset_code_convert(dccp_hdr_reset(skb)->dccph_reset_code);

	sk->sk_err = err;

	
	dccp_fin(sk, skb);

	if (err && !sock_flag(sk, SOCK_DEAD))
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_ERR);
	dccp_time_wait(sk, DCCP_TIME_WAIT, 0);
}

static void dccp_handle_ackvec_processing(struct sock *sk, struct sk_buff *skb)
{
	struct dccp_ackvec *av = dccp_sk(sk)->dccps_hc_rx_ackvec;

	if (av == NULL)
		return;
	if (DCCP_SKB_CB(skb)->dccpd_ack_seq != DCCP_PKT_WITHOUT_ACK_SEQ)
		dccp_ackvec_clear_state(av, DCCP_SKB_CB(skb)->dccpd_ack_seq);
	dccp_ackvec_input(av, skb);
}

static void dccp_deliver_input_to_ccids(struct sock *sk, struct sk_buff *skb)
{
	const struct dccp_sock *dp = dccp_sk(sk);

	
	if (!(sk->sk_shutdown & RCV_SHUTDOWN))
		ccid_hc_rx_packet_recv(dp->dccps_hc_rx_ccid, sk, skb);
	if (sk->sk_write_queue.qlen > 0 || !(sk->sk_shutdown & SEND_SHUTDOWN))
		ccid_hc_tx_packet_recv(dp->dccps_hc_tx_ccid, sk, skb);
}

static int dccp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	const struct dccp_hdr *dh = dccp_hdr(skb);
	struct dccp_sock *dp = dccp_sk(sk);
	u64 lswl, lawl, seqno = DCCP_SKB_CB(skb)->dccpd_seq,
			ackno = DCCP_SKB_CB(skb)->dccpd_ack_seq;

	if (dh->dccph_type == DCCP_PKT_SYNC ||
	    dh->dccph_type == DCCP_PKT_SYNCACK) {
		if (between48(ackno, dp->dccps_awl, dp->dccps_awh) &&
		    dccp_delta_seqno(dp->dccps_swl, seqno) >= 0)
			dccp_update_gsr(sk, seqno);
		else
			return -1;
	}

	lswl = dp->dccps_swl;
	lawl = dp->dccps_awl;

	if (dh->dccph_type == DCCP_PKT_CLOSEREQ ||
	    dh->dccph_type == DCCP_PKT_CLOSE ||
	    dh->dccph_type == DCCP_PKT_RESET) {
		lswl = ADD48(dp->dccps_gsr, 1);
		lawl = dp->dccps_gar;
	}

	if (between48(seqno, lswl, dp->dccps_swh) &&
	    (ackno == DCCP_PKT_WITHOUT_ACK_SEQ ||
	     between48(ackno, lawl, dp->dccps_awh))) {
		dccp_update_gsr(sk, seqno);

		if (dh->dccph_type != DCCP_PKT_SYNC &&
		    ackno != DCCP_PKT_WITHOUT_ACK_SEQ &&
		    after48(ackno, dp->dccps_gar))
			dp->dccps_gar = ackno;
	} else {
		unsigned long now = jiffies;
		if (time_before(now, (dp->dccps_rate_last +
				      sysctl_dccp_sync_ratelimit)))
			return -1;

		DCCP_WARN("Step 6 failed for %s packet, "
			  "(LSWL(%llu) <= P.seqno(%llu) <= S.SWH(%llu)) and "
			  "(P.ackno %s or LAWL(%llu) <= P.ackno(%llu) <= S.AWH(%llu), "
			  "sending SYNC...\n",  dccp_packet_name(dh->dccph_type),
			  (unsigned long long) lswl, (unsigned long long) seqno,
			  (unsigned long long) dp->dccps_swh,
			  (ackno == DCCP_PKT_WITHOUT_ACK_SEQ) ? "doesn't exist"
							      : "exists",
			  (unsigned long long) lawl, (unsigned long long) ackno,
			  (unsigned long long) dp->dccps_awh);

		dp->dccps_rate_last = now;

		if (dh->dccph_type == DCCP_PKT_RESET)
			seqno = dp->dccps_gsr;
		dccp_send_sync(sk, seqno, DCCP_PKT_SYNC);
		return -1;
	}

	return 0;
}

static int __dccp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct dccp_hdr *dh, const unsigned len)
{
	struct dccp_sock *dp = dccp_sk(sk);

	switch (dccp_hdr(skb)->dccph_type) {
	case DCCP_PKT_DATAACK:
	case DCCP_PKT_DATA:
		dccp_enqueue_skb(sk, skb);
		return 0;
	case DCCP_PKT_ACK:
		goto discard;
	case DCCP_PKT_RESET:
		dccp_rcv_reset(sk, skb);
		return 0;
	case DCCP_PKT_CLOSEREQ:
		if (dccp_rcv_closereq(sk, skb))
			return 0;
		goto discard;
	case DCCP_PKT_CLOSE:
		if (dccp_rcv_close(sk, skb))
			return 0;
		goto discard;
	case DCCP_PKT_REQUEST:
		if (dp->dccps_role != DCCP_ROLE_LISTEN)
			goto send_sync;
		goto check_seq;
	case DCCP_PKT_RESPONSE:
		if (dp->dccps_role != DCCP_ROLE_CLIENT)
			goto send_sync;
check_seq:
		if (dccp_delta_seqno(dp->dccps_osr,
				     DCCP_SKB_CB(skb)->dccpd_seq) >= 0) {
send_sync:
			dccp_send_sync(sk, DCCP_SKB_CB(skb)->dccpd_seq,
				       DCCP_PKT_SYNC);
		}
		break;
	case DCCP_PKT_SYNC:
		dccp_send_sync(sk, DCCP_SKB_CB(skb)->dccpd_seq,
			       DCCP_PKT_SYNCACK);
		goto discard;
	}

	DCCP_INC_STATS_BH(DCCP_MIB_INERRS);
discard:
	__kfree_skb(skb);
	return 0;
}

int dccp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct dccp_hdr *dh, const unsigned len)
{
	if (dccp_check_seqno(sk, skb))
		goto discard;

	if (dccp_parse_options(sk, NULL, skb))
		return 1;

	dccp_handle_ackvec_processing(sk, skb);
	dccp_deliver_input_to_ccids(sk, skb);

	return __dccp_rcv_established(sk, skb, dh, len);
discard:
	__kfree_skb(skb);
	return 0;
}

EXPORT_SYMBOL_GPL(dccp_rcv_established);

static int dccp_rcv_request_sent_state_process(struct sock *sk,
					       struct sk_buff *skb,
					       const struct dccp_hdr *dh,
					       const unsigned len)
{
	if (dh->dccph_type == DCCP_PKT_RESPONSE) {
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct dccp_sock *dp = dccp_sk(sk);
		long tstamp = dccp_timestamp();

		if (!between48(DCCP_SKB_CB(skb)->dccpd_ack_seq,
			       dp->dccps_awl, dp->dccps_awh)) {
			dccp_pr_debug("invalid ackno: S.AWL=%llu, "
				      "P.ackno=%llu, S.AWH=%llu\n",
				      (unsigned long long)dp->dccps_awl,
			   (unsigned long long)DCCP_SKB_CB(skb)->dccpd_ack_seq,
				      (unsigned long long)dp->dccps_awh);
			goto out_invalid_packet;
		}

		if (dccp_parse_options(sk, NULL, skb))
			return 1;

		
		if (likely(dp->dccps_options_received.dccpor_timestamp_echo))
			dp->dccps_syn_rtt = dccp_sample_rtt(sk, 10 * (tstamp -
			    dp->dccps_options_received.dccpor_timestamp_echo));

		
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
		WARN_ON(sk->sk_send_head == NULL);
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;

		dp->dccps_gsr = dp->dccps_isr = DCCP_SKB_CB(skb)->dccpd_seq;

		dccp_sync_mss(sk, icsk->icsk_pmtu_cookie);

		dccp_set_state(sk, DCCP_PARTOPEN);

		if (dccp_feat_activate_values(sk, &dp->dccps_featneg))
			goto unable_to_proceed;

		
		icsk->icsk_af_ops->rebuild_header(sk);

		if (!sock_flag(sk, SOCK_DEAD)) {
			sk->sk_state_change(sk);
			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
		}

		if (sk->sk_write_pending || icsk->icsk_ack.pingpong ||
		    icsk->icsk_accept_queue.rskq_defer_accept) {
			__kfree_skb(skb);
			return 0;
		}
		dccp_send_ack(sk);
		return -1;
	}

out_invalid_packet:
	
	DCCP_SKB_CB(skb)->dccpd_reset_code = DCCP_RESET_CODE_PACKET_ERROR;
	return 1;

unable_to_proceed:
	DCCP_SKB_CB(skb)->dccpd_reset_code = DCCP_RESET_CODE_ABORTED;
	dccp_set_state(sk, DCCP_CLOSED);
	sk->sk_err = ECOMM;
	return 1;
}

static int dccp_rcv_respond_partopen_state_process(struct sock *sk,
						   struct sk_buff *skb,
						   const struct dccp_hdr *dh,
						   const unsigned len)
{
	struct dccp_sock *dp = dccp_sk(sk);
	u32 sample = dp->dccps_options_received.dccpor_timestamp_echo;
	int queued = 0;

	switch (dh->dccph_type) {
	case DCCP_PKT_RESET:
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
		break;
	case DCCP_PKT_DATA:
		if (sk->sk_state == DCCP_RESPOND)
			break;
	case DCCP_PKT_DATAACK:
	case DCCP_PKT_ACK:

		
		if (sk->sk_state == DCCP_PARTOPEN)
			inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);

		
		if (likely(sample)) {
			long delta = dccp_timestamp() - sample;

			dp->dccps_syn_rtt = dccp_sample_rtt(sk, 10 * delta);
		}

		dp->dccps_osr = DCCP_SKB_CB(skb)->dccpd_seq;
		dccp_set_state(sk, DCCP_OPEN);

		if (dh->dccph_type == DCCP_PKT_DATAACK ||
		    dh->dccph_type == DCCP_PKT_DATA) {
			__dccp_rcv_established(sk, skb, dh, len);
			queued = 1; 
		}
		break;
	}

	return queued;
}

int dccp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
			   struct dccp_hdr *dh, unsigned len)
{
	struct dccp_sock *dp = dccp_sk(sk);
	struct dccp_skb_cb *dcb = DCCP_SKB_CB(skb);
	const int old_state = sk->sk_state;
	int queued = 0;

	if (sk->sk_state == DCCP_LISTEN) {
		if (dh->dccph_type == DCCP_PKT_REQUEST) {
			if (inet_csk(sk)->icsk_af_ops->conn_request(sk,
								    skb) < 0)
				return 1;
			goto discard;
		}
		if (dh->dccph_type == DCCP_PKT_RESET)
			goto discard;

		
		dcb->dccpd_reset_code = DCCP_RESET_CODE_NO_CONNECTION;
		return 1;
	} else if (sk->sk_state == DCCP_CLOSED) {
		dcb->dccpd_reset_code = DCCP_RESET_CODE_NO_CONNECTION;
		return 1;
	}

	
	if (sk->sk_state != DCCP_REQUESTING && dccp_check_seqno(sk, skb))
		goto discard;

	if ((dp->dccps_role != DCCP_ROLE_CLIENT &&
	     dh->dccph_type == DCCP_PKT_RESPONSE) ||
	    (dp->dccps_role == DCCP_ROLE_CLIENT &&
	     dh->dccph_type == DCCP_PKT_REQUEST) ||
	    (sk->sk_state == DCCP_RESPOND && dh->dccph_type == DCCP_PKT_DATA)) {
		dccp_send_sync(sk, dcb->dccpd_seq, DCCP_PKT_SYNC);
		goto discard;
	}

	
	if (dccp_parse_options(sk, NULL, skb))
		return 1;

	if (dh->dccph_type == DCCP_PKT_RESET) {
		dccp_rcv_reset(sk, skb);
		return 0;
	} else if (dh->dccph_type == DCCP_PKT_CLOSEREQ) {	
		if (dccp_rcv_closereq(sk, skb))
			return 0;
		goto discard;
	} else if (dh->dccph_type == DCCP_PKT_CLOSE) {		
		if (dccp_rcv_close(sk, skb))
			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case DCCP_REQUESTING:
		queued = dccp_rcv_request_sent_state_process(sk, skb, dh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

	case DCCP_PARTOPEN:
		
		dccp_handle_ackvec_processing(sk, skb);
		dccp_deliver_input_to_ccids(sk, skb);
		
	case DCCP_RESPOND:
		queued = dccp_rcv_respond_partopen_state_process(sk, skb,
								 dh, len);
		break;
	}

	if (dh->dccph_type == DCCP_PKT_ACK ||
	    dh->dccph_type == DCCP_PKT_DATAACK) {
		switch (old_state) {
		case DCCP_PARTOPEN:
			sk->sk_state_change(sk);
			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
			break;
		}
	} else if (unlikely(dh->dccph_type == DCCP_PKT_SYNC)) {
		dccp_send_sync(sk, dcb->dccpd_seq, DCCP_PKT_SYNCACK);
		goto discard;
	}

	if (!queued) {
discard:
		__kfree_skb(skb);
	}
	return 0;
}

EXPORT_SYMBOL_GPL(dccp_rcv_state_process);

u32 dccp_sample_rtt(struct sock *sk, long delta)
{
	
	delta -= dccp_sk(sk)->dccps_options_received.dccpor_elapsed_time * 10;

	if (unlikely(delta <= 0)) {
		DCCP_WARN("unusable RTT sample %ld, using min\n", delta);
		return DCCP_SANE_RTT_MIN;
	}
	if (unlikely(delta > DCCP_SANE_RTT_MAX)) {
		DCCP_WARN("RTT sample %ld too large, using max\n", delta);
		return DCCP_SANE_RTT_MAX;
	}

	return delta;
}
