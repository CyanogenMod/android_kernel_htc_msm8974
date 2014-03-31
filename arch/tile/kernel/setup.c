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

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/node.h>
#include <linux/cpu.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/kexec.h>
#include <linux/pci.h>
#include <linux/initrd.h>
#include <linux/io.h>
#include <linux/highmem.h>
#include <linux/smp.h>
#include <linux/timex.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <hv/hypervisor.h>
#include <arch/interrupts.h>

#ifndef CONFIG_SMP
#define setup_max_cpus 1
#endif

static inline int ABS(int x) { return x >= 0 ? x : -x; }

char chip_model[64] __write_once;

struct pglist_data node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

static bootmem_data_t __initdata node0_bdata;

unsigned long __cpuinitdata node_start_pfn[MAX_NUMNODES];
unsigned long __cpuinitdata node_end_pfn[MAX_NUMNODES];
unsigned long __initdata node_memmap_pfn[MAX_NUMNODES];
unsigned long __initdata node_percpu_pfn[MAX_NUMNODES];
unsigned long __initdata node_free_pfn[MAX_NUMNODES];

static unsigned long __initdata node_percpu[MAX_NUMNODES];

#ifdef CONFIG_HIGHMEM
unsigned long __cpuinitdata node_lowmem_end_pfn[MAX_NUMNODES];

static unsigned long __initdata mappable_physpages;
#endif

int node_controller[MAX_NUMNODES] = { [0 ... MAX_NUMNODES-1] = -1 };

#ifdef CONFIG_HIGHMEM
unsigned long pbase_map[1 << (32 - HPAGE_SHIFT)]
  __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(pbase_map);

void *vbase_map[NR_PA_HIGHBIT_VALUES]
  __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(vbase_map);
#endif

int highbits_to_node[NR_PA_HIGHBIT_VALUES] __write_once;
EXPORT_SYMBOL(highbits_to_node);

static unsigned int __initdata maxmem_pfn = -1U;
static unsigned int __initdata maxnodemem_pfn[MAX_NUMNODES] = {
	[0 ... MAX_NUMNODES-1] = -1U
};
static nodemask_t __initdata isolnodes;

#ifdef CONFIG_PCI
enum { DEFAULT_PCI_RESERVE_MB = 64 };
static unsigned int __initdata pci_reserve_mb = DEFAULT_PCI_RESERVE_MB;
unsigned long __initdata pci_reserve_start_pfn = -1U;
unsigned long __initdata pci_reserve_end_pfn = -1U;
#endif

static int __init setup_maxmem(char *str)
{
	unsigned long long maxmem;
	if (str == NULL || (maxmem = memparse(str, NULL)) == 0)
		return -EINVAL;

	maxmem_pfn = (maxmem >> HPAGE_SHIFT) << (HPAGE_SHIFT - PAGE_SHIFT);
	pr_info("Forcing RAM used to no more than %dMB\n",
	       maxmem_pfn >> (20 - PAGE_SHIFT));
	return 0;
}
early_param("maxmem", setup_maxmem);

static int __init setup_maxnodemem(char *str)
{
	char *endp;
	unsigned long long maxnodemem;
	long node;

	node = str ? simple_strtoul(str, &endp, 0) : INT_MAX;
	if (node >= MAX_NUMNODES || *endp != ':')
		return -EINVAL;

	maxnodemem = memparse(endp+1, NULL);
	maxnodemem_pfn[node] = (maxnodemem >> HPAGE_SHIFT) <<
		(HPAGE_SHIFT - PAGE_SHIFT);
	pr_info("Forcing RAM used on node %ld to no more than %dMB\n",
	       node, maxnodemem_pfn[node] >> (20 - PAGE_SHIFT));
	return 0;
}
early_param("maxnodemem", setup_maxnodemem);

static int __init setup_isolnodes(char *str)
{
	char buf[MAX_NUMNODES * 5];
	if (str == NULL || nodelist_parse(str, isolnodes) != 0)
		return -EINVAL;

	nodelist_scnprintf(buf, sizeof(buf), isolnodes);
	pr_info("Set isolnodes value to '%s'\n", buf);
	return 0;
}
early_param("isolnodes", setup_isolnodes);

#ifdef CONFIG_PCI
static int __init setup_pci_reserve(char* str)
{
	unsigned long mb;

	if (str == NULL || strict_strtoul(str, 0, &mb) != 0 ||
	    mb > 3 * 1024)
		return -EINVAL;

	pci_reserve_mb = mb;
	pr_info("Reserving %dMB for PCIE root complex mappings\n",
	       pci_reserve_mb);
	return 0;
}
early_param("pci_reserve", setup_pci_reserve);
#endif

#ifndef __tilegx__
static int __init parse_vmalloc(char *arg)
{
	if (!arg)
		return -EINVAL;

	VMALLOC_RESERVE = (memparse(arg, &arg) + PGDIR_SIZE - 1) & PGDIR_MASK;

	
	if ((long)_VMALLOC_START >= 0)
		early_panic("\"vmalloc=%#lx\" value too large: maximum %#lx\n",
			    VMALLOC_RESERVE, _VMALLOC_END - 0x80000000UL);

	return 0;
}
early_param("vmalloc", parse_vmalloc);
#endif

