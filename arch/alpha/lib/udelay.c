/*
 * Copyright (C) 1993, 2000 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */

#include <linux/module.h>
#include <linux/sched.h> 
#include <asm/param.h>
#include <asm/smp.h>
#include <linux/delay.h>


void
__delay(int loops)
{
	int tmp;
	__asm__ __volatile__(
		"	rpcc %0\n"
		"	addl %1,%0,%1\n"
		"1:	rpcc %0\n"
		"	subl %1,%0,%0\n"
		"	bgt %0,1b"
		: "=&r" (tmp), "=r" (loops) : "1"(loops));
}

#ifdef CONFIG_SMP
#define LPJ	 cpu_data[smp_processor_id()].loops_per_jiffy
#else
#define LPJ	 loops_per_jiffy
#endif

void
udelay(unsigned long usecs)
{
	usecs *= (((unsigned long)HZ << 32) / 1000000) * LPJ;
	__delay((long)usecs >> 32);
}
EXPORT_SYMBOL(udelay);

void
ndelay(unsigned long nsecs)
{
	nsecs *= (((unsigned long)HZ << 32) / 1000000000) * LPJ;
	__delay((long)nsecs >> 32);
}
EXPORT_SYMBOL(ndelay);
