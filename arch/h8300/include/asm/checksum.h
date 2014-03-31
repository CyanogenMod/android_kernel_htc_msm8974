#ifndef _H8300_CHECKSUM_H
#define _H8300_CHECKSUM_H

__wsum csum_partial(const void *buff, int len, __wsum sum);


__wsum csum_partial_copy_nocheck(const void *src, void *dst, int len, __wsum sum);



extern __wsum csum_partial_copy_from_user(const void __user *src, void *dst,
						int len, __wsum sum, int *csum_err);

__sum16 ip_fast_csum(const void *iph, unsigned int ihl);



static inline __sum16 csum_fold(__wsum sum)
{
	__asm__("mov.l %0,er0\n\t"
		"add.w e0,r0\n\t"
		"xor.w e0,e0\n\t"
		"rotxl.w e0\n\t"
		"add.w e0,r0\n\t"
		"sub.w e0,e0\n\t"
		"mov.l er0,%0"
		: "=r"(sum)
		: "0"(sum)
		: "er0");
	return (__force __sum16)~sum;
}



static inline __wsum
csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	__asm__ ("sub.l er0,er0\n\t"
		 "add.l %2,%0\n\t"
		 "addx	#0,r0l\n\t"
		 "add.l	%3,%0\n\t"
		 "addx	#0,r0l\n\t"
		 "add.l %4,%0\n\t"
		 "addx	#0,r0l\n\t"
		 "add.l	er0,%0\n\t"
		 "bcc	1f\n\t"
		 "inc.l	#1,%0\n"
		 "1:"
		 : "=&r" (sum)
		 : "0" (sum), "r" (daddr), "r" (saddr), "r" (len + proto)
		 :"er0");
	return sum;
}

static inline __sum16
csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
		  unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}


extern __sum16 ip_compute_csum(const void *buff, int len);

#endif 
