#ifndef _ASM_IA64_BITOPS_H
#define _ASM_IA64_BITOPS_H

/*
 * Copyright (C) 1998-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 02/06/02 find_next_bit() and find_first_bit() added from Erich Focht's ia64
 * O(1) scheduler patch
 */

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/intrinsics.h>

static __inline__ void
set_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = 1 << (nr & 31);
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old | bit;
	} while (cmpxchg_acq(m, old, new) != old);
}

static __inline__ void
__set_bit (int nr, volatile void *addr)
{
	*((__u32 *) addr + (nr >> 5)) |= (1 << (nr & 31));
}

#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	do { ; } while (0)

static __inline__ void
clear_bit (int nr, volatile void *addr)
{
	__u32 mask, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	mask = ~(1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old & mask;
	} while (cmpxchg_acq(m, old, new) != old);
}

static __inline__ void
clear_bit_unlock (int nr, volatile void *addr)
{
	__u32 mask, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	mask = ~(1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old & mask;
	} while (cmpxchg_rel(m, old, new) != old);
}

static __inline__ void
__clear_bit_unlock(int nr, void *addr)
{
	__u32 * const m = (__u32 *) addr + (nr >> 5);
	__u32 const new = *m & ~(1 << (nr & 31));

	ia64_st4_rel_nta(m, new);
}

static __inline__ void
__clear_bit (int nr, volatile void *addr)
{
	*((__u32 *) addr + (nr >> 5)) &= ~(1 << (nr & 31));
}

static __inline__ void
change_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = (1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old ^ bit;
	} while (cmpxchg_acq(m, old, new) != old);
}

static __inline__ void
__change_bit (int nr, volatile void *addr)
{
	*((__u32 *) addr + (nr >> 5)) ^= (1 << (nr & 31));
}

static __inline__ int
test_and_set_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = 1 << (nr & 31);
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old | bit;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & bit) != 0;
}

#define test_and_set_bit_lock test_and_set_bit

static __inline__ int
__test_and_set_bit (int nr, volatile void *addr)
{
	__u32 *p = (__u32 *) addr + (nr >> 5);
	__u32 m = 1 << (nr & 31);
	int oldbitset = (*p & m) != 0;

	*p |= m;
	return oldbitset;
}

static __inline__ int
test_and_clear_bit (int nr, volatile void *addr)
{
	__u32 mask, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	mask = ~(1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old & mask;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & ~mask) != 0;
}

static __inline__ int
__test_and_clear_bit(int nr, volatile void * addr)
{
	__u32 *p = (__u32 *) addr + (nr >> 5);
	__u32 m = 1 << (nr & 31);
	int oldbitset = (*p & m) != 0;

	*p &= ~m;
	return oldbitset;
}

static __inline__ int
test_and_change_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = (1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old ^ bit;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & bit) != 0;
}

static __inline__ int
__test_and_change_bit (int nr, void *addr)
{
	__u32 old, bit = (1 << (nr & 31));
	__u32 *m = (__u32 *) addr + (nr >> 5);

	old = *m;
	*m = old ^ bit;
	return (old & bit) != 0;
}

static __inline__ int
test_bit (int nr, const volatile void *addr)
{
	return 1 & (((const volatile __u32 *) addr)[nr >> 5] >> (nr & 31));
}

static inline unsigned long
ffz (unsigned long x)
{
	unsigned long result;

	result = ia64_popcnt(x & (~x - 1));
	return result;
}

static __inline__ unsigned long
__ffs (unsigned long x)
{
	unsigned long result;

	result = ia64_popcnt((x-1) & ~x);
	return result;
}

#ifdef __KERNEL__

static inline unsigned long
ia64_fls (unsigned long x)
{
	long double d = x;
	long exp;

	exp = ia64_getf_exp(d);
	return exp - 0xffff;
}

static inline int
fls (int t)
{
	unsigned long x = t & 0xffffffffu;

	if (!x)
		return 0;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return ia64_popcnt(x);
}

static inline unsigned long
__fls (unsigned long x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return ia64_popcnt(x) - 1;
}

#include <asm-generic/bitops/fls64.h>

#define ffs(x)	__builtin_ffs(x)

static __inline__ unsigned long __arch_hweight64(unsigned long x)
{
	unsigned long result;
	result = ia64_popcnt(x);
	return result;
}

#define __arch_hweight32(x) ((unsigned int) __arch_hweight64((x) & 0xfffffffful))
#define __arch_hweight16(x) ((unsigned int) __arch_hweight64((x) & 0xfffful))
#define __arch_hweight8(x)  ((unsigned int) __arch_hweight64((x) & 0xfful))

#include <asm-generic/bitops/const_hweight.h>

#endif 

#include <asm-generic/bitops/find.h>

#ifdef __KERNEL__

#include <asm-generic/bitops/le.h>

#include <asm-generic/bitops/ext2-atomic-setbit.h>

#include <asm-generic/bitops/sched.h>

#endif 

#endif 
