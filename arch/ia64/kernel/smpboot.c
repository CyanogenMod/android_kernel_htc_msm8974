/*
 * SMP boot-related support
 *
 * Copyright (C) 1998-2003, 2005 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 2001, 2004-2005 Intel Corp
 * 	Rohit Seth <rohit.seth@intel.com>
 * 	Suresh Siddha <suresh.b.siddha@intel.com>
 * 	Gordon Jin <gordon.jin@intel.com>
 *	Ashok Raj  <ashok.raj@intel.com>
 *
 * 01/05/16 Rohit Seth <rohit.seth@intel.com>	Moved SMP booting functions from smp.c to here.
 * 01/04/27 David Mosberger <davidm@hpl.hp.com>	Added ITC synching code.
 * 02/07/31 David Mosberger <davidm@hpl.hp.com>	Switch over to hotplug-CPU boot-sequence.
 *						smp_boot_cpus()/smp_commence() is replaced by
 *						smp_prepare_cpus()/__cpu_up()/smp_cpus_done().
 * 04/06/21 Ashok Raj		<ashok.raj@intel.com> Added CPU Hotplug Support
 * 04/12/26 Jin Gordon <gordon.jin@intel.com>
 * 04/12/26 Rohit Seth <rohit.seth@intel.com>
 *						Add multi-threading and multi-core detection
 * 05/01/30 Suresh Siddha <suresh.b.siddha@intel.com>
 *						Setup cpu_sibling_map and cpu_core_map
 */

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/bootmem.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/efi.h>
#include <linux/percpu.h>
#include <linux/bitops.h>

#include <linux/atomic.h>
#include <asm/cache.h>
#include <asm/current.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/machvec.h>
#include <asm/mca.h>
#include <asm/page.h>
#include <asm/paravirt.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/tlbflush.h>
#include <asm/unistd.h>
#include <asm/sn/arch.h>

#define SMP_DEBUG 0

#if SMP_DEBUG
#define Dprintk(x...)  printk(x)
#else
#define Dprintk(x...)
#endif

#ifdef CONFIG_HOTPLUG_CPU
#ifdef CONFIG_PERMIT_BSP_REMOVE
#define bsp_remove_ok	1
#else
#define bsp_remove_ok	0
#endif

struct task_struct *idle_thread_array[NR_CPUS];

struct sal_to_os_boot sal_boot_rendez_state[NR_CPUS];

struct sal_to_os_boot *sal_state_for_booting_cpu = &sal_boot_rendez_state[0];

#define set_brendez_area(x) (sal_state_for_booting_cpu = &sal_boot_rendez_state[(x)]);

#define get_idle_for_cpu(x)		(idle_thread_array[(x)])
#define set_idle_for_cpu(x,p)	(idle_thread_array[(x)] = (p))

#else

#define get_idle_for_cpu(x)		(NULL)
#define set_idle_for_cpu(x,p)
#define set_brendez_area(x)
#endif


#define MASTER	(0)
#define SLAVE	(SMP_CACHE_BYTES/8)

#define NUM_ROUNDS	64	
#define NUM_ITERS	5	

static DEFINE_SPINLOCK(itc_sync_lock);
static volatile unsigned long go[SLAVE + 1];

#define DEBUG_ITC_SYNC	0

extern void start_ap (void);
extern unsigned long ia64_iobase;

struct task_struct *task_for_booting_cpu;

DEFINE_PER_CPU(int, cpu_state);

