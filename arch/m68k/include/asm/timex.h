#ifndef _ASMm68K_TIMEX_H
#define _ASMm68K_TIMEX_H

#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#define CLOCK_TICK_RATE	MCF_CLK
#else
#define CLOCK_TICK_RATE	1193180 
#endif

typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	return 0;
}

#endif
