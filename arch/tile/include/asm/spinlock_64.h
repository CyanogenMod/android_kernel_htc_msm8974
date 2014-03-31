/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
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
 * 64-bit SMP ticket spinlocks, allowing only a single CPU anywhere
 * (the type definitions are in asm/spinlock_types.h)
 */

#ifndef _ASM_TILE_SPINLOCK_64_H
#define _ASM_TILE_SPINLOCK_64_H

#define __ARCH_SPIN_CURRENT_SHIFT	17
#define __ARCH_SPIN_NEXT_MASK		0x7fff
#define __ARCH_SPIN_NEXT_OVERFLOW	0x8000

static inline int arch_spin_current(u32 val)
{
	return val >> __ARCH_SPIN_CURRENT_SHIFT;
}

static inline int arch_spin_next(u32 val)
{
	return val & __ARCH_SPIN_NEXT_MASK;
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	u32 val = lock->lock;
	return arch_spin_current(val) != arch_spin_next(val);
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	wmb();  
	__insn_fetchadd4(&lock->lock, 1U << __ARCH_SPIN_CURRENT_SHIFT);
}

void arch_spin_unlock_wait(arch_spinlock_t *lock);

void arch_spin_lock_slow(arch_spinlock_t *lock, u32 val);

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	u32 val = __insn_fetchadd4(&lock->lock, 1);
	u32 ticket = val & (__ARCH_SPIN_NEXT_MASK | __ARCH_SPIN_NEXT_OVERFLOW);
	if (unlikely(arch_spin_current(val) != ticket))
		arch_spin_lock_slow(lock, ticket);
}

int arch_spin_trylock(arch_spinlock_t *lock);

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)


#define __WRITE_LOCK_BIT (1 << 31)

static inline int arch_write_val_locked(int val)
{
	return val < 0;  
}

static inline int arch_read_can_lock(arch_rwlock_t *rw)
{
	return !arch_write_val_locked(rw->lock);
}

static inline int arch_write_can_lock(arch_rwlock_t *rw)
{
	return rw->lock == 0;
}

extern void __read_lock_failed(arch_rwlock_t *rw);

static inline void arch_read_lock(arch_rwlock_t *rw)
{
	u32 val = __insn_fetchaddgez4(&rw->lock, 1);
	if (unlikely(arch_write_val_locked(val)))
		__read_lock_failed(rw);
}

extern void __write_lock_failed(arch_rwlock_t *rw, u32 val);

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	u32 val = __insn_fetchor4(&rw->lock, __WRITE_LOCK_BIT);
	if (unlikely(val != 0))
		__write_lock_failed(rw, val);
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	__insn_mf();
	__insn_fetchadd4(&rw->lock, -1);
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	__insn_mf();
	__insn_exch4(&rw->lock, 0);  
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	return !arch_write_val_locked(__insn_fetchaddgez4(&rw->lock, 1));
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	u32 val = __insn_fetchor4(&rw->lock, __WRITE_LOCK_BIT);
	if (likely(val == 0))
		return 1;
	if (!arch_write_val_locked(val))
		__insn_fetchand4(&rw->lock, ~__WRITE_LOCK_BIT);
	return 0;
}

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#endif 
