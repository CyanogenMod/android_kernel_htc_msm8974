#ifndef _ASM_M32R_BITOPS_H
#define _ASM_M32R_BITOPS_H

/*
 *  linux/include/asm-m32r/bitops.h
 *
 *  Copyright 1992, Linus Torvalds.
 *
 *  M32R version:
 *    Copyright (C) 2001, 2002  Hitoshi Yamamoto
 *    Copyright (C) 2004  Hirokazu Takata <takata at linux-m32r.org>
 */

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <asm/assembler.h>
#include <asm/byteorder.h>
#include <asm/dcache_clear.h>
#include <asm/types.h>


static __inline__ void set_bit(int nr, volatile void * addr)
{
	__u32 mask;
	volatile __u32 *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);
	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "r6", "%1")
		M32R_LOCK" %0, @%1;		\n\t"
		"or	%0, %2;			\n\t"
		M32R_UNLOCK" %0, @%1;		\n\t"
		: "=&r" (tmp)
		: "r" (a), "r" (mask)
		: "memory"
#ifdef CONFIG_CHIP_M32700_TS1
		, "r6"
#endif	
	);
	local_irq_restore(flags);
}

static __inline__ void clear_bit(int nr, volatile void * addr)
{
	__u32 mask;
	volatile __u32 *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);

	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "r6", "%1")
		M32R_LOCK" %0, @%1;		\n\t"
		"and	%0, %2;			\n\t"
		M32R_UNLOCK" %0, @%1;		\n\t"
		: "=&r" (tmp)
		: "r" (a), "r" (~mask)
		: "memory"
#ifdef CONFIG_CHIP_M32700_TS1
		, "r6"
#endif	
	);
	local_irq_restore(flags);
}

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

static __inline__ void change_bit(int nr, volatile void * addr)
{
	__u32  mask;
	volatile __u32  *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);
	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "r6", "%1")
		M32R_LOCK" %0, @%1;		\n\t"
		"xor	%0, %2;			\n\t"
		M32R_UNLOCK" %0, @%1;		\n\t"
		: "=&r" (tmp)
		: "r" (a), "r" (mask)
		: "memory"
#ifdef CONFIG_CHIP_M32700_TS1
		, "r6"
#endif	
	);
	local_irq_restore(flags);
}

static __inline__ int test_and_set_bit(int nr, volatile void * addr)
{
	__u32 mask, oldbit;
	volatile __u32 *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);
	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "%1", "%2")
		M32R_LOCK" %0, @%2;		\n\t"
		"mv	%1, %0;			\n\t"
		"and	%0, %3;			\n\t"
		"or	%1, %3;			\n\t"
		M32R_UNLOCK" %1, @%2;		\n\t"
		: "=&r" (oldbit), "=&r" (tmp)
		: "r" (a), "r" (mask)
		: "memory"
	);
	local_irq_restore(flags);

	return (oldbit != 0);
}

static __inline__ int test_and_clear_bit(int nr, volatile void * addr)
{
	__u32 mask, oldbit;
	volatile __u32 *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);

	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "%1", "%3")
		M32R_LOCK" %0, @%3;		\n\t"
		"mv	%1, %0;			\n\t"
		"and	%0, %2;			\n\t"
		"not	%2, %2;			\n\t"
		"and	%1, %2;			\n\t"
		M32R_UNLOCK" %1, @%3;		\n\t"
		: "=&r" (oldbit), "=&r" (tmp), "+r" (mask)
		: "r" (a)
		: "memory"
	);
	local_irq_restore(flags);

	return (oldbit != 0);
}

static __inline__ int test_and_change_bit(int nr, volatile void * addr)
{
	__u32 mask, oldbit;
	volatile __u32 *a = addr;
	unsigned long flags;
	unsigned long tmp;

	a += (nr >> 5);
	mask = (1 << (nr & 0x1F));

	local_irq_save(flags);
	__asm__ __volatile__ (
		DCACHE_CLEAR("%0", "%1", "%2")
		M32R_LOCK" %0, @%2;		\n\t"
		"mv	%1, %0;			\n\t"
		"and	%0, %3;			\n\t"
		"xor	%1, %3;			\n\t"
		M32R_UNLOCK" %1, @%2;		\n\t"
		: "=&r" (oldbit), "=&r" (tmp)
		: "r" (a), "r" (mask)
		: "memory"
	);
	local_irq_restore(flags);

	return (oldbit != 0);
}

#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>

#ifdef __KERNEL__

#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/find.h>
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

#endif 

#ifdef __KERNEL__

#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif 

#endif 
