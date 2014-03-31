/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Nokia, Inc.
 * Copyright (c) 2001 La Monte H.P. Yarroll
 *
 * This file is part of the SCTP kernel implementation
 *
 * Initialization/cleanup for SCTP protocol support.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson <karl@athena.chicago.il.us>
 *    Jon Grimm <jgrimm@us.ibm.com>
 *    Sridhar Samudrala <sri@us.ibm.com>
 *    Daisy Chang <daisyc@us.ibm.com>
 *    Ardelle Fan <ardelle.fan@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/seq_file.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/protocol.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/route.h>
#include <net/sctp/sctp.h>
#include <net/addrconf.h>
#include <net/inet_common.h>
#include <net/inet_ecn.h>

struct sctp_globals sctp_globals __read_mostly;
DEFINE_SNMP_STAT(struct sctp_mib, sctp_statistics) __read_mostly;

#ifdef CONFIG_PROC_FS
struct proc_dir_entry	*proc_net_sctp;
#endif

struct idr sctp_assocs_id;
DEFINE_SPINLOCK(sctp_assocs_id_lock);

static struct sock *sctp_ctl_sock;

static struct sctp_pf *sctp_pf_inet6_specific;
static struct sctp_pf *sctp_pf_inet_specific;
static struct sctp_af *sctp_af_v4_specific;
static struct sctp_af *sctp_af_v6_specific;

struct kmem_cache *sctp_chunk_cachep __read_mostly;
struct kmem_cache *sctp_bucket_cachep __read_mostly;

long sysctl_sctp_mem[3];
int sysctl_sctp_rmem[3];
int sysctl_sctp_wmem[3];

struct sock *sctp_get_ctl_sock(void)
{
	return sctp_ctl_sock;
}

static __init int sctp_proc_init(void)
{
	if (percpu_counter_init(&sctp_sockets_allocated, 0))
		goto out_nomem;
#ifdef CONFIG_PROC_FS
	if (!proc_net_sctp) {
		proc_net_sctp = proc_mkdir("sctp", init_net.proc_net);
		if (!proc_net_sctp)
			goto out_free_percpu;
	}

	if (sctp_snmp_proc_init())
		goto out_snmp_proc_init;
	if (sctp_eps_proc_init())
		goto out_eps_proc_init;
	if (sctp_assocs_proc_init())
		goto out_assocs_proc_init;
	if (sctp_remaddr_proc_init())
		goto out_remaddr_proc_init;

	return 0;

out_remaddr_proc_init:
	sctp_assocs_proc_exit();
out_assocs_proc_init:
	sctp_eps_proc_exit();
out_eps_proc_init:
	sctp_snmp_proc_exit();
out_snmp_proc_init:
	if (proc_net_sctp) {
		proc_net_sctp = NULL;
		remove_proc_entry("sctp", init_net.proc_net);
	}
out_free_percpu:
	percpu_counter_destroy(&sctp_sockets_allocated);
#else
	return 0;
#endif 

out_nomem:
	return -ENOMEM;
}

static void sctp_proc_exit(void)
{
#ifdef CONFIG_PROC_FS
	sctp_snmp_proc_exit();
	sctp_eps_proc_exit();
	sctp_assocs_proc_exit();
	sctp_remaddr_proc_exit();

	if (proc_net_sctp) {
		proc_net_sctp = NULL;
		remove_proc_entry("sctp", init_net.proc_net);
	}
#endif
	percpu_counter_destroy(&sctp_sockets_allocated);
}

static void sctp_v4_copy_addrlist(struct list_head *addrlist,
				  struct net_device *dev)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	struct sctp_sockaddr_entry *addr;

	rcu_read_lock();
	if ((in_dev = __in_dev_get_rcu(dev)) == NULL) {
		rcu_read_unlock();
		return;
	}

	for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next) {
		
		addr = t_new(struct sctp_sockaddr_entry, GFP_ATOMIC);
		if (addr) {
			addr->a.v4.sin_family = AF_INET;
			addr->a.v4.sin_port = 0;
			addr->a.v4.sin_addr.s_addr = ifa->ifa_local;
			addr->valid = 1;
			INIT_LIST_HEAD(&addr->list);
			list_add_tail(&addr->list, addrlist);
		}
	}

	rcu_read_unlock();
}

static void sctp_get_local_addr_list(void)
{
	struct net_device *dev;
	struct list_head *pos;
	struct sctp_af *af;

	rcu_read_lock();
	for_each_netdev_rcu(&init_net, dev) {
		__list_for_each(pos, &sctp_address_families) {
			af = list_entry(pos, struct sctp_af, list);
			af->copy_addrlist(&sctp_local_addr_list, dev);
		}
	}
	rcu_read_unlock();
}

static void sctp_free_local_addr_list(void)
{
	struct sctp_sockaddr_entry *addr;
	struct list_head *pos, *temp;

	list_for_each_safe(pos, temp, &sctp_local_addr_list) {
		addr = list_entry(pos, struct sctp_sockaddr_entry, list);
		list_del(pos);
		kfree(addr);
	}
}

int sctp_copy_local_addr_list(struct sctp_bind_addr *bp, sctp_scope_t scope,
			      gfp_t gfp, int copy_flags)
{
	struct sctp_sockaddr_entry *addr;
	int error = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(addr, &sctp_local_addr_list, list) {
		if (!addr->valid)
			continue;
		if (sctp_in_scope(&addr->a, scope)) {
			if ((((AF_INET == addr->a.sa.sa_family) &&
			      (copy_flags & SCTP_ADDR4_PEERSUPP))) ||
			    (((AF_INET6 == addr->a.sa.sa_family) &&
			      (copy_flags & SCTP_ADDR6_ALLOWED) &&
			      (copy_flags & SCTP_ADDR6_PEERSUPP)))) {
				error = sctp_add_bind_addr(bp, &addr->a,
						    SCTP_ADDR_SRC, GFP_ATOMIC);
				if (error)
					goto end_copy;
			}
		}
	}

end_copy:
	rcu_read_unlock();
	return error;
}

