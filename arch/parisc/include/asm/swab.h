#ifndef _PARISC_SWAB_H
#define _PARISC_SWAB_H

#include <linux/types.h>
#include <linux/compiler.h>

#define __SWAB_64_THRU_32__

static inline __attribute_const__ __u16 __arch_swab16(__u16 x)
{
	__asm__("dep %0, 15, 8, %0\n\t"		
		"shd %%r0, %0, 8, %0"		
		: "=r" (x)
		: "0" (x));
	return x;
}
#define __arch_swab16 __arch_swab16

static inline __attribute_const__ __u32 __arch_swab24(__u32 x)
{
	__asm__("shd %0, %0, 8, %0\n\t"		
		"dep %0, 15, 8, %0\n\t"		
		"shd %%r0, %0, 8, %0"		
		: "=r" (x)
		: "0" (x));
	return x;
}

static inline __attribute_const__ __u32 __arch_swab32(__u32 x)
{
	unsigned int temp;
	__asm__("shd %0, %0, 16, %1\n\t"	
		"dep %1, 15, 8, %1\n\t"		
		"shd %0, %1, 8, %0"		
		: "=r" (x), "=&r" (temp)
		: "0" (x));
	return x;
}
#define __arch_swab32 __arch_swab32

#if BITS_PER_LONG > 32
static inline __attribute_const__ __u64 __arch_swab64(__u64 x)
{
	__u64 temp;
	__asm__("permh,3210 %0, %0\n\t"
		"hshl %0, 8, %1\n\t"
		"hshr,u %0, 8, %0\n\t"
		"or %1, %0, %0"
		: "=r" (x), "=&r" (temp)
		: "0" (x));
	return x;
}
#define __arch_swab64 __arch_swab64
#endif 

#endif 
