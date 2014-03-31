
#include <linux/module.h>
#include <net/tcp.h>

struct hybla {
	u8    hybla_en;
	u32   snd_cwnd_cents; 
	u32   rho;	      
	u32   rho2;	      
	u32   rho_3ls;	      
	u32   rho2_7ls;	      
	u32   minrtt;	      
};

static int rtt0 = 25;
module_param(rtt0, int, 0644);
MODULE_PARM_DESC(rtt0, "reference rout trip time (ms)");


static inline void hybla_recalc_param (struct sock *sk)
{
	struct hybla *ca = inet_csk_ca(sk);

	ca->rho_3ls = max_t(u32, tcp_sk(sk)->srtt / msecs_to_jiffies(rtt0), 8);
	ca->rho = ca->rho_3ls >> 3;
	ca->rho2_7ls = (ca->rho_3ls * ca->rho_3ls) << 1;
	ca->rho2 = ca->rho2_7ls >>7;
}

static void hybla_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct hybla *ca = inet_csk_ca(sk);

	ca->rho = 0;
	ca->rho2 = 0;
	ca->rho_3ls = 0;
	ca->rho2_7ls = 0;
	ca->snd_cwnd_cents = 0;
	ca->hybla_en = 1;
	tp->snd_cwnd = 2;
	tp->snd_cwnd_clamp = 65535;

	
	hybla_recalc_param(sk);

	
	ca->minrtt = tp->srtt;
	tp->snd_cwnd = ca->rho;
}

static void hybla_state(struct sock *sk, u8 ca_state)
{
	struct hybla *ca = inet_csk_ca(sk);
	ca->hybla_en = (ca_state == TCP_CA_Open);
}

static inline u32 hybla_fraction(u32 odds)
{
	static const u32 fractions[] = {
		128, 139, 152, 165, 181, 197, 215, 234,
	};

	return (odds < ARRAY_SIZE(fractions)) ? fractions[odds] : 128;
}

static void hybla_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct hybla *ca = inet_csk_ca(sk);
	u32 increment, odd, rho_fractions;
	int is_slowstart = 0;

	
	if (tp->srtt < ca->minrtt){
		hybla_recalc_param(sk);
		ca->minrtt = tp->srtt;
	}

	if (!tcp_is_cwnd_limited(sk, in_flight))
		return;

	if (!ca->hybla_en) {
		tcp_reno_cong_avoid(sk, ack, in_flight);
		return;
	}

	if (ca->rho == 0)
		hybla_recalc_param(sk);

	rho_fractions = ca->rho_3ls - (ca->rho << 3);

	if (tp->snd_cwnd < tp->snd_ssthresh) {
		is_slowstart = 1;
		increment = ((1 << min(ca->rho, 16U)) *
			hybla_fraction(rho_fractions)) - 128;
	} else {
		increment = ca->rho2_7ls / tp->snd_cwnd;
		if (increment < 128)
			tp->snd_cwnd_cnt++;
	}

	odd = increment % 128;
	tp->snd_cwnd += increment >> 7;
	ca->snd_cwnd_cents += odd;

	
	while (ca->snd_cwnd_cents >= 128) {
		tp->snd_cwnd++;
		ca->snd_cwnd_cents -= 128;
		tp->snd_cwnd_cnt = 0;
	}
	
	if (increment == 0 && odd == 0 && tp->snd_cwnd_cnt >= tp->snd_cwnd) {
		tp->snd_cwnd++;
		tp->snd_cwnd_cnt = 0;
	}
	
	if (is_slowstart)
		tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_ssthresh);

	tp->snd_cwnd = min_t(u32, tp->snd_cwnd, tp->snd_cwnd_clamp);
}

static struct tcp_congestion_ops tcp_hybla __read_mostly = {
	.init		= hybla_init,
	.ssthresh	= tcp_reno_ssthresh,
	.min_cwnd	= tcp_reno_min_cwnd,
	.cong_avoid	= hybla_cong_avoid,
	.set_state	= hybla_state,

	.owner		= THIS_MODULE,
	.name		= "hybla"
};

static int __init hybla_register(void)
{
	BUILD_BUG_ON(sizeof(struct hybla) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_hybla);
}

static void __exit hybla_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_hybla);
}

module_init(hybla_register);
module_exit(hybla_unregister);

MODULE_AUTHOR("Daniele Lacamera");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Hybla");