static void sctp_v4_from_skb(union sctp_addr *addr, struct sk_buff *skb,
			     int is_saddr)
{
	void *from;
	__be16 *port;
	struct sctphdr *sh;

	port = &addr->v4.sin_port;
	addr->v4.sin_family = AF_INET;

	sh = sctp_hdr(skb);
	if (is_saddr) {
		*port  = sh->source;
		from = &ip_hdr(skb)->saddr;
	} else {
		*port = sh->dest;
		from = &ip_hdr(skb)->daddr;
	}
	memcpy(&addr->v4.sin_addr.s_addr, from, sizeof(struct in_addr));
}

static void sctp_v4_from_sk(union sctp_addr *addr, struct sock *sk)
{
	addr->v4.sin_family = AF_INET;
	addr->v4.sin_port = 0;
	addr->v4.sin_addr.s_addr = inet_sk(sk)->inet_rcv_saddr;
}

static void sctp_v4_to_sk_saddr(union sctp_addr *addr, struct sock *sk)
{
	inet_sk(sk)->inet_rcv_saddr = addr->v4.sin_addr.s_addr;
}

static void sctp_v4_to_sk_daddr(union sctp_addr *addr, struct sock *sk)
{
	inet_sk(sk)->inet_daddr = addr->v4.sin_addr.s_addr;
}

static void sctp_v4_from_addr_param(union sctp_addr *addr,
				    union sctp_addr_param *param,
				    __be16 port, int iif)
{
	addr->v4.sin_family = AF_INET;
	addr->v4.sin_port = port;
	addr->v4.sin_addr.s_addr = param->v4.addr.s_addr;
}

static int sctp_v4_to_addr_param(const union sctp_addr *addr,
				 union sctp_addr_param *param)
{
	int length = sizeof(sctp_ipv4addr_param_t);

	param->v4.param_hdr.type = SCTP_PARAM_IPV4_ADDRESS;
	param->v4.param_hdr.length = htons(length);
	param->v4.addr.s_addr = addr->v4.sin_addr.s_addr;

	return length;
}

static void sctp_v4_dst_saddr(union sctp_addr *saddr, struct flowi4 *fl4,
			      __be16 port)
{
	saddr->v4.sin_family = AF_INET;
	saddr->v4.sin_port = port;
	saddr->v4.sin_addr.s_addr = fl4->saddr;
}

static int sctp_v4_cmp_addr(const union sctp_addr *addr1,
			    const union sctp_addr *addr2)
{
	if (addr1->sa.sa_family != addr2->sa.sa_family)
		return 0;
	if (addr1->v4.sin_port != addr2->v4.sin_port)
		return 0;
	if (addr1->v4.sin_addr.s_addr != addr2->v4.sin_addr.s_addr)
		return 0;

	return 1;
}

static void sctp_v4_inaddr_any(union sctp_addr *addr, __be16 port)
{
	addr->v4.sin_family = AF_INET;
	addr->v4.sin_addr.s_addr = htonl(INADDR_ANY);
	addr->v4.sin_port = port;
}

static int sctp_v4_is_any(const union sctp_addr *addr)
{
	return htonl(INADDR_ANY) == addr->v4.sin_addr.s_addr;
}

static int sctp_v4_addr_valid(union sctp_addr *addr,
			      struct sctp_sock *sp,
			      const struct sk_buff *skb)
{
	
	if (sp && ipv6_only_sock(sctp_opt2sk(sp)))
		return 0;

	
	if (IS_IPV4_UNUSABLE_ADDRESS(addr->v4.sin_addr.s_addr))
		return 0;

	
	if (skb && skb_rtable(skb)->rt_flags & RTCF_BROADCAST)
		return 0;

	return 1;
}

static int sctp_v4_available(union sctp_addr *addr, struct sctp_sock *sp)
{
	int ret = inet_addr_type(&init_net, addr->v4.sin_addr.s_addr);


	if (addr->v4.sin_addr.s_addr != htonl(INADDR_ANY) &&
	   ret != RTN_LOCAL &&
	   !sp->inet.freebind &&
	   !sysctl_ip_nonlocal_bind)
		return 0;

	if (ipv6_only_sock(sctp_opt2sk(sp)))
		return 0;

	return 1;
}

static sctp_scope_t sctp_v4_scope(union sctp_addr *addr)
{
	sctp_scope_t retval;

	
	if (IS_IPV4_UNUSABLE_ADDRESS(addr->v4.sin_addr.s_addr)) {
		retval =  SCTP_SCOPE_UNUSABLE;
	} else if (ipv4_is_loopback(addr->v4.sin_addr.s_addr)) {
		retval = SCTP_SCOPE_LOOPBACK;
	} else if (ipv4_is_linklocal_169(addr->v4.sin_addr.s_addr)) {
		retval = SCTP_SCOPE_LINK;
	} else if (ipv4_is_private_10(addr->v4.sin_addr.s_addr) ||
		   ipv4_is_private_172(addr->v4.sin_addr.s_addr) ||
		   ipv4_is_private_192(addr->v4.sin_addr.s_addr)) {
		retval = SCTP_SCOPE_PRIVATE;
	} else {
		retval = SCTP_SCOPE_GLOBAL;
	}

	return retval;
}