#ifdef CONFIG_HIGHMEM
static void *__init setup_pa_va_mapping(void)
{
	unsigned long curr_pages = 0;
	unsigned long vaddr = PAGE_OFFSET;
	nodemask_t highonlynodes = isolnodes;
	int i, j;

	memset(pbase_map, -1, sizeof(pbase_map));
	memset(vbase_map, -1, sizeof(vbase_map));

	
	node_clear(0, highonlynodes);

	
	mappable_physpages = 0;
	for_each_online_node(i) {
		if (!node_isset(i, highonlynodes))
			mappable_physpages +=
				node_end_pfn[i] - node_start_pfn[i];
	}

	for_each_online_node(i) {
		unsigned long start = node_start_pfn[i];
		unsigned long end = node_end_pfn[i];
		unsigned long size = end - start;
		unsigned long vaddr_end;

		if (node_isset(i, highonlynodes)) {
			
			node_lowmem_end_pfn[i] = start;
			continue;
		}

		curr_pages += size;
		if (mappable_physpages > MAXMEM_PFN) {
			vaddr_end = PAGE_OFFSET +
				(((u64)curr_pages * MAXMEM_PFN /
				  mappable_physpages)
				 << PAGE_SHIFT);
		} else {
			vaddr_end = PAGE_OFFSET + (curr_pages << PAGE_SHIFT);
		}
		for (j = 0; vaddr < vaddr_end; vaddr += HPAGE_SIZE, ++j) {
			unsigned long this_pfn =
				start + (j << HUGETLB_PAGE_ORDER);
			pbase_map[vaddr >> HPAGE_SHIFT] = this_pfn;
			if (vbase_map[__pfn_to_highbits(this_pfn)] ==
			    (void *)-1)
				vbase_map[__pfn_to_highbits(this_pfn)] =
					(void *)(vaddr & HPAGE_MASK);
		}
		node_lowmem_end_pfn[i] = start + (j << HUGETLB_PAGE_ORDER);
		BUG_ON(node_lowmem_end_pfn[i] > end);
	}

	
	return (void *)vaddr;
}
#endif 

static void __cpuinit store_permanent_mappings(void)
{
	int i;

	for_each_online_node(i) {
		HV_PhysAddr pa = ((HV_PhysAddr)node_start_pfn[i]) << PAGE_SHIFT;
#ifdef CONFIG_HIGHMEM
		HV_PhysAddr high_mapped_pa = node_lowmem_end_pfn[i];
#else
		HV_PhysAddr high_mapped_pa = node_end_pfn[i];
#endif

		unsigned long pages = high_mapped_pa - node_start_pfn[i];
		HV_VirtAddr addr = (HV_VirtAddr) __va(pa);
		hv_store_mapping(addr, pages << PAGE_SHIFT, pa);
	}

	hv_store_mapping((HV_VirtAddr)_stext,
			 (uint32_t)(_einittext - _stext), 0);
}

