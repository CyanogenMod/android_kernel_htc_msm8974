/*
 * IPVS:        Source Hashing scheduling module
 *
 * Authors:     Wensong Zhang <wensong@gnuchina.org>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Changes:
 *
 */


#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/ip.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>

#include <net/ip_vs.h>


struct ip_vs_sh_bucket {
	struct ip_vs_dest       *dest;          
};

#ifndef CONFIG_IP_VS_SH_TAB_BITS
#define CONFIG_IP_VS_SH_TAB_BITS        8
#endif
#define IP_VS_SH_TAB_BITS               CONFIG_IP_VS_SH_TAB_BITS
#define IP_VS_SH_TAB_SIZE               (1 << IP_VS_SH_TAB_BITS)
#define IP_VS_SH_TAB_MASK               (IP_VS_SH_TAB_SIZE - 1)


static inline unsigned ip_vs_sh_hashkey(int af, const union nf_inet_addr *addr)
{
	__be32 addr_fold = addr->ip;

#ifdef CONFIG_IP_VS_IPV6
	if (af == AF_INET6)
		addr_fold = addr->ip6[0]^addr->ip6[1]^
			    addr->ip6[2]^addr->ip6[3];
#endif
	return (ntohl(addr_fold)*2654435761UL) & IP_VS_SH_TAB_MASK;
}


static inline struct ip_vs_dest *
ip_vs_sh_get(int af, struct ip_vs_sh_bucket *tbl,
	     const union nf_inet_addr *addr)
{
	return (tbl[ip_vs_sh_hashkey(af, addr)]).dest;
}


static int
ip_vs_sh_assign(struct ip_vs_sh_bucket *tbl, struct ip_vs_service *svc)
{
	int i;
	struct ip_vs_sh_bucket *b;
	struct list_head *p;
	struct ip_vs_dest *dest;
	int d_count;

	b = tbl;
	p = &svc->destinations;
	d_count = 0;
	for (i=0; i<IP_VS_SH_TAB_SIZE; i++) {
		if (list_empty(p)) {
			b->dest = NULL;
		} else {
			if (p == &svc->destinations)
				p = p->next;

			dest = list_entry(p, struct ip_vs_dest, n_list);
			atomic_inc(&dest->refcnt);
			b->dest = dest;

			IP_VS_DBG_BUF(6, "assigned i: %d dest: %s weight: %d\n",
				      i, IP_VS_DBG_ADDR(svc->af, &dest->addr),
				      atomic_read(&dest->weight));

			
			if (++d_count >= atomic_read(&dest->weight)) {
				p = p->next;
				d_count = 0;
			}

		}
		b++;
	}
	return 0;
}


static void ip_vs_sh_flush(struct ip_vs_sh_bucket *tbl)
{
	int i;
	struct ip_vs_sh_bucket *b;

	b = tbl;
	for (i=0; i<IP_VS_SH_TAB_SIZE; i++) {
		if (b->dest) {
			atomic_dec(&b->dest->refcnt);
			b->dest = NULL;
		}
		b++;
	}
}


static int ip_vs_sh_init_svc(struct ip_vs_service *svc)
{
	struct ip_vs_sh_bucket *tbl;

	
	tbl = kmalloc(sizeof(struct ip_vs_sh_bucket)*IP_VS_SH_TAB_SIZE,
		      GFP_ATOMIC);
	if (tbl == NULL)
		return -ENOMEM;

	svc->sched_data = tbl;
	IP_VS_DBG(6, "SH hash table (memory=%Zdbytes) allocated for "
		  "current service\n",
		  sizeof(struct ip_vs_sh_bucket)*IP_VS_SH_TAB_SIZE);

	
	ip_vs_sh_assign(tbl, svc);

	return 0;
}


static int ip_vs_sh_done_svc(struct ip_vs_service *svc)
{
	struct ip_vs_sh_bucket *tbl = svc->sched_data;

	
	ip_vs_sh_flush(tbl);

	
	kfree(svc->sched_data);
	IP_VS_DBG(6, "SH hash table (memory=%Zdbytes) released\n",
		  sizeof(struct ip_vs_sh_bucket)*IP_VS_SH_TAB_SIZE);

	return 0;
}


static int ip_vs_sh_update_svc(struct ip_vs_service *svc)
{
	struct ip_vs_sh_bucket *tbl = svc->sched_data;

	
	ip_vs_sh_flush(tbl);

	
	ip_vs_sh_assign(tbl, svc);

	return 0;
}


static inline int is_overloaded(struct ip_vs_dest *dest)
{
	return dest->flags & IP_VS_DEST_F_OVERLOAD;
}


static struct ip_vs_dest *
ip_vs_sh_schedule(struct ip_vs_service *svc, const struct sk_buff *skb)
{
	struct ip_vs_dest *dest;
	struct ip_vs_sh_bucket *tbl;
	struct ip_vs_iphdr iph;

	ip_vs_fill_iphdr(svc->af, skb_network_header(skb), &iph);

	IP_VS_DBG(6, "ip_vs_sh_schedule(): Scheduling...\n");

	tbl = (struct ip_vs_sh_bucket *)svc->sched_data;
	dest = ip_vs_sh_get(svc->af, tbl, &iph.saddr);
	if (!dest
	    || !(dest->flags & IP_VS_DEST_F_AVAILABLE)
	    || atomic_read(&dest->weight) <= 0
	    || is_overloaded(dest)) {
		ip_vs_scheduler_err(svc, "no destination available");
		return NULL;
	}

	IP_VS_DBG_BUF(6, "SH: source IP address %s --> server %s:%d\n",
		      IP_VS_DBG_ADDR(svc->af, &iph.saddr),
		      IP_VS_DBG_ADDR(svc->af, &dest->addr),
		      ntohs(dest->port));

	return dest;
}


static struct ip_vs_scheduler ip_vs_sh_scheduler =
{
	.name =			"sh",
	.refcnt =		ATOMIC_INIT(0),
	.module =		THIS_MODULE,
	.n_list	 =		LIST_HEAD_INIT(ip_vs_sh_scheduler.n_list),
	.init_service =		ip_vs_sh_init_svc,
	.done_service =		ip_vs_sh_done_svc,
	.update_service =	ip_vs_sh_update_svc,
	.schedule =		ip_vs_sh_schedule,
};


static int __init ip_vs_sh_init(void)
{
	return register_ip_vs_scheduler(&ip_vs_sh_scheduler);
}


static void __exit ip_vs_sh_cleanup(void)
{
	unregister_ip_vs_scheduler(&ip_vs_sh_scheduler);
}


module_init(ip_vs_sh_init);
module_exit(ip_vs_sh_cleanup);
MODULE_LICENSE("GPL");
