/*
 * 
 * Common boot and setup code.
 *
 * Copyright (C) 2001 PPC64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#undef DEBUG

#include <linux/export.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/initrd.h>
#include <linux/seq_file.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/utsname.h>
#include <linux/tty.h>
#include <linux/root_dev.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/unistd.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/bootmem.h>
#include <linux/pci.h>
#include <linux/lockdep.h>
#include <linux/memblock.h>
#include <linux/hugetlb.h>

#include <asm/io.h>
#include <asm/kdump.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/smp.h>
#include <asm/elf.h>
#include <asm/machdep.h>
#include <asm/paca.h>
#include <asm/time.h>
#include <asm/cputable.h>
#include <asm/sections.h>
#include <asm/btext.h>
#include <asm/nvram.h>
#include <asm/setup.h>
#include <asm/rtas.h>
#include <asm/iommu.h>
#include <asm/serial.h>
#include <asm/cache.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/firmware.h>
#include <asm/xmon.h>
#include <asm/udbg.h>
#include <asm/kexec.h>
#include <asm/mmu_context.h>
#include <asm/code-patching.h>
#include <asm/kvm_ppc.h>
#include <asm/hugetlb.h>

#include "setup.h"

#ifdef DEBUG
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

int boot_cpuid = 0;
int __initdata spinning_secondaries;
u64 ppc64_pft_size;

struct ppc64_caches ppc64_caches = {
	.dline_size = 0x40,
	.log_dline_size = 6,
	.iline_size = 0x40,
	.log_iline_size = 6
};
EXPORT_SYMBOL_GPL(ppc64_caches);

int dcache_bsize;
int icache_bsize;
int ucache_bsize;

#ifdef CONFIG_SMP

static char *smt_enabled_cmdline;

static void check_smt_enabled(void)
{
	struct device_node *dn;
	const char *smt_option;

	
	smt_enabled_at_boot = threads_per_core;

	
	if (smt_enabled_cmdline) {
		if (!strcmp(smt_enabled_cmdline, "on"))
			smt_enabled_at_boot = threads_per_core;
		else if (!strcmp(smt_enabled_cmdline, "off"))
			smt_enabled_at_boot = 0;
		else {
			long smt;
			int rc;

			rc = strict_strtol(smt_enabled_cmdline, 10, &smt);
			if (!rc)
				smt_enabled_at_boot =
					min(threads_per_core, (int)smt);
		}
	} else {
		dn = of_find_node_by_path("/options");
		if (dn) {
			smt_option = of_get_property(dn, "ibm,smt-enabled",
						     NULL);

			if (smt_option) {
				if (!strcmp(smt_option, "on"))
					smt_enabled_at_boot = threads_per_core;
				else if (!strcmp(smt_option, "off"))
					smt_enabled_at_boot = 0;
			}

			of_node_put(dn);
		}
	}
}

static int __init early_smt_enabled(char *p)
{
	smt_enabled_cmdline = p;
	return 0;
}
early_param("smt-enabled", early_smt_enabled);

#else
#define check_smt_enabled()
#endif 


void __init early_setup(unsigned long dt_ptr)
{
	

	
	identify_cpu(0, mfspr(SPRN_PVR));

	
	initialise_paca(&boot_paca, 0);
	setup_paca(&boot_paca);

	
	lockdep_init();

	

	
	udbg_early_init();

 	DBG(" -> early_setup(), dt_ptr: 0x%lx\n", dt_ptr);

	early_init_devtree(__va(dt_ptr));

	
	setup_paca(&paca[boot_cpuid]);

	
	get_paca()->cpu_start = 1;

	
	probe_machine();

	setup_kdump_trampoline();

	DBG("Found, Initializing memory management...\n");

	
	early_init_mmu();

	reserve_hugetlb_gpages();

	DBG(" <- early_setup()\n");
}

#ifdef CONFIG_SMP
void early_setup_secondary(void)
{
	
	get_paca()->soft_enabled = 0;

	
	early_init_mmu_secondary();
}

#endif 

#if defined(CONFIG_SMP) || defined(CONFIG_KEXEC)
void smp_release_cpus(void)
{
	unsigned long *ptr;
	int i;

	DBG(" -> smp_release_cpus()\n");


	ptr  = (unsigned long *)((unsigned long)&__secondary_hold_spinloop
			- PHYSICAL_START);
	*ptr = __pa(generic_secondary_smp_init);

	
	for (i = 0; i < 100000; i++) {
		mb();
		HMT_low();
		if (spinning_secondaries == 0)
			break;
		udelay(1);
	}
	DBG("spinning_secondaries = %d\n", spinning_secondaries);

	DBG(" <- smp_release_cpus()\n");
}
#endif 

static void __init initialize_cache_info(void)
{
	struct device_node *np;
	unsigned long num_cpus = 0;

	DBG(" -> initialize_cache_info()\n");

	for_each_node_by_type(np, "cpu") {
		num_cpus += 1;

		if (num_cpus == 1) {
			const u32 *sizep, *lsizep;
			u32 size, lsize;

			size = 0;
			lsize = cur_cpu_spec->dcache_bsize;
			sizep = of_get_property(np, "d-cache-size", NULL);
			if (sizep != NULL)
				size = *sizep;
			lsizep = of_get_property(np, "d-cache-block-size",
						 NULL);
			
			if (lsizep == NULL)
				lsizep = of_get_property(np,
							 "d-cache-line-size",
							 NULL);
			if (lsizep != NULL)
				lsize = *lsizep;
			if (sizep == 0 || lsizep == 0)
				DBG("Argh, can't find dcache properties ! "
				    "sizep: %p, lsizep: %p\n", sizep, lsizep);

			ppc64_caches.dsize = size;
			ppc64_caches.dline_size = lsize;
			ppc64_caches.log_dline_size = __ilog2(lsize);
			ppc64_caches.dlines_per_page = PAGE_SIZE / lsize;

			size = 0;
			lsize = cur_cpu_spec->icache_bsize;
			sizep = of_get_property(np, "i-cache-size", NULL);
			if (sizep != NULL)
				size = *sizep;
			lsizep = of_get_property(np, "i-cache-block-size",
						 NULL);
			if (lsizep == NULL)
				lsizep = of_get_property(np,
							 "i-cache-line-size",
							 NULL);
			if (lsizep != NULL)
				lsize = *lsizep;
			if (sizep == 0 || lsizep == 0)
				DBG("Argh, can't find icache properties ! "
				    "sizep: %p, lsizep: %p\n", sizep, lsizep);

			ppc64_caches.isize = size;
			ppc64_caches.iline_size = lsize;
			ppc64_caches.log_iline_size = __ilog2(lsize);
			ppc64_caches.ilines_per_page = PAGE_SIZE / lsize;
		}
	}

	DBG(" <- initialize_cache_info()\n");
}


void __init setup_system(void)
{
	DBG(" -> setup_system()\n");

	do_feature_fixups(cur_cpu_spec->cpu_features,
			  &__start___ftr_fixup, &__stop___ftr_fixup);
	do_feature_fixups(cur_cpu_spec->mmu_features,
			  &__start___mmu_ftr_fixup, &__stop___mmu_ftr_fixup);
	do_feature_fixups(powerpc_firmware_features,
			  &__start___fw_ftr_fixup, &__stop___fw_ftr_fixup);
	do_lwsync_fixups(cur_cpu_spec->cpu_features,
			 &__start___lwsync_fixup, &__stop___lwsync_fixup);
	do_final_fixups();

	unflatten_device_tree();

	initialize_cache_info();

#ifdef CONFIG_PPC_RTAS
	rtas_initialize();
#endif 

	check_for_initrd();

	if (ppc_md.init_early)
		ppc_md.init_early();

	find_legacy_serial_ports();

	register_early_udbg_console();

	xmon_setup();

	smp_setup_cpu_maps();
	check_smt_enabled();

#ifdef CONFIG_SMP
	smp_release_cpus();
#endif

	printk("Starting Linux PPC64 %s\n", init_utsname()->version);

	printk("-----------------------------------------------------\n");
	printk("ppc64_pft_size                = 0x%llx\n", ppc64_pft_size);
	printk("physicalMemorySize            = 0x%llx\n", memblock_phys_mem_size());
	if (ppc64_caches.dline_size != 0x80)
		printk("ppc64_caches.dcache_line_size = 0x%x\n",
		       ppc64_caches.dline_size);
	if (ppc64_caches.iline_size != 0x80)
		printk("ppc64_caches.icache_line_size = 0x%x\n",
		       ppc64_caches.iline_size);
#ifdef CONFIG_PPC_STD_MMU_64
	if (htab_address)
		printk("htab_address                  = 0x%p\n", htab_address);
	printk("htab_hash_mask                = 0x%lx\n", htab_hash_mask);
#endif 
	if (PHYSICAL_START > 0)
		printk("physical_start                = 0x%llx\n",
		       (unsigned long long)PHYSICAL_START);
	printk("-----------------------------------------------------\n");

	DBG(" <- setup_system()\n");
}

static u64 safe_stack_limit(void)
{
#ifdef CONFIG_PPC_BOOK3E
	
	if (mmu_has_feature(MMU_FTR_TYPE_FSL_E))
		return linear_map_top;
	
	return 1ul << 30;
#else
	
	if (mmu_has_feature(MMU_FTR_1T_SEGMENT))
		return 1UL << SID_SHIFT_1T;
	return 1UL << SID_SHIFT;
#endif
}

static void __init irqstack_early_init(void)
{
	u64 limit = safe_stack_limit();
	unsigned int i;

	for_each_possible_cpu(i) {
		softirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc_base(THREAD_SIZE,
					    THREAD_SIZE, limit));
		hardirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc_base(THREAD_SIZE,
					    THREAD_SIZE, limit));
	}
}

#ifdef CONFIG_PPC_BOOK3E
static void __init exc_lvl_early_init(void)
{
	extern unsigned int interrupt_base_book3e;
	extern unsigned int exc_debug_debug_book3e;

	unsigned int i;

	for_each_possible_cpu(i) {
		critirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
		dbgirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
		mcheckirq_ctx[i] = (struct thread_info *)
			__va(memblock_alloc(THREAD_SIZE, THREAD_SIZE));
	}

	if (cpu_has_feature(CPU_FTR_DEBUG_LVL_EXC))
		patch_branch(&interrupt_base_book3e + (0x040 / 4) + 1,
			     (unsigned long)&exc_debug_debug_book3e, 0);
}
#else
#define exc_lvl_early_init()
#endif

static void __init emergency_stack_init(void)
{
	u64 limit;
	unsigned int i;

	limit = min(safe_stack_limit(), ppc64_rma_size);

	for_each_possible_cpu(i) {
		unsigned long sp;
		sp  = memblock_alloc_base(THREAD_SIZE, THREAD_SIZE, limit);
		sp += THREAD_SIZE;
		paca[i].emergency_sp = __va(sp);
	}
}

void __init setup_arch(char **cmdline_p)
{
	ppc64_boot_msg(0x12, "Setup Arch");

	*cmdline_p = cmd_line;

	dcache_bsize = ppc64_caches.dline_size;
	icache_bsize = ppc64_caches.iline_size;

	
	panic_timeout = 180;

	if (ppc_md.panic)
		setup_panic();

	init_mm.start_code = (unsigned long)_stext;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = klimit;
	
	irqstack_early_init();
	exc_lvl_early_init();
	emergency_stack_init();

#ifdef CONFIG_PPC_STD_MMU_64
	stabs_alloc();
#endif
	
	do_init_bootmem();
	sparse_init();

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

	if (ppc_md.setup_arch)
		ppc_md.setup_arch();

	paging_init();

	
	mmu_context_init();

	kvm_linear_init();

	ppc64_boot_msg(0x15, "Setup Done");
}


#define PPC64_LINUX_FUNCTION 0x0f000000
#define PPC64_IPL_MESSAGE 0xc0000000
#define PPC64_TERM_MESSAGE 0xb0000000

static void ppc64_do_msg(unsigned int src, const char *msg)
{
	if (ppc_md.progress) {
		char buf[128];

		sprintf(buf, "%08X\n", src);
		ppc_md.progress(buf, 0);
		snprintf(buf, 128, "%s", msg);
		ppc_md.progress(buf, 0);
	}
}

void ppc64_boot_msg(unsigned int src, const char *msg)
{
	ppc64_do_msg(PPC64_LINUX_FUNCTION|PPC64_IPL_MESSAGE|src, msg);
	printk("[boot]%04x %s\n", src, msg);
}

#ifdef CONFIG_SMP
#define PCPU_DYN_SIZE		()

static void * __init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	return __alloc_bootmem_node(NODE_DATA(cpu_to_node(cpu)), size, align,
				    __pa(MAX_DMA_ADDRESS));
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	free_bootmem(__pa(ptr), size);
}

static int pcpu_cpu_distance(unsigned int from, unsigned int to)
{
	if (cpu_to_node(from) == cpu_to_node(to))
		return LOCAL_DISTANCE;
	else
		return REMOTE_DISTANCE;
}

unsigned long __per_cpu_offset[NR_CPUS] __read_mostly;
EXPORT_SYMBOL(__per_cpu_offset);

void __init setup_per_cpu_areas(void)
{
	const size_t dyn_size = PERCPU_MODULE_RESERVE + PERCPU_DYNAMIC_RESERVE;
	size_t atom_size;
	unsigned long delta;
	unsigned int cpu;
	int rc;

	if (mmu_linear_psize == MMU_PAGE_4K)
		atom_size = PAGE_SIZE;
	else
		atom_size = 1 << 20;

	rc = pcpu_embed_first_chunk(0, dyn_size, atom_size, pcpu_cpu_distance,
				    pcpu_fc_alloc, pcpu_fc_free);
	if (rc < 0)
		panic("cannot initialize percpu area (err=%d)", rc);

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu) {
                __per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];
		paca[cpu].data_offset = __per_cpu_offset[cpu];
	}
}
#endif


#ifdef CONFIG_PPC_INDIRECT_IO
struct ppc_pci_io ppc_pci_io;
EXPORT_SYMBOL(ppc_pci_io);
#endif 

