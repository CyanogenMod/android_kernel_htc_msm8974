#ifndef _ASM_H8300_TIMEX_H
#define _ASM_H8300_TIMEX_H

#define CLOCK_TICK_RATE (CONFIG_CPU_CLOCK*1000/8192) 

typedef unsigned long cycles_t;
extern short h8300_timer_count;

static inline cycles_t get_cycles(void)
{
	return 0;
}

#endif
