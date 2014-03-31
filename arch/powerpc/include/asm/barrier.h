/*
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 */
#ifndef _ASM_POWERPC_BARRIER_H
#define _ASM_POWERPC_BARRIER_H

#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#define rmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define wmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define read_barrier_depends()  do { } while(0)

#define set_mb(var, value)	do { var = value; mb(); } while (0)

#ifdef CONFIG_SMP

#ifdef __SUBARCH_HAS_LWSYNC
#    define SMPWMB      LWSYNC
#else
#    define SMPWMB      eieio
#endif

#define smp_mb()	mb()
#define smp_rmb()	__asm__ __volatile__ (stringify_in_c(LWSYNC) : : :"memory")
#define smp_wmb()	__asm__ __volatile__ (stringify_in_c(SMPWMB) : : :"memory")
#define smp_read_barrier_depends()	read_barrier_depends()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while(0)
#endif 

#define data_barrier(x)	\
	asm volatile("twi 0,%0,0; isync" : : "r" (x) : "memory");

#endif 
