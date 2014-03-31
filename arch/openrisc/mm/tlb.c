/*
 * OpenRISC tlb.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Julius Baxter <julius.baxter@orsoc.se>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/segment.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/spr_defs.h>

#define NO_CONTEXT -1

#define NUM_DTLB_SETS (1 << ((mfspr(SPR_IMMUCFGR) & SPR_IMMUCFGR_NTS) >> \
			    SPR_DMMUCFGR_NTS_OFF))
#define NUM_ITLB_SETS (1 << ((mfspr(SPR_IMMUCFGR) & SPR_IMMUCFGR_NTS) >> \
			    SPR_IMMUCFGR_NTS_OFF))
#define DTLB_OFFSET(addr) (((addr) >> PAGE_SHIFT) & (NUM_DTLB_SETS-1))
#define ITLB_OFFSET(addr) (((addr) >> PAGE_SHIFT) & (NUM_ITLB_SETS-1))

void flush_tlb_all(void)
{
	int i;
	unsigned long num_tlb_sets;

	
	
	num_tlb_sets = NUM_ITLB_SETS;

	for (i = 0; i < num_tlb_sets; i++) {
		mtspr_off(SPR_DTLBMR_BASE(0), i, 0);
		mtspr_off(SPR_ITLBMR_BASE(0), i, 0);
	}
}

#define have_dtlbeir (mfspr(SPR_DMMUCFGR) & SPR_DMMUCFGR_TEIRI)
#define have_itlbeir (mfspr(SPR_IMMUCFGR) & SPR_IMMUCFGR_TEIRI)


#define flush_dtlb_page_eir(addr) mtspr(SPR_DTLBEIR, addr)
#define flush_dtlb_page_no_eir(addr) \
	mtspr_off(SPR_DTLBMR_BASE(0), DTLB_OFFSET(addr), 0);

#define flush_itlb_page_eir(addr) mtspr(SPR_ITLBEIR, addr)
#define flush_itlb_page_no_eir(addr) \
	mtspr_off(SPR_ITLBMR_BASE(0), ITLB_OFFSET(addr), 0);

void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	if (have_dtlbeir)
		flush_dtlb_page_eir(addr);
	else
		flush_dtlb_page_no_eir(addr);

	if (have_itlbeir)
		flush_itlb_page_eir(addr);
	else
		flush_itlb_page_no_eir(addr);
}

void flush_tlb_range(struct vm_area_struct *vma,
		     unsigned long start, unsigned long end)
{
	int addr;
	bool dtlbeir;
	bool itlbeir;

	dtlbeir = have_dtlbeir;
	itlbeir = have_itlbeir;

	for (addr = start; addr < end; addr += PAGE_SIZE) {
		if (dtlbeir)
			flush_dtlb_page_eir(addr);
		else
			flush_dtlb_page_no_eir(addr);

		if (itlbeir)
			flush_itlb_page_eir(addr);
		else
			flush_itlb_page_no_eir(addr);
	}
}


void flush_tlb_mm(struct mm_struct *mm)
{

	
	flush_tlb_all();
}


void switch_mm(struct mm_struct *prev, struct mm_struct *next,
	       struct task_struct *next_tsk)
{
	current_pgd = next->pgd;


	if (prev != next)
		flush_tlb_mm(prev);

}


int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context = NO_CONTEXT;
	return 0;
}


void destroy_context(struct mm_struct *mm)
{
	flush_tlb_mm(mm);

}


void __init tlb_init(void)
{
	
	
	
}
