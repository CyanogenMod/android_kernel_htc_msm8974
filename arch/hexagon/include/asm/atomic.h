/*
 * Atomic operations for the Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_ATOMIC_H
#define _ASM_ATOMIC_H

#include <linux/types.h>
#include <asm/cmpxchg.h>

#define ATOMIC_INIT(i)		{ (i) }
#define atomic_set(v, i)	((v)->counter = (i))

#define atomic_read(v)		((v)->counter)

#define atomic_xchg(v, new)	(xchg(&((v)->counter), (new)))


static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	int __oldval;

	asm volatile(
		"1:	%0 = memw_locked(%1);\n"
		"	{ P0 = cmp.eq(%0,%2);\n"
		"	  if (!P0.new) jump:nt 2f; }\n"
		"	memw_locked(%1,P0) = %3;\n"
		"	if (!P0) jump 1b;\n"
		"2:\n"
		: "=&r" (__oldval)
		: "r" (&v->counter), "r" (old), "r" (new)
		: "memory", "p0"
	);

	return __oldval;
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	int output;

	__asm__ __volatile__ (
		"1:	%0 = memw_locked(%1);\n"
		"	%0 = add(%0,%2);\n"
		"	memw_locked(%1,P3)=%0;\n"
		"	if !P3 jump 1b;\n"
		: "=&r" (output)
		: "r" (&v->counter), "r" (i)
		: "memory", "p3"
	);
	return output;

}

#define atomic_add(i, v) atomic_add_return(i, (v))

static inline int atomic_sub_return(int i, atomic_t *v)
{
	int output;
	__asm__ __volatile__ (
		"1:	%0 = memw_locked(%1);\n"
		"	%0 = sub(%0,%2);\n"
		"	memw_locked(%1,P3)=%0\n"
		"	if !P3 jump 1b;\n"
		: "=&r" (output)
		: "r" (&v->counter), "r" (i)
		: "memory", "p3"
	);
	return output;
}

#define atomic_sub(i, v) atomic_sub_return(i, (v))

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int output, __oldval;
	asm volatile(
		"1:	%0 = memw_locked(%2);"
		"	{"
		"		p3 = cmp.eq(%0, %4);"
		"		if (p3.new) jump:nt 2f;"
		"		%0 = add(%0, %3);"
		"		%1 = #0;"
		"	}"
		"	memw_locked(%2, p3) = %0;"
		"	{"
		"		if !p3 jump 1b;"
		"		%1 = #1;"
		"	}"
		"2:"
		: "=&r" (__oldval), "=&r" (output)
		: "r" (v), "r" (a), "r" (u)
		: "memory", "p3"
	);
	return output;
}

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

#define atomic_inc(v) atomic_add(1, (v))
#define atomic_dec(v) atomic_sub(1, (v))

#define atomic_inc_and_test(v) (atomic_add_return(1, (v)) == 0)
#define atomic_dec_and_test(v) (atomic_sub_return(1, (v)) == 0)
#define atomic_sub_and_test(i, v) (atomic_sub_return(i, (v)) == 0)
#define atomic_add_negative(i, v) (atomic_add_return(i, (v)) < 0)


#define atomic_inc_return(v) (atomic_add_return(1, v))
#define atomic_dec_return(v) (atomic_sub_return(1, v))

#endif
