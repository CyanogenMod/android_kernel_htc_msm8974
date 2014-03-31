/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * Do not include directly; use <linux/atomic.h>.
 */

#ifndef _ASM_TILE_ATOMIC_32_H
#define _ASM_TILE_ATOMIC_32_H

#include <asm/barrier.h>
#include <arch/chip.h>

#ifndef __ASSEMBLY__

int _atomic_xchg(atomic_t *v, int n);
int _atomic_xchg_add(atomic_t *v, int i);
int _atomic_xchg_add_unless(atomic_t *v, int a, int u);
int _atomic_cmpxchg(atomic_t *v, int o, int n);

static inline int atomic_xchg(atomic_t *v, int n)
{
	smp_mb();  
	return _atomic_xchg(v, n);
}

static inline int atomic_cmpxchg(atomic_t *v, int o, int n)
{
	smp_mb();  
	return _atomic_cmpxchg(v, o, n);
}

static inline void atomic_add(int i, atomic_t *v)
{
	_atomic_xchg_add(v, i);
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	smp_mb();  
	return _atomic_xchg_add(v, i) + i;
}

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	smp_mb();  
	return _atomic_xchg_add_unless(v, a, u);
}

static inline void atomic_set(atomic_t *v, int n)
{
	_atomic_xchg(v, n);
}


typedef struct {
	u64 __aligned(8) counter;
} atomic64_t;

#define ATOMIC64_INIT(val) { (val) }

u64 _atomic64_xchg(atomic64_t *v, u64 n);
u64 _atomic64_xchg_add(atomic64_t *v, u64 i);
u64 _atomic64_xchg_add_unless(atomic64_t *v, u64 a, u64 u);
u64 _atomic64_cmpxchg(atomic64_t *v, u64 o, u64 n);

static inline u64 atomic64_read(const atomic64_t *v)
{
	return _atomic64_xchg_add((atomic64_t *)v, 0);
}

static inline u64 atomic64_xchg(atomic64_t *v, u64 n)
{
	smp_mb();  
	return _atomic64_xchg(v, n);
}

static inline u64 atomic64_cmpxchg(atomic64_t *v, u64 o, u64 n)
{
	smp_mb();  
	return _atomic64_cmpxchg(v, o, n);
}

static inline void atomic64_add(u64 i, atomic64_t *v)
{
	_atomic64_xchg_add(v, i);
}

static inline u64 atomic64_add_return(u64 i, atomic64_t *v)
{
	smp_mb();  
	return _atomic64_xchg_add(v, i) + i;
}

static inline u64 atomic64_add_unless(atomic64_t *v, u64 a, u64 u)
{
	smp_mb();  
	return _atomic64_xchg_add_unless(v, a, u) != u;
}

static inline void atomic64_set(atomic64_t *v, u64 n)
{
	_atomic64_xchg(v, n);
}

#define atomic64_add_negative(a, v)	(atomic64_add_return((a), (v)) < 0)
#define atomic64_inc(v)			atomic64_add(1LL, (v))
#define atomic64_inc_return(v)		atomic64_add_return(1LL, (v))
#define atomic64_inc_and_test(v)	(atomic64_inc_return(v) == 0)
#define atomic64_sub_return(i, v)	atomic64_add_return(-(i), (v))
#define atomic64_sub_and_test(a, v)	(atomic64_sub_return((a), (v)) == 0)
#define atomic64_sub(i, v)		atomic64_add(-(i), (v))
#define atomic64_dec(v)			atomic64_sub(1LL, (v))
#define atomic64_dec_return(v)		atomic64_sub_return(1LL, (v))
#define atomic64_dec_and_test(v)	(atomic64_dec_return((v)) == 0)
#define atomic64_inc_not_zero(v)	atomic64_add_unless((v), 1LL, 0LL)

#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_dec()	do { } while (0)
#define smp_mb__after_atomic_inc()	do { } while (0)

#endif 


#define ATOMIC_LOCKS_FOUND_VIA_TABLE() \
  (!CHIP_HAS_CBOX_HOME_MAP() && defined(CONFIG_SMP))

#if ATOMIC_LOCKS_FOUND_VIA_TABLE()

#define ATOMIC_HASH_L1_SHIFT 6
#define ATOMIC_HASH_L1_SIZE (1 << ATOMIC_HASH_L1_SHIFT)

#define ATOMIC_HASH_L2_SHIFT (CHIP_L2_LOG_LINE_SIZE() - 2)
#define ATOMIC_HASH_L2_SIZE (1 << ATOMIC_HASH_L2_SHIFT)

#else 

#define ATOMIC_HASH_SHIFT (PAGE_SHIFT - 3)
#define ATOMIC_HASH_SIZE (1 << ATOMIC_HASH_SHIFT)

#ifndef __ASSEMBLY__
extern int atomic_locks[];
#endif

#endif 

#define ATOMIC_LOCK_REG 20
#define ATOMIC_LOCK_REG_NAME r20

#ifndef __ASSEMBLY__
void __init_atomic_per_cpu(void);

#ifdef CONFIG_SMP
void __atomic_fault_unlock(int *lock_ptr);
#endif

extern struct __get_user __atomic_cmpxchg(volatile int *p,
					  int *lock, int o, int n);
extern struct __get_user __atomic_xchg(volatile int *p, int *lock, int n);
extern struct __get_user __atomic_xchg_add(volatile int *p, int *lock, int n);
extern struct __get_user __atomic_xchg_add_unless(volatile int *p,
						  int *lock, int o, int n);
extern struct __get_user __atomic_or(volatile int *p, int *lock, int n);
extern struct __get_user __atomic_andn(volatile int *p, int *lock, int n);
extern struct __get_user __atomic_xor(volatile int *p, int *lock, int n);
extern u64 __atomic64_cmpxchg(volatile u64 *p, int *lock, u64 o, u64 n);
extern u64 __atomic64_xchg(volatile u64 *p, int *lock, u64 n);
extern u64 __atomic64_xchg_add(volatile u64 *p, int *lock, u64 n);
extern u64 __atomic64_xchg_add_unless(volatile u64 *p,
				      int *lock, u64 o, u64 n);

#endif 

#endif 
