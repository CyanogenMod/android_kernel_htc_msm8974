/*
 * Assembly implementation of the mutex fastpath, based on atomic
 * decrement/increment.
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#ifndef _ASM_X86_MUTEX_32_H
#define _ASM_X86_MUTEX_32_H

#include <asm/alternative.h>

#define __mutex_fastpath_lock(count, fail_fn)			\
do {								\
	unsigned int dummy;					\
								\
	typecheck(atomic_t *, count);				\
	typecheck_fn(void (*)(atomic_t *), fail_fn);		\
								\
	asm volatile(LOCK_PREFIX "   decl (%%eax)\n"		\
		     "   jns 1f	\n"				\
		     "   call " #fail_fn "\n"			\
		     "1:\n"					\
		     : "=a" (dummy)				\
		     : "a" (count)				\
		     : "memory", "ecx", "edx");			\
} while (0)


static inline int __mutex_fastpath_lock_retval(atomic_t *count,
					       int (*fail_fn)(atomic_t *))
{
	if (unlikely(atomic_dec_return(count) < 0))
		return fail_fn(count);
	else
		return 0;
}

#define __mutex_fastpath_unlock(count, fail_fn)			\
do {								\
	unsigned int dummy;					\
								\
	typecheck(atomic_t *, count);				\
	typecheck_fn(void (*)(atomic_t *), fail_fn);		\
								\
	asm volatile(LOCK_PREFIX "   incl (%%eax)\n"		\
		     "   jg	1f\n"				\
		     "   call " #fail_fn "\n"			\
		     "1:\n"					\
		     : "=a" (dummy)				\
		     : "a" (count)				\
		     : "memory", "ecx", "edx");			\
} while (0)

#define __mutex_slowpath_needs_to_unlock()	1

static inline int __mutex_fastpath_trylock(atomic_t *count,
					   int (*fail_fn)(atomic_t *))
{
#ifdef __HAVE_ARCH_CMPXCHG
	if (likely(atomic_cmpxchg(count, 1, 0) == 1))
		return 1;
	return 0;
#else
	return fail_fn(count);
#endif
}

#endif 
