/* checksum.h: FRV checksumming
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_CHECKSUM_H
#define _ASM_CHECKSUM_H

#include <linux/in6.h>

__wsum csum_partial(const void *buff, int len, __wsum sum);

__wsum csum_partial_copy_nocheck(const void *src, void *dst, int len, __wsum sum);

extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
						int len, __wsum sum, int *csum_err);

static inline
__sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int tmp, inc, sum = 0;

	asm("	addcc		gr0,gr0,gr0,icc0\n" 
	    "	subi		%1,#4,%1	\n"
	    "0:					\n"
	    "	ldu.p		@(%1,%3),%4	\n"
	    "	subicc		%2,#1,%2,icc1	\n"
	    "	addxcc.p	%4,%0,%0,icc0	\n"
	    "	bhi		icc1,#2,0b	\n"

	    
	    "	addxcc		gr0,%0,%0,icc0	\n"
	    "	srli		%0,#16,%1	\n"
	    "	sethi		#0,%0		\n"
	    "	add		%1,%0,%0	\n"
	    "	srli		%0,#16,%1	\n"
	    "	add		%1,%0,%0	\n"

	    : "=r" (sum), "=r" (iph), "=r" (ihl), "=r" (inc), "=&r"(tmp)
	    : "0" (sum), "1" (iph), "2" (ihl), "3" (4),
	    "m"(*(volatile struct { int _[100]; } *)iph)
	    : "icc0", "icc1", "memory"
	    );

	return (__force __sum16)~sum;
}

static inline __sum16 csum_fold(__wsum sum)
{
	unsigned int tmp;

	asm("	srli		%0,#16,%1	\n"
	    "	sethi		#0,%0		\n"
	    "	add		%1,%0,%0	\n"
	    "	srli		%0,#16,%1	\n"
	    "	add		%1,%0,%0	\n"
	    : "=r"(sum), "=&r"(tmp)
	    : "0"(sum)
	    );

	return (__force __sum16)~sum;
}

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	asm("	addcc		%1,%0,%0,icc0	\n"
	    "	addxcc		%2,%0,%0,icc0	\n"
	    "	addxcc		%3,%0,%0,icc0	\n"
	    "	addxcc		gr0,%0,%0,icc0	\n"
	    : "=r" (sum)
	    : "r" (daddr), "r" (saddr), "r" (len + proto), "0"(sum)
	    : "icc0"
	    );
	return sum;
}

static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

extern __sum16 ip_compute_csum(const void *buff, int len);

#define _HAVE_ARCH_IPV6_CSUM
static inline __sum16
csum_ipv6_magic(const struct in6_addr *saddr, const struct in6_addr *daddr,
		__u32 len, unsigned short proto, __wsum sum)
{
	unsigned long tmp, tmp2;

	asm("	addcc		%2,%0,%0,icc0	\n"

	    
	    "	ldi		@(%3,0),%1	\n"
	    "	addxcc		%1,%0,%0,icc0	\n"
	    "	ldi		@(%3,4),%2	\n"
	    "	addxcc		%2,%0,%0,icc0	\n"
	    "	ldi		@(%3,8),%1	\n"
	    "	addxcc		%1,%0,%0,icc0	\n"
	    "	ldi		@(%3,12),%2	\n"
	    "	addxcc		%2,%0,%0,icc0	\n"

	    
	    "	ldi		@(%4,0),%1	\n"
	    "	addxcc		%1,%0,%0,icc0	\n"
	    "	ldi		@(%4,4),%2	\n"
	    "	addxcc		%2,%0,%0,icc0	\n"
	    "	ldi		@(%4,8),%1	\n"
	    "	addxcc		%1,%0,%0,icc0	\n"
	    "	ldi		@(%4,12),%2	\n"
	    "	addxcc		%2,%0,%0,icc0	\n"

	    
	    "	addxcc		gr0,%0,%0,icc0	\n"
	    "	srli		%0,#16,%1	\n"
	    "	sethi		#0,%0		\n"
	    "	add		%1,%0,%0	\n"
	    "	srli		%0,#16,%1	\n"
	    "	add		%1,%0,%0	\n"

	    : "=r" (sum), "=&r" (tmp), "=r" (tmp2)
	    : "r" (saddr), "r" (daddr), "0" (sum), "2" (len + proto)
	    : "icc0"
	    );

	return (__force __sum16)~sum;
}

#endif 
