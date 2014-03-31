/*
 * This file contains the routines for TLB flushing.
 * On machines where the MMU does not use a hash table to store virtual to
 * physical translations (ie, SW loaded TLBs or Book3E compilant processors,
 * this does -not- include 603 however which shares the implementation with
 * hash based processors)
 *
 *  -- BenH
 *
 * Copyright 2008,2009 Ben Herrenschmidt <benh@kernel.crashing.org>
 *                     IBM Corp.
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/memblock.h>
#include <linux/of_fdt.h>
#include <linux/hugetlb.h>

#include <asm/tlbflush.h>
#include <asm/tlb.h>
#include <asm/code-patching.h>
#include <asm/hugetlb.h>

#include "mmu_decl.h"

#ifdef CONFIG_PPC_BOOK3E_MMU
#ifdef CONFIG_PPC_FSL_BOOK3E
struct mmu_psize_def mmu_psize_defs[MMU_PAGE_COUNT] = {
	[MMU_PAGE_4K] = {
		.shift	= 12,
		.enc	= BOOK3E_PAGESZ_4K,
	},
	[MMU_PAGE_4M] = {
		.shift	= 22,
		.enc	= BOOK3E_PAGESZ_4M,
	},
	[MMU_PAGE_16M] = {
		.shift	= 24,
		.enc	= BOOK3E_PAGESZ_16M,
	},
	[MMU_PAGE_64M] = {
		.shift	= 26,
		.enc	= BOOK3E_PAGESZ_64M,
	},
	[MMU_PAGE_256M] = {
		.shift	= 28,
		.enc	= BOOK3E_PAGESZ_256M,
	},
	[MMU_PAGE_1G] = {
		.shift	= 30,
		.enc	= BOOK3E_PAGESZ_1GB,
	},
};
#else
struct mmu_psize_def mmu_psize_defs[MMU_PAGE_COUNT] = {
	[MMU_PAGE_4K] = {
		.shift	= 12,
		.ind	= 20,
		.enc	= BOOK3E_PAGESZ_4K,
	},
	[MMU_PAGE_16K] = {
		.shift	= 14,
		.enc	= BOOK3E_PAGESZ_16K,
	},
	[MMU_PAGE_64K] = {
		.shift	= 16,
		.ind	= 28,
		.enc	= BOOK3E_PAGESZ_64K,
	},
	[MMU_PAGE_1M] = {
		.shift	= 20,
		.enc	= BOOK3E_PAGESZ_1M,
	},
	[MMU_PAGE_16M] = {
		.shift	= 24,
		.ind	= 36,
		.enc	= BOOK3E_PAGESZ_16M,
	},
	[MMU_PAGE_256M] = {
		.shift	= 28,
		.enc	= BOOK3E_PAGESZ_256M,
	},
	[MMU_PAGE_1G] = {
		.shift	= 30,
		.enc	= BOOK3E_PAGESZ_1GB,
	},
};
#endif 

static inline int mmu_get_tsize(int psize)
{
	return mmu_psize_defs[psize].enc;
}
#else
static inline int mmu_get_tsize(int psize)
{
	
	return 0;
}
#endif 

#ifdef CONFIG_PPC64

int mmu_linear_psize;		
int mmu_pte_psize;		
int mmu_vmemmap_psize;		
int book3e_htw_enabled;		
unsigned long linear_map_top;	

#endif 

#ifdef CONFIG_PPC_FSL_BOOK3E
DEFINE_PER_CPU(int, next_tlbcam_idx);
EXPORT_PER_CPU_SYMBOL(next_tlbcam_idx);
#endif


void local_flush_tlb_mm(struct mm_struct *mm)
{
	unsigned int pid;

	preempt_disable();
	pid = mm->context.id;
	if (pid != MMU_NO_CONTEXT)
		_tlbil_pid(pid);
	preempt_enable();
}
EXPORT_SYMBOL(local_flush_tlb_mm);

void __local_flush_tlb_page(struct mm_struct *mm, unsigned long vmaddr,
			    int tsize, int ind)
{
	unsigned int pid;

	preempt_disable();
	pid = mm ? mm->context.id : 0;
	if (pid != MMU_NO_CONTEXT)
		_tlbil_va(vmaddr, pid, tsize, ind);
	preempt_enable();
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long vmaddr)
{
	__local_flush_tlb_page(vma ? vma->vm_mm : NULL, vmaddr,
			       mmu_get_tsize(mmu_virtual_psize), 0);
}
EXPORT_SYMBOL(local_flush_tlb_page);

#ifdef CONFIG_SMP

static DEFINE_RAW_SPINLOCK(tlbivax_lock);

static int mm_is_core_local(struct mm_struct *mm)
{
	return cpumask_subset(mm_cpumask(mm),
			      topology_thread_cpumask(smp_processor_id()));
}

struct tlb_flush_param {
	unsigned long addr;
	unsigned int pid;
	unsigned int tsize;
	unsigned int ind;
};

static void do_flush_tlb_mm_ipi(void *param)
{
	struct tlb_flush_param *p = param;

	_tlbil_pid(p ? p->pid : 0);
}

static void do_flush_tlb_page_ipi(void *param)
{
	struct tlb_flush_param *p = param;

	_tlbil_va(p->addr, p->pid, p->tsize, p->ind);
}



void flush_tlb_mm(struct mm_struct *mm)
{
	unsigned int pid;

	preempt_disable();
	pid = mm->context.id;
	if (unlikely(pid == MMU_NO_CONTEXT))
		goto no_context;
	if (!mm_is_core_local(mm)) {
		struct tlb_flush_param p = { .pid = pid };
		
		smp_call_function_many(mm_cpumask(mm),
				       do_flush_tlb_mm_ipi, &p, 1);
	}
	_tlbil_pid(pid);
 no_context:
	preempt_enable();
}
EXPORT_SYMBOL(flush_tlb_mm);

void __flush_tlb_page(struct mm_struct *mm, unsigned long vmaddr,
		      int tsize, int ind)
{
	struct cpumask *cpu_mask;
	unsigned int pid;

	preempt_disable();
	pid = mm ? mm->context.id : 0;
	if (unlikely(pid == MMU_NO_CONTEXT))
		goto bail;
	cpu_mask = mm_cpumask(mm);
	if (!mm_is_core_local(mm)) {
		
		if (mmu_has_feature(MMU_FTR_USE_TLBIVAX_BCAST)) {
			int lock = mmu_has_feature(MMU_FTR_LOCK_BCAST_INVAL);
			if (lock)
				raw_spin_lock(&tlbivax_lock);
			_tlbivax_bcast(vmaddr, pid, tsize, ind);
			if (lock)
				raw_spin_unlock(&tlbivax_lock);
			goto bail;
		} else {
			struct tlb_flush_param p = {
				.pid = pid,
				.addr = vmaddr,
				.tsize = tsize,
				.ind = ind,
			};
			
			smp_call_function_many(cpu_mask,
					       do_flush_tlb_page_ipi, &p, 1);
		}
	}
	_tlbil_va(vmaddr, pid, tsize, ind);
 bail:
	preempt_enable();
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long vmaddr)
{
#ifdef CONFIG_HUGETLB_PAGE
	if (is_vm_hugetlb_page(vma))
		flush_hugetlb_page(vma, vmaddr);
#endif

	__flush_tlb_page(vma ? vma->vm_mm : NULL, vmaddr,
			 mmu_get_tsize(mmu_virtual_psize), 0);
}
EXPORT_SYMBOL(flush_tlb_page);

#endif 

#ifdef CONFIG_PPC_47x
void __init early_init_mmu_47x(void)
{
#ifdef CONFIG_SMP
	unsigned long root = of_get_flat_dt_root();
	if (of_get_flat_dt_prop(root, "cooperative-partition", NULL))
		mmu_clear_feature(MMU_FTR_USE_TLBIVAX_BCAST);
#endif 
}
#endif 

void flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
#ifdef CONFIG_SMP
	preempt_disable();
	smp_call_function(do_flush_tlb_mm_ipi, NULL, 1);
	_tlbil_pid(0);
	preempt_enable();
#else
	_tlbil_pid(0);
#endif
}
EXPORT_SYMBOL(flush_tlb_kernel_range);

void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
		     unsigned long end)

{
	flush_tlb_mm(vma->vm_mm);
}
EXPORT_SYMBOL(flush_tlb_range);

void tlb_flush(struct mmu_gather *tlb)
{
	flush_tlb_mm(tlb->mm);
}


#ifdef CONFIG_PPC64

void tlb_flush_pgtable(struct mmu_gather *tlb, unsigned long address)
{
	int tsize = mmu_psize_defs[mmu_pte_psize].enc;

	if (book3e_htw_enabled) {
		unsigned long start = address & PMD_MASK;
		unsigned long end = address + PMD_SIZE;
		unsigned long size = 1UL << mmu_psize_defs[mmu_pte_psize].shift;

		while (start < end) {
			__flush_tlb_page(tlb->mm, start, tsize, 1);
			start += size;
		}
	} else {
		unsigned long rmask = 0xf000000000000000ul;
		unsigned long rid = (address & rmask) | 0x1000000000000000ul;
		unsigned long vpte = address & ~rmask;

#ifdef CONFIG_PPC_64K_PAGES
		vpte = (vpte >> (PAGE_SHIFT - 4)) & ~0xfffful;
#else
		vpte = (vpte >> (PAGE_SHIFT - 3)) & ~0xffful;
#endif
		vpte |= rid;
		__flush_tlb_page(tlb->mm, vpte, tsize, 0);
	}
}

static void setup_page_sizes(void)
{
	unsigned int tlb0cfg;
	unsigned int tlb0ps;
	unsigned int eptcfg;
	int i, psize;

#ifdef CONFIG_PPC_FSL_BOOK3E
	unsigned int mmucfg = mfspr(SPRN_MMUCFG);

	if (((mmucfg & MMUCFG_MAVN) == MMUCFG_MAVN_V1) &&
		(mmu_has_feature(MMU_FTR_TYPE_FSL_E))) {
		unsigned int tlb1cfg = mfspr(SPRN_TLB1CFG);
		unsigned int min_pg, max_pg;

		min_pg = (tlb1cfg & TLBnCFG_MINSIZE) >> TLBnCFG_MINSIZE_SHIFT;
		max_pg = (tlb1cfg & TLBnCFG_MAXSIZE) >> TLBnCFG_MAXSIZE_SHIFT;

		for (psize = 0; psize < MMU_PAGE_COUNT; ++psize) {
			struct mmu_psize_def *def;
			unsigned int shift;

			def = &mmu_psize_defs[psize];
			shift = def->shift;

			if (shift == 0)
				continue;

			
			shift = (shift - 10) >> 1;

			if ((shift >= min_pg) && (shift <= max_pg))
				def->flags |= MMU_PAGE_SIZE_DIRECT;
		}

		goto no_indirect;
	}
#endif

	tlb0cfg = mfspr(SPRN_TLB0CFG);
	tlb0ps = mfspr(SPRN_TLB0PS);
	eptcfg = mfspr(SPRN_EPTCFG);

	
	for (psize = 0; psize < MMU_PAGE_COUNT; ++psize) {
		struct mmu_psize_def *def = &mmu_psize_defs[psize];

		if (tlb0ps & (1U << (def->shift - 10)))
			def->flags |= MMU_PAGE_SIZE_DIRECT;
	}

	
	if ((tlb0cfg & TLBnCFG_IND) == 0)
		goto no_indirect;

	for (i = 0; i < 3; i++) {
		unsigned int ps, sps;

		sps = eptcfg & 0x1f;
		eptcfg >>= 5;
		ps = eptcfg & 0x1f;
		eptcfg >>= 5;
		if (!ps || !sps)
			continue;
		for (psize = 0; psize < MMU_PAGE_COUNT; psize++) {
			struct mmu_psize_def *def = &mmu_psize_defs[psize];

			if (ps == (def->shift - 10))
				def->flags |= MMU_PAGE_SIZE_INDIRECT;
			if (sps == (def->shift - 10))
				def->ind = ps + 10;
		}
	}
 no_indirect:

	
	pr_info("MMU: Supported page sizes\n");
	for (psize = 0; psize < MMU_PAGE_COUNT; ++psize) {
		struct mmu_psize_def *def = &mmu_psize_defs[psize];
		const char *__page_type_names[] = {
			"unsupported",
			"direct",
			"indirect",
			"direct & indirect"
		};
		if (def->flags == 0) {
			def->shift = 0;	
			continue;
		}
		pr_info("  %8ld KB as %s\n", 1ul << (def->shift - 10),
			__page_type_names[def->flags & 0x3]);
	}
}

static void __patch_exception(int exc, unsigned long addr)
{
	extern unsigned int interrupt_base_book3e;
 	unsigned int *ibase = &interrupt_base_book3e;
 

	patch_branch(ibase + (exc / 4) + 1, addr, 0);
}

#define patch_exception(exc, name) do { \
	extern unsigned int name; \
	__patch_exception((exc), (unsigned long)&name); \
} while (0)

static void setup_mmu_htw(void)
{
	unsigned int tlb0cfg = mfspr(SPRN_TLB0CFG);

	if ((tlb0cfg & TLBnCFG_IND) &&
	    (tlb0cfg & TLBnCFG_PT)) {
		patch_exception(0x1c0, exc_data_tlb_miss_htw_book3e);
		patch_exception(0x1e0, exc_instruction_tlb_miss_htw_book3e);
		book3e_htw_enabled = 1;
	}
	pr_info("MMU: Book3E HW tablewalk %s\n",
		book3e_htw_enabled ? "enabled" : "not supported");
}

static void __early_init_mmu(int boot_cpu)
{
	unsigned int mas4;

	mmu_linear_psize = MMU_PAGE_1G;

	mmu_vmemmap_psize = MMU_PAGE_16M;

	if (boot_cpu) {
		
		setup_page_sizes();

		
		setup_mmu_htw();
	}

	

	mas4 = 0x4 << MAS4_WIMGED_SHIFT;
	if (book3e_htw_enabled) {
		mas4 |= mas4 | MAS4_INDD;
#ifdef CONFIG_PPC_64K_PAGES
		mas4 |=	BOOK3E_PAGESZ_256M << MAS4_TSIZED_SHIFT;
		mmu_pte_psize = MMU_PAGE_256M;
#else
		mas4 |=	BOOK3E_PAGESZ_1M << MAS4_TSIZED_SHIFT;
		mmu_pte_psize = MMU_PAGE_1M;
#endif
	} else {
#ifdef CONFIG_PPC_64K_PAGES
		mas4 |=	BOOK3E_PAGESZ_64K << MAS4_TSIZED_SHIFT;
#else
		mas4 |=	BOOK3E_PAGESZ_4K << MAS4_TSIZED_SHIFT;
#endif
		mmu_pte_psize = mmu_virtual_psize;
	}
	mtspr(SPRN_MAS4, mas4);

	linear_map_top = memblock_end_of_DRAM();

#ifdef CONFIG_PPC_FSL_BOOK3E
	if (mmu_has_feature(MMU_FTR_TYPE_FSL_E)) {
		unsigned int num_cams;

		
		num_cams = (mfspr(SPRN_TLB1CFG) & TLBnCFG_N_ENTRY) / 4;
		linear_map_top = map_mem_in_cams(linear_map_top, num_cams);

		
		memblock_enforce_memory_limit(linear_map_top);

		patch_exception(0x1c0, exc_data_tlb_miss_bolted_book3e);
		patch_exception(0x1e0, exc_instruction_tlb_miss_bolted_book3e);
	}
#endif

	mb();

	memblock_set_current_limit(linear_map_top);
}

void __init early_init_mmu(void)
{
	__early_init_mmu(1);
}

void __cpuinit early_init_mmu_secondary(void)
{
	__early_init_mmu(0);
}

void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				phys_addr_t first_memblock_size)
{
#ifdef CONFIG_PPC_FSL_BOOK3E
	if (mmu_has_feature(MMU_FTR_TYPE_FSL_E)) {
		unsigned long linear_sz;
		linear_sz = calc_cam_sz(first_memblock_size, PAGE_OFFSET,
					first_memblock_base);
		ppc64_rma_size = min_t(u64, linear_sz, 0x40000000);
	} else
#endif
		ppc64_rma_size = min_t(u64, first_memblock_size, 0x40000000);

	
	memblock_set_current_limit(first_memblock_base + ppc64_rma_size);
}
#else 
void __init early_init_mmu(void)
{
#ifdef CONFIG_PPC_47x
	early_init_mmu_47x();
#endif
}
#endif 
