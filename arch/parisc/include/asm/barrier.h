#ifndef __PARISC_BARRIER_H
#define __PARISC_BARRIER_H

#define mb()		__asm__ __volatile__("":::"memory")	
#define rmb()		mb()
#define wmb()		mb()
#define smp_mb()	mb()
#define smp_rmb()	mb()
#define smp_wmb()	mb()
#define smp_read_barrier_depends()	do { } while(0)
#define read_barrier_depends()		do { } while(0)

#define set_mb(var, value)		do { var = value; mb(); } while (0)

#endif 
