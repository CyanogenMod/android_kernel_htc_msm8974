/*
 * arch/sh/mm/cache-sh5.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2002  Benedict Gaster
 * Copyright (C) 2003  Richard Curnow
 * Copyright (C) 2003 - 2008  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <asm/tlb.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>

extern void __weak sh4__flush_region_init(void);

static unsigned long long dtlb_cache_slot;

static inline void
sh64_setup_dtlb_cache_slot(unsigned long eaddr, unsigned long asid,
			   unsigned long paddr)
{
	local_irq_disable();
	sh64_setup_tlb_slot(dtlb_cache_slot, eaddr, asid, paddr);
}

static inline void sh64_teardown_dtlb_cache_slot(void)
{
	sh64_teardown_tlb_slot(dtlb_cache_slot);
	local_irq_enable();
}

static inline void sh64_icache_inv_all(void)
{
	unsigned long long addr, flag, data;
	unsigned long flags;

	addr = ICCR0;
	flag = ICCR0_ICI;
	data = 0;

	
	local_irq_save(flags);

	
	__asm__ __volatile__ (
		"getcfg	%3, 0, %0\n\t"
		"or	%0, %2, %0\n\t"
		"putcfg	%3, 0, %0\n\t"
		"synci"
		: "=&r" (data)
		: "0" (data), "r" (flag), "r" (addr));

	local_irq_restore(flags);
}

static void sh64_icache_inv_kernel_range(unsigned long start, unsigned long end)
{

	unsigned long long ullend, addr, aligned_start;
	aligned_start = (unsigned long long)(signed long long)(signed long) start;
	addr = L1_CACHE_ALIGN(aligned_start);
	ullend = (unsigned long long) (signed long long) (signed long) end;

	while (addr <= ullend) {
		__asm__ __volatile__ ("icbi %0, 0" : : "r" (addr));
		addr += L1_CACHE_BYTES;
	}
}

static void sh64_icache_inv_user_page(struct vm_area_struct *vma, unsigned long eaddr)
{
	unsigned int cpu = smp_processor_id();
	unsigned long long addr, end_addr;
	unsigned long flags = 0;
	unsigned long running_asid, vma_asid;
	addr = eaddr;
	end_addr = addr + PAGE_SIZE;


	running_asid = get_asid();
	vma_asid = cpu_asid(cpu, vma->vm_mm);
	if (running_asid != vma_asid) {
		local_irq_save(flags);
		switch_and_save_asid(vma_asid);
	}
	while (addr < end_addr) {
		
		__asm__ __volatile__("icbi %0,  0" : : "r" (addr));
		__asm__ __volatile__("icbi %0, 32" : : "r" (addr));
		__asm__ __volatile__("icbi %0, 64" : : "r" (addr));
		__asm__ __volatile__("icbi %0, 96" : : "r" (addr));
		addr += 128;
	}
	if (running_asid != vma_asid) {
		switch_and_save_asid(running_asid);
		local_irq_restore(flags);
	}
}

static void sh64_icache_inv_user_page_range(struct mm_struct *mm,
			  unsigned long start, unsigned long end)
{

	int n_pages;

	if (!mm)
		return;

	n_pages = ((end - start) >> PAGE_SHIFT);
	if (n_pages >= 64) {
		sh64_icache_inv_all();
	} else {
		unsigned long aligned_start;
		unsigned long eaddr;
		unsigned long after_last_page_start;
		unsigned long mm_asid, current_asid;
		unsigned long flags = 0;

		mm_asid = cpu_asid(smp_processor_id(), mm);
		current_asid = get_asid();

		if (mm_asid != current_asid) {
			
			local_irq_save(flags);
			switch_and_save_asid(mm_asid);
		}

		aligned_start = start & PAGE_MASK;
		after_last_page_start = PAGE_SIZE + ((end - 1) & PAGE_MASK);

		while (aligned_start < after_last_page_start) {
			struct vm_area_struct *vma;
			unsigned long vma_end;
			vma = find_vma(mm, aligned_start);
			if (!vma || (aligned_start <= vma->vm_end)) {
				
				aligned_start += PAGE_SIZE;
				continue;
			}
			vma_end = vma->vm_end;
			if (vma->vm_flags & VM_EXEC) {
				
				eaddr = aligned_start;
				while (eaddr < vma_end) {
					sh64_icache_inv_user_page(vma, eaddr);
					eaddr += PAGE_SIZE;
				}
			}
			aligned_start = vma->vm_end; 
		}

		if (mm_asid != current_asid) {
			switch_and_save_asid(current_asid);
			local_irq_restore(flags);
		}
	}
}

static void sh64_icache_inv_current_user_range(unsigned long start, unsigned long end)
{

	unsigned long long aligned_start;
	unsigned long long ull_end;
	unsigned long long addr;

	ull_end = end;

	aligned_start = L1_CACHE_ALIGN(start);
	addr = aligned_start;
	while (addr < ull_end) {
		__asm__ __volatile__ ("icbi %0, 0" : : "r" (addr));
		__asm__ __volatile__ ("nop");
		__asm__ __volatile__ ("nop");
		addr += L1_CACHE_BYTES;
	}
}

#define DUMMY_ALLOCO_AREA_SIZE ((L1_CACHE_BYTES << 10) + (1024 * 4))
static unsigned char dummy_alloco_area[DUMMY_ALLOCO_AREA_SIZE] __cacheline_aligned = { 0, };

static void inline sh64_dcache_purge_sets(int sets_to_purge_base, int n_sets)
{

	int dummy_buffer_base_set;
	unsigned long long eaddr, eaddr0, eaddr1;
	int j;
	int set_offset;

	dummy_buffer_base_set = ((int)&dummy_alloco_area &
				 cpu_data->dcache.entry_mask) >>
				 cpu_data->dcache.entry_shift;
	set_offset = sets_to_purge_base - dummy_buffer_base_set;

	for (j = 0; j < n_sets; j++, set_offset++) {
		set_offset &= (cpu_data->dcache.sets - 1);
		eaddr0 = (unsigned long long)dummy_alloco_area +
			(set_offset << cpu_data->dcache.entry_shift);

		eaddr1 = eaddr0 + cpu_data->dcache.way_size *
				  cpu_data->dcache.ways;

		for (eaddr = eaddr0; eaddr < eaddr1;
		     eaddr += cpu_data->dcache.way_size) {
			__asm__ __volatile__ ("alloco %0, 0" : : "r" (eaddr));
			__asm__ __volatile__ ("synco"); 
		}

		eaddr1 = eaddr0 + cpu_data->dcache.way_size *
				  cpu_data->dcache.ways;

		for (eaddr = eaddr0; eaddr < eaddr1;
		     eaddr += cpu_data->dcache.way_size) {
			if (test_bit(SH_CACHE_MODE_WT, &(cpu_data->dcache.flags)))
				__raw_readb((unsigned long)eaddr);
		}
	}

}

static void sh64_dcache_purge_all(void)
{

	sh64_dcache_purge_sets(0, cpu_data->dcache.sets);
}


#define MAGIC_PAGE0_START 0xffffffffec000000ULL

static void sh64_dcache_purge_coloured_phy_page(unsigned long paddr,
					        unsigned long eaddr)
{
	unsigned long long magic_page_start;
	unsigned long long magic_eaddr, magic_eaddr_end;

	magic_page_start = MAGIC_PAGE0_START + (eaddr & CACHE_OC_SYN_MASK);

	sh64_setup_dtlb_cache_slot(magic_page_start, get_asid(), paddr);

	magic_eaddr = magic_page_start;
	magic_eaddr_end = magic_eaddr + PAGE_SIZE;

	while (magic_eaddr < magic_eaddr_end) {
		__asm__ __volatile__ ("ocbp %0, 0" : : "r" (magic_eaddr));
		magic_eaddr += L1_CACHE_BYTES;
	}

	sh64_teardown_dtlb_cache_slot();
}

static void sh64_dcache_purge_phy_page(unsigned long paddr)
{
	unsigned long long eaddr_start, eaddr, eaddr_end;
	int i;

	eaddr_start = MAGIC_PAGE0_START;
	for (i = 0; i < (1 << CACHE_OC_N_SYNBITS); i++) {
		sh64_setup_dtlb_cache_slot(eaddr_start, get_asid(), paddr);

		eaddr = eaddr_start;
		eaddr_end = eaddr + PAGE_SIZE;
		while (eaddr < eaddr_end) {
			__asm__ __volatile__ ("ocbp %0, 0" : : "r" (eaddr));
			eaddr += L1_CACHE_BYTES;
		}

		sh64_teardown_dtlb_cache_slot();
		eaddr_start += PAGE_SIZE;
	}
}

static void sh64_dcache_purge_user_pages(struct mm_struct *mm,
				unsigned long addr, unsigned long end)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	spinlock_t *ptl;
	unsigned long paddr;

	if (!mm)
		return; 

	pgd = pgd_offset(mm, addr);
	if (pgd_bad(*pgd))
		return;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud))
		return;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return;

	pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
	do {
		entry = *pte;
		if (pte_none(entry) || !pte_present(entry))
			continue;
		paddr = pte_val(entry) & PAGE_MASK;
		sh64_dcache_purge_coloured_phy_page(paddr, addr);
	} while (pte++, addr += PAGE_SIZE, addr != end);
	pte_unmap_unlock(pte - 1, ptl);
}

static void sh64_dcache_purge_user_range(struct mm_struct *mm,
			  unsigned long start, unsigned long end)
{
	int n_pages = ((end - start) >> PAGE_SHIFT);

	if (n_pages >= 64 || ((start ^ (end - 1)) & PMD_MASK)) {
		sh64_dcache_purge_all();
	} else {
		
		start &= PAGE_MASK;	
		end = PAGE_ALIGN(end);	
		sh64_dcache_purge_user_pages(mm, start, end);
	}
}

static void sh5_flush_cache_all(void *unused)
{
	sh64_dcache_purge_all();
	sh64_icache_inv_all();
}

/*
 * Invalidate an entire user-address space from both caches, after
 * writing back dirty data (e.g. for shared mmap etc).
 *
 * This could be coded selectively by inspecting all the tags then
 * doing 4*alloco on any set containing a match (as for
 * flush_cache_range), but fork/exit/execve (where this is called from)
 * are expensive anyway.
 *
 * Have to do a purge here, despite the comments re I-cache below.
 * There could be odd-coloured dirty data associated with the mm still
 * in the cache - if this gets written out through natural eviction
 * after the kernel has reused the page there will be chaos.
 *
 * The mm being torn down won't ever be active again, so any Icache
 * lines tagged with its ASID won't be visible for the rest of the
 * lifetime of this ASID cycle.  Before the ASID gets reused, there
 * will be a flush_cache_all.  Hence we don't need to touch the
 * I-cache.  This is similar to the lack of action needed in
 * flush_tlb_mm - see fault.c.
 */
