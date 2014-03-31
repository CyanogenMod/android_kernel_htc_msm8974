#ifndef __ASM_GENERIC_CHECKSUM_H
#define __ASM_GENERIC_CHECKSUM_H

extern __wsum csum_partial(const void *buff, int len, __wsum sum);

extern __wsum csum_partial_copy(const void *src, void *dst, int len, __wsum sum);

extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
					int len, __wsum sum, int *csum_err);

#ifndef csum_partial_copy_nocheck
#define csum_partial_copy_nocheck(src, dst, len, sum)	\
	csum_partial_copy((src), (dst), (len), (sum))
#endif

extern __sum16 ip_fast_csum(const void *iph, unsigned int ihl);

static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (__force __sum16)~sum;
}

#ifndef csum_tcpudp_nofold
extern __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		unsigned short proto, __wsum sum);
#endif

#ifndef csum_tcpudp_magic
static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}
#endif

extern __sum16 ip_compute_csum(const void *buff, int len);

#endif 
