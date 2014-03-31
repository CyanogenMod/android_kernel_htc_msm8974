/*
 * Common boot and setup code for both 32-bit and 64-bit.
 * Extracted from arch/powerpc/kernel/setup_64.c.
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
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/screen_info.h>
#include <linux/root_dev.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/unistd.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/debugfs.h>
#include <linux/percpu.h>
#include <linux/memblock.h>
#include <linux/of_platform.h>
#include <asm/io.h>
#include <asm/paca.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/vdso_datapage.h>
#include <asm/pgtable.h>
#include <asm/smp.h>
#include <asm/elf.h>
#include <asm/machdep.h>
#include <asm/time.h>
#include <asm/cputable.h>
#include <asm/sections.h>
#include <asm/firmware.h>
#include <asm/btext.h>
#include <asm/nvram.h>
#include <asm/setup.h>
#include <asm/rtas.h>
#include <asm/iommu.h>
#include <asm/serial.h>
#include <asm/cache.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/xmon.h>
#include <asm/cputhreads.h>
#include <mm/mmu_decl.h>
#include <asm/fadump.h>

#include "setup.h"

#ifdef DEBUG
#include <asm/udbg.h>
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

struct machdep_calls ppc_md;
EXPORT_SYMBOL(ppc_md);
struct machdep_calls *machine_id;
EXPORT_SYMBOL(machine_id);

unsigned long klimit = (unsigned long) _end;

char cmd_line[COMMAND_LINE_SIZE];

 
struct screen_info screen_info = {
	.orig_x = 0,
	.orig_y = 25,
	.orig_video_cols = 80,
	.orig_video_lines = 25,
	.orig_video_isVGA = 1,
	.orig_video_points = 16
};

int of_i8042_kbd_irq;
EXPORT_SYMBOL_GPL(of_i8042_kbd_irq);
int of_i8042_aux_irq;
EXPORT_SYMBOL_GPL(of_i8042_aux_irq);

#ifdef __DO_IRQ_CANON
int ppc_do_canonicalize_irqs;
EXPORT_SYMBOL(ppc_do_canonicalize_irqs);
#endif

void machine_shutdown(void)
{
#ifdef CONFIG_FA_DUMP
	fadump_cleanup();
#endif

	if (ppc_md.machine_shutdown)
		ppc_md.machine_shutdown();
}

void machine_restart(char *cmd)
{
	machine_shutdown();
	if (ppc_md.restart)
		ppc_md.restart(cmd);
#ifdef CONFIG_SMP
	smp_send_stop();
#endif
	printk(KERN_EMERG "System Halted, OK to turn off power\n");
	local_irq_disable();
	while (1) ;
}

void machine_power_off(void)
{
	machine_shutdown();
	if (ppc_md.power_off)
		ppc_md.power_off();
#ifdef CONFIG_SMP
	smp_send_stop();
#endif
	printk(KERN_EMERG "System Halted, OK to turn off power\n");
	local_irq_disable();
	while (1) ;
}
EXPORT_SYMBOL_GPL(machine_power_off);

void (*pm_power_off)(void) = machine_power_off;
EXPORT_SYMBOL_GPL(pm_power_off);

void machine_halt(void)
{
	machine_shutdown();
	if (ppc_md.halt)
		ppc_md.halt();
#ifdef CONFIG_SMP
	smp_send_stop();
#endif
	printk(KERN_EMERG "System Halted, OK to turn off power\n");
	local_irq_disable();
	while (1) ;
}


#ifdef CONFIG_TAU
extern u32 cpu_temp(unsigned long cpu);
extern u32 cpu_temp_both(unsigned long cpu);
#endif 

#ifdef CONFIG_SMP
DEFINE_PER_CPU(unsigned int, cpu_pvr);
#endif

static void show_cpuinfo_summary(struct seq_file *m)
{
	struct device_node *root;
	const char *model = NULL;
#if defined(CONFIG_SMP) && defined(CONFIG_PPC32)
	unsigned long bogosum = 0;
	int i;
	for_each_online_cpu(i)
		bogosum += loops_per_jiffy;
	seq_printf(m, "total bogomips\t: %lu.%02lu\n",
		   bogosum/(500000/HZ), bogosum/(5000/HZ) % 100);
#endif 
	seq_printf(m, "timebase\t: %lu\n", ppc_tb_freq);
	if (ppc_md.name)
		seq_printf(m, "platform\t: %s\n", ppc_md.name);
	root = of_find_node_by_path("/");
	if (root)
		model = of_get_property(root, "model", NULL);
	if (model)
		seq_printf(m, "model\t\t: %s\n", model);
	of_node_put(root);

	if (ppc_md.show_cpuinfo != NULL)
		ppc_md.show_cpuinfo(m);

#ifdef CONFIG_PPC32
	
	seq_printf(m, "Memory\t\t: %d MB\n",
		   (unsigned int)(total_memory / (1024 * 1024)));
#endif
}

static int show_cpuinfo(struct seq_file *m, void *v)
{
	unsigned long cpu_id = (unsigned long)v - 1;
	unsigned int pvr;
	unsigned short maj;
	unsigned short min;

	preempt_disable();
	if (!cpu_online(cpu_id)) {
		preempt_enable();
		return 0;
	}

#ifdef CONFIG_SMP
	pvr = per_cpu(cpu_pvr, cpu_id);
#else
	pvr = mfspr(SPRN_PVR);
#endif
	maj = (pvr >> 8) & 0xFF;
	min = pvr & 0xFF;

	seq_printf(m, "processor\t: %lu\n", cpu_id);
	seq_printf(m, "cpu\t\t: ");

	if (cur_cpu_spec->pvr_mask)
		seq_printf(m, "%s", cur_cpu_spec->cpu_name);
	else
		seq_printf(m, "unknown (%08x)", pvr);

#ifdef CONFIG_ALTIVEC
	if (cpu_has_feature(CPU_FTR_ALTIVEC))
		seq_printf(m, ", altivec supported");
#endif 

	seq_printf(m, "\n");

#ifdef CONFIG_TAU
	if (cur_cpu_spec->cpu_features & CPU_FTR_TAU) {
#ifdef CONFIG_TAU_AVERAGE
		
		seq_printf(m,  "temperature \t: %u C (uncalibrated)\n",
			   cpu_temp(cpu_id));
#else
		
		u32 temp;
		temp = cpu_temp_both(cpu_id);
		seq_printf(m, "temperature \t: %u-%u C (uncalibrated)\n",
			   temp & 0xff, temp >> 16);
#endif
	}
#endif 

	if (ppc_proc_freq)
		seq_printf(m, "clock\t\t: %lu.%06luMHz\n",
			   ppc_proc_freq / 1000000, ppc_proc_freq % 1000000);

	if (ppc_md.show_percpuinfo != NULL)
		ppc_md.show_percpuinfo(m, cpu_id);

	if (PVR_VER(pvr) & 0x8000) {
		switch (PVR_VER(pvr)) {
		case 0x8000:	
		case 0x8001:	
		case 0x8002:	
		case 0x8003:	
		case 0x8004:	
		case 0x800c:	
			maj = ((pvr >> 8) & 0xF);
			min = PVR_MIN(pvr);
			break;
		default:	
			maj = PVR_MAJ(pvr);
			min = PVR_MIN(pvr);
			break;
		}
	} else {
		switch (PVR_VER(pvr)) {
			case 0x0020:	
				maj = PVR_MAJ(pvr) + 1;
				min = PVR_MIN(pvr);
				break;
			case 0x1008:	
				maj = ((pvr >> 8) & 0xFF) - 1;
				min = pvr & 0xFF;
				break;
			default:
				maj = (pvr >> 8) & 0xFF;
				min = pvr & 0xFF;
				break;
		}
	}

	seq_printf(m, "revision\t: %hd.%hd (pvr %04x %04x)\n",
		   maj, min, PVR_VER(pvr), PVR_REV(pvr));

#ifdef CONFIG_PPC32
	seq_printf(m, "bogomips\t: %lu.%02lu\n",
		   loops_per_jiffy / (500000/HZ),
		   (loops_per_jiffy / (5000/HZ)) % 100);
#endif

#ifdef CONFIG_SMP
	seq_printf(m, "\n");
#endif

	preempt_enable();

	
	if (cpumask_next(cpu_id, cpu_online_mask) >= nr_cpu_ids)
		show_cpuinfo_summary(m);

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	if (*pos == 0)	
		*pos = cpumask_first(cpu_online_mask);
	else
		*pos = cpumask_next(*pos - 1, cpu_online_mask);
	if ((*pos) < nr_cpu_ids)
		return (void *)(unsigned long)(*pos + 1);
	return NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start =c_start,
	.next =	c_next,
	.stop =	c_stop,
	.show =	show_cpuinfo,
};

void __init check_for_initrd(void)
{
#ifdef CONFIG_BLK_DEV_INITRD
	DBG(" -> check_for_initrd()  initrd_start=0x%lx  initrd_end=0x%lx\n",
	    initrd_start, initrd_end);

	if (is_kernel_addr(initrd_start) && is_kernel_addr(initrd_end) &&
	    initrd_end > initrd_start)
		ROOT_DEV = Root_RAM0;
	else
		initrd_start = initrd_end = 0;

	if (initrd_start)
		printk("Found initrd at 0x%lx:0x%lx\n", initrd_start, initrd_end);

	DBG(" <- check_for_initrd()\n");
#endif 
}

#ifdef CONFIG_SMP

int threads_per_core, threads_shift;
cpumask_t threads_core_mask;
EXPORT_SYMBOL_GPL(threads_per_core);
EXPORT_SYMBOL_GPL(threads_shift);
EXPORT_SYMBOL_GPL(threads_core_mask);

static void __init cpu_init_thread_core_maps(int tpc)
{
	int i;

	threads_per_core = tpc;
	cpumask_clear(&threads_core_mask);

	threads_shift = ilog2(tpc);
	BUG_ON(tpc != (1 << threads_shift));

	for (i = 0; i < tpc; i++)
		cpumask_set_cpu(i, &threads_core_mask);

	printk(KERN_INFO "CPU maps initialized for %d thread%s per core\n",
	       tpc, tpc > 1 ? "s" : "");
	printk(KERN_DEBUG " (thread shift is %d)\n", threads_shift);
}


void __init smp_setup_cpu_maps(void)
{
	struct device_node *dn = NULL;
	int cpu = 0;
	int nthreads = 1;

	DBG("smp_setup_cpu_maps()\n");

	while ((dn = of_find_node_by_type(dn, "cpu")) && cpu < nr_cpu_ids) {
		const int *intserv;
		int j, len;

		DBG("  * %s...\n", dn->full_name);

		intserv = of_get_property(dn, "ibm,ppc-interrupt-server#s",
				&len);
		if (intserv) {
			nthreads = len / sizeof(int);
			DBG("    ibm,ppc-interrupt-server#s -> %d threads\n",
			    nthreads);
		} else {
			DBG("    no ibm,ppc-interrupt-server#s -> 1 thread\n");
			intserv = of_get_property(dn, "reg", NULL);
			if (!intserv)
				intserv = &cpu;	
		}

		for (j = 0; j < nthreads && cpu < nr_cpu_ids; j++) {
			DBG("    thread %d -> cpu %d (hard id %d)\n",
			    j, cpu, intserv[j]);
			set_cpu_present(cpu, true);
			set_hard_smp_processor_id(cpu, intserv[j]);
			set_cpu_possible(cpu, true);
			cpu++;
		}
	}

	
	if (!cpu_has_feature(CPU_FTR_SMT)) {
		DBG("  SMT disabled ! nthreads forced to 1\n");
		nthreads = 1;
	}

#ifdef CONFIG_PPC64
	if (machine_is(pseries) && firmware_has_feature(FW_FEATURE_LPAR) &&
	    (dn = of_find_node_by_path("/rtas"))) {
		int num_addr_cell, num_size_cell, maxcpus;
		const unsigned int *ireg;

		num_addr_cell = of_n_addr_cells(dn);
		num_size_cell = of_n_size_cells(dn);

		ireg = of_get_property(dn, "ibm,lrdr-capacity", NULL);

		if (!ireg)
			goto out;

		maxcpus = ireg[num_addr_cell + num_size_cell];

		
		if (cpu_has_feature(CPU_FTR_SMT))
			maxcpus *= nthreads;

		if (maxcpus > nr_cpu_ids) {
			printk(KERN_WARNING
			       "Partition configured for %d cpus, "
			       "operating system maximum is %d.\n",
			       maxcpus, nr_cpu_ids);
			maxcpus = nr_cpu_ids;
		} else
			printk(KERN_INFO "Partition configured for %d cpus.\n",
			       maxcpus);

		for (cpu = 0; cpu < maxcpus; cpu++)
			set_cpu_possible(cpu, true);
	out:
		of_node_put(dn);
	}
	vdso_data->processorCount = num_present_cpus();
#endif 

	cpu_init_thread_core_maps(nthreads);

	
	setup_nr_cpu_ids();

	free_unused_pacas();
}
#endif 

#ifdef CONFIG_PCSPKR_PLATFORM
static __init int add_pcspkr(void)
{
	struct device_node *np;
	struct platform_device *pd;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "pnpPNP,100");
	of_node_put(np);
	if (!np)
		return -ENODEV;

	pd = platform_device_alloc("pcspkr", -1);
	if (!pd)
		return -ENOMEM;

	ret = platform_device_add(pd);
	if (ret)
		platform_device_put(pd);

	return ret;
}
device_initcall(add_pcspkr);
#endif	

void probe_machine(void)
{
	extern struct machdep_calls __machine_desc_start;
	extern struct machdep_calls __machine_desc_end;

	DBG("Probing machine type ...\n");

	for (machine_id = &__machine_desc_start;
	     machine_id < &__machine_desc_end;
	     machine_id++) {
		DBG("  %s ...", machine_id->name);
		memcpy(&ppc_md, machine_id, sizeof(struct machdep_calls));
		if (ppc_md.probe()) {
			DBG(" match !\n");
			break;
		}
		DBG("\n");
	}
	
	if (machine_id >= &__machine_desc_end) {
		DBG("No suitable machine found !\n");
		for (;;);
	}

	printk(KERN_INFO "Using %s machine description\n", ppc_md.name);
}

int check_legacy_ioport(unsigned long base_port)
{
	struct device_node *parent, *np = NULL;
	int ret = -ENODEV;

	switch(base_port) {
	case I8042_DATA_REG:
		if (!(np = of_find_compatible_node(NULL, NULL, "pnpPNP,303")))
			np = of_find_compatible_node(NULL, NULL, "pnpPNP,f03");
		if (np) {
			parent = of_get_parent(np);

			of_i8042_kbd_irq = irq_of_parse_and_map(parent, 0);
			if (!of_i8042_kbd_irq)
				of_i8042_kbd_irq = 1;

			of_i8042_aux_irq = irq_of_parse_and_map(parent, 1);
			if (!of_i8042_aux_irq)
				of_i8042_aux_irq = 12;

			of_node_put(np);
			np = parent;
			break;
		}
		np = of_find_node_by_type(NULL, "8042");
		if (!np)
			np = of_find_node_by_name(NULL, "8042");
		if (np) {
			of_i8042_kbd_irq = 1;
			of_i8042_aux_irq = 12;
		}
		break;
	case FDC_BASE: 
		np = of_find_node_by_type(NULL, "fdc");
		break;
#ifdef CONFIG_PPC_PREP
	case _PIDXR:
	case _PNPWRP:
	case PNPBIOS_BASE:
		
#endif
	default:
		
		break;
	}
	if (!np)
		return ret;
	parent = of_get_parent(np);
	if (parent) {
		if (strcmp(parent->type, "isa") == 0)
			ret = 0;
		of_node_put(parent);
	}
	of_node_put(np);
	return ret;
}
EXPORT_SYMBOL(check_legacy_ioport);

static int ppc_panic_event(struct notifier_block *this,
                             unsigned long event, void *ptr)
{
	crash_fadump(NULL, ptr);
	ppc_md.panic(ptr);  
	return NOTIFY_DONE;
}

static struct notifier_block ppc_panic_block = {
	.notifier_call = ppc_panic_event,
	.priority = INT_MIN 
};

void __init setup_panic(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &ppc_panic_block);
}

#ifdef CONFIG_CHECK_CACHE_COHERENCY

#ifdef CONFIG_NOT_COHERENT_CACHE
#define KERNEL_COHERENCY	0
#else
#define KERNEL_COHERENCY	1
#endif

static int __init check_cache_coherency(void)
{
	struct device_node *np;
	const void *prop;
	int devtree_coherency;

	np = of_find_node_by_path("/");
	prop = of_get_property(np, "coherency-off", NULL);
	of_node_put(np);

	devtree_coherency = prop ? 0 : 1;

	if (devtree_coherency != KERNEL_COHERENCY) {
		printk(KERN_ERR
			"kernel coherency:%s != device tree_coherency:%s\n",
			KERNEL_COHERENCY ? "on" : "off",
			devtree_coherency ? "on" : "off");
		BUG();
	}

	return 0;
}

late_initcall(check_cache_coherency);
#endif 

#ifdef CONFIG_DEBUG_FS
struct dentry *powerpc_debugfs_root;
EXPORT_SYMBOL(powerpc_debugfs_root);

static int powerpc_debugfs_init(void)
{
	powerpc_debugfs_root = debugfs_create_dir("powerpc", NULL);

	return powerpc_debugfs_root == NULL;
}
arch_initcall(powerpc_debugfs_init);
#endif

void ppc_printk_progress(char *s, unsigned short hex)
{
	pr_info("%s\n", s);
}

void arch_setup_pdev_archdata(struct platform_device *pdev)
{
	pdev->archdata.dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &pdev->archdata.dma_mask;
 	set_dma_ops(&pdev->dev, &dma_direct_ops);
}
