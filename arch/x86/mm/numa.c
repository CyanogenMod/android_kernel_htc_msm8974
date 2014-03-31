#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/nodemask.h>
#include <linux/sched.h>
#include <linux/topology.h>

#include <asm/e820.h>
#include <asm/proto.h>
#include <asm/dma.h>
#include <asm/acpi.h>
#include <asm/amd_nb.h>

#include "numa_internal.h"

int __initdata numa_off;
nodemask_t numa_nodes_parsed __initdata;

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

static struct numa_meminfo numa_meminfo
#ifndef CONFIG_MEMORY_HOTPLUG
__initdata
#endif
;

static int numa_distance_cnt;
static u8 *numa_distance;

static __init int numa_setup(char *opt)
{
	if (!opt)
		return -EINVAL;
	if (!strncmp(opt, "off", 3))
		numa_off = 1;
#ifdef CONFIG_NUMA_EMU
	if (!strncmp(opt, "fake=", 5))
		numa_emu_cmdline(opt + 5);
#endif
#ifdef CONFIG_ACPI_NUMA
	if (!strncmp(opt, "noacpi", 6))
		acpi_numa = -1;
#endif
	return 0;
}
early_param("numa", numa_setup);

s16 __apicid_to_node[MAX_LOCAL_APIC] __cpuinitdata = {
	[0 ... MAX_LOCAL_APIC-1] = NUMA_NO_NODE
};

int __cpuinit numa_cpu_node(int cpu)
{
	int apicid = early_per_cpu(x86_cpu_to_apicid, cpu);

	if (apicid != BAD_APICID)
		return __apicid_to_node[apicid];
	return NUMA_NO_NODE;
}

cpumask_var_t node_to_cpumask_map[MAX_NUMNODES];
EXPORT_SYMBOL(node_to_cpumask_map);

DEFINE_EARLY_PER_CPU(int, x86_cpu_to_node_map, NUMA_NO_NODE);
EXPORT_EARLY_PER_CPU_SYMBOL(x86_cpu_to_node_map);

void __cpuinit numa_set_node(int cpu, int node)
{
	int *cpu_to_node_map = early_per_cpu_ptr(x86_cpu_to_node_map);

	
	if (cpu_to_node_map) {
		cpu_to_node_map[cpu] = node;
		return;
	}

#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	if (cpu >= nr_cpu_ids || !cpu_possible(cpu)) {
		printk(KERN_ERR "numa_set_node: invalid cpu# (%d)\n", cpu);
		dump_stack();
		return;
	}
#endif
	per_cpu(x86_cpu_to_node_map, cpu) = node;

	if (node != NUMA_NO_NODE)
		set_cpu_numa_node(cpu, node);
}

void __cpuinit numa_clear_node(int cpu)
{
	numa_set_node(cpu, NUMA_NO_NODE);
}

void __init setup_node_to_cpumask_map(void)
{
	unsigned int node, num = 0;

	
	if (nr_node_ids == MAX_NUMNODES) {
		for_each_node_mask(node, node_possible_map)
			num = node;
		nr_node_ids = num + 1;
	}

	
	for (node = 0; node < nr_node_ids; node++)
		alloc_bootmem_cpumask_var(&node_to_cpumask_map[node]);

	
	pr_debug("Node to cpumask map for %d nodes\n", nr_node_ids);
}

static int __init numa_add_memblk_to(int nid, u64 start, u64 end,
				     struct numa_meminfo *mi)
{
	
	if (start == end)
		return 0;

	
	if (start > end || nid < 0 || nid >= MAX_NUMNODES) {
		pr_warning("NUMA: Warning: invalid memblk node %d (%Lx-%Lx)\n",
			   nid, start, end);
		return 0;
	}

	if (mi->nr_blks >= NR_NODE_MEMBLKS) {
		pr_err("NUMA: too many memblk ranges\n");
		return -EINVAL;
	}

	mi->blk[mi->nr_blks].start = start;
	mi->blk[mi->nr_blks].end = end;
	mi->blk[mi->nr_blks].nid = nid;
	mi->nr_blks++;
	return 0;
}

void __init numa_remove_memblk_from(int idx, struct numa_meminfo *mi)
{
	mi->nr_blks--;
	memmove(&mi->blk[idx], &mi->blk[idx + 1],
		(mi->nr_blks - idx) * sizeof(mi->blk[0]));
}

