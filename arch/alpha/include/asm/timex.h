#ifndef _ASMALPHA_TIMEX_H
#define _ASMALPHA_TIMEX_H

#define CLOCK_TICK_RATE	32768


typedef unsigned int cycles_t;

static inline cycles_t get_cycles (void)
{
	cycles_t ret;
	__asm__ __volatile__ ("rpcc %0" : "=r"(ret));
	return ret;
}

#endif
