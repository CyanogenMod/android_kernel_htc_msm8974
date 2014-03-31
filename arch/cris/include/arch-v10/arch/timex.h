#ifndef _ASM_CRIS_ARCH_TIMEX_H
#define _ASM_CRIS_ARCH_TIMEX_H

#define PRESCALE_FREQ 25000000
#define PRESCALE_VALUE 1000
#define CLOCK_TICK_RATE 25000 
#define TIMER0_FREQ (CLOCK_TICK_RATE)
#define TIMER0_CLKSEL flexible
#define TIMER0_DIV (TIMER0_FREQ/(HZ))


#define GET_JIFFIES_USEC() \
  ( (TIMER0_DIV - *R_TIMER0_DATA) * (1000000/HZ)/TIMER0_DIV )

unsigned long get_ns_in_jiffie(void);

static inline unsigned long get_us_in_jiffie_highres(void)
{
	return get_ns_in_jiffie()/1000;
}

#endif
