/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_BITOPS_32_H
#define _ASM_TILE_BITOPS_32_H

#include <linux/compiler.h>
#include <linux/atomic.h>

unsigned long _atomic_or(volatile unsigned long *p, unsigned long mask);
unsigned long _atomic_andn(volatile unsigned long *p, unsigned long mask);
unsigned long _atomic_xor(volatile unsigned long *p, unsigned long mask);

static inline void set_bit(unsigned nr, volatile unsigned long *addr)
{
	_atomic_or(addr + BIT_WORD(nr), BIT_MASK(nr));
}

static inline void clear_bit(unsigned nr, volatile unsigned long *addr)
{
	_atomic_andn(addr + BIT_WORD(nr), BIT_MASK(nr));
}

static inline void change_bit(unsigned nr, volatile unsigned long *addr)
{
	_atomic_xor(addr + BIT_WORD(nr), BIT_MASK(nr));
}

static inline int test_and_set_bit(unsigned nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	addr += BIT_WORD(nr);
	smp_mb();  
	return (_atomic_or(addr, mask) & mask) != 0;
}

static inline int test_and_clear_bit(unsigned nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	addr += BIT_WORD(nr);
	smp_mb();  
	return (_atomic_andn(addr, mask) & mask) != 0;
}

static inline int test_and_change_bit(unsigned nr,
				      volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	addr += BIT_WORD(nr);
	smp_mb();  
	return (_atomic_xor(addr, mask) & mask) != 0;
}

#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	do {} while (0)

#include <asm-generic/bitops/ext2-atomic.h>

#endif 
