/*
 * Copyright 1992, Linus Torvalds.
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

#ifndef _ASM_TILE_BITOPS_H
#define _ASM_TILE_BITOPS_H

#include <linux/types.h>

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#ifdef __tilegx__
#include <asm/bitops_64.h>
#else
#include <asm/bitops_32.h>
#endif

static inline unsigned long __ffs(unsigned long word)
{
	return __builtin_ctzl(word);
}

static inline unsigned long ffz(unsigned long word)
{
	return __builtin_ctzl(~word);
}

static inline unsigned long __fls(unsigned long word)
{
	return (sizeof(word) * 8) - 1 - __builtin_clzl(word);
}

static inline int ffs(int x)
{
	return __builtin_ffs(x);
}

static inline int fls(int x)
{
	return (sizeof(int) * 8) - __builtin_clz(x);
}

static inline int fls64(__u64 w)
{
	return (sizeof(__u64) * 8) - __builtin_clzll(w);
}

static inline unsigned int __arch_hweight32(unsigned int w)
{
	return __builtin_popcount(w);
}

static inline unsigned int __arch_hweight16(unsigned int w)
{
	return __builtin_popcount(w & 0xffff);
}

static inline unsigned int __arch_hweight8(unsigned int w)
{
	return __builtin_popcount(w & 0xff);
}

static inline unsigned long __arch_hweight64(__u64 w)
{
	return __builtin_popcountll(w);
}

#include <asm-generic/bitops/const_hweight.h>
#include <asm-generic/bitops/lock.h>
#include <asm-generic/bitops/find.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>

#endif 
