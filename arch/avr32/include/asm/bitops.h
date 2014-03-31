/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_BITOPS_H
#define __ASM_AVR32_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <asm/byteorder.h>

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

static inline void set_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long tmp;

	if (__builtin_constant_p(nr)) {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %2\n"
			"	sbr	%0, %3\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p)
			: "m"(*p), "i"(nr)
			: "cc");
	} else {
		unsigned long mask = 1UL << (nr % BITS_PER_LONG);
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %2\n"
			"	or	%0, %3\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p)
			: "m"(*p), "r"(mask)
			: "cc");
	}
}

static inline void clear_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long tmp;

	if (__builtin_constant_p(nr)) {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %2\n"
			"	cbr	%0, %3\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p)
			: "m"(*p), "i"(nr)
			: "cc");
	} else {
		unsigned long mask = 1UL << (nr % BITS_PER_LONG);
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %2\n"
			"	andn	%0, %3\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p)
			: "m"(*p), "r"(mask)
			: "cc");
	}
}

static inline void change_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long mask = 1UL << (nr % BITS_PER_LONG);
	unsigned long tmp;

	asm volatile(
		"1:	ssrf	5\n"
		"	ld.w	%0, %2\n"
		"	eor	%0, %3\n"
		"	stcond	%1, %0\n"
		"	brne	1b"
		: "=&r"(tmp), "=o"(*p)
		: "m"(*p), "r"(mask)
		: "cc");
}

static inline int test_and_set_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long mask = 1UL << (nr % BITS_PER_LONG);
	unsigned long tmp, old;

	if (__builtin_constant_p(nr)) {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %3\n"
			"	mov	%2, %0\n"
			"	sbr	%0, %4\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p), "=&r"(old)
			: "m"(*p), "i"(nr)
			: "memory", "cc");
	} else {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%2, %3\n"
			"	or	%0, %2, %4\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p), "=&r"(old)
			: "m"(*p), "r"(mask)
			: "memory", "cc");
	}

	return (old & mask) != 0;
}

static inline int test_and_clear_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long mask = 1UL << (nr % BITS_PER_LONG);
	unsigned long tmp, old;

	if (__builtin_constant_p(nr)) {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %3\n"
			"	mov	%2, %0\n"
			"	cbr	%0, %4\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p), "=&r"(old)
			: "m"(*p), "i"(nr)
			: "memory", "cc");
	} else {
		asm volatile(
			"1:	ssrf	5\n"
			"	ld.w	%0, %3\n"
			"	mov	%2, %0\n"
			"	andn	%0, %4\n"
			"	stcond	%1, %0\n"
			"	brne	1b"
			: "=&r"(tmp), "=o"(*p), "=&r"(old)
			: "m"(*p), "r"(mask)
			: "memory", "cc");
	}

	return (old & mask) != 0;
}

static inline int test_and_change_bit(int nr, volatile void * addr)
{
	unsigned long *p = ((unsigned long *)addr) + nr / BITS_PER_LONG;
	unsigned long mask = 1UL << (nr % BITS_PER_LONG);
	unsigned long tmp, old;

	asm volatile(
		"1:	ssrf	5\n"
		"	ld.w	%2, %3\n"
		"	eor	%0, %2, %4\n"
		"	stcond	%1, %0\n"
		"	brne	1b"
		: "=&r"(tmp), "=o"(*p), "=&r"(old)
		: "m"(*p), "r"(mask)
		: "memory", "cc");

	return (old & mask) != 0;
}

#include <asm-generic/bitops/non-atomic.h>

static inline unsigned long __ffs(unsigned long word)
{
	unsigned long result;

	asm("brev %1\n\t"
	    "clz %0,%1"
	    : "=r"(result), "=&r"(word)
	    : "1"(word));
	return result;
}

static inline unsigned long ffz(unsigned long word)
{
	return __ffs(~word);
}

static inline int fls(unsigned long word)
{
	unsigned long result;

	asm("clz %0,%1" : "=r"(result) : "r"(word));
	return 32 - result;
}

static inline int __fls(unsigned long word)
{
	return fls(word) - 1;
}

unsigned long find_first_zero_bit(const unsigned long *addr,
				  unsigned long size);
#define find_first_zero_bit find_first_zero_bit

unsigned long find_next_zero_bit(const unsigned long *addr,
				 unsigned long size,
				 unsigned long offset);
#define find_next_zero_bit find_next_zero_bit

unsigned long find_first_bit(const unsigned long *addr,
			     unsigned long size);
#define find_first_bit find_first_bit

unsigned long find_next_bit(const unsigned long *addr,
				 unsigned long size,
				 unsigned long offset);
#define find_next_bit find_next_bit

static inline int ffs(unsigned long word)
{
	if(word == 0)
		return 0;
	return __ffs(word) + 1;
}

#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

extern unsigned long find_next_zero_bit_le(const void *addr,
		unsigned long size, unsigned long offset);
#define find_next_zero_bit_le find_next_zero_bit_le

extern unsigned long find_next_bit_le(const void *addr,
		unsigned long size, unsigned long offset);
#define find_next_bit_le find_next_bit_le

#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif 
