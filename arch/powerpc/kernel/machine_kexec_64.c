/*
 * PPC64 code to handle Linux booting another kernel.
 *
 * Copyright (C) 2004-2005, IBM Corp.
 *
 * Created by: Milton D Miller II
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */


#include <linux/kexec.h>
#include <linux/smp.h>
#include <linux/thread_info.h>
#include <linux/init_task.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/cpu.h>

#include <asm/page.h>
#include <asm/current.h>
#include <asm/machdep.h>
#include <asm/cacheflush.h>
#include <asm/paca.h>
#include <asm/mmu.h>
#include <asm/sections.h>	
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/hw_breakpoint.h>

int default_machine_kexec_prepare(struct kimage *image)
{
	int i;
	unsigned long begin, end;	
	unsigned long low, high;	
	struct device_node *node;
	const unsigned long *basep;
	const unsigned int *sizep;

	if (!ppc_md.hpte_clear_all)
		return -ENOENT;

	for (i = 0; i < image->nr_segments; i++)
		if (image->segment[i].mem < __pa(_end))
			return -ETXTBSY;

	if (htab_address) {
		low = __pa(htab_address);
		high = low + htab_size_bytes;

		for (i = 0; i < image->nr_segments; i++) {
			begin = image->segment[i].mem;
			end = begin + image->segment[i].memsz;

			if ((begin < high) && (end > low))
				return -ETXTBSY;
		}
	}

	
	for_each_node_by_type(node, "pci") {
		basep = of_get_property(node, "linux,tce-base", NULL);
		sizep = of_get_property(node, "linux,tce-size", NULL);
		if (basep == NULL || sizep == NULL)
			continue;

		low = *basep;
		high = low + (*sizep);

		for (i = 0; i < image->nr_segments; i++) {
			begin = image->segment[i].mem;
			end = begin + image->segment[i].memsz;

			if ((begin < high) && (end > low))
				return -ETXTBSY;
		}
	}

	return 0;
}

#define IND_FLAGS (IND_DESTINATION | IND_INDIRECTION | IND_DONE | IND_SOURCE)

static void copy_segments(unsigned long ind)
{
	unsigned long entry;
	unsigned long *ptr;
	void *dest;
	void *addr;

	ptr = NULL;
	dest = NULL;

	for (entry = ind; !(entry & IND_DONE); entry = *ptr++) {
		addr = __va(entry & PAGE_MASK);

		switch (entry & IND_FLAGS) {
		case IND_DESTINATION:
			dest = addr;
			break;
		case IND_INDIRECTION:
			ptr = addr;
			break;
		case IND_SOURCE:
			copy_page(dest, addr);
			dest += PAGE_SIZE;
		}
	}
}

void kexec_copy_flush(struct kimage *image)
{
	long i, nr_segments = image->nr_segments;
	struct  kexec_segment ranges[KEXEC_SEGMENT_MAX];

	
	memcpy(ranges, image->segment, sizeof(ranges));

	copy_segments(image->head);

	for (i = 0; i < nr_segments; i++)
		flush_icache_range((unsigned long)__va(ranges[i].mem),
			(unsigned long)__va(ranges[i].mem + ranges[i].memsz));
}

#ifdef CONFIG_SMP

static int kexec_all_irq_disabled = 0;

static void kexec_smp_down(void *arg)
{
	local_irq_disable();
	mb(); 
	get_paca()->kexec_state = KEXEC_STATE_IRQS_OFF;
	while(kexec_all_irq_disabled == 0)
		cpu_relax();
	mb(); 
	hw_breakpoint_disable();
	if (ppc_md.kexec_cpu_down)
		ppc_md.kexec_cpu_down(0, 1);

	kexec_smp_wait();
	
}

