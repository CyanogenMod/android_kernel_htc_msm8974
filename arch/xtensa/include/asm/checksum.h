/*
 * include/asm-xtensa/checksum.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_CHECKSUM_H
#define _XTENSA_CHECKSUM_H

#include <linux/in6.h>
#include <variant/core.h>

asmlinkage __wsum csum_partial(const void *buff, int len, __wsum sum);


asmlinkage __wsum csum_partial_copy_generic(const void *src, void *dst, int len, __wsum sum,
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


static __inline__ __sum16 csum_fold(__wsum sum)
{
	unsigned int __dummy;
	__asm__("extui	%1, %0, 16, 16\n\t"
		"extui	%0 ,%0, 0, 16\n\t"
		"add	%0, %0, %1\n\t"
		"slli	%1, %0, 16\n\t"
		"add	%0, %0, %1\n\t"
		"extui	%0, %0, 16, 16\n\t"
		"neg	%0, %0\n\t"
		"addi	%0, %0, -1\n\t"
		"extui	%0, %0, 0, 16\n\t"
		: "=r" (sum), "=&r" (__dummy)
		: "0" (sum));
	return (__force __sum16)sum;
}

static __inline__ __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum, tmp, endaddr;

	__asm__ __volatile__(
		"sub		%0, %0, %0\n\t"
#if XCHAL_HAVE_LOOPS
		"loopgtz	%2, 2f\n\t"
#else
		"beqz		%2, 2f\n\t"
		"slli		%4, %2, 2\n\t"
		"add		%4, %4, %1\n\t"
		"0:\t"
#endif
		"l32i		%3, %1, 0\n\t"
		"add		%0, %0, %3\n\t"
		"bgeu		%0, %3, 1f\n\t"
		"addi		%0, %0, 1\n\t"
		"1:\t"
		"addi		%1, %1, 4\n\t"
#if !XCHAL_HAVE_LOOPS
		"blt		%1, %4, 0b\n\t"
#endif
		"2:\t"
		: "=r" (sum), "=r" (iph), "=r" (ihl), "=&r" (tmp), "=&r" (endaddr)
		: "1" (iph), "2" (ihl)
		: "memory");

	return	csum_fold(sum);
}

static __inline__ __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
						   unsigned short len,
						   unsigned short proto,
						   __wsum sum)
{

#ifdef __XTENSA_EL__
	unsigned long len_proto = (len + proto) << 8;
#elif defined(__XTENSA_EB__)
	unsigned long len_proto = len + proto;
#else
# error processor byte order undefined!
#endif
	__asm__("add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"add	%0, %0, %2\n\t"
		"bgeu	%0, %2, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"add	%0, %0, %3\n\t"
		"bgeu	%0, %3, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		: "=r" (sum), "=r" (len_proto)
		: "r" (daddr), "r" (saddr), "1" (len_proto), "0" (sum));
	return sum;
}

static __inline__ __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
						       unsigned short len,
						       unsigned short proto,
						       __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}


static __inline__ __sum16 ip_compute_csum(const void *buff, int len)
{
    return csum_fold (csum_partial(buff, len, 0));
}

#define _HAVE_ARCH_IPV6_CSUM
static __inline__ __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
					  const struct in6_addr *daddr,
					  __u32 len, unsigned short proto,
					  __wsum sum)
{
	unsigned int __dummy;
	__asm__("l32i	%1, %2, 0\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %2, 4\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %2, 8\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %2, 12\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %3, 0\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %3, 4\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %3, 8\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"l32i	%1, %3, 12\n\t"
		"add	%0, %0, %1\n\t"
		"bgeu	%0, %1, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"add	%0, %0, %4\n\t"
		"bgeu	%0, %4, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		"add	%0, %0, %5\n\t"
		"bgeu	%0, %5, 1f\n\t"
		"addi	%0, %0, 1\n\t"
		"1:\t"
		: "=r" (sum), "=&r" (__dummy)
		: "r" (saddr), "r" (daddr),
		  "r" (htonl(len)), "r" (htonl(proto)), "0" (sum)
		: "memory");

	return csum_fold(sum);
}

#define HAVE_CSUM_COPY_USER
static __inline__ __wsum csum_and_copy_to_user(const void *src, void __user *dst,
				    int len, __wsum sum, int *err_ptr)
{
	if (access_ok(VERIFY_WRITE, dst, len))
		return csum_partial_copy_generic(src, dst, len, sum, NULL, err_ptr);

	if (len)
		*err_ptr = -EFAULT;

	return (__force __wsum)-1; 
}
#endif
