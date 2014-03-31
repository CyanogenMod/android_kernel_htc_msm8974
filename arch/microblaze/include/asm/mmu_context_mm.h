/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_MMU_CONTEXT_H
#define _ASM_MICROBLAZE_MMU_CONTEXT_H

#include <linux/atomic.h>
#include <asm/bitops.h>
#include <asm/mmu.h>
#include <asm-generic/mm_hooks.h>

# ifdef __KERNEL__
# define CTX_TO_VSID(ctx, va)	(((ctx) * (897 * 16) + ((va) >> 28) * 0x111) \
				 & 0xffffff)


static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

# define NO_CONTEXT	256
# define LAST_CONTEXT	255
# define FIRST_CONTEXT	1

extern void set_context(mm_context_t context, pgd_t *pgd);

extern unsigned long context_map[];

extern mm_context_t next_mmu_context;

extern atomic_t nr_free_contexts;
extern struct mm_struct *context_mm[LAST_CONTEXT+1];
extern void steal_context(void);

static inline void get_mmu_context(struct mm_struct *mm)
{
	mm_context_t ctx;

	if (mm->context != NO_CONTEXT)
		return;
	while (atomic_dec_if_positive(&nr_free_contexts) < 0)
		steal_context();
	ctx = next_mmu_context;
	while (test_and_set_bit(ctx, context_map)) {
		ctx = find_next_zero_bit(context_map, LAST_CONTEXT+1, ctx);
		if (ctx > LAST_CONTEXT)
			ctx = 0;
	}
	next_mmu_context = (ctx + 1) & LAST_CONTEXT;
	mm->context = ctx;
	context_mm[ctx] = mm;
}

# define init_new_context(tsk, mm)	(((mm)->context = NO_CONTEXT), 0)

static inline void destroy_context(struct mm_struct *mm)
{
	if (mm->context != NO_CONTEXT) {
		clear_bit(mm->context, context_map);
		mm->context = NO_CONTEXT;
		atomic_inc(&nr_free_contexts);
	}
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	tsk->thread.pgdir = next->pgd;
	get_mmu_context(next);
	set_context(next->context, next->pgd);
}

static inline void activate_mm(struct mm_struct *active_mm,
			struct mm_struct *mm)
{
	current->thread.pgdir = mm->pgd;
	get_mmu_context(mm);
	set_context(mm->context, mm->pgd);
}

extern void mmu_context_init(void);

# endif 
#endif 