static void sctp_v4_get_dst(struct sctp_transport *t, union sctp_addr *saddr,
				struct flowi *fl, struct sock *sk)
{
	struct sctp_association *asoc = t->asoc;
	struct rtable *rt;
	struct flowi4 *fl4 = &fl->u.ip4;
	struct sctp_bind_addr *bp;
	struct sctp_sockaddr_entry *laddr;
	struct dst_entry *dst = NULL;
	union sctp_addr *daddr = &t->ipaddr;
	union sctp_addr dst_saddr;

	memset(fl4, 0x0, sizeof(struct flowi4));
	fl4->daddr  = daddr->v4.sin_addr.s_addr;
	fl4->fl4_dport = daddr->v4.sin_port;
	fl4->flowi4_proto = IPPROTO_SCTP;
	if (asoc) {
		fl4->flowi4_tos = RT_CONN_FLAGS(asoc->base.sk);
		fl4->flowi4_oif = asoc->base.sk->sk_bound_dev_if;
		fl4->fl4_sport = htons(asoc->base.bind_addr.port);
	}
	if (saddr) {
		fl4->saddr = saddr->v4.sin_addr.s_addr;
		fl4->fl4_sport = saddr->v4.sin_port;
	}

	SCTP_DEBUG_PRINTK("%s: DST:%pI4, SRC:%pI4 - ",
			  __func__, &fl4->daddr, &fl4->saddr);

	rt = ip_route_output_key(&init_net, fl4);
	if (!IS_ERR(rt))
		dst = &rt->dst;

	if (!asoc || saddr)
		goto out;

	bp = &asoc->base.bind_addr;

	if (dst) {
		sctp_v4_dst_saddr(&dst_saddr, fl4, htons(bp->port));
		rcu_read_lock();
		list_for_each_entry_rcu(laddr, &bp->address_list, list) {
			if (!laddr->valid || (laddr->state == SCTP_ADDR_DEL) ||
			    (laddr->state != SCTP_ADDR_SRC &&
			    !asoc->src_out_of_asoc_ok))
				continue;
			if (sctp_v4_cmp_addr(&dst_saddr, &laddr->a))
				goto out_unlock;
		}
		rcu_read_unlock();

		dst_release(dst);
		dst = NULL;
	}

	rcu_read_lock();
	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		if (!laddr->valid)
			continue;
		if ((laddr->state == SCTP_ADDR_SRC) &&
		    (AF_INET == laddr->a.sa.sa_family)) {
			fl4->saddr = laddr->a.v4.sin_addr.s_addr;
			fl4->fl4_sport = laddr->a.v4.sin_port;
			rt = ip_route_output_key(&init_net, fl4);
			if (!IS_ERR(rt)) {
				dst = &rt->dst;
				goto out_unlock;
			}
		}
	}

out_unlock:
	rcu_read_unlock();
out:
	t->dst = dst;
	if (dst)
		SCTP_DEBUG_PRINTK("rt_dst:%pI4, rt_src:%pI4\n",
				  &fl4->daddr, &fl4->saddr);
	else
		SCTP_DEBUG_PRINTK("NO ROUTE\n");
}

static void sctp_v4_get_saddr(struct sctp_sock *sk,
			      struct sctp_transport *t,
			      struct flowi *fl)
{
	union sctp_addr *saddr = &t->saddr;
	struct rtable *rt = (struct rtable *)t->dst;

	if (rt) {
		saddr->v4.sin_family = AF_INET;
		saddr->v4.sin_addr.s_addr = fl->u.ip4.saddr;
	}
}

static int sctp_v4_skb_iif(const struct sk_buff *skb)
{
	return skb_rtable(skb)->rt_iif;
}

static int sctp_v4_is_ce(const struct sk_buff *skb)
{
	return INET_ECN_is_ce(ip_hdr(skb)->tos);
}

static struct sock *sctp_v4_create_accept_sk(struct sock *sk,
					     struct sctp_association *asoc)
{
	struct sock *newsk = sk_alloc(sock_net(sk), PF_INET, GFP_KERNEL,
			sk->sk_prot);
	struct inet_sock *newinet;

	if (!newsk)
		goto out;

	sock_init_data(NULL, newsk);

	sctp_copy_sock(newsk, sk, asoc);
	sock_reset_flag(newsk, SOCK_ZAPPED);

	newinet = inet_sk(newsk);

	newinet->inet_daddr = asoc->peer.primary_addr.v4.sin_addr.s_addr;

	sk_refcnt_debug_inc(newsk);

	if (newsk->sk_prot->init(newsk)) {
		sk_common_release(newsk);
		newsk = NULL;
	}

out:
	return newsk;
}

static void sctp_v4_addr_v4map(struct sctp_sock *sp, union sctp_addr *addr)
{
	
}

static void sctp_v4_seq_dump_addr(struct seq_file *seq, union sctp_addr *addr)
{
	seq_printf(seq, "%pI4 ", &addr->v4.sin_addr);
}

static void sctp_v4_ecn_capable(struct sock *sk)
{
	INET_ECN_xmit(sk);
}

