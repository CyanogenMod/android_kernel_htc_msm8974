/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1994 - 1997, 99, 2000, 06, 07  Ralf Baechle (ralf@linux-mips.org)
 * Copyright (c) 1999, 2000  Silicon Graphics, Inc.
 */
#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/types.h>
#include <asm/barrier.h>
#include <asm/bug.h>
#include <asm/byteorder.h>		
#include <asm/cpu-features.h>
#include <asm/sgidefs.h>
#include <asm/war.h>

#if _MIPS_SZLONG == 32
#define SZLONG_LOG 5
#define SZLONG_MASK 31UL
#define __LL		"ll	"
#define __SC		"sc	"
#define __INS		"ins    "
#define __EXT		"ext    "
#elif _MIPS_SZLONG == 64
#define SZLONG_LOG 6
#define SZLONG_MASK 63UL
#define __LL		"lld	"
#define __SC		"scd	"
#define __INS		"dins    "
#define __EXT		"dext    "
#endif

#define smp_mb__before_clear_bit()	smp_mb__before_llsc()
#define smp_mb__after_clear_bit()	smp_llsc_mb()

static inline void set_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long temp;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL "%0, %1			# set_bit	\n"
		"	or	%0, %2					\n"
		"	" __SC	"%0, %1					\n"
		"	beqzl	%0, 1b					\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "=m" (*m)
		: "ir" (1UL << bit), "m" (*m));
#ifdef CONFIG_CPU_MIPSR2
	} else if (kernel_uses_llsc && __builtin_constant_p(bit)) {
		do {
			__asm__ __volatile__(
			"	" __LL "%0, %1		# set_bit	\n"
			"	" __INS "%0, %3, %2, 1			\n"
			"	" __SC "%0, %1				\n"
			: "=&r" (temp), "+m" (*m)
			: "ir" (bit), "r" (~0));
		} while (unlikely(!temp));
#endif 
	} else if (kernel_uses_llsc) {
		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL "%0, %1		# set_bit	\n"
			"	or	%0, %2				\n"
			"	" __SC	"%0, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m)
			: "ir" (1UL << bit));
		} while (unlikely(!temp));
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		*a |= mask;
		raw_local_irq_restore(flags);
	}
}

static inline void clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long temp;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL "%0, %1			# clear_bit	\n"
		"	and	%0, %2					\n"
		"	" __SC "%0, %1					\n"
		"	beqzl	%0, 1b					\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "+m" (*m)
		: "ir" (~(1UL << bit)));
#ifdef CONFIG_CPU_MIPSR2
	} else if (kernel_uses_llsc && __builtin_constant_p(bit)) {
		do {
			__asm__ __volatile__(
			"	" __LL "%0, %1		# clear_bit	\n"
			"	" __INS "%0, $0, %2, 1			\n"
			"	" __SC "%0, %1				\n"
			: "=&r" (temp), "+m" (*m)
			: "ir" (bit));
		} while (unlikely(!temp));
#endif 
	} else if (kernel_uses_llsc) {
		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL "%0, %1		# clear_bit	\n"
			"	and	%0, %2				\n"
			"	" __SC "%0, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m)
			: "ir" (~(1UL << bit)));
		} while (unlikely(!temp));
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		*a &= ~mask;
		raw_local_irq_restore(flags);
	}
}

static inline void clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	smp_mb__before_clear_bit();
	clear_bit(nr, addr);
}

static inline void change_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned short bit = nr & SZLONG_MASK;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3				\n"
		"1:	" __LL "%0, %1		# change_bit	\n"
		"	xor	%0, %2				\n"
		"	" __SC	"%0, %1				\n"
		"	beqzl	%0, 1b				\n"
		"	.set	mips0				\n"
		: "=&r" (temp), "+m" (*m)
		: "ir" (1UL << bit));
	} else if (kernel_uses_llsc) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL "%0, %1		# change_bit	\n"
			"	xor	%0, %2				\n"
			"	" __SC	"%0, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m)
			: "ir" (1UL << bit));
		} while (unlikely(!temp));
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		*a ^= mask;
		raw_local_irq_restore(flags);
	}
}

static inline int test_and_set_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long res;

	smp_mb__before_llsc();

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL "%0, %1		# test_and_set_bit	\n"
		"	or	%2, %0, %3				\n"
		"	" __SC	"%2, %1					\n"
		"	beqzl	%2, 1b					\n"
		"	and	%2, %0, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "+m" (*m), "=&r" (res)
		: "r" (1UL << bit)
		: "memory");
	} else if (kernel_uses_llsc) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL "%0, %1	# test_and_set_bit	\n"
			"	or	%2, %0, %3			\n"
			"	" __SC	"%2, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m), "=&r" (res)
			: "r" (1UL << bit)
			: "memory");
		} while (unlikely(!res));

		res = temp & (1UL << bit);
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		res = (mask & *a);
		*a |= mask;
		raw_local_irq_restore(flags);
	}

	smp_llsc_mb();

	return res != 0;
}

