/*
 * Generic C implementation of atomic counter operations. Usable on
 * UP systems only. Do not include in machine independent code.
 *
 * Originally implemented for MN10300.
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef __ASM_GENERIC_ATOMIC_H
#define __ASM_GENERIC_ATOMIC_H

#include <asm/cmpxchg.h>

#ifdef CONFIG_SMP
# if !defined(atomic_add_return) || !defined(atomic_sub_return) || \
     !defined(atomic_clear_mask) || !defined(atomic_set_mask)
#  error "SMP requires a little arch-specific magic"
# endif
#endif


#define ATOMIC_INIT(i)	{ (i) }

#ifdef __KERNEL__

#ifndef atomic_read
#define atomic_read(v)	(*(volatile int *)&(v)->counter)
#endif

#define atomic_set(v, i) (((v)->counter) = (i))

#include <linux/irqflags.h>

#ifndef atomic_add_return
static inline int atomic_add_return(int i, atomic_t *v)
{
	unsigned long flags;
	int temp;

	raw_local_irq_save(flags); 
	temp = v->counter;
	temp += i;
	v->counter = temp;
	raw_local_irq_restore(flags);

	return temp;
}
#endif

#ifndef atomic_sub_return
static inline int atomic_sub_return(int i, atomic_t *v)
{
	unsigned long flags;
	int temp;

	raw_local_irq_save(flags); 
	temp = v->counter;
	temp -= i;
	v->counter = temp;
	raw_local_irq_restore(flags);

	return temp;
}
#endif

static inline int atomic_add_negative(int i, atomic_t *v)
{
	return atomic_add_return(i, v) < 0;
}

static inline void atomic_add(int i, atomic_t *v)
{
	atomic_add_return(i, v);
}

static inline void atomic_sub(int i, atomic_t *v)
{
	atomic_sub_return(i, v);
}

static inline void atomic_inc(atomic_t *v)
{
	atomic_add_return(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub_return(1, v);
}

#define atomic_dec_return(v)		atomic_sub_return(1, (v))
#define atomic_inc_return(v)		atomic_add_return(1, (v))

#define atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_dec_return(v) == 0)
#define atomic_inc_and_test(v)		(atomic_inc_return(v) == 0)

#define atomic_xchg(ptr, v)		(xchg(&(ptr)->counter, (v)))
#define atomic_cmpxchg(v, old, new)	(cmpxchg(&((v)->counter), (old), (new)))

#define cmpxchg_local(ptr, o, n)				  	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))

#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
  int c, old;
  c = atomic_read(v);
  while (c != u && (old = atomic_cmpxchg(v, c, c + a)) != c)
    c = old;
  return c;
}

#ifndef atomic_clear_mask
static inline void atomic_clear_mask(unsigned long mask, atomic_t *v)
{
	unsigned long flags;

	mask = ~mask;
	raw_local_irq_save(flags); 
	v->counter &= mask;
	raw_local_irq_restore(flags);
}
#endif

#ifndef atomic_set_mask
static inline void atomic_set_mask(unsigned int mask, atomic_t *v)
{
	unsigned long flags;

	raw_local_irq_save(flags); 
	v->counter |= mask;
	raw_local_irq_restore(flags);
}
#endif

#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif 
#endif 
