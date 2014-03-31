/*
 *  PowerPC version
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
 *
 */

#undef DEBUG

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/nodemask.h>
#include <linux/module.h>
#include <linux/poison.h>
#include <linux/memblock.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>

#include <asm/pgalloc.h>
#include <asm/page.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/uaccess.h>
#include <asm/smp.h>
#include <asm/machdep.h>
#include <asm/tlb.h>
#include <asm/eeh.h>
#include <asm/processor.h>
#include <asm/mmzone.h>
#include <asm/cputable.h>
#include <asm/sections.h>
#include <asm/iommu.h>
#include <asm/abs_addr.h>
#include <asm/vdso.h>

#include "mmu_decl.h"

#ifdef CONFIG_PPC_STD_MMU_64
#if PGTABLE_RANGE > USER_VSID_RANGE
#warning Limited user VSID range means pagetable space is wasted
#endif

#if (TASK_SIZE_USER64 < PGTABLE_RANGE) && (TASK_SIZE_USER64 < USER_VSID_RANGE)
#warning TASK_SIZE is smaller than it needs to be.
#endif
#endif 

phys_addr_t memstart_addr = ~0;
EXPORT_SYMBOL_GPL(memstart_addr);
phys_addr_t kernstart_addr;
EXPORT_SYMBOL_GPL(kernstart_addr);

static void pgd_ctor(void *addr)
{
	memset(addr, 0, PGD_TABLE_SIZE);
}

static void pmd_ctor(void *addr)
{
	memset(addr, 0, PMD_TABLE_SIZE);
}

struct kmem_cache *pgtable_cache[MAX_PGTABLE_INDEX_SIZE];

void pgtable_cache_add(unsigned shift, void (*ctor)(void *))
{
	char *name;
	unsigned long table_size = sizeof(void *) << shift;
	unsigned long align = table_size;

	unsigned long minalign = max(MAX_PGTABLE_INDEX_SIZE + 1,
				     HUGEPD_SHIFT_MASK + 1);
	struct kmem_cache *new;

	BUG_ON(!is_power_of_2(minalign));
	BUG_ON((shift < 1) || (shift > MAX_PGTABLE_INDEX_SIZE));

	if (PGT_CACHE(shift))
		return; 

	align = max_t(unsigned long, align, minalign);
	name = kasprintf(GFP_KERNEL, "pgtable-2^%d", shift);
	new = kmem_cache_create(name, table_size, align, 0, ctor);
	PGT_CACHE(shift) = new;

	pr_debug("Allocated pgtable cache for order %d\n", shift);
}


void pgtable_cache_init(void)
{
	pgtable_cache_add(PGD_INDEX_SIZE, pgd_ctor);
	pgtable_cache_add(PMD_INDEX_SIZE, pmd_ctor);
	if (!PGT_CACHE(PGD_INDEX_SIZE) || !PGT_CACHE(PMD_INDEX_SIZE))
		panic("Couldn't allocate pgtable caches");

	BUG_ON(PUD_INDEX_SIZE && !PGT_CACHE(PUD_INDEX_SIZE));
}

#ifdef CONFIG_SPARSEMEM_VMEMMAP
static unsigned long __meminit vmemmap_section_start(unsigned long page)
{
	unsigned long offset = page - ((unsigned long)(vmemmap));

	
	return (offset / sizeof(struct page)) & PAGE_SECTION_MASK;
}

static int __meminit vmemmap_populated(unsigned long start, int page_size)
{
	unsigned long end = start + page_size;

	for (; start < end; start += (PAGES_PER_SECTION * sizeof(struct page)))
		if (pfn_valid(vmemmap_section_start(start)))
			return 1;

	return 0;
}


#ifdef CONFIG_PPC_BOOK3E
static void __meminit vmemmap_create_mapping(unsigned long start,
					     unsigned long page_size,
					     unsigned long phys)
{
	
	unsigned long i, flags = _PAGE_PRESENT | _PAGE_ACCESSED |
		_PAGE_KERNEL_RW;

	
	BUG_ON(mmu_psize_defs[mmu_vmemmap_psize].enc > 0xf);

	
	flags |= mmu_psize_defs[mmu_vmemmap_psize].enc << 8;

	for (i = 0; i < page_size; i += PAGE_SIZE)
		BUG_ON(map_kernel_page(start + i, phys, flags));
}
#else 
static void __meminit vmemmap_create_mapping(unsigned long start,
					     unsigned long page_size,
					     unsigned long phys)
{
	int  mapped = htab_bolt_mapping(start, start + page_size, phys,
					PAGE_KERNEL, mmu_vmemmap_psize,
					mmu_kernel_ssize);
	BUG_ON(mapped < 0);
}
#endif 

struct vmemmap_backing *vmemmap_list;

static __meminit struct vmemmap_backing * vmemmap_list_alloc(int node)
{
	static struct vmemmap_backing *next;
	static int num_left;

	
	if (!next || !num_left) {
		next = vmemmap_alloc_block(PAGE_SIZE, node);
		if (unlikely(!next)) {
			WARN_ON(1);
			return NULL;
		}
		num_left = PAGE_SIZE / sizeof(struct vmemmap_backing);
	}

	num_left--;

	return next++;
}

static __meminit void vmemmap_list_populate(unsigned long phys,
					    unsigned long start,
					    int node)
{
	struct vmemmap_backing *vmem_back;

	vmem_back = vmemmap_list_alloc(node);
	if (unlikely(!vmem_back)) {
		WARN_ON(1);
		return;
	}

	vmem_back->phys = phys;
	vmem_back->virt_addr = start;
	vmem_back->list = vmemmap_list;

	vmemmap_list = vmem_back;
}

int __meminit vmemmap_populate(struct page *start_page,
			       unsigned long nr_pages, int node)
{
	unsigned long start = (unsigned long)start_page;
	unsigned long end = (unsigned long)(start_page + nr_pages);
	unsigned long page_size = 1 << mmu_psize_defs[mmu_vmemmap_psize].shift;

	
	start = _ALIGN_DOWN(start, page_size);

	pr_debug("vmemmap_populate page %p, %ld pages, node %d\n",
		 start_page, nr_pages, node);
	pr_debug(" -> map %lx..%lx\n", start, end);

	for (; start < end; start += page_size) {
		void *p;

		if (vmemmap_populated(start, page_size))
			continue;

		p = vmemmap_alloc_block(page_size, node);
		if (!p)
			return -ENOMEM;

		vmemmap_list_populate(__pa(p), start, node);

		pr_debug("      * %016lx..%016lx allocated at %p\n",
			 start, start + page_size, p);

		vmemmap_create_mapping(start, page_size, __pa(p));
	}

	return 0;
}
#endif 

