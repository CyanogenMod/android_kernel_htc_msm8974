/*
 *  arch/s390/lib/div64.c
 *
 *  __div64_32 implementation for 31 bit.
 *
 *    Copyright (C) IBM Corp. 2006
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 */

#include <linux/types.h>
#include <linux/module.h>

#ifdef CONFIG_MARCH_G5

static uint32_t __div64_31(uint64_t *n, uint32_t base)
{
	register uint32_t reg2 asm("2");
	register uint32_t reg3 asm("3");
	uint32_t *words = (uint32_t *) n;
	uint32_t tmp;

	
	if (base == 1)
		return 0;
	reg2 = 0UL;
	reg3 = words[0];
	asm volatile(
		"	dr	%0,%2\n"
		: "+d" (reg2), "+d" (reg3) : "d" (base) : "cc" );
	words[0] = reg3;
	reg3 = words[1];
	asm volatile(
		"	nr	%2,%1\n"
		"	srdl	%0,1\n"
		"	dr	%0,%3\n"
		"	alr	%0,%0\n"
		"	alr	%1,%1\n"
		"	alr	%0,%2\n"
		"	clr	%0,%3\n"
		"	jl	0f\n"
		"	slr	%0,%3\n"
		"	ahi	%1,1\n"
		"0:\n"
		: "+d" (reg2), "+d" (reg3), "=d" (tmp)
		: "d" (base), "2" (1UL) : "cc" );
	words[1] = reg3;
	return reg2;
}

uint32_t __div64_32(uint64_t *n, uint32_t base)
{
	uint32_t r;

	r = __div64_31(n, ((signed) base < 0) ? (base/2) : base);
	if ((signed) base < 0) {
		uint64_t q = *n;
		if (q & 1)
			r += base/2;
		q >>= 1;
		if (base & 1) {
			int64_t rx = r - q;
			while (rx < 0) {
				rx += base;
				q--;
			}
			r = rx;
		}
		*n = q;
	}
	return r;
}

#else 

uint32_t __div64_32(uint64_t *n, uint32_t base)
{
	register uint32_t reg2 asm("2");
	register uint32_t reg3 asm("3");
	uint32_t *words = (uint32_t *) n;

	reg2 = 0UL;
	reg3 = words[0];
	asm volatile(
		"	dlr	%0,%2\n"
		: "+d" (reg2), "+d" (reg3) : "d" (base) : "cc" );
	words[0] = reg3;
	reg3 = words[1];
	asm volatile(
		"	dlr	%0,%2\n"
		: "+d" (reg2), "+d" (reg3) : "d" (base) : "cc" );
	words[1] = reg3;
	return reg2;
}

#endif 
