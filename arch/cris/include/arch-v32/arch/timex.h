#ifndef _ASM_CRIS_ARCH_TIMEX_H
#define _ASM_CRIS_ARCH_TIMEX_H

#include <hwregs/reg_map.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/timer_defs.h>


#define CLOCK_TICK_RATE 100000000	

#define TIMER0_FREQ (CLOCK_TICK_RATE)
#define TIMER0_DIV (TIMER0_FREQ/(HZ))

#define GET_JIFFIES_USEC() \
	((TIMER0_DIV - REG_RD(timer, regi_timer0, r_tmr0_data)) / 100)

extern unsigned long get_ns_in_jiffie(void);

static inline unsigned long get_us_in_jiffie_highres(void)
{
	return get_ns_in_jiffie() / 1000;
}

#endif

