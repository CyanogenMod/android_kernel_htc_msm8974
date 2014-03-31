/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998, 1999, 2003 by Ralf Baechle
 */
#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H

#ifdef __KERNEL__

#include <asm/mipsregs.h>

#define CLOCK_TICK_RATE 1193182


typedef unsigned int cycles_t;

static inline cycles_t get_cycles(void)
{
	return 0;
}

#endif 

#endif 
