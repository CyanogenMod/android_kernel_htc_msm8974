/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2007 Cavium Networks
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/bitops.h>
#include <linux/cpu.h>
#include <linux/io.h>

#include <asm/bcache.h>
#include <asm/bootinfo.h>
#include <asm/cacheops.h>
#include <asm/cpu-features.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/r4kcache.h>
#include <asm/mmu_context.h>
#include <asm/war.h>

#include <asm/octeon/octeon.h>

unsigned long long cache_err_dcache[NR_CPUS];

static void octeon_flush_data_cache_page(unsigned long addr)
{
    
}

static inline void octeon_local_flush_icache(void)
{
	asm volatile ("synci 0($0)");
}

static void local_octeon_flush_icache_range(unsigned long start,
					    unsigned long end)
{
	octeon_local_flush_icache();
}

static void octeon_flush_icache_all_cores(struct vm_area_struct *vma)
{
	extern void octeon_send_ipi_single(int cpu, unsigned int action);
#ifdef CONFIG_SMP
	int cpu;
	cpumask_t mask;
#endif

	mb();
	octeon_local_flush_icache();
#ifdef CONFIG_SMP
	preempt_disable();
	cpu = smp_processor_id();

	if (vma)
		mask = *mm_cpumask(vma->vm_mm);
	else
		mask = *cpu_online_mask;
	cpumask_clear_cpu(cpu, &mask);
	for_each_cpu(cpu, &mask)
		octeon_send_ipi_single(cpu, SMP_ICACHE_FLUSH);

	preempt_enable();
#endif
}


static void octeon_flush_icache_all(void)
{
	octeon_flush_icache_all_cores(NULL);
}


static void octeon_flush_cache_mm(struct mm_struct *mm)
{
}


static void octeon_flush_icache_range(unsigned long start, unsigned long end)
{
	octeon_flush_icache_all_cores(NULL);
}


static void octeon_flush_cache_sigtramp(unsigned long addr)
{
	struct vm_area_struct *vma;

	vma = find_vma(current->mm, addr);
	octeon_flush_icache_all_cores(vma);
}


static void octeon_flush_cache_range(struct vm_area_struct *vma,
				     unsigned long start, unsigned long end)
{
	if (vma->vm_flags & VM_EXEC)
		octeon_flush_icache_all_cores(vma);
}


static void octeon_flush_cache_page(struct vm_area_struct *vma,
				    unsigned long page, unsigned long pfn)
{
	if (vma->vm_flags & VM_EXEC)
		octeon_flush_icache_all_cores(vma);
}

static void octeon_flush_kernel_vmap_range(unsigned long vaddr, int size)
{
	BUG();
}

