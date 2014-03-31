/*
 * include/asm-xtensa/atomic.h
 *
 * Atomic operations that C can't guarantee us.  Useful for resource counting..
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_ATOMIC_H
#define _XTENSA_ATOMIC_H

#include <linux/stringify.h>
#include <linux/types.h>

#ifdef __KERNEL__
#include <asm/processor.h>
#include <asm/cmpxchg.h>

#define ATOMIC_INIT(i)	{ (i) }


#define atomic_read(v)		(*(volatile int *)&(v)->counter)

#define atomic_set(v,i)		((v)->counter = (i))

static inline void atomic_add(int i, atomic_t * v)
{
    unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15, "__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0              \n\t"
	"add     %0, %0, %1             \n\t"
	"s32i    %0, %2, 0              \n\t"
	"wsr     a15, "__stringify(PS)"       \n\t"
	"rsync                          \n"
	: "=&a" (vval)
	: "a" (i), "a" (v)
	: "a15", "memory"
	);
}

static inline void atomic_sub(int i, atomic_t *v)
{
    unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15, "__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0              \n\t"
	"sub     %0, %0, %1             \n\t"
	"s32i    %0, %2, 0              \n\t"
	"wsr     a15, "__stringify(PS)"       \n\t"
	"rsync                          \n"
	: "=&a" (vval)
	: "a" (i), "a" (v)
	: "a15", "memory"
	);
}


static inline int atomic_add_return(int i, atomic_t * v)
{
     unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15,"__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0             \n\t"
	"add     %0, %0, %1            \n\t"
	"s32i    %0, %2, 0             \n\t"
	"wsr     a15, "__stringify(PS)"      \n\t"
	"rsync                         \n"
	: "=&a" (vval)
	: "a" (i), "a" (v)
	: "a15", "memory"
	);

    return vval;
}

static inline int atomic_sub_return(int i, atomic_t * v)
{
    unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15,"__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0             \n\t"
	"sub     %0, %0, %1            \n\t"
	"s32i    %0, %2, 0             \n\t"
	"wsr     a15, "__stringify(PS)"       \n\t"
	"rsync                         \n"
	: "=&a" (vval)
	: "a" (i), "a" (v)
	: "a15", "memory"
	);

    return vval;
}

#define atomic_sub_and_test(i,v) (atomic_sub_return((i),(v)) == 0)

#define atomic_inc(v) atomic_add(1,(v))

#define atomic_inc_return(v) atomic_add_return(1,(v))

#define atomic_dec(v) atomic_sub(1,(v))

#define atomic_dec_return(v) atomic_sub_return(1,(v))

#define atomic_dec_and_test(v) (atomic_sub_return(1,(v)) == 0)

#define atomic_inc_and_test(v) (atomic_add_return(1,(v)) == 0)

#define atomic_add_negative(i,v) (atomic_add_return((i),(v)) < 0)

#define atomic_cmpxchg(v, o, n) ((int)cmpxchg(&((v)->counter), (o), (n)))
#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

static __inline__ int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c;
}


static inline void atomic_clear_mask(unsigned int mask, atomic_t *v)
{
    unsigned int all_f = -1;
    unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15,"__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0             \n\t"
	"xor     %1, %4, %3            \n\t"
	"and     %0, %0, %4            \n\t"
	"s32i    %0, %2, 0             \n\t"
	"wsr     a15, "__stringify(PS)"      \n\t"
	"rsync                         \n"
	: "=&a" (vval), "=a" (mask)
	: "a" (v), "a" (all_f), "1" (mask)
	: "a15", "memory"
	);
}

static inline void atomic_set_mask(unsigned int mask, atomic_t *v)
{
    unsigned int vval;

    __asm__ __volatile__(
	"rsil    a15,"__stringify(LOCKLEVEL)"\n\t"
	"l32i    %0, %2, 0             \n\t"
	"or      %0, %0, %1            \n\t"
	"s32i    %0, %2, 0             \n\t"
	"wsr     a15, "__stringify(PS)"       \n\t"
	"rsync                         \n"
	: "=&a" (vval)
	: "a" (mask), "a" (v)
	: "a15", "memory"
	);
}

#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif 

#endif 

