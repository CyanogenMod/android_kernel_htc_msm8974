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
 * This file is included into spinlock_32.c or _64.c.
 */

#ifdef __tilegx__
#define CYCLES_PER_RELAX_LOOP 7
#else
#define CYCLES_PER_RELAX_LOOP 8
#endif

static inline void
relax(int iterations)
{
	for (; iterations > 0; iterations--)
		__insn_mfspr(SPR_PASS);
	barrier();
}

static void delay_backoff(int iterations)
{
	u32 exponent, loops;

	exponent = iterations + 1;

	if (exponent > 8)
		exponent = 8;

	loops = 1 << exponent;

	
	loops += __insn_crc32_32(stack_pointer, get_cycles_low()) &
		(loops - 1);

	relax(loops);
}