void sctp_addr_wq_timeout_handler(unsigned long arg)
{
	struct sctp_sockaddr_entry *addrw, *temp;
	struct sctp_sock *sp;

	spin_lock_bh(&sctp_addr_wq_lock);

	list_for_each_entry_safe(addrw, temp, &sctp_addr_waitq, list) {
		SCTP_DEBUG_PRINTK_IPADDR("sctp_addrwq_timo_handler: the first ent in wq %p is ",
		    " for cmd %d at entry %p\n", &sctp_addr_waitq, &addrw->a, addrw->state,
		    addrw);

#if IS_ENABLED(CONFIG_IPV6)
		
		
		if (addrw->a.sa.sa_family == AF_INET6) {
			struct in6_addr *in6;

			if (ipv6_addr_type(&addrw->a.v6.sin6_addr) &
			    IPV6_ADDR_LINKLOCAL)
				goto free_next;

			in6 = (struct in6_addr *)&addrw->a.v6.sin6_addr;
			if (ipv6_chk_addr(&init_net, in6, NULL, 0) == 0 &&
			    addrw->state == SCTP_ADDR_NEW) {
				unsigned long timeo_val;

				SCTP_DEBUG_PRINTK("sctp_timo_handler: this is on DAD, trying %d sec later\n",
				    SCTP_ADDRESS_TICK_DELAY);
				timeo_val = jiffies;
				timeo_val += msecs_to_jiffies(SCTP_ADDRESS_TICK_DELAY);
				mod_timer(&sctp_addr_wq_timer, timeo_val);
				break;
			}
		}
#endif
		list_for_each_entry(sp, &sctp_auto_asconf_splist, auto_asconf_list) {
			struct sock *sk;

			sk = sctp_opt2sk(sp);
			
			if (!sctp_is_ep_boundall(sk))
				continue;
			sctp_bh_lock_sock(sk);
			if (sctp_asconf_mgmt(sp, addrw) < 0)
				SCTP_DEBUG_PRINTK("sctp_addrwq_timo_handler: sctp_asconf_mgmt failed\n");
			sctp_bh_unlock_sock(sk);
		}
free_next:
		list_del(&addrw->list);
		kfree(addrw);
	}
	spin_unlock_bh(&sctp_addr_wq_lock);
}

static void sctp_free_addr_wq(void)
{
	struct sctp_sockaddr_entry *addrw;
	struct sctp_sockaddr_entry *temp;

	spin_lock_bh(&sctp_addr_wq_lock);
	del_timer(&sctp_addr_wq_timer);
	list_for_each_entry_safe(addrw, temp, &sctp_addr_waitq, list) {
		list_del(&addrw->list);
		kfree(addrw);
	}
	spin_unlock_bh(&sctp_addr_wq_lock);
}

static struct sctp_sockaddr_entry *sctp_addr_wq_lookup(struct sctp_sockaddr_entry *addr)
{
	struct sctp_sockaddr_entry *addrw;

	list_for_each_entry(addrw, &sctp_addr_waitq, list) {
		if (addrw->a.sa.sa_family != addr->a.sa.sa_family)
			continue;
		if (addrw->a.sa.sa_family == AF_INET) {
			if (addrw->a.v4.sin_addr.s_addr ==
			    addr->a.v4.sin_addr.s_addr)
				return addrw;
		} else if (addrw->a.sa.sa_family == AF_INET6) {
			if (ipv6_addr_equal(&addrw->a.v6.sin6_addr,
			    &addr->a.v6.sin6_addr))
				return addrw;
		}
	}
	return NULL;
}

void sctp_addr_wq_mgmt(struct sctp_sockaddr_entry *addr, int cmd)
{
	struct sctp_sockaddr_entry *addrw;
	unsigned long timeo_val;


	spin_lock_bh(&sctp_addr_wq_lock);
	
	addrw = sctp_addr_wq_lookup(addr);
	if (addrw) {
		if (addrw->state != cmd) {
			SCTP_DEBUG_PRINTK_IPADDR("sctp_addr_wq_mgmt offsets existing entry for %d ",
			    " in wq %p\n", addrw->state, &addrw->a,
			    &sctp_addr_waitq);
			list_del(&addrw->list);
			kfree(addrw);
		}
		spin_unlock_bh(&sctp_addr_wq_lock);
		return;
	}

	
	addrw = kmemdup(addr, sizeof(struct sctp_sockaddr_entry), GFP_ATOMIC);
	if (addrw == NULL) {
		spin_unlock_bh(&sctp_addr_wq_lock);
		return;
	}
	addrw->state = cmd;
	list_add_tail(&addrw->list, &sctp_addr_waitq);
	SCTP_DEBUG_PRINTK_IPADDR("sctp_addr_wq_mgmt add new entry for cmd:%d ",
	    " in wq %p\n", addrw->state, &addrw->a, &sctp_addr_waitq);

	if (!timer_pending(&sctp_addr_wq_timer)) {
		timeo_val = jiffies;
		timeo_val += msecs_to_jiffies(SCTP_ADDRESS_TICK_DELAY);
		mod_timer(&sctp_addr_wq_timer, timeo_val);
	}
	spin_unlock_bh(&sctp_addr_wq_lock);
}

