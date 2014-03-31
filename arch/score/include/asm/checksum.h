#ifndef _ASM_SCORE_CHECKSUM_H
#define _ASM_SCORE_CHECKSUM_H

#include <linux/in6.h>
#include <asm/uaccess.h>

unsigned int csum_partial(const void *buff, int len, __wsum sum);
unsigned int csum_partial_copy_from_user(const char *src, char *dst, int len,
					unsigned int sum, int *csum_err);
unsigned int csum_partial_copy(const char *src, char *dst,
					int len, unsigned int sum);


#define HAVE_CSUM_COPY_USER
static inline
__wsum csum_and_copy_to_user(const void *src, void __user *dst, int len,
			__wsum sum, int *err_ptr)
{
	sum = csum_partial(src, len, sum);
	if (copy_to_user(dst, src, len)) {
		*err_ptr = -EFAULT;
		return (__force __wsum) -1; 
	}
	return sum;
}


#define csum_partial_copy_nocheck csum_partial_copy

static inline __sum16 csum_fold(__wsum sum)
{
	__asm__ __volatile__(
		".set volatile\n\t"
		".set\tr1\n\t"
		"slli\tr1,%0, 16\n\t"
		"add\t%0,%0, r1\n\t"
		"cmp.c\tr1, %0\n\t"
		"srli\t%0, %0, 16\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:ldi\tr30, 0xffff\n\t"
		"xor\t%0, %0, r30\n\t"
		"slli\t%0, %0, 16\n\t"
		"srli\t%0, %0, 16\n\t"
		".set\tnor1\n\t"
		".set optimize\n\t"
		: "=r" (sum)
		: "0" (sum));
	return sum;
}

static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;
	unsigned long dummy;

	__asm__ __volatile__(
		".set volatile\n\t"
		".set\tnor1\n\t"
		"lw\t%0, [%1]\n\t"
		"subri\t%2, %2, 4\n\t"
		"slli\t%2, %2, 2\n\t"
		"lw\t%3, [%1, 4]\n\t"
		"add\t%2, %2, %1\n\t"
		"add\t%0, %0, %3\n\t"
		"cmp.c\t%3, %0\n\t"
		"lw\t%3, [%1, 8]\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:\n\t"
		"add\t%0, %0, %3\n\t"
		"cmp.c\t%3, %0\n\t"
		"lw\t%3, [%1, 12]\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:add\t%0, %0, %3\n\t"
		"cmp.c\t%3, %0\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n"

		"1:\tlw\t%3, [%1, 16]\n\t"
		"addi\t%1, 4\n\t"
		"add\t%0, %0, %3\n\t"
		"cmp.c\t%3, %0\n\t"
		"bleu\t2f\n\t"
		"addi\t%0, 0x1\n"
		"2:cmp.c\t%2, %1\n\t"
		"bne\t1b\n\t"

		".set\tr1\n\t"
		".set optimize\n\t"
		: "=&r" (sum), "=&r" (iph), "=&r" (ihl), "=&r" (dummy)
		: "1" (iph), "2" (ihl));

	return csum_fold(sum);
}

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		unsigned short proto, __wsum sum)
{
	unsigned long tmp = (ntohs(len) << 16) + proto * 256;
	__asm__ __volatile__(
		".set volatile\n\t"
		"add\t%0, %0, %2\n\t"
		"cmp.c\t%2, %0\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:\n\t"
		"add\t%0, %0, %3\n\t"
		"cmp.c\t%3, %0\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:\n\t"
		"add\t%0, %0, %4\n\t"
		"cmp.c\t%4, %0\n\t"
		"bleu\t1f\n\t"
		"addi\t%0, 0x1\n\t"
		"1:\n\t"
		".set optimize\n\t"
		: "=r" (sum)
		: "0" (daddr), "r"(saddr),
		"r" (tmp),
		"r" (sum));
	return sum;
}

static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}


static inline unsigned short ip_compute_csum(const void *buff, int len)
{
	return csum_fold(csum_partial(buff, len, 0));
}

#define _HAVE_ARCH_IPV6_CSUM
static inline __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
				const struct in6_addr *daddr,
				__u32 len, unsigned short proto,
				__wsum sum)
{
	__asm__ __volatile__(
		".set\tnoreorder\t\t\t# csum_ipv6_magic\n\t"
		".set\tnoat\n\t"
		"addu\t%0, %5\t\t\t# proto (long in network byte order)\n\t"
		"sltu\t$1, %0, %5\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %6\t\t\t# csum\n\t"
		"sltu\t$1, %0, %6\n\t"
		"lw\t%1, 0(%2)\t\t\t# four words source address\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 4(%2)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 8(%2)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 12(%2)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 0(%3)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 4(%3)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 8(%3)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"lw\t%1, 12(%3)\n\t"
		"addu\t%0, $1\n\t"
		"addu\t%0, %1\n\t"
		"sltu\t$1, %0, %1\n\t"
		"addu\t%0, $1\t\t\t# Add final carry\n\t"
		".set\tnoat\n\t"
		".set\tnoreorder"
		: "=r" (sum), "=r" (proto)
		: "r" (saddr), "r" (daddr),
		  "0" (htonl(len)), "1" (htonl(proto)), "r" (sum));

	return csum_fold(sum);
}
#endif 
