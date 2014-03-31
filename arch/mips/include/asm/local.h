#ifndef _ARCH_MIPS_LOCAL_H
#define _ARCH_MIPS_LOCAL_H

#include <linux/percpu.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include <asm/cmpxchg.h>
#include <asm/war.h>

typedef struct
{
	atomic_long_t a;
} local_t;

#define LOCAL_INIT(i)	{ ATOMIC_LONG_INIT(i) }

#define local_read(l)	atomic_long_read(&(l)->a)
#define local_set(l, i)	atomic_long_set(&(l)->a, (i))

#define local_add(i, l)	atomic_long_add((i), (&(l)->a))
#define local_sub(i, l)	atomic_long_sub((i), (&(l)->a))
#define local_inc(l)	atomic_long_inc(&(l)->a)
#define local_dec(l)	atomic_long_dec(&(l)->a)

static __inline__ long local_add_return(long i, local_t * l)
{
	unsigned long result;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:"	__LL	"%1, %2		# local_add_return	\n"
		"	addu	%0, %1, %3				\n"
			__SC	"%0, %2					\n"
		"	beqzl	%0, 1b					\n"
		"	addu	%0, %1, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (result), "=&r" (temp), "=m" (l->a.counter)
		: "Ir" (i), "m" (l->a.counter)
		: "memory");
	} else if (kernel_uses_llsc) {
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:"	__LL	"%1, %2		# local_add_return	\n"
		"	addu	%0, %1, %3				\n"
			__SC	"%0, %2					\n"
		"	beqz	%0, 1b					\n"
		"	addu	%0, %1, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (result), "=&r" (temp), "=m" (l->a.counter)
		: "Ir" (i), "m" (l->a.counter)
		: "memory");
	} else {
		unsigned long flags;

		local_irq_save(flags);
		result = l->a.counter;
		result += i;
		l->a.counter = result;
		local_irq_restore(flags);
	}

	return result;
}

static __inline__ long local_sub_return(long i, local_t * l)
{
	unsigned long result;

	if (kernel_uses_llsc && R10000_LLSC_WAR) {
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:"	__LL	"%1, %2		# local_sub_return	\n"
		"	subu	%0, %1, %3				\n"
			__SC	"%0, %2					\n"
		"	beqzl	%0, 1b					\n"
		"	subu	%0, %1, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (result), "=&r" (temp), "=m" (l->a.counter)
		: "Ir" (i), "m" (l->a.counter)
		: "memory");
	} else if (kernel_uses_llsc) {
		unsigned long temp;

		__asm__ __volatile__(
		"	.set	mips3					\n"
		"1:"	__LL	"%1, %2		# local_sub_return	\n"
		"	subu	%0, %1, %3				\n"
			__SC	"%0, %2					\n"
		"	beqz	%0, 1b					\n"
		"	subu	%0, %1, %3				\n"
		"	.set	mips0					\n"
		: "=&r" (result), "=&r" (temp), "=m" (l->a.counter)
		: "Ir" (i), "m" (l->a.counter)
		: "memory");
	} else {
		unsigned long flags;

		local_irq_save(flags);
		result = l->a.counter;
		result -= i;
		l->a.counter = result;
		local_irq_restore(flags);
	}

	return result;
}

#define local_cmpxchg(l, o, n) \
	((long)cmpxchg_local(&((l)->a.counter), (o), (n)))
#define local_xchg(l, n) (atomic_long_xchg((&(l)->a), (n)))

#define local_add_unless(l, a, u)				\
({								\
	long c, old;						\
	c = local_read(l);					\
	while (c != (u) && (old = local_cmpxchg((l), c, c + (a))) != c) \
		c = old;					\
	c != (u);						\
})
#define local_inc_not_zero(l) local_add_unless((l), 1, 0)

#define local_dec_return(l) local_sub_return(1, (l))
#define local_inc_return(l) local_add_return(1, (l))

#define local_sub_and_test(i, l) (local_sub_return((i), (l)) == 0)

#define local_inc_and_test(l) (local_inc_return(l) == 0)

#define local_dec_and_test(l) (local_sub_return(1, (l)) == 0)

#define local_add_negative(i, l) (local_add_return(i, (l)) < 0)


#define __local_inc(l)		((l)->a.counter++)
#define __local_dec(l)		((l)->a.counter++)
#define __local_add(i, l)	((l)->a.counter+=(i))
#define __local_sub(i, l)	((l)->a.counter-=(i))

#endif 
