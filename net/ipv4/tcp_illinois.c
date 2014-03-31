/*
 * TCP Illinois congestion control.
 * Home page:
 *	http://www.ews.uiuc.edu/~shaoliu/tcpillinois/index.html
 *
 * The algorithm is described in:
 * "TCP-Illinois: A Loss and Delay-Based Congestion Control Algorithm
 *  for High-Speed Networks"
 * http://www.ifp.illinois.edu/~srikant/Papers/liubassri06perf.pdf
 *
 * Implemented from description in paper and ns-2 simulation.
 * Copyright (C) 2007 Stephen Hemminger <shemminger@linux-foundation.org>
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>
#include <asm/div64.h>
#include <net/tcp.h>

#define ALPHA_SHIFT	7
#define ALPHA_SCALE	(1u<<ALPHA_SHIFT)
#define ALPHA_MIN	((3*ALPHA_SCALE)/10)	
#define ALPHA_MAX	(10*ALPHA_SCALE)	
#define ALPHA_BASE	ALPHA_SCALE		
#define U32_MAX		((u32)~0U)
#define RTT_MAX		(U32_MAX / ALPHA_MAX)	

#define BETA_SHIFT	6
#define BETA_SCALE	(1u<<BETA_SHIFT)
#define BETA_MIN	(BETA_SCALE/8)		
#define BETA_MAX	(BETA_SCALE/2)		
#define BETA_BASE	BETA_MAX

static int win_thresh __read_mostly = 15;
module_param(win_thresh, int, 0);
MODULE_PARM_DESC(win_thresh, "Window threshold for starting adaptive sizing");

static int theta __read_mostly = 5;
module_param(theta, int, 0);
MODULE_PARM_DESC(theta, "# of fast RTT's before full growth");

struct illinois {
	u64	sum_rtt;	
	u16	cnt_rtt;	
	u32	base_rtt;	
	u32	max_rtt;	
	u32	end_seq;	
	u32	alpha;		
	u32	beta;		
	u16	acked;		
	u8	rtt_above;	
	u8	rtt_low;	
};

static void rtt_reset(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct illinois *ca = inet_csk_ca(sk);

	ca->end_seq = tp->snd_nxt;
	ca->cnt_rtt = 0;
	ca->sum_rtt = 0;

	
}

static void tcp_illinois_init(struct sock *sk)
{
	struct illinois *ca = inet_csk_ca(sk);

	ca->alpha = ALPHA_MAX;
	ca->beta = BETA_BASE;
	ca->base_rtt = 0x7fffffff;
	ca->max_rtt = 0;

	ca->acked = 0;
	ca->rtt_low = 0;
	ca->rtt_above = 0;

	rtt_reset(sk);
}

static void tcp_illinois_acked(struct sock *sk, u32 pkts_acked, s32 rtt)
{
	struct illinois *ca = inet_csk_ca(sk);

	ca->acked = pkts_acked;

	
	if (rtt < 0)
		return;

	
	if (rtt > RTT_MAX)
		rtt = RTT_MAX;

	
	if (ca->base_rtt > rtt)
		ca->base_rtt = rtt;

	
	if (ca->max_rtt < rtt)
		ca->max_rtt = rtt;

	++ca->cnt_rtt;
	ca->sum_rtt += rtt;
}

static inline u32 max_delay(const struct illinois *ca)
{
	return ca->max_rtt - ca->base_rtt;
}

static inline u32 avg_delay(const struct illinois *ca)
{
	u64 t = ca->sum_rtt;

	do_div(t, ca->cnt_rtt);
	return t - ca->base_rtt;
}

static u32 alpha(struct illinois *ca, u32 da, u32 dm)
{
	u32 d1 = dm / 100;	

	if (da <= d1) {
		
		if (!ca->rtt_above)
			return ALPHA_MAX;

		if (++ca->rtt_low < theta)
			return ca->alpha;

		ca->rtt_low = 0;
		ca->rtt_above = 0;
		return ALPHA_MAX;
	}

	ca->rtt_above = 1;


	dm -= d1;
	da -= d1;
	return (dm * ALPHA_MAX) /
		(dm + (da  * (ALPHA_MAX - ALPHA_MIN)) / ALPHA_MIN);
}

static u32 beta(u32 da, u32 dm)
{
	u32 d2, d3;

	d2 = dm / 10;
	if (da <= d2)
		return BETA_MIN;

	d3 = (8 * dm) / 10;
	if (da >= d3 || d3 <= d2)
		return BETA_MAX;

	return (BETA_MIN * d3 - BETA_MAX * d2 + (BETA_MAX - BETA_MIN) * da)
		/ (d3 - d2);
}

static void update_params(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct illinois *ca = inet_csk_ca(sk);

	if (tp->snd_cwnd < win_thresh) {
		ca->alpha = ALPHA_BASE;
		ca->beta = BETA_BASE;
	} else if (ca->cnt_rtt > 0) {
		u32 dm = max_delay(ca);
		u32 da = avg_delay(ca);

		ca->alpha = alpha(ca, da, dm);
		ca->beta = beta(da, dm);
	}

	rtt_reset(sk);
}

static void tcp_illinois_state(struct sock *sk, u8 new_state)
{
	struct illinois *ca = inet_csk_ca(sk);

	if (new_state == TCP_CA_Loss) {
		ca->alpha = ALPHA_BASE;
		ca->beta = BETA_BASE;
		ca->rtt_low = 0;
		ca->rtt_above = 0;
		rtt_reset(sk);
	}
}

static void tcp_illinois_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct illinois *ca = inet_csk_ca(sk);

	if (after(ack, ca->end_seq))
		update_params(sk);

	
	if (!tcp_is_cwnd_limited(sk, in_flight))
		return;

	
	if (tp->snd_cwnd <= tp->snd_ssthresh)
		tcp_slow_start(tp);

	else {
		u32 delta;

		
		tp->snd_cwnd_cnt += ca->acked;
		ca->acked = 1;

		delta = (tp->snd_cwnd_cnt * ca->alpha) >> ALPHA_SHIFT;
		if (delta >= tp->snd_cwnd) {
			tp->snd_cwnd = min(tp->snd_cwnd + delta / tp->snd_cwnd,
					   (u32) tp->snd_cwnd_clamp);
			tp->snd_cwnd_cnt = 0;
		}
	}
}

static u32 tcp_illinois_ssthresh(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct illinois *ca = inet_csk_ca(sk);

	
	return max(tp->snd_cwnd - ((tp->snd_cwnd * ca->beta) >> BETA_SHIFT), 2U);
}


static void tcp_illinois_info(struct sock *sk, u32 ext,
			      struct sk_buff *skb)
{
	const struct illinois *ca = inet_csk_ca(sk);

	if (ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		struct tcpvegas_info info = {
			.tcpv_enabled = 1,
			.tcpv_rttcnt = ca->cnt_rtt,
			.tcpv_minrtt = ca->base_rtt,
		};
		u64 t = ca->sum_rtt;

		do_div(t, ca->cnt_rtt);
		info.tcpv_rtt = t;

		nla_put(skb, INET_DIAG_VEGASINFO, sizeof(info), &info);
	}
}

static struct tcp_congestion_ops tcp_illinois __read_mostly = {
	.flags		= TCP_CONG_RTT_STAMP,
	.init		= tcp_illinois_init,
	.ssthresh	= tcp_illinois_ssthresh,
	.min_cwnd	= tcp_reno_min_cwnd,
	.cong_avoid	= tcp_illinois_cong_avoid,
	.set_state	= tcp_illinois_state,
	.get_info	= tcp_illinois_info,
	.pkts_acked	= tcp_illinois_acked,

	.owner		= THIS_MODULE,
	.name		= "illinois",
};

static int __init tcp_illinois_register(void)
{
	BUILD_BUG_ON(sizeof(struct illinois) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_illinois);
}

static void __exit tcp_illinois_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_illinois);
}

module_init(tcp_illinois_register);
module_exit(tcp_illinois_unregister);

MODULE_AUTHOR("Stephen Hemminger, Shao Liu");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Illinois");
MODULE_VERSION("1.0");
