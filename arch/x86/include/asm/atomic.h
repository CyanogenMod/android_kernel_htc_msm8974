#ifndef _ASM_X86_ATOMIC_H
#define _ASM_X86_ATOMIC_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/processor.h>
#include <asm/alternative.h>
#include <asm/cmpxchg.h>


#define ATOMIC_INIT(i)	{ (i) }

static inline int atomic_read(const atomic_t *v)
{
	return (*(volatile int *)&(v)->counter);
}

static inline void atomic_set(atomic_t *v, int i)
{
	v->counter = i;
}

static inline void atomic_add(int i, atomic_t *v)
{
	asm volatile(LOCK_PREFIX "addl %1,%0"
		     : "+m" (v->counter)
		     : "ir" (i));
}

static inline void atomic_sub(int i, atomic_t *v)
{
	asm volatile(LOCK_PREFIX "subl %1,%0"
		     : "+m" (v->counter)
		     : "ir" (i));
}

static inline int atomic_sub_and_test(int i, atomic_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "subl %2,%0; sete %1"
		     : "+m" (v->counter), "=qm" (c)
		     : "ir" (i) : "memory");
	return c;
}

static inline void atomic_inc(atomic_t *v)
{
	asm volatile(LOCK_PREFIX "incl %0"
		     : "+m" (v->counter));
}

static inline void atomic_dec(atomic_t *v)
{
	asm volatile(LOCK_PREFIX "decl %0"
		     : "+m" (v->counter));
}

static inline int atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "decl %0; sete %1"
		     : "+m" (v->counter), "=qm" (c)
		     : : "memory");
	return c != 0;
}

static inline int atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "incl %0; sete %1"
		     : "+m" (v->counter), "=qm" (c)
		     : : "memory");
	return c != 0;
}

static inline int atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "addl %2,%0; sets %1"
		     : "+m" (v->counter), "=qm" (c)
		     : "ir" (i) : "memory");
	return c;
}

static inline int atomic_add_return(int i, atomic_t *v)
{
#ifdef CONFIG_M386
	int __i;
	unsigned long flags;
	if (unlikely(boot_cpu_data.x86 <= 3))
		goto no_xadd;
#endif
	
	return i + xadd(&v->counter, i);

#ifdef CONFIG_M386
no_xadd: 
	raw_local_irq_save(flags);
	__i = atomic_read(v);
	atomic_set(v, i + __i);
	raw_local_irq_restore(flags);
	return i + __i;
#endif
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	return atomic_add_return(-i, v);
}

#define atomic_inc_return(v)  (atomic_add_return(1, v))
#define atomic_dec_return(v)  (atomic_sub_return(1, v))

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	return cmpxchg(&v->counter, old, new);
}

static inline int atomic_xchg(atomic_t *v, int new)
{
	return xchg(&v->counter, new);
}

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c;
}


static inline int atomic_dec_if_positive(atomic_t *v)
{
	int c, old, dec;
	c = atomic_read(v);
	for (;;) {
		dec = c - 1;
		if (unlikely(dec < 0))
			break;
		old = atomic_cmpxchg((v), c, dec);
		if (likely(old == c))
			break;
		c = old;
	}
	return dec;
}

static inline short int atomic_inc_short(short int *v)
{
	asm(LOCK_PREFIX "addw $1, %0" : "+m" (*v));
	return *v;
}

#ifdef CONFIG_X86_64
static inline void atomic_or_long(unsigned long *v1, unsigned long v2)
{
	asm(LOCK_PREFIX "orq %1, %0" : "+m" (*v1) : "r" (v2));
}
#endif

#define atomic_clear_mask(mask, addr)				\
	asm volatile(LOCK_PREFIX "andl %0,%1"			\
		     : : "r" (~(mask)), "m" (*(addr)) : "memory")

#define atomic_set_mask(mask, addr)				\
	asm volatile(LOCK_PREFIX "orl %0,%1"			\
		     : : "r" ((unsigned)(mask)), "m" (*(addr))	\
		     : "memory")

#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#ifdef CONFIG_X86_32
# include "atomic64_32.h"
#else
# include "atomic64_64.h"
#endif

#endif 
