#ifndef __ASM_SH_TIMEX_H
#define __ASM_SH_TIMEX_H

#ifdef CONFIG_SH_PCLK_FREQ
#define CLOCK_TICK_RATE		(CONFIG_SH_PCLK_FREQ / 4) 
#else
#define CLOCK_TICK_RATE		1193180
#endif

#include <asm-generic/timex.h>

#endif 
