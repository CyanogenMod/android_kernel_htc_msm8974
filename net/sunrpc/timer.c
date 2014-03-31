/*
 * linux/net/sunrpc/timer.c
 *
 * Estimate RPC request round trip time.
 *
 * Based on packet round-trip and variance estimator algorithms described
 * in appendix A of "Congestion Avoidance and Control" by Van Jacobson
 * and Michael J. Karels (ACM Computer Communication Review; Proceedings
 * of the Sigcomm '88 Symposium in Stanford, CA, August, 1988).
 *
 * This RTT estimator is used only for RPC over datagram protocols.
 *
 * Copyright (C) 2002 Trond Myklebust <trond.myklebust@fys.uio.no>
 */

#include <asm/param.h>

#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/module.h>

#include <linux/sunrpc/clnt.h>

#define RPC_RTO_MAX (60*HZ)
#define RPC_RTO_INIT (HZ/5)
#define RPC_RTO_MIN (HZ/10)

void rpc_init_rtt(struct rpc_rtt *rt, unsigned long timeo)
{
	unsigned long init = 0;
	unsigned i;

	rt->timeo = timeo;

	if (timeo > RPC_RTO_INIT)
		init = (timeo - RPC_RTO_INIT) << 3;
	for (i = 0; i < 5; i++) {
		rt->srtt[i] = init;
		rt->sdrtt[i] = RPC_RTO_INIT;
		rt->ntimeouts[i] = 0;
	}
}
EXPORT_SYMBOL_GPL(rpc_init_rtt);

void rpc_update_rtt(struct rpc_rtt *rt, unsigned timer, long m)
{
	long *srtt, *sdrtt;

	if (timer-- == 0)
		return;

	
	if (m < 0)
		return;

	if (m == 0)
		m = 1L;

	srtt = (long *)&rt->srtt[timer];
	m -= *srtt >> 3;
	*srtt += m;

	if (m < 0)
		m = -m;

	sdrtt = (long *)&rt->sdrtt[timer];
	m -= *sdrtt >> 2;
	*sdrtt += m;

	
	if (*sdrtt < RPC_RTO_MIN)
		*sdrtt = RPC_RTO_MIN;
}
EXPORT_SYMBOL_GPL(rpc_update_rtt);

unsigned long rpc_calc_rto(struct rpc_rtt *rt, unsigned timer)
{
	unsigned long res;

	if (timer-- == 0)
		return rt->timeo;

	res = ((rt->srtt[timer] + 7) >> 3) + rt->sdrtt[timer];
	if (res > RPC_RTO_MAX)
		res = RPC_RTO_MAX;

	return res;
}
EXPORT_SYMBOL_GPL(rpc_calc_rto);
