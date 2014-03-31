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
 * 32-bit SMP spinlocks.
 */

#ifndef _ASM_TILE_SPINLOCK_32_H
#define _ASM_TILE_SPINLOCK_32_H

#include <linux/atomic.h>
#include <asm/page.h>
#include <linux/compiler.h>

#define TICKET_QUANTUM 2


static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	return lock->next_ticket != lock->current_ticket;
}

void arch_spin_lock(arch_spinlock_t *lock);

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)

int arch_spin_trylock(arch_spinlock_t *lock);

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	
	int old_ticket = lock->current_ticket;
	wmb();  
	lock->current_ticket = old_ticket + TICKET_QUANTUM;
}

void arch_spin_unlock_wait(arch_spinlock_t *lock);


#define _WR_NEXT_SHIFT	8
#define _WR_CURR_SHIFT  16
#define _WR_WIDTH       8
#define _RD_COUNT_SHIFT 24
#define _RD_COUNT_WIDTH 8

static inline int arch_read_can_lock(arch_rwlock_t *rwlock)
{
	return (rwlock->lock << _RD_COUNT_WIDTH) == 0;
}

static inline int arch_write_can_lock(arch_rwlock_t *rwlock)
{
	return rwlock->lock == 0;
}

void arch_read_lock(arch_rwlock_t *rwlock);

void arch_write_lock(arch_rwlock_t *rwlock);

int arch_read_trylock(arch_rwlock_t *rwlock);

int arch_write_trylock(arch_rwlock_t *rwlock);

void arch_read_unlock(arch_rwlock_t *rwlock);

void arch_write_unlock(arch_rwlock_t *rwlock);

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#endif 
