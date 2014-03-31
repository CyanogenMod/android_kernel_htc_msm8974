/*
 * PowerPC64 Segment Translation Support.
 *
 * Dave Engebretsen and Mike Corrigan {engebret|mikejc}@us.ibm.com
 *    Copyright (c) 2001 Dave Engebretsen
 *
 * Copyright (C) 2002 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/memblock.h>

#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/paca.h>
#include <asm/cputable.h>
#include <asm/prom.h>
#include <asm/abs_addr.h>

struct stab_entry {
	unsigned long esid_data;
	unsigned long vsid_data;
};

#define NR_STAB_CACHE_ENTRIES 8
static DEFINE_PER_CPU(long, stab_cache_ptr);
static DEFINE_PER_CPU(long [NR_STAB_CACHE_ENTRIES], stab_cache);

static int make_ste(unsigned long stab, unsigned long esid, unsigned long vsid)
{
	unsigned long esid_data, vsid_data;
	unsigned long entry, group, old_esid, castout_entry, i;
	unsigned int global_entry;
	struct stab_entry *ste, *castout_ste;
	unsigned long kernel_segment = (esid << SID_SHIFT) >= PAGE_OFFSET;

	vsid_data = vsid << STE_VSID_SHIFT;
	esid_data = esid << SID_SHIFT | STE_ESID_KP | STE_ESID_V;
	if (! kernel_segment)
		esid_data |= STE_ESID_KS;

	
	global_entry = (esid & 0x1f) << 3;
	ste = (struct stab_entry *)(stab | ((esid & 0x1f) << 7));

	
	for (group = 0; group < 2; group++) {
		for (entry = 0; entry < 8; entry++, ste++) {
			if (!(ste->esid_data & STE_ESID_V)) {
				ste->vsid_data = vsid_data;
				eieio();
				ste->esid_data = esid_data;
				return (global_entry | entry);
			}
		}
		
		global_entry = ((~esid) & 0x1f) << 3;
		ste = (struct stab_entry *)(stab | (((~esid) & 0x1f) << 7));
	}

	castout_entry = get_paca()->stab_rr;
	for (i = 0; i < 16; i++) {
		if (castout_entry < 8) {
			global_entry = (esid & 0x1f) << 3;
			ste = (struct stab_entry *)(stab | ((esid & 0x1f) << 7));
			castout_ste = ste + castout_entry;
		} else {
			global_entry = ((~esid) & 0x1f) << 3;
			ste = (struct stab_entry *)(stab | (((~esid) & 0x1f) << 7));
			castout_ste = ste + (castout_entry - 8);
		}

		
		if ((castout_ste->esid_data & ESID_MASK) != PAGE_OFFSET)
			break;

		castout_entry = (castout_entry + 1) & 0xf;
	}

	get_paca()->stab_rr = (castout_entry + 1) & 0xf;

	

	
	asm volatile("isync" : : : "memory");

	old_esid = castout_ste->esid_data >> SID_SHIFT;
	castout_ste->esid_data = 0;		

	asm volatile("sync" : : : "memory");    

	castout_ste->vsid_data = vsid_data;
	eieio();				
	castout_ste->esid_data = esid_data;

	asm volatile("slbie  %0" : : "r" (old_esid << SID_SHIFT));
	
	asm volatile("sync" : : : "memory");

	return (global_entry | (castout_entry & 0x7));
}

static int __ste_allocate(unsigned long ea, struct mm_struct *mm)
{
	unsigned long vsid;
	unsigned char stab_entry;
	unsigned long offset;

	
	if (is_kernel_addr(ea)) {
		vsid = get_kernel_vsid(ea, MMU_SEGSIZE_256M);
	} else {
		if ((ea >= TASK_SIZE_USER64) || (! mm))
			return 1;

		vsid = get_vsid(mm->context.id, ea, MMU_SEGSIZE_256M);
	}

	stab_entry = make_ste(get_paca()->stab_addr, GET_ESID(ea), vsid);

	if (!is_kernel_addr(ea)) {
		offset = __get_cpu_var(stab_cache_ptr);
		if (offset < NR_STAB_CACHE_ENTRIES)
			__get_cpu_var(stab_cache[offset++]) = stab_entry;
		else
			offset = NR_STAB_CACHE_ENTRIES+1;
		__get_cpu_var(stab_cache_ptr) = offset;

		
		asm volatile("sync":::"memory");
	}

	return 0;
}

int ste_allocate(unsigned long ea)
{
	return __ste_allocate(ea, current->mm);
}

void switch_stab(struct task_struct *tsk, struct mm_struct *mm)
{
	struct stab_entry *stab = (struct stab_entry *) get_paca()->stab_addr;
	struct stab_entry *ste;
	unsigned long offset;
	unsigned long pc = KSTK_EIP(tsk);
	unsigned long stack = KSTK_ESP(tsk);
	unsigned long unmapped_base;

	
	asm volatile("isync" : : : "memory");

	hard_irq_disable();

	offset = __get_cpu_var(stab_cache_ptr);
	if (offset <= NR_STAB_CACHE_ENTRIES) {
		int i;

		for (i = 0; i < offset; i++) {
			ste = stab + __get_cpu_var(stab_cache[i]);
			ste->esid_data = 0; 
		}
	} else {
		unsigned long entry;

		
		ste = stab;

		
		ste += 1;
		for (entry = 1;
		     entry < (HW_PAGE_SIZE / sizeof(struct stab_entry));
		     entry++, ste++) {
			unsigned long ea;
			ea = ste->esid_data & ESID_MASK;
			if (!is_kernel_addr(ea)) {
				ste->esid_data = 0;
			}
		}
	}

	asm volatile("sync; slbia; sync":::"memory");

	__get_cpu_var(stab_cache_ptr) = 0;

	
	if (test_tsk_thread_flag(tsk, TIF_32BIT))
		unmapped_base = TASK_UNMAPPED_BASE_USER32;
	else
		unmapped_base = TASK_UNMAPPED_BASE_USER64;

	__ste_allocate(pc, mm);

	if (GET_ESID(pc) == GET_ESID(stack))
		return;

	__ste_allocate(stack, mm);

	if ((GET_ESID(pc) == GET_ESID(unmapped_base))
	    || (GET_ESID(stack) == GET_ESID(unmapped_base)))
		return;

	__ste_allocate(unmapped_base, mm);

	
	asm volatile("sync" : : : "memory");
}

void __init stabs_alloc(void)
{
	int cpu;

	if (mmu_has_feature(MMU_FTR_SLB))
		return;

	for_each_possible_cpu(cpu) {
		unsigned long newstab;

		if (cpu == 0)
			continue; 

		newstab = memblock_alloc_base(HW_PAGE_SIZE, HW_PAGE_SIZE,
					 1<<SID_SHIFT);
		newstab = (unsigned long)__va(newstab);

		memset((void *)newstab, 0, HW_PAGE_SIZE);

		paca[cpu].stab_addr = newstab;
		paca[cpu].stab_real = virt_to_abs(newstab);
		printk(KERN_INFO "Segment table for CPU %d at 0x%llx "
		       "virtual, 0x%llx absolute\n",
		       cpu, paca[cpu].stab_addr, paca[cpu].stab_real);
	}
}

void stab_initialize(unsigned long stab)
{
	unsigned long vsid = get_kernel_vsid(PAGE_OFFSET, MMU_SEGSIZE_256M);
	unsigned long stabreal;

	asm volatile("isync; slbia; isync":::"memory");
	make_ste(stab, GET_ESID(PAGE_OFFSET), vsid);

	
	asm volatile("sync":::"memory");

	
	stabreal = get_paca()->stab_real | 0x1ul;

	mtspr(SPRN_ASR, stabreal);
}
