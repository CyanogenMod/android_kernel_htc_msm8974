/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_TIMEX_H
#define __ASM_AVR32_TIMEX_H

#define CLOCK_TICK_RATE		500000	

typedef unsigned long cycles_t;

static inline cycles_t get_cycles (void)
{
	return 0;
}

#define ARCH_HAS_READ_CURRENT_TIMER

#endif 
