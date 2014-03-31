/*
 * MM context support for the Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_MMU_CONTEXT_H
#define _ASM_MMU_CONTEXT_H

#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/mem-layout.h>

static inline void destroy_context(struct mm_struct *mm)
{
}

static inline void enter_lazy_tlb(struct mm_struct *mm,
	struct task_struct *tsk)
{
}

static inline void deactivate_mm(struct task_struct *tsk,
	struct mm_struct *mm)
{
}

static inline int init_new_context(struct task_struct *tsk,
					struct mm_struct *mm)
{
	
	return 0;
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
				struct task_struct *tsk)
{
	int l1;

	if (next->context.generation < prev->context.generation) {
		for (l1 = MIN_KERNEL_SEG; l1 <= max_kernel_seg; l1++)
			next->pgd[l1] = init_mm.pgd[l1];

		next->context.generation = prev->context.generation;
	}

	__vmnewmap((void *)next->context.ptbase);
}

static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned long flags;

	local_irq_save(flags);
	switch_mm(prev, next, current_thread_info()->task);
	local_irq_restore(flags);
}

#include <asm-generic/mm_hooks.h>

#endif