int __init numa_add_memblk(int nid, u64 start, u64 end)
{
	return numa_add_memblk_to(nid, start, end, &numa_meminfo);
}

static void __init setup_node_data(int nid, u64 start, u64 end)
{
	const size_t nd_size = roundup(sizeof(pg_data_t), PAGE_SIZE);
	bool remapped = false;
	u64 nd_pa;
	void *nd;
	int tnid;

	if (end && (end - start) < NODE_MIN_SIZE)
		return;

	
	init_alloc_remap(nid, start, end);

	start = roundup(start, ZONE_ALIGN);

	printk(KERN_INFO "Initmem setup node %d %016Lx-%016Lx\n",
	       nid, start, end);

	nd = alloc_remap(nid, nd_size);
	if (nd) {
		nd_pa = __pa(nd);
		remapped = true;
	} else {
		nd_pa = memblock_alloc_nid(nd_size, SMP_CACHE_BYTES, nid);
		if (!nd_pa) {
			pr_err("Cannot find %zu bytes in node %d\n",
			       nd_size, nid);
			return;
		}
		nd = __va(nd_pa);
	}

	
	printk(KERN_INFO "  NODE_DATA [%016Lx - %016Lx]%s\n",
	       nd_pa, nd_pa + nd_size - 1, remapped ? " (remapped)" : "");
	tnid = early_pfn_to_nid(nd_pa >> PAGE_SHIFT);
	if (!remapped && tnid != nid)
		printk(KERN_INFO "    NODE_DATA(%d) on node %d\n", nid, tnid);

	node_data[nid] = nd;
	memset(NODE_DATA(nid), 0, sizeof(pg_data_t));
	NODE_DATA(nid)->node_id = nid;
	NODE_DATA(nid)->node_start_pfn = start >> PAGE_SHIFT;
	NODE_DATA(nid)->node_spanned_pages = (end - start) >> PAGE_SHIFT;

	node_set_online(nid);
}

int __init numa_cleanup_meminfo(struct numa_meminfo *mi)
{
	const u64 low = 0;
	const u64 high = PFN_PHYS(max_pfn);
	int i, j, k;

	
	for (i = 0; i < mi->nr_blks; i++) {
		struct numa_memblk *bi = &mi->blk[i];

		
		bi->start = max(bi->start, low);
		bi->end = min(bi->end, high);

		
		if (bi->start >= bi->end)
			numa_remove_memblk_from(i--, mi);
	}

	
	for (i = 0; i < mi->nr_blks; i++) {
		struct numa_memblk *bi = &mi->blk[i];

		for (j = i + 1; j < mi->nr_blks; j++) {
			struct numa_memblk *bj = &mi->blk[j];
			u64 start, end;

			if (bi->end > bj->start && bi->start < bj->end) {
				if (bi->nid != bj->nid) {
					pr_err("NUMA: node %d (%Lx-%Lx) overlaps with node %d (%Lx-%Lx)\n",
					       bi->nid, bi->start, bi->end,
					       bj->nid, bj->start, bj->end);
					return -EINVAL;
				}
				pr_warning("NUMA: Warning: node %d (%Lx-%Lx) overlaps with itself (%Lx-%Lx)\n",
					   bi->nid, bi->start, bi->end,
					   bj->start, bj->end);
			}

			if (bi->nid != bj->nid)
				continue;
			start = min(bi->start, bj->start);
			end = max(bi->end, bj->end);
			for (k = 0; k < mi->nr_blks; k++) {
				struct numa_memblk *bk = &mi->blk[k];

				if (bi->nid == bk->nid)
					continue;
				if (start < bk->end && end > bk->start)
					break;
			}
			if (k < mi->nr_blks)
				continue;
			printk(KERN_INFO "NUMA: Node %d [%Lx,%Lx) + [%Lx,%Lx) -> [%Lx,%Lx)\n",
			       bi->nid, bi->start, bi->end, bj->start, bj->end,
			       start, end);
			bi->start = start;
			bi->end = end;
			numa_remove_memblk_from(j--, mi);
		}
	}

	
	for (i = mi->nr_blks; i < ARRAY_SIZE(mi->blk); i++) {
		mi->blk[i].start = mi->blk[i].end = 0;
		mi->blk[i].nid = NUMA_NO_NODE;
	}

	return 0;
}

