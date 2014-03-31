/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_CHECKSUM_H
#define __ASM_AVR32_CHECKSUM_H

__wsum csum_partial(const void *buff, int len, __wsum sum);

__wsum csum_partial_copy_generic(const void *src, void *dst, int len,
				       __wsum sum, int *src_err_ptr,
				       int *dst_err_ptr);

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
	return csum_partial_copy_generic((const void __force *)src, dst, len,
					 sum, err_ptr, NULL);
}

static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum, tmp;

	__asm__ __volatile__(
		"	ld.w	%0, %1++\n"
		"	ld.w	%3, %1++\n"
		"	sub	%2, 4\n"
		"	add	%0, %3\n"
		"	ld.w	%3, %1++\n"
		"	adc	%0, %0, %3\n"
		"	ld.w	%3, %1++\n"
		"	adc	%0, %0, %3\n"
		"	acr	%0\n"
		"1:	ld.w	%3, %1++\n"
		"	add	%0, %3\n"
		"	acr	%0\n"
		"	sub	%2, 1\n"
		"	brne	1b\n"
		"	lsl	%3, %0, 16\n"
		"	andl	%0, 0\n"
		"	mov	%2, 0xffff\n"
		"	add	%0, %3\n"
		"	adc	%0, %0, %2\n"
		"	com	%0\n"
		"	lsr	%0, 16\n"
		: "=r"(sum), "=r"(iph), "=r"(ihl), "=r"(tmp)
		: "1"(iph), "2"(ihl)
		: "memory", "cc");
	return (__force __sum16)sum;
}


static inline __sum16 csum_fold(__wsum sum)
{
	unsigned int tmp;

	asm("	bfextu	%1, %0, 0, 16\n"
	    "	lsr	%0, 16\n"
	    "	add	%0, %1\n"
	    "	bfextu	%1, %0, 16, 16\n"
	    "	add	%0, %1"
	    : "=&r"(sum), "=&r"(tmp)
	    : "0"(sum));

	return (__force __sum16)~sum;
}

static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
					       unsigned short len,
					       unsigned short proto,
					       __wsum sum)
{
	asm("	add	%0, %1\n"
	    "	adc	%0, %0, %2\n"
	    "	adc	%0, %0, %3\n"
	    "	acr	%0"
	    : "=r"(sum)
	    : "r"(daddr), "r"(saddr), "r"(len + proto),
	      "0"(sum)
	    : "cc");

	return sum;
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
    return csum_fold(csum_partial(buff, len, 0));
}

#endif 
