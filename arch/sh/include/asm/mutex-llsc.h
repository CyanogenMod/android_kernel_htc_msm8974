#ifndef __ASM_SH_MUTEX_LLSC_H
#define __ASM_SH_MUTEX_LLSC_H

static inline void
__mutex_fastpath_lock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	int __done, __res;

	__asm__ __volatile__ (
		"movli.l	@%2, %0	\n"
		"add		#-1, %0	\n"
		"movco.l	%0, @%2	\n"
		"movt		%1	\n"
		: "=&z" (__res), "=&r" (__done)
		: "r" (&(count)->counter)
		: "t");

	if (unlikely(!__done || __res != 0))
		fail_fn(count);
}

static inline int
__mutex_fastpath_lock_retval(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	int __done, __res;

	__asm__ __volatile__ (
		"movli.l	@%2, %0	\n"
		"add		#-1, %0	\n"
		"movco.l	%0, @%2	\n"
		"movt		%1	\n"
		: "=&z" (__res), "=&r" (__done)
		: "r" (&(count)->counter)
		: "t");

	if (unlikely(!__done || __res != 0))
		__res = fail_fn(count);

	return __res;
}

static inline void
__mutex_fastpath_unlock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	int __done, __res;

	__asm__ __volatile__ (
		"movli.l	@%2, %0	\n\t"
		"add		#1, %0	\n\t"
		"movco.l	%0, @%2 \n\t"
		"movt		%1	\n\t"
		: "=&z" (__res), "=&r" (__done)
		: "r" (&(count)->counter)
		: "t");

	if (unlikely(!__done || __res <= 0))
		fail_fn(count);
}

#define __mutex_slowpath_needs_to_unlock()	1

static inline int
__mutex_fastpath_trylock(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	int __res, __orig;

	__asm__ __volatile__ (
		"1: movli.l	@%2, %0		\n\t"
		"dt		%0		\n\t"
		"movco.l	%0,@%2		\n\t"
		"bf		1b		\n\t"
		"cmp/eq		#0,%0		\n\t"
		"bt		2f		\n\t"
		"mov		#0, %1		\n\t"
		"bf		3f		\n\t"
		"2: mov		#1, %1		\n\t"
		"3:				"
		: "=&z" (__orig), "=&r" (__res)
		: "r" (&count->counter)
		: "t");

	return __res;
}
#endif 
