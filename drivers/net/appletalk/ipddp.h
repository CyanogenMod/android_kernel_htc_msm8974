
#ifndef __LINUX_IPDDP_H
#define __LINUX_IPDDP_H

#ifdef __KERNEL__

#define SIOCADDIPDDPRT   (SIOCDEVPRIVATE)
#define SIOCDELIPDDPRT   (SIOCDEVPRIVATE+1)
#define SIOCFINDIPDDPRT  (SIOCDEVPRIVATE+2)

struct ipddp_route
{
        struct net_device *dev;             
        __be32 ip;                       
        struct atalk_addr at;              
        int flags;
        struct ipddp_route *next;
};

#define IPDDP_ENCAP	1
#define IPDDP_DECAP	2

#endif	
#endif	