static void __cpuinit probe_octeon(void)
{
	unsigned long icache_size;
	unsigned long dcache_size;
	unsigned int config1;
	struct cpuinfo_mips *c = &current_cpu_data;

	config1 = read_c0_config1();
	switch (c->cputype) {
	case CPU_CAVIUM_OCTEON:
	case CPU_CAVIUM_OCTEON_PLUS:
		c->icache.linesz = 2 << ((config1 >> 19) & 7);
		c->icache.sets = 64 << ((config1 >> 22) & 7);
		c->icache.ways = 1 + ((config1 >> 16) & 7);
		c->icache.flags |= MIPS_CACHE_VTAG;
		icache_size =
			c->icache.sets * c->icache.ways * c->icache.linesz;
		c->icache.waybit = ffs(icache_size / c->icache.ways) - 1;
		c->dcache.linesz = 128;
		if (c->cputype == CPU_CAVIUM_OCTEON_PLUS)
			c->dcache.sets = 2; 
		else
			c->dcache.sets = 1; 
		c->dcache.ways = 64;
		dcache_size =
			c->dcache.sets * c->dcache.ways * c->dcache.linesz;
		c->dcache.waybit = ffs(dcache_size / c->dcache.ways) - 1;
		c->options |= MIPS_CPU_PREFETCH;
		break;

	case CPU_CAVIUM_OCTEON2:
		c->icache.linesz = 2 << ((config1 >> 19) & 7);
		c->icache.sets = 8;
		c->icache.ways = 37;
		c->icache.flags |= MIPS_CACHE_VTAG;
		icache_size = c->icache.sets * c->icache.ways * c->icache.linesz;

		c->dcache.linesz = 128;
		c->dcache.ways = 32;
		c->dcache.sets = 8;
		dcache_size = c->dcache.sets * c->dcache.ways * c->dcache.linesz;
		c->options |= MIPS_CPU_PREFETCH;
		break;

	default:
		panic("Unsupported Cavium Networks CPU type");
		break;
	}

	
	c->icache.waysize = icache_size / c->icache.ways;
	c->dcache.waysize = dcache_size / c->dcache.ways;

	c->icache.sets = icache_size / (c->icache.linesz * c->icache.ways);
	c->dcache.sets = dcache_size / (c->dcache.linesz * c->dcache.ways);

	if (smp_processor_id() == 0) {
		pr_notice("Primary instruction cache %ldkB, %s, %d way, "
			  "%d sets, linesize %d bytes.\n",
			  icache_size >> 10,
			  cpu_has_vtag_icache ?
				"virtually tagged" : "physically tagged",
			  c->icache.ways, c->icache.sets, c->icache.linesz);

		pr_notice("Primary data cache %ldkB, %d-way, %d sets, "
			  "linesize %d bytes.\n",
			  dcache_size >> 10, c->dcache.ways,
			  c->dcache.sets, c->dcache.linesz);
	}
}


void __cpuinit octeon_cache_init(void)
{
	extern unsigned long ebase;
	extern char except_vec2_octeon;

	memcpy((void *)(ebase + 0x100), &except_vec2_octeon, 0x80);
	octeon_flush_cache_sigtramp(ebase + 0x100);

	probe_octeon();

	shm_align_mask = PAGE_SIZE - 1;

	flush_cache_all			= octeon_flush_icache_all;
	__flush_cache_all		= octeon_flush_icache_all;
	flush_cache_mm			= octeon_flush_cache_mm;
	flush_cache_page		= octeon_flush_cache_page;
	flush_cache_range		= octeon_flush_cache_range;
	flush_cache_sigtramp		= octeon_flush_cache_sigtramp;
	flush_icache_all		= octeon_flush_icache_all;
	flush_data_cache_page		= octeon_flush_data_cache_page;
	flush_icache_range		= octeon_flush_icache_range;
	local_flush_icache_range	= local_octeon_flush_icache_range;

	__flush_kernel_vmap_range	= octeon_flush_kernel_vmap_range;

	build_clear_page();
	build_copy_page();
}


static void  cache_parity_error_octeon(int non_recoverable)
{
	unsigned long coreid = cvmx_get_core_num();
	uint64_t icache_err = read_octeon_c0_icacheerr();

	pr_err("Cache error exception:\n");
	pr_err("cp0_errorepc == %lx\n", read_c0_errorepc());
	if (icache_err & 1) {
		pr_err("CacheErr (Icache) == %llx\n",
		       (unsigned long long)icache_err);
		write_octeon_c0_icacheerr(0);
	}
	if (cache_err_dcache[coreid] & 1) {
		pr_err("CacheErr (Dcache) == %llx\n",
		       (unsigned long long)cache_err_dcache[coreid]);
		cache_err_dcache[coreid] = 0;
	}

	if (non_recoverable)
		panic("Can't handle cache error: nested exception");
}


asmlinkage void cache_parity_error_octeon_recoverable(void)
{
	cache_parity_error_octeon(0);
}


asmlinkage void cache_parity_error_octeon_non_recoverable(void)
{
	cache_parity_error_octeon(1);
}
