/*
 * linux/arch/unicore32/include/asm/delay.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */
#ifndef __UNICORE_DELAY_H__
#define __UNICORE_DELAY_H__

#include <asm/param.h>	

extern void __delay(int loops);

extern void __bad_udelay(void);

extern void __udelay(unsigned long usecs);
extern void __const_udelay(unsigned long);

#define MAX_UDELAY_MS 2

#define udelay(n)							\
	(__builtin_constant_p(n) ?					\
	  ((n) > (MAX_UDELAY_MS * 1000) ? __bad_udelay() :		\
			__const_udelay((n) * ((2199023U*HZ)>>11))) :	\
	  __udelay(n))

#endif 

