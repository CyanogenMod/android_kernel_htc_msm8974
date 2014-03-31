
/* Written 1998-2000 by Werner Almesberger, EPFL ICA */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/atmdev.h>
#include <linux/atmclip.h>
#include <linux/rtnetlink.h>
#include <linux/file.h>		
#include <net/netlink.h>
#include <net/pkt_sched.h>

extern struct socket *sockfd_lookup(int fd, int *err);	


#define VCC2FLOW(vcc) ((struct atm_flow_data *) ((vcc)->user_back))

struct atm_flow_data {
	struct Qdisc		*q;	
	struct tcf_proto	*filter_list;
	struct atm_vcc		*vcc;	
	void			(*old_pop)(struct atm_vcc *vcc,
					   struct sk_buff *skb); 
	struct atm_qdisc_data	*parent;	
	struct socket		*sock;		
	u32			classid;	
	int			ref;		
	struct gnet_stats_basic_packed	bstats;
	struct gnet_stats_queue	qstats;
	struct list_head	list;
	struct atm_flow_data	*excess;	
	int			hdr_len;
	unsigned char		hdr[0];		
};

struct atm_qdisc_data {
	struct atm_flow_data	link;		
	struct list_head	flows;		
	struct tasklet_struct	task;		
};


static inline struct atm_flow_data *lookup_flow(struct Qdisc *sch, u32 classid)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;

	list_for_each_entry(flow, &p->flows, list) {
		if (flow->classid == classid)
			return flow;
	}
	return NULL;
}

static int atm_tc_graft(struct Qdisc *sch, unsigned long arg,
			struct Qdisc *new, struct Qdisc **old)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)arg;

	pr_debug("atm_tc_graft(sch %p,[qdisc %p],flow %p,new %p,old %p)\n",
		sch, p, flow, new, old);
	if (list_empty(&flow->list))
		return -EINVAL;
	if (!new)
		new = &noop_qdisc;
	*old = flow->q;
	flow->q = new;
	if (*old)
		qdisc_reset(*old);
	return 0;
}

static struct Qdisc *atm_tc_leaf(struct Qdisc *sch, unsigned long cl)
{
	struct atm_flow_data *flow = (struct atm_flow_data *)cl;

	pr_debug("atm_tc_leaf(sch %p,flow %p)\n", sch, flow);
	return flow ? flow->q : NULL;
}

static unsigned long atm_tc_get(struct Qdisc *sch, u32 classid)
{
	struct atm_qdisc_data *p __maybe_unused = qdisc_priv(sch);
	struct atm_flow_data *flow;

	pr_debug("atm_tc_get(sch %p,[qdisc %p],classid %x)\n", sch, p, classid);
	flow = lookup_flow(sch, classid);
	if (flow)
		flow->ref++;
	pr_debug("atm_tc_get: flow %p\n", flow);
	return (unsigned long)flow;
}

static unsigned long atm_tc_bind_filter(struct Qdisc *sch,
					unsigned long parent, u32 classid)
{
	return atm_tc_get(sch, classid);
}

static void atm_tc_put(struct Qdisc *sch, unsigned long cl)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)cl;

	pr_debug("atm_tc_put(sch %p,[qdisc %p],flow %p)\n", sch, p, flow);
	if (--flow->ref)
		return;
	pr_debug("atm_tc_put: destroying\n");
	list_del_init(&flow->list);
	pr_debug("atm_tc_put: qdisc %p\n", flow->q);
	qdisc_destroy(flow->q);
	tcf_destroy_chain(&flow->filter_list);
	if (flow->sock) {
		pr_debug("atm_tc_put: f_count %ld\n",
			file_count(flow->sock->file));
		flow->vcc->pop = flow->old_pop;
		sockfd_put(flow->sock);
	}
	if (flow->excess)
		atm_tc_put(sch, (unsigned long)flow->excess);
	if (flow != &p->link)
		kfree(flow);
}

