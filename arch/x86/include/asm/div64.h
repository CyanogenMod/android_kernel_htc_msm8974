#ifndef _ASM_X86_DIV64_H
#define _ASM_X86_DIV64_H

#ifdef CONFIG_X86_32

#include <linux/types.h>
#include <linux/log2.h>

#define do_div(n, base)						\
({								\
	unsigned long __upper, __low, __high, __mod, __base;	\
	__base = (base);					\
	if (__builtin_constant_p(__base) && is_power_of_2(__base)) { \
		__mod = n & (__base - 1);			\
		n >>= ilog2(__base);				\
	} else {						\
		asm("" : "=a" (__low), "=d" (__high) : "A" (n));\
		__upper = __high;				\
		if (__high) {					\
			__upper = __high % (__base);		\
			__high = __high / (__base);		\
		}						\
		asm("divl %2" : "=a" (__low), "=d" (__mod)	\
			: "rm" (__base), "0" (__low), "1" (__upper));	\
		asm("" : "=A" (n) : "a" (__low), "d" (__high));	\
	}							\
	__mod;							\
})

static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
	union {
		u64 v64;
		u32 v32[2];
	} d = { dividend };
	u32 upper;

	upper = d.v32[1];
	d.v32[1] = 0;
	if (upper >= divisor) {
		d.v32[1] = upper / divisor;
		upper %= divisor;
	}
	asm ("divl %2" : "=a" (d.v32[0]), "=d" (*remainder) :
		"rm" (divisor), "0" (d.v32[0]), "1" (upper));
	return d.v64;
}
#define div_u64_rem	div_u64_rem

#else
# include <asm-generic/div64.h>
#endif 

#endif 
