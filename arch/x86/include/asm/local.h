#ifndef _ASM_X86_LOCAL_H
#define _ASM_X86_LOCAL_H

#include <linux/percpu.h>

#include <linux/atomic.h>
#include <asm/asm.h>

typedef struct {
	atomic_long_t a;
} local_t;

#define LOCAL_INIT(i)	{ ATOMIC_LONG_INIT(i) }

#define local_read(l)	atomic_long_read(&(l)->a)
#define local_set(l, i)	atomic_long_set(&(l)->a, (i))

static inline void local_inc(local_t *l)
{
	asm volatile(_ASM_INC "%0"
		     : "+m" (l->a.counter));
}

static inline void local_dec(local_t *l)
{
	asm volatile(_ASM_DEC "%0"
		     : "+m" (l->a.counter));
}

static inline void local_add(long i, local_t *l)
{
	asm volatile(_ASM_ADD "%1,%0"
		     : "+m" (l->a.counter)
		     : "ir" (i));
}

static inline void local_sub(long i, local_t *l)
{
	asm volatile(_ASM_SUB "%1,%0"
		     : "+m" (l->a.counter)
		     : "ir" (i));
}

static inline int local_sub_and_test(long i, local_t *l)
{
	unsigned char c;

	asm volatile(_ASM_SUB "%2,%0; sete %1"
		     : "+m" (l->a.counter), "=qm" (c)
		     : "ir" (i) : "memory");
	return c;
}

static inline int local_dec_and_test(local_t *l)
{
	unsigned char c;

	asm volatile(_ASM_DEC "%0; sete %1"
		     : "+m" (l->a.counter), "=qm" (c)
		     : : "memory");
	return c != 0;
}

static inline int local_inc_and_test(local_t *l)
{
	unsigned char c;

	asm volatile(_ASM_INC "%0; sete %1"
		     : "+m" (l->a.counter), "=qm" (c)
		     : : "memory");
	return c != 0;
}

static inline int local_add_negative(long i, local_t *l)
{
	unsigned char c;

	asm volatile(_ASM_ADD "%2,%0; sets %1"
		     : "+m" (l->a.counter), "=qm" (c)
		     : "ir" (i) : "memory");
	return c;
}

static inline long local_add_return(long i, local_t *l)
{
	long __i;
#ifdef CONFIG_M386
	unsigned long flags;
	if (unlikely(boot_cpu_data.x86 <= 3))
		goto no_xadd;
#endif
	
	__i = i;
	asm volatile(_ASM_XADD "%0, %1;"
		     : "+r" (i), "+m" (l->a.counter)
		     : : "memory");
	return i + __i;

#ifdef CONFIG_M386
no_xadd: 
	local_irq_save(flags);
	__i = local_read(l);
	local_set(l, i + __i);
	local_irq_restore(flags);
	return i + __i;
#endif
}

static inline long local_sub_return(long i, local_t *l)
{
	return local_add_return(-i, l);
}

#define local_inc_return(l)  (local_add_return(1, l))
#define local_dec_return(l)  (local_sub_return(1, l))

#define local_cmpxchg(l, o, n) \
	(cmpxchg_local(&((l)->a.counter), (o), (n)))
#define local_xchg(l, n) (xchg(&((l)->a.counter), (n)))

#define local_add_unless(l, a, u)				\
({								\
	long c, old;						\
	c = local_read((l));					\
	for (;;) {						\
		if (unlikely(c == (u)))				\
			break;					\
		old = local_cmpxchg((l), c, c + (a));		\
		if (likely(old == c))				\
			break;					\
		c = old;					\
	}							\
	c != (u);						\
})
#define local_inc_not_zero(l) local_add_unless((l), 1, 0)

#define __local_inc(l)		local_inc(l)
#define __local_dec(l)		local_dec(l)
#define __local_add(i, l)	local_add((i), (l))
#define __local_sub(i, l)	local_sub((i), (l))

#endif 
