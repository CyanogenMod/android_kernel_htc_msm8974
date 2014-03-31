/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001  Hiroyuki Kondo, Hirokazu Takata, and Hitoshi Yamamoto
 * Copyright (C) 2004, 2006  Hirokazu Takata <takata at linux-m32r.org>
 */
#ifndef _ASM_M32R_BARRIER_H
#define _ASM_M32R_BARRIER_H

#define nop()  __asm__ __volatile__ ("nop" : : )

#define mb()   barrier()
#define rmb()  mb()
#define wmb()  mb()


#define read_barrier_depends()	do { } while (0)

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define smp_read_barrier_depends()	read_barrier_depends()
#define set_mb(var, value) do { (void) xchg(&var, value); } while (0)
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while (0)
#define set_mb(var, value) do { var = value; barrier(); } while (0)
#endif

#endif 
