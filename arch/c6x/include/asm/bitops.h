/*
 *  Port on Texas Instruments TMS320C6x architecture
 *
 *  Copyright (C) 2004, 2009, 2010 Texas Instruments Incorporated
 *  Author: Aurelien Jacquiot (aurelien.jacquiot@jaluna.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef _ASM_C6X_BITOPS_H
#define _ASM_C6X_BITOPS_H

#ifdef __KERNEL__

#include <linux/bitops.h>

#include <asm/byteorder.h>

#define smp_mb__before_clear_bit() barrier()
#define smp_mb__after_clear_bit()  barrier()


static inline unsigned long __ffs(unsigned long x)
{
	asm (" bitr  .M1  %0,%0\n"
	     " nop\n"
	     " lmbd  .L1  1,%0,%0\n"
	     : "+a"(x));

	return x;
}

#define ffz(x) __ffs(~(x))

static inline int fls(int x)
{
	if (!x)
		return 0;

	asm (" lmbd  .L1  1,%0,%0\n" : "+a"(x));

	return 32 - x;
}

static inline int ffs(int x)
{
	if (!x)
		return 0;

	return __ffs(x) + 1;
}

#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/find.h>

#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

#include <asm-generic/bitops/atomic.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif 
#endif 