cpumask_t cpu_core_map[NR_CPUS] __cacheline_aligned;
EXPORT_SYMBOL(cpu_core_map);
DEFINE_PER_CPU_SHARED_ALIGNED(cpumask_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

int smp_num_siblings = 1;

volatile int ia64_cpu_to_sapicid[NR_CPUS];
EXPORT_SYMBOL(ia64_cpu_to_sapicid);

static volatile cpumask_t cpu_callin_map;

struct smp_boot_data smp_boot_data __initdata;

unsigned long ap_wakeup_vector = -1; 

char __initdata no_int_routing;

unsigned char smp_int_redirect; 

#ifdef CONFIG_FORCE_CPEI_RETARGET
#define CPEI_OVERRIDE_DEFAULT	(1)
#else
#define CPEI_OVERRIDE_DEFAULT	(0)
#endif

unsigned int force_cpei_retarget = CPEI_OVERRIDE_DEFAULT;

static int __init
cmdl_force_cpei(char *str)
{
	int value=0;

	get_option (&str, &value);
	force_cpei_retarget = value;

	return 1;
}

__setup("force_cpei=", cmdl_force_cpei);

static int __init
nointroute (char *str)
{
	no_int_routing = 1;
	printk ("no_int_routing on\n");
	return 1;
}

__setup("nointroute", nointroute);

static void fix_b0_for_bsp(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpuid;
	static int fix_bsp_b0 = 1;

	cpuid = smp_processor_id();

	if (!(fix_bsp_b0 && cpuid))
		return;

	sal_boot_rendez_state[0].br[0] = sal_boot_rendez_state[cpuid].br[0];
	printk ("Fixed BSP b0 value from CPU %d\n", cpuid);

	fix_bsp_b0 = 0;
#endif
}

void
sync_master (void *arg)
{
	unsigned long flags, i;

	go[MASTER] = 0;

	local_irq_save(flags);
	{
		for (i = 0; i < NUM_ROUNDS*NUM_ITERS; ++i) {
			while (!go[MASTER])
				cpu_relax();
			go[MASTER] = 0;
			go[SLAVE] = ia64_get_itc();
		}
	}
	local_irq_restore(flags);
}

static inline long
get_delta (long *rt, long *master)
{
	unsigned long best_t0 = 0, best_t1 = ~0UL, best_tm = 0;
	unsigned long tcenter, t0, t1, tm;
	long i;

	for (i = 0; i < NUM_ITERS; ++i) {
		t0 = ia64_get_itc();
		go[MASTER] = 1;
		while (!(tm = go[SLAVE]))
			cpu_relax();
		go[SLAVE] = 0;
		t1 = ia64_get_itc();

		if (t1 - t0 < best_t1 - best_t0)
			best_t0 = t0, best_t1 = t1, best_tm = tm;
	}

	*rt = best_t1 - best_t0;
	*master = best_tm - best_t0;

	
	tcenter = (best_t0/2 + best_t1/2);
	if (best_t0 % 2 + best_t1 % 2 == 2)
		++tcenter;
	return tcenter - best_tm;
}

void
ia64_sync_itc (unsigned int master)
{
	long i, delta, adj, adjust_latency = 0, done = 0;
	unsigned long flags, rt, master_time_stamp, bound;
#if DEBUG_ITC_SYNC
	struct {
		long rt;	
		long master;	
		long diff;	
		long lat;	
	} t[NUM_ROUNDS];
#endif

	BUG_ON((ia64_get_itv() & (1 << 16)) == 0);

	go[MASTER] = 1;

	if (smp_call_function_single(master, sync_master, NULL, 0) < 0) {
		printk(KERN_ERR "sync_itc: failed to get attention of CPU %u!\n", master);
		return;
	}

	while (go[MASTER])
		cpu_relax();	

	spin_lock_irqsave(&itc_sync_lock, flags);
	{
		for (i = 0; i < NUM_ROUNDS; ++i) {
			delta = get_delta(&rt, &master_time_stamp);
			if (delta == 0) {
				done = 1;	
				bound = rt;
			}

			if (!done) {
				if (i > 0) {
					adjust_latency += -delta;
					adj = -delta + adjust_latency/4;
				} else
					adj = -delta;

				ia64_set_itc(ia64_get_itc() + adj);
			}
#if DEBUG_ITC_SYNC
			t[i].rt = rt;
			t[i].master = master_time_stamp;
			t[i].diff = delta;
			t[i].lat = adjust_latency/4;
#endif
		}
	}
	spin_unlock_irqrestore(&itc_sync_lock, flags);

#if DEBUG_ITC_SYNC
	for (i = 0; i < NUM_ROUNDS; ++i)
		printk("rt=%5ld master=%5ld diff=%5ld adjlat=%5ld\n",
		       t[i].rt, t[i].master, t[i].diff, t[i].lat);
#endif

	printk(KERN_INFO "CPU %d: synchronized ITC with CPU %u (last diff %ld cycles, "
	       "maxerr %lu cycles)\n", smp_processor_id(), master, delta, rt);
}

static inline void __devinit
smp_setup_percpu_timer (void)
{
}

static void __cpuinit
smp_callin (void)
{
	int cpuid, phys_id, itc_master;
	struct cpuinfo_ia64 *last_cpuinfo, *this_cpuinfo;
	extern void ia64_init_itm(void);
	extern volatile int time_keeper_id;

#ifdef CONFIG_PERFMON
	extern void pfm_init_percpu(void);
#endif

	cpuid = smp_processor_id();
	phys_id = hard_smp_processor_id();
	itc_master = time_keeper_id;

	if (cpu_online(cpuid)) {
		printk(KERN_ERR "huh, phys CPU#0x%x, CPU#0x%x already present??\n",
		       phys_id, cpuid);
		BUG();
	}

	fix_b0_for_bsp();

	set_numa_node(cpu_to_node_map[cpuid]);
	set_numa_mem(local_memory_node(cpu_to_node_map[cpuid]));

	ipi_call_lock_irq();
	spin_lock(&vector_lock);
	
	__setup_vector_irq(cpuid);
	notify_cpu_starting(cpuid);
	set_cpu_online(cpuid, true);
	per_cpu(cpu_state, cpuid) = CPU_ONLINE;
	spin_unlock(&vector_lock);
	ipi_call_unlock_irq();

	smp_setup_percpu_timer();

	ia64_mca_cmc_vector_setup();	

#ifdef CONFIG_PERFMON
	pfm_init_percpu();
#endif

	local_irq_enable();

	if (!(sal_platform_features & IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT)) {
		Dprintk("Going to syncup ITC with ITC Master.\n");
		ia64_sync_itc(itc_master);
	}

	ia64_init_itm();

	last_cpuinfo = cpu_data(cpuid - 1);
	this_cpuinfo = local_cpu_data;
	if (last_cpuinfo->itc_freq != this_cpuinfo->itc_freq ||
	    last_cpuinfo->proc_freq != this_cpuinfo->proc_freq ||
	    last_cpuinfo->features != this_cpuinfo->features ||
	    last_cpuinfo->revision != this_cpuinfo->revision ||
	    last_cpuinfo->family != this_cpuinfo->family ||
	    last_cpuinfo->archrev != this_cpuinfo->archrev ||
	    last_cpuinfo->model != this_cpuinfo->model)
		calibrate_delay();
	local_cpu_data->loops_per_jiffy = loops_per_jiffy;

	cpu_set(cpuid, cpu_callin_map);
	Dprintk("Stack on CPU %d at about %p\n",cpuid, &cpuid);
}


int __cpuinit
start_secondary (void *unused)
{
	
	ia64_set_kr(IA64_KR_IO_BASE, __pa(ia64_iobase));
#ifndef CONFIG_PRINTK_TIME
	Dprintk("start_secondary: starting CPU 0x%x\n", hard_smp_processor_id());
#endif
	efi_map_pal_code();
	cpu_init();
	preempt_disable();
	smp_callin();

	cpu_idle();
	return 0;
}

struct pt_regs * __cpuinit idle_regs(struct pt_regs *regs)
{
	return NULL;
}

struct create_idle {
	struct work_struct work;
	struct task_struct *idle;
	struct completion done;
	int cpu;
};

void __cpuinit
do_fork_idle(struct work_struct *work)
{
	struct create_idle *c_idle =
		container_of(work, struct create_idle, work);

	c_idle->idle = fork_idle(c_idle->cpu);
	complete(&c_idle->done);
}

static int __cpuinit
do_boot_cpu (int sapicid, int cpu)
{
	int timeout;
	struct create_idle c_idle = {
		.work = __WORK_INITIALIZER(c_idle.work, do_fork_idle),
		.cpu	= cpu,
		.done	= COMPLETION_INITIALIZER(c_idle.done),
	};

 	c_idle.idle = get_idle_for_cpu(cpu);
 	if (c_idle.idle) {
		init_idle(c_idle.idle, cpu);
 		goto do_rest;
	}

	schedule_work(&c_idle.work);
	wait_for_completion(&c_idle.done);

	if (IS_ERR(c_idle.idle))
		panic("failed fork for CPU %d", cpu);

	set_idle_for_cpu(cpu, c_idle.idle);

do_rest:
	task_for_booting_cpu = c_idle.idle;

	Dprintk("Sending wakeup vector %lu to AP 0x%x/0x%x.\n", ap_wakeup_vector, cpu, sapicid);

	set_brendez_area(cpu);
	platform_send_ipi(cpu, ap_wakeup_vector, IA64_IPI_DM_INT, 0);

	Dprintk("Waiting on callin_map ...");
	for (timeout = 0; timeout < 100000; timeout++) {
		if (cpu_isset(cpu, cpu_callin_map))
			break;  
		udelay(100);
	}
	Dprintk("\n");

	if (!cpu_isset(cpu, cpu_callin_map)) {
		printk(KERN_ERR "Processor 0x%x/0x%x is stuck.\n", cpu, sapicid);
		ia64_cpu_to_sapicid[cpu] = -1;
		set_cpu_online(cpu, false);  
		return -EINVAL;
	}
	return 0;
}

static int __init
decay (char *str)
{
	int ticks;
	get_option (&str, &ticks);
	return 1;
}

__setup("decay=", decay);

void __init
smp_build_cpu_map (void)
{
	int sapicid, cpu, i;
	int boot_cpu_id = hard_smp_processor_id();

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		ia64_cpu_to_sapicid[cpu] = -1;
	}

	ia64_cpu_to_sapicid[0] = boot_cpu_id;
	init_cpu_present(cpumask_of(0));
	set_cpu_possible(0, true);
	for (cpu = 1, i = 0; i < smp_boot_data.cpu_count; i++) {
		sapicid = smp_boot_data.cpu_phys_id[i];
		if (sapicid == boot_cpu_id)
			continue;
		set_cpu_present(cpu, true);
		set_cpu_possible(cpu, true);
		ia64_cpu_to_sapicid[cpu] = sapicid;
		cpu++;
	}
}