static int sctp_inetaddr_event(struct notifier_block *this, unsigned long ev,
			       void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct sctp_sockaddr_entry *addr = NULL;
	struct sctp_sockaddr_entry *temp;
	int found = 0;

	if (!net_eq(dev_net(ifa->ifa_dev->dev), &init_net))
		return NOTIFY_DONE;

	switch (ev) {
	case NETDEV_UP:
		addr = kmalloc(sizeof(struct sctp_sockaddr_entry), GFP_ATOMIC);
		if (addr) {
			addr->a.v4.sin_family = AF_INET;
			addr->a.v4.sin_port = 0;
			addr->a.v4.sin_addr.s_addr = ifa->ifa_local;
			addr->valid = 1;
			spin_lock_bh(&sctp_local_addr_lock);
			list_add_tail_rcu(&addr->list, &sctp_local_addr_list);
			sctp_addr_wq_mgmt(addr, SCTP_ADDR_NEW);
			spin_unlock_bh(&sctp_local_addr_lock);
		}
		break;
	case NETDEV_DOWN:
		spin_lock_bh(&sctp_local_addr_lock);
		list_for_each_entry_safe(addr, temp,
					&sctp_local_addr_list, list) {
			if (addr->a.sa.sa_family == AF_INET &&
					addr->a.v4.sin_addr.s_addr ==
					ifa->ifa_local) {
				sctp_addr_wq_mgmt(addr, SCTP_ADDR_DEL);
				found = 1;
				addr->valid = 0;
				list_del_rcu(&addr->list);
				break;
			}
		}
		spin_unlock_bh(&sctp_local_addr_lock);
		if (found)
			kfree_rcu(addr, rcu);
		break;
	}

	return NOTIFY_DONE;
}

static int sctp_ctl_sock_init(void)
{
	int err;
	sa_family_t family = PF_INET;

	if (sctp_get_pf_specific(PF_INET6))
		family = PF_INET6;

	err = inet_ctl_sock_create(&sctp_ctl_sock, family,
				   SOCK_SEQPACKET, IPPROTO_SCTP, &init_net);

	
	if (err < 0 && family == PF_INET6)
		err = inet_ctl_sock_create(&sctp_ctl_sock, AF_INET,
					   SOCK_SEQPACKET, IPPROTO_SCTP,
					   &init_net);

	if (err < 0) {
		pr_err("Failed to create the SCTP control socket\n");
		return err;
	}
	return 0;
}

int sctp_register_af(struct sctp_af *af)
{
	switch (af->sa_family) {
	case AF_INET:
		if (sctp_af_v4_specific)
			return 0;
		sctp_af_v4_specific = af;
		break;
	case AF_INET6:
		if (sctp_af_v6_specific)
			return 0;
		sctp_af_v6_specific = af;
		break;
	default:
		return 0;
	}

	INIT_LIST_HEAD(&af->list);
	list_add_tail(&af->list, &sctp_address_families);
	return 1;
}

struct sctp_af *sctp_get_af_specific(sa_family_t family)
{
	switch (family) {
	case AF_INET:
		return sctp_af_v4_specific;
	case AF_INET6:
		return sctp_af_v6_specific;
	default:
		return NULL;
	}
}

static void sctp_inet_msgname(char *msgname, int *addr_len)
{
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in *)msgname;
	*addr_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
}

static void sctp_inet_event_msgname(struct sctp_ulpevent *event, char *msgname,
				    int *addr_len)
{
	struct sockaddr_in *sin, *sinfrom;

	if (msgname) {
		struct sctp_association *asoc;

		asoc = event->asoc;
		sctp_inet_msgname(msgname, addr_len);
		sin = (struct sockaddr_in *)msgname;
		sinfrom = &asoc->peer.primary_addr.v4;
		sin->sin_port = htons(asoc->peer.port);
		sin->sin_addr.s_addr = sinfrom->sin_addr.s_addr;
	}
}

static void sctp_inet_skb_msgname(struct sk_buff *skb, char *msgname, int *len)
{
	if (msgname) {
		struct sctphdr *sh = sctp_hdr(skb);
		struct sockaddr_in *sin = (struct sockaddr_in *)msgname;

		sctp_inet_msgname(msgname, len);
		sin->sin_port = sh->source;
		sin->sin_addr.s_addr = ip_hdr(skb)->saddr;
	}
}

static int sctp_inet_af_supported(sa_family_t family, struct sctp_sock *sp)
{
	
	return AF_INET == family;
}

static int sctp_inet_cmp_addr(const union sctp_addr *addr1,
			      const union sctp_addr *addr2,
			      struct sctp_sock *opt)
{
	
	if (addr1->sa.sa_family != addr2->sa.sa_family)
		return 0;
	if (htonl(INADDR_ANY) == addr1->v4.sin_addr.s_addr ||
	    htonl(INADDR_ANY) == addr2->v4.sin_addr.s_addr)
		return 1;
	if (addr1->v4.sin_addr.s_addr == addr2->v4.sin_addr.s_addr)
		return 1;

	return 0;
}

static int sctp_inet_bind_verify(struct sctp_sock *opt, union sctp_addr *addr)
{
	return sctp_v4_available(addr, opt);
}

static int sctp_inet_send_verify(struct sctp_sock *opt, union sctp_addr *addr)
{
	return 1;
}

static int sctp_inet_supported_addrs(const struct sctp_sock *opt,
				     __be16 *types)
{
	types[0] = SCTP_PARAM_IPV4_ADDRESS;
	return 1;
}

static inline int sctp_v4_xmit(struct sk_buff *skb,
			       struct sctp_transport *transport)
{
	struct inet_sock *inet = inet_sk(skb->sk);

	SCTP_DEBUG_PRINTK("%s: skb:%p, len:%d, src:%pI4, dst:%pI4\n",
			  __func__, skb, skb->len,
			  &transport->fl.u.ip4.saddr,
			  &transport->fl.u.ip4.daddr);

	inet->pmtudisc = transport->param_flags & SPP_PMTUD_ENABLE ?
			 IP_PMTUDISC_DO : IP_PMTUDISC_DONT;

	SCTP_INC_STATS(SCTP_MIB_OUTSCTPPACKS);
	return ip_queue_xmit(skb, &transport->fl);
}

static struct sctp_af sctp_af_inet;

