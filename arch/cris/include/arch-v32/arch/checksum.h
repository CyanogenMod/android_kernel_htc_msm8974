#ifndef _ASM_CRIS_ARCH_CHECKSUM_H
#define _ASM_CRIS_ARCH_CHECKSUM_H

static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
		   unsigned short len, unsigned short proto, __wsum sum)
{
	__wsum res;

	__asm__ __volatile__ ("add.d %2, %0\n\t"
			      "addc %3, %0\n\t"
			      "addc %4, %0\n\t"
			      "addc 0, %0\n\t"
			      : "=r" (res)
			      : "0" (sum), "r" (daddr), "r" (saddr), \
			      "r" ((len + proto) << 8));

	return res;
}

#endif 
