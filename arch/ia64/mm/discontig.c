/*
 * Copyright (c) 2000, 2003 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Tony Luck <tony.luck@intel.com>
 * Copyright (c) 2002 NEC Corp.
 * Copyright (c) 2002 Kimio Suganuma <k-suganuma@da.jp.nec.com>
 * Copyright (c) 2004 Silicon Graphics, Inc
 *	Russ Anderson <rja@sgi.com>
 *	Jesse Barnes <jbarnes@sgi.com>
 *	Jack Steiner <steiner@sgi.com>
 */


#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/swap.h>
#include <linux/bootmem.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <linux/nodemask.h>
#include <linux/slab.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>
#include <asm/meminit.h>
#include <asm/numa.h>
#include <asm/sections.h>

struct early_node_data {
	struct ia64_node_data *node_data;
	unsigned long pernode_addr;
	unsigned long pernode_size;
	unsigned long num_physpages;
#ifdef CONFIG_ZONE_DMA
	unsigned long num_dma_physpages;
#endif
	unsigned long min_pfn;
	unsigned long max_pfn;
};

static struct early_node_data mem_data[MAX_NUMNODES] __initdata;
static nodemask_t memory_less_mask __initdata;

pg_data_t *pgdat_list[MAX_NUMNODES];

#define MAX_NODE_ALIGN_OFFSET	(32 * 1024 * 1024)
#define NODEDATA_ALIGN(addr, node)						\
	((((addr) + 1024*1024-1) & ~(1024*1024-1)) + 				\
	     (((node)*PERCPU_PAGE_SIZE) & (MAX_NODE_ALIGN_OFFSET - 1)))

static int __init build_node_maps(unsigned long start, unsigned long len,
				  int node)
{
	unsigned long spfn, epfn, end = start + len;
	struct bootmem_data *bdp = &bootmem_node_data[node];

	epfn = GRANULEROUNDUP(end) >> PAGE_SHIFT;
	spfn = GRANULEROUNDDOWN(start) >> PAGE_SHIFT;

	if (!bdp->node_low_pfn) {
		bdp->node_min_pfn = spfn;
		bdp->node_low_pfn = epfn;
	} else {
		bdp->node_min_pfn = min(spfn, bdp->node_min_pfn);
		bdp->node_low_pfn = max(epfn, bdp->node_low_pfn);
	}

	return 0;
}

static int __meminit early_nr_cpus_node(int node)
{
	int cpu, n = 0;

	for_each_possible_early_cpu(cpu)
		if (node == node_cpuid[cpu].nid)
			n++;

	return n;
}

static unsigned long __meminit compute_pernodesize(int node)
{
	unsigned long pernodesize = 0, cpus;

	cpus = early_nr_cpus_node(node);
	pernodesize += PERCPU_PAGE_SIZE * cpus;
	pernodesize += node * L1_CACHE_BYTES;
	pernodesize += L1_CACHE_ALIGN(sizeof(pg_data_t));
	pernodesize += L1_CACHE_ALIGN(sizeof(struct ia64_node_data));
	pernodesize += L1_CACHE_ALIGN(sizeof(pg_data_t));
	pernodesize = PAGE_ALIGN(pernodesize);
	return pernodesize;
}

static void *per_cpu_node_setup(void *cpu_data, int node)
{
#ifdef CONFIG_SMP
	int cpu;

	for_each_possible_early_cpu(cpu) {
		void *src = cpu == 0 ? __cpu0_per_cpu : __phys_per_cpu_start;

		if (node != node_cpuid[cpu].nid)
			continue;

		memcpy(__va(cpu_data), src, __per_cpu_end - __per_cpu_start);
		__per_cpu_offset[cpu] = (char *)__va(cpu_data) -
			__per_cpu_start;

		if (cpu == 0)
			ia64_set_kr(IA64_KR_PER_CPU_DATA,
				    (unsigned long)cpu_data -
				    (unsigned long)__per_cpu_start);

		cpu_data += PERCPU_PAGE_SIZE;
	}
#endif
	return cpu_data;
}

