#ifndef __ASM_SH_CHECKSUM_H
#define __ASM_SH_CHECKSUM_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Kaz Kojima & Niibe Yutaka
 */

#include <linux/in6.h>

asmlinkage __wsum csum_partial(const void *buff, int len, __wsum sum);


asmlinkage __wsum csum_partial_copy_generic(const void *src, void *dst,
					    int len, __wsum sum,
					    int *src_err_ptr, int *dst_err_ptr);

static inline
__wsum csum_partial_copy_nocheck(const void *src, void *dst,
				 int len, __wsum sum)
{
	return csum_partial_copy_generic(src, dst, len, sum, NULL, NULL);
}

static inline
__wsum csum_partial_copy_from_user(const void __user *src, void *dst,
				   int len, __wsum sum, int *err_ptr)
{
	return csum_partial_copy_generic((__force const void *)src, dst,
					len, sum, err_ptr, NULL);
}


static inline __sum16 csum_fold(__wsum sum)
{
	unsigned int __dummy;
	__asm__("swap.w %0, %1\n\t"
		"extu.w	%0, %0\n\t"
		"extu.w	%1, %1\n\t"
		"add	%1, %0\n\t"
		"swap.w	%0, %1\n\t"
		"add	%1, %0\n\t"
		"not	%0, %0\n\t"
		: "=r" (sum), "=&r" (__dummy)
		: "0" (sum)
		: "t");
	return (__force __sum16)sum;
}

static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum, __dummy0, __dummy1;

	__asm__ __volatile__(
		"mov.l	@%1+, %0\n\t"
		"mov.l	@%1+, %3\n\t"
		"add	#-2, %2\n\t"
		"clrt\n\t"
		"1:\t"
		"addc	%3, %0\n\t"
		"movt	%4\n\t"
		"mov.l	@%1+, %3\n\t"
		"dt	%2\n\t"
		"bf/s	1b\n\t"
		" cmp/eq #1, %4\n\t"
		"addc	%3, %0\n\t"
		"addc	%2, %0"	    
	: "=r" (sum), "=r" (iph), "=r" (ihl), "=&r" (__dummy0), "=&z" (__dummy1)
	: "1" (iph), "2" (ihl)
	: "t", "memory");

	return	csum_fold(sum);
}

static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
					unsigned short len,
					unsigned short proto,
					__wsum sum)
{
#ifdef __LITTLE_ENDIAN__
	unsigned long len_proto = (proto + len) << 8;
#else
	unsigned long len_proto = proto + len;
#endif
	__asm__("clrt\n\t"
		"addc	%0, %1\n\t"
		"addc	%2, %1\n\t"
		"addc	%3, %1\n\t"
		"movt	%0\n\t"
		"add	%1, %0"
		: "=r" (sum), "=r" (len_proto)
		: "r" (daddr), "r" (saddr), "1" (len_proto), "0" (sum)
		: "t");

	return sum;
}

static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
					unsigned short len,
					unsigned short proto,
					__wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

static inline __sum16 ip_compute_csum(const void *buff, int len)
{
    return csum_fold(csum_partial(buff, len, 0));
}

#define _HAVE_ARCH_IPV6_CSUM
static inline __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
				      const struct in6_addr *daddr,
				      __u32 len, unsigned short proto,
				      __wsum sum)
{
	unsigned int __dummy;
	__asm__("clrt\n\t"
		"mov.l	@(0,%2), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(4,%2), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(8,%2), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(12,%2), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(0,%3), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(4,%3), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(8,%3), %1\n\t"
		"addc	%1, %0\n\t"
		"mov.l	@(12,%3), %1\n\t"
		"addc	%1, %0\n\t"
		"addc	%4, %0\n\t"
		"addc	%5, %0\n\t"
		"movt	%1\n\t"
		"add	%1, %0\n"
		: "=r" (sum), "=&r" (__dummy)
		: "r" (saddr), "r" (daddr),
		  "r" (htonl(len)), "r" (htonl(proto)), "0" (sum)
		: "t");

	return csum_fold(sum);
}

#define HAVE_CSUM_COPY_USER
static inline __wsum csum_and_copy_to_user(const void *src,
					   void __user *dst,
					   int len, __wsum sum,
					   int *err_ptr)
{
	if (access_ok(VERIFY_WRITE, dst, len))
		return csum_partial_copy_generic((__force const void *)src,
						dst, len, sum, NULL, err_ptr);

	if (len)
		*err_ptr = -EFAULT;

	return (__force __wsum)-1; 
}
#endif 
