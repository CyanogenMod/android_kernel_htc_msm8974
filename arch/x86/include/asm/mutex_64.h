/*
 * Assembly implementation of the mutex fastpath, based on atomic
 * decrement/increment.
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#ifndef _ASM_X86_MUTEX_64_H
#define _ASM_X86_MUTEX_64_H

#define __mutex_fastpath_lock(v, fail_fn)			\
do {								\
	unsigned long dummy;					\
								\
	typecheck(atomic_t *, v);				\
	typecheck_fn(void (*)(atomic_t *), fail_fn);		\
								\
	asm volatile(LOCK_PREFIX "   decl (%%rdi)\n"		\
		     "   jns 1f		\n"			\
		     "   call " #fail_fn "\n"			\
		     "1:"					\
		     : "=D" (dummy)				\
		     : "D" (v)					\
		     : "rax", "rsi", "rdx", "rcx",		\
		       "r8", "r9", "r10", "r11", "memory");	\
} while (0)

static inline int __mutex_fastpath_lock_retval(atomic_t *count,
					       int (*fail_fn)(atomic_t *))
{
	if (unlikely(atomic_dec_return(count) < 0))
		return fail_fn(count);
	else
		return 0;
}

#define __mutex_fastpath_unlock(v, fail_fn)			\
do {								\
	unsigned long dummy;					\
								\
	typecheck(atomic_t *, v);				\
	typecheck_fn(void (*)(atomic_t *), fail_fn);		\
								\
	asm volatile(LOCK_PREFIX "   incl (%%rdi)\n"		\
		     "   jg 1f\n"				\
		     "   call " #fail_fn "\n"			\
		     "1:"					\
		     : "=D" (dummy)				\
		     : "D" (v)					\
		     : "rax", "rsi", "rdx", "rcx",		\
		       "r8", "r9", "r10", "r11", "memory");	\
} while (0)

#define __mutex_slowpath_needs_to_unlock()	1

static inline int __mutex_fastpath_trylock(atomic_t *count,
					   int (*fail_fn)(atomic_t *))
{
	if (likely(atomic_cmpxchg(count, 1, 0) == 1))
		return 1;
	else
		return 0;
}

#endif 