#ifdef CONFIG_SMP
void __init setup_per_cpu_areas(void)
{
	struct pcpu_alloc_info *ai;
	struct pcpu_group_info *uninitialized_var(gi);
	unsigned int *cpu_map;
	void *base;
	unsigned long base_offset;
	unsigned int cpu;
	ssize_t static_size, reserved_size, dyn_size;
	int node, prev_node, unit, nr_units, rc;

	ai = pcpu_alloc_alloc_info(MAX_NUMNODES, nr_cpu_ids);
	if (!ai)
		panic("failed to allocate pcpu_alloc_info");
	cpu_map = ai->groups[0].cpu_map;

	
	base = (void *)ULONG_MAX;
	for_each_possible_cpu(cpu)
		base = min(base,
			   (void *)(__per_cpu_offset[cpu] + __per_cpu_start));
	base_offset = (void *)__per_cpu_start - base;

	
	unit = 0;
	for_each_node(node)
		for_each_possible_cpu(cpu)
			if (node == node_cpuid[cpu].nid)
				cpu_map[unit++] = cpu;
	nr_units = unit;

	
	static_size = __per_cpu_end - __per_cpu_start;
	reserved_size = PERCPU_MODULE_RESERVE;
	dyn_size = PERCPU_PAGE_SIZE - static_size - reserved_size;
	if (dyn_size < 0)
		panic("percpu area overflow static=%zd reserved=%zd\n",
		      static_size, reserved_size);

	ai->static_size		= static_size;
	ai->reserved_size	= reserved_size;
	ai->dyn_size		= dyn_size;
	ai->unit_size		= PERCPU_PAGE_SIZE;
	ai->atom_size		= PAGE_SIZE;
	ai->alloc_size		= PERCPU_PAGE_SIZE;

	prev_node = -1;
	ai->nr_groups = 0;
	for (unit = 0; unit < nr_units; unit++) {
		cpu = cpu_map[unit];
		node = node_cpuid[cpu].nid;

		if (node == prev_node) {
			gi->nr_units++;
			continue;
		}
		prev_node = node;

		gi = &ai->groups[ai->nr_groups++];
		gi->nr_units		= 1;
		gi->base_offset		= __per_cpu_offset[cpu] + base_offset;
		gi->cpu_map		= &cpu_map[unit];
	}

	rc = pcpu_setup_first_chunk(ai, base);
	if (rc)
		panic("failed to setup percpu area (err=%d)", rc);

	pcpu_free_alloc_info(ai);
}
#endif

static void __init fill_pernode(int node, unsigned long pernode,
	unsigned long pernodesize)
{
	void *cpu_data;
	int cpus = early_nr_cpus_node(node);
	struct bootmem_data *bdp = &bootmem_node_data[node];

	mem_data[node].pernode_addr = pernode;
	mem_data[node].pernode_size = pernodesize;
	memset(__va(pernode), 0, pernodesize);

	cpu_data = (void *)pernode;
	pernode += PERCPU_PAGE_SIZE * cpus;
	pernode += node * L1_CACHE_BYTES;

	pgdat_list[node] = __va(pernode);
	pernode += L1_CACHE_ALIGN(sizeof(pg_data_t));

	mem_data[node].node_data = __va(pernode);
	pernode += L1_CACHE_ALIGN(sizeof(struct ia64_node_data));

	pgdat_list[node]->bdata = bdp;
	pernode += L1_CACHE_ALIGN(sizeof(pg_data_t));

	cpu_data = per_cpu_node_setup(cpu_data, node);

	return;
}

