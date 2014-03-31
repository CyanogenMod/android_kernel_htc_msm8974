#ifndef _ASMPARISC_TIMEX_H
#define _ASMPARISC_TIMEX_H


#define CLOCK_TICK_RATE	1193180 

typedef unsigned long cycles_t;

static inline cycles_t get_cycles (void)
{
	return mfctl(16);
}

#endif
