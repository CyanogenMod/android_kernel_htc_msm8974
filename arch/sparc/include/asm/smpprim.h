/*
 *  smpprim.h:  SMP locking primitives on the Sparc
 *
 *  God knows we won't be actually using this code for some time
 *  but I thought I'd write it since I knew how.
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC_SMPPRIM_H
#define __SPARC_SMPPRIM_H


static inline __volatile__ char test_and_set(void *addr)
{
	char state = 0;

	__asm__ __volatile__("ldstub [%0], %1         ! test_and_set\n\t"
			     "=r" (addr), "=r" (state) :
			     "0" (addr), "1" (state) : "memory");

	return state;
}

static inline __volatile__ smp_initlock(void *spinlock)
{
	
	*((unsigned char *) spinlock) = 0;

	return;
}

static inline __volatile__ smp_lock(void *addr)
{
	while(test_and_set(addr) == 0xff)
		;

	
	return;
}

static inline __volatile__ smp_unlock(void *addr)
{
	*((unsigned char *) addr) = 0;
}

#endif 
