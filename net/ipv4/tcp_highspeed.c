
#include <linux/module.h>
#include <net/tcp.h>


static const struct hstcp_aimd_val {
	unsigned int cwnd;
	unsigned int md;
} hstcp_aimd_vals[] = {
 {     38,  128,  },
 {    118,  112,  },
 {    221,  104,  },
 {    347,   98,  },
 {    495,   93,  },
 {    663,   89,  },
 {    851,   86,  },
 {   1058,   83,  },
 {   1284,   81,  },
 {   1529,   78,  },
 {   1793,   76,  },
 {   2076,   74,  },
 {   2378,   72,  },
 {   2699,   71,  },
 {   3039,   69,  },
 {   3399,   68,  },
 {   3778,   66,  },
 {   4177,   65,  },
 {   4596,   64,  },
 {   5036,   62,  },
 {   5497,   61,  },
 {   5979,   60,  },
 {   6483,   59,  },
 {   7009,   58,  },
 {   7558,   57,  },
 {   8130,   56,  },
 {   8726,   55,  },
 {   9346,   54,  },
 {   9991,   53,  },
 {  10661,   52,  },
 {  11358,   52,  },
 {  12082,   51,  },
 {  12834,   50,  },
 {  13614,   49,  },
 {  14424,   48,  },
 {  15265,   48,  },
 {  16137,   47,  },
 {  17042,   46,  },
 {  17981,   45,  },
 {  18955,   45,  },
 {  19965,   44,  },
 {  21013,   43,  },
 {  22101,   43,  },
 {  23230,   42,  },
 {  24402,   41,  },
 {  25618,   41,  },
 {  26881,   40,  },
 {  28193,   39,  },
 {  29557,   39,  },
 {  30975,   38,  },
 {  32450,   38,  },
 {  33986,   37,  },
 {  35586,   36,  },
 {  37253,   36,  },
 {  38992,   35,  },
 {  40808,   35,  },
 {  42707,   34,  },
 {  44694,   33,  },
 {  46776,   33,  },
 {  48961,   32,  },
 {  51258,   32,  },
 {  53677,   31,  },
 {  56230,   30,  },
 {  58932,   30,  },
 {  61799,   29,  },
 {  64851,   28,  },
 {  68113,   28,  },
 {  71617,   27,  },
 {  75401,   26,  },
 {  79517,   26,  },
 {  84035,   25,  },
 {  89053,   24,  },
};

#define HSTCP_AIMD_MAX	ARRAY_SIZE(hstcp_aimd_vals)

struct hstcp {
	u32	ai;
};

static void hstcp_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct hstcp *ca = inet_csk_ca(sk);

	ca->ai = 0;

	tp->snd_cwnd_clamp = min_t(u32, tp->snd_cwnd_clamp, 0xffffffff/128);
}

static void hstcp_cong_avoid(struct sock *sk, u32 adk, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct hstcp *ca = inet_csk_ca(sk);

	if (!tcp_is_cwnd_limited(sk, in_flight))
		return;

	if (tp->snd_cwnd <= tp->snd_ssthresh)
		tcp_slow_start(tp);
	else {
		if (tp->snd_cwnd > hstcp_aimd_vals[ca->ai].cwnd) {
			while (tp->snd_cwnd > hstcp_aimd_vals[ca->ai].cwnd &&
			       ca->ai < HSTCP_AIMD_MAX - 1)
				ca->ai++;
		} else if (ca->ai && tp->snd_cwnd <= hstcp_aimd_vals[ca->ai-1].cwnd) {
			while (ca->ai && tp->snd_cwnd <= hstcp_aimd_vals[ca->ai-1].cwnd)
				ca->ai--;
		}

		
		if (tp->snd_cwnd < tp->snd_cwnd_clamp) {
			
			tp->snd_cwnd_cnt += ca->ai + 1;
			if (tp->snd_cwnd_cnt >= tp->snd_cwnd) {
				tp->snd_cwnd_cnt -= tp->snd_cwnd;
				tp->snd_cwnd++;
			}
		}
	}
}

static u32 hstcp_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	const struct hstcp *ca = inet_csk_ca(sk);

	
	return max(tp->snd_cwnd - ((tp->snd_cwnd * hstcp_aimd_vals[ca->ai].md) >> 8), 2U);
}


static struct tcp_congestion_ops tcp_highspeed __read_mostly = {
	.init		= hstcp_init,
	.ssthresh	= hstcp_ssthresh,
	.cong_avoid	= hstcp_cong_avoid,
	.min_cwnd	= tcp_reno_min_cwnd,

	.owner		= THIS_MODULE,
	.name		= "highspeed"
};

static int __init hstcp_register(void)
{
	BUILD_BUG_ON(sizeof(struct hstcp) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_highspeed);
}

static void __exit hstcp_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_highspeed);
}

module_init(hstcp_register);
module_exit(hstcp_unregister);

MODULE_AUTHOR("John Heffner");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("High Speed TCP");
