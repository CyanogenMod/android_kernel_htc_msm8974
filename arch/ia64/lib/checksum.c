/*
 * Network checksum routines
 *
 * Copyright (C) 1999, 2003 Hewlett-Packard Co
 *	Stephane Eranian <eranian@hpl.hp.com>
 *
 * Most of the code coming from arch/alpha/lib/checksum.c
 *
 * This file contains network checksum routines that are better done
 * in an architecture-specific manner due to speed..
 */

#include <linux/module.h>
#include <linux/string.h>

#include <asm/byteorder.h>

static inline unsigned short
from64to16 (unsigned long x)
{
	
	x = (x & 0xffffffff) + (x >> 32);
	
	x = (x & 0xffff) + (x >> 16);
	
	x = (x & 0xffff) + (x >> 16);
	
	x = (x & 0xffff) + (x >> 16);
	return x;
}

__sum16
csum_tcpudp_magic (__be32 saddr, __be32 daddr, unsigned short len,
		   unsigned short proto, __wsum sum)
{
	return (__force __sum16)~from64to16(
		(__force u64)saddr + (__force u64)daddr +
		(__force u64)sum + ((len + proto) << 8));
}

EXPORT_SYMBOL(csum_tcpudp_magic);

__wsum
csum_tcpudp_nofold (__be32 saddr, __be32 daddr, unsigned short len,
		    unsigned short proto, __wsum sum)
{
	unsigned long result;

	result = (__force u64)saddr + (__force u64)daddr +
		 (__force u64)sum + ((len + proto) << 8);

	
	
	result = (result & 0xffffffff) + (result >> 32);
	
	result = (result & 0xffffffff) + (result >> 32);
	return (__force __wsum)result;
}
EXPORT_SYMBOL(csum_tcpudp_nofold);

extern unsigned long do_csum (const unsigned char *, long);

__wsum csum_partial(const void *buff, int len, __wsum sum)
{
	u64 result = do_csum(buff, len);

	
	result += (__force u32)sum;
	
	result = (result & 0xffffffff) + (result >> 32);
	return (__force __wsum)result;
}

EXPORT_SYMBOL(csum_partial);

__sum16 ip_compute_csum (const void *buff, int len)
{
	return (__force __sum16)~do_csum(buff,len);
}

EXPORT_SYMBOL(ip_compute_csum);
