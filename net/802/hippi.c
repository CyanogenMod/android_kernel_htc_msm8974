/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		HIPPI-type device handling.
 *
 * Version:	@(#)hippi.c	1.0.0	05/29/97
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Florian  La Roche, <rzsfl@rz.uni-sb.de>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Jes Sorensen, <Jes.Sorensen@cern.ch>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/hippidevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <net/arp.h>
#include <net/sock.h>
#include <asm/uaccess.h>


static int hippi_header(struct sk_buff *skb, struct net_device *dev,
			unsigned short type,
			const void *daddr, const void *saddr, unsigned len)
{
	struct hippi_hdr *hip = (struct hippi_hdr *)skb_push(skb, HIPPI_HLEN);
	struct hippi_cb *hcb = (struct hippi_cb *) skb->cb;

	if (!len){
		len = skb->len - HIPPI_HLEN;
		printk("hippi_header(): length not supplied\n");
	}

	hip->fp.fixed		= htonl(0x04800018);
	hip->fp.d2_size		= htonl(len + 8);
	hip->le.fc		= 0;
	hip->le.double_wide	= 0;	
	hip->le.message_type	= 0;	

	hip->le.dest_addr_type	= 2;	
	hip->le.src_addr_type	= 2;	

	memcpy(hip->le.src_switch_addr, dev->dev_addr + 3, 3);
	memset(&hip->le.reserved, 0, 16);

	hip->snap.dsap		= HIPPI_EXTENDED_SAP;
	hip->snap.ssap		= HIPPI_EXTENDED_SAP;
	hip->snap.ctrl		= HIPPI_UI_CMD;
	hip->snap.oui[0]	= 0x00;
	hip->snap.oui[1]	= 0x00;
	hip->snap.oui[2]	= 0x00;
	hip->snap.ethertype	= htons(type);

	if (daddr)
	{
		memcpy(hip->le.dest_switch_addr, daddr + 3, 3);
		memcpy(&hcb->ifield, daddr + 2, 4);
		return HIPPI_HLEN;
	}
	hcb->ifield = 0;
	return -((int)HIPPI_HLEN);
}



static int hippi_rebuild_header(struct sk_buff *skb)
{
	struct hippi_hdr *hip = (struct hippi_hdr *)skb->data;


	if(hip->snap.ethertype != htons(ETH_P_IP))
	{
		printk(KERN_DEBUG "%s: unable to resolve type %X addresses.\n",skb->dev->name,ntohs(hip->snap.ethertype));
		return 0;
	}

	return arp_find(hip->le.daddr, skb);
}



__be16 hippi_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct hippi_hdr *hip;

	skb->dev = dev;
	skb_reset_mac_header(skb);
	hip = (struct hippi_hdr *)skb_mac_header(skb);
	skb_pull(skb, HIPPI_HLEN);


	return hip->snap.ethertype;
}

EXPORT_SYMBOL(hippi_type_trans);

int hippi_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 65280))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}
EXPORT_SYMBOL(hippi_change_mtu);

int hippi_mac_addr(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	if (netif_running(dev))
		return -EBUSY;
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}
EXPORT_SYMBOL(hippi_mac_addr);

int hippi_neigh_setup_dev(struct net_device *dev, struct neigh_parms *p)
{
	
	p->mcast_probes = 0;

	if (p->tbl->family != AF_INET6)
		p->ucast_probes = 0;
	return 0;
}
EXPORT_SYMBOL(hippi_neigh_setup_dev);

static const struct header_ops hippi_header_ops = {
	.create		= hippi_header,
	.rebuild	= hippi_rebuild_header,
};


static void hippi_setup(struct net_device *dev)
{
	dev->header_ops			= &hippi_header_ops;

	dev->type		= ARPHRD_HIPPI;
	dev->hard_header_len 	= HIPPI_HLEN;
	dev->mtu		= 65280;
	dev->addr_len		= HIPPI_ALEN;
	dev->tx_queue_len	= 25 ;
	memset(dev->broadcast, 0xFF, HIPPI_ALEN);


	dev->flags = 0;
}


struct net_device *alloc_hippi_dev(int sizeof_priv)
{
	return alloc_netdev(sizeof_priv, "hip%d", hippi_setup);
}

EXPORT_SYMBOL(alloc_hippi_dev);
