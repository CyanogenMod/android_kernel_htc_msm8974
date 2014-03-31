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

#ifndef _ASM_TILE_TIMEX_H
#define _ASM_TILE_TIMEX_H

#define CLOCK_TICK_RATE	1000000

typedef unsigned long long cycles_t;

#if CHIP_HAS_SPLIT_CYCLE()
cycles_t get_cycles(void);
#define get_cycles_low() __insn_mfspr(SPR_CYCLE_LOW)
#else
static inline cycles_t get_cycles(void)
{
	return __insn_mfspr(SPR_CYCLE);
}
#define get_cycles_low() __insn_mfspr(SPR_CYCLE)   
#endif

cycles_t get_clock_rate(void);

cycles_t ns2cycles(unsigned long nsecs);

void setup_clock(void);

void setup_tile_timer(void);

#endif 