void __init
smp_prepare_cpus (unsigned int max_cpus)
{
	int boot_cpu_id = hard_smp_processor_id();


	smp_setup_percpu_timer();

	cpu_set(0, cpu_callin_map);

	local_cpu_data->loops_per_jiffy = loops_per_jiffy;
	ia64_cpu_to_sapicid[0] = boot_cpu_id;

	printk(KERN_INFO "Boot processor id 0x%x/0x%x\n", 0, boot_cpu_id);

	current_thread_info()->cpu = 0;

	if (!max_cpus) {
		printk(KERN_INFO "SMP mode deactivated.\n");
		init_cpu_online(cpumask_of(0));
		init_cpu_present(cpumask_of(0));
		init_cpu_possible(cpumask_of(0));
		return;
	}
}

void __devinit smp_prepare_boot_cpu(void)
{
	set_cpu_online(smp_processor_id(), true);
	cpu_set(smp_processor_id(), cpu_callin_map);
	set_numa_node(cpu_to_node_map[smp_processor_id()]);
	per_cpu(cpu_state, smp_processor_id()) = CPU_ONLINE;
	paravirt_post_smp_prepare_boot_cpu();
}

#ifdef CONFIG_HOTPLUG_CPU
static inline void
clear_cpu_sibling_map(int cpu)
{
	int i;

	for_each_cpu_mask(i, per_cpu(cpu_sibling_map, cpu))
		cpu_clear(cpu, per_cpu(cpu_sibling_map, i));
	for_each_cpu_mask(i, cpu_core_map[cpu])
		cpu_clear(cpu, cpu_core_map[i]);

	per_cpu(cpu_sibling_map, cpu) = cpu_core_map[cpu] = CPU_MASK_NONE;
}

