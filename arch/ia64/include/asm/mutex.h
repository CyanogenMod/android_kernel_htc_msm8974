/*
 * ia64 implementation of the mutex fastpath.
 *
 * Copyright (C) 2006 Ken Chen <kenneth.w.chen@intel.com>
 *
 */

#ifndef _ASM_MUTEX_H
#define _ASM_MUTEX_H

static inline void
__mutex_fastpath_lock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	if (unlikely(ia64_fetchadd4_acq(count, -1) != 1))
		fail_fn(count);
}

static inline int
__mutex_fastpath_lock_retval(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (unlikely(ia64_fetchadd4_acq(count, -1) != 1))
		return fail_fn(count);
	return 0;
}

static inline void
__mutex_fastpath_unlock(atomic_t *count, void (*fail_fn)(atomic_t *))
{
	int ret = ia64_fetchadd4_rel(count, 1);
	if (unlikely(ret < 0))
		fail_fn(count);
}

#define __mutex_slowpath_needs_to_unlock()		1

static inline int
__mutex_fastpath_trylock(atomic_t *count, int (*fail_fn)(atomic_t *))
{
	if (cmpxchg_acq(count, 1, 0) == 1)
		return 1;
	return 0;
}

#endif
