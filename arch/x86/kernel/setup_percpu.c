#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/percpu.h>
#include <linux/kexec.h>
#include <linux/crash_dump.h>
#include <linux/smp.h>
#include <linux/topology.h>
#include <linux/pfn.h>
#include <asm/sections.h>
#include <asm/processor.h>
#include <asm/setup.h>
#include <asm/mpspec.h>
#include <asm/apicdef.h>
#include <asm/highmem.h>
#include <asm/proto.h>
#include <asm/cpumask.h>
#include <asm/cpu.h>
#include <asm/stackprotector.h>

DEFINE_PER_CPU(int, cpu_number);
EXPORT_PER_CPU_SYMBOL(cpu_number);

#ifdef CONFIG_X86_64
#define BOOT_PERCPU_OFFSET ((unsigned long)__per_cpu_load)
#else
#define BOOT_PERCPU_OFFSET 0
#endif

DEFINE_PER_CPU(unsigned long, this_cpu_off) = BOOT_PERCPU_OFFSET;
EXPORT_PER_CPU_SYMBOL(this_cpu_off);

unsigned long __per_cpu_offset[NR_CPUS] __read_mostly = {
	[0 ... NR_CPUS-1] = BOOT_PERCPU_OFFSET,
};
EXPORT_SYMBOL(__per_cpu_offset);

#ifdef CONFIG_X86_64
#define PERCPU_FIRST_CHUNK_RESERVE	PERCPU_MODULE_RESERVE
#else
#define PERCPU_FIRST_CHUNK_RESERVE	0
#endif

#ifdef CONFIG_X86_32
static bool __init pcpu_need_numa(void)
{
#ifdef CONFIG_NEED_MULTIPLE_NODES
	pg_data_t *last = NULL;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		int node = early_cpu_to_node(cpu);

		if (node_online(node) && NODE_DATA(node) &&
		    last && last != NODE_DATA(node))
			return true;

		last = NODE_DATA(node);
	}
#endif
	return false;
}
#endif

static void * __init pcpu_alloc_bootmem(unsigned int cpu, unsigned long size,
					unsigned long align)
{
	const unsigned long goal = __pa(MAX_DMA_ADDRESS);
#ifdef CONFIG_NEED_MULTIPLE_NODES
	int node = early_cpu_to_node(cpu);
	void *ptr;

	if (!node_online(node) || !NODE_DATA(node)) {
		ptr = __alloc_bootmem_nopanic(size, align, goal);
		pr_info("cpu %d has no node %d or node-local memory\n",
			cpu, node);
		pr_debug("per cpu data for cpu%d %lu bytes at %016lx\n",
			 cpu, size, __pa(ptr));
	} else {
		ptr = __alloc_bootmem_node_nopanic(NODE_DATA(node),
						   size, align, goal);
		pr_debug("per cpu data for cpu%d %lu bytes on node%d at %016lx\n",
			 cpu, size, node, __pa(ptr));
	}
	return ptr;
#else
	return __alloc_bootmem_nopanic(size, align, goal);
#endif
}

static void * __init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	return pcpu_alloc_bootmem(cpu, size, align);
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	free_bootmem(__pa(ptr), size);
}

static int __init pcpu_cpu_distance(unsigned int from, unsigned int to)
{
#ifdef CONFIG_NEED_MULTIPLE_NODES
	if (early_cpu_to_node(from) == early_cpu_to_node(to))
		return LOCAL_DISTANCE;
	else
		return REMOTE_DISTANCE;
#else
	return LOCAL_DISTANCE;
#endif
}

static void __init pcpup_populate_pte(unsigned long addr)
{
	populate_extra_pte(addr);
}

static inline void setup_percpu_segment(int cpu)
{
#ifdef CONFIG_X86_32
	struct desc_struct gdt;

	pack_descriptor(&gdt, per_cpu_offset(cpu), 0xFFFFF,
			0x2 | DESCTYPE_S, 0x8);
	gdt.s = 1;
	write_gdt_entry(get_cpu_gdt_table(cpu),
			GDT_ENTRY_PERCPU, &gdt, DESCTYPE_S);
#endif
}

