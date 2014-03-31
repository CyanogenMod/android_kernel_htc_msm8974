
#ifndef _CRIS_BITOPS_H
#define _CRIS_BITOPS_H

#ifdef __KERNEL__ 

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <arch/bitops.h>
#include <linux/atomic.h>
#include <linux/compiler.h>


#define set_bit(nr, addr)    (void)test_and_set_bit(nr, addr)


#define clear_bit(nr, addr)  (void)test_and_clear_bit(nr, addr)


#define change_bit(nr, addr) (void)test_and_change_bit(nr, addr)


static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	cris_atomic_save(addr, flags);
	retval = (mask & *adr) != 0;
	*adr |= mask;
	cris_atomic_restore(addr, flags);
	return retval;
}

#define smp_mb__before_clear_bit()      barrier()
#define smp_mb__after_clear_bit()       barrier()


static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	cris_atomic_save(addr, flags);
	retval = (mask & *adr) != 0;
	*adr &= ~mask;
	cris_atomic_restore(addr, flags);
	return retval;
}


static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	cris_atomic_save(addr, flags);
	retval = (mask & *adr) != 0;
	*adr ^= mask;
	cris_atomic_restore(addr, flags);
	return retval;
}

#include <asm-generic/bitops/non-atomic.h>

#define ffs kernel_ffs

#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/find.h>
#include <asm-generic/bitops/lock.h>

#include <asm-generic/bitops/le.h>

#include <asm-generic/bitops/ext2-atomic-setbit.h>

#include <asm-generic/bitops/sched.h>

#endif 

#endif 
