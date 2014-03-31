/* 32-bit atomic xchg() and cmpxchg() definitions.
 *
 * Copyright (C) 1996 David S. Miller (davem@davemloft.net)
 * Copyright (C) 2000 Anton Blanchard (anton@linuxcare.com.au)
 * Copyright (C) 2007 Kyle McMartin (kyle@parisc-linux.org)
 *
 * Additions by Keith M Wesolowski (wesolows@foobazco.org) based
 * on asm-parisc/atomic.h Copyright (C) 2000 Philipp Rumpf <prumpf@tux.org>.
 */

#ifndef __ARCH_SPARC_CMPXCHG__
#define __ARCH_SPARC_CMPXCHG__

#include <asm/btfixup.h>

#ifndef CONFIG_SMP
BTFIXUPDEF_CALL(void, ___xchg32, void)
#endif

static inline unsigned long xchg_u32(__volatile__ unsigned long *m, unsigned long val)
{
#ifdef CONFIG_SMP
	__asm__ __volatile__("swap [%2], %0"
			     : "=&r" (val)
			     : "0" (val), "r" (m)
			     : "memory");
	return val;
#else
	register unsigned long *ptr asm("g1");
	register unsigned long ret asm("g2");

	ptr = (unsigned long *) m;
	ret = val;

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___f____xchg32\n\t"
	" nop\n\t"
	: "=&r" (ret)
	: "0" (ret), "r" (ptr)
	: "g3", "g4", "g7", "memory", "cc");

	return ret;
#endif
}

extern void __xchg_called_with_bad_pointer(void);

static inline unsigned long __xchg(unsigned long x, __volatile__ void * ptr, int size)
{
	switch (size) {
	case 4:
		return xchg_u32(ptr, x);
	}
	__xchg_called_with_bad_pointer();
	return x;
}

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#define __HAVE_ARCH_CMPXCHG	1

extern void __cmpxchg_called_with_bad_pointer(void);
extern unsigned long __cmpxchg_u32(volatile u32 *m, u32 old, u32 new_);

static inline unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new_, int size)
{
	switch (size) {
	case 4:
		return __cmpxchg_u32((u32 *)ptr, (u32)old, (u32)new_);
	default:
		__cmpxchg_called_with_bad_pointer();
		break;
	}
	return old;
}

#define cmpxchg(ptr, o, n)						\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,	\
			(unsigned long)_n_, sizeof(*(ptr)));		\
})

#include <asm-generic/cmpxchg-local.h>

#define cmpxchg_local(ptr, o, n)				  	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#endif 
