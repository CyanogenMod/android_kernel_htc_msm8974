#ifndef _NET_DN_ROUTE_H
#define _NET_DN_ROUTE_H

/******************************************************************************
    (c) 1995-1998 E.M. Serrat		emserrat@geocities.com
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*******************************************************************************/

extern struct sk_buff *dn_alloc_skb(struct sock *sk, int size, gfp_t pri);
extern int dn_route_output_sock(struct dst_entry **pprt, struct flowidn *, struct sock *sk, int flags);
extern int dn_cache_dump(struct sk_buff *skb, struct netlink_callback *cb);
extern void dn_rt_cache_flush(int delay);

#define DN_RT_F_PID 0x07 
#define DN_RT_F_PF  0x80 
#define DN_RT_F_VER 0x40 
#define DN_RT_F_IE  0x20 
#define DN_RT_F_RTS 0x10 
#define DN_RT_F_RQR 0x08 

#define DN_RT_PKT_MSK   0x06
#define DN_RT_PKT_SHORT 0x02 
#define DN_RT_PKT_LONG  0x06 

#define DN_RT_PKT_CNTL  0x01 
#define DN_RT_CNTL_MSK  0x0f 
#define DN_RT_PKT_INIT  0x01 
#define DN_RT_PKT_VERI  0x03 
#define DN_RT_PKT_HELO  0x05 
#define DN_RT_PKT_L1RT  0x07 
#define DN_RT_PKT_L2RT  0x09 
#define DN_RT_PKT_ERTH  0x0b 
#define DN_RT_PKT_EEDH  0x0d 

#define DN_RT_INFO_TYPE 0x03 
#define DN_RT_INFO_L1RT 0x02 
#define DN_RT_INFO_L2RT 0x01 
#define DN_RT_INFO_ENDN 0x03 
#define DN_RT_INFO_VERI 0x04 
#define DN_RT_INFO_RJCT 0x08 
#define DN_RT_INFO_VFLD 0x10 
#define DN_RT_INFO_NOML 0x20 
#define DN_RT_INFO_BLKR 0x40 

struct dn_route {
	struct dst_entry dst;

	struct flowidn fld;

	__le16 rt_saddr;
	__le16 rt_daddr;
	__le16 rt_gateway;
	__le16 rt_local_src;	
	__le16 rt_src_map;
	__le16 rt_dst_map;

	unsigned rt_flags;
	unsigned rt_type;
};

static inline bool dn_is_input_route(struct dn_route *rt)
{
	return rt->fld.flowidn_iif != 0;
}

static inline bool dn_is_output_route(struct dn_route *rt)
{
	return rt->fld.flowidn_iif == 0;
}

extern void dn_route_init(void);
extern void dn_route_cleanup(void);

#include <net/sock.h>
#include <linux/if_arp.h>

static inline void dn_rt_send(struct sk_buff *skb)
{
	dev_queue_xmit(skb);
}

static inline void dn_rt_finish_output(struct sk_buff *skb, char *dst, char *src)
{
	struct net_device *dev = skb->dev;

	if ((dev->type != ARPHRD_ETHER) && (dev->type != ARPHRD_LOOPBACK))
		dst = NULL;

	if (dev_hard_header(skb, dev, ETH_P_DNA_RT, dst, src, skb->len) >= 0)
		dn_rt_send(skb);
	else
		kfree_skb(skb);
}

#endif 
