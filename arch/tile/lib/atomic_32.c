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
 */

#include <linux/cache.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <asm/futex.h>
#include <arch/chip.h>

#if ATOMIC_LOCKS_FOUND_VIA_TABLE()

struct atomic_locks_on_cpu {
	int lock[ATOMIC_HASH_L2_SIZE];
} __attribute__((aligned(ATOMIC_HASH_L2_SIZE * 4)));

static DEFINE_PER_CPU(struct atomic_locks_on_cpu, atomic_lock_pool);

static struct atomic_locks_on_cpu __initdata initial_atomic_locks;

struct atomic_locks_on_cpu *atomic_lock_ptr[ATOMIC_HASH_L1_SIZE]
	__write_once = {
	[0 ... ATOMIC_HASH_L1_SIZE-1] (&initial_atomic_locks)
};

#else 

int atomic_locks[PAGE_SIZE / sizeof(int)] __page_aligned_bss;

#endif 

static inline int *__atomic_hashed_lock(volatile void *v)
{
	
#if ATOMIC_LOCKS_FOUND_VIA_TABLE()
	unsigned long i =
		(unsigned long) v & ((PAGE_SIZE-1) & -sizeof(long long));
	unsigned long n = __insn_crc32_32(0, i);

	
	unsigned long l1_index = n >> ((sizeof(n) * 8) - ATOMIC_HASH_L1_SHIFT);
	
	unsigned long l2_index = n & (ATOMIC_HASH_L2_SIZE - 1);

	return &atomic_lock_ptr[l1_index]->lock[l2_index];
#else
	unsigned long ptr = __insn_mm((unsigned long)v >> 1,
				      (unsigned long)atomic_locks,
				      2, (ATOMIC_HASH_SHIFT + 2) - 1);
	return (int *)ptr;
#endif
}

#ifdef CONFIG_SMP
static int is_atomic_lock(int *p)
{
#if ATOMIC_LOCKS_FOUND_VIA_TABLE()
	int i;
	for (i = 0; i < ATOMIC_HASH_L1_SIZE; ++i) {

		if (p >= &atomic_lock_ptr[i]->lock[0] &&
		    p < &atomic_lock_ptr[i]->lock[ATOMIC_HASH_L2_SIZE]) {
			return 1;
		}
	}
	return 0;
#else
	return p >= &atomic_locks[0] && p < &atomic_locks[ATOMIC_HASH_SIZE];
#endif
}

void __atomic_fault_unlock(int *irqlock_word)
{
	BUG_ON(!is_atomic_lock(irqlock_word));
	BUG_ON(*irqlock_word != 1);
	*irqlock_word = 0;
}

#endif 

static inline int *__atomic_setup(volatile void *v)
{
	
	*(volatile int *)v;
	return __atomic_hashed_lock(v);
}

int _atomic_xchg(atomic_t *v, int n)
{
	return __atomic_xchg(&v->counter, __atomic_setup(v), n).val;
}
EXPORT_SYMBOL(_atomic_xchg);

int _atomic_xchg_add(atomic_t *v, int i)
{
	return __atomic_xchg_add(&v->counter, __atomic_setup(v), i).val;
}
EXPORT_SYMBOL(_atomic_xchg_add);

int _atomic_xchg_add_unless(atomic_t *v, int a, int u)
{
	return __atomic_xchg_add_unless(&v->counter, __atomic_setup(v), u, a)
		.val;
}
EXPORT_SYMBOL(_atomic_xchg_add_unless);

int _atomic_cmpxchg(atomic_t *v, int o, int n)
{
	return __atomic_cmpxchg(&v->counter, __atomic_setup(v), o, n).val;
}
EXPORT_SYMBOL(_atomic_cmpxchg);

unsigned long _atomic_or(volatile unsigned long *p, unsigned long mask)
{
	return __atomic_or((int *)p, __atomic_setup(p), mask).val;
}
EXPORT_SYMBOL(_atomic_or);

unsigned long _atomic_andn(volatile unsigned long *p, unsigned long mask)
{
	return __atomic_andn((int *)p, __atomic_setup(p), mask).val;
}
EXPORT_SYMBOL(_atomic_andn);

