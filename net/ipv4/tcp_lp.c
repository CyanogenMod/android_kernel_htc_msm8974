
#include <linux/module.h>
#include <net/tcp.h>

#define LP_RESOL       1000

enum tcp_lp_state {
	LP_VALID_RHZ = (1 << 0),
	LP_VALID_OWD = (1 << 1),
	LP_WITHIN_THR = (1 << 3),
	LP_WITHIN_INF = (1 << 4),
};

struct lp {
	u32 flag;
	u32 sowd;
	u32 owd_min;
	u32 owd_max;
	u32 owd_max_rsv;
	u32 remote_hz;
	u32 remote_ref_time;
	u32 local_ref_time;
	u32 last_drop;
	u32 inference;
};

static void tcp_lp_init(struct sock *sk)
{
	struct lp *lp = inet_csk_ca(sk);

	lp->flag = 0;
	lp->sowd = 0;
	lp->owd_min = 0xffffffff;
	lp->owd_max = 0;
	lp->owd_max_rsv = 0;
	lp->remote_hz = 0;
	lp->remote_ref_time = 0;
	lp->local_ref_time = 0;
	lp->last_drop = 0;
	lp->inference = 0;
}

static void tcp_lp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct lp *lp = inet_csk_ca(sk);

	if (!(lp->flag & LP_WITHIN_INF))
		tcp_reno_cong_avoid(sk, ack, in_flight);
}

static u32 tcp_lp_remote_hz_estimator(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct lp *lp = inet_csk_ca(sk);
	s64 rhz = lp->remote_hz << 6;	
	s64 m = 0;

	if (lp->remote_ref_time == 0 || lp->local_ref_time == 0)
		goto out;

	
	if (tp->rx_opt.rcv_tsval == lp->remote_ref_time ||
	    tp->rx_opt.rcv_tsecr == lp->local_ref_time)
		goto out;

	m = HZ * (tp->rx_opt.rcv_tsval -
		  lp->remote_ref_time) / (tp->rx_opt.rcv_tsecr -
					  lp->local_ref_time);
	if (m < 0)
		m = -m;

	if (rhz > 0) {
		m -= rhz >> 6;	
		rhz += m;	
	} else
		rhz = m << 6;

 out:
	
	if ((rhz >> 6) > 0)
		lp->flag |= LP_VALID_RHZ;
	else
		lp->flag &= ~LP_VALID_RHZ;

	
	lp->remote_ref_time = tp->rx_opt.rcv_tsval;
	lp->local_ref_time = tp->rx_opt.rcv_tsecr;

	return rhz >> 6;
}

static u32 tcp_lp_owd_calculator(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct lp *lp = inet_csk_ca(sk);
	s64 owd = 0;

	lp->remote_hz = tcp_lp_remote_hz_estimator(sk);

	if (lp->flag & LP_VALID_RHZ) {
		owd =
		    tp->rx_opt.rcv_tsval * (LP_RESOL / lp->remote_hz) -
		    tp->rx_opt.rcv_tsecr * (LP_RESOL / HZ);
		if (owd < 0)
			owd = -owd;
	}

	if (owd > 0)
		lp->flag |= LP_VALID_OWD;
	else
		lp->flag &= ~LP_VALID_OWD;

	return owd;
}

static void tcp_lp_rtt_sample(struct sock *sk, u32 rtt)
{
	struct lp *lp = inet_csk_ca(sk);
	s64 mowd = tcp_lp_owd_calculator(sk);

	
	if (!(lp->flag & LP_VALID_RHZ) || !(lp->flag & LP_VALID_OWD))
		return;

	
	if (mowd < lp->owd_min)
		lp->owd_min = mowd;

	if (mowd > lp->owd_max) {
		if (mowd > lp->owd_max_rsv) {
			if (lp->owd_max_rsv == 0)
				lp->owd_max = mowd;
			else
				lp->owd_max = lp->owd_max_rsv;
			lp->owd_max_rsv = mowd;
		} else
			lp->owd_max = mowd;
	}

	
	if (lp->sowd != 0) {
		mowd -= lp->sowd >> 3;	
		lp->sowd += mowd;	
	} else
		lp->sowd = mowd << 3;	
}

static void tcp_lp_pkts_acked(struct sock *sk, u32 num_acked, s32 rtt_us)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct lp *lp = inet_csk_ca(sk);

	if (rtt_us > 0)
		tcp_lp_rtt_sample(sk, rtt_us);

	
	if (tcp_time_stamp > tp->rx_opt.rcv_tsecr)
		lp->inference = 3 * (tcp_time_stamp - tp->rx_opt.rcv_tsecr);

	
	if (lp->last_drop && (tcp_time_stamp - lp->last_drop < lp->inference))
		lp->flag |= LP_WITHIN_INF;
	else
		lp->flag &= ~LP_WITHIN_INF;

	
	if (lp->sowd >> 3 <
	    lp->owd_min + 15 * (lp->owd_max - lp->owd_min) / 100)
		lp->flag |= LP_WITHIN_THR;
	else
		lp->flag &= ~LP_WITHIN_THR;

	pr_debug("TCP-LP: %05o|%5u|%5u|%15u|%15u|%15u\n", lp->flag,
		 tp->snd_cwnd, lp->remote_hz, lp->owd_min, lp->owd_max,
		 lp->sowd >> 3);

	if (lp->flag & LP_WITHIN_THR)
		return;

	lp->owd_min = lp->sowd >> 3;
	lp->owd_max = lp->sowd >> 2;
	lp->owd_max_rsv = lp->sowd >> 2;

	if (lp->flag & LP_WITHIN_INF)
		tp->snd_cwnd = 1U;

	else
		tp->snd_cwnd = max(tp->snd_cwnd >> 1U, 1U);

	
	lp->last_drop = tcp_time_stamp;
}

static struct tcp_congestion_ops tcp_lp __read_mostly = {
	.flags = TCP_CONG_RTT_STAMP,
	.init = tcp_lp_init,
	.ssthresh = tcp_reno_ssthresh,
	.cong_avoid = tcp_lp_cong_avoid,
	.min_cwnd = tcp_reno_min_cwnd,
	.pkts_acked = tcp_lp_pkts_acked,

	.owner = THIS_MODULE,
	.name = "lp"
};

static int __init tcp_lp_register(void)
{
	BUILD_BUG_ON(sizeof(struct lp) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_lp);
}

static void __exit tcp_lp_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_lp);
}

module_init(tcp_lp_register);
module_exit(tcp_lp_unregister);

MODULE_AUTHOR("Wong Hoi Sing Edison, Hung Hing Lun Mike");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Low Priority");
