#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H

#define CLOCK_TICK_RATE		1193180 
#define CLOCK_TICK_FACTOR	20	

typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	return 0;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif

