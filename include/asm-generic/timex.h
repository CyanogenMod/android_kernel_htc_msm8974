#ifndef __ASM_GENERIC_TIMEX_H
#define __ASM_GENERIC_TIMEX_H

typedef unsigned long cycles_t;
#ifndef get_cycles
static inline cycles_t get_cycles(void)
{
	return 0;
}
#endif

#undef ARCH_HAS_READ_CURRENT_TIMER

#endif 
