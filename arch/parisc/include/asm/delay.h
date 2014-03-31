#ifndef _PARISC_DELAY_H
#define _PARISC_DELAY_H

#include <asm/special_insns.h>    
#include <asm/processor.h> 


/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines
 */

static __inline__ void __delay(unsigned long loops) {
	asm volatile(
	"	.balignl	64,0x34000034\n"
	"	addib,UV -1,%0,.\n"
	"	nop\n"
		: "=r" (loops) : "0" (loops));
}

static __inline__ void __cr16_delay(unsigned long clocks) {
	unsigned long start;


	start = mfctl(16);
	while ((mfctl(16) - start) < clocks)
	    ;
}

static __inline__ void __udelay(unsigned long usecs) {
	__cr16_delay(usecs * ((unsigned long)boot_cpu_data.cpu_hz / 1000000UL));
}

#define udelay(n) __udelay(n)

#endif 