static struct sctp_pf sctp_pf_inet = {
	.event_msgname = sctp_inet_event_msgname,
	.skb_msgname   = sctp_inet_skb_msgname,
	.af_supported  = sctp_inet_af_supported,
	.cmp_addr      = sctp_inet_cmp_addr,
	.bind_verify   = sctp_inet_bind_verify,
	.send_verify   = sctp_inet_send_verify,
	.supported_addrs = sctp_inet_supported_addrs,
	.create_accept_sk = sctp_v4_create_accept_sk,
	.addr_v4map	= sctp_v4_addr_v4map,
	.af            = &sctp_af_inet
};

static struct notifier_block sctp_inetaddr_notifier = {
	.notifier_call = sctp_inetaddr_event,
};

static const struct proto_ops inet_seqpacket_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,	
	.bind		   = inet_bind,
	.connect	   = inet_dgram_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = inet_getname,	
	.poll		   = sctp_poll,
	.ioctl		   = inet_ioctl,
	.listen		   = sctp_inet_listen,
	.shutdown	   = inet_shutdown,	
	.setsockopt	   = sock_common_setsockopt, 
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = sock_common_recvmsg,
	.mmap		   = sock_no_mmap,
	.sendpage	   = sock_no_sendpage,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_sock_common_setsockopt,
	.compat_getsockopt = compat_sock_common_getsockopt,
#endif
};

static struct inet_protosw sctp_seqpacket_protosw = {
	.type       = SOCK_SEQPACKET,
	.protocol   = IPPROTO_SCTP,
	.prot       = &sctp_prot,
	.ops        = &inet_seqpacket_ops,
	.no_check   = 0,
	.flags      = SCTP_PROTOSW_FLAG
};
static struct inet_protosw sctp_stream_protosw = {
	.type       = SOCK_STREAM,
	.protocol   = IPPROTO_SCTP,
	.prot       = &sctp_prot,
	.ops        = &inet_seqpacket_ops,
	.no_check   = 0,
	.flags      = SCTP_PROTOSW_FLAG
};

static const struct net_protocol sctp_protocol = {
	.handler     = sctp_rcv,
	.err_handler = sctp_v4_err,
	.no_policy   = 1,
};

static struct sctp_af sctp_af_inet = {
	.sa_family	   = AF_INET,
	.sctp_xmit	   = sctp_v4_xmit,
	.setsockopt	   = ip_setsockopt,
	.getsockopt	   = ip_getsockopt,
	.get_dst	   = sctp_v4_get_dst,
	.get_saddr	   = sctp_v4_get_saddr,
	.copy_addrlist	   = sctp_v4_copy_addrlist,
	.from_skb	   = sctp_v4_from_skb,
	.from_sk	   = sctp_v4_from_sk,
	.to_sk_saddr	   = sctp_v4_to_sk_saddr,
	.to_sk_daddr	   = sctp_v4_to_sk_daddr,
	.from_addr_param   = sctp_v4_from_addr_param,
	.to_addr_param	   = sctp_v4_to_addr_param,
	.cmp_addr	   = sctp_v4_cmp_addr,
	.addr_valid	   = sctp_v4_addr_valid,
	.inaddr_any	   = sctp_v4_inaddr_any,
	.is_any		   = sctp_v4_is_any,
	.available	   = sctp_v4_available,
	.scope		   = sctp_v4_scope,
	.skb_iif	   = sctp_v4_skb_iif,
	.is_ce		   = sctp_v4_is_ce,
	.seq_dump_addr	   = sctp_v4_seq_dump_addr,
	.ecn_capable	   = sctp_v4_ecn_capable,
	.net_header_len	   = sizeof(struct iphdr),
	.sockaddr_len	   = sizeof(struct sockaddr_in),
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_ip_setsockopt,
	.compat_getsockopt = compat_ip_getsockopt,
#endif
};

struct sctp_pf *sctp_get_pf_specific(sa_family_t family) {

	switch (family) {
	case PF_INET:
		return sctp_pf_inet_specific;
	case PF_INET6:
		return sctp_pf_inet6_specific;
	default:
		return NULL;
	}
}

int sctp_register_pf(struct sctp_pf *pf, sa_family_t family)
{
	switch (family) {
	case PF_INET:
		if (sctp_pf_inet_specific)
			return 0;
		sctp_pf_inet_specific = pf;
		break;
	case PF_INET6:
		if (sctp_pf_inet6_specific)
			return 0;
		sctp_pf_inet6_specific = pf;
		break;
	default:
		return 0;
	}
	return 1;
}

static inline int init_sctp_mibs(void)
{
	return snmp_mib_init((void __percpu **)sctp_statistics,
			     sizeof(struct sctp_mib),
			     __alignof__(struct sctp_mib));
}

static inline void cleanup_sctp_mibs(void)
{
	snmp_mib_free((void __percpu **)sctp_statistics);
}

static void sctp_v4_pf_init(void)
{
	
	sctp_register_pf(&sctp_pf_inet, PF_INET);
	sctp_register_af(&sctp_af_inet);
}

static void sctp_v4_pf_exit(void)
{
	list_del(&sctp_af_inet.list);
}

static int sctp_v4_protosw_init(void)
{
	int rc;

	rc = proto_register(&sctp_prot, 1);
	if (rc)
		return rc;

	
	inet_register_protosw(&sctp_seqpacket_protosw);
	inet_register_protosw(&sctp_stream_protosw);

	return 0;
}

static void sctp_v4_protosw_exit(void)
{
	inet_unregister_protosw(&sctp_stream_protosw);
	inet_unregister_protosw(&sctp_seqpacket_protosw);
	proto_unregister(&sctp_prot);
}

