/*
 * Licensed under the GPL
 */

#ifndef __UM_SYSDEP_CHECKSUM_H
#define __UM_SYSDEP_CHECKSUM_H

#include "linux/string.h"
#include "linux/in6.h"
#include "asm/uaccess.h"

extern __wsum csum_partial(const void *buff, int len, __wsum sum);


static __inline__
__wsum csum_partial_copy_nocheck(const void *src, void *dst,
				       int len, __wsum sum)
{
	memcpy(dst, src, len);
	return(csum_partial(dst, len, sum));
}

static __inline__
__wsum csum_partial_copy_from_user(const void __user *src,
                                         void *dst, int len, __wsum sum,
                                         int *err_ptr)
{
        if (copy_from_user(dst, src, len)) {
                *err_ptr = -EFAULT;
                return (__force __wsum)-1;
        }
        return csum_partial(dst, len, sum);
}

static inline __sum16 csum_fold(__wsum sum)
{
	__asm__(
		"  addl %1,%0\n"
		"  adcl $0xffff,%0"
		: "=r" (sum)
		: "r" ((__force u32)sum << 16),
		  "0" ((__force u32)sum & 0xffff0000)
	);
	return (__force __sum16)(~(__force u32)sum >> 16);
}

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		   unsigned short proto, __wsum sum)
{
	asm("  addl %1, %0\n"
	    "  adcl %2, %0\n"
	    "  adcl %3, %0\n"
	    "  adcl $0, %0\n"
		: "=r" (sum)
	    : "g" (daddr), "g" (saddr), "g" ((len + proto) << 8), "0" (sum));
	return sum;
}

static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
					   unsigned short len,
					   unsigned short proto,
					   __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	asm(	"  movl (%1), %0\n"
		"  subl $4, %2\n"
		"  jbe 2f\n"
		"  addl 4(%1), %0\n"
		"  adcl 8(%1), %0\n"
		"  adcl 12(%1), %0\n"
		"1: adcl 16(%1), %0\n"
		"  lea 4(%1), %1\n"
		"  decl %2\n"
		"  jne	1b\n"
		"  adcl $0, %0\n"
		"  movl %0, %2\n"
		"  shrl $16, %0\n"
		"  addw %w2, %w0\n"
		"  adcl $0, %0\n"
		"  notl %0\n"
		"2:"
	: "=r" (sum), "=r" (iph), "=r" (ihl)
	: "1" (iph), "2" (ihl)
	: "memory");
	return (__force __sum16)sum;
}

static inline unsigned add32_with_carry(unsigned a, unsigned b)
{
        asm("addl %2,%0\n\t"
            "adcl $0,%0"
            : "=r" (a)
            : "0" (a), "r" (b));
        return a;
}

extern __sum16 ip_compute_csum(const void *buff, int len);

#endif