static inline int test_and_set_bit_lock(unsigned long nr,
	volatile unsigned long *addr)
{
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long res;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL "%0, %1		# test_and_set_bit	\n"
		"	or	%2, %0, %3				\n"
		"	" __SC	"%2, %1					\n"
		"	beqzl	%2, 1b					\n"
		"	and	%2, %0, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "+m" (*m), "=&r" (res)
		: "r" (1UL << bit)
		: "memory");
	} else if (kernel_uses_llsc) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL "%0, %1	# test_and_set_bit	\n"
			"	or	%2, %0, %3			\n"
			"	" __SC	"%2, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m), "=&r" (res)
			: "r" (1UL << bit)
			: "memory");
		} while (unlikely(!res));

		res = temp & (1UL << bit);
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		res = (mask & *a);
		*a |= mask;
		raw_local_irq_restore(flags);
	}

	smp_llsc_mb();

	return res != 0;
}
static inline int test_and_clear_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long res;

	smp_mb__before_llsc();

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL	"%0, %1		# test_and_clear_bit	\n"
		"	or	%2, %0, %3				\n"
		"	xor	%2, %3					\n"
		"	" __SC 	"%2, %1					\n"
		"	beqzl	%2, 1b					\n"
		"	and	%2, %0, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "+m" (*m), "=&r" (res)
		: "r" (1UL << bit)
		: "memory");
#ifdef CONFIG_CPU_MIPSR2
	} else if (kernel_uses_llsc && __builtin_constant_p(nr)) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	" __LL	"%0, %1	# test_and_clear_bit	\n"
			"	" __EXT "%2, %0, %3, 1			\n"
			"	" __INS	"%0, $0, %3, 1			\n"
			"	" __SC 	"%0, %1				\n"
			: "=&r" (temp), "+m" (*m), "=&r" (res)
			: "ir" (bit)
			: "memory");
		} while (unlikely(!temp));
#endif
	} else if (kernel_uses_llsc) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL	"%0, %1	# test_and_clear_bit	\n"
			"	or	%2, %0, %3			\n"
			"	xor	%2, %3				\n"
			"	" __SC 	"%2, %1				\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m), "=&r" (res)
			: "r" (1UL << bit)
			: "memory");
		} while (unlikely(!res));

		res = temp & (1UL << bit);
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		res = (mask & *a);
		*a &= ~mask;
		raw_local_irq_restore(flags);
	}

	smp_llsc_mb();

	return res != 0;
}

static inline int test_and_change_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	unsigned short bit = nr & SZLONG_MASK;
	unsigned long res;

	smp_mb__before_llsc();

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:	" __LL	"%0, %1		# test_and_change_bit	\n"
		"	xor	%2, %0, %3				\n"
		"	" __SC	"%2, %1					\n"
		"	beqzl	%2, 1b					\n"
		"	and	%2, %0, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (temp), "+m" (*m), "=&r" (res)
		: "r" (1UL << bit)
		: "memory");
	} else if (kernel_uses_llsc) {
		unsigned long *m = ((unsigned long *) addr) + (nr >> SZLONG_LOG);
		unsigned long temp;

		do {
			__asm__ __volatile__(
			"	.set	mips3				\n"
			"	" __LL	"%0, %1	# test_and_change_bit	\n"
			"	xor	%2, %0, %3			\n"
			"	" __SC	"\t%2, %1			\n"
			"	.set	mips0				\n"
			: "=&r" (temp), "+m" (*m), "=&r" (res)
			: "r" (1UL << bit)
			: "memory");
		} while (unlikely(!res));

		res = temp & (1UL << bit);
	} else {
		volatile unsigned long *a = addr;
		unsigned long mask;
		unsigned long flags;

		a += nr >> SZLONG_LOG;
		mask = 1UL << bit;
		raw_local_irq_save(flags);
		res = (mask & *a);
		*a ^= mask;
		raw_local_irq_restore(flags);
	}

	smp_llsc_mb();

	return res != 0;
}

#include <asm-generic/bitops/non-atomic.h>

static inline void __clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	smp_mb();
	__clear_bit(nr, addr);
}

static inline unsigned long __fls(unsigned long word)
{
	int num;

	if (BITS_PER_LONG == 32 &&
	    __builtin_constant_p(cpu_has_clo_clz) && cpu_has_clo_clz) {
		__asm__(
		"	.set	push					\n"
		"	.set	mips32					\n"
		"	clz	%0, %1					\n"
		"	.set	pop					\n"
		: "=r" (num)
		: "r" (word));

		return 31 - num;
	}

	if (BITS_PER_LONG == 64 &&
	    __builtin_constant_p(cpu_has_mips64) && cpu_has_mips64) {
		__asm__(
		"	.set	push					\n"
		"	.set	mips64					\n"
		"	dclz	%0, %1					\n"
		"	.set	pop					\n"
		: "=r" (num)
		: "r" (word));

		return 63 - num;
	}

	num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
	if (!(word & (~0ul << 32))) {
		num -= 32;
		word <<= 32;
	}
#endif
	if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
		num -= 16;
		word <<= 16;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
		num -= 8;
		word <<= 8;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
		num -= 4;
		word <<= 4;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
		num -= 2;
		word <<= 2;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-1))))
		num -= 1;
	return num;
}

static inline unsigned long __ffs(unsigned long word)
{
	return __fls(word & -word);
}

static inline int fls(int x)
{
	int r;

	if (__builtin_constant_p(cpu_has_clo_clz) && cpu_has_clo_clz) {
		__asm__("clz %0, %1" : "=r" (x) : "r" (x));

		return 32 - x;
	}

	r = 32;
	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#include <asm-generic/bitops/fls64.h>

static inline int ffs(int word)
{
	if (!word)
		return 0;

	return fls(word & -word);
}

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/find.h>

#ifdef __KERNEL__

#include <asm-generic/bitops/sched.h>

#include <asm/arch_hweight.h>
#include <asm-generic/bitops/const_hweight.h>

#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif 

#endif 
