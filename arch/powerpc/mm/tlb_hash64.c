/*
 * This file contains the routines for flushing entries from the
 * TLB and MMU hash table.
 *
 *  Derived from arch/ppc64/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Dave Engebretsen <engebret@us.ibm.com>
 *      Rework for PPC64 port.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/tlb.h>
#include <asm/bug.h>

DEFINE_PER_CPU(struct ppc64_tlb_batch, ppc64_tlb_batch);

void hpte_need_flush(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, unsigned long pte, int huge)
{
	struct ppc64_tlb_batch *batch = &get_cpu_var(ppc64_tlb_batch);
	unsigned long vsid, vaddr;
	unsigned int psize;
	int ssize;
	real_pte_t rpte;
	int i;

	i = batch->index;

	if (huge) {
#ifdef CONFIG_HUGETLB_PAGE
		psize = get_slice_psize(mm, addr);
		
		addr &= ~((1UL << mmu_psize_defs[psize].shift) - 1);
#else
		BUG();
		psize = pte_pagesize_index(mm, addr, pte); 
#endif
	} else {
		psize = pte_pagesize_index(mm, addr, pte);
		addr &= PAGE_MASK;
	}


	
	if (!is_kernel_addr(addr)) {
		ssize = user_segment_size(addr);
		vsid = get_vsid(mm->context.id, addr, ssize);
		WARN_ON(vsid == 0);
	} else {
		vsid = get_kernel_vsid(addr, mmu_kernel_ssize);
		ssize = mmu_kernel_ssize;
	}
	vaddr = hpt_va(addr, vsid, ssize);
	rpte = __real_pte(__pte(pte), ptep);

	if (!batch->active) {
		flush_hash_page(vaddr, rpte, psize, ssize, 0);
		put_cpu_var(ppc64_tlb_batch);
		return;
	}

	if (i != 0 && (mm != batch->mm || batch->psize != psize ||
		       batch->ssize != ssize)) {
		__flush_tlb_pending(batch);
		i = 0;
	}
	if (i == 0) {
		batch->mm = mm;
		batch->psize = psize;
		batch->ssize = ssize;
	}
	batch->pte[i] = rpte;
	batch->vaddr[i] = vaddr;
	batch->index = ++i;
	if (i >= PPC64_TLB_BATCH_NR)
		__flush_tlb_pending(batch);
	put_cpu_var(ppc64_tlb_batch);
}

void __flush_tlb_pending(struct ppc64_tlb_batch *batch)
{
	const struct cpumask *tmp;
	int i, local = 0;

	i = batch->index;
	tmp = cpumask_of(smp_processor_id());
	if (cpumask_equal(mm_cpumask(batch->mm), tmp))
		local = 1;
	if (i == 1)
		flush_hash_page(batch->vaddr[0], batch->pte[0],
				batch->psize, batch->ssize, local);
	else
		flush_hash_range(i, local);
	batch->index = 0;
}

void tlb_flush(struct mmu_gather *tlb)
{
	struct ppc64_tlb_batch *tlbbatch = &get_cpu_var(ppc64_tlb_batch);

	if (tlbbatch->index)
		__flush_tlb_pending(tlbbatch);

	put_cpu_var(ppc64_tlb_batch);
}

#ifdef CONFIG_HOTPLUG

void __flush_hash_table_range(struct mm_struct *mm, unsigned long start,
			      unsigned long end)
{
	unsigned long flags;

	start = _ALIGN_DOWN(start, PAGE_SIZE);
	end = _ALIGN_UP(end, PAGE_SIZE);

	BUG_ON(!mm->pgd);

	local_irq_save(flags);
	arch_enter_lazy_mmu_mode();
	for (; start < end; start += PAGE_SIZE) {
		pte_t *ptep = find_linux_pte(mm->pgd, start);
		unsigned long pte;

		if (ptep == NULL)
			continue;
		pte = pte_val(*ptep);
		if (!(pte & _PAGE_HASHPTE))
			continue;
		hpte_need_flush(mm, start, ptep, pte, 0);
	}
	arch_leave_lazy_mmu_mode();
	local_irq_restore(flags);
}

#endif 