static void
remove_siblinginfo(int cpu)
{
	int last = 0;

	if (cpu_data(cpu)->threads_per_core == 1 &&
	    cpu_data(cpu)->cores_per_socket == 1) {
		cpu_clear(cpu, cpu_core_map[cpu]);
		cpu_clear(cpu, per_cpu(cpu_sibling_map, cpu));
		return;
	}

	last = (cpus_weight(cpu_core_map[cpu]) == 1 ? 1 : 0);

	
	clear_cpu_sibling_map(cpu);
}

extern void fixup_irqs(void);

int migrate_platform_irqs(unsigned int cpu)
{
	int new_cpei_cpu;
	struct irq_data *data = NULL;
	const struct cpumask *mask;
	int 		retval = 0;

	if (cpe_vector > 0 && is_cpu_cpei_target(cpu)) {
		printk ("CPU (%d) is CPEI Target\n", cpu);
		if (can_cpei_retarget()) {
			new_cpei_cpu = cpumask_any(cpu_online_mask);
			mask = cpumask_of(new_cpei_cpu);
			set_cpei_target_cpu(new_cpei_cpu);
			data = irq_get_irq_data(ia64_cpe_irq);
			if (data && data->chip) {
				data->chip->irq_disable(data);
				data->chip->irq_set_affinity(data, mask, false);
				data->chip->irq_enable(data);
				printk ("Re-targeting CPEI to cpu %d\n", new_cpei_cpu);
			}
		}
		if (!data) {
			printk ("Unable to retarget CPEI, offline cpu [%d] failed\n", cpu);
			retval = -EBUSY;
		}
	}
	return retval;
}

