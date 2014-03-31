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

#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/processor.h>
#include <arch/spr_def.h>

#include "spinlock_common.h"

void arch_spin_lock(arch_spinlock_t *lock)
{
	int my_ticket;
	int iterations = 0;
	int delta;

	while ((my_ticket = __insn_tns((void *)&lock->next_ticket)) & 1)
		delay_backoff(iterations++);

	
	lock->next_ticket = my_ticket + TICKET_QUANTUM;

	
	while ((delta = my_ticket - lock->current_ticket) != 0)
		relax((128 / CYCLES_PER_RELAX_LOOP) * delta);
}
EXPORT_SYMBOL(arch_spin_lock);

int arch_spin_trylock(arch_spinlock_t *lock)
{
	int my_ticket = __insn_tns((void *)&lock->next_ticket);

	if (my_ticket == lock->current_ticket) {
		
		lock->next_ticket = my_ticket + TICKET_QUANTUM;
		
		return 1;
	}

	if (!(my_ticket & 1)) {
		
		lock->next_ticket = my_ticket;
	}

	return 0;
}
EXPORT_SYMBOL(arch_spin_trylock);

void arch_spin_unlock_wait(arch_spinlock_t *lock)
{
	u32 iterations = 0;
	while (arch_spin_is_locked(lock))
		delay_backoff(iterations++);
}
EXPORT_SYMBOL(arch_spin_unlock_wait);

#define WR_NEXT_SHIFT   _WR_NEXT_SHIFT
#define WR_CURR_SHIFT   _WR_CURR_SHIFT
#define WR_WIDTH        _WR_WIDTH
#define WR_MASK         ((1 << WR_WIDTH) - 1)

#define RD_COUNT_SHIFT  _RD_COUNT_SHIFT
#define RD_COUNT_WIDTH  _RD_COUNT_WIDTH
#define RD_COUNT_MASK   ((1 << RD_COUNT_WIDTH) - 1)


inline int arch_read_trylock(arch_rwlock_t *rwlock)
{
	u32 val;
	__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 1);
	val = __insn_tns((int *)&rwlock->lock);
	if (likely((val << _RD_COUNT_WIDTH) == 0)) {
		val += 1 << RD_COUNT_SHIFT;
		rwlock->lock = val;
		__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 0);
		BUG_ON(val == 0);  
		return 1;
	}
	if ((val & 1) == 0)
		rwlock->lock = val;
	__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 0);
	return 0;
}
EXPORT_SYMBOL(arch_read_trylock);

void arch_read_lock(arch_rwlock_t *rwlock)
{
	u32 iterations = 0;
	while (unlikely(!arch_read_trylock(rwlock)))
		delay_backoff(iterations++);
}
EXPORT_SYMBOL(arch_read_lock);

void arch_read_unlock(arch_rwlock_t *rwlock)
{
	u32 val, iterations = 0;

	mb();  
	for (;;) {
		__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 1);
		val = __insn_tns((int *)&rwlock->lock);
		if (likely((val & 1) == 0)) {
			rwlock->lock = val - (1 << _RD_COUNT_SHIFT);
			__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 0);
			break;
		}
		__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 0);
		delay_backoff(iterations++);
	}
}
EXPORT_SYMBOL(arch_read_unlock);

void arch_write_lock(arch_rwlock_t *rwlock)
{
	u32 my_ticket_;
	u32 iterations = 0;
	u32 val = __insn_tns((int *)&rwlock->lock);

	if (likely(val == 0)) {
		rwlock->lock = 1 << _WR_NEXT_SHIFT;
		return;
	}

	for (;;) {
		if (!(val & 1)) {
			if ((val >> RD_COUNT_SHIFT) == 0)
				break;
			rwlock->lock = val;
		}
		delay_backoff(iterations++);
		val = __insn_tns((int *)&rwlock->lock);
	}

	
	rwlock->lock = __insn_addb(val, 1 << WR_NEXT_SHIFT);
	my_ticket_ = val >> WR_NEXT_SHIFT;

	
	for (;;) {
		u32 curr_ = val >> WR_CURR_SHIFT;
		u32 delta = ((my_ticket_ - curr_) & WR_MASK);
		if (likely(delta == 0))
			break;

		
		relax((256 / CYCLES_PER_RELAX_LOOP) * delta);

		while ((val = rwlock->lock) & 1)
			relax(4);
	}
}
EXPORT_SYMBOL(arch_write_lock);

int arch_write_trylock(arch_rwlock_t *rwlock)
{
	u32 val = __insn_tns((int *)&rwlock->lock);

	if (unlikely(val != 0)) {
		if (!(val & 1))
			rwlock->lock = val;
		return 0;
	}

	
	rwlock->lock = 1 << _WR_NEXT_SHIFT;
	return 1;
}
EXPORT_SYMBOL(arch_write_trylock);

void arch_write_unlock(arch_rwlock_t *rwlock)
{
	u32 val, eq, mask;

	mb();  
	val = __insn_tns((int *)&rwlock->lock);
	if (likely(val == (1 << _WR_NEXT_SHIFT))) {
		rwlock->lock = 0;
		return;
	}
	while (unlikely(val & 1)) {
		
		relax(4);
		val = __insn_tns((int *)&rwlock->lock);
	}
	mask = 1 << WR_CURR_SHIFT;
	val = __insn_addb(val, mask);
	eq = __insn_seqb(val, val << (WR_CURR_SHIFT - WR_NEXT_SHIFT));
	val = __insn_mz(eq & mask, val);
	rwlock->lock = val;
}
EXPORT_SYMBOL(arch_write_unlock);
