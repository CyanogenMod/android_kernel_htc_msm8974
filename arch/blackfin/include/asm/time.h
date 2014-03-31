/*
 * asm-blackfin/time.h:
 *
 * Copyright 2004-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _ASM_BLACKFIN_TIME_H
#define _ASM_BLACKFIN_TIME_H


#ifndef CONFIG_CPU_FREQ
# define TIME_SCALE 1
#else
#define TIME_SCALE 4

# ifdef CONFIG_CYCLES_CLOCKSOURCE
extern unsigned long long __bfin_cycles_off;
extern unsigned int __bfin_cycles_mod;
# endif
#endif

#if defined(CONFIG_TICKSOURCE_CORETMR)
extern void bfin_coretmr_init(void);
extern void bfin_coretmr_clockevent_init(void);
#endif

#endif
