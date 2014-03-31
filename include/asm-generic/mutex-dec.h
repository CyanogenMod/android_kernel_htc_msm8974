#ifndef _ASM_GENERIC_MUTEX_DEC_H
#define _ASM_GENERIC_MUTEX_DEC_H

static inline void
__mutex_fastpath_lock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(atomic_dec_return(count) < 0))
		fail_fn(count);
}

static inline int
__mutex_fastpath_lock_retval(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (unlikely(atomic_dec_return(count) < 0))
		return fail_fn(count);
	return 0;
}

static inline void
__mutex_fastpath_unlock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(atomic_inc_return(count) <= 0))
		fail_fn(count);
}

#define __mutex_slowpath_needs_to_unlock()		1

static inline int
__mutex_fastpath_trylock(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (likely(atomic_cmpxchg(count, 1, 0) == 1))
		return 1;
	return 0;
}

#endif