static void __init numa_nodemask_from_meminfo(nodemask_t *nodemask,
					      const struct numa_meminfo *mi)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mi->blk); i++)
		if (mi->blk[i].start != mi->blk[i].end &&
		    mi->blk[i].nid != NUMA_NO_NODE)
			node_set(mi->blk[i].nid, *nodemask);
}

void __init numa_reset_distance(void)
{
	size_t size = numa_distance_cnt * numa_distance_cnt * sizeof(numa_distance[0]);

	
	if (numa_distance_cnt)
		memblock_free(__pa(numa_distance), size);
	numa_distance_cnt = 0;
	numa_distance = NULL;	
}

static int __init numa_alloc_distance(void)
{
	nodemask_t nodes_parsed;
	size_t size;
	int i, j, cnt = 0;
	u64 phys;

	
	nodes_parsed = numa_nodes_parsed;
	numa_nodemask_from_meminfo(&nodes_parsed, &numa_meminfo);

	for_each_node_mask(i, nodes_parsed)
		cnt = i;
	cnt++;
	size = cnt * cnt * sizeof(numa_distance[0]);

	phys = memblock_find_in_range(0, PFN_PHYS(max_pfn_mapped),
				      size, PAGE_SIZE);
	if (!phys) {
		pr_warning("NUMA: Warning: can't allocate distance table!\n");
		
		numa_distance = (void *)1LU;
		return -ENOMEM;
	}
	memblock_reserve(phys, size);

	numa_distance = __va(phys);
	numa_distance_cnt = cnt;

	
	for (i = 0; i < cnt; i++)
		for (j = 0; j < cnt; j++)
			numa_distance[i * cnt + j] = i == j ?
				LOCAL_DISTANCE : REMOTE_DISTANCE;
	printk(KERN_DEBUG "NUMA: Initialized distance table, cnt=%d\n", cnt);

	return 0;
}

void __init numa_set_distance(int from, int to, int distance)
{
	if (!numa_distance && numa_alloc_distance() < 0)
		return;

	if (from >= numa_distance_cnt || to >= numa_distance_cnt ||
			from < 0 || to < 0) {
		pr_warn_once("NUMA: Warning: node ids are out of bound, from=%d to=%d distance=%d\n",
			    from, to, distance);
		return;
	}

	if ((u8)distance != distance ||
	    (from == to && distance != LOCAL_DISTANCE)) {
		pr_warn_once("NUMA: Warning: invalid distance parameter, from=%d to=%d distance=%d\n",
			     from, to, distance);
		return;
	}

	numa_distance[from * numa_distance_cnt + to] = distance;
}

int __node_distance(int from, int to)
{
	if (from >= numa_distance_cnt || to >= numa_distance_cnt)
		return from == to ? LOCAL_DISTANCE : REMOTE_DISTANCE;
	return numa_distance[from * numa_distance_cnt + to];
}
EXPORT_SYMBOL(__node_distance);

static bool __init numa_meminfo_cover_memory(const struct numa_meminfo *mi)
{
	u64 numaram, e820ram;
	int i;

	numaram = 0;
	for (i = 0; i < mi->nr_blks; i++) {
		u64 s = mi->blk[i].start >> PAGE_SHIFT;
		u64 e = mi->blk[i].end >> PAGE_SHIFT;
		numaram += e - s;
		numaram -= __absent_pages_in_range(mi->blk[i].nid, s, e);
		if ((s64)numaram < 0)
			numaram = 0;
	}

	e820ram = max_pfn - absent_pages_in_range(0, max_pfn);

	
	if ((s64)(e820ram - numaram) >= (1 << (20 - PAGE_SHIFT))) {
		printk(KERN_ERR "NUMA: nodes only cover %LuMB of your %LuMB e820 RAM. Not used.\n",
		       (numaram << PAGE_SHIFT) >> 20,
		       (e820ram << PAGE_SHIFT) >> 20);
		return false;
	}
	return true;
}

