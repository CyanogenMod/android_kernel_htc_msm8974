#ifndef _ASM_X86_CHECKSUM_64_H
#define _ASM_X86_CHECKSUM_64_H

/*
 * Checksums for x86-64
 * Copyright 2002 by Andi Kleen, SuSE Labs
 * with some code from asm-x86/checksum.h
 */

#include <linux/compiler.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

static inline __sum16 csum_fold(__wsum sum)
{
	asm("  addl %1,%0\n"
	    "  adcl $0xffff,%0"
	    : "=r" (sum)
	    : "r" ((__force u32)sum << 16),
	      "0" ((__force u32)sum & 0xffff0000));
	return (__force __sum16)(~(__force u32)sum >> 16);
}


static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	asm("  movl (%1), %0\n"
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

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		   unsigned short proto, __wsum sum)
{
	asm("  addl %1, %0\n"
	    "  adcl %2, %0\n"
	    "  adcl %3, %0\n"
	    "  adcl $0, %0\n"
	    : "=r" (sum)
	    : "g" (daddr), "g" (saddr),
	      "g" ((len + proto)<<8), "0" (sum));
	return sum;
}


static inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
					unsigned short len,
					unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

extern __wsum csum_partial(const void *buff, int len, __wsum sum);

#define  _HAVE_ARCH_COPY_AND_CSUM_FROM_USER 1
#define HAVE_CSUM_COPY_USER 1


extern __wsum csum_partial_copy_generic(const void *src, const void *dst,
					int len, __wsum sum,
					int *src_err_ptr, int *dst_err_ptr);


extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
					  int len, __wsum isum, int *errp);
extern __wsum csum_partial_copy_to_user(const void *src, void __user *dst,
					int len, __wsum isum, int *errp);
extern __wsum csum_partial_copy_nocheck(const void *src, void *dst,
					int len, __wsum sum);

#define csum_and_copy_to_user csum_partial_copy_to_user
#define csum_and_copy_from_user csum_partial_copy_from_user

extern __sum16 ip_compute_csum(const void *buff, int len);


struct in6_addr;

#define _HAVE_ARCH_IPV6_CSUM 1
extern __sum16
csum_ipv6_magic(const struct in6_addr *saddr, const struct in6_addr *daddr,
		__u32 len, unsigned short proto, __wsum sum);

static inline unsigned add32_with_carry(unsigned a, unsigned b)
{
	asm("addl %2,%0\n\t"
	    "adcl $0,%0"
	    : "=r" (a)
	    : "0" (a), "r" (b));
	return a;
}

#endif 