static void sch_atm_pop(struct atm_vcc *vcc, struct sk_buff *skb)
{
	struct atm_qdisc_data *p = VCC2FLOW(vcc)->parent;

	pr_debug("sch_atm_pop(vcc %p,skb %p,[qdisc %p])\n", vcc, skb, p);
	VCC2FLOW(vcc)->old_pop(vcc, skb);
	tasklet_schedule(&p->task);
}

static const u8 llc_oui_ip[] = {
	0xaa,			
	0xaa,			
	0x03,			
	0x00,			
	0x00, 0x00,
	0x08, 0x00
};				

static const struct nla_policy atm_policy[TCA_ATM_MAX + 1] = {
	[TCA_ATM_FD]		= { .type = NLA_U32 },
	[TCA_ATM_EXCESS]	= { .type = NLA_U32 },
};

static int atm_tc_change(struct Qdisc *sch, u32 classid, u32 parent,
			 struct nlattr **tca, unsigned long *arg)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)*arg;
	struct atm_flow_data *excess = NULL;
	struct nlattr *opt = tca[TCA_OPTIONS];
	struct nlattr *tb[TCA_ATM_MAX + 1];
	struct socket *sock;
	int fd, error, hdr_len;
	void *hdr;

	pr_debug("atm_tc_change(sch %p,[qdisc %p],classid %x,parent %x,"
		"flow %p,opt %p)\n", sch, p, classid, parent, flow, opt);
	if (parent && parent != TC_H_ROOT && parent != sch->handle)
		return -EINVAL;
	if (flow)
		return -EBUSY;
	if (opt == NULL)
		return -EINVAL;

	error = nla_parse_nested(tb, TCA_ATM_MAX, opt, atm_policy);
	if (error < 0)
		return error;

	if (!tb[TCA_ATM_FD])
		return -EINVAL;
	fd = nla_get_u32(tb[TCA_ATM_FD]);
	pr_debug("atm_tc_change: fd %d\n", fd);
	if (tb[TCA_ATM_HDR]) {
		hdr_len = nla_len(tb[TCA_ATM_HDR]);
		hdr = nla_data(tb[TCA_ATM_HDR]);
	} else {
		hdr_len = RFC1483LLC_LEN;
		hdr = NULL;	
	}
	if (!tb[TCA_ATM_EXCESS])
		excess = NULL;
	else {
		excess = (struct atm_flow_data *)
			atm_tc_get(sch, nla_get_u32(tb[TCA_ATM_EXCESS]));
		if (!excess)
			return -ENOENT;
	}
	pr_debug("atm_tc_change: type %d, payload %d, hdr_len %d\n",
		 opt->nla_type, nla_len(opt), hdr_len);
	sock = sockfd_lookup(fd, &error);
	if (!sock)
		return error;	
	pr_debug("atm_tc_change: f_count %ld\n", file_count(sock->file));
	if (sock->ops->family != PF_ATMSVC && sock->ops->family != PF_ATMPVC) {
		error = -EPROTOTYPE;
		goto err_out;
	}
	if (classid) {
		if (TC_H_MAJ(classid ^ sch->handle)) {
			pr_debug("atm_tc_change: classid mismatch\n");
			error = -EINVAL;
			goto err_out;
		}
	} else {
		int i;
		unsigned long cl;

		for (i = 1; i < 0x8000; i++) {
			classid = TC_H_MAKE(sch->handle, 0x8000 | i);
			cl = atm_tc_get(sch, classid);
			if (!cl)
				break;
			atm_tc_put(sch, cl);
		}
	}
	pr_debug("atm_tc_change: new id %x\n", classid);
	flow = kzalloc(sizeof(struct atm_flow_data) + hdr_len, GFP_KERNEL);
	pr_debug("atm_tc_change: flow %p\n", flow);
	if (!flow) {
		error = -ENOBUFS;
		goto err_out;
	}
	flow->filter_list = NULL;
	flow->q = qdisc_create_dflt(sch->dev_queue, &pfifo_qdisc_ops, classid);
	if (!flow->q)
		flow->q = &noop_qdisc;
	pr_debug("atm_tc_change: qdisc %p\n", flow->q);
	flow->sock = sock;
	flow->vcc = ATM_SD(sock);	
	flow->vcc->user_back = flow;
	pr_debug("atm_tc_change: vcc %p\n", flow->vcc);
	flow->old_pop = flow->vcc->pop;
	flow->parent = p;
	flow->vcc->pop = sch_atm_pop;
	flow->classid = classid;
	flow->ref = 1;
	flow->excess = excess;
	list_add(&flow->list, &p->link.list);
	flow->hdr_len = hdr_len;
	if (hdr)
		memcpy(flow->hdr, hdr, hdr_len);
	else
		memcpy(flow->hdr, llc_oui_ip, sizeof(llc_oui_ip));
	*arg = (unsigned long)flow;
	return 0;