static int __init numa_register_memblks(struct numa_meminfo *mi)
{
	unsigned long uninitialized_var(pfn_align);
	int i, nid;

	
	node_possible_map = numa_nodes_parsed;
	numa_nodemask_from_meminfo(&node_possible_map, mi);
	if (WARN_ON(nodes_empty(node_possible_map)))
		return -EINVAL;

	for (i = 0; i < mi->nr_blks; i++) {
		struct numa_memblk *mb = &mi->blk[i];
		memblock_set_node(mb->start, mb->end - mb->start, mb->nid);
	}

#ifdef NODE_NOT_IN_PAGE_FLAGS
	pfn_align = node_map_pfn_alignment();
	if (pfn_align && pfn_align < PAGES_PER_SECTION) {
		printk(KERN_WARNING "Node alignment %LuMB < min %LuMB, rejecting NUMA config\n",
		       PFN_PHYS(pfn_align) >> 20,
		       PFN_PHYS(PAGES_PER_SECTION) >> 20);
		return -EINVAL;
	}
#endif
	if (!numa_meminfo_cover_memory(mi))
		return -EINVAL;

	
	for_each_node_mask(nid, node_possible_map) {
		u64 start = PFN_PHYS(max_pfn);
		u64 end = 0;

		for (i = 0; i < mi->nr_blks; i++) {
			if (nid != mi->blk[i].nid)
				continue;
			start = min(mi->blk[i].start, start);
			end = max(mi->blk[i].end, end);
		}

		if (start < end)
			setup_node_data(nid, start, end);
	}

	
	memblock_dump_all();
	return 0;
}

static void __init numa_init_array(void)
{
	int rr, i;

	rr = first_node(node_online_map);
	for (i = 0; i < nr_cpu_ids; i++) {
		if (early_cpu_to_node(i) != NUMA_NO_NODE)
			continue;
		numa_set_node(i, rr);
		rr = next_node(rr, node_online_map);
		if (rr == MAX_NUMNODES)
			rr = first_node(node_online_map);
	}
}

static int __init numa_init(int (*init_func)(void))
{
	int i;
	int ret;

	for (i = 0; i < MAX_LOCAL_APIC; i++)
		set_apicid_to_node(i, NUMA_NO_NODE);

	nodes_clear(numa_nodes_parsed);
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
	memset(&numa_meminfo, 0, sizeof(numa_meminfo));
	WARN_ON(memblock_set_node(0, ULLONG_MAX, MAX_NUMNODES));
	numa_reset_distance();

	ret = init_func();
	if (ret < 0)
		return ret;
	ret = numa_cleanup_meminfo(&numa_meminfo);
	if (ret < 0)
		return ret;

	numa_emulation(&numa_meminfo, numa_distance_cnt);

	ret = numa_register_memblks(&numa_meminfo);
	if (ret < 0)
		return ret;

	for (i = 0; i < nr_cpu_ids; i++) {
		int nid = early_cpu_to_node(i);

		if (nid == NUMA_NO_NODE)
			continue;
		if (!node_online(nid))
			numa_clear_node(i);
	}
	numa_init_array();
	return 0;
}

static int __init dummy_numa_init(void)
{
	printk(KERN_INFO "%s\n",
	       numa_off ? "NUMA turned off" : "No NUMA configuration found");
	printk(KERN_INFO "Faking a node at %016Lx-%016Lx\n",
	       0LLU, PFN_PHYS(max_pfn));

	node_set(0, numa_nodes_parsed);
	numa_add_memblk(0, 0, PFN_PHYS(max_pfn));

	return 0;
}

void __init x86_numa_init(void)
{
	if (!numa_off) {
#ifdef CONFIG_X86_NUMAQ
		if (!numa_init(numaq_numa_init))
			return;
#endif
#ifdef CONFIG_ACPI_NUMA
		if (!numa_init(x86_acpi_numa_init))
			return;
#endif
#ifdef CONFIG_AMD_NUMA
		if (!numa_init(amd_numa_init))
			return;
#endif
	}

	numa_init(dummy_numa_init);
}

