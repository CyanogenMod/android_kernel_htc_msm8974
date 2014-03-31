/*
 * Written by: Patricia Gaughen <gone@us.ibm.com>, IBM Corporation
 * August 2002: added remote node KVA remap - Martin J. Bligh 
 *
 * Copyright (C) 2002, IBM Corp.
 *
 * All rights reserved.          
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/module.h>

#include "numa_internal.h"

#ifdef CONFIG_DISCONTIGMEM
s8 physnode_map[MAX_SECTIONS] __read_mostly = { [0 ... (MAX_SECTIONS - 1)] = -1};
EXPORT_SYMBOL(physnode_map);

void memory_present(int nid, unsigned long start, unsigned long end)
{
	unsigned long pfn;

	printk(KERN_INFO "Node: %d, start_pfn: %lx, end_pfn: %lx\n",
			nid, start, end);
	printk(KERN_DEBUG "  Setting physnode_map array to node %d for pfns:\n", nid);
	printk(KERN_DEBUG "  ");
	for (pfn = start; pfn < end; pfn += PAGES_PER_SECTION) {
		physnode_map[pfn / PAGES_PER_SECTION] = nid;
		printk(KERN_CONT "%lx ", pfn);
	}
	printk(KERN_CONT "\n");
}

unsigned long node_memmap_size_bytes(int nid, unsigned long start_pfn,
					      unsigned long end_pfn)
{
	unsigned long nr_pages = end_pfn - start_pfn;

	if (!nr_pages)
		return 0;

	return (nr_pages + 1) * sizeof(struct page);
}
#endif

extern unsigned long highend_pfn, highstart_pfn;

#define LARGE_PAGE_BYTES (PTRS_PER_PTE * PAGE_SIZE)

static void *node_remap_start_vaddr[MAX_NUMNODES];
void set_pmd_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags);

static unsigned long node_remap_start_pfn[MAX_NUMNODES];
static void *node_remap_end_vaddr[MAX_NUMNODES];
static void *node_remap_alloc_vaddr[MAX_NUMNODES];

void *alloc_remap(int nid, unsigned long size)
{
	void *allocation = node_remap_alloc_vaddr[nid];

	size = ALIGN(size, L1_CACHE_BYTES);

	if (!allocation || (allocation + size) > node_remap_end_vaddr[nid])
		return NULL;

	node_remap_alloc_vaddr[nid] += size;
	memset(allocation, 0, size);

	return allocation;
}

#ifdef CONFIG_HIBERNATION
void resume_map_numa_kva(pgd_t *pgd_base)
{
	int node;

	for_each_online_node(node) {
		unsigned long start_va, start_pfn, nr_pages, pfn;

		start_va = (unsigned long)node_remap_start_vaddr[node];
		start_pfn = node_remap_start_pfn[node];
		nr_pages = (node_remap_end_vaddr[node] -
			    node_remap_start_vaddr[node]) >> PAGE_SHIFT;

		printk(KERN_DEBUG "%s: node %d\n", __func__, node);

		for (pfn = 0; pfn < nr_pages; pfn += PTRS_PER_PTE) {
			unsigned long vaddr = start_va + (pfn << PAGE_SHIFT);
			pgd_t *pgd = pgd_base + pgd_index(vaddr);
			pud_t *pud = pud_offset(pgd, vaddr);
			pmd_t *pmd = pmd_offset(pud, vaddr);

			set_pmd(pmd, pfn_pmd(start_pfn + pfn,
						PAGE_KERNEL_LARGE_EXEC));

			printk(KERN_DEBUG "%s: %08lx -> pfn %08lx\n",
				__func__, vaddr, start_pfn + pfn);
		}
	}
}
#endif

void __init init_alloc_remap(int nid, u64 start, u64 end)
{
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long end_pfn = end >> PAGE_SHIFT;
	unsigned long size, pfn;
	u64 node_pa, remap_pa;
	void *remap_va;

	printk(KERN_DEBUG "node %d pfn: [%lx - %lx]\n",
	       nid, start_pfn, end_pfn);

	
	size = node_memmap_size_bytes(nid, start_pfn, end_pfn);
	size += ALIGN(sizeof(pg_data_t), PAGE_SIZE);
	size = ALIGN(size, LARGE_PAGE_BYTES);

	
	node_pa = memblock_find_in_range(start, end, size, LARGE_PAGE_BYTES);
	if (!node_pa) {
		pr_warning("remap_alloc: failed to allocate %lu bytes for node %d\n",
			   size, nid);
		return;
	}
	memblock_reserve(node_pa, size);

	remap_pa = memblock_find_in_range(min_low_pfn << PAGE_SHIFT,
					  max_low_pfn << PAGE_SHIFT,
					  size, LARGE_PAGE_BYTES);
	if (!remap_pa) {
		pr_warning("remap_alloc: failed to allocate %lu bytes remap area for node %d\n",
			   size, nid);
		memblock_free(node_pa, size);
		return;
	}
	memblock_reserve(remap_pa, size);
	remap_va = phys_to_virt(remap_pa);

	
	for (pfn = 0; pfn < size >> PAGE_SHIFT; pfn += PTRS_PER_PTE)
		set_pmd_pfn((unsigned long)remap_va + (pfn << PAGE_SHIFT),
			    (node_pa >> PAGE_SHIFT) + pfn,
			    PAGE_KERNEL_LARGE);

	
	node_remap_start_pfn[nid] = node_pa >> PAGE_SHIFT;
	node_remap_start_vaddr[nid] = remap_va;
	node_remap_end_vaddr[nid] = remap_va + size;
	node_remap_alloc_vaddr[nid] = remap_va;

	printk(KERN_DEBUG "remap_alloc: node %d [%08llx-%08llx) -> [%p-%p)\n",
	       nid, node_pa, node_pa + size, remap_va, remap_va + size);
}

void __init initmem_init(void)
{
	x86_numa_init();

#ifdef CONFIG_HIGHMEM
	highstart_pfn = highend_pfn = max_pfn;
	if (max_pfn > max_low_pfn)
		highstart_pfn = max_low_pfn;
	printk(KERN_NOTICE "%ldMB HIGHMEM available.\n",
	       pages_to_mb(highend_pfn - highstart_pfn));
	num_physpages = highend_pfn;
	high_memory = (void *) __va(highstart_pfn * PAGE_SIZE - 1) + 1;
#else
	num_physpages = max_low_pfn;
	high_memory = (void *) __va(max_low_pfn * PAGE_SIZE - 1) + 1;
#endif
	printk(KERN_NOTICE "%ldMB LOWMEM available.\n",
			pages_to_mb(max_low_pfn));
	printk(KERN_DEBUG "max_low_pfn = %lx, highstart_pfn = %lx\n",
			max_low_pfn, highstart_pfn);

	printk(KERN_DEBUG "Low memory ends at vaddr %08lx\n",
			(ulong) pfn_to_kaddr(max_low_pfn));

	printk(KERN_DEBUG "High memory starts at vaddr %08lx\n",
			(ulong) pfn_to_kaddr(highstart_pfn));

	setup_bootmem_allocator();
}
