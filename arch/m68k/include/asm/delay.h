#ifndef _M68K_DELAY_H
#define _M68K_DELAY_H

#include <asm/param.h>

/*
 * Copyright (C) 1994 Hamish Macdonald
 * Copyright (C) 2004 Greg Ungerer <gerg@uclinux.com>
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */

#if defined(CONFIG_COLDFIRE)
#define	DELAY_ALIGN	".balignw 4, 0x4a8e\n\t"
#else
#define	DELAY_ALIGN
#endif

static inline void __delay(unsigned long loops)
{
	__asm__ __volatile__ (
		DELAY_ALIGN
		"1: subql #1,%0\n\t"
		"jcc 1b"
		: "=d" (loops)
		: "0" (loops));
}

extern void __bad_udelay(void);


#if defined(CONFIG_M68000) || defined(CONFIG_COLDFIRE)
#define	HZSCALE		(268435456 / (1000000 / HZ))

#define	__const_udelay(u) \
	__delay(((((u) * HZSCALE) >> 11) * (loops_per_jiffy >> 11)) >> 6)

#else

static inline void __xdelay(unsigned long xloops)
{
	unsigned long tmp;

	__asm__ ("mulul %2,%0:%1"
		: "=d" (xloops), "=d" (tmp)
		: "d" (xloops), "1" (loops_per_jiffy));
	__delay(xloops * HZ);
}

#define	__const_udelay(n)	(__xdelay((n) * 4295))

#endif

static inline void __udelay(unsigned long usecs)
{
	__const_udelay(usecs);
}

#define udelay(n) (__builtin_constant_p(n) ? \
	((n) > 20000 ? __bad_udelay() : __const_udelay(n)) : __udelay(n))


#endif 
