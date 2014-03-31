#ifndef _ASMsparc_TIMEX_H
#define _ASMsparc_TIMEX_H

#define CLOCK_TICK_RATE	1193180 

typedef unsigned long cycles_t;
#define get_cycles()	(0)

extern u32 (*do_arch_gettimeoffset)(void);
#endif
