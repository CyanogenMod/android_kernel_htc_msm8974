/*
 * Simulated Ethernet Driver
 *
 * Copyright (C) 1999-2001, 2003 Hewlett-Packard Co
 *	Stephane Eranian <eranian@hpl.hp.com>
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/hpsim.h>

#include "hpsim_ssc.h"

#define SIMETH_RECV_MAX	10

#define SIMETH_FRAME_SIZE	ETH_FRAME_LEN


#define NETWORK_INTR			8

struct simeth_local {
	struct net_device_stats stats;
	int 			simfd;	 
};

static int simeth_probe1(void);
static int simeth_open(struct net_device *dev);
static int simeth_close(struct net_device *dev);
static int simeth_tx(struct sk_buff *skb, struct net_device *dev);
static int simeth_rx(struct net_device *dev);
static struct net_device_stats *simeth_get_stats(struct net_device *dev);
static irqreturn_t simeth_interrupt(int irq, void *dev_id);
static void set_multicast_list(struct net_device *dev);
static int simeth_device_event(struct notifier_block *this,unsigned long event, void *ptr);

static char *simeth_version="0.3";

static char *simeth_device="eth0";	 



static volatile unsigned int card_count; 
static int simeth_debug;		

static struct notifier_block simeth_dev_notifier = {
	simeth_device_event,
	NULL
};


static int __init
simeth_setup(char *str)
{
	simeth_device = str;
	return 1;
}

__setup("simeth=", simeth_setup);


int __init
simeth_probe (void)
{
	int r;

	printk(KERN_INFO "simeth: v%s\n", simeth_version);

	r = simeth_probe1();

	if (r == 0) register_netdevice_notifier(&simeth_dev_notifier);

	return r;
}

static inline int
netdev_probe(char *name, unsigned char *ether)
{
	return ia64_ssc(__pa(name), __pa(ether), 0,0, SSC_NETDEV_PROBE);
}


static inline int
netdev_attach(int fd, int irq, unsigned int ipaddr)
{
	
	return ia64_ssc(fd, ipaddr, 0,0, SSC_NETDEV_ATTACH);
}


static inline int
netdev_detach(int fd)
{
	return ia64_ssc(fd, 0,0,0, SSC_NETDEV_DETACH);
}

static inline int
netdev_send(int fd, unsigned char *buf, unsigned int len)
{
	return ia64_ssc(fd, __pa(buf), len, 0, SSC_NETDEV_SEND);
}

static inline int
netdev_read(int fd, unsigned char *buf, unsigned int len)
{
	return ia64_ssc(fd, __pa(buf), len, 0, SSC_NETDEV_RECV);
}

static const struct net_device_ops simeth_netdev_ops = {
	.ndo_open		= simeth_open,
	.ndo_stop		= simeth_close,
	.ndo_start_xmit		= simeth_tx,
	.ndo_get_stats		= simeth_get_stats,
	.ndo_set_rx_mode	= set_multicast_list, 

};

static int
simeth_probe1(void)
{
	unsigned char mac_addr[ETH_ALEN];
	struct simeth_local *local;
	struct net_device *dev;
	int fd, err, rc;

	if (test_and_set_bit(0, &card_count))
		return -ENODEV;

	fd = netdev_probe(simeth_device, mac_addr);
	if (fd == -1)
		return -ENODEV;

	dev = alloc_etherdev(sizeof(struct simeth_local));
	if (!dev)
		return -ENOMEM;

	memcpy(dev->dev_addr, mac_addr, sizeof(mac_addr));

	local = netdev_priv(dev);
	local->simfd = fd; 

	dev->netdev_ops = &simeth_netdev_ops;

	err = register_netdev(dev);
	if (err) {
		free_netdev(dev);
		return err;
	}

	if ((rc = hpsim_get_irq(NETWORK_INTR)) < 0)
		panic("%s: out of interrupt vectors!\n", __func__);
	dev->irq = rc;

	printk(KERN_INFO "%s: hosteth=%s simfd=%d, HwAddr=%pm, IRQ %d\n",
	       dev->name, simeth_device, local->simfd, dev->dev_addr, dev->irq);

	return 0;
}

static int
simeth_open(struct net_device *dev)
{
	if (request_irq(dev->irq, simeth_interrupt, 0, "simeth", dev)) {
		printk(KERN_WARNING "simeth: unable to get IRQ %d.\n", dev->irq);
		return -EAGAIN;
	}

	netif_start_queue(dev);

	return 0;
}

static __inline__ int dev_is_ethdev(struct net_device *dev)
{
       return ( dev->type == ARPHRD_ETHER && strncmp(dev->name, "dummy", 5));
}


static int
simeth_device_event(struct notifier_block *this,unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct simeth_local *local;
	struct in_device *in_dev;
	struct in_ifaddr **ifap = NULL;
	struct in_ifaddr *ifa = NULL;
	int r;


	if ( ! dev ) {
		printk(KERN_WARNING "simeth_device_event dev=0\n");
		return NOTIFY_DONE;
	}

	if (dev_net(dev) != &init_net)
		return NOTIFY_DONE;

	if ( event != NETDEV_UP && event != NETDEV_DOWN ) return NOTIFY_DONE;

	if ( !dev_is_ethdev(dev) ) return NOTIFY_DONE;

	if ((in_dev=dev->ip_ptr) != NULL) {
		for (ifap=&in_dev->ifa_list; (ifa=*ifap) != NULL; ifap=&ifa->ifa_next)
			if (strcmp(dev->name, ifa->ifa_label) == 0) break;
	}
	if ( ifa == NULL ) {
		printk(KERN_ERR "simeth_open: can't find device %s's ifa\n", dev->name);
		return NOTIFY_DONE;
	}

	printk(KERN_INFO "simeth_device_event: %s ipaddr=0x%x\n",
	       dev->name, ntohl(ifa->ifa_local));


	local = netdev_priv(dev);
	
	r = event == NETDEV_UP ?
		netdev_attach(local->simfd, dev->irq, ntohl(ifa->ifa_local)):
		netdev_detach(local->simfd);

	printk(KERN_INFO "simeth: netdev_attach/detach: event=%s ->%d\n",
	       event == NETDEV_UP ? "attach":"detach", r);

	return NOTIFY_DONE;
}

static int
simeth_close(struct net_device *dev)
{
	netif_stop_queue(dev);

	free_irq(dev->irq, dev);

	return 0;
}

static void
frame_print(unsigned char *from, unsigned char *frame, int len)
{
	int i;

	printk("%s: (%d) %02x", from, len, frame[0] & 0xff);
	for(i=1; i < 6; i++ ) {
		printk(":%02x", frame[i] &0xff);
	}
	printk(" %2x", frame[6] &0xff);
	for(i=7; i < 12; i++ ) {
		printk(":%02x", frame[i] &0xff);
	}
	printk(" [%02x%02x]\n", frame[12], frame[13]);

	for(i=14; i < len; i++ ) {
		printk("%02x ", frame[i] &0xff);
		if ( (i%10)==0) printk("\n");
	}
	printk("\n");
}


static int
simeth_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct simeth_local *local = netdev_priv(dev);

#if 0
	
	unsigned int length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	
#else
	unsigned int length = skb->len;
#endif

	local->stats.tx_bytes += skb->len;
	local->stats.tx_packets++;


	if (simeth_debug > 5) frame_print("simeth_tx", skb->data, length);

	netdev_send(local->simfd, skb->data, length);


	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static inline struct sk_buff *
make_new_skb(struct net_device *dev)
{
	struct sk_buff *nskb;

	nskb = dev_alloc_skb(SIMETH_FRAME_SIZE + 2);
	if ( nskb == NULL ) {
		printk(KERN_NOTICE "%s: memory squeeze. dropping packet.\n", dev->name);
		return NULL;
	}

	skb_reserve(nskb, 2);	

	skb_put(nskb,SIMETH_FRAME_SIZE);

	return nskb;
}

static int
simeth_rx(struct net_device *dev)
{
	struct simeth_local	*local;
	struct sk_buff		*skb;
	int			len;
	int			rcv_count = SIMETH_RECV_MAX;

	local = netdev_priv(dev);
	do {
		if ( (skb=make_new_skb(dev)) == NULL ) {
			printk(KERN_NOTICE "%s: memory squeeze. dropping packet.\n", dev->name);
			local->stats.rx_dropped++;
			return 0;
		}
		len = netdev_read(local->simfd, skb->data, SIMETH_FRAME_SIZE);
		if ( len == 0 ) {
			if ( simeth_debug > 0 ) printk(KERN_WARNING "%s: count=%d netdev_read=0\n",
						       dev->name, SIMETH_RECV_MAX-rcv_count);
			break;
		}
#if 0
		skb_copy_to_linear_data(skb, frame, len);
#endif
		skb->protocol = eth_type_trans(skb, dev);

		if ( simeth_debug > 6 ) frame_print("simeth_rx", skb->data, len);

		netif_rx(skb);

		local->stats.rx_packets++;
		local->stats.rx_bytes += len;

	} while ( --rcv_count );

	return len; 
}

static irqreturn_t
simeth_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;

	while (simeth_rx(dev));
	return IRQ_HANDLED;
}

static struct net_device_stats *
simeth_get_stats(struct net_device *dev)
{
	struct simeth_local *local = netdev_priv(dev);

	return &local->stats;
}

static void
set_multicast_list(struct net_device *dev)
{
	printk(KERN_WARNING "%s: set_multicast_list called\n", dev->name);
}

__initcall(simeth_probe);
