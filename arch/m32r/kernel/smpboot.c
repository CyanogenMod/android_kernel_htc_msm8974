/*
 *  linux/arch/m32r/kernel/smpboot.c
 *    orig : i386 2.4.10
 *
 *  M32R SMP booting functions
 *
 *  Copyright (c) 2001, 2002, 2003  Hitoshi Yamamoto
 *
 *  Taken from i386 version.
 *	  (c) 1995 Alan Cox, Building #3 <alan@redhat.com>
 *	  (c) 1998, 1999, 2000 Ingo Molnar <mingo@redhat.com>
 *
 *	Much of the core SMP work is based on previous work by Thomas Radke, to
 *	whom a great many thanks are extended.
 *
 *	Thanks to Intel for making available several different Pentium,
 *	Pentium Pro and Pentium-II/Xeon MP machines.
 *	Original development of Linux SMP code supported by Caldera.
 *
 *	This code is released under the GNU General Public License version 2 or
 *	later.
 *
 *	Fixes
 *		Felix Koop	:	NR_CPUS used properly
 *		Jose Renau	:	Handle single CPU case.
 *		Alan Cox	:	By repeated request
 *					8) - Total BogoMIP report.
 *		Greg Wright	:	Fix for kernel stacks panic.
 *		Erich Boleyn	:	MP v1.4 and additional changes.
 *	Matthias Sattler	:	Changes for 2.1 kernel map.
 *	Michel Lespinasse	:	Changes for 2.1 kernel map.
 *	Michael Chastain	:	Change trampoline.S to gnu as.
 *		Alan Cox	:	Dumb bug: 'B' step PPro's are fine
 *		Ingo Molnar	:	Added APIC timers, based on code
 *					from Jose Renau
 *		Ingo Molnar	:	various cleanups and rewrites
 *		Tigran Aivazian	:	fixed "0.00 in /proc/uptime on SMP" bug.
 *	Maciej W. Rozycki	:	Bits for genuine 82489DX APICs
 *		Martin J. Bligh	: 	Added support for multi-quad systems
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

#define DEBUG_SMP
#ifdef DEBUG_SMP
#define Dprintk(x...) printk(x)
#else
#define Dprintk(x...)
#endif

extern cpumask_t cpu_initialized;


static unsigned int bsp_phys_id = -1;

physid_mask_t phys_cpu_present_map;

cpumask_t cpu_bootout_map;
cpumask_t cpu_bootin_map;
static cpumask_t cpu_callin_map;
cpumask_t cpu_callout_map;
EXPORT_SYMBOL(cpu_callout_map);

struct cpuinfo_m32r cpu_data[NR_CPUS] __cacheline_aligned;

static int cpucount;
static cpumask_t smp_commenced_mask;

extern struct {
	void * spi;
	unsigned short ss;
} stack_start;

static volatile int physid_2_cpu[NR_CPUS];
#define physid_to_cpu(physid)	physid_2_cpu[physid]

volatile int cpu_2_physid[NR_CPUS];

DEFINE_PER_CPU(int, prof_multiplier) = 1;
DEFINE_PER_CPU(int, prof_old_multiplier) = 1;
DEFINE_PER_CPU(int, prof_counter) = 1;

spinlock_t ipi_lock[NR_IPIS];

static unsigned int calibration_result;


void smp_prepare_boot_cpu(void);
void smp_prepare_cpus(unsigned int);
static void init_ipi_lock(void);
static void do_boot_cpu(int);
int __cpu_up(unsigned int);
void smp_cpus_done(unsigned int);

int start_secondary(void *);
static void smp_callin(void);
static void smp_online(void);

static void show_mp_info(int);
static void smp_store_cpu_info(int);
static void show_cpu_info(int);
int setup_profiling_timer(unsigned int);
static void init_cpu_to_physid(void);
static void map_cpu_to_physid(int, int);
static void unmap_cpu_to_physid(int, int);

void __devinit smp_prepare_boot_cpu(void)
{
	bsp_phys_id = hard_smp_processor_id();
	physid_set(bsp_phys_id, phys_cpu_present_map);
	set_cpu_online(0, true);	
	cpumask_set_cpu(0, &cpu_callout_map);
	cpumask_set_cpu(0, &cpu_callin_map);

	init_cpu_to_physid();
	map_cpu_to_physid(0, bsp_phys_id);
	current_thread_info()->cpu = 0;
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int phys_id;
	unsigned long nr_cpu;

	nr_cpu = inl(M32R_FPGA_NUM_OF_CPUS_PORTL);
	if (nr_cpu > NR_CPUS) {
		printk(KERN_INFO "NUM_OF_CPUS reg. value [%ld] > NR_CPU [%d]",
			nr_cpu, NR_CPUS);
		goto smp_done;
	}
	for (phys_id = 0 ; phys_id < nr_cpu ; phys_id++)
		physid_set(phys_id, phys_cpu_present_map);
#ifndef CONFIG_HOTPLUG_CPU
	init_cpu_present(cpu_possible_mask);
#endif

	show_mp_info(nr_cpu);

	init_ipi_lock();

	smp_store_cpu_info(0); 

	if (!max_cpus) {
		printk(KERN_INFO "SMP mode deactivated by commandline.\n");
		goto smp_done;
	}

	Dprintk("CPU present map : %lx\n", physids_coerce(phys_cpu_present_map));

	for (phys_id = 0 ; phys_id < NR_CPUS ; phys_id++) {
		if (phys_id == bsp_phys_id)
			continue;

		if (!physid_isset(phys_id, phys_cpu_present_map))
			continue;

		if (max_cpus <= cpucount + 1)
			continue;

		do_boot_cpu(phys_id);

		if (physid_to_cpu(phys_id) == -1) {
			physid_clear(phys_id, phys_cpu_present_map);
			printk("phys CPU#%d not responding - " \
				"cannot use it.\n", phys_id);
		}
	}

smp_done:
	Dprintk("Boot done.\n");
}

static void __init init_ipi_lock(void)
{
	int ipi;

	for (ipi = 0 ; ipi < NR_IPIS ; ipi++)
		spin_lock_init(&ipi_lock[ipi]);
}

static void __init do_boot_cpu(int phys_id)
{
	struct task_struct *idle;
	unsigned long send_status, boot_status;
	int timeout, cpu_id;

	cpu_id = ++cpucount;

	idle = fork_idle(cpu_id);
	if (IS_ERR(idle))
		panic("failed fork for CPU#%d.", cpu_id);

	idle->thread.lr = (unsigned long)start_secondary;

	map_cpu_to_physid(cpu_id, phys_id);

	
	printk("Booting processor %d/%d\n", phys_id, cpu_id);
	stack_start.spi = (void *)idle->thread.sp;
	task_thread_info(idle)->cpu = cpu_id;

	send_status = 0;
	boot_status = 0;

	cpumask_set_cpu(phys_id, &cpu_bootout_map);

	
	send_IPI_mask_phys(cpumask_of(phys_id), CPU_BOOT_IPI, 0);

	Dprintk("Waiting for send to finish...\n");
	timeout = 0;

	
	do {
		Dprintk("+");
		udelay(1000);
		send_status = !cpumask_test_cpu(phys_id, &cpu_bootin_map);
	} while (send_status && (timeout++ < 100));

	Dprintk("After Startup.\n");

	if (!send_status) {
		Dprintk("Before Callout %d.\n", cpu_id);
		cpumask_set_cpu(cpu_id, &cpu_callout_map);
		Dprintk("After Callout %d.\n", cpu_id);

		for (timeout = 0; timeout < 5000; timeout++) {
			if (cpumask_test_cpu(cpu_id, &cpu_callin_map))
				break;	
			udelay(1000);
		}

		if (cpumask_test_cpu(cpu_id, &cpu_callin_map)) {
			
			Dprintk("OK.\n");
		} else {
			boot_status = 1;
			printk("Not responding.\n");
		}
	} else
		printk("IPI never delivered???\n");

	if (send_status || boot_status) {
		unmap_cpu_to_physid(cpu_id, phys_id);
		cpumask_clear_cpu(cpu_id, &cpu_callout_map);
		cpumask_clear_cpu(cpu_id, &cpu_callin_map);
		cpumask_clear_cpu(cpu_id, &cpu_initialized);
		cpucount--;
	}
}

int __cpuinit __cpu_up(unsigned int cpu_id)
{
	int timeout;

	cpumask_set_cpu(cpu_id, &smp_commenced_mask);

	for (timeout = 0; timeout < 5000; timeout++) {
		if (cpu_online(cpu_id))
			break;
		udelay(1000);
	}
	if (!cpu_online(cpu_id))
		BUG();

	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
	int cpu_id, timeout;
	unsigned long bogosum = 0;

	for (timeout = 0; timeout < 5000; timeout++) {
		if (cpumask_equal(&cpu_callin_map, cpu_online_mask))
			break;
		udelay(1000);
	}
	if (!cpumask_equal(&cpu_callin_map, cpu_online_mask))
		BUG();

	for (cpu_id = 0 ; cpu_id < num_online_cpus() ; cpu_id++)
		show_cpu_info(cpu_id);

	Dprintk("Before bogomips.\n");
	if (cpucount) {
		for_each_cpu(cpu_id,cpu_online_mask)
			bogosum += cpu_data[cpu_id].loops_per_jiffy;

		printk(KERN_INFO "Total of %d processors activated " \
			"(%lu.%02lu BogoMIPS).\n", cpucount + 1,
			bogosum / (500000 / HZ),
			(bogosum / (5000 / HZ)) % 100);
		Dprintk("Before bogocount - setting activated=1.\n");
	}
}


int __init start_secondary(void *unused)
{
	cpu_init();
	preempt_disable();
	smp_callin();
	while (!cpumask_test_cpu(smp_processor_id(), &smp_commenced_mask))
		cpu_relax();

	smp_online();

	local_flush_tlb_all();

	cpu_idle();
	return 0;
}

static void __init smp_callin(void)
{
	int phys_id = hard_smp_processor_id();
	int cpu_id = smp_processor_id();
	unsigned long timeout;

	if (cpumask_test_cpu(cpu_id, &cpu_callin_map)) {
		printk("huh, phys CPU#%d, CPU#%d already present??\n",
			phys_id, cpu_id);
		BUG();
	}
	Dprintk("CPU#%d (phys ID: %d) waiting for CALLOUT\n", cpu_id, phys_id);

	
	timeout = jiffies + (2 * HZ);
	while (time_before(jiffies, timeout)) {
		
		if (cpumask_test_cpu(cpu_id, &cpu_callout_map))
			break;
		cpu_relax();
	}

	if (!time_before(jiffies, timeout)) {
		printk("BUG: CPU#%d started up but did not get a callout!\n",
			cpu_id);
		BUG();
	}

	
	cpumask_set_cpu(cpu_id, &cpu_callin_map);
}

static void __init smp_online(void)
{
	int cpu_id = smp_processor_id();

	notify_cpu_starting(cpu_id);

	local_irq_enable();

	
	calibrate_delay();

	
 	smp_store_cpu_info(cpu_id);

	set_cpu_online(cpu_id, true);
}

static void __init show_mp_info(int nr_cpu)
{
	int i;
	char cpu_model0[17], cpu_model1[17], cpu_ver[9];

	strncpy(cpu_model0, (char *)M32R_FPGA_CPU_NAME_ADDR, 16);
	strncpy(cpu_model1, (char *)M32R_FPGA_MODEL_ID_ADDR, 16);
	strncpy(cpu_ver, (char *)M32R_FPGA_VERSION_ADDR, 8);

	cpu_model0[16] = '\0';
	for (i = 15 ; i >= 0 ; i--) {
		if (cpu_model0[i] != ' ')
			break;
		cpu_model0[i] = '\0';
	}
	cpu_model1[16] = '\0';
	for (i = 15 ; i >= 0 ; i--) {
		if (cpu_model1[i] != ' ')
			break;
		cpu_model1[i] = '\0';
	}
	cpu_ver[8] = '\0';
	for (i = 7 ; i >= 0 ; i--) {
		if (cpu_ver[i] != ' ')
			break;
		cpu_ver[i] = '\0';
	}

	printk(KERN_INFO "M32R-mp information\n");
	printk(KERN_INFO "  On-chip CPUs : %d\n", nr_cpu);
	printk(KERN_INFO "  CPU model : %s/%s(%s)\n", cpu_model0,
		cpu_model1, cpu_ver);
}

static void __init smp_store_cpu_info(int cpu_id)
{
	struct cpuinfo_m32r *ci = cpu_data + cpu_id;

	*ci = boot_cpu_data;
	ci->loops_per_jiffy = loops_per_jiffy;
}

static void __init show_cpu_info(int cpu_id)
{
	struct cpuinfo_m32r *ci = &cpu_data[cpu_id];

	printk("CPU#%d : ", cpu_id);

#define PRINT_CLOCK(name, value) \
	printk(name " clock %d.%02dMHz", \
		((value) / 1000000), ((value) % 1000000) / 10000)

	PRINT_CLOCK("CPU", (int)ci->cpu_clock);
	PRINT_CLOCK(", Bus", (int)ci->bus_clock);
	printk(", loops_per_jiffy[%ld]\n", ci->loops_per_jiffy);
}

int setup_profiling_timer(unsigned int multiplier)
{
	int i;

	if ( (!multiplier) || (calibration_result / multiplier < 500))
		return -EINVAL;

	for_each_possible_cpu(i)
		per_cpu(prof_multiplier, i) = multiplier;

	return 0;
}

static void __init init_cpu_to_physid(void)
{
	int  i;

	for (i = 0 ; i < NR_CPUS ; i++) {
		cpu_2_physid[i] = -1;
		physid_2_cpu[i] = -1;
	}
}

static void __init map_cpu_to_physid(int cpu_id, int phys_id)
{
	physid_2_cpu[phys_id] = cpu_id;
	cpu_2_physid[cpu_id] = phys_id;
}

static void __init unmap_cpu_to_physid(int cpu_id, int phys_id)
{
	physid_2_cpu[phys_id] = -1;
	cpu_2_physid[cpu_id] = -1;
}
