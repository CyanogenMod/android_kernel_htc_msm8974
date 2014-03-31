#ifndef _ASMsparc64_TIMEX_H
#define _ASMsparc64_TIMEX_H

#include <asm/timer.h>

#define CLOCK_TICK_RATE	1193180 

typedef unsigned long cycles_t;
#define get_cycles()	tick_ops->get_tick()

#define ARCH_HAS_READ_CURRENT_TIMER

#endif
