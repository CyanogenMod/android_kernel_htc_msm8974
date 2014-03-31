#ifndef _ASM_POWERPC_DELAY_H
#define _ASM_POWERPC_DELAY_H
#ifdef __KERNEL__

#include <asm/time.h>

/*
 * Copyright 1996, Paul Mackerras.
 * Copyright (C) 2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * PPC64 Support added by Dave Engebretsen, Todd Inglett, Mike Corrigan,
 * Anton Blanchard.
 */

extern void __delay(unsigned long loops);
extern void udelay(unsigned long usecs);

#ifdef CONFIG_PPC64
#define mdelay(n)	udelay((n) * 1000)
#endif

#define spin_event_timeout(condition, timeout, delay)                          \
({                                                                             \
	typeof(condition) __ret;                                               \
	unsigned long __loops = tb_ticks_per_usec * timeout;                   \
	unsigned long __start = get_tbl();                                     \
	while (!(__ret = (condition)) && (tb_ticks_since(__start) <= __loops)) \
		if (delay)                                                     \
			udelay(delay);                                         \
		else                                                           \
			cpu_relax();                                           \
	if (!__ret)                                                            \
		__ret = (condition);                                           \
	__ret;		                                                       \
})

#endif 
#endif 