void __init setup_per_cpu_areas(void)
{
	unsigned int cpu;
	unsigned long delta;
	int rc;

	pr_info("NR_CPUS:%d nr_cpumask_bits:%d nr_cpu_ids:%d nr_node_ids:%d\n",
		NR_CPUS, nr_cpumask_bits, nr_cpu_ids, nr_node_ids);

#ifdef CONFIG_X86_32
	if (pcpu_chosen_fc == PCPU_FC_AUTO && pcpu_need_numa())
		pcpu_chosen_fc = PCPU_FC_PAGE;
#endif
	rc = -EINVAL;
	if (pcpu_chosen_fc != PCPU_FC_PAGE) {
		const size_t dyn_size = PERCPU_MODULE_RESERVE +
			PERCPU_DYNAMIC_RESERVE - PERCPU_FIRST_CHUNK_RESERVE;
		size_t atom_size;

#ifdef CONFIG_X86_64
		atom_size = PMD_SIZE;
#else
		atom_size = PAGE_SIZE;
#endif
		rc = pcpu_embed_first_chunk(PERCPU_FIRST_CHUNK_RESERVE,
					    dyn_size, atom_size,
					    pcpu_cpu_distance,
					    pcpu_fc_alloc, pcpu_fc_free);
		if (rc < 0)
			pr_warning("%s allocator failed (%d), falling back to page size\n",
				   pcpu_fc_names[pcpu_chosen_fc], rc);
	}
	if (rc < 0)
		rc = pcpu_page_first_chunk(PERCPU_FIRST_CHUNK_RESERVE,
					   pcpu_fc_alloc, pcpu_fc_free,
					   pcpup_populate_pte);
	if (rc < 0)
		panic("cannot initialize percpu area (err=%d)", rc);

	
	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu) {
		per_cpu_offset(cpu) = delta + pcpu_unit_offsets[cpu];
		per_cpu(this_cpu_off, cpu) = per_cpu_offset(cpu);
		per_cpu(cpu_number, cpu) = cpu;
		setup_percpu_segment(cpu);
		setup_stack_canary_segment(cpu);
#ifdef CONFIG_X86_LOCAL_APIC
		per_cpu(x86_cpu_to_apicid, cpu) =
			early_per_cpu_map(x86_cpu_to_apicid, cpu);
		per_cpu(x86_bios_cpu_apicid, cpu) =
			early_per_cpu_map(x86_bios_cpu_apicid, cpu);
#endif
#ifdef CONFIG_X86_32
		per_cpu(x86_cpu_to_logical_apicid, cpu) =
			early_per_cpu_map(x86_cpu_to_logical_apicid, cpu);
#endif
#ifdef CONFIG_X86_64
		per_cpu(irq_stack_ptr, cpu) =
			per_cpu(irq_stack_union.irq_stack, cpu) +
			IRQ_STACK_SIZE - 64;
#endif
#ifdef CONFIG_NUMA
		per_cpu(x86_cpu_to_node_map, cpu) =
			early_per_cpu_map(x86_cpu_to_node_map, cpu);
		set_cpu_numa_node(cpu, early_cpu_to_node(cpu));
#endif
		if (!cpu)
			switch_to_new_gdt(cpu);
	}

	
#ifdef CONFIG_X86_LOCAL_APIC
	early_per_cpu_ptr(x86_cpu_to_apicid) = NULL;
	early_per_cpu_ptr(x86_bios_cpu_apicid) = NULL;
#endif
#ifdef CONFIG_X86_32
	early_per_cpu_ptr(x86_cpu_to_logical_apicid) = NULL;
#endif
#ifdef CONFIG_NUMA
	early_per_cpu_ptr(x86_cpu_to_node_map) = NULL;
#endif

	
	setup_node_to_cpumask_map();

	
	setup_cpu_local_masks();
}
