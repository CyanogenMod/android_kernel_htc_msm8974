/*
 *
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		IP/TCP/UDP checksumming routines
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Tom May, <ftom@netcom.com>
 *		Andreas Schwab, <schwab@issan.informatik.uni-dortmund.de>
 *		Lots of code moved from tcp.c and ip.c; see those files
 *		for more names.
 *
 * 03/02/96	Jes Sorensen, Andreas Schwab, Roman Hodek:
 *		Fixed some nasty bugs, causing some horrible crashes.
 *		A: At some points, the sum (%0) was used as
 *		length-counter instead of the length counter
 *		(%1). Thanks to Roman Hodek for pointing this out.
 *		B: GCC seems to mess up if one uses too many
 *		data-registers to hold input values and one tries to
 *		specify d0 and d1 as scratch registers. Letting gcc
 *		choose these registers itself solves the problem.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */


#include <linux/export.h>
#include <net/checksum.h>

#include <asm/byteorder.h>

#ifndef do_csum
static inline unsigned short from32to16(unsigned int x)
{
	
	x = (x & 0xffff) + (x >> 16);
	
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static unsigned int do_csum(const unsigned char *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const unsigned char *end = buff + ((unsigned)len & ~3);
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}
#endif

__sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	return (__force __sum16)~do_csum(iph, ihl*4);
}
EXPORT_SYMBOL(ip_fast_csum);

__wsum csum_partial(const void *buff, int len, __wsum wsum)
{
	unsigned int sum = (__force unsigned int)wsum;
	unsigned int result = do_csum(buff, len);

	
	result += sum;
	if (sum > result)
		result += 1;
	return (__force __wsum)result;
}
EXPORT_SYMBOL(csum_partial);

__sum16 ip_compute_csum(const void *buff, int len)
{
	return (__force __sum16)~do_csum(buff, len);
}
EXPORT_SYMBOL(ip_compute_csum);

__wsum
csum_partial_copy_from_user(const void __user *src, void *dst, int len,
						__wsum sum, int *csum_err)
{
	int missing;

	missing = __copy_from_user(dst, src, len);
	if (missing) {
		memset(dst + len - missing, 0, missing);
		*csum_err = -EFAULT;
	} else
		*csum_err = 0;

	return csum_partial(dst, len, sum);
}
EXPORT_SYMBOL(csum_partial_copy_from_user);

__wsum
csum_partial_copy(const void *src, void *dst, int len, __wsum sum)
{
	memcpy(dst, src, len);
	return csum_partial(dst, len, sum);
}
EXPORT_SYMBOL(csum_partial_copy);

#ifndef csum_tcpudp_nofold
__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
			unsigned short len,
			unsigned short proto,
			__wsum sum)
{
	unsigned long long s = (__force u32)sum;

	s += (__force u32)saddr;
	s += (__force u32)daddr;
#ifdef __BIG_ENDIAN
	s += proto + len;
#else
	s += (proto + len) << 8;
#endif
	s += (s >> 32);
	return (__force __wsum)s;
}
EXPORT_SYMBOL(csum_tcpudp_nofold);
#endif
