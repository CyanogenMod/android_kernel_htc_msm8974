/*
 * Copyright (C) 1999, 2000  Niibe Yutaka  &  Kaz Kojima
 * Copyright (C) 2002 Paul Mundt
 */
#ifndef __ASM_SH_BARRIER_H
#define __ASM_SH_BARRIER_H

#if defined(CONFIG_CPU_SH4A) || defined(CONFIG_CPU_SH5)
#include <asm/cache_insns.h>
#endif

#if defined(CONFIG_CPU_SH4A) || defined(CONFIG_CPU_SH5)
#define mb()		__asm__ __volatile__ ("synco": : :"memory")
#define rmb()		mb()
#define wmb()		__asm__ __volatile__ ("synco": : :"memory")
#define ctrl_barrier()	__icbi(PAGE_OFFSET)
#define read_barrier_depends()	do { } while(0)
#else
#define mb()		__asm__ __volatile__ ("": : :"memory")
#define rmb()		mb()
#define wmb()		__asm__ __volatile__ ("": : :"memory")
#define ctrl_barrier()	__asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop")
#define read_barrier_depends()	do { } while(0)
#endif

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define smp_read_barrier_depends()	read_barrier_depends()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while(0)
#endif

#define set_mb(var, value) do { (void)xchg(&var, value); } while (0)

#endif 
