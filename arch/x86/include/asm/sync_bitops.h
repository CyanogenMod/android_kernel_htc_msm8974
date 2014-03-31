#ifndef _ASM_X86_SYNC_BITOPS_H
#define _ASM_X86_SYNC_BITOPS_H

/*
 * Copyright 1992, Linus Torvalds.
 */


#define ADDR (*(volatile long *)addr)

static inline void sync_set_bit(int nr, volatile unsigned long *addr)
{
	asm volatile("lock; btsl %1,%0"
		     : "+m" (ADDR)
		     : "Ir" (nr)
		     : "memory");
}

static inline void sync_clear_bit(int nr, volatile unsigned long *addr)
{
	asm volatile("lock; btrl %1,%0"
		     : "+m" (ADDR)
		     : "Ir" (nr)
		     : "memory");
}

static inline void sync_change_bit(int nr, volatile unsigned long *addr)
{
	asm volatile("lock; btcl %1,%0"
		     : "+m" (ADDR)
		     : "Ir" (nr)
		     : "memory");
}

static inline int sync_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("lock; btsl %2,%1\n\tsbbl %0,%0"
		     : "=r" (oldbit), "+m" (ADDR)
		     : "Ir" (nr) : "memory");
	return oldbit;
}

static inline int sync_test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("lock; btrl %2,%1\n\tsbbl %0,%0"
		     : "=r" (oldbit), "+m" (ADDR)
		     : "Ir" (nr) : "memory");
	return oldbit;
}

static inline int sync_test_and_change_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("lock; btcl %2,%1\n\tsbbl %0,%0"
		     : "=r" (oldbit), "+m" (ADDR)
		     : "Ir" (nr) : "memory");
	return oldbit;
}

#define sync_test_bit(nr, addr) test_bit(nr, addr)

#undef ADDR

#endif 