static int __init find_pernode_space(unsigned long start, unsigned long len,
				     int node)
{
	unsigned long spfn, epfn;
	unsigned long pernodesize = 0, pernode, pages, mapsize;
	struct bootmem_data *bdp = &bootmem_node_data[node];

	spfn = start >> PAGE_SHIFT;
	epfn = (start + len) >> PAGE_SHIFT;

	pages = bdp->node_low_pfn - bdp->node_min_pfn;
	mapsize = bootmem_bootmap_pages(pages) << PAGE_SHIFT;

	if (spfn < bdp->node_min_pfn || epfn > bdp->node_low_pfn)
		return 0;

	
	if (mem_data[node].pernode_addr)
		return 0;

	pernodesize = compute_pernodesize(node);
	pernode = NODEDATA_ALIGN(start, node);

	
	if (start + len > (pernode + pernodesize + mapsize))
		fill_pernode(node, pernode, pernodesize);

	return 0;
}

static int __init free_node_bootmem(unsigned long start, unsigned long len,
				    int node)
{
	free_bootmem_node(pgdat_list[node], start, len);

	return 0;
}

static void __init reserve_pernode_space(void)
{
	unsigned long base, size, pages;
	struct bootmem_data *bdp;
	int node;

	for_each_online_node(node) {
		pg_data_t *pdp = pgdat_list[node];

		if (node_isset(node, memory_less_mask))
			continue;

		bdp = pdp->bdata;

		
		pages = bdp->node_low_pfn - bdp->node_min_pfn;
		size = bootmem_bootmap_pages(pages) << PAGE_SHIFT;
		base = __pa(bdp->node_bootmem_map);
		reserve_bootmem_node(pdp, base, size, BOOTMEM_DEFAULT);

		
		size = mem_data[node].pernode_size;
		base = __pa(mem_data[node].pernode_addr);
		reserve_bootmem_node(pdp, base, size, BOOTMEM_DEFAULT);
	}
}

static void __meminit scatter_node_data(void)
{
	pg_data_t **dst;
	int node;

	for_each_node(node) {
		if (pgdat_list[node]) {
			dst = LOCAL_DATA_ADDR(pgdat_list[node])->pg_data_ptrs;
			memcpy(dst, pgdat_list, sizeof(pgdat_list));
		}
	}
}

static void __init initialize_pernode_data(void)
{
	int cpu, node;

	scatter_node_data();

#ifdef CONFIG_SMP
	
	for_each_possible_early_cpu(cpu) {
		node = node_cpuid[cpu].nid;
		per_cpu(ia64_cpu_info, cpu).node_data =
			mem_data[node].node_data;
	}
#else
	{
		struct cpuinfo_ia64 *cpu0_cpu_info;
		cpu = 0;
		node = node_cpuid[cpu].nid;
		cpu0_cpu_info = (struct cpuinfo_ia64 *)(__phys_per_cpu_start +
			((char *)&ia64_cpu_info - __per_cpu_start));
		cpu0_cpu_info->node_data = mem_data[node].node_data;
	}
#endif 
}

static void __init *memory_less_node_alloc(int nid, unsigned long pernodesize)
{
	void *ptr = NULL;
	u8 best = 0xff;
	int bestnode = -1, node, anynode = 0;

	for_each_online_node(node) {
		if (node_isset(node, memory_less_mask))
			continue;
		else if (node_distance(nid, node) < best) {
			best = node_distance(nid, node);
			bestnode = node;
		}
		anynode = node;
	}

	if (bestnode == -1)
		bestnode = anynode;

	ptr = __alloc_bootmem_node(pgdat_list[bestnode], pernodesize,
		PERCPU_PAGE_SIZE, __pa(MAX_DMA_ADDRESS));

	return ptr;
}

static void __init memory_less_nodes(void)
{
	unsigned long pernodesize;
	void *pernode;
	int node;

	for_each_node_mask(node, memory_less_mask) {
		pernodesize = compute_pernodesize(node);
		pernode = memory_less_node_alloc(node, pernodesize);
		fill_pernode(node, __pa(pernode), pernodesize);
	}

	return;
}

