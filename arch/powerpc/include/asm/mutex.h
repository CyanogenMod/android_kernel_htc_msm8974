#ifndef _ASM_POWERPC_MUTEX_H
#define _ASM_POWERPC_MUTEX_H

static inline int __mutex_cmpxchg_lock(atomic_t *v, int old, int new)
{
	int t;

	__asm__ __volatile__ (
"1:	lwarx	%0,0,%1		# mutex trylock\n\
	cmpw	0,%0,%2\n\
	bne-	2f\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%3,0,%1\n\
	bne-	1b"
	PPC_ACQUIRE_BARRIER
	"\n\
2:"
	: "=&r" (t)
	: "r" (&v->counter), "r" (old), "r" (new)
	: "cc", "memory");

	return t;
}

static inline int __mutex_dec_return_lock(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1		# mutex lock\n\
	addic	%0,%0,-1\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%0,0,%1\n\
	bne-	1b"
	PPC_ACQUIRE_BARRIER
	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

static inline int __mutex_inc_return_unlock(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
	PPC_RELEASE_BARRIER
"1:	lwarx	%0,0,%1		# mutex unlock\n\
	addic	%0,%0,1\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%0,0,%1 \n\
	bne-	1b"
	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

static inline void
__mutex_fastpath_lock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(__mutex_dec_return_lock(count) < 0))
		fail_fn(count);
}

static inline int
__mutex_fastpath_lock_retval(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (unlikely(__mutex_dec_return_lock(count) < 0))
		return fail_fn(count);
	return 0;
}

static inline void
__mutex_fastpath_unlock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(__mutex_inc_return_unlock(count) <= 0))
		fail_fn(count);
}

#define __mutex_slowpath_needs_to_unlock()		1

static inline int
__mutex_fastpath_trylock(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (likely(__mutex_cmpxchg_lock(count, 1, 0) == 1))
		return 1;
	return 0;
}

#endif
