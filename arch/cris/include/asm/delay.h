#ifndef _CRIS_DELAY_H
#define _CRIS_DELAY_H

/*
 * Copyright (C) 1998-2002 Axis Communications AB
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */

#include <arch/delay.h>


extern unsigned long loops_per_usec; 

#ifndef udelay
static inline void udelay(unsigned long usecs)
{
	__delay(usecs * loops_per_usec);
}
#endif

#endif 