void __init find_memory(void)
{
	int node;

	reserve_memory();

	if (num_online_nodes() == 0) {
		printk(KERN_ERR "node info missing!\n");
		node_set_online(0);
	}

	nodes_or(memory_less_mask, memory_less_mask, node_online_map);
	min_low_pfn = -1;
	max_low_pfn = 0;

	
	efi_memmap_walk(filter_rsvd_memory, build_node_maps);
	efi_memmap_walk(filter_rsvd_memory, find_pernode_space);
	efi_memmap_walk(find_max_min_low_pfn, NULL);

	for_each_online_node(node)
		if (bootmem_node_data[node].node_low_pfn) {
			node_clear(node, memory_less_mask);
			mem_data[node].min_pfn = ~0UL;
		}

	efi_memmap_walk(filter_memory, register_active_ranges);

	for (node = MAX_NUMNODES - 1; node >= 0; node--) {
		unsigned long pernode, pernodesize, map;
		struct bootmem_data *bdp;

		if (!node_online(node))
			continue;
		else if (node_isset(node, memory_less_mask))
			continue;

		bdp = &bootmem_node_data[node];
		pernode = mem_data[node].pernode_addr;
		pernodesize = mem_data[node].pernode_size;
		map = pernode + pernodesize;

		init_bootmem_node(pgdat_list[node],
				  map>>PAGE_SHIFT,
				  bdp->node_min_pfn,
				  bdp->node_low_pfn);
	}

	efi_memmap_walk(filter_rsvd_memory, free_node_bootmem);

	reserve_pernode_space();
	memory_less_nodes();
	initialize_pernode_data();

	max_pfn = max_low_pfn;

	find_initrd();
}

#ifdef CONFIG_SMP
void __cpuinit *per_cpu_init(void)
{
	int cpu;
	static int first_time = 1;

	if (first_time) {
		first_time = 0;
		for_each_possible_early_cpu(cpu)
			per_cpu(local_per_cpu_offset, cpu) = __per_cpu_offset[cpu];
	}

	return __per_cpu_start + __per_cpu_offset[smp_processor_id()];
}
#endif 

void show_mem(unsigned int filter)
{
	int i, total_reserved = 0;
	int total_shared = 0, total_cached = 0;
	unsigned long total_present = 0;
	pg_data_t *pgdat;

	printk(KERN_INFO "Mem-info:\n");
	show_free_areas(filter);
	printk(KERN_INFO "Node memory in pages:\n");
	for_each_online_pgdat(pgdat) {
		unsigned long present;
		unsigned long flags;
		int shared = 0, cached = 0, reserved = 0;
		int nid = pgdat->node_id;

		if (skip_free_areas_node(filter, nid))
			continue;
		pgdat_resize_lock(pgdat, &flags);
		present = pgdat->node_present_pages;
		for(i = 0; i < pgdat->node_spanned_pages; i++) {
			struct page *page;
			if (unlikely(i % MAX_ORDER_NR_PAGES == 0))
				touch_nmi_watchdog();
			if (pfn_valid(pgdat->node_start_pfn + i))
				page = pfn_to_page(pgdat->node_start_pfn + i);
			else {
				i = vmemmap_find_next_valid_pfn(nid, i) - 1;
				continue;
			}
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (page_count(page))
				shared += page_count(page)-1;
		}
		pgdat_resize_unlock(pgdat, &flags);
		total_present += present;
		total_reserved += reserved;
		total_cached += cached;
		total_shared += shared;
		printk(KERN_INFO "Node %4d:  RAM: %11ld, rsvd: %8d, "
		       "shrd: %10d, swpd: %10d\n", nid,
		       present, reserved, shared, cached);
	}
	printk(KERN_INFO "%ld pages of RAM\n", total_present);
	printk(KERN_INFO "%d reserved pages\n", total_reserved);
	printk(KERN_INFO "%d pages shared\n", total_shared);
	printk(KERN_INFO "%d pages swap cached\n", total_cached);
	printk(KERN_INFO "Total of %ld pages in page table cache\n",
	       quicklist_total_size());
	printk(KERN_INFO "%d free buffer pages\n", nr_free_buffer_pages());
}