unsigned long _atomic_xor(volatile unsigned long *p, unsigned long mask)
{
	return __atomic_xor((int *)p, __atomic_setup(p), mask).val;
}
EXPORT_SYMBOL(_atomic_xor);


u64 _atomic64_xchg(atomic64_t *v, u64 n)
{
	return __atomic64_xchg(&v->counter, __atomic_setup(v), n);
}
EXPORT_SYMBOL(_atomic64_xchg);

u64 _atomic64_xchg_add(atomic64_t *v, u64 i)
{
	return __atomic64_xchg_add(&v->counter, __atomic_setup(v), i);
}
EXPORT_SYMBOL(_atomic64_xchg_add);

u64 _atomic64_xchg_add_unless(atomic64_t *v, u64 a, u64 u)
{
	return __atomic64_xchg_add_unless(&v->counter, __atomic_setup(v),
					  u, a);
}
EXPORT_SYMBOL(_atomic64_xchg_add_unless);

u64 _atomic64_cmpxchg(atomic64_t *v, u64 o, u64 n)
{
	return __atomic64_cmpxchg(&v->counter, __atomic_setup(v), o, n);
}
EXPORT_SYMBOL(_atomic64_cmpxchg);


static inline int *__futex_setup(int __user *v)
{
	__insn_prefetch(v);
	return __atomic_hashed_lock((int __force *)v);
}

struct __get_user futex_set(u32 __user *v, int i)
{
	return __atomic_xchg((int __force *)v, __futex_setup(v), i);
}

struct __get_user futex_add(u32 __user *v, int n)
{
	return __atomic_xchg_add((int __force *)v, __futex_setup(v), n);
}

struct __get_user futex_or(u32 __user *v, int n)
{
	return __atomic_or((int __force *)v, __futex_setup(v), n);
}

struct __get_user futex_andn(u32 __user *v, int n)
{
	return __atomic_andn((int __force *)v, __futex_setup(v), n);
}

struct __get_user futex_xor(u32 __user *v, int n)
{
	return __atomic_xor((int __force *)v, __futex_setup(v), n);
}

struct __get_user futex_cmpxchg(u32 __user *v, int o, int n)
{
	return __atomic_cmpxchg((int __force *)v, __futex_setup(v), o, n);
}

struct __get_user __atomic_bad_address(int __user *addr)
{
	if (unlikely(!access_ok(VERIFY_WRITE, addr, sizeof(int))))
		panic("Bad address used for kernel atomic op: %p\n", addr);
	return (struct __get_user) { .err = -EFAULT };
}


#if CHIP_HAS_CBOX_HOME_MAP()
static int __init noatomichash(char *str)
{
	pr_warning("noatomichash is deprecated.\n");
	return 1;
}
__setup("noatomichash", noatomichash);
#endif

void __init __init_atomic_per_cpu(void)
{
#if ATOMIC_LOCKS_FOUND_VIA_TABLE()

	unsigned int i;
	int actual_cpu;


	actual_cpu = cpumask_first(cpu_possible_mask);

	for (i = 0; i < ATOMIC_HASH_L1_SIZE; ++i) {
		actual_cpu = cpumask_next(actual_cpu, cpu_possible_mask);
		if (actual_cpu >= nr_cpu_ids)
			actual_cpu = cpumask_first(cpu_possible_mask);

		atomic_lock_ptr[i] = &per_cpu(atomic_lock_pool, actual_cpu);
	}

#else 

	
	BUILD_BUG_ON(ATOMIC_HASH_SIZE & (ATOMIC_HASH_SIZE-1));
	BUG_ON(ATOMIC_HASH_SIZE < nr_cpu_ids);

	BUG_ON((unsigned long)atomic_locks % PAGE_SIZE != 0);

	
	BUILD_BUG_ON(ATOMIC_HASH_SIZE * sizeof(int) > PAGE_SIZE);

	BUILD_BUG_ON((PAGE_SIZE >> 3) > ATOMIC_HASH_SIZE);

#endif 

	
	BUILD_BUG_ON(sizeof(atomic_t) != sizeof(int));
}
