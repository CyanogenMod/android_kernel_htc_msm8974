/*
 * include/asm-microblaze/delay.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Michal Simek
 * Copyright (C) 2007 John Williams
 * Copyright (C) 2006 Atmark Techno, Inc.
 */

#ifndef _ASM_MICROBLAZE_DELAY_H
#define _ASM_MICROBLAZE_DELAY_H

extern inline void __delay(unsigned long loops)
{
	asm volatile ("# __delay		\n\t"		\
			"1: addi	%0, %0, -1\t\n"		\
			"bneid	%0, 1b		\t\n"		\
			"nop			\t\n"
			: "=r" (loops)
			: "0" (loops));
}

#define __MAX_UDELAY	(226050910UL/HZ)	
#define __MAX_NDELAY	(4294967295UL/HZ)	

extern unsigned long loops_per_jiffy;

extern inline void __udelay(unsigned int x)
{

	unsigned long long tmp =
		(unsigned long long)x * (unsigned long long)loops_per_jiffy \
			* 226LL;
	unsigned loops = tmp >> 32;

	__delay(loops);
}

extern void __bad_udelay(void);		
extern void __bad_ndelay(void);		

#define udelay(n) (__builtin_constant_p(n) ? \
	((n) > __MAX_UDELAY ? __bad_udelay() : __udelay((n) * (19 * HZ))) : \
	__udelay((n) * (19 * HZ)))

#define ndelay(n) (__builtin_constant_p(n) ? \
	((n) > __MAX_NDELAY ? __bad_ndelay() : __udelay((n) * HZ)) : \
	__udelay((n) * HZ))

#define muldiv(a, b, c)		(((a)*(b))/(c))

#endif 