static void sh5_flush_cache_mm(void *unused)
{
	sh64_dcache_purge_all();
}

static void sh5_flush_cache_range(void *args)
{
	struct flusher_data *data = args;
	struct vm_area_struct *vma;
	unsigned long start, end;

	vma = data->vma;
	start = data->addr1;
	end = data->addr2;

	sh64_dcache_purge_user_range(vma->vm_mm, start, end);
	sh64_icache_inv_user_page_range(vma->vm_mm, start, end);
}

static void sh5_flush_cache_page(void *args)
{
	struct flusher_data *data = args;
	struct vm_area_struct *vma;
	unsigned long eaddr, pfn;

	vma = data->vma;
	eaddr = data->addr1;
	pfn = data->addr2;

	sh64_dcache_purge_phy_page(pfn << PAGE_SHIFT);

	if (vma->vm_flags & VM_EXEC)
		sh64_icache_inv_user_page(vma, eaddr);
}

static void sh5_flush_dcache_page(void *page)
{
	sh64_dcache_purge_phy_page(page_to_phys((struct page *)page));
	wmb();
}

static void sh5_flush_icache_range(void *args)
{
	struct flusher_data *data = args;
	unsigned long start, end;

	start = data->addr1;
	end = data->addr2;

	__flush_purge_region((void *)start, end);
	wmb();
	sh64_icache_inv_kernel_range(start, end);
}

static void sh5_flush_cache_sigtramp(void *vaddr)
{
	unsigned long end = (unsigned long)vaddr + L1_CACHE_BYTES;

	__flush_wback_region(vaddr, L1_CACHE_BYTES);
	wmb();
	sh64_icache_inv_current_user_range((unsigned long)vaddr, end);
}

void __init sh5_cache_init(void)
{
	local_flush_cache_all		= sh5_flush_cache_all;
	local_flush_cache_mm		= sh5_flush_cache_mm;
	local_flush_cache_dup_mm	= sh5_flush_cache_mm;
	local_flush_cache_page		= sh5_flush_cache_page;
	local_flush_cache_range		= sh5_flush_cache_range;
	local_flush_dcache_page		= sh5_flush_dcache_page;
	local_flush_icache_range	= sh5_flush_icache_range;
	local_flush_cache_sigtramp	= sh5_flush_cache_sigtramp;

	
	dtlb_cache_slot	= sh64_get_wired_dtlb_entry();

	sh4__flush_region_init();
}
