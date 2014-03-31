
#ifndef _CRIS_CHECKSUM_H
#define _CRIS_CHECKSUM_H

#include <arch/checksum.h>

__wsum csum_partial(const void *buff, int len, __wsum sum);


__wsum csum_partial_copy_nocheck(const void *src, void *dst,
				       int len, __wsum sum);


static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum = (sum & 0xffff) + (sum >> 16); 
	sum = (sum & 0xffff) + (sum >> 16); 
	return (__force __sum16)~sum;
}

extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
						int len, __wsum sum,
						int *errptr);


static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	return csum_fold(csum_partial(iph, ihl * 4, 0));
}
 

static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
						   unsigned short len,
						   unsigned short proto,
						   __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}


static inline __sum16 ip_compute_csum(const void *buff, int len)
{
	return csum_fold (csum_partial(buff, len, 0));
}

#endif