err_out:
	if (excess)
		atm_tc_put(sch, (unsigned long)excess);
	sockfd_put(sock);
	return error;
}

static int atm_tc_delete(struct Qdisc *sch, unsigned long arg)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)arg;

	pr_debug("atm_tc_delete(sch %p,[qdisc %p],flow %p)\n", sch, p, flow);
	if (list_empty(&flow->list))
		return -EINVAL;
	if (flow->filter_list || flow == &p->link)
		return -EBUSY;
	if (flow->ref < 2) {
		pr_err("atm_tc_delete: flow->ref == %d\n", flow->ref);
		return -EINVAL;
	}
	if (flow->ref > 2)
		return -EBUSY;	
	atm_tc_put(sch, arg);
	return 0;
}

static void atm_tc_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;

	pr_debug("atm_tc_walk(sch %p,[qdisc %p],walker %p)\n", sch, p, walker);
	if (walker->stop)
		return;
	list_for_each_entry(flow, &p->flows, list) {
		if (walker->count >= walker->skip &&
		    walker->fn(sch, (unsigned long)flow, walker) < 0) {
			walker->stop = 1;
			break;
		}
		walker->count++;
	}
}

static struct tcf_proto **atm_tc_find_tcf(struct Qdisc *sch, unsigned long cl)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)cl;

	pr_debug("atm_tc_find_tcf(sch %p,[qdisc %p],flow %p)\n", sch, p, flow);
	return flow ? &flow->filter_list : &p->link.filter_list;
}


static int atm_tc_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;
	struct tcf_result res;
	int result;
	int ret = NET_XMIT_POLICED;

	pr_debug("atm_tc_enqueue(skb %p,sch %p,[qdisc %p])\n", skb, sch, p);
	result = TC_POLICE_OK;	
	flow = NULL;
	if (TC_H_MAJ(skb->priority) != sch->handle ||
	    !(flow = (struct atm_flow_data *)atm_tc_get(sch, skb->priority))) {
		list_for_each_entry(flow, &p->flows, list) {
			if (flow->filter_list) {
				result = tc_classify_compat(skb,
							    flow->filter_list,
							    &res);
				if (result < 0)
					continue;
				flow = (struct atm_flow_data *)res.class;
				if (!flow)
					flow = lookup_flow(sch, res.classid);
				goto done;
			}
		}
		flow = NULL;
done:
		;
	}
	if (!flow) {
		flow = &p->link;
	} else {
		if (flow->vcc)
			ATM_SKB(skb)->atm_options = flow->vcc->atm_options;
		
#ifdef CONFIG_NET_CLS_ACT
		switch (result) {
		case TC_ACT_QUEUED:
		case TC_ACT_STOLEN:
			kfree_skb(skb);
			return NET_XMIT_SUCCESS | __NET_XMIT_STOLEN;
		case TC_ACT_SHOT:
			kfree_skb(skb);
			goto drop;
		case TC_POLICE_RECLASSIFY:
			if (flow->excess)
				flow = flow->excess;
			else
				ATM_SKB(skb)->atm_options |= ATM_ATMOPT_CLP;
			break;
		}
#endif
	}

	ret = qdisc_enqueue(skb, flow->q);
	if (ret != NET_XMIT_SUCCESS) {
drop: __maybe_unused
		if (net_xmit_drop_count(ret)) {
			sch->qstats.drops++;
			if (flow)
				flow->qstats.drops++;
		}
		return ret;
	}
	qdisc_bstats_update(sch, skb);
	bstats_update(&flow->bstats, skb);
	if (flow == &p->link) {
		sch->q.qlen++;
		return NET_XMIT_SUCCESS;
	}
	tasklet_schedule(&p->task);
	return NET_XMIT_SUCCESS | __NET_XMIT_BYPASS;
}


