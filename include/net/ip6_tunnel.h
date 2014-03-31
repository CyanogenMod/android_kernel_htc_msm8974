#ifndef _NET_IP6_TUNNEL_H
#define _NET_IP6_TUNNEL_H

#include <linux/ipv6.h>
#include <linux/netdevice.h>
#include <linux/ip6_tunnel.h>

#define IP6_TNL_F_CAP_XMIT 0x10000
#define IP6_TNL_F_CAP_RCV 0x20000


struct ip6_tnl {
	struct ip6_tnl __rcu *next;	
	struct net_device *dev;	
	struct ip6_tnl_parm parms;	
	struct flowi fl;	
	struct dst_entry *dst_cache;    
	u32 dst_cookie;
};


struct ipv6_tlv_tnl_enc_lim {
	__u8 type;		
	__u8 length;		
	__u8 encap_limit;	
} __packed;

#endif
