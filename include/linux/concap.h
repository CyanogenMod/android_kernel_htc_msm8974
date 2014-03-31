/* $Id: concap.h,v 1.3.2.2 2004/01/12 23:08:35 keil Exp $
 *
 * Copyright 1997 by Henner Eisen <eis@baty.hanse.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#ifndef _LINUX_CONCAP_H
#define _LINUX_CONCAP_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>


struct concap_proto_ops;
struct concap_device_ops;

struct concap_proto{
	struct net_device *net_dev;	
	struct concap_device_ops *dops;	
 	struct concap_proto_ops  *pops;	
 	spinlock_t lock;
	int flags;
	void *proto_data;		
};

struct concap_device_ops{

	 
	int (*data_req)(struct concap_proto *, struct sk_buff *);

	 
	int (*connect_req)(struct concap_proto *);

	
	int (*disconn_req)(struct concap_proto *);	
};

struct concap_proto_ops{

	
	struct concap_proto *  (*proto_new) (void);

	void (*proto_del)(struct concap_proto *cprot);

	int (*restart)(struct concap_proto *cprot, 
		       struct net_device *ndev,
		       struct concap_device_ops *dops);

	int (*close)(struct concap_proto *cprot);

	
	int (*encap_and_xmit)(struct concap_proto *cprot, struct sk_buff *skb);

	 
	int (*data_ind)(struct concap_proto *cprot, struct sk_buff *skb);

	int (*connect_ind)(struct concap_proto *cprot);
	int (*disconn_ind)(struct concap_proto *cprot);
};

extern int concap_nop(struct concap_proto *cprot); 

extern int concap_drop_skb(struct concap_proto *cprot, struct sk_buff *skb);
#endif