static void sch_atm_dequeue(unsigned long data)
{
	struct Qdisc *sch = (struct Qdisc *)data;
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;
	struct sk_buff *skb;

	pr_debug("sch_atm_dequeue(sch %p,[qdisc %p])\n", sch, p);
	list_for_each_entry(flow, &p->flows, list) {
		if (flow == &p->link)
			continue;
		while ((skb = flow->q->ops->peek(flow->q))) {
			if (!atm_may_send(flow->vcc, skb->truesize))
				break;

			skb = qdisc_dequeue_peeked(flow->q);
			if (unlikely(!skb))
				break;

			pr_debug("atm_tc_dequeue: sending on class %p\n", flow);
			
			skb_pull(skb, skb_network_offset(skb));
			if (skb_headroom(skb) < flow->hdr_len) {
				struct sk_buff *new;

				new = skb_realloc_headroom(skb, flow->hdr_len);
				dev_kfree_skb(skb);
				if (!new)
					continue;
				skb = new;
			}
			pr_debug("sch_atm_dequeue: ip %p, data %p\n",
				 skb_network_header(skb), skb->data);
			ATM_SKB(skb)->vcc = flow->vcc;
			memcpy(skb_push(skb, flow->hdr_len), flow->hdr,
			       flow->hdr_len);
			atomic_add(skb->truesize,
				   &sk_atm(flow->vcc)->sk_wmem_alloc);
			
			flow->vcc->send(flow->vcc, skb);
		}
	}
}

static struct sk_buff *atm_tc_dequeue(struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct sk_buff *skb;

	pr_debug("atm_tc_dequeue(sch %p,[qdisc %p])\n", sch, p);
	tasklet_schedule(&p->task);
	skb = qdisc_dequeue_peeked(p->link.q);
	if (skb)
		sch->q.qlen--;
	return skb;
}

static struct sk_buff *atm_tc_peek(struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);

	pr_debug("atm_tc_peek(sch %p,[qdisc %p])\n", sch, p);

	return p->link.q->ops->peek(p->link.q);
}

static unsigned int atm_tc_drop(struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;
	unsigned int len;

	pr_debug("atm_tc_drop(sch %p,[qdisc %p])\n", sch, p);
	list_for_each_entry(flow, &p->flows, list) {
		if (flow->q->ops->drop && (len = flow->q->ops->drop(flow->q)))
			return len;
	}
	return 0;
}

static int atm_tc_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);

	pr_debug("atm_tc_init(sch %p,[qdisc %p],opt %p)\n", sch, p, opt);
	INIT_LIST_HEAD(&p->flows);
	INIT_LIST_HEAD(&p->link.list);
	list_add(&p->link.list, &p->flows);
	p->link.q = qdisc_create_dflt(sch->dev_queue,
				      &pfifo_qdisc_ops, sch->handle);
	if (!p->link.q)
		p->link.q = &noop_qdisc;
	pr_debug("atm_tc_init: link (%p) qdisc %p\n", &p->link, p->link.q);
	p->link.filter_list = NULL;
	p->link.vcc = NULL;
	p->link.sock = NULL;
	p->link.classid = sch->handle;
	p->link.ref = 1;
	tasklet_init(&p->task, sch_atm_dequeue, (unsigned long)sch);
	return 0;
}

static void atm_tc_reset(struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow;

	pr_debug("atm_tc_reset(sch %p,[qdisc %p])\n", sch, p);
	list_for_each_entry(flow, &p->flows, list)
		qdisc_reset(flow->q);
	sch->q.qlen = 0;
}

