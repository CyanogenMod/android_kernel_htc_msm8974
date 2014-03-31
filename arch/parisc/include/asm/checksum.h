#ifndef _PARISC_CHECKSUM_H
#define _PARISC_CHECKSUM_H

#include <linux/in6.h>

extern __wsum csum_partial(const void *, int, __wsum);

extern __wsum csum_partial_copy_nocheck(const void *, void *, int, __wsum);

extern __wsum csum_partial_copy_from_user(const void __user *src,
		void *dst, int len, __wsum sum, int *errp);

/*
 *	Optimized for IP headers, which always checksum on 4 octet boundaries.
 *
 *	Written by Randolph Chung <tausq@debian.org>, and then mucked with by
 *	LaMont Jones <lamont@debian.org>
 */
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	__asm__ __volatile__ (
"	ldws,ma		4(%1), %0\n"
"	addib,<=	-4, %2, 2f\n"
"\n"
"	ldws		4(%1), %%r20\n"
"	ldws		8(%1), %%r21\n"
"	add		%0, %%r20, %0\n"
"	ldws,ma		12(%1), %%r19\n"
"	addc		%0, %%r21, %0\n"
"	addc		%0, %%r19, %0\n"
"1:	ldws,ma		4(%1), %%r19\n"
"	addib,<		0, %2, 1b\n"
"	addc		%0, %%r19, %0\n"
"\n"
"	extru		%0, 31, 16, %%r20\n"
"	extru		%0, 15, 16, %%r21\n"
"	addc		%%r20, %%r21, %0\n"
"	extru		%0, 15, 16, %%r21\n"
"	add		%0, %%r21, %0\n"
"	subi		-1, %0, %0\n"
"2:\n"
	: "=r" (sum), "=r" (iph), "=r" (ihl)
	: "1" (iph), "2" (ihl)
	: "r19", "r20", "r21", "memory");

	return (__force __sum16)sum;
}

static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;
	sum += (sum << 16) + (sum >> 16);
	return (__force __sum16)(~sum >> 16);
}
 
static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
					       unsigned short len,
					       unsigned short proto,
					       __wsum sum)
{
	__asm__(
	"	add  %1, %0, %0\n"
	"	addc %2, %0, %0\n"
	"	addc %3, %0, %0\n"
	"	addc %%r0, %0, %0\n"
		: "=r" (sum)
		: "r" (daddr), "r"(saddr), "r"(proto+len), "0"(sum));
	return sum;
}

static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
						   unsigned short len,
						   unsigned short proto,
						   __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

static inline __sum16 ip_compute_csum(const void *buf, int len)
{
	 return csum_fold (csum_partial(buf, len, 0));
}


#define _HAVE_ARCH_IPV6_CSUM
static __inline__ __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
					  const struct in6_addr *daddr,
					  __u32 len, unsigned short proto,
					  __wsum sum)
{
	__asm__ __volatile__ (

#if BITS_PER_LONG > 32


"	ldd,ma		8(%1), %%r19\n"	
"	ldd,ma		8(%2), %%r20\n"	
"	add		%8, %3, %3\n"
"	add		%%r19, %0, %0\n"
"	ldd,ma		8(%1), %%r21\n"	
"	ldd,ma		8(%2), %%r22\n"	
"	add,dc		%%r20, %0, %0\n"
"	add,dc		%%r21, %0, %0\n"
"	add,dc		%%r22, %0, %0\n"
"	add,dc		%3, %0, %0\n"  
"	extrd,u		%0, 31, 32, %%r19\n"	
"	depdi		0, 31, 32, %0\n"	
"	add		%%r19, %0, %0\n"	
"	addc		0, %0, %0\n"		

#else


"	ldw,ma		4(%1), %%r19\n"	
"	ldw,ma		4(%2), %%r20\n"	
"	add		%8, %3, %3\n"	
"	add		%%r19, %0, %0\n"
"	ldw,ma		4(%1), %%r21\n"	
"	addc		%%r20, %0, %0\n"
"	ldw,ma		4(%2), %%r22\n"	
"	addc		%%r21, %0, %0\n"
"	ldw,ma		4(%1), %%r19\n"	
"	addc		%%r22, %0, %0\n"
"	ldw,ma		4(%2), %%r20\n"	
"	addc		%%r19, %0, %0\n"
"	ldw,ma		4(%1), %%r21\n"	
"	addc		%%r20, %0, %0\n"
"	ldw,ma		4(%2), %%r22\n"	
"	addc		%%r21, %0, %0\n"
"	addc		%%r22, %0, %0\n"
"	addc		%3, %0, %0\n"	

#endif
	: "=r" (sum), "=r" (saddr), "=r" (daddr), "=r" (len)
	: "0" (sum), "1" (saddr), "2" (daddr), "3" (len), "r" (proto)
	: "r19", "r20", "r21", "r22", "memory");
	return csum_fold(sum);
}

#define HAVE_CSUM_COPY_USER
static __inline__ __wsum csum_and_copy_to_user(const void *src,
						      void __user *dst,
						      int len, __wsum sum,
						      int *err_ptr)
{
	
	sum = csum_partial(src, len, sum);
	 
	if (copy_to_user(dst, src, len)) {
		*err_ptr = -EFAULT;
		return (__force __wsum)-1;
	}

	return sum;
}

#endif

