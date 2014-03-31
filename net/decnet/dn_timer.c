#include <linux/net.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <net/sock.h>
#include <linux/atomic.h>
#include <net/flow.h>
#include <net/dn.h>


#define SLOW_INTERVAL (HZ/2)

static void dn_slow_timer(unsigned long arg);

void dn_start_slow_timer(struct sock *sk)
{
	setup_timer(&sk->sk_timer, dn_slow_timer, (unsigned long)sk);
	sk_reset_timer(sk, &sk->sk_timer, jiffies + SLOW_INTERVAL);
}

void dn_stop_slow_timer(struct sock *sk)
{
	sk_stop_timer(sk, &sk->sk_timer);
}

static void dn_slow_timer(unsigned long arg)
{
	struct sock *sk = (struct sock *)arg;
	struct dn_scp *scp = DN_SK(sk);

	bh_lock_sock(sk);

	if (sock_owned_by_user(sk)) {
		sk_reset_timer(sk, &sk->sk_timer, jiffies + HZ / 10);
		goto out;
	}

	if (scp->persist && scp->persist_fxn) {
		if (scp->persist <= SLOW_INTERVAL) {
			scp->persist = 0;

			if (scp->persist_fxn(sk))
				goto out;
		} else {
			scp->persist -= SLOW_INTERVAL;
		}
	}

	if (scp->keepalive && scp->keepalive_fxn && (scp->state == DN_RUN)) {
		if ((jiffies - scp->stamp) >= scp->keepalive)
			scp->keepalive_fxn(sk);
	}

	sk_reset_timer(sk, &sk->sk_timer, jiffies + SLOW_INTERVAL);
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}
