#ifndef _ASM_POWERPC_CHECKSUM_H
#define _ASM_POWERPC_CHECKSUM_H
#ifdef __KERNEL__

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

extern __sum16 ip_fast_csum(const void *iph, unsigned int ihl);

extern __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
					unsigned short len,
					unsigned short proto,
					__wsum sum);

extern __wsum csum_partial(const void *buff, int len, __wsum sum);

extern __wsum csum_partial_copy_generic(const void *src, void *dst,
					      int len, __wsum sum,
					      int *src_err, int *dst_err);

#ifdef __powerpc64__
#define _HAVE_ARCH_COPY_AND_CSUM_FROM_USER
extern __wsum csum_and_copy_from_user(const void __user *src, void *dst,
				      int len, __wsum sum, int *err_ptr);
#define HAVE_CSUM_COPY_USER
extern __wsum csum_and_copy_to_user(const void *src, void __user *dst,
				    int len, __wsum sum, int *err_ptr);
#else
#define csum_partial_copy_from_user(src, dst, len, sum, errp)   \
        csum_partial_copy_generic((__force const void *)(src), (dst), (len), (sum), (errp), NULL)
#endif

#define csum_partial_copy_nocheck(src, dst, len, sum)   \
        csum_partial_copy_generic((src), (dst), (len), (sum), NULL, NULL)


static inline __sum16 csum_fold(__wsum sum)
{
	unsigned int tmp;

	
	__asm__("rlwinm %0,%1,16,0,31" : "=r" (tmp) : "r" (sum));
	return (__force __sum16)(~((__force u32)sum + tmp) >> 16);
}

static inline __sum16 ip_compute_csum(const void *buff, int len)
{
	return csum_fold(csum_partial(buff, len, 0));
}

static inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
                                     unsigned short len,
                                     unsigned short proto,
                                     __wsum sum)
{
#ifdef __powerpc64__
	unsigned long s = (__force u32)sum;

	s += (__force u32)saddr;
	s += (__force u32)daddr;
	s += proto + len;
	s += (s >> 32);
	return (__force __wsum) s;
#else
    __asm__("\n\
	addc %0,%0,%1 \n\
	adde %0,%0,%2 \n\
	adde %0,%0,%3 \n\
	addze %0,%0 \n\
	"
	: "=r" (sum)
	: "r" (daddr), "r"(saddr), "r"(proto + len), "0"(sum));
	return sum;
#endif
}
#endif 
#endif
