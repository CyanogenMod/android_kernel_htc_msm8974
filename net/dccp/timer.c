/*
 *  net/dccp/timer.c
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
#include <linux/export.h>

#include "dccp.h"

int  sysctl_dccp_request_retries	__read_mostly = TCP_SYN_RETRIES;
int  sysctl_dccp_retries1		__read_mostly = TCP_RETR1;
int  sysctl_dccp_retries2		__read_mostly = TCP_RETR2;

static void dccp_write_err(struct sock *sk)
{
	sk->sk_err = sk->sk_err_soft ? : ETIMEDOUT;
	sk->sk_error_report(sk);

	dccp_send_reset(sk, DCCP_RESET_CODE_ABORTED);
	dccp_done(sk);
	DCCP_INC_STATS_BH(DCCP_MIB_ABORTONTIMEOUT);
}

static int dccp_write_timeout(struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	int retry_until;

	if (sk->sk_state == DCCP_REQUESTING || sk->sk_state == DCCP_PARTOPEN) {
		if (icsk->icsk_retransmits != 0)
			dst_negative_advice(sk);
		retry_until = icsk->icsk_syn_retries ?
			    : sysctl_dccp_request_retries;
	} else {
		if (icsk->icsk_retransmits >= sysctl_dccp_retries1) {

			dst_negative_advice(sk);
		}

		retry_until = sysctl_dccp_retries2;
	}

	if (icsk->icsk_retransmits >= retry_until) {
		
		dccp_write_err(sk);
		return 1;
	}
	return 0;
}

static void dccp_retransmit_timer(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	if (dccp_write_timeout(sk))
		return;

	if (icsk->icsk_retransmits == 0)
		DCCP_INC_STATS_BH(DCCP_MIB_TIMEOUTS);

	if (dccp_retransmit_skb(sk) != 0) {
		if (--icsk->icsk_retransmits == 0)
			icsk->icsk_retransmits = 1;
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  min(icsk->icsk_rto,
					      TCP_RESOURCE_PROBE_INTERVAL),
					  DCCP_RTO_MAX);
		return;
	}

	icsk->icsk_backoff++;

	icsk->icsk_rto = min(icsk->icsk_rto << 1, DCCP_RTO_MAX);
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, icsk->icsk_rto,
				  DCCP_RTO_MAX);
	if (icsk->icsk_retransmits > sysctl_dccp_retries1)
		__sk_dst_reset(sk);
}

static void dccp_write_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct inet_connection_sock *icsk = inet_csk(sk);
	int event = 0;

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer,
			       jiffies + (HZ / 20));
		goto out;
	}

	if (sk->sk_state == DCCP_CLOSED || !icsk->icsk_pending)
		goto out;

	if (time_after(icsk->icsk_timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer,
			       icsk->icsk_timeout);
		goto out;
	}

	event = icsk->icsk_pending;
	icsk->icsk_pending = 0;

	switch (event) {
	case ICSK_TIME_RETRANS:
		dccp_retransmit_timer(sk);
		break;
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static void dccp_response_timer(struct sock *sk)
{
	inet_csk_reqsk_queue_prune(sk, TCP_SYNQ_INTERVAL, DCCP_TIMEOUT_INIT,
				   DCCP_RTO_MAX);
}

static void dccp_keepalive_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;

	
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		
		inet_csk_reset_keepalive_timer(sk, HZ / 20);
		goto out;
	}

	if (sk->sk_state == DCCP_LISTEN) {
		dccp_response_timer(sk);
		goto out;
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static void dccp_delack_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct inet_connection_sock *icsk = inet_csk(sk);

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		
		icsk->icsk_ack.blocked = 1;
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_DELAYEDACKLOCKED);
		sk_reset_timer(sk, &icsk->icsk_delack_timer,
			       jiffies + TCP_DELACK_MIN);
		goto out;
	}

	if (sk->sk_state == DCCP_CLOSED ||
	    !(icsk->icsk_ack.pending & ICSK_ACK_TIMER))
		goto out;
	if (time_after(icsk->icsk_ack.timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_delack_timer,
			       icsk->icsk_ack.timeout);
		goto out;
	}

	icsk->icsk_ack.pending &= ~ICSK_ACK_TIMER;

	if (inet_csk_ack_scheduled(sk)) {
		if (!icsk->icsk_ack.pingpong) {
			
			icsk->icsk_ack.ato = min(icsk->icsk_ack.ato << 1,
						 icsk->icsk_rto);
		} else {
			icsk->icsk_ack.pingpong = 0;
			icsk->icsk_ack.ato = TCP_ATO_MIN;
		}
		dccp_send_ack(sk);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_DELAYEDACKS);
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static void dccp_write_xmitlet(unsigned long data)
{
	struct sock *sk = (struct sock *)data;

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk))
		sk_reset_timer(sk, &dccp_sk(sk)->dccps_xmit_timer, jiffies + 1);
	else
		dccp_write_xmit(sk);
	bh_unlock_sock(sk);
}

static void dccp_write_xmit_timer(unsigned long data)
{
	dccp_write_xmitlet(data);
	sock_put((struct sock *)data);
}

void dccp_init_xmit_timers(struct sock *sk)
{
	struct dccp_sock *dp = dccp_sk(sk);

	tasklet_init(&dp->dccps_xmitlet, dccp_write_xmitlet, (unsigned long)sk);
	setup_timer(&dp->dccps_xmit_timer, dccp_write_xmit_timer,
							     (unsigned long)sk);
	inet_csk_init_xmit_timers(sk, &dccp_write_timer, &dccp_delack_timer,
				  &dccp_keepalive_timer);
}

static ktime_t dccp_timestamp_seed;
u32 dccp_timestamp(void)
{
	s64 delta = ktime_us_delta(ktime_get_real(), dccp_timestamp_seed);

	do_div(delta, 10);
	return delta;
}
EXPORT_SYMBOL_GPL(dccp_timestamp);

void __init dccp_timestamping_init(void)
{
	dccp_timestamp_seed = ktime_get_real();
}