static int sctp_v4_add_protocol(void)
{
	
	register_inetaddr_notifier(&sctp_inetaddr_notifier);

	
	if (inet_add_protocol(&sctp_protocol, IPPROTO_SCTP) < 0)
		return -EAGAIN;

	return 0;
}

static void sctp_v4_del_protocol(void)
{
	inet_del_protocol(&sctp_protocol, IPPROTO_SCTP);
	unregister_inetaddr_notifier(&sctp_inetaddr_notifier);
}

SCTP_STATIC __init int sctp_init(void)
{
	int i;
	int status = -EINVAL;
	unsigned long goal;
	unsigned long limit;
	int max_share;
	int order;

	
	if (!sctp_sanity_check())
		goto out;

	
	status = -ENOBUFS;
	sctp_bucket_cachep = kmem_cache_create("sctp_bind_bucket",
					       sizeof(struct sctp_bind_bucket),
					       0, SLAB_HWCACHE_ALIGN,
					       NULL);
	if (!sctp_bucket_cachep)
		goto out;

	sctp_chunk_cachep = kmem_cache_create("sctp_chunk",
					       sizeof(struct sctp_chunk),
					       0, SLAB_HWCACHE_ALIGN,
					       NULL);
	if (!sctp_chunk_cachep)
		goto err_chunk_cachep;

	
	status = init_sctp_mibs();
	if (status)
		goto err_init_mibs;

	
	status = sctp_proc_init();
	if (status)
		goto err_init_proc;

	
	sctp_dbg_objcnt_init();

	
	
	sctp_rto_initial		= SCTP_RTO_INITIAL;
	
	sctp_rto_min	 		= SCTP_RTO_MIN;
	
	sctp_rto_max 			= SCTP_RTO_MAX;
	
	sctp_rto_alpha	        	= SCTP_RTO_ALPHA;
	
	sctp_rto_beta			= SCTP_RTO_BETA;

	
	sctp_valid_cookie_life		= SCTP_DEFAULT_COOKIE_LIFE;

	
	sctp_cookie_preserve_enable 	= 1;

	
	sctp_max_burst 			= SCTP_DEFAULT_MAX_BURST;

	sctp_max_retrans_association 	= 10;
	sctp_max_retrans_path		= 5;
	sctp_max_retrans_init		= 8;

	
	sctp_sndbuf_policy		= 0;

	
	sctp_rcvbuf_policy		= 0;

	
	sctp_hb_interval		= SCTP_DEFAULT_TIMEOUT_HEARTBEAT;

	
	sctp_sack_timeout		= SCTP_DEFAULT_TIMEOUT_SACK;

	

	
	sctp_max_instreams    		= SCTP_DEFAULT_INSTREAMS;
	sctp_max_outstreams   		= SCTP_DEFAULT_OUTSTREAMS;

	
	sctp_max_autoclose		= INT_MAX / HZ;

	
	idr_init(&sctp_assocs_id);

	limit = nr_free_buffer_pages() / 8;
	limit = max(limit, 128UL);
	sysctl_sctp_mem[0] = limit / 4 * 3;
	sysctl_sctp_mem[1] = limit;
	sysctl_sctp_mem[2] = sysctl_sctp_mem[0] * 2;

	
	limit = (sysctl_sctp_mem[1]) << (PAGE_SHIFT - 7);
	max_share = min(4UL*1024*1024, limit);

	sysctl_sctp_rmem[0] = SK_MEM_QUANTUM; 
	sysctl_sctp_rmem[1] = 1500 * SKB_TRUESIZE(1);
	sysctl_sctp_rmem[2] = max(sysctl_sctp_rmem[1], max_share);

	sysctl_sctp_wmem[0] = SK_MEM_QUANTUM;
	sysctl_sctp_wmem[1] = 16*1024;
	sysctl_sctp_wmem[2] = max(64*1024, max_share);

	if (totalram_pages >= (128 * 1024))
		goal = totalram_pages >> (22 - PAGE_SHIFT);
	else
		goal = totalram_pages >> (24 - PAGE_SHIFT);

	for (order = 0; (1UL << order) < goal; order++)
		;

	do {
		sctp_assoc_hashsize = (1UL << order) * PAGE_SIZE /
					sizeof(struct sctp_hashbucket);
		if ((sctp_assoc_hashsize > (64 * 1024)) && order > 0)
			continue;
		sctp_assoc_hashtable = (struct sctp_hashbucket *)
			__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, order);
	} while (!sctp_assoc_hashtable && --order > 0);
	if (!sctp_assoc_hashtable) {
		pr_err("Failed association hash alloc\n");
		status = -ENOMEM;
		goto err_ahash_alloc;
	}
	for (i = 0; i < sctp_assoc_hashsize; i++) {
		rwlock_init(&sctp_assoc_hashtable[i].lock);
		INIT_HLIST_HEAD(&sctp_assoc_hashtable[i].chain);
	}

	
	sctp_ep_hashsize = 64;
	sctp_ep_hashtable = (struct sctp_hashbucket *)
		kmalloc(64 * sizeof(struct sctp_hashbucket), GFP_KERNEL);
	if (!sctp_ep_hashtable) {
		pr_err("Failed endpoint_hash alloc\n");
		status = -ENOMEM;
		goto err_ehash_alloc;
	}
	for (i = 0; i < sctp_ep_hashsize; i++) {
		rwlock_init(&sctp_ep_hashtable[i].lock);
		INIT_HLIST_HEAD(&sctp_ep_hashtable[i].chain);
	}

	
	do {
		sctp_port_hashsize = (1UL << order) * PAGE_SIZE /
					sizeof(struct sctp_bind_hashbucket);
		if ((sctp_port_hashsize > (64 * 1024)) && order > 0)
			continue;
		sctp_port_hashtable = (struct sctp_bind_hashbucket *)
			__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, order);
	} while (!sctp_port_hashtable && --order > 0);
	if (!sctp_port_hashtable) {
		pr_err("Failed bind hash alloc\n");
		status = -ENOMEM;
		goto err_bhash_alloc;
	}
	for (i = 0; i < sctp_port_hashsize; i++) {
		spin_lock_init(&sctp_port_hashtable[i].lock);
		INIT_HLIST_HEAD(&sctp_port_hashtable[i].chain);
	}

	pr_info("Hash tables configured (established %d bind %d)\n",
		sctp_assoc_hashsize, sctp_port_hashsize);

	
	sctp_addip_enable = 0;
	sctp_addip_noauth = 0;
	sctp_default_auto_asconf = 0;

	
	sctp_prsctp_enable = 1;

	
	sctp_auth_enable = 0;

	
	sctp_scope_policy = SCTP_SCOPE_POLICY_ENABLE;

	
	sctp_rwnd_upd_shift		= SCTP_DEFAULT_RWND_SHIFT;

	sctp_sysctl_register();

	INIT_LIST_HEAD(&sctp_address_families);
	sctp_v4_pf_init();
	sctp_v6_pf_init();

	
	INIT_LIST_HEAD(&sctp_local_addr_list);
	spin_lock_init(&sctp_local_addr_lock);
	sctp_get_local_addr_list();

	
	INIT_LIST_HEAD(&sctp_addr_waitq);
	INIT_LIST_HEAD(&sctp_auto_asconf_splist);
	spin_lock_init(&sctp_addr_wq_lock);
	sctp_addr_wq_timer.expires = 0;
	setup_timer(&sctp_addr_wq_timer, sctp_addr_wq_timeout_handler, 0);

	status = sctp_v4_protosw_init();

	if (status)
		goto err_protosw_init;

	status = sctp_v6_protosw_init();
	if (status)
		goto err_v6_protosw_init;

	
	if ((status = sctp_ctl_sock_init())) {
		pr_err("Failed to initialize the SCTP control sock\n");
		goto err_ctl_sock_init;
	}

	status = sctp_v4_add_protocol();
	if (status)
		goto err_add_protocol;

	
	status = sctp_v6_add_protocol();
	if (status)
		goto err_v6_add_protocol;

	status = 0;