int __cpu_disable(void)
{
	int cpu = smp_processor_id();

	if (cpu == 0 && !bsp_remove_ok) {
		printk ("Your platform does not support removal of BSP\n");
		return (-EBUSY);
	}

	if (ia64_platform_is("sn2")) {
		if (!sn_cpu_disable_allowed(cpu))
			return -EBUSY;
	}

	set_cpu_online(cpu, false);

	if (migrate_platform_irqs(cpu)) {
		set_cpu_online(cpu, true);
		return -EBUSY;
	}

	remove_siblinginfo(cpu);
	fixup_irqs();
	local_flush_tlb_all();
	cpu_clear(cpu, cpu_callin_map);
	return 0;
}

void __cpu_die(unsigned int cpu)
{
	unsigned int i;

	for (i = 0; i < 100; i++) {
		
		if (per_cpu(cpu_state, cpu) == CPU_DEAD)
		{
			printk ("CPU %d is now offline\n", cpu);
			return;
		}
		msleep(100);
	}
 	printk(KERN_ERR "CPU %u didn't die...\n", cpu);
}
#endif 

void
smp_cpus_done (unsigned int dummy)
{
	int cpu;
	unsigned long bogosum = 0;


	for_each_online_cpu(cpu) {
		bogosum += cpu_data(cpu)->loops_per_jiffy;
	}

	printk(KERN_INFO "Total of %d processors activated (%lu.%02lu BogoMIPS).\n",
	       (int)num_online_cpus(), bogosum/(500000/HZ), (bogosum/(5000/HZ))%100);
}

