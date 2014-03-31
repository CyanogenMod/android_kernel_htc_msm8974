#ifndef _ASM_X86_ATOMIC64_64_H
#define _ASM_X86_ATOMIC64_64_H

#include <linux/types.h>
#include <asm/alternative.h>
#include <asm/cmpxchg.h>


#define ATOMIC64_INIT(i)	{ (i) }

static inline long atomic64_read(const atomic64_t *v)
{
	return (*(volatile long *)&(v)->counter);
}

static inline void atomic64_set(atomic64_t *v, long i)
{
	v->counter = i;
}

static inline void atomic64_add(long i, atomic64_t *v)
{
	asm volatile(LOCK_PREFIX "addq %1,%0"
		     : "=m" (v->counter)
		     : "er" (i), "m" (v->counter));
}

static inline void atomic64_sub(long i, atomic64_t *v)
{
	asm volatile(LOCK_PREFIX "subq %1,%0"
		     : "=m" (v->counter)
		     : "er" (i), "m" (v->counter));
}

static inline int atomic64_sub_and_test(long i, atomic64_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "subq %2,%0; sete %1"
		     : "=m" (v->counter), "=qm" (c)
		     : "er" (i), "m" (v->counter) : "memory");
	return c;
}

static inline void atomic64_inc(atomic64_t *v)
{
	asm volatile(LOCK_PREFIX "incq %0"
		     : "=m" (v->counter)
		     : "m" (v->counter));
}

static inline void atomic64_dec(atomic64_t *v)
{
	asm volatile(LOCK_PREFIX "decq %0"
		     : "=m" (v->counter)
		     : "m" (v->counter));
}

static inline int atomic64_dec_and_test(atomic64_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "decq %0; sete %1"
		     : "=m" (v->counter), "=qm" (c)
		     : "m" (v->counter) : "memory");
	return c != 0;
}

static inline int atomic64_inc_and_test(atomic64_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "incq %0; sete %1"
		     : "=m" (v->counter), "=qm" (c)
		     : "m" (v->counter) : "memory");
	return c != 0;
}

static inline int atomic64_add_negative(long i, atomic64_t *v)
{
	unsigned char c;

	asm volatile(LOCK_PREFIX "addq %2,%0; sets %1"
		     : "=m" (v->counter), "=qm" (c)
		     : "er" (i), "m" (v->counter) : "memory");
	return c;
}

static inline long atomic64_add_return(long i, atomic64_t *v)
{
	return i + xadd(&v->counter, i);
}

static inline long atomic64_sub_return(long i, atomic64_t *v)
{
	return atomic64_add_return(-i, v);
}

#define atomic64_inc_return(v)  (atomic64_add_return(1, (v)))
#define atomic64_dec_return(v)  (atomic64_sub_return(1, (v)))

static inline long atomic64_cmpxchg(atomic64_t *v, long old, long new)
{
	return cmpxchg(&v->counter, old, new);
}

static inline long atomic64_xchg(atomic64_t *v, long new)
{
	return xchg(&v->counter, new);
}

static inline int atomic64_add_unless(atomic64_t *v, long a, long u)
{
	long c, old;
	c = atomic64_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic64_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c != (u);
}

#define atomic64_inc_not_zero(v) atomic64_add_unless((v), 1, 0)

static inline long atomic64_dec_if_positive(atomic64_t *v)
{
	long c, old, dec;
	c = atomic64_read(v);
	for (;;) {
		dec = c - 1;
		if (unlikely(dec < 0))
			break;
		old = atomic64_cmpxchg((v), c, dec);
		if (likely(old == c))
			break;
		c = old;
	}
	return dec;
}

#endif 
