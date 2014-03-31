/*
 * Generic HDLC support routines for Linux
 *
 * Copyright (C) 1999-2005 Krzysztof Halasa <khc@pm.waw.pl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#ifndef __HDLC_H
#define __HDLC_H


#define HDLC_MAX_MTU 1500	
#if 0
#define HDLC_MAX_MRU (HDLC_MAX_MTU + 10 + 14 + 4) 
#else
#define HDLC_MAX_MRU 1600 
#endif


#ifdef __KERNEL__

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/hdlc/ioctl.h>


struct hdlc_proto {
	int (*open)(struct net_device *dev);
	void (*close)(struct net_device *dev);
	void (*start)(struct net_device *dev); 
	void (*stop)(struct net_device *dev); 
	void (*detach)(struct net_device *dev);
	int (*ioctl)(struct net_device *dev, struct ifreq *ifr);
	__be16 (*type_trans)(struct sk_buff *skb, struct net_device *dev);
	int (*netif_rx)(struct sk_buff *skb);
	netdev_tx_t (*xmit)(struct sk_buff *skb, struct net_device *dev);
	struct module *module;
	struct hdlc_proto *next; 
};


typedef struct hdlc_device {
	
	int (*attach)(struct net_device *dev,
		      unsigned short encoding, unsigned short parity);

	
	netdev_tx_t (*xmit)(struct sk_buff *skb, struct net_device *dev);

	
	const struct hdlc_proto *proto;
	int carrier;
	int open;
	spinlock_t state_lock;
	void *state;
	void *priv;
} hdlc_device;




int hdlc_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

#define register_hdlc_device(dev)	register_netdev(dev)
void unregister_hdlc_device(struct net_device *dev);


void register_hdlc_protocol(struct hdlc_proto *proto);
void unregister_hdlc_protocol(struct hdlc_proto *proto);

struct net_device *alloc_hdlcdev(void *priv);

static inline struct hdlc_device* dev_to_hdlc(struct net_device *dev)
{
	return netdev_priv(dev);
}

static __inline__ void debug_frame(const struct sk_buff *skb)
{
	int i;

	for (i=0; i < skb->len; i++) {
		if (i == 100) {
			printk("...\n");
			return;
		}
		printk(" %02X", skb->data[i]);
	}
	printk("\n");
}


int hdlc_open(struct net_device *dev);
void hdlc_close(struct net_device *dev);
int hdlc_change_mtu(struct net_device *dev, int new_mtu);
netdev_tx_t hdlc_start_xmit(struct sk_buff *skb, struct net_device *dev);

int attach_hdlc_protocol(struct net_device *dev, struct hdlc_proto *proto,
			 size_t size);
void detach_hdlc_protocol(struct net_device *dev);

static __inline__ __be16 hdlc_type_trans(struct sk_buff *skb,
					 struct net_device *dev)
{
	hdlc_device *hdlc = dev_to_hdlc(dev);

	skb->dev = dev;
	skb_reset_mac_header(skb);

	if (hdlc->proto->type_trans)
		return hdlc->proto->type_trans(skb, dev);
	else
		return htons(ETH_P_HDLC);
}

#endif 
#endif 
