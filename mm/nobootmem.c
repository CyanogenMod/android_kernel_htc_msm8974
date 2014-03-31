/*
 *  bootmem - A boot-time physical memory allocator and configurator
 *
 *  Copyright (C) 1999 Ingo Molnar
 *                1999 Kanoj Sarcar, SGI
 *                2008 Johannes Weiner
 *
 * Access to this subsystem has to be serialized externally (which is true
 * for the boot process anyway).
 */
#include <linux/init.h>
#include <linux/pfn.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/export.h>
#include <linux/kmemleak.h>
#include <linux/range.h>
#include <linux/memblock.h>

#include <asm/bug.h>
#include <asm/io.h>
#include <asm/processor.h>

#include "internal.h"

#ifndef CONFIG_NEED_MULTIPLE_NODES
struct pglist_data __refdata contig_page_data;
EXPORT_SYMBOL(contig_page_data);
#endif

unsigned long max_low_pfn;
unsigned long min_low_pfn;
unsigned long max_pfn;

static void * __init __alloc_memory_core_early(int nid, u64 size, u64 align,
					u64 goal, u64 limit)
{
	void *ptr;
	u64 addr;

	if (limit > memblock.current_limit)
		limit = memblock.current_limit;

	addr = memblock_find_in_range_node(goal, limit, size, align, nid);
	if (!addr)
		return NULL;

	ptr = phys_to_virt(addr);
	memset(ptr, 0, size);
	memblock_reserve(addr, size);
	kmemleak_alloc(ptr, size, 0, 0);
	return ptr;
}

void __init free_bootmem_late(unsigned long addr, unsigned long size)
{
	unsigned long cursor, end;

	kmemleak_free_part(__va(addr), size);

	cursor = PFN_UP(addr);
	end = PFN_DOWN(addr + size);

	for (; cursor < end; cursor++) {
		__free_pages_bootmem(pfn_to_page(cursor), 0);
		totalram_pages++;
	}
}

static void __init __free_pages_memory(unsigned long start, unsigned long end)
{
	unsigned long i, start_aligned, end_aligned;
	int order = ilog2(BITS_PER_LONG);

	start_aligned = (start + (BITS_PER_LONG - 1)) & ~(BITS_PER_LONG - 1);
	end_aligned = end & ~(BITS_PER_LONG - 1);

	if (end_aligned <= start_aligned) {
		for (i = start; i < end; i++)
			__free_pages_bootmem(pfn_to_page(i), 0);

		return;
	}

	for (i = start; i < start_aligned; i++)
		__free_pages_bootmem(pfn_to_page(i), 0);

	for (i = start_aligned; i < end_aligned; i += BITS_PER_LONG)
		__free_pages_bootmem(pfn_to_page(i), order);

	for (i = end_aligned; i < end; i++)
		__free_pages_bootmem(pfn_to_page(i), 0);
}

unsigned long __init free_low_memory_core_early(int nodeid)
{
	unsigned long count = 0;
	phys_addr_t start, end;
	u64 i;

	
	memblock_free_reserved_regions();

	for_each_free_mem_range(i, MAX_NUMNODES, &start, &end, NULL) {
		unsigned long start_pfn = PFN_UP(start);
		unsigned long end_pfn = min_t(unsigned long,
					      PFN_DOWN(end), max_low_pfn);
		if (start_pfn < end_pfn) {
			__free_pages_memory(start_pfn, end_pfn);
			count += end_pfn - start_pfn;
		}
	}

	
	memblock_reserve_reserved_regions();
	return count;
}

unsigned long __init free_all_bootmem_node(pg_data_t *pgdat)
{
	register_page_bootmem_info_node(pgdat);

	
	return 0;
}

unsigned long __init free_all_bootmem(void)
{
	return free_low_memory_core_early(MAX_NUMNODES);
}

void __init free_bootmem_node(pg_data_t *pgdat, unsigned long physaddr,
			      unsigned long size)
{
	kmemleak_free_part(__va(physaddr), size);
	memblock_free(physaddr, size);
}

void __init free_bootmem(unsigned long addr, unsigned long size)
{
	kmemleak_free_part(__va(addr), size);
	memblock_free(addr, size);
}

