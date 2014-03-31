/* MN10300 bit operations
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */
#ifndef __ASM_BITOPS_H
#define __ASM_BITOPS_H

#include <asm/cpu-regs.h>

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

#define __set_bit(nr, addr)					\
({								\
	volatile unsigned char *_a = (unsigned char *)(addr);	\
	const unsigned shift = (nr) & 7;			\
	_a += (nr) >> 3;					\
								\
	asm volatile("bset %2,(%1) # set_bit reg"		\
		     : "=m"(*_a)				\
		     : "a"(_a), "d"(1 << shift),  "m"(*_a)	\
		     : "memory", "cc");				\
})

#define set_bit(nr, addr) __set_bit((nr), (addr))

#define ___clear_bit(nr, addr)					\
({								\
	volatile unsigned char *_a = (unsigned char *)(addr);	\
	const unsigned shift = (nr) & 7;			\
	_a += (nr) >> 3;					\
								\
	asm volatile("bclr %2,(%1) # clear_bit reg"		\
		     : "=m"(*_a)				\
		     : "a"(_a), "d"(1 << shift), "m"(*_a)	\
		     : "memory", "cc");				\
})

#define clear_bit(nr, addr) ___clear_bit((nr), (addr))


static inline void __clear_bit(unsigned long nr, volatile void *addr)
{
	unsigned int *a = (unsigned int *) addr;
	int mask;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	*a &= ~mask;
}

static inline int test_bit(unsigned long nr, const volatile void *addr)
{
	return 1UL & (((const volatile unsigned int *) addr)[nr >> 5] >> (nr & 31));
}

static inline void __change_bit(unsigned long nr, volatile void *addr)
{
	int	mask;
	unsigned int *a = (unsigned int *) addr;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	*a ^= mask;
}

extern void change_bit(unsigned long nr, volatile void *addr);

#define __test_and_set_bit(nr,addr)				\
({								\
	volatile unsigned char *_a = (unsigned char *)(addr);	\
	const unsigned shift = (nr) & 7;			\
	unsigned epsw;						\
	_a += (nr) >> 3;					\
								\
	asm volatile("bset %3,(%2) # test_set_bit reg\n"	\
		     "mov epsw,%1"				\
		     : "=m"(*_a), "=d"(epsw)			\
		     : "a"(_a), "d"(1 << shift), "m"(*_a)	\
		     : "memory", "cc");				\
								\
	!(epsw & EPSW_FLAG_Z);					\
})

#define test_and_set_bit(nr, addr) __test_and_set_bit((nr), (addr))

#define __test_and_clear_bit(nr, addr)				\
({								\
        volatile unsigned char *_a = (unsigned char *)(addr);	\
	const unsigned shift = (nr) & 7;			\
	unsigned epsw;						\
	_a += (nr) >> 3;					\
								\
	asm volatile("bclr %3,(%2) # test_clear_bit reg\n"	\
		     "mov epsw,%1"				\
		     : "=m"(*_a), "=d"(epsw)			\
		     : "a"(_a), "d"(1 << shift), "m"(*_a)	\
		     : "memory", "cc");				\
								\
	!(epsw & EPSW_FLAG_Z);					\
})

#define test_and_clear_bit(nr, addr) __test_and_clear_bit((nr), (addr))

static inline int __test_and_change_bit(unsigned long nr, volatile void *addr)
{
	int	mask, retval;
	unsigned int *a = (unsigned int *)addr;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a ^= mask;

	return retval;
}

extern int test_and_change_bit(unsigned long nr, volatile void *addr);

#include <asm-generic/bitops/lock.h>

#ifdef __KERNEL__

static inline __attribute__((const))
unsigned long __ffs(unsigned long x)
{
	int bit;
	asm("bsch %2,%0" : "=r"(bit) : "0"(0), "r"(x & -x) : "cc");
	return bit;
}

static inline __attribute__((const))
int __ilog2_u32(u32 n)
{
	int bit;
	asm("bsch %2,%0" : "=r"(bit) : "0"(0), "r"(n) : "cc");
	return bit;
}

static inline __attribute__((const))
int fls(int x)
{
	return (x != 0) ? __ilog2_u32(x) + 1 : 0;
}

static inline unsigned long __fls(unsigned long word)
{
	return __ilog2_u32(word);
}

static inline __attribute__((const))
int ffs(int x)
{
	return fls(x & -x);
}

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/find.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/ext2-atomic-setbit.h>
#include <asm-generic/bitops/le.h>

#endif 
#endif 
