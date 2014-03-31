/*
 *  Copyright (c) 2007   The University of Aberdeen, Scotland, UK
 *  Copyright (c) 2005-7 The University of Waikato, Hamilton, New Zealand.
 *  Copyright (c) 2005-7 Ian McDonald <ian.mcdonald@jandi.co.nz>
 *
 *  An implementation of the DCCP protocol
 *
 *  This code has been developed by the University of Waikato WAND
 *  research group. For further information please see http://www.wand.net.nz/
 *
 *  This code also uses code from Lulea University, rereleased as GPL by its
 *  authors:
 *  Copyright (c) 2003 Nils-Erik Mattsson, Joacim Haggmark, Magnus Erixzon
 *
 *  Changes to meet Linux coding standards, to make it meet latest ccid3 draft
 *  and to make it work as a loadable module in the DCCP stack written by
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>.
 *
 *  Copyright (c) 2005 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "../dccp.h"
#include "ccid3.h"

#include <asm/unaligned.h>

#ifdef CONFIG_IP_DCCP_CCID3_DEBUG
static bool ccid3_debug;
#define ccid3_pr_debug(format, a...)	DCCP_PR_DEBUG(ccid3_debug, format, ##a)
#else
#define ccid3_pr_debug(format, a...)
#endif

#ifdef CONFIG_IP_DCCP_CCID3_DEBUG
static const char *ccid3_tx_state_name(enum ccid3_hc_tx_states state)
{
	static const char *const ccid3_state_names[] = {
	[TFRC_SSTATE_NO_SENT]  = "NO_SENT",
	[TFRC_SSTATE_NO_FBACK] = "NO_FBACK",
	[TFRC_SSTATE_FBACK]    = "FBACK",
	};

	return ccid3_state_names[state];
}
#endif

static void ccid3_hc_tx_set_state(struct sock *sk,
				  enum ccid3_hc_tx_states state)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	enum ccid3_hc_tx_states oldstate = hc->tx_state;

	ccid3_pr_debug("%s(%p) %-8.8s -> %s\n",
		       dccp_role(sk), sk, ccid3_tx_state_name(oldstate),
		       ccid3_tx_state_name(state));
	WARN_ON(state == oldstate);
	hc->tx_state = state;
}

static inline u64 rfc3390_initial_rate(struct sock *sk)
{
	const struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	const __u32 w_init = clamp_t(__u32, 4380U, 2 * hc->tx_s, 4 * hc->tx_s);

	return scaled_div(w_init << 6, hc->tx_rtt);
}

static void ccid3_update_send_interval(struct ccid3_hc_tx_sock *hc)
{
	hc->tx_t_ipi = scaled_div32(((u64)hc->tx_s) << 6, hc->tx_x);

	DCCP_BUG_ON(hc->tx_t_ipi == 0);
	ccid3_pr_debug("t_ipi=%u, s=%u, X=%u\n", hc->tx_t_ipi,
		       hc->tx_s, (unsigned)(hc->tx_x >> 6));
}

static u32 ccid3_hc_tx_idle_rtt(struct ccid3_hc_tx_sock *hc, ktime_t now)
{
	u32 delta = ktime_us_delta(now, hc->tx_t_last_win_count);

	return delta / hc->tx_rtt;
}

static void ccid3_hc_tx_update_x(struct sock *sk, ktime_t *stamp)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	__u64 min_rate = 2 * hc->tx_x_recv;
	const __u64 old_x = hc->tx_x;
	ktime_t now = stamp ? *stamp : ktime_get_real();

	if (ccid3_hc_tx_idle_rtt(hc, now) >= 2) {
		min_rate = rfc3390_initial_rate(sk);
		min_rate = max(min_rate, 2 * hc->tx_x_recv);
	}

	if (hc->tx_p > 0) {

		hc->tx_x = min(((__u64)hc->tx_x_calc) << 6, min_rate);
		hc->tx_x = max(hc->tx_x, (((__u64)hc->tx_s) << 6) / TFRC_T_MBI);

	} else if (ktime_us_delta(now, hc->tx_t_ld) - (s64)hc->tx_rtt >= 0) {

		hc->tx_x = min(2 * hc->tx_x, min_rate);
		hc->tx_x = max(hc->tx_x,
			       scaled_div(((__u64)hc->tx_s) << 6, hc->tx_rtt));
		hc->tx_t_ld = now;
	}

	if (hc->tx_x != old_x) {
		ccid3_pr_debug("X_prev=%u, X_now=%u, X_calc=%u, "
			       "X_recv=%u\n", (unsigned)(old_x >> 6),
			       (unsigned)(hc->tx_x >> 6), hc->tx_x_calc,
			       (unsigned)(hc->tx_x_recv >> 6));

		ccid3_update_send_interval(hc);
	}
}

static inline void ccid3_hc_tx_update_s(struct ccid3_hc_tx_sock *hc, int len)
{
	const u16 old_s = hc->tx_s;

	hc->tx_s = tfrc_ewma(hc->tx_s, len, 9);

	if (hc->tx_s != old_s)
		ccid3_update_send_interval(hc);
}

static inline void ccid3_hc_tx_update_win_count(struct ccid3_hc_tx_sock *hc,
						ktime_t now)
{
	u32 delta = ktime_us_delta(now, hc->tx_t_last_win_count),
	    quarter_rtts = (4 * delta) / hc->tx_rtt;

	if (quarter_rtts > 0) {
		hc->tx_t_last_win_count = now;
		hc->tx_last_win_count  += min(quarter_rtts, 5U);
		hc->tx_last_win_count  &= 0xF;		
	}
}

static void ccid3_hc_tx_no_feedback_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	unsigned long t_nfb = USEC_PER_SEC / 5;

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		
		
		goto restart_timer;
	}

	ccid3_pr_debug("%s(%p, state=%s) - entry\n", dccp_role(sk), sk,
		       ccid3_tx_state_name(hc->tx_state));

	
	if ((1 << sk->sk_state) & ~(DCCPF_OPEN | DCCPF_PARTOPEN))
		goto out;

	
	if (hc->tx_state == TFRC_SSTATE_FBACK)
		ccid3_hc_tx_set_state(sk, TFRC_SSTATE_NO_FBACK);

	if (hc->tx_t_rto == 0 || hc->tx_p == 0) {

		
		hc->tx_x = max(hc->tx_x / 2,
			       (((__u64)hc->tx_s) << 6) / TFRC_T_MBI);
		ccid3_update_send_interval(hc);
	} else {
		if (hc->tx_x_calc > (hc->tx_x_recv >> 5))
			hc->tx_x_recv =
				max(hc->tx_x_recv / 2,
				    (((__u64)hc->tx_s) << 6) / (2*TFRC_T_MBI));
		else {
			hc->tx_x_recv = hc->tx_x_calc;
			hc->tx_x_recv <<= 4;
		}
		ccid3_hc_tx_update_x(sk, NULL);
	}
	ccid3_pr_debug("Reduced X to %llu/64 bytes/sec\n",
			(unsigned long long)hc->tx_x);

	if (unlikely(hc->tx_t_rto == 0))	
		t_nfb = TFRC_INITIAL_TIMEOUT;
	else
		t_nfb = max(hc->tx_t_rto, 2 * hc->tx_t_ipi);

restart_timer:
	sk_reset_timer(sk, &hc->tx_no_feedback_timer,
			   jiffies + usecs_to_jiffies(t_nfb));
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static int ccid3_hc_tx_send_packet(struct sock *sk, struct sk_buff *skb)
{
	struct dccp_sock *dp = dccp_sk(sk);
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	ktime_t now = ktime_get_real();
	s64 delay;

	if (unlikely(skb->len == 0))
		return -EBADMSG;

	if (hc->tx_state == TFRC_SSTATE_NO_SENT) {
		sk_reset_timer(sk, &hc->tx_no_feedback_timer, (jiffies +
			       usecs_to_jiffies(TFRC_INITIAL_TIMEOUT)));
		hc->tx_last_win_count	= 0;
		hc->tx_t_last_win_count = now;

		
		hc->tx_t_nom = now;

		hc->tx_s = skb->len;

		if (dp->dccps_syn_rtt) {
			ccid3_pr_debug("SYN RTT = %uus\n", dp->dccps_syn_rtt);
			hc->tx_rtt  = dp->dccps_syn_rtt;
			hc->tx_x    = rfc3390_initial_rate(sk);
			hc->tx_t_ld = now;
		} else {
			hc->tx_rtt = DCCP_FALLBACK_RTT;
			hc->tx_x   = hc->tx_s;
			hc->tx_x <<= 6;
		}
		ccid3_update_send_interval(hc);

		ccid3_hc_tx_set_state(sk, TFRC_SSTATE_NO_FBACK);

	} else {
		delay = ktime_us_delta(hc->tx_t_nom, now);
		ccid3_pr_debug("delay=%ld\n", (long)delay);
		if (delay >= TFRC_T_DELTA)
			return (u32)delay / USEC_PER_MSEC;

		ccid3_hc_tx_update_win_count(hc, now);
	}

	
	dp->dccps_hc_tx_insert_options = 1;
	DCCP_SKB_CB(skb)->dccpd_ccval  = hc->tx_last_win_count;

	
	hc->tx_t_nom = ktime_add_us(hc->tx_t_nom, hc->tx_t_ipi);
	return CCID_PACKET_SEND_AT_ONCE;
}

static void ccid3_hc_tx_packet_sent(struct sock *sk, unsigned int len)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);

	ccid3_hc_tx_update_s(hc, len);

	if (tfrc_tx_hist_add(&hc->tx_hist, dccp_sk(sk)->dccps_gss))
		DCCP_CRIT("packet history - out of memory!");
}

static void ccid3_hc_tx_packet_recv(struct sock *sk, struct sk_buff *skb)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	struct tfrc_tx_hist_entry *acked;
	ktime_t now;
	unsigned long t_nfb;
	u32 r_sample;

	
	if (!(DCCP_SKB_CB(skb)->dccpd_type == DCCP_PKT_ACK ||
	      DCCP_SKB_CB(skb)->dccpd_type == DCCP_PKT_DATAACK))
		return;
	acked = tfrc_tx_hist_find_entry(hc->tx_hist, dccp_hdr_ack_seq(skb));
	if (acked == NULL)
		return;
	
	tfrc_tx_hist_purge(&acked->next);

	
	now	  = ktime_get_real();
	r_sample  = dccp_sample_rtt(sk, ktime_us_delta(now, acked->stamp));
	hc->tx_rtt = tfrc_ewma(hc->tx_rtt, r_sample, 9);

	if (hc->tx_state == TFRC_SSTATE_NO_FBACK) {
		ccid3_hc_tx_set_state(sk, TFRC_SSTATE_FBACK);

		if (hc->tx_t_rto == 0) {
			hc->tx_x    = rfc3390_initial_rate(sk);
			hc->tx_t_ld = now;

			ccid3_update_send_interval(hc);

			goto done_computing_x;
		} else if (hc->tx_p == 0) {
			goto done_computing_x;
		}
	}

	
	if (hc->tx_p > 0)
		hc->tx_x_calc = tfrc_calc_x(hc->tx_s, hc->tx_rtt, hc->tx_p);
	ccid3_hc_tx_update_x(sk, &now);

done_computing_x:
	ccid3_pr_debug("%s(%p), RTT=%uus (sample=%uus), s=%u, "
			       "p=%u, X_calc=%u, X_recv=%u, X=%u\n",
			       dccp_role(sk), sk, hc->tx_rtt, r_sample,
			       hc->tx_s, hc->tx_p, hc->tx_x_calc,
			       (unsigned)(hc->tx_x_recv >> 6),
			       (unsigned)(hc->tx_x >> 6));

	
	sk_stop_timer(sk, &hc->tx_no_feedback_timer);

	sk->sk_write_space(sk);

	hc->tx_t_rto = max_t(u32, 4 * hc->tx_rtt,
				  USEC_PER_SEC/HZ * tcp_rto_min(sk));
	t_nfb = max(hc->tx_t_rto, 2 * hc->tx_t_ipi);

	ccid3_pr_debug("%s(%p), Scheduled no feedback timer to "
		       "expire in %lu jiffies (%luus)\n",
		       dccp_role(sk), sk, usecs_to_jiffies(t_nfb), t_nfb);

	sk_reset_timer(sk, &hc->tx_no_feedback_timer,
			   jiffies + usecs_to_jiffies(t_nfb));
}

static int ccid3_hc_tx_parse_options(struct sock *sk, u8 packet_type,
				     u8 option, u8 *optval, u8 optlen)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	__be32 opt_val;

	switch (option) {
	case TFRC_OPT_RECEIVE_RATE:
	case TFRC_OPT_LOSS_EVENT_RATE:
		
		if (packet_type == DCCP_PKT_DATA)
			break;
		if (unlikely(optlen != 4)) {
			DCCP_WARN("%s(%p), invalid len %d for %u\n",
				  dccp_role(sk), sk, optlen, option);
			return -EINVAL;
		}
		opt_val = ntohl(get_unaligned((__be32 *)optval));

		if (option == TFRC_OPT_RECEIVE_RATE) {
			
			hc->tx_x_recv = opt_val;
			hc->tx_x_recv <<= 6;

			ccid3_pr_debug("%s(%p), RECEIVE_RATE=%u\n",
				       dccp_role(sk), sk, opt_val);
		} else {
			
			hc->tx_p = tfrc_invert_loss_event_rate(opt_val);

			ccid3_pr_debug("%s(%p), LOSS_EVENT_RATE=%u\n",
				       dccp_role(sk), sk, opt_val);
		}
	}
	return 0;
}

static int ccid3_hc_tx_init(struct ccid *ccid, struct sock *sk)
{
	struct ccid3_hc_tx_sock *hc = ccid_priv(ccid);

	hc->tx_state = TFRC_SSTATE_NO_SENT;
	hc->tx_hist  = NULL;
	setup_timer(&hc->tx_no_feedback_timer,
			ccid3_hc_tx_no_feedback_timer, (unsigned long)sk);
	return 0;
}

static void ccid3_hc_tx_exit(struct sock *sk)
{
	struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);

	sk_stop_timer(sk, &hc->tx_no_feedback_timer);
	tfrc_tx_hist_purge(&hc->tx_hist);
}

static void ccid3_hc_tx_get_info(struct sock *sk, struct tcp_info *info)
{
	info->tcpi_rto = ccid3_hc_tx_sk(sk)->tx_t_rto;
	info->tcpi_rtt = ccid3_hc_tx_sk(sk)->tx_rtt;
}

static int ccid3_hc_tx_getsockopt(struct sock *sk, const int optname, int len,
				  u32 __user *optval, int __user *optlen)
{
	const struct ccid3_hc_tx_sock *hc = ccid3_hc_tx_sk(sk);
	struct tfrc_tx_info tfrc;
	const void *val;

	switch (optname) {
	case DCCP_SOCKOPT_CCID_TX_INFO:
		if (len < sizeof(tfrc))
			return -EINVAL;
		tfrc.tfrctx_x	   = hc->tx_x;
		tfrc.tfrctx_x_recv = hc->tx_x_recv;
		tfrc.tfrctx_x_calc = hc->tx_x_calc;
		tfrc.tfrctx_rtt	   = hc->tx_rtt;
		tfrc.tfrctx_p	   = hc->tx_p;
		tfrc.tfrctx_rto	   = hc->tx_t_rto;
		tfrc.tfrctx_ipi	   = hc->tx_t_ipi;
		len = sizeof(tfrc);
		val = &tfrc;
		break;
	default:
		return -ENOPROTOOPT;
	}

	if (put_user(len, optlen) || copy_to_user(optval, val, len))
		return -EFAULT;

	return 0;
}


enum ccid3_fback_type {
	CCID3_FBACK_NONE = 0,
	CCID3_FBACK_INITIAL,
	CCID3_FBACK_PERIODIC,
	CCID3_FBACK_PARAM_CHANGE
};

#ifdef CONFIG_IP_DCCP_CCID3_DEBUG
static const char *ccid3_rx_state_name(enum ccid3_hc_rx_states state)
{
	static const char *const ccid3_rx_state_names[] = {
	[TFRC_RSTATE_NO_DATA] = "NO_DATA",
	[TFRC_RSTATE_DATA]    = "DATA",
	};

	return ccid3_rx_state_names[state];
}
#endif

static void ccid3_hc_rx_set_state(struct sock *sk,
				  enum ccid3_hc_rx_states state)
{
	struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	enum ccid3_hc_rx_states oldstate = hc->rx_state;

	ccid3_pr_debug("%s(%p) %-8.8s -> %s\n",
		       dccp_role(sk), sk, ccid3_rx_state_name(oldstate),
		       ccid3_rx_state_name(state));
	WARN_ON(state == oldstate);
	hc->rx_state = state;
}

static void ccid3_hc_rx_send_feedback(struct sock *sk,
				      const struct sk_buff *skb,
				      enum ccid3_fback_type fbtype)
{
	struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	struct dccp_sock *dp = dccp_sk(sk);
	ktime_t now = ktime_get_real();
	s64 delta = 0;

	switch (fbtype) {
	case CCID3_FBACK_INITIAL:
		hc->rx_x_recv = 0;
		hc->rx_pinv   = ~0U;   
		break;
	case CCID3_FBACK_PARAM_CHANGE:
		if (hc->rx_x_recv > 0)
			break;
		
	case CCID3_FBACK_PERIODIC:
		delta = ktime_us_delta(now, hc->rx_tstamp_last_feedback);
		if (delta <= 0)
			DCCP_BUG("delta (%ld) <= 0", (long)delta);
		else
			hc->rx_x_recv = scaled_div32(hc->rx_bytes_recv, delta);
		break;
	default:
		return;
	}

	ccid3_pr_debug("Interval %ldusec, X_recv=%u, 1/p=%u\n", (long)delta,
		       hc->rx_x_recv, hc->rx_pinv);

	hc->rx_tstamp_last_feedback = now;
	hc->rx_last_counter	    = dccp_hdr(skb)->dccph_ccval;
	hc->rx_bytes_recv	    = 0;

	dp->dccps_hc_rx_insert_options = 1;
	dccp_send_ack(sk);
}

static int ccid3_hc_rx_insert_options(struct sock *sk, struct sk_buff *skb)
{
	const struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	__be32 x_recv, pinv;

	if (!(sk->sk_state == DCCP_OPEN || sk->sk_state == DCCP_PARTOPEN))
		return 0;

	if (dccp_packet_without_ack(skb))
		return 0;

	x_recv = htonl(hc->rx_x_recv);
	pinv   = htonl(hc->rx_pinv);

	if (dccp_insert_option(skb, TFRC_OPT_LOSS_EVENT_RATE,
			       &pinv, sizeof(pinv)) ||
	    dccp_insert_option(skb, TFRC_OPT_RECEIVE_RATE,
			       &x_recv, sizeof(x_recv)))
		return -1;

	return 0;
}

static u32 ccid3_first_li(struct sock *sk)
{
	struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	u32 x_recv, p, delta;
	u64 fval;

	if (hc->rx_rtt == 0) {
		DCCP_WARN("No RTT estimate available, using fallback RTT\n");
		hc->rx_rtt = DCCP_FALLBACK_RTT;
	}

	delta  = ktime_to_us(net_timedelta(hc->rx_tstamp_last_feedback));
	x_recv = scaled_div32(hc->rx_bytes_recv, delta);
	if (x_recv == 0) {		
		DCCP_WARN("X_recv==0\n");
		if (hc->rx_x_recv == 0) {
			DCCP_BUG("stored value of X_recv is zero");
			return ~0U;
		}
		x_recv = hc->rx_x_recv;
	}

	fval = scaled_div(hc->rx_s, hc->rx_rtt);
	fval = scaled_div32(fval, x_recv);
	p = tfrc_calc_x_reverse_lookup(fval);

	ccid3_pr_debug("%s(%p), receive rate=%u bytes/s, implied "
		       "loss rate=%u\n", dccp_role(sk), sk, x_recv, p);

	return p == 0 ? ~0U : scaled_div(1, p);
}

static void ccid3_hc_rx_packet_recv(struct sock *sk, struct sk_buff *skb)
{
	struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	enum ccid3_fback_type do_feedback = CCID3_FBACK_NONE;
	const u64 ndp = dccp_sk(sk)->dccps_options_received.dccpor_ndp;
	const bool is_data_packet = dccp_data_packet(skb);

	if (unlikely(hc->rx_state == TFRC_RSTATE_NO_DATA)) {
		if (is_data_packet) {
			const u32 payload = skb->len - dccp_hdr(skb)->dccph_doff * 4;
			do_feedback = CCID3_FBACK_INITIAL;
			ccid3_hc_rx_set_state(sk, TFRC_RSTATE_DATA);
			hc->rx_s = payload;
		}
		goto update_records;
	}

	if (tfrc_rx_hist_duplicate(&hc->rx_hist, skb))
		return; 

	if (is_data_packet) {
		const u32 payload = skb->len - dccp_hdr(skb)->dccph_doff * 4;
		hc->rx_s = tfrc_ewma(hc->rx_s, payload, 9);
		hc->rx_bytes_recv += payload;
	}

	if (tfrc_rx_handle_loss(&hc->rx_hist, &hc->rx_li_hist,
				skb, ndp, ccid3_first_li, sk)) {
		do_feedback = CCID3_FBACK_PARAM_CHANGE;
		goto done_receiving;
	}

	if (tfrc_rx_hist_loss_pending(&hc->rx_hist))
		return; 

	if (unlikely(!is_data_packet))
		goto update_records;

	if (!tfrc_lh_is_initialised(&hc->rx_li_hist)) {
		const u32 sample = tfrc_rx_hist_sample_rtt(&hc->rx_hist, skb);
		if (sample != 0)
			hc->rx_rtt = tfrc_ewma(hc->rx_rtt, sample, 9);

	} else if (tfrc_lh_update_i_mean(&hc->rx_li_hist, skb)) {
		do_feedback = CCID3_FBACK_PARAM_CHANGE;
	}

	if (SUB16(dccp_hdr(skb)->dccph_ccval, hc->rx_last_counter) > 3)
		do_feedback = CCID3_FBACK_PERIODIC;

update_records:
	tfrc_rx_hist_add_packet(&hc->rx_hist, skb, ndp);

done_receiving:
	if (do_feedback)
		ccid3_hc_rx_send_feedback(sk, skb, do_feedback);
}

static int ccid3_hc_rx_init(struct ccid *ccid, struct sock *sk)
{
	struct ccid3_hc_rx_sock *hc = ccid_priv(ccid);

	hc->rx_state = TFRC_RSTATE_NO_DATA;
	tfrc_lh_init(&hc->rx_li_hist);
	return tfrc_rx_hist_alloc(&hc->rx_hist);
}

static void ccid3_hc_rx_exit(struct sock *sk)
{
	struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);

	tfrc_rx_hist_purge(&hc->rx_hist);
	tfrc_lh_cleanup(&hc->rx_li_hist);
}

static void ccid3_hc_rx_get_info(struct sock *sk, struct tcp_info *info)
{
	info->tcpi_ca_state = ccid3_hc_rx_sk(sk)->rx_state;
	info->tcpi_options  |= TCPI_OPT_TIMESTAMPS;
	info->tcpi_rcv_rtt  = ccid3_hc_rx_sk(sk)->rx_rtt;
}

static int ccid3_hc_rx_getsockopt(struct sock *sk, const int optname, int len,
				  u32 __user *optval, int __user *optlen)
{
	const struct ccid3_hc_rx_sock *hc = ccid3_hc_rx_sk(sk);
	struct tfrc_rx_info rx_info;
	const void *val;

	switch (optname) {
	case DCCP_SOCKOPT_CCID_RX_INFO:
		if (len < sizeof(rx_info))
			return -EINVAL;
		rx_info.tfrcrx_x_recv = hc->rx_x_recv;
		rx_info.tfrcrx_rtt    = hc->rx_rtt;
		rx_info.tfrcrx_p      = tfrc_invert_loss_event_rate(hc->rx_pinv);
		len = sizeof(rx_info);
		val = &rx_info;
		break;
	default:
		return -ENOPROTOOPT;
	}

	if (put_user(len, optlen) || copy_to_user(optval, val, len))
		return -EFAULT;

	return 0;
}

struct ccid_operations ccid3_ops = {
	.ccid_id		   = DCCPC_CCID3,
	.ccid_name		   = "TCP-Friendly Rate Control",
	.ccid_hc_tx_obj_size	   = sizeof(struct ccid3_hc_tx_sock),
	.ccid_hc_tx_init	   = ccid3_hc_tx_init,
	.ccid_hc_tx_exit	   = ccid3_hc_tx_exit,
	.ccid_hc_tx_send_packet	   = ccid3_hc_tx_send_packet,
	.ccid_hc_tx_packet_sent	   = ccid3_hc_tx_packet_sent,
	.ccid_hc_tx_packet_recv	   = ccid3_hc_tx_packet_recv,
	.ccid_hc_tx_parse_options  = ccid3_hc_tx_parse_options,
	.ccid_hc_rx_obj_size	   = sizeof(struct ccid3_hc_rx_sock),
	.ccid_hc_rx_init	   = ccid3_hc_rx_init,
	.ccid_hc_rx_exit	   = ccid3_hc_rx_exit,
	.ccid_hc_rx_insert_options = ccid3_hc_rx_insert_options,
	.ccid_hc_rx_packet_recv	   = ccid3_hc_rx_packet_recv,
	.ccid_hc_rx_get_info	   = ccid3_hc_rx_get_info,
	.ccid_hc_tx_get_info	   = ccid3_hc_tx_get_info,
	.ccid_hc_rx_getsockopt	   = ccid3_hc_rx_getsockopt,
	.ccid_hc_tx_getsockopt	   = ccid3_hc_tx_getsockopt,
};

#ifdef CONFIG_IP_DCCP_CCID3_DEBUG
module_param(ccid3_debug, bool, 0644);
MODULE_PARM_DESC(ccid3_debug, "Enable CCID-3 debug messages");
#endif