static void kexec_prepare_cpus_wait(int wait_state)
{
	int my_cpu, i, notified=-1;

	hw_breakpoint_disable();
	my_cpu = get_cpu();
	/* Make sure each CPU has at least made it to the state we need.
	 *
	 * FIXME: There is a (slim) chance of a problem if not all of the CPUs
	 * are correctly onlined.  If somehow we start a CPU on boot with RTAS
	 * start-cpu, but somehow that CPU doesn't write callin_cpu_map[] in
	 * time, the boot CPU will timeout.  If it does eventually execute
	 * stuff, the secondary will start up (paca[].cpu_start was written) and
	 * get into a peculiar state.  If the platform supports
	 * smp_ops->take_timebase(), the secondary CPU will probably be spinning
	 * in there.  If not (i.e. pseries), the secondary will continue on and
	 * try to online itself/idle/etc. If it survives that, we need to find
	 * these possible-but-not-online-but-should-be CPUs and chaperone them
	 * into kexec_smp_wait().
	 */
	for_each_online_cpu(i) {
		if (i == my_cpu)
			continue;

		while (paca[i].kexec_state < wait_state) {
			barrier();
			if (i != notified) {
				printk(KERN_INFO "kexec: waiting for cpu %d "
				       "(physical %d) to enter %i state\n",
				       i, paca[i].hw_cpu_id, wait_state);
				notified = i;
			}
		}
	}
	mb();
}

static void wake_offline_cpus(void)
{
	int cpu = 0;

	for_each_present_cpu(cpu) {
		if (!cpu_online(cpu)) {
			printk(KERN_INFO "kexec: Waking offline cpu %d.\n",
			       cpu);
			cpu_up(cpu);
		}
	}
}

static void kexec_prepare_cpus(void)
{
	wake_offline_cpus();
	smp_call_function(kexec_smp_down, NULL, 0);
	local_irq_disable();
	mb(); 
	get_paca()->kexec_state = KEXEC_STATE_IRQS_OFF;

	kexec_prepare_cpus_wait(KEXEC_STATE_IRQS_OFF);
	
	kexec_all_irq_disabled = 1;

	
	if (ppc_md.kexec_cpu_down)
		ppc_md.kexec_cpu_down(0, 0);

	kexec_prepare_cpus_wait(KEXEC_STATE_REAL_MODE);

	put_cpu();
}

#else 

static void kexec_prepare_cpus(void)
{
	smp_release_cpus();
	if (ppc_md.kexec_cpu_down)
		ppc_md.kexec_cpu_down(0, 0);
	local_irq_disable();
}

#endif 

/*
 * kexec thread structure and stack.
 *
 * We need to make sure that this is 16384-byte aligned due to the
 * way process stacks are handled.  It also must be statically allocated
 * or allocated as part of the kimage, because everything else may be
 * overwritten when we copy the kexec image.  We piggyback on the
 * "init_task" linker section here to statically allocate a stack.
 *
 * We could use a smaller stack if we don't care about anything using
 * current, but that audit has not been performed.
 */
static union thread_union kexec_stack __init_task_data =
	{ };

struct paca_struct kexec_paca;

extern void kexec_sequence(void *newstack, unsigned long start,
			   void *image, void *control,
			   void (*clear_all)(void)) __noreturn;

void default_machine_kexec(struct kimage *image)
{
	


	if (crashing_cpu == -1)
		kexec_prepare_cpus();

	pr_debug("kexec: Starting switchover sequence.\n");

	kexec_stack.thread_info.task = current_thread_info()->task;
	kexec_stack.thread_info.flags = 0;

	memcpy(&kexec_paca, get_paca(), sizeof(struct paca_struct));
	kexec_paca.data_offset = 0xedeaddeadeeeeeeeUL;
	paca = (struct paca_struct *)RELOC_HIDE(&kexec_paca, 0) -
		kexec_paca.paca_index;
	setup_paca(&kexec_paca);


	kexec_sequence(&kexec_stack, image->start, image,
			page_address(image->control_code_page),
			ppc_md.hpte_clear_all);
	
}

static unsigned long htab_base;

static struct property htab_base_prop = {
	.name = "linux,htab-base",
	.length = sizeof(unsigned long),
	.value = &htab_base,
};

static struct property htab_size_prop = {
	.name = "linux,htab-size",
	.length = sizeof(unsigned long),
	.value = &htab_size_bytes,
};

static int __init export_htab_values(void)
{
	struct device_node *node;
	struct property *prop;

	
	if (!htab_address)
		return -ENODEV;

	node = of_find_node_by_path("/chosen");
	if (!node)
		return -ENODEV;

	
	prop = of_find_property(node, htab_base_prop.name, NULL);
	if (prop)
		prom_remove_property(node, prop);
	prop = of_find_property(node, htab_size_prop.name, NULL);
	if (prop)
		prom_remove_property(node, prop);

	htab_base = __pa(htab_address);
	prom_add_property(node, &htab_base_prop);
	prom_add_property(node, &htab_size_prop);

	of_node_put(node);
	return 0;
}
late_initcall(export_htab_values);
