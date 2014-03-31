/*
 * include/asm-xtensa/mmu_context.h
 *
 * Switch an MMU context.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_MMU_CONTEXT_H
#define _XTENSA_MMU_CONTEXT_H

#ifndef CONFIG_MMU
#include <asm/nommu_context.h>
#else

#include <linux/stringify.h>
#include <linux/sched.h>

#include <variant/core.h>

#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm-generic/mm_hooks.h>

#if (XCHAL_HAVE_TLBS != 1)
# error "Linux must have an MMU!"
#endif

extern unsigned long asid_cache;


#define NO_CONTEXT	0
#define ASID_USER_FIRST	4
#define ASID_MASK	((1 << XCHAL_MMU_ASID_BITS) - 1)
#define ASID_INSERT(x)	(0x03020001 | (((x) & ASID_MASK) << 8))

static inline void set_rasid_register (unsigned long val)
{
	__asm__ __volatile__ (" wsr %0, "__stringify(RASID)"\n\t"
			      " isync\n" : : "a" (val));
}

static inline unsigned long get_rasid_register (void)
{
	unsigned long tmp;
	__asm__ __volatile__ (" rsr %0,"__stringify(RASID)"\n\t" : "=a" (tmp));
	return tmp;
}

static inline void
__get_new_mmu_context(struct mm_struct *mm)
{
	extern void flush_tlb_all(void);
	if (! (++asid_cache & ASID_MASK) ) {
		flush_tlb_all(); 
		asid_cache += ASID_USER_FIRST;
	}
	mm->context = asid_cache;
}

static inline void
__load_mmu_context(struct mm_struct *mm)
{
	set_rasid_register(ASID_INSERT(mm->context));
	invalidate_page_directory();
}


static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context = NO_CONTEXT;
	return 0;
}

static inline void
activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	

	__get_new_mmu_context(next);
	__load_mmu_context(next);
}


static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
                             struct task_struct *tsk)
{
	unsigned long asid = asid_cache;

	

	if (next->context == NO_CONTEXT || ((next->context^asid) & ~ASID_MASK))
		__get_new_mmu_context(next);

	__load_mmu_context(next);
}

#define deactivate_mm(tsk, mm)	do { } while(0)

static inline void destroy_context(struct mm_struct *mm)
{
	invalidate_page_directory();
}


static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	

}

#endif 
#endif 