void call_pernode_memory(unsigned long start, unsigned long len, void *arg)
{
	unsigned long rs, re, end = start + len;
	void (*func)(unsigned long, unsigned long, int);
	int i;

	start = PAGE_ALIGN(start);
	end &= PAGE_MASK;
	if (start >= end)
		return;

	func = arg;

	if (!num_node_memblks) {
		
		if (start < end)
			(*func)(start, end - start, 0);
		return;
	}

	for (i = 0; i < num_node_memblks; i++) {
		rs = max(start, node_memblk[i].start_paddr);
		re = min(end, node_memblk[i].start_paddr +
			 node_memblk[i].size);

		if (rs < re)
			(*func)(rs, re - rs, node_memblk[i].nid);

		if (re == end)
			break;
	}
}

static __init int count_node_pages(unsigned long start, unsigned long len, int node)
{
	unsigned long end = start + len;

	mem_data[node].num_physpages += len >> PAGE_SHIFT;
#ifdef CONFIG_ZONE_DMA
	if (start <= __pa(MAX_DMA_ADDRESS))
		mem_data[node].num_dma_physpages +=
			(min(end, __pa(MAX_DMA_ADDRESS)) - start) >>PAGE_SHIFT;
#endif
	start = GRANULEROUNDDOWN(start);
	end = GRANULEROUNDUP(end);
	mem_data[node].max_pfn = max(mem_data[node].max_pfn,
				     end >> PAGE_SHIFT);
	mem_data[node].min_pfn = min(mem_data[node].min_pfn,
				     start >> PAGE_SHIFT);

	return 0;
}

void __init paging_init(void)
{
	unsigned long max_dma;
	unsigned long pfn_offset = 0;
	unsigned long max_pfn = 0;
	int node;
	unsigned long max_zone_pfns[MAX_NR_ZONES];

	max_dma = virt_to_phys((void *) MAX_DMA_ADDRESS) >> PAGE_SHIFT;

	efi_memmap_walk(filter_rsvd_memory, count_node_pages);

	sparse_memory_present_with_active_regions(MAX_NUMNODES);
	sparse_init();

#ifdef CONFIG_VIRTUAL_MEM_MAP
	VMALLOC_END -= PAGE_ALIGN(ALIGN(max_low_pfn, MAX_ORDER_NR_PAGES) *
		sizeof(struct page));
	vmem_map = (struct page *) VMALLOC_END;
	efi_memmap_walk(create_mem_map_page_table, NULL);
	printk("Virtual mem_map starts at 0x%p\n", vmem_map);
#endif

	for_each_online_node(node) {
		num_physpages += mem_data[node].num_physpages;
		pfn_offset = mem_data[node].min_pfn;

#ifdef CONFIG_VIRTUAL_MEM_MAP
		NODE_DATA(node)->node_mem_map = vmem_map + pfn_offset;
#endif
		if (mem_data[node].max_pfn > max_pfn)
			max_pfn = mem_data[node].max_pfn;
	}

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));
#ifdef CONFIG_ZONE_DMA
	max_zone_pfns[ZONE_DMA] = max_dma;
#endif
	max_zone_pfns[ZONE_NORMAL] = max_pfn;
	free_area_init_nodes(max_zone_pfns);

	zero_page_memmap_ptr = virt_to_page(ia64_imva(empty_zero_page));
}

#ifdef CONFIG_MEMORY_HOTPLUG
pg_data_t *arch_alloc_nodedata(int nid)
{
	unsigned long size = compute_pernodesize(nid);

	return kzalloc(size, GFP_KERNEL);
}

void arch_free_nodedata(pg_data_t *pgdat)
{
	kfree(pgdat);
}

void arch_refresh_nodedata(int update_node, pg_data_t *update_pgdat)
{
	pgdat_list[update_node] = update_pgdat;
	scatter_node_data();
}
#endif

#ifdef CONFIG_SPARSEMEM_VMEMMAP
int __meminit vmemmap_populate(struct page *start_page,
						unsigned long size, int node)
{
	return vmemmap_populate_basepages(start_page, size, node);
}
#endif