out:
	return status;
err_v6_add_protocol:
	sctp_v4_del_protocol();
err_add_protocol:
	inet_ctl_sock_destroy(sctp_ctl_sock);
err_ctl_sock_init:
	sctp_v6_protosw_exit();
err_v6_protosw_init:
	sctp_v4_protosw_exit();
err_protosw_init:
	sctp_free_local_addr_list();
	sctp_v4_pf_exit();
	sctp_v6_pf_exit();
	sctp_sysctl_unregister();
	free_pages((unsigned long)sctp_port_hashtable,
		   get_order(sctp_port_hashsize *
			     sizeof(struct sctp_bind_hashbucket)));
err_bhash_alloc:
	kfree(sctp_ep_hashtable);
err_ehash_alloc:
	free_pages((unsigned long)sctp_assoc_hashtable,
		   get_order(sctp_assoc_hashsize *
			     sizeof(struct sctp_hashbucket)));
err_ahash_alloc:
	sctp_dbg_objcnt_exit();
	sctp_proc_exit();
err_init_proc:
	cleanup_sctp_mibs();
err_init_mibs:
	kmem_cache_destroy(sctp_chunk_cachep);
err_chunk_cachep:
	kmem_cache_destroy(sctp_bucket_cachep);
	goto out;
}

SCTP_STATIC __exit void sctp_exit(void)
{

	
	sctp_v6_del_protocol();
	sctp_v4_del_protocol();
	sctp_free_addr_wq();

	
	inet_ctl_sock_destroy(sctp_ctl_sock);

	
	sctp_v6_protosw_exit();
	sctp_v4_protosw_exit();

	
	sctp_free_local_addr_list();

	
	sctp_v6_pf_exit();
	sctp_v4_pf_exit();

	sctp_sysctl_unregister();

	free_pages((unsigned long)sctp_assoc_hashtable,
		   get_order(sctp_assoc_hashsize *
			     sizeof(struct sctp_hashbucket)));
	kfree(sctp_ep_hashtable);
	free_pages((unsigned long)sctp_port_hashtable,
		   get_order(sctp_port_hashsize *
			     sizeof(struct sctp_bind_hashbucket)));

	sctp_dbg_objcnt_exit();
	sctp_proc_exit();
	cleanup_sctp_mibs();

	rcu_barrier(); 

	kmem_cache_destroy(sctp_chunk_cachep);
	kmem_cache_destroy(sctp_bucket_cachep);
}

module_init(sctp_init);
module_exit(sctp_exit);

MODULE_ALIAS("net-pf-" __stringify(PF_INET) "-proto-132");
MODULE_ALIAS("net-pf-" __stringify(PF_INET6) "-proto-132");
MODULE_AUTHOR("Linux Kernel SCTP developers <lksctp-developers@lists.sourceforge.net>");
MODULE_DESCRIPTION("Support for the SCTP protocol (RFC2960)");
module_param_named(no_checksums, sctp_checksum_disable, bool, 0644);
MODULE_PARM_DESC(no_checksums, "Disable checksums computing and verification");
MODULE_LICENSE("GPL");