static inline void __devinit
set_cpu_sibling_map(int cpu)
{
	int i;

	for_each_online_cpu(i) {
		if ((cpu_data(cpu)->socket_id == cpu_data(i)->socket_id)) {
			cpu_set(i, cpu_core_map[cpu]);
			cpu_set(cpu, cpu_core_map[i]);
			if (cpu_data(cpu)->core_id == cpu_data(i)->core_id) {
				cpu_set(i, per_cpu(cpu_sibling_map, cpu));
				cpu_set(cpu, per_cpu(cpu_sibling_map, i));
			}
		}
	}
}

int __cpuinit
__cpu_up (unsigned int cpu)
{
	int ret;
	int sapicid;

	sapicid = ia64_cpu_to_sapicid[cpu];
	if (sapicid == -1)
		return -EINVAL;

	if (cpu_isset(cpu, cpu_callin_map))
		return -EINVAL;

	per_cpu(cpu_state, cpu) = CPU_UP_PREPARE;
	
	ret = do_boot_cpu(sapicid, cpu);
	if (ret < 0)
		return ret;

	if (cpu_data(cpu)->threads_per_core == 1 &&
	    cpu_data(cpu)->cores_per_socket == 1) {
		cpu_set(cpu, per_cpu(cpu_sibling_map, cpu));
		cpu_set(cpu, cpu_core_map[cpu]);
		return 0;
	}

	set_cpu_sibling_map(cpu);

	return 0;
}

void __init
init_smp_config(void)
{
	struct fptr {
		unsigned long fp;
		unsigned long gp;
	} *ap_startup;
	long sal_ret;

	
	ap_startup = (struct fptr *) start_ap;
	sal_ret = ia64_sal_set_vectors(SAL_VECTOR_OS_BOOT_RENDEZ,
				       ia64_tpa(ap_startup->fp), ia64_tpa(ap_startup->gp), 0, 0, 0, 0);
	if (sal_ret < 0)
		printk(KERN_ERR "SMP: Can't set SAL AP Boot Rendezvous: %s\n",
		       ia64_sal_strerror(sal_ret));
}

void __devinit
identify_siblings(struct cpuinfo_ia64 *c)
{
	long status;
	u16 pltid;
	pal_logical_to_physical_t info;

	status = ia64_pal_logical_to_phys(-1, &info);
	if (status != PAL_STATUS_SUCCESS) {
		if (status != PAL_STATUS_UNIMPLEMENTED) {
			printk(KERN_ERR
				"ia64_pal_logical_to_phys failed with %ld\n",
				status);
			return;
		}

		info.overview_ppid = 0;
		info.overview_cpp  = 1;
		info.overview_tpc  = 1;
	}

	status = ia64_sal_physical_id_info(&pltid);
	if (status != PAL_STATUS_SUCCESS) {
		if (status != PAL_STATUS_UNIMPLEMENTED)
			printk(KERN_ERR
				"ia64_sal_pltid failed with %ld\n",
				status);
		return;
	}

	c->socket_id =  (pltid << 8) | info.overview_ppid;

	if (info.overview_cpp == 1 && info.overview_tpc == 1)
		return;

	c->cores_per_socket = info.overview_cpp;
	c->threads_per_core = info.overview_tpc;
	c->num_log = info.overview_num_log;

	c->core_id = info.log1_cid;
	c->thread_id = info.log1_tid;
}

int is_multithreading_enabled(void)
{
	int i, j;

	for_each_present_cpu(i) {
		for_each_present_cpu(j) {
			if (j == i)
				continue;
			if ((cpu_data(j)->socket_id == cpu_data(i)->socket_id)) {
				if (cpu_data(j)->core_id == cpu_data(i)->core_id)
					return 1;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(is_multithreading_enabled);
