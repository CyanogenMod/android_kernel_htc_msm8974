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

#ifndef _ASM_TILE_MMU_CONTEXT_H
#define _ASM_TILE_MMU_CONTEXT_H

#include <linux/smp.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/homecache.h>
#include <asm-generic/mm_hooks.h>

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	return 0;
}

static inline void __install_page_table(pgd_t *pgdir, int asid, pgprot_t prot)
{
	
	int rc = hv_install_context(__pa(pgdir), prot, asid, HV_CTX_DIRECTIO);
	if (rc < 0)
		panic("hv_install_context failed: %d", rc);
}

static inline void install_page_table(pgd_t *pgdir, int asid)
{
	pte_t *ptep = virt_to_pte(NULL, (unsigned long)pgdir);
	__install_page_table(pgdir, asid, *ptep);
}

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *t)
{
#if CHIP_HAS_TILE_DMA()
	if (current->thread.tile_dma_state.enabled)
		install_page_table(mm->pgd, __get_cpu_var(current_asid));
#endif
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	if (likely(prev != next)) {

		int cpu = smp_processor_id();

		
		int asid = __get_cpu_var(current_asid) + 1;
		if (asid > max_asid) {
			asid = min_asid;
			local_flush_tlb();
		}
		__get_cpu_var(current_asid) = asid;

		
		cpumask_clear_cpu(cpu, mm_cpumask(prev));
		cpumask_set_cpu(cpu, mm_cpumask(next));

		
		install_page_table(next->pgd, asid);

		
		check_mm_caching(prev, next);

		__flush_icache();

	}
}

static inline void activate_mm(struct mm_struct *prev_mm,
			       struct mm_struct *next_mm)
{
	switch_mm(prev_mm, next_mm, NULL);
}

#define destroy_context(mm)		do { } while (0)
#define deactivate_mm(tsk, mm)          do { } while (0)

#endif 
