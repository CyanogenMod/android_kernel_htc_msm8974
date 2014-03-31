
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>
#include <net/tcp.h>

struct westwood {
	u32    bw_ns_est;        
	u32    bw_est;           
	u32    rtt_win_sx;       
	u32    bk;
	u32    snd_una;          
	u32    cumul_ack;
	u32    accounted;
	u32    rtt;
	u32    rtt_min;          
	u8     first_ack;        
	u8     reset_rtt_min;    
};


#define TCP_WESTWOOD_RTT_MIN   (HZ/20)	
#define TCP_WESTWOOD_INIT_RTT  (20*HZ)	

static void tcp_westwood_init(struct sock *sk)
{
	struct westwood *w = inet_csk_ca(sk);

	w->bk = 0;
	w->bw_ns_est = 0;
	w->bw_est = 0;
	w->accounted = 0;
	w->cumul_ack = 0;
	w->reset_rtt_min = 1;
	w->rtt_min = w->rtt = TCP_WESTWOOD_INIT_RTT;
	w->rtt_win_sx = tcp_time_stamp;
	w->snd_una = tcp_sk(sk)->snd_una;
	w->first_ack = 1;
}

static inline u32 westwood_do_filter(u32 a, u32 b)
{
	return ((7 * a) + b) >> 3;
}

static void westwood_filter(struct westwood *w, u32 delta)
{
	
	if (w->bw_ns_est == 0 && w->bw_est == 0) {
		w->bw_ns_est = w->bk / delta;
		w->bw_est = w->bw_ns_est;
	} else {
		w->bw_ns_est = westwood_do_filter(w->bw_ns_est, w->bk / delta);
		w->bw_est = westwood_do_filter(w->bw_est, w->bw_ns_est);
	}
}

static void tcp_westwood_pkts_acked(struct sock *sk, u32 cnt, s32 rtt)
{
	struct westwood *w = inet_csk_ca(sk);

	if (rtt > 0)
		w->rtt = usecs_to_jiffies(rtt);
}

static void westwood_update_window(struct sock *sk)
{
	struct westwood *w = inet_csk_ca(sk);
	s32 delta = tcp_time_stamp - w->rtt_win_sx;

	if (w->first_ack) {
		w->snd_una = tcp_sk(sk)->snd_una;
		w->first_ack = 0;
	}

	if (w->rtt && delta > max_t(u32, w->rtt, TCP_WESTWOOD_RTT_MIN)) {
		westwood_filter(w, delta);

		w->bk = 0;
		w->rtt_win_sx = tcp_time_stamp;
	}
}

static inline void update_rtt_min(struct westwood *w)
{
	if (w->reset_rtt_min) {
		w->rtt_min = w->rtt;
		w->reset_rtt_min = 0;
	} else
		w->rtt_min = min(w->rtt, w->rtt_min);
}


static inline void westwood_fast_bw(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct westwood *w = inet_csk_ca(sk);

	westwood_update_window(sk);

	w->bk += tp->snd_una - w->snd_una;
	w->snd_una = tp->snd_una;
	update_rtt_min(w);
}

static inline u32 westwood_acked_count(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct westwood *w = inet_csk_ca(sk);

	w->cumul_ack = tp->snd_una - w->snd_una;

	if (!w->cumul_ack) {
		w->accounted += tp->mss_cache;
		w->cumul_ack = tp->mss_cache;
	}

	if (w->cumul_ack > tp->mss_cache) {
		
		if (w->accounted >= w->cumul_ack) {
			w->accounted -= w->cumul_ack;
			w->cumul_ack = tp->mss_cache;
		} else {
			w->cumul_ack -= w->accounted;
			w->accounted = 0;
		}
	}

	w->snd_una = tp->snd_una;

	return w->cumul_ack;
}


static u32 tcp_westwood_bw_rttmin(const struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	const struct westwood *w = inet_csk_ca(sk);
	return max_t(u32, (w->bw_est * w->rtt_min) / tp->mss_cache, 2);
}

static void tcp_westwood_event(struct sock *sk, enum tcp_ca_event event)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct westwood *w = inet_csk_ca(sk);

	switch (event) {
	case CA_EVENT_FAST_ACK:
		westwood_fast_bw(sk);
		break;

	case CA_EVENT_COMPLETE_CWR:
		tp->snd_cwnd = tp->snd_ssthresh = tcp_westwood_bw_rttmin(sk);
		break;

	case CA_EVENT_FRTO:
		tp->snd_ssthresh = tcp_westwood_bw_rttmin(sk);
		
		w->reset_rtt_min = 1;
		break;

	case CA_EVENT_SLOW_ACK:
		westwood_update_window(sk);
		w->bk += westwood_acked_count(sk);
		update_rtt_min(w);
		break;

	default:
		
		break;
	}
}


static void tcp_westwood_info(struct sock *sk, u32 ext,
			      struct sk_buff *skb)
{
	const struct westwood *ca = inet_csk_ca(sk);
	if (ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		struct tcpvegas_info info = {
			.tcpv_enabled = 1,
			.tcpv_rtt = jiffies_to_usecs(ca->rtt),
			.tcpv_minrtt = jiffies_to_usecs(ca->rtt_min),
		};

		nla_put(skb, INET_DIAG_VEGASINFO, sizeof(info), &info);
	}
}


static struct tcp_congestion_ops tcp_westwood __read_mostly = {
	.init		= tcp_westwood_init,
	.ssthresh	= tcp_reno_ssthresh,
	.cong_avoid	= tcp_reno_cong_avoid,
	.min_cwnd	= tcp_westwood_bw_rttmin,
	.cwnd_event	= tcp_westwood_event,
	.get_info	= tcp_westwood_info,
	.pkts_acked	= tcp_westwood_pkts_acked,

	.owner		= THIS_MODULE,
	.name		= "westwood"
};

static int __init tcp_westwood_register(void)
{
	BUILD_BUG_ON(sizeof(struct westwood) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_westwood);
}

static void __exit tcp_westwood_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_westwood);
}

module_init(tcp_westwood_register);
module_exit(tcp_westwood_unregister);

MODULE_AUTHOR("Stephen Hemminger, Angelo Dell'Aera");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Westwood+");
