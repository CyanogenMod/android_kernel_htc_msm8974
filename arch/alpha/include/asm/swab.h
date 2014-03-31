#ifndef _ALPHA_SWAB_H
#define _ALPHA_SWAB_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <asm/compiler.h>

#ifdef __GNUC__

static inline __attribute_const__ __u32 __arch_swab32(__u32 x)
{

	__u64 t0, t1, t2, t3;

	t0 = __kernel_inslh(x, 7);	
	t1 = __kernel_inswl(x, 3);	
	t1 |= t0;			
	t2 = t1 >> 16;			
	t0 = t1 & 0xFF00FF00;		
	t3 = t2 & 0x00FF00FF;		
	t1 = t0 + t3;			

	return t1;
}
#define __arch_swab32 __arch_swab32

#endif 

#endif 
