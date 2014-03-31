/*
 * af_can.c - Protocol family CAN core module
 *            (used by different CAN protocol modules)
 *
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/uaccess.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/can.h>
#include <linux/can/core.h>
#include <linux/ratelimit.h>
#include <net/net_namespace.h>
#include <net/sock.h>

#include "af_can.h"

static __initdata const char banner[] = KERN_INFO
	"can: controller area network core (" CAN_VERSION_STRING ")\n";

MODULE_DESCRIPTION("Controller Area Network PF_CAN core");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Urs Thuermann <urs.thuermann@volkswagen.de>, "
	      "Oliver Hartkopp <oliver.hartkopp@volkswagen.de>");

MODULE_ALIAS_NETPROTO(PF_CAN);

static int stats_timer __read_mostly = 1;
module_param(stats_timer, int, S_IRUGO);
MODULE_PARM_DESC(stats_timer, "enable timer for statistics (default:on)");

struct dev_rcv_lists can_rx_alldev_list;
static DEFINE_SPINLOCK(can_rcvlists_lock);

static struct kmem_cache *rcv_cache __read_mostly;

static const struct can_proto *proto_tab[CAN_NPROTO] __read_mostly;
static DEFINE_MUTEX(proto_tab_lock);

struct timer_list can_stattimer;   
struct s_stats    can_stats;       
struct s_pstats   can_pstats;      


int can_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {

	case SIOCGSTAMP:
		return sock_get_timestamp(sk, (struct timeval __user *)arg);

	default:
		return -ENOIOCTLCMD;
	}
}
EXPORT_SYMBOL(can_ioctl);

static void can_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->sk_receive_queue);
}

static const struct can_proto *can_get_proto(int protocol)
{
	const struct can_proto *cp;

	rcu_read_lock();
	cp = rcu_dereference(proto_tab[protocol]);
	if (cp && !try_module_get(cp->prot->owner))
		cp = NULL;
	rcu_read_unlock();

	return cp;
}

static inline void can_put_proto(const struct can_proto *cp)
{
	module_put(cp->prot->owner);
}

static int can_create(struct net *net, struct socket *sock, int protocol,
		      int kern)
{
	struct sock *sk;
	const struct can_proto *cp;
	int err = 0;

	sock->state = SS_UNCONNECTED;

	if (protocol < 0 || protocol >= CAN_NPROTO)
		return -EINVAL;

	if (!net_eq(net, &init_net))
		return -EAFNOSUPPORT;

	cp = can_get_proto(protocol);

#ifdef CONFIG_MODULES
	if (!cp) {
		

		err = request_module("can-proto-%d", protocol);

		if (err)
			printk_ratelimited(KERN_ERR "can: request_module "
			       "(can-proto-%d) failed.\n", protocol);

		cp = can_get_proto(protocol);
	}
#endif

	

	if (!cp)
		return -EPROTONOSUPPORT;

	if (cp->type != sock->type) {
		err = -EPROTOTYPE;
		goto errout;
	}

	sock->ops = cp->ops;

	sk = sk_alloc(net, PF_CAN, GFP_KERNEL, cp->prot);
	if (!sk) {
		err = -ENOMEM;
		goto errout;
	}

	sock_init_data(sock, sk);
	sk->sk_destruct = can_sock_destruct;

	if (sk->sk_prot->init)
		err = sk->sk_prot->init(sk);

	if (err) {
		
		sock_orphan(sk);
		sock_put(sk);
	}

 errout:
	can_put_proto(cp);
	return err;
}


int can_send(struct sk_buff *skb, int loop)
{
	struct sk_buff *newskb = NULL;
	struct can_frame *cf = (struct can_frame *)skb->data;
	int err;

	if (skb->len != sizeof(struct can_frame) || cf->can_dlc > 8) {
		kfree_skb(skb);
		return -EINVAL;
	}

	if (skb->dev->type != ARPHRD_CAN) {
		kfree_skb(skb);
		return -EPERM;
	}

	if (!(skb->dev->flags & IFF_UP)) {
		kfree_skb(skb);
		return -ENETDOWN;
	}

	skb->protocol = htons(ETH_P_CAN);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);

	if (loop) {
		

		
		skb->pkt_type = PACKET_LOOPBACK;


		if (!(skb->dev->flags & IFF_ECHO)) {
			newskb = skb_clone(skb, GFP_ATOMIC);
			if (!newskb) {
				kfree_skb(skb);
				return -ENOMEM;
			}

			newskb->sk = skb->sk;
			newskb->ip_summed = CHECKSUM_UNNECESSARY;
			newskb->pkt_type = PACKET_BROADCAST;
		}
	} else {
		
		skb->pkt_type = PACKET_HOST;
	}

	
	err = dev_queue_xmit(skb);
	if (err > 0)
		err = net_xmit_errno(err);

	if (err) {
		kfree_skb(newskb);
		return err;
	}

	if (newskb)
		netif_rx_ni(newskb);

	
	can_stats.tx_frames++;
	can_stats.tx_frames_delta++;

	return 0;
}
EXPORT_SYMBOL(can_send);


static struct dev_rcv_lists *find_dev_rcv_lists(struct net_device *dev)
{
	if (!dev)
		return &can_rx_alldev_list;
	else
		return (struct dev_rcv_lists *)dev->ml_priv;
}

static struct hlist_head *find_rcv_list(canid_t *can_id, canid_t *mask,
					struct dev_rcv_lists *d)
{
	canid_t inv = *can_id & CAN_INV_FILTER; 

	
	if (*mask & CAN_ERR_FLAG) {
		
		*mask &= CAN_ERR_MASK;
		return &d->rx[RX_ERR];
	}

	

#define CAN_EFF_RTR_FLAGS (CAN_EFF_FLAG | CAN_RTR_FLAG)

	
	if ((*mask & CAN_EFF_FLAG) && !(*can_id & CAN_EFF_FLAG))
		*mask &= (CAN_SFF_MASK | CAN_EFF_RTR_FLAGS);

	
	*can_id &= *mask;

	
	if (inv)
		return &d->rx[RX_INV];

	
	if (!(*mask))
		return &d->rx[RX_ALL];

	
	if (((*mask & CAN_EFF_RTR_FLAGS) == CAN_EFF_RTR_FLAGS) &&
	    !(*can_id & CAN_RTR_FLAG)) {

		if (*can_id & CAN_EFF_FLAG) {
			if (*mask == (CAN_EFF_MASK | CAN_EFF_RTR_FLAGS)) {
				
				return &d->rx[RX_EFF];
			}
		} else {
			if (*mask == (CAN_SFF_MASK | CAN_EFF_RTR_FLAGS))
				return &d->rx_sff[*can_id];
		}
	}

	
	return &d->rx[RX_FIL];
}

int can_rx_register(struct net_device *dev, canid_t can_id, canid_t mask,
		    void (*func)(struct sk_buff *, void *), void *data,
		    char *ident)
{
	struct receiver *r;
	struct hlist_head *rl;
	struct dev_rcv_lists *d;
	int err = 0;

	

	if (dev && dev->type != ARPHRD_CAN)
		return -ENODEV;

	r = kmem_cache_alloc(rcv_cache, GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	spin_lock(&can_rcvlists_lock);

	d = find_dev_rcv_lists(dev);
	if (d) {
		rl = find_rcv_list(&can_id, &mask, d);

		r->can_id  = can_id;
		r->mask    = mask;
		r->matches = 0;
		r->func    = func;
		r->data    = data;
		r->ident   = ident;

		hlist_add_head_rcu(&r->list, rl);
		d->entries++;

		can_pstats.rcv_entries++;
		if (can_pstats.rcv_entries_max < can_pstats.rcv_entries)
			can_pstats.rcv_entries_max = can_pstats.rcv_entries;
	} else {
		kmem_cache_free(rcv_cache, r);
		err = -ENODEV;
	}

	spin_unlock(&can_rcvlists_lock);

	return err;
}
EXPORT_SYMBOL(can_rx_register);

static void can_rx_delete_receiver(struct rcu_head *rp)
{
	struct receiver *r = container_of(rp, struct receiver, rcu);

	kmem_cache_free(rcv_cache, r);
}

void can_rx_unregister(struct net_device *dev, canid_t can_id, canid_t mask,
		       void (*func)(struct sk_buff *, void *), void *data)
{
	struct receiver *r = NULL;
	struct hlist_head *rl;
	struct hlist_node *next;
	struct dev_rcv_lists *d;

	if (dev && dev->type != ARPHRD_CAN)
		return;

	spin_lock(&can_rcvlists_lock);

	d = find_dev_rcv_lists(dev);
	if (!d) {
		printk(KERN_ERR "BUG: receive list not found for "
		       "dev %s, id %03X, mask %03X\n",
		       DNAME(dev), can_id, mask);
		goto out;
	}

	rl = find_rcv_list(&can_id, &mask, d);


	hlist_for_each_entry_rcu(r, next, rl, list) {
		if (r->can_id == can_id && r->mask == mask &&
		    r->func == func && r->data == data)
			break;
	}


	if (!next) {
		printk(KERN_ERR "BUG: receive list entry not found for "
		       "dev %s, id %03X, mask %03X\n",
		       DNAME(dev), can_id, mask);
		r = NULL;
		goto out;
	}

	hlist_del_rcu(&r->list);
	d->entries--;

	if (can_pstats.rcv_entries > 0)
		can_pstats.rcv_entries--;

	
	if (d->remove_on_zero_entries && !d->entries) {
		kfree(d);
		dev->ml_priv = NULL;
	}

 out:
	spin_unlock(&can_rcvlists_lock);

	
	if (r)
		call_rcu(&r->rcu, can_rx_delete_receiver);
}
EXPORT_SYMBOL(can_rx_unregister);

static inline void deliver(struct sk_buff *skb, struct receiver *r)
{
	r->func(skb, r->data);
	r->matches++;
}

static int can_rcv_filter(struct dev_rcv_lists *d, struct sk_buff *skb)
{
	struct receiver *r;
	struct hlist_node *n;
	int matches = 0;
	struct can_frame *cf = (struct can_frame *)skb->data;
	canid_t can_id = cf->can_id;

	if (d->entries == 0)
		return 0;

	if (can_id & CAN_ERR_FLAG) {
		
		hlist_for_each_entry_rcu(r, n, &d->rx[RX_ERR], list) {
			if (can_id & r->mask) {
				deliver(skb, r);
				matches++;
			}
		}
		return matches;
	}

	
	hlist_for_each_entry_rcu(r, n, &d->rx[RX_ALL], list) {
		deliver(skb, r);
		matches++;
	}

	
	hlist_for_each_entry_rcu(r, n, &d->rx[RX_FIL], list) {
		if ((can_id & r->mask) == r->can_id) {
			deliver(skb, r);
			matches++;
		}
	}

	
	hlist_for_each_entry_rcu(r, n, &d->rx[RX_INV], list) {
		if ((can_id & r->mask) != r->can_id) {
			deliver(skb, r);
			matches++;
		}
	}

	
	if (can_id & CAN_RTR_FLAG)
		return matches;

	if (can_id & CAN_EFF_FLAG) {
		hlist_for_each_entry_rcu(r, n, &d->rx[RX_EFF], list) {
			if (r->can_id == can_id) {
				deliver(skb, r);
				matches++;
			}
		}
	} else {
		can_id &= CAN_SFF_MASK;
		hlist_for_each_entry_rcu(r, n, &d->rx_sff[can_id], list) {
			deliver(skb, r);
			matches++;
		}
	}

	return matches;
}

static int can_rcv(struct sk_buff *skb, struct net_device *dev,
		   struct packet_type *pt, struct net_device *orig_dev)
{
	struct dev_rcv_lists *d;
	struct can_frame *cf = (struct can_frame *)skb->data;
	int matches;

	if (!net_eq(dev_net(dev), &init_net))
		goto drop;

	if (WARN_ONCE(dev->type != ARPHRD_CAN ||
		      skb->len != sizeof(struct can_frame) ||
		      cf->can_dlc > 8,
		      "PF_CAN: dropped non conform skbuf: "
		      "dev type %d, len %d, can_dlc %d\n",
		      dev->type, skb->len, cf->can_dlc))
		goto drop;

	
	can_stats.rx_frames++;
	can_stats.rx_frames_delta++;

	rcu_read_lock();

	
	matches = can_rcv_filter(&can_rx_alldev_list, skb);

	
	d = find_dev_rcv_lists(dev);
	if (d)
		matches += can_rcv_filter(d, skb);

	rcu_read_unlock();

	
	consume_skb(skb);

	if (matches > 0) {
		can_stats.matches++;
		can_stats.matches_delta++;
	}

	return NET_RX_SUCCESS;

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}


int can_proto_register(const struct can_proto *cp)
{
	int proto = cp->protocol;
	int err = 0;

	if (proto < 0 || proto >= CAN_NPROTO) {
		printk(KERN_ERR "can: protocol number %d out of range\n",
		       proto);
		return -EINVAL;
	}

	err = proto_register(cp->prot, 0);
	if (err < 0)
		return err;

	mutex_lock(&proto_tab_lock);

	if (proto_tab[proto]) {
		printk(KERN_ERR "can: protocol %d already registered\n",
		       proto);
		err = -EBUSY;
	} else
		RCU_INIT_POINTER(proto_tab[proto], cp);

	mutex_unlock(&proto_tab_lock);

	if (err < 0)
		proto_unregister(cp->prot);

	return err;
}
EXPORT_SYMBOL(can_proto_register);

void can_proto_unregister(const struct can_proto *cp)
{
	int proto = cp->protocol;

	mutex_lock(&proto_tab_lock);
	BUG_ON(proto_tab[proto] != cp);
	RCU_INIT_POINTER(proto_tab[proto], NULL);
	mutex_unlock(&proto_tab_lock);

	synchronize_rcu();

	proto_unregister(cp->prot);
}
EXPORT_SYMBOL(can_proto_unregister);

static int can_notifier(struct notifier_block *nb, unsigned long msg,
			void *data)
{
	struct net_device *dev = (struct net_device *)data;
	struct dev_rcv_lists *d;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	if (dev->type != ARPHRD_CAN)
		return NOTIFY_DONE;

	switch (msg) {

	case NETDEV_REGISTER:

		
		d = kzalloc(sizeof(*d), GFP_KERNEL);
		if (!d) {
			printk(KERN_ERR
			       "can: allocation of receive list failed\n");
			return NOTIFY_DONE;
		}
		BUG_ON(dev->ml_priv);
		dev->ml_priv = d;

		break;

	case NETDEV_UNREGISTER:
		spin_lock(&can_rcvlists_lock);

		d = dev->ml_priv;
		if (d) {
			if (d->entries)
				d->remove_on_zero_entries = 1;
			else {
				kfree(d);
				dev->ml_priv = NULL;
			}
		} else
			printk(KERN_ERR "can: notifier: receive list not "
			       "found for dev %s\n", dev->name);

		spin_unlock(&can_rcvlists_lock);

		break;
	}

	return NOTIFY_DONE;
}


static struct packet_type can_packet __read_mostly = {
	.type = cpu_to_be16(ETH_P_CAN),
	.dev  = NULL,
	.func = can_rcv,
};

static const struct net_proto_family can_family_ops = {
	.family = PF_CAN,
	.create = can_create,
	.owner  = THIS_MODULE,
};

static struct notifier_block can_netdev_notifier __read_mostly = {
	.notifier_call = can_notifier,
};

static __init int can_init(void)
{
	printk(banner);

	memset(&can_rx_alldev_list, 0, sizeof(can_rx_alldev_list));

	rcv_cache = kmem_cache_create("can_receiver", sizeof(struct receiver),
				      0, 0, NULL);
	if (!rcv_cache)
		return -ENOMEM;

	if (stats_timer) {
		
		setup_timer(&can_stattimer, can_stat_update, 0);
		mod_timer(&can_stattimer, round_jiffies(jiffies + HZ));
	} else
		can_stattimer.function = NULL;

	can_init_proc();

	
	sock_register(&can_family_ops);
	register_netdevice_notifier(&can_netdev_notifier);
	dev_add_pack(&can_packet);

	return 0;
}

static __exit void can_exit(void)
{
	struct net_device *dev;

	if (stats_timer)
		del_timer_sync(&can_stattimer);

	can_remove_proc();

	
	dev_remove_pack(&can_packet);
	unregister_netdevice_notifier(&can_netdev_notifier);
	sock_unregister(PF_CAN);

	
	rcu_read_lock();
	for_each_netdev_rcu(&init_net, dev) {
		if (dev->type == ARPHRD_CAN && dev->ml_priv){

			struct dev_rcv_lists *d = dev->ml_priv;

			BUG_ON(d->entries);
			kfree(d);
			dev->ml_priv = NULL;
		}
	}
	rcu_read_unlock();

	rcu_barrier(); 

	kmem_cache_destroy(rcv_cache);
}

module_init(can_init);
module_exit(can_exit);
