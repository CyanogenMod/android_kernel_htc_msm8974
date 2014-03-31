#ifndef _ASM_GENERIC_MUTEX_NULL_H
#define _ASM_GENERIC_MUTEX_NULL_H

#define __mutex_fastpath_lock(count, fail_fn)		fail_fn(count)
#define __mutex_fastpath_lock_retval(count, fail_fn)	fail_fn(count)
#define __mutex_fastpath_unlock(count, fail_fn)		fail_fn(count)
#define __mutex_fastpath_trylock(count, fail_fn)	fail_fn(count)
#define __mutex_slowpath_needs_to_unlock()		1

#endif