static __init int find_near_online_node(int node)
{
	int n, val;
	int min_val = INT_MAX;
	int best_node = -1;

	for_each_online_node(n) {
		val = node_distance(node, n);

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	return best_node;
}

void __init init_cpu_to_node(void)
{
	int cpu;
	u16 *cpu_to_apicid = early_per_cpu_ptr(x86_cpu_to_apicid);

	BUG_ON(cpu_to_apicid == NULL);

	for_each_possible_cpu(cpu) {
		int node = numa_cpu_node(cpu);

		if (node == NUMA_NO_NODE)
			continue;
		if (!node_online(node))
			node = find_near_online_node(node);
		numa_set_node(cpu, node);
	}
}

#ifndef CONFIG_DEBUG_PER_CPU_MAPS

# ifndef CONFIG_NUMA_EMU
void __cpuinit numa_add_cpu(int cpu)
{
	cpumask_set_cpu(cpu, node_to_cpumask_map[early_cpu_to_node(cpu)]);
}

void __cpuinit numa_remove_cpu(int cpu)
{
	cpumask_clear_cpu(cpu, node_to_cpumask_map[early_cpu_to_node(cpu)]);
}
# endif	

#else	

int __cpu_to_node(int cpu)
{
	if (early_per_cpu_ptr(x86_cpu_to_node_map)) {
		printk(KERN_WARNING
			"cpu_to_node(%d): usage too early!\n", cpu);
		dump_stack();
		return early_per_cpu_ptr(x86_cpu_to_node_map)[cpu];
	}
	return per_cpu(x86_cpu_to_node_map, cpu);
}
EXPORT_SYMBOL(__cpu_to_node);

int early_cpu_to_node(int cpu)
{
	if (early_per_cpu_ptr(x86_cpu_to_node_map))
		return early_per_cpu_ptr(x86_cpu_to_node_map)[cpu];

	if (!cpu_possible(cpu)) {
		printk(KERN_WARNING
			"early_cpu_to_node(%d): no per_cpu area!\n", cpu);
		dump_stack();
		return NUMA_NO_NODE;
	}
	return per_cpu(x86_cpu_to_node_map, cpu);
}

void debug_cpumask_set_cpu(int cpu, int node, bool enable)
{
	struct cpumask *mask;
	char buf[64];

	if (node == NUMA_NO_NODE) {
		
		return;
	}
	mask = node_to_cpumask_map[node];
	if (!mask) {
		pr_err("node_to_cpumask_map[%i] NULL\n", node);
		dump_stack();
		return;
	}

	if (enable)
		cpumask_set_cpu(cpu, mask);
	else
		cpumask_clear_cpu(cpu, mask);

	cpulist_scnprintf(buf, sizeof(buf), mask);
	printk(KERN_DEBUG "%s cpu %d node %d: mask now %s\n",
		enable ? "numa_add_cpu" : "numa_remove_cpu",
		cpu, node, buf);
	return;
}

# ifndef CONFIG_NUMA_EMU
static void __cpuinit numa_set_cpumask(int cpu, bool enable)
{
	debug_cpumask_set_cpu(cpu, early_cpu_to_node(cpu), enable);
}

void __cpuinit numa_add_cpu(int cpu)
{
	numa_set_cpumask(cpu, true);
}

void __cpuinit numa_remove_cpu(int cpu)
{
	numa_set_cpumask(cpu, false);
}
# endif	

const struct cpumask *cpumask_of_node(int node)
{
	if (node >= nr_node_ids) {
		printk(KERN_WARNING
			"cpumask_of_node(%d): node > nr_node_ids(%d)\n",
			node, nr_node_ids);
		dump_stack();
		return cpu_none_mask;
	}
	if (node_to_cpumask_map[node] == NULL) {
		printk(KERN_WARNING
			"cpumask_of_node(%d): no node_to_cpumask_map!\n",
			node);
		dump_stack();
		return cpu_online_mask;
	}
	return node_to_cpumask_map[node];
}
EXPORT_SYMBOL(cpumask_of_node);

#endif	

#ifdef CONFIG_MEMORY_HOTPLUG
int memory_add_physaddr_to_nid(u64 start)
{
	struct numa_meminfo *mi = &numa_meminfo;
	int nid = mi->blk[0].nid;
	int i;

	for (i = 0; i < mi->nr_blks; i++)
		if (mi->blk[i].start <= start && mi->blk[i].end > start)
			nid = mi->blk[i].nid;
	return nid;
}
EXPORT_SYMBOL_GPL(memory_add_physaddr_to_nid);
#endif