static void __init setup_memory(void)
{
	int i, j;
	int highbits_seen[NR_PA_HIGHBIT_VALUES] = { 0 };
#ifdef CONFIG_HIGHMEM
	long highmem_pages;
#endif
#ifndef __tilegx__
	int cap;
#endif
#if defined(CONFIG_HIGHMEM) || defined(__tilegx__)
	long lowmem_pages;
#endif

	
	BUILD_BUG_ON(MAX_NUMNODES > 127);

	
	for (i = 0; ; ++i) {
		unsigned long start, size, end, highbits;
		HV_PhysAddrRange range = hv_inquire_physical(i);
		if (range.size == 0)
			break;
#ifdef CONFIG_FLATMEM
		if (i > 0) {
			pr_err("Can't use discontiguous PAs: %#llx..%#llx\n",
			       range.size, range.start + range.size);
			continue;
		}
#endif
#ifndef __tilegx__
		if ((unsigned long)range.start) {
			pr_err("Range not at 4GB multiple: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
#endif
		if ((range.start & (HPAGE_SIZE-1)) != 0 ||
		    (range.size & (HPAGE_SIZE-1)) != 0) {
			unsigned long long start_pa = range.start;
			unsigned long long orig_size = range.size;
			range.start = (start_pa + HPAGE_SIZE - 1) & HPAGE_MASK;
			range.size -= (range.start - start_pa);
			range.size &= HPAGE_MASK;
			pr_err("Range not hugepage-aligned: %#llx..%#llx:"
			       " now %#llx-%#llx\n",
			       start_pa, start_pa + orig_size,
			       range.start, range.start + range.size);
		}
		highbits = __pa_to_highbits(range.start);
		if (highbits >= NR_PA_HIGHBIT_VALUES) {
			pr_err("PA high bits too high: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
		if (highbits_seen[highbits]) {
			pr_err("Range overlaps in high bits: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
		highbits_seen[highbits] = 1;
		if (PFN_DOWN(range.size) > maxnodemem_pfn[i]) {
			int max_size = maxnodemem_pfn[i];
			if (max_size > 0) {
				pr_err("Maxnodemem reduced node %d to"
				       " %d pages\n", i, max_size);
				range.size = PFN_PHYS(max_size);
			} else {
				pr_err("Maxnodemem disabled node %d\n", i);
				continue;
			}
		}
		if (num_physpages + PFN_DOWN(range.size) > maxmem_pfn) {
			int max_size = maxmem_pfn - num_physpages;
			if (max_size > 0) {
				pr_err("Maxmem reduced node %d to %d pages\n",
				       i, max_size);
				range.size = PFN_PHYS(max_size);
			} else {
				pr_err("Maxmem disabled node %d\n", i);
				continue;
			}
		}
		if (i >= MAX_NUMNODES) {
			pr_err("Too many PA nodes (#%d): %#llx...%#llx\n",
			       i, range.size, range.size + range.start);
			continue;
		}

		start = range.start >> PAGE_SHIFT;
		size = range.size >> PAGE_SHIFT;
		end = start + size;

#ifndef __tilegx__
		if (((HV_PhysAddr)end << PAGE_SHIFT) !=
		    (range.start + range.size)) {
			pr_err("PAs too high to represent: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
#endif
#ifdef CONFIG_PCI
		if (start <= pci_reserve_start_pfn &&
		    end > pci_reserve_start_pfn) {
			unsigned int per_cpu_size =
				__per_cpu_end - __per_cpu_start;
			unsigned int percpu_pages =
				NR_CPUS * (PFN_UP(per_cpu_size) >> PAGE_SHIFT);
			if (end < pci_reserve_end_pfn + percpu_pages) {
				end = pci_reserve_start_pfn;
				pr_err("PCI mapping region reduced node %d to"
				       " %ld pages\n", i, end - start);
			}
		}
#endif

		for (j = __pfn_to_highbits(start);
		     j <= __pfn_to_highbits(end - 1); j++)
			highbits_to_node[j] = i;

		node_start_pfn[i] = start;
		node_end_pfn[i] = end;
		node_controller[i] = range.controller;
		num_physpages += size;
		max_pfn = end;

		
		node_set(i, node_online_map);
		node_set(i, node_possible_map);
	}

#ifndef __tilegx__
	cap = 8 * 1024 * 1024;  
	if (num_physpages > cap) {
		int num_nodes = num_online_nodes();
		int cap_each = cap / num_nodes;
		unsigned long dropped_pages = 0;
		for (i = 0; i < num_nodes; ++i) {
			int size = node_end_pfn[i] - node_start_pfn[i];
			if (size > cap_each) {
				dropped_pages += (size - cap_each);
				node_end_pfn[i] = node_start_pfn[i] + cap_each;
			}
		}
		num_physpages -= dropped_pages;
		pr_warning("Only using %ldMB memory;"
		       " ignoring %ldMB.\n",
		       num_physpages >> (20 - PAGE_SHIFT),
		       dropped_pages >> (20 - PAGE_SHIFT));
		pr_warning("Consider using a larger page size.\n");
	}
#endif

	
	min_low_pfn = PFN_UP((unsigned long)_end - PAGE_OFFSET);

#ifdef CONFIG_HIGHMEM
	
	high_memory = setup_pa_va_mapping();

	
	max_low_pfn = node_lowmem_end_pfn[0];

	lowmem_pages = (mappable_physpages > MAXMEM_PFN) ?
		MAXMEM_PFN : mappable_physpages;
	highmem_pages = (long) (num_physpages - lowmem_pages);

	pr_notice("%ldMB HIGHMEM available.\n",
	       pages_to_mb(highmem_pages > 0 ? highmem_pages : 0));
	pr_notice("%ldMB LOWMEM available.\n",
			pages_to_mb(lowmem_pages));
#else
	
	max_low_pfn = node_end_pfn[0];

#ifndef __tilegx__
	if (node_end_pfn[0] > MAXMEM_PFN) {
		pr_warning("Only using %ldMB LOWMEM.\n",
		       MAXMEM>>20);
		pr_warning("Use a HIGHMEM enabled kernel.\n");
		max_low_pfn = MAXMEM_PFN;
		max_pfn = MAXMEM_PFN;
		num_physpages = MAXMEM_PFN;
		node_end_pfn[0] = MAXMEM_PFN;
	} else {
		pr_notice("%ldMB memory available.\n",
		       pages_to_mb(node_end_pfn[0]));
	}
	for (i = 1; i < MAX_NUMNODES; ++i) {
		node_start_pfn[i] = 0;
		node_end_pfn[i] = 0;
	}
	high_memory = __va(node_end_pfn[0]);
#else
	lowmem_pages = 0;
	for (i = 0; i < MAX_NUMNODES; ++i) {
		int pages = node_end_pfn[i] - node_start_pfn[i];
		lowmem_pages += pages;
		if (pages)
			high_memory = pfn_to_kaddr(node_end_pfn[i]);
	}
	pr_notice("%ldMB memory available.\n",
	       pages_to_mb(lowmem_pages));
#endif
#endif
}

static void __init setup_bootmem_allocator(void)
{
	unsigned long bootmap_size, first_alloc_pfn, last_alloc_pfn;

	
	NODE_DATA(0)->bdata = &node0_bdata;

#ifdef CONFIG_PCI
	
	last_alloc_pfn = min(max_low_pfn, pci_reserve_start_pfn);
#else
	last_alloc_pfn = max_low_pfn;
#endif

	bootmap_size = init_bootmem(min_low_pfn, last_alloc_pfn);

	first_alloc_pfn = min_low_pfn + PFN_UP(bootmap_size);
	if (first_alloc_pfn >= last_alloc_pfn)
		early_panic("Not enough memory on controller 0 for bootmem\n");

	free_bootmem(PFN_PHYS(first_alloc_pfn),
		     PFN_PHYS(last_alloc_pfn - first_alloc_pfn));

#ifdef CONFIG_KEXEC
	if (crashk_res.start != crashk_res.end)
		reserve_bootmem(crashk_res.start, resource_size(&crashk_res), 0);
#endif
}

void *__init alloc_remap(int nid, unsigned long size)
{
	int pages = node_end_pfn[nid] - node_start_pfn[nid];
	void *map = pfn_to_kaddr(node_memmap_pfn[nid]);
	BUG_ON(size != pages * sizeof(struct page));
	memset(map, 0, size);
	return map;
}

static int __init percpu_size(void)
{
	int size = __per_cpu_end - __per_cpu_start;
	size += PERCPU_MODULE_RESERVE;
	size += PERCPU_DYNAMIC_EARLY_SIZE;
	if (size < PCPU_MIN_UNIT_SIZE)
		size = PCPU_MIN_UNIT_SIZE;
	size = roundup(size, PAGE_SIZE);

	
	BUG_ON(kdata_huge && size > HPAGE_SIZE);
	return size;
}

static inline unsigned long alloc_bootmem_pfn(int size, unsigned long goal)
{
	void *kva = __alloc_bootmem(size, PAGE_SIZE, goal);
	unsigned long pfn = kaddr_to_pfn(kva);
	BUG_ON(goal && PFN_PHYS(pfn) != goal);
	return pfn;
}

static void __init zone_sizes_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = { 0 };
	int size = percpu_size();
	int num_cpus = smp_height * smp_width;
	int i;

	for (i = 0; i < num_cpus; ++i)
		node_percpu[cpu_to_node(i)] += size;

	for_each_online_node(i) {
		unsigned long start = node_start_pfn[i];
		unsigned long end = node_end_pfn[i];
#ifdef CONFIG_HIGHMEM
		unsigned long lowmem_end = node_lowmem_end_pfn[i];
#else
		unsigned long lowmem_end = end;
#endif
		int memmap_size = (end - start) * sizeof(struct page);
		node_free_pfn[i] = start;

		if (__pfn_to_highbits(start) == 0) {
			
			unsigned long goal = 0;
			node_memmap_pfn[i] =
				alloc_bootmem_pfn(memmap_size, goal);
			if (kdata_huge)
				goal = PFN_PHYS(lowmem_end) - node_percpu[i];
			if (node_percpu[i])
				node_percpu_pfn[i] =
				    alloc_bootmem_pfn(node_percpu[i], goal);
		} else if (cpu_isset(i, isolnodes)) {
			node_memmap_pfn[i] = alloc_bootmem_pfn(memmap_size, 0);
			BUG_ON(node_percpu[i] != 0);
		} else {
			
			node_memmap_pfn[i] = node_free_pfn[i];
			node_free_pfn[i] += PFN_UP(memmap_size);
			if (!kdata_huge) {
				node_percpu_pfn[i] = node_free_pfn[i];
				node_free_pfn[i] += PFN_UP(node_percpu[i]);
			} else {
				node_percpu_pfn[i] =
					lowmem_end - PFN_UP(node_percpu[i]);
			}
		}

#ifdef CONFIG_HIGHMEM
		if (start > lowmem_end) {
			zones_size[ZONE_NORMAL] = 0;
			zones_size[ZONE_HIGHMEM] = end - start;
		} else {
			zones_size[ZONE_NORMAL] = lowmem_end - start;
			zones_size[ZONE_HIGHMEM] = end - lowmem_end;
		}
#else
		zones_size[ZONE_NORMAL] = end - start;
#endif

		NODE_DATA(i)->bdata = NODE_DATA(0)->bdata;

		free_area_init_node(i, zones_size, start, NULL);
		printk(KERN_DEBUG "  Normal zone: %ld per-cpu pages\n",
		       PFN_UP(node_percpu[i]));

		
		if (zones_size[ZONE_NORMAL])
			node_set_state(i, N_NORMAL_MEMORY);
#ifdef CONFIG_HIGHMEM
		if (end != start)
			node_set_state(i, N_HIGH_MEMORY);
#endif

		node_set_online(i);
	}
}

#ifdef CONFIG_NUMA

struct cpumask node_2_cpu_mask[MAX_NUMNODES] __write_once;
EXPORT_SYMBOL(node_2_cpu_mask);

char cpu_2_node[NR_CPUS] __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(cpu_2_node);

static int __init cpu_to_bound_node(int cpu, struct cpumask* unbound_cpus)
{
	if (!cpu_possible(cpu) || cpumask_test_cpu(cpu, unbound_cpus))
		return -1;
	else
		return cpu_to_node(cpu);
}

static int __init node_neighbors(int node, int cpu,
				 struct cpumask *unbound_cpus)
{
	int neighbors = 0;
	int w = smp_width;
	int h = smp_height;
	int x = cpu % w;
	int y = cpu / w;
	if (x > 0 && cpu_to_bound_node(cpu-1, unbound_cpus) == node)
		++neighbors;
	if (x < w-1 && cpu_to_bound_node(cpu+1, unbound_cpus) == node)
		++neighbors;
	if (y > 0 && cpu_to_bound_node(cpu-w, unbound_cpus) == node)
		++neighbors;
	if (y < h-1 && cpu_to_bound_node(cpu+w, unbound_cpus) == node)
		++neighbors;
	return neighbors;
}

static void __init setup_numa_mapping(void)
{
	int distance[MAX_NUMNODES][NR_CPUS];
	HV_Coord coord;
	int cpu, node, cpus, i, x, y;
	int num_nodes = num_online_nodes();
	struct cpumask unbound_cpus;
	nodemask_t default_nodes;

	cpumask_clear(&unbound_cpus);

	
	nodes_andnot(default_nodes, node_online_map, isolnodes);
	if (nodes_empty(default_nodes)) {
		BUG_ON(!node_isset(0, node_online_map));
		pr_err("Forcing NUMA node zero available as a default node\n");
		node_set(0, default_nodes);
	}

	
	memset(distance, -1, sizeof(distance));
	cpu = 0;
	for (coord.y = 0; coord.y < smp_height; ++coord.y) {
		for (coord.x = 0; coord.x < smp_width;
		     ++coord.x, ++cpu) {
			BUG_ON(cpu >= nr_cpu_ids);
			if (!cpu_possible(cpu)) {
				cpu_2_node[cpu] = -1;
				continue;
			}
			for_each_node_mask(node, default_nodes) {
				HV_MemoryControllerInfo info =
					hv_inquire_memory_controller(
						coord, node_controller[node]);
				distance[node][cpu] =
					ABS(info.coord.x) + ABS(info.coord.y);
			}
			cpumask_set_cpu(cpu, &unbound_cpus);
		}
	}
	cpus = cpu;

	node = first_node(default_nodes);
	while (!cpumask_empty(&unbound_cpus)) {
		int best_cpu = -1;
		int best_distance = INT_MAX;
		for (cpu = 0; cpu < cpus; ++cpu) {
			if (cpumask_test_cpu(cpu, &unbound_cpus)) {
				int d = distance[node][cpu] * num_nodes;
				for_each_node_mask(i, default_nodes) {
					if (i != node)
						d -= distance[i][cpu];
				}
				d *= 8;  
				d -= node_neighbors(node, cpu, &unbound_cpus);
				if (d < best_distance) {
					best_cpu = cpu;
					best_distance = d;
				}
			}
		}
		BUG_ON(best_cpu < 0);
		cpumask_set_cpu(best_cpu, &node_2_cpu_mask[node]);
		cpu_2_node[best_cpu] = node;
		cpumask_clear_cpu(best_cpu, &unbound_cpus);
		node = next_node(node, default_nodes);
		if (node == MAX_NUMNODES)
			node = first_node(default_nodes);
	}

	
	cpu = 0;
	for (y = 0; y < smp_height; ++y) {
		printk(KERN_DEBUG "NUMA cpu-to-node row %d:", y);
		for (x = 0; x < smp_width; ++x, ++cpu) {
			if (cpu_to_node(cpu) < 0) {
				pr_cont(" -");
				cpu_2_node[cpu] = first_node(default_nodes);
			} else {
				pr_cont(" %d", cpu_to_node(cpu));
			}
		}
		pr_cont("\n");
	}
}

static struct cpu cpu_devices[NR_CPUS];

static int __init topology_init(void)
{
	int i;

	for_each_online_node(i)
		register_one_node(i);

	for (i = 0; i < smp_height * smp_width; ++i)
		register_cpu(&cpu_devices[i], i);

	return 0;
}

subsys_initcall(topology_init);

#else 

#define setup_numa_mapping() do { } while (0)

#endif 

void __cpuinit setup_cpu(int boot)
{
	
	if (!boot)
		store_permanent_mappings();

	
#if CHIP_HAS_TILE_DMA()
	arch_local_irq_unmask(INT_DMATLB_MISS);
	arch_local_irq_unmask(INT_DMATLB_ACCESS);
#endif
#if CHIP_HAS_SN_PROC()
	arch_local_irq_unmask(INT_SNITLB_MISS);
#endif
#ifdef __tilegx__
	arch_local_irq_unmask(INT_SINGLE_STEP_K);
#endif

	__insn_mtspr(SPR_MPL_WORLD_ACCESS_SET_0, 1);

#if CHIP_HAS_SN()
	
	__insn_mtspr(SPR_MPL_SN_ACCESS_SET_0, 1);
#endif
#if CHIP_HAS_SN_PROC()
	__insn_mtspr(SPR_MPL_SN_NOTIFY_SET_0, 1);
	__insn_mtspr(SPR_MPL_SN_CPL_SET_0, 1);
#endif

	__insn_mtspr(SPR_MPL_INTCTRL_0_SET_0, 1);
	__insn_mtspr(SPR_MPL_INTCTRL_1_SET_1, 1);

	
	setup_irq_regs();

#ifdef CONFIG_HARDWALL
	
	reset_network_state();
#endif
}

#ifdef CONFIG_BLK_DEV_INITRD

static int __initdata set_initramfs_file;
static char __initdata initramfs_file[128] = "initramfs.cpio.gz";

static int __init setup_initramfs_file(char *str)
{
	if (str == NULL)
		return -EINVAL;
	strncpy(initramfs_file, str, sizeof(initramfs_file) - 1);
	set_initramfs_file = 1;

	return 0;
}
early_param("initramfs_file", setup_initramfs_file);

static void __init load_hv_initrd(void)
{
	HV_FS_StatInfo stat;
	int fd, rc;
	void *initrd;

	fd = hv_fs_findfile((HV_VirtAddr) initramfs_file);
	if (fd == HV_ENOENT) {
		if (set_initramfs_file)
			pr_warning("No such hvfs initramfs file '%s'\n",
				   initramfs_file);
		return;
	}
	BUG_ON(fd < 0);
	stat = hv_fs_fstat(fd);
	BUG_ON(stat.size < 0);
	if (stat.flags & HV_FS_ISDIR) {
		pr_warning("Ignoring hvfs file '%s': it's a directory.\n",
			   initramfs_file);
		return;
	}
	initrd = alloc_bootmem_pages(stat.size);
	rc = hv_fs_pread(fd, (HV_VirtAddr) initrd, stat.size, 0);
	if (rc != stat.size) {
		pr_err("Error reading %d bytes from hvfs file '%s': %d\n",
		       stat.size, initramfs_file, rc);
		free_initrd_mem((unsigned long) initrd, stat.size);
		return;
	}
	initrd_start = (unsigned long) initrd;
	initrd_end = initrd_start + stat.size;
}

void __init free_initrd_mem(unsigned long begin, unsigned long end)
{
	free_bootmem(__pa(begin), end - begin);
}

#else
static inline void load_hv_initrd(void) {}
#endif 

static void __init validate_hv(void)
{
	unsigned long glue_size = hv_sysconf(HV_SYSCONF_GLUE_SIZE);
	int hv_page_size = hv_sysconf(HV_SYSCONF_PAGE_SIZE_SMALL);
	int hv_hpage_size = hv_sysconf(HV_SYSCONF_PAGE_SIZE_LARGE);
	HV_ASIDRange asid_range;

#ifndef CONFIG_SMP
	HV_Topology topology = hv_inquire_topology();
	BUG_ON(topology.coord.x != 0 || topology.coord.y != 0);
	if (topology.width != 1 || topology.height != 1) {
		pr_warning("Warning: booting UP kernel on %dx%d grid;"
			   " will ignore all but first tile.\n",
			   topology.width, topology.height);
	}
#endif

	if (PAGE_OFFSET + HV_GLUE_START_CPA + glue_size > (unsigned long)_text)
		early_panic("Hypervisor glue size %ld is too big!\n",
			    glue_size);
	if (hv_page_size != PAGE_SIZE)
		early_panic("Hypervisor page size %#x != our %#lx\n",
			    hv_page_size, PAGE_SIZE);
	if (hv_hpage_size != HPAGE_SIZE)
		early_panic("Hypervisor huge page size %#x != our %#lx\n",
			    hv_hpage_size, HPAGE_SIZE);

#ifdef CONFIG_SMP
	if ((smp_height * smp_width) > nr_cpu_ids)
		early_panic("Hypervisor %d x %d grid too big for Linux"
			    " NR_CPUS %d\n", smp_height, smp_width,
			    nr_cpu_ids);
#endif

	asid_range = hv_inquire_asid(0);
	__get_cpu_var(current_asid) = min_asid = asid_range.start;
	max_asid = asid_range.start + asid_range.size - 1;

	if (hv_confstr(HV_CONFSTR_CHIP_MODEL, (HV_VirtAddr)chip_model,
		       sizeof(chip_model)) < 0) {
		pr_err("Warning: HV_CONFSTR_CHIP_MODEL not available\n");
		strlcpy(chip_model, "unknown", sizeof(chip_model));
	}
}

static void __init validate_va(void)
{
#ifndef __tilegx__   
	int i, user_kernel_ok = 0;
	unsigned long max_va = 0;
	unsigned long list_va =
		((PGD_LIST_OFFSET / sizeof(pgd_t)) << PGDIR_SHIFT);

	for (i = 0; ; ++i) {
		HV_VirtAddrRange range = hv_inquire_virtual(i);
		if (range.size == 0)
			break;
		if (range.start <= MEM_USER_INTRPT &&
		    range.start + range.size >= MEM_HV_INTRPT)
			user_kernel_ok = 1;
		if (range.start == 0)
			max_va = range.size;
		BUG_ON(range.start + range.size > list_va);
	}
	if (!user_kernel_ok)
		early_panic("Hypervisor not configured for user/kernel VAs\n");
	if (max_va == 0)
		early_panic("Hypervisor not configured for low VAs\n");
	if (max_va < KERNEL_HIGH_VADDR)
		early_panic("Hypervisor max VA %#lx smaller than %#lx\n",
			    max_va, KERNEL_HIGH_VADDR);

	
	if ((long)VMALLOC_START >= 0)
		early_panic(
			"Linux VMALLOC region below the 2GB line (%#lx)!\n"
			"Reconfigure the kernel with fewer NR_HUGE_VMAPS\n"
			"or smaller VMALLOC_RESERVE.\n",
			VMALLOC_START);
#endif
}

struct cpumask __write_once cpu_lotar_map;
EXPORT_SYMBOL(cpu_lotar_map);

#if CHIP_HAS_CBOX_HOME_MAP()
struct cpumask hash_for_home_map;
EXPORT_SYMBOL(hash_for_home_map);
#endif

struct cpumask __write_once cpu_cacheable_map;
EXPORT_SYMBOL(cpu_cacheable_map);

static __initdata struct cpumask disabled_map;

static int __init disabled_cpus(char *str)
{
	int boot_cpu = smp_processor_id();

	if (str == NULL || cpulist_parse_crop(str, &disabled_map) != 0)
		return -EINVAL;
	if (cpumask_test_cpu(boot_cpu, &disabled_map)) {
		pr_err("disabled_cpus: can't disable boot cpu %d\n", boot_cpu);
		cpumask_clear_cpu(boot_cpu, &disabled_map);
	}
	return 0;
}

early_param("disabled_cpus", disabled_cpus);

void __init print_disabled_cpus(void)
{
	if (!cpumask_empty(&disabled_map)) {
		char buf[100];
		cpulist_scnprintf(buf, sizeof(buf), &disabled_map);
		pr_info("CPUs not available for Linux: %s\n", buf);
	}
}

static void __init setup_cpu_maps(void)
{
	struct cpumask hv_disabled_map, cpu_possible_init;
	int boot_cpu = smp_processor_id();
	int cpus, i, rc;

	
	rc = hv_inquire_tiles(HV_INQ_TILES_AVAIL,
			      (HV_VirtAddr) cpumask_bits(&cpu_possible_init),
			      sizeof(cpu_cacheable_map));
	if (rc < 0)
		early_panic("hv_inquire_tiles(AVAIL) failed: rc %d\n", rc);
	if (!cpumask_test_cpu(boot_cpu, &cpu_possible_init))
		early_panic("Boot CPU %d disabled by hypervisor!\n", boot_cpu);

	
	cpumask_complement(&hv_disabled_map, &cpu_possible_init);

	
	cpumask_or(&disabled_map, &disabled_map, &hv_disabled_map);

	cpus = 1;                          
	cpumask_set_cpu(boot_cpu, &disabled_map);   
	for (i = 0; cpus < setup_max_cpus; ++i)
		if (!cpumask_test_cpu(i, &disabled_map))
			++cpus;
	for (; i < smp_height * smp_width; ++i)
		cpumask_set_cpu(i, &disabled_map);
	cpumask_clear_cpu(boot_cpu, &disabled_map); 
	for (i = smp_height * smp_width; i < NR_CPUS; ++i)
		cpumask_clear_cpu(i, &disabled_map);

	cpumask_andnot(&cpu_possible_init, &cpu_possible_init, &disabled_map);
	init_cpu_possible(&cpu_possible_init);

	
	rc = hv_inquire_tiles(HV_INQ_TILES_LOTAR,
			      (HV_VirtAddr) cpumask_bits(&cpu_lotar_map),
			      sizeof(cpu_lotar_map));
	if (rc < 0) {
		pr_err("warning: no HV_INQ_TILES_LOTAR; using AVAIL\n");
		cpu_lotar_map = *cpu_possible_mask;
	}

#if CHIP_HAS_CBOX_HOME_MAP()
	
	rc = hv_inquire_tiles(HV_INQ_TILES_HFH_CACHE,
			      (HV_VirtAddr) hash_for_home_map.bits,
			      sizeof(hash_for_home_map));
	if (rc < 0)
		early_panic("hv_inquire_tiles(HFH_CACHE) failed: rc %d\n", rc);
	cpumask_or(&cpu_cacheable_map, cpu_possible_mask, &hash_for_home_map);
#else
	cpu_cacheable_map = *cpu_possible_mask;
#endif
}


static int __init dataplane(char *str)
{
	pr_warning("WARNING: dataplane support disabled in this kernel\n");
	return 0;
}

early_param("dataplane", dataplane);

#ifdef CONFIG_CMDLINE_BOOL
static char __initdata builtin_cmdline[COMMAND_LINE_SIZE] = CONFIG_CMDLINE;
#endif

void __init setup_arch(char **cmdline_p)
{
	int len;

#if defined(CONFIG_CMDLINE_BOOL) && defined(CONFIG_CMDLINE_OVERRIDE)
	len = hv_get_command_line((HV_VirtAddr) boot_command_line,
				  COMMAND_LINE_SIZE);
	if (boot_command_line[0])
		pr_warning("WARNING: ignoring dynamic command line \"%s\"\n",
			   boot_command_line);
	strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
#else
	char *hv_cmdline;
#if defined(CONFIG_CMDLINE_BOOL)
	if (builtin_cmdline[0]) {
		int builtin_len = strlcpy(boot_command_line, builtin_cmdline,
					  COMMAND_LINE_SIZE);
		if (builtin_len < COMMAND_LINE_SIZE-1)
			boot_command_line[builtin_len++] = ' ';
		hv_cmdline = &boot_command_line[builtin_len];
		len = COMMAND_LINE_SIZE - builtin_len;
	} else
#endif
	{
		hv_cmdline = boot_command_line;
		len = COMMAND_LINE_SIZE;
	}
	len = hv_get_command_line((HV_VirtAddr) hv_cmdline, len);
	if (len < 0 || len > COMMAND_LINE_SIZE)
		early_panic("hv_get_command_line failed: %d\n", len);
#endif

	*cmdline_p = boot_command_line;

	
	parse_early_param();

	
	validate_hv();
	validate_va();

	setup_cpu_maps();


#ifdef CONFIG_PCI
	if (tile_pci_init() == 0)
		pci_reserve_mb = 0;

	
	pci_reserve_end_pfn  = (1 << (32 - PAGE_SHIFT));
	pci_reserve_start_pfn = pci_reserve_end_pfn -
		(pci_reserve_mb << (20 - PAGE_SHIFT));
#endif

	init_mm.start_code = (unsigned long) _text;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = (unsigned long) _end;

	setup_memory();
	store_permanent_mappings();
	setup_bootmem_allocator();


	paging_init();
	setup_numa_mapping();
	zone_sizes_init();
	set_page_homes();
	setup_cpu(1);
	setup_clock();
	load_hv_initrd();
}



unsigned long __per_cpu_offset[NR_CPUS] __write_once;
EXPORT_SYMBOL(__per_cpu_offset);

static size_t __initdata pfn_offset[MAX_NUMNODES] = { 0 };
static unsigned long __initdata percpu_pfn[NR_CPUS] = { 0 };

static void *__init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	int nid = cpu_to_node(cpu);
	unsigned long pfn = node_percpu_pfn[nid] + pfn_offset[nid];

	BUG_ON(size % PAGE_SIZE != 0);
	pfn_offset[nid] += size / PAGE_SIZE;
	BUG_ON(node_percpu[nid] < size);
	node_percpu[nid] -= size;
	if (percpu_pfn[cpu] == 0)
		percpu_pfn[cpu] = pfn;
	return pfn_to_kaddr(pfn);
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
}

static void __init pcpu_fc_populate_pte(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	BUG_ON(pgd_addr_invalid(addr));
	if (addr < VMALLOC_START || addr >= VMALLOC_END)
		panic("PCPU addr %#lx outside vmalloc range %#lx..%#lx;"
		      " try increasing CONFIG_VMALLOC_RESERVE\n",
		      addr, VMALLOC_START, VMALLOC_END);

	pgd = swapper_pg_dir + pgd_index(addr);
	pud = pud_offset(pgd, addr);
	BUG_ON(!pud_present(*pud));
	pmd = pmd_offset(pud, addr);
	if (pmd_present(*pmd)) {
		BUG_ON(pmd_huge_page(*pmd));
	} else {
		pte = __alloc_bootmem(L2_KERNEL_PGTABLE_SIZE,
				      HV_PAGE_TABLE_ALIGN, 0);
		pmd_populate_kernel(&init_mm, pmd, pte);
	}
}

void __init setup_per_cpu_areas(void)
{
	struct page *pg;
	unsigned long delta, pfn, lowmem_va;
	unsigned long size = percpu_size();
	char *ptr;
	int rc, cpu, i;

	rc = pcpu_page_first_chunk(PERCPU_MODULE_RESERVE, pcpu_fc_alloc,
				   pcpu_fc_free, pcpu_fc_populate_pte);
	if (rc < 0)
		panic("Cannot initialize percpu area (err=%d)", rc);

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu) {
		__per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];

		
		ptr = pcpu_base_addr + pcpu_unit_offsets[cpu];
		__finv_buffer(ptr, size);
		pfn = percpu_pfn[cpu];

		
		pg = pfn_to_page(pfn);
		for (i = 0; i < size; i += PAGE_SIZE, ++pfn, ++pg) {

			
			pte_t *ptep =
				virt_to_pte(NULL, (unsigned long)ptr + i);
			pte_t pte = *ptep;
			BUG_ON(pfn != pte_pfn(pte));
			pte = hv_pte_set_mode(pte, HV_PTE_MODE_CACHE_TILE_L3);
			pte = set_remote_cache_cpu(pte, cpu);
			set_pte(ptep, pte);

			
			lowmem_va = (unsigned long)pfn_to_kaddr(pfn);
			ptep = virt_to_pte(NULL, lowmem_va);
			if (pte_huge(*ptep)) {
				printk(KERN_DEBUG "early shatter of huge page"
				       " at %#lx\n", lowmem_va);
				shatter_pmd((pmd_t *)ptep);
				ptep = virt_to_pte(NULL, lowmem_va);
				BUG_ON(pte_huge(*ptep));
			}
			BUG_ON(pfn != pte_pfn(*ptep));
			set_pte(ptep, pte);
		}
	}

	
	set_my_cpu_offset(__per_cpu_offset[smp_processor_id()]);

	
	mb_incoherent();

	
	local_flush_tlb_all();
}

static struct resource data_resource = {
	.name	= "Kernel data",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

static struct resource code_resource = {
	.name	= "Kernel code",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

#ifdef CONFIG_PCI
static struct resource* __init
insert_non_bus_resource(void)
{
	struct resource *res =
		kzalloc(sizeof(struct resource), GFP_ATOMIC);
	res->name = "Non-Bus Physical Address Space";
	res->start = (1ULL << 32);
	res->end = -1LL;
	res->flags = IORESOURCE_BUSY | IORESOURCE_MEM;
	if (insert_resource(&iomem_resource, res)) {
		kfree(res);
		return NULL;
	}
	return res;
}
#endif

static struct resource* __init
insert_ram_resource(u64 start_pfn, u64 end_pfn)
{
	struct resource *res =
		kzalloc(sizeof(struct resource), GFP_ATOMIC);
	res->name = "System RAM";
	res->start = start_pfn << PAGE_SHIFT;
	res->end = (end_pfn << PAGE_SHIFT) - 1;
	res->flags = IORESOURCE_BUSY | IORESOURCE_MEM;
	if (insert_resource(&iomem_resource, res)) {
		kfree(res);
		return NULL;
	}
	return res;
}

static int __init request_standard_resources(void)
{
	int i;
	enum { CODE_DELTA = MEM_SV_INTRPT - PAGE_OFFSET };

	iomem_resource.end = -1LL;
#ifdef CONFIG_PCI
	insert_non_bus_resource();
#endif

	for_each_online_node(i) {
		u64 start_pfn = node_start_pfn[i];
		u64 end_pfn = node_end_pfn[i];

#ifdef CONFIG_PCI
		if (start_pfn <= pci_reserve_start_pfn &&
		    end_pfn > pci_reserve_start_pfn) {
			if (end_pfn > pci_reserve_end_pfn)
				insert_ram_resource(pci_reserve_end_pfn,
						     end_pfn);
			end_pfn = pci_reserve_start_pfn;
		}
#endif
		insert_ram_resource(start_pfn, end_pfn);
	}

	code_resource.start = __pa(_text - CODE_DELTA);
	code_resource.end = __pa(_etext - CODE_DELTA)-1;
	data_resource.start = __pa(_sdata);
	data_resource.end = __pa(_end)-1;

	insert_resource(&iomem_resource, &code_resource);
	insert_resource(&iomem_resource, &data_resource);

#ifdef CONFIG_KEXEC
	insert_resource(&iomem_resource, &crashk_res);
#endif

	return 0;
}

subsys_initcall(request_standard_resources);