static void atm_tc_destroy(struct Qdisc *sch)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow, *tmp;

	pr_debug("atm_tc_destroy(sch %p,[qdisc %p])\n", sch, p);
	list_for_each_entry(flow, &p->flows, list)
		tcf_destroy_chain(&flow->filter_list);

	list_for_each_entry_safe(flow, tmp, &p->flows, list) {
		if (flow->ref > 1)
			pr_err("atm_destroy: %p->ref = %d\n", flow, flow->ref);
		atm_tc_put(sch, (unsigned long)flow);
	}
	tasklet_kill(&p->task);
}

static int atm_tc_dump_class(struct Qdisc *sch, unsigned long cl,
			     struct sk_buff *skb, struct tcmsg *tcm)
{
	struct atm_qdisc_data *p = qdisc_priv(sch);
	struct atm_flow_data *flow = (struct atm_flow_data *)cl;
	struct nlattr *nest;

	pr_debug("atm_tc_dump_class(sch %p,[qdisc %p],flow %p,skb %p,tcm %p)\n",
		sch, p, flow, skb, tcm);
	if (list_empty(&flow->list))
		return -EINVAL;
	tcm->tcm_handle = flow->classid;
	tcm->tcm_info = flow->q->handle;

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;

	NLA_PUT(skb, TCA_ATM_HDR, flow->hdr_len, flow->hdr);
	if (flow->vcc) {
		struct sockaddr_atmpvc pvc;
		int state;

		pvc.sap_family = AF_ATMPVC;
		pvc.sap_addr.itf = flow->vcc->dev ? flow->vcc->dev->number : -1;
		pvc.sap_addr.vpi = flow->vcc->vpi;
		pvc.sap_addr.vci = flow->vcc->vci;
		NLA_PUT(skb, TCA_ATM_ADDR, sizeof(pvc), &pvc);
		state = ATM_VF2VS(flow->vcc->flags);
		NLA_PUT_U32(skb, TCA_ATM_STATE, state);
	}
	if (flow->excess)
		NLA_PUT_U32(skb, TCA_ATM_EXCESS, flow->classid);
	else
		NLA_PUT_U32(skb, TCA_ATM_EXCESS, 0);

	nla_nest_end(skb, nest);
	return skb->len;

nla_put_failure:
	nla_nest_cancel(skb, nest);
	return -1;
}
static int
atm_tc_dump_class_stats(struct Qdisc *sch, unsigned long arg,
			struct gnet_dump *d)
{
	struct atm_flow_data *flow = (struct atm_flow_data *)arg;

	flow->qstats.qlen = flow->q->q.qlen;

	if (gnet_stats_copy_basic(d, &flow->bstats) < 0 ||
	    gnet_stats_copy_queue(d, &flow->qstats) < 0)
		return -1;

	return 0;
}

static int atm_tc_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	return 0;
}

static const struct Qdisc_class_ops atm_class_ops = {
	.graft		= atm_tc_graft,
	.leaf		= atm_tc_leaf,
	.get		= atm_tc_get,
	.put		= atm_tc_put,
	.change		= atm_tc_change,
	.delete		= atm_tc_delete,
	.walk		= atm_tc_walk,
	.tcf_chain	= atm_tc_find_tcf,
	.bind_tcf	= atm_tc_bind_filter,
	.unbind_tcf	= atm_tc_put,
	.dump		= atm_tc_dump_class,
	.dump_stats	= atm_tc_dump_class_stats,
};

static struct Qdisc_ops atm_qdisc_ops __read_mostly = {
	.cl_ops		= &atm_class_ops,
	.id		= "atm",
	.priv_size	= sizeof(struct atm_qdisc_data),
	.enqueue	= atm_tc_enqueue,
	.dequeue	= atm_tc_dequeue,
	.peek		= atm_tc_peek,
	.drop		= atm_tc_drop,
	.init		= atm_tc_init,
	.reset		= atm_tc_reset,
	.destroy	= atm_tc_destroy,
	.dump		= atm_tc_dump,
	.owner		= THIS_MODULE,
};

static int __init atm_init(void)
{
	return register_qdisc(&atm_qdisc_ops);
}

static void __exit atm_exit(void)
{
	unregister_qdisc(&atm_qdisc_ops);
}

module_init(atm_init)
module_exit(atm_exit)
MODULE_LICENSE("GPL");