static void * __init ___alloc_bootmem_nopanic(unsigned long size,
					unsigned long align,
					unsigned long goal,
					unsigned long limit)
{
	void *ptr;

	if (WARN_ON_ONCE(slab_is_available()))
		return kzalloc(size, GFP_NOWAIT);

restart:

	ptr = __alloc_memory_core_early(MAX_NUMNODES, size, align, goal, limit);

	if (ptr)
		return ptr;

	if (goal != 0) {
		goal = 0;
		goto restart;
	}

	return NULL;
}

void * __init __alloc_bootmem_nopanic(unsigned long size, unsigned long align,
					unsigned long goal)
{
	unsigned long limit = -1UL;

	return ___alloc_bootmem_nopanic(size, align, goal, limit);
}

static void * __init ___alloc_bootmem(unsigned long size, unsigned long align,
					unsigned long goal, unsigned long limit)
{
	void *mem = ___alloc_bootmem_nopanic(size, align, goal, limit);

	if (mem)
		return mem;
	printk(KERN_ALERT "bootmem alloc of %lu bytes failed!\n", size);
	panic("Out of memory");
	return NULL;
}

void * __init __alloc_bootmem(unsigned long size, unsigned long align,
			      unsigned long goal)
{
	unsigned long limit = -1UL;

	return ___alloc_bootmem(size, align, goal, limit);
}

void * __init __alloc_bootmem_node(pg_data_t *pgdat, unsigned long size,
				   unsigned long align, unsigned long goal)
{
	void *ptr;

	if (WARN_ON_ONCE(slab_is_available()))
		return kzalloc_node(size, GFP_NOWAIT, pgdat->node_id);

again:
	ptr = __alloc_memory_core_early(pgdat->node_id, size, align,
					 goal, -1ULL);
	if (ptr)
		return ptr;

	ptr = __alloc_memory_core_early(MAX_NUMNODES, size, align,
					goal, -1ULL);
	if (!ptr && goal) {
		goal = 0;
		goto again;
	}
	return ptr;
}

void * __init __alloc_bootmem_node_high(pg_data_t *pgdat, unsigned long size,
				   unsigned long align, unsigned long goal)
{
	return __alloc_bootmem_node(pgdat, size, align, goal);
}

#ifdef CONFIG_SPARSEMEM
void * __init alloc_bootmem_section(unsigned long size,
				    unsigned long section_nr)
{
	unsigned long pfn, goal, limit;

	pfn = section_nr_to_pfn(section_nr);
	goal = pfn << PAGE_SHIFT;
	limit = section_nr_to_pfn(section_nr + 1) << PAGE_SHIFT;

	return __alloc_memory_core_early(early_pfn_to_nid(pfn), size,
					 SMP_CACHE_BYTES, goal, limit);
}
#endif

void * __init __alloc_bootmem_node_nopanic(pg_data_t *pgdat, unsigned long size,
				   unsigned long align, unsigned long goal)
{
	void *ptr;

	if (WARN_ON_ONCE(slab_is_available()))
		return kzalloc_node(size, GFP_NOWAIT, pgdat->node_id);

	ptr =  __alloc_memory_core_early(pgdat->node_id, size, align,
						 goal, -1ULL);
	if (ptr)
		return ptr;

	return __alloc_bootmem_nopanic(size, align, goal);
}

#ifndef ARCH_LOW_ADDRESS_LIMIT
#define ARCH_LOW_ADDRESS_LIMIT	0xffffffffUL
#endif

void * __init __alloc_bootmem_low(unsigned long size, unsigned long align,
				  unsigned long goal)
{
	return ___alloc_bootmem(size, align, goal, ARCH_LOW_ADDRESS_LIMIT);
}

void * __init __alloc_bootmem_low_node(pg_data_t *pgdat, unsigned long size,
				       unsigned long align, unsigned long goal)
{
	void *ptr;

	if (WARN_ON_ONCE(slab_is_available()))
		return kzalloc_node(size, GFP_NOWAIT, pgdat->node_id);

	ptr = __alloc_memory_core_early(pgdat->node_id, size, align,
				goal, ARCH_LOW_ADDRESS_LIMIT);
	if (ptr)
		return ptr;

	return  __alloc_memory_core_early(MAX_NUMNODES, size, align,
				goal, ARCH_LOW_ADDRESS_LIMIT);
}
