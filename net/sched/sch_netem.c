/*
 * net/sched/sch_netem.c	Network emulator
 *
 * 		This program is free software; you can redistribute it and/or
 * 		modify it under the terms of the GNU General Public License
 * 		as published by the Free Software Foundation; either version
 * 		2 of the License.
 *
 *  		Many of the algorithms and ideas for this came from
 *		NIST Net which is not copyrighted.
 *
 * Authors:	Stephen Hemminger <shemminger@osdl.org>
 *		Catalin(ux aka Dino) BOIE <catab at umbrella dot ro>
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>
#include <linux/rtnetlink.h>
#include <linux/reciprocal_div.h>

#include <net/netlink.h>
#include <net/pkt_sched.h>

#define VERSION "1.3"


struct netem_sched_data {
	

	
	struct Qdisc	*qdisc;

	struct qdisc_watchdog watchdog;

	psched_tdiff_t latency;
	psched_tdiff_t jitter;

	u32 loss;
	u32 limit;
	u32 counter;
	u32 gap;
	u32 duplicate;
	u32 reorder;
	u32 corrupt;
	u32 rate;
	s32 packet_overhead;
	u32 cell_size;
	u32 cell_size_reciprocal;
	s32 cell_overhead;

	struct crndstate {
		u32 last;
		u32 rho;
	} delay_cor, loss_cor, dup_cor, reorder_cor, corrupt_cor;

	struct disttable {
		u32  size;
		s16 table[0];
	} *delay_dist;

	enum  {
		CLG_RANDOM,
		CLG_4_STATES,
		CLG_GILB_ELL,
	} loss_model;

	
	struct clgstate {
		
		u8 state;

		
		u32 a1;	
		u32 a2;	
		u32 a3;	
		u32 a4;	
		u32 a5; 
	} clg;

};

struct netem_skb_cb {
	psched_time_t	time_to_send;
};

static inline struct netem_skb_cb *netem_skb_cb(struct sk_buff *skb)
{
	qdisc_cb_private_validate(skb, sizeof(struct netem_skb_cb));
	return (struct netem_skb_cb *)qdisc_skb_cb(skb)->data;
}

static void init_crandom(struct crndstate *state, unsigned long rho)
{
	state->rho = rho;
	state->last = net_random();
}

static u32 get_crandom(struct crndstate *state)
{
	u64 value, rho;
	unsigned long answer;

	if (state->rho == 0)	
		return net_random();

	value = net_random();
	rho = (u64)state->rho + 1;
	answer = (value * ((1ull<<32) - rho) + state->last * rho) >> 32;
	state->last = answer;
	return answer;
}

static bool loss_4state(struct netem_sched_data *q)
{
	struct clgstate *clg = &q->clg;
	u32 rnd = net_random();

	switch (clg->state) {
	case 1:
		if (rnd < clg->a4) {
			clg->state = 4;
			return true;
		} else if (clg->a4 < rnd && rnd < clg->a1) {
			clg->state = 3;
			return true;
		} else if (clg->a1 < rnd)
			clg->state = 1;

		break;
	case 2:
		if (rnd < clg->a5) {
			clg->state = 3;
			return true;
		} else
			clg->state = 2;

		break;
	case 3:
		if (rnd < clg->a3)
			clg->state = 2;
		else if (clg->a3 < rnd && rnd < clg->a2 + clg->a3) {
			clg->state = 1;
			return true;
		} else if (clg->a2 + clg->a3 < rnd) {
			clg->state = 3;
			return true;
		}
		break;
	case 4:
		clg->state = 1;
		break;
	}

	return false;
}

static bool loss_gilb_ell(struct netem_sched_data *q)
{
	struct clgstate *clg = &q->clg;

	switch (clg->state) {
	case 1:
		if (net_random() < clg->a1)
			clg->state = 2;
		if (net_random() < clg->a4)
			return true;
	case 2:
		if (net_random() < clg->a2)
			clg->state = 1;
		if (clg->a3 > net_random())
			return true;
	}

	return false;
}

static bool loss_event(struct netem_sched_data *q)
{
	switch (q->loss_model) {
	case CLG_RANDOM:
		
		return q->loss && q->loss >= get_crandom(&q->loss_cor);

	case CLG_4_STATES:
		return loss_4state(q);

	case CLG_GILB_ELL:
		return loss_gilb_ell(q);
	}

	return false;	
}


static psched_tdiff_t tabledist(psched_tdiff_t mu, psched_tdiff_t sigma,
				struct crndstate *state,
				const struct disttable *dist)
{
	psched_tdiff_t x;
	long t;
	u32 rnd;

	if (sigma == 0)
		return mu;

	rnd = get_crandom(state);

	
	if (dist == NULL)
		return (rnd % (2*sigma)) - sigma + mu;

	t = dist->table[rnd % dist->size];
	x = (sigma % NETEM_DIST_SCALE) * t;
	if (x >= 0)
		x += NETEM_DIST_SCALE/2;
	else
		x -= NETEM_DIST_SCALE/2;

	return  x / NETEM_DIST_SCALE + (sigma / NETEM_DIST_SCALE) * t + mu;
}

static psched_time_t packet_len_2_sched_time(unsigned int len, struct netem_sched_data *q)
{
	u64 ticks;

	len += q->packet_overhead;

	if (q->cell_size) {
		u32 cells = reciprocal_divide(len, q->cell_size_reciprocal);

		if (len > cells * q->cell_size)	
			cells++;
		len = cells * (q->cell_size + q->cell_overhead);
	}

	ticks = (u64)len * NSEC_PER_SEC;

	do_div(ticks, q->rate);
	return PSCHED_NS2TICKS(ticks);
}

static int tfifo_enqueue(struct sk_buff *nskb, struct Qdisc *sch)
{
	struct sk_buff_head *list = &sch->q;
	psched_time_t tnext = netem_skb_cb(nskb)->time_to_send;
	struct sk_buff *skb;

	if (likely(skb_queue_len(list) < sch->limit)) {
		skb = skb_peek_tail(list);
		
		if (likely(!skb || tnext >= netem_skb_cb(skb)->time_to_send))
			return qdisc_enqueue_tail(nskb, sch);

		skb_queue_reverse_walk(list, skb) {
			if (tnext >= netem_skb_cb(skb)->time_to_send)
				break;
		}

		__skb_queue_after(list, skb, nskb);
		sch->qstats.backlog += qdisc_pkt_len(nskb);
		return NET_XMIT_SUCCESS;
	}

	return qdisc_reshape_fail(nskb, sch);
}

static int netem_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	
	struct netem_skb_cb *cb;
	struct sk_buff *skb2;
	int ret;
	int count = 1;

	
	if (q->duplicate && q->duplicate >= get_crandom(&q->dup_cor))
		++count;

	
	if (loss_event(q))
		--count;

	if (count == 0) {
		sch->qstats.drops++;
		kfree_skb(skb);
		return NET_XMIT_SUCCESS | __NET_XMIT_BYPASS;
	}

	skb_orphan(skb);

	if (count > 1 && (skb2 = skb_clone(skb, GFP_ATOMIC)) != NULL) {
		struct Qdisc *rootq = qdisc_root(sch);
		u32 dupsave = q->duplicate; 
		q->duplicate = 0;

		qdisc_enqueue_root(skb2, rootq);
		q->duplicate = dupsave;
	}

	if (q->corrupt && q->corrupt >= get_crandom(&q->corrupt_cor)) {
		if (!(skb = skb_unshare(skb, GFP_ATOMIC)) ||
		    (skb->ip_summed == CHECKSUM_PARTIAL &&
		     skb_checksum_help(skb)))
			return qdisc_drop(skb, sch);

		skb->data[net_random() % skb_headlen(skb)] ^= 1<<(net_random() % 8);
	}

	cb = netem_skb_cb(skb);
	if (q->gap == 0 ||		
	    q->counter < q->gap - 1 ||	
	    q->reorder < get_crandom(&q->reorder_cor)) {
		psched_time_t now;
		psched_tdiff_t delay;

		delay = tabledist(q->latency, q->jitter,
				  &q->delay_cor, q->delay_dist);

		now = psched_get_time();

		if (q->rate) {
			struct sk_buff_head *list = &sch->q;

			delay += packet_len_2_sched_time(skb->len, q);

			if (!skb_queue_empty(list)) {
				delay -= now - netem_skb_cb(skb_peek(list))->time_to_send;
				now = netem_skb_cb(skb_peek_tail(list))->time_to_send;
			}
		}

		cb->time_to_send = now + delay;
		++q->counter;
		ret = tfifo_enqueue(skb, sch);
	} else {
		cb->time_to_send = psched_get_time();
		q->counter = 0;

		__skb_queue_head(&sch->q, skb);
		sch->qstats.backlog += qdisc_pkt_len(skb);
		sch->qstats.requeues++;
		ret = NET_XMIT_SUCCESS;
	}

	if (ret != NET_XMIT_SUCCESS) {
		if (net_xmit_drop_count(ret)) {
			sch->qstats.drops++;
			return ret;
		}
	}

	return NET_XMIT_SUCCESS;
}

static unsigned int netem_drop(struct Qdisc *sch)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	unsigned int len;

	len = qdisc_queue_drop(sch);
	if (!len && q->qdisc && q->qdisc->ops->drop)
	    len = q->qdisc->ops->drop(q->qdisc);
	if (len)
		sch->qstats.drops++;

	return len;
}

static struct sk_buff *netem_dequeue(struct Qdisc *sch)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	struct sk_buff *skb;

	if (qdisc_is_throttled(sch))
		return NULL;

tfifo_dequeue:
	skb = qdisc_peek_head(sch);
	if (skb) {
		const struct netem_skb_cb *cb = netem_skb_cb(skb);

		
		if (cb->time_to_send <= psched_get_time()) {
			__skb_unlink(skb, &sch->q);
			sch->qstats.backlog -= qdisc_pkt_len(skb);

#ifdef CONFIG_NET_CLS_ACT
			if (G_TC_FROM(skb->tc_verd) & AT_INGRESS)
				skb->tstamp.tv64 = 0;
#endif

			if (q->qdisc) {
				int err = qdisc_enqueue(skb, q->qdisc);

				if (unlikely(err != NET_XMIT_SUCCESS)) {
					if (net_xmit_drop_count(err)) {
						sch->qstats.drops++;
						qdisc_tree_decrease_qlen(sch, 1);
					}
				}
				goto tfifo_dequeue;
			}
deliver:
			qdisc_unthrottled(sch);
			qdisc_bstats_update(sch, skb);
			return skb;
		}

		if (q->qdisc) {
			skb = q->qdisc->ops->dequeue(q->qdisc);
			if (skb)
				goto deliver;
		}
		qdisc_watchdog_schedule(&q->watchdog, cb->time_to_send);
	}

	if (q->qdisc) {
		skb = q->qdisc->ops->dequeue(q->qdisc);
		if (skb)
			goto deliver;
	}
	return NULL;
}

static void netem_reset(struct Qdisc *sch)
{
	struct netem_sched_data *q = qdisc_priv(sch);

	qdisc_reset_queue(sch);
	if (q->qdisc)
		qdisc_reset(q->qdisc);
	qdisc_watchdog_cancel(&q->watchdog);
}

static void dist_free(struct disttable *d)
{
	if (d) {
		if (is_vmalloc_addr(d))
			vfree(d);
		else
			kfree(d);
	}
}

static int get_dist_table(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	size_t n = nla_len(attr)/sizeof(__s16);
	const __s16 *data = nla_data(attr);
	spinlock_t *root_lock;
	struct disttable *d;
	int i;
	size_t s;

	if (n > NETEM_DIST_MAX)
		return -EINVAL;

	s = sizeof(struct disttable) + n * sizeof(s16);
	d = kmalloc(s, GFP_KERNEL | __GFP_NOWARN);
	if (!d)
		d = vmalloc(s);
	if (!d)
		return -ENOMEM;

	d->size = n;
	for (i = 0; i < n; i++)
		d->table[i] = data[i];

	root_lock = qdisc_root_sleeping_lock(sch);

	spin_lock_bh(root_lock);
	swap(q->delay_dist, d);
	spin_unlock_bh(root_lock);

	dist_free(d);
	return 0;
}

static void get_correlation(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	const struct tc_netem_corr *c = nla_data(attr);

	init_crandom(&q->delay_cor, c->delay_corr);
	init_crandom(&q->loss_cor, c->loss_corr);
	init_crandom(&q->dup_cor, c->dup_corr);
}

static void get_reorder(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	const struct tc_netem_reorder *r = nla_data(attr);

	q->reorder = r->probability;
	init_crandom(&q->reorder_cor, r->correlation);
}

static void get_corrupt(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	const struct tc_netem_corrupt *r = nla_data(attr);

	q->corrupt = r->probability;
	init_crandom(&q->corrupt_cor, r->correlation);
}

static void get_rate(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	const struct tc_netem_rate *r = nla_data(attr);

	q->rate = r->rate;
	q->packet_overhead = r->packet_overhead;
	q->cell_size = r->cell_size;
	if (q->cell_size)
		q->cell_size_reciprocal = reciprocal_value(q->cell_size);
	q->cell_overhead = r->cell_overhead;
}

static int get_loss_clg(struct Qdisc *sch, const struct nlattr *attr)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	const struct nlattr *la;
	int rem;

	nla_for_each_nested(la, attr, rem) {
		u16 type = nla_type(la);

		switch(type) {
		case NETEM_LOSS_GI: {
			const struct tc_netem_gimodel *gi = nla_data(la);

			if (nla_len(la) < sizeof(struct tc_netem_gimodel)) {
				pr_info("netem: incorrect gi model size\n");
				return -EINVAL;
			}

			q->loss_model = CLG_4_STATES;

			q->clg.state = 1;
			q->clg.a1 = gi->p13;
			q->clg.a2 = gi->p31;
			q->clg.a3 = gi->p32;
			q->clg.a4 = gi->p14;
			q->clg.a5 = gi->p23;
			break;
		}

		case NETEM_LOSS_GE: {
			const struct tc_netem_gemodel *ge = nla_data(la);

			if (nla_len(la) < sizeof(struct tc_netem_gemodel)) {
				pr_info("netem: incorrect ge model size\n");
				return -EINVAL;
			}

			q->loss_model = CLG_GILB_ELL;
			q->clg.state = 1;
			q->clg.a1 = ge->p;
			q->clg.a2 = ge->r;
			q->clg.a3 = ge->h;
			q->clg.a4 = ge->k1;
			break;
		}

		default:
			pr_info("netem: unknown loss type %u\n", type);
			return -EINVAL;
		}
	}

	return 0;
}

static const struct nla_policy netem_policy[TCA_NETEM_MAX + 1] = {
	[TCA_NETEM_CORR]	= { .len = sizeof(struct tc_netem_corr) },
	[TCA_NETEM_REORDER]	= { .len = sizeof(struct tc_netem_reorder) },
	[TCA_NETEM_CORRUPT]	= { .len = sizeof(struct tc_netem_corrupt) },
	[TCA_NETEM_RATE]	= { .len = sizeof(struct tc_netem_rate) },
	[TCA_NETEM_LOSS]	= { .type = NLA_NESTED },
};

static int parse_attr(struct nlattr *tb[], int maxtype, struct nlattr *nla,
		      const struct nla_policy *policy, int len)
{
	int nested_len = nla_len(nla) - NLA_ALIGN(len);

	if (nested_len < 0) {
		pr_info("netem: invalid attributes len %d\n", nested_len);
		return -EINVAL;
	}

	if (nested_len >= nla_attr_size(0))
		return nla_parse(tb, maxtype, nla_data(nla) + NLA_ALIGN(len),
				 nested_len, policy);

	memset(tb, 0, sizeof(struct nlattr *) * (maxtype + 1));
	return 0;
}

static int netem_change(struct Qdisc *sch, struct nlattr *opt)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_NETEM_MAX + 1];
	struct tc_netem_qopt *qopt;
	int ret;

	if (opt == NULL)
		return -EINVAL;

	qopt = nla_data(opt);
	ret = parse_attr(tb, TCA_NETEM_MAX, opt, netem_policy, sizeof(*qopt));
	if (ret < 0)
		return ret;

	sch->limit = qopt->limit;

	q->latency = qopt->latency;
	q->jitter = qopt->jitter;
	q->limit = qopt->limit;
	q->gap = qopt->gap;
	q->counter = 0;
	q->loss = qopt->loss;
	q->duplicate = qopt->duplicate;

	if (q->gap)
		q->reorder = ~0;

	if (tb[TCA_NETEM_CORR])
		get_correlation(sch, tb[TCA_NETEM_CORR]);

	if (tb[TCA_NETEM_DELAY_DIST]) {
		ret = get_dist_table(sch, tb[TCA_NETEM_DELAY_DIST]);
		if (ret)
			return ret;
	}

	if (tb[TCA_NETEM_REORDER])
		get_reorder(sch, tb[TCA_NETEM_REORDER]);

	if (tb[TCA_NETEM_CORRUPT])
		get_corrupt(sch, tb[TCA_NETEM_CORRUPT]);

	if (tb[TCA_NETEM_RATE])
		get_rate(sch, tb[TCA_NETEM_RATE]);

	q->loss_model = CLG_RANDOM;
	if (tb[TCA_NETEM_LOSS])
		ret = get_loss_clg(sch, tb[TCA_NETEM_LOSS]);

	return ret;
}

static int netem_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	int ret;

	if (!opt)
		return -EINVAL;

	qdisc_watchdog_init(&q->watchdog, sch);

	q->loss_model = CLG_RANDOM;
	ret = netem_change(sch, opt);
	if (ret)
		pr_info("netem: change failed\n");
	return ret;
}

static void netem_destroy(struct Qdisc *sch)
{
	struct netem_sched_data *q = qdisc_priv(sch);

	qdisc_watchdog_cancel(&q->watchdog);
	if (q->qdisc)
		qdisc_destroy(q->qdisc);
	dist_free(q->delay_dist);
}

static int dump_loss_model(const struct netem_sched_data *q,
			   struct sk_buff *skb)
{
	struct nlattr *nest;

	nest = nla_nest_start(skb, TCA_NETEM_LOSS);
	if (nest == NULL)
		goto nla_put_failure;

	switch (q->loss_model) {
	case CLG_RANDOM:
		
		nla_nest_cancel(skb, nest);
		return 0;	

	case CLG_4_STATES: {
		struct tc_netem_gimodel gi = {
			.p13 = q->clg.a1,
			.p31 = q->clg.a2,
			.p32 = q->clg.a3,
			.p14 = q->clg.a4,
			.p23 = q->clg.a5,
		};

		NLA_PUT(skb, NETEM_LOSS_GI, sizeof(gi), &gi);
		break;
	}
	case CLG_GILB_ELL: {
		struct tc_netem_gemodel ge = {
			.p = q->clg.a1,
			.r = q->clg.a2,
			.h = q->clg.a3,
			.k1 = q->clg.a4,
		};

		NLA_PUT(skb, NETEM_LOSS_GE, sizeof(ge), &ge);
		break;
	}
	}

	nla_nest_end(skb, nest);
	return 0;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}

static int netem_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	const struct netem_sched_data *q = qdisc_priv(sch);
	struct nlattr *nla = (struct nlattr *) skb_tail_pointer(skb);
	struct tc_netem_qopt qopt;
	struct tc_netem_corr cor;
	struct tc_netem_reorder reorder;
	struct tc_netem_corrupt corrupt;
	struct tc_netem_rate rate;

	qopt.latency = q->latency;
	qopt.jitter = q->jitter;
	qopt.limit = q->limit;
	qopt.loss = q->loss;
	qopt.gap = q->gap;
	qopt.duplicate = q->duplicate;
	NLA_PUT(skb, TCA_OPTIONS, sizeof(qopt), &qopt);

	cor.delay_corr = q->delay_cor.rho;
	cor.loss_corr = q->loss_cor.rho;
	cor.dup_corr = q->dup_cor.rho;
	NLA_PUT(skb, TCA_NETEM_CORR, sizeof(cor), &cor);

	reorder.probability = q->reorder;
	reorder.correlation = q->reorder_cor.rho;
	NLA_PUT(skb, TCA_NETEM_REORDER, sizeof(reorder), &reorder);

	corrupt.probability = q->corrupt;
	corrupt.correlation = q->corrupt_cor.rho;
	NLA_PUT(skb, TCA_NETEM_CORRUPT, sizeof(corrupt), &corrupt);

	rate.rate = q->rate;
	rate.packet_overhead = q->packet_overhead;
	rate.cell_size = q->cell_size;
	rate.cell_overhead = q->cell_overhead;
	NLA_PUT(skb, TCA_NETEM_RATE, sizeof(rate), &rate);

	if (dump_loss_model(q, skb) != 0)
		goto nla_put_failure;

	return nla_nest_end(skb, nla);

nla_put_failure:
	nlmsg_trim(skb, nla);
	return -1;
}

static int netem_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	struct netem_sched_data *q = qdisc_priv(sch);

	if (cl != 1 || !q->qdisc) 	
		return -ENOENT;

	tcm->tcm_handle |= TC_H_MIN(1);
	tcm->tcm_info = q->qdisc->handle;

	return 0;
}

static int netem_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct netem_sched_data *q = qdisc_priv(sch);

	sch_tree_lock(sch);
	*old = q->qdisc;
	q->qdisc = new;
	if (*old) {
		qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
		qdisc_reset(*old);
	}
	sch_tree_unlock(sch);

	return 0;
}

static struct Qdisc *netem_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct netem_sched_data *q = qdisc_priv(sch);
	return q->qdisc;
}

static unsigned long netem_get(struct Qdisc *sch, u32 classid)
{
	return 1;
}

static void netem_put(struct Qdisc *sch, unsigned long arg)
{
}

static void netem_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
	if (!walker->stop) {
		if (walker->count >= walker->skip)
			if (walker->fn(sch, 1, walker) < 0) {
				walker->stop = 1;
				return;
			}
		walker->count++;
	}
}

static const struct Qdisc_class_ops netem_class_ops = {
	.graft		=	netem_graft,
	.leaf		=	netem_leaf,
	.get		=	netem_get,
	.put		=	netem_put,
	.walk		=	netem_walk,
	.dump		=	netem_dump_class,
};

static struct Qdisc_ops netem_qdisc_ops __read_mostly = {
	.id		=	"netem",
	.cl_ops		=	&netem_class_ops,
	.priv_size	=	sizeof(struct netem_sched_data),
	.enqueue	=	netem_enqueue,
	.dequeue	=	netem_dequeue,
	.peek		=	qdisc_peek_dequeued,
	.drop		=	netem_drop,
	.init		=	netem_init,
	.reset		=	netem_reset,
	.destroy	=	netem_destroy,
	.change		=	netem_change,
	.dump		=	netem_dump,
	.owner		=	THIS_MODULE,
};


static int __init netem_module_init(void)
{
	pr_info("netem: version " VERSION "\n");
	return register_qdisc(&netem_qdisc_ops);
}
static void __exit netem_module_exit(void)
{
	unregister_qdisc(&netem_qdisc_ops);
}
module_init(netem_module_init)
module_exit(netem_module_exit)
MODULE_LICENSE("GPL");
