/* SMP support routines.
 *
 * Copyright (C) 2006-2008 Panasonic Corporation
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/profile.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <asm/tlbflush.h>
#include <asm/bitops.h>
#include <asm/processor.h>
#include <asm/bug.h>
#include <asm/exceptions.h>
#include <asm/hardirq.h>
#include <asm/fpu.h>
#include <asm/mmu_context.h>
#include <asm/thread_info.h>
#include <asm/cpu-regs.h>
#include <asm/intctl-regs.h>
#include "internal.h"

#ifdef CONFIG_HOTPLUG_CPU
#include <asm/cacheflush.h>

static unsigned long sleep_mode[NR_CPUS];

static void run_sleep_cpu(unsigned int cpu);
static void run_wakeup_cpu(unsigned int cpu);
#endif 


#undef DEBUG_SMP
#ifdef DEBUG_SMP
#define Dprintk(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define Dprintk(fmt, ...) no_printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#endif

#define	CALL_FUNCTION_NMI_IPI_TIMEOUT	0

struct nmi_call_data_struct {
	smp_call_func_t	func;
	void		*info;
	cpumask_t	started;
	cpumask_t	finished;
	int		wait;
	char		size_alignment[0]
	__attribute__ ((__aligned__(SMP_CACHE_BYTES)));
} __attribute__ ((__aligned__(SMP_CACHE_BYTES)));

static DEFINE_SPINLOCK(smp_nmi_call_lock);
static struct nmi_call_data_struct *nmi_call_data;

static cpumask_t cpu_callin_map;	
static cpumask_t cpu_callout_map;	
cpumask_t cpu_boot_map;			
unsigned long start_stack[NR_CPUS - 1];

struct mn10300_cpuinfo cpu_data[NR_CPUS] __cacheline_aligned;

static int cpucount;			
static cpumask_t smp_commenced_mask;
cpumask_t cpu_initialized __initdata = CPU_MASK_NONE;

static int do_boot_cpu(int);
static void smp_show_cpu_info(int cpu_id);
static void smp_callin(void);
static void smp_online(void);
static void smp_store_cpu_info(int);
static void smp_cpu_init(void);
static void smp_tune_scheduling(void);
static void send_IPI_mask(const cpumask_t *cpumask, int irq);
static void init_ipi(void);

static void mn10300_ipi_disable(unsigned int irq);
static void mn10300_ipi_enable(unsigned int irq);
static void mn10300_ipi_chip_disable(struct irq_data *d);
static void mn10300_ipi_chip_enable(struct irq_data *d);
static void mn10300_ipi_ack(struct irq_data *d);
static void mn10300_ipi_nop(struct irq_data *d);

static struct irq_chip mn10300_ipi_type = {
	.name		= "cpu_ipi",
	.irq_disable	= mn10300_ipi_chip_disable,
	.irq_enable	= mn10300_ipi_chip_enable,
	.irq_ack	= mn10300_ipi_ack,
	.irq_eoi	= mn10300_ipi_nop
};

static irqreturn_t smp_reschedule_interrupt(int irq, void *dev_id);
static irqreturn_t smp_call_function_interrupt(int irq, void *dev_id);

static struct irqaction reschedule_ipi = {
	.handler	= smp_reschedule_interrupt,
	.name		= "smp reschedule IPI"
};
static struct irqaction call_function_ipi = {
	.handler	= smp_call_function_interrupt,
	.name		= "smp call function IPI"
};

#if !defined(CONFIG_GENERIC_CLOCKEVENTS) || defined(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST)
static irqreturn_t smp_ipi_timer_interrupt(int irq, void *dev_id);
static struct irqaction local_timer_ipi = {
	.handler	= smp_ipi_timer_interrupt,
	.flags		= IRQF_DISABLED,
	.name		= "smp local timer IPI"
};
#endif

static void init_ipi(void)
{
	unsigned long flags;
	u16 tmp16;

	
	irq_set_chip_and_handler(RESCHEDULE_IPI, &mn10300_ipi_type,
				 handle_percpu_irq);
	setup_irq(RESCHEDULE_IPI, &reschedule_ipi);
	set_intr_level(RESCHEDULE_IPI, RESCHEDULE_GxICR_LV);
	mn10300_ipi_enable(RESCHEDULE_IPI);

	
	irq_set_chip_and_handler(CALL_FUNC_SINGLE_IPI, &mn10300_ipi_type,
				 handle_percpu_irq);
	setup_irq(CALL_FUNC_SINGLE_IPI, &call_function_ipi);
	set_intr_level(CALL_FUNC_SINGLE_IPI, CALL_FUNCTION_GxICR_LV);
	mn10300_ipi_enable(CALL_FUNC_SINGLE_IPI);

	
#if !defined(CONFIG_GENERIC_CLOCKEVENTS) || \
    defined(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST)
	irq_set_chip_and_handler(LOCAL_TIMER_IPI, &mn10300_ipi_type,
				 handle_percpu_irq);
	setup_irq(LOCAL_TIMER_IPI, &local_timer_ipi);
	set_intr_level(LOCAL_TIMER_IPI, LOCAL_TIMER_GxICR_LV);
	mn10300_ipi_enable(LOCAL_TIMER_IPI);
#endif

#ifdef CONFIG_MN10300_CACHE_ENABLED
	
	flags = arch_local_cli_save();
	__set_intr_stub(NUM2EXCEP_IRQ_LEVEL(FLUSH_CACHE_GxICR_LV),
			mn10300_low_ipi_handler);
	GxICR(FLUSH_CACHE_IPI) = FLUSH_CACHE_GxICR_LV | GxICR_DETECT;
	mn10300_ipi_enable(FLUSH_CACHE_IPI);
	arch_local_irq_restore(flags);
#endif

	
	flags = arch_local_cli_save();
	GxICR(CALL_FUNCTION_NMI_IPI) = GxICR_NMI | GxICR_ENABLE | GxICR_DETECT;
	tmp16 = GxICR(CALL_FUNCTION_NMI_IPI);
	arch_local_irq_restore(flags);

	
	flags = arch_local_cli_save();
	__set_intr_stub(NUM2EXCEP_IRQ_LEVEL(SMP_BOOT_GxICR_LV),
			mn10300_low_ipi_handler);
	arch_local_irq_restore(flags);
}

static void mn10300_ipi_shutdown(unsigned int irq)
{
	unsigned long flags;
	u16 tmp;

	flags = arch_local_cli_save();

	tmp = GxICR(irq);
	GxICR(irq) = (tmp & GxICR_LEVEL) | GxICR_DETECT;
	tmp = GxICR(irq);

	arch_local_irq_restore(flags);
}

static void mn10300_ipi_enable(unsigned int irq)
{
	unsigned long flags;
	u16 tmp;

	flags = arch_local_cli_save();

	tmp = GxICR(irq);
	GxICR(irq) = (tmp & GxICR_LEVEL) | GxICR_ENABLE;
	tmp = GxICR(irq);

	arch_local_irq_restore(flags);
}

static void mn10300_ipi_chip_enable(struct irq_data *d)
{
	mn10300_ipi_enable(d->irq);
}

static void mn10300_ipi_disable(unsigned int irq)
{
	unsigned long flags;
	u16 tmp;

	flags = arch_local_cli_save();

	tmp = GxICR(irq);
	GxICR(irq) = tmp & GxICR_LEVEL;
	tmp = GxICR(irq);

	arch_local_irq_restore(flags);
}

static void mn10300_ipi_chip_disable(struct irq_data *d)
{
	mn10300_ipi_disable(d->irq);
}


static void mn10300_ipi_ack(struct irq_data *d)
{
	unsigned int irq = d->irq;
	unsigned long flags;
	u16 tmp;

	flags = arch_local_cli_save();
	GxICR_u8(irq) = GxICR_DETECT;
	tmp = GxICR(irq);
	arch_local_irq_restore(flags);
}

static void mn10300_ipi_nop(struct irq_data *d)
{
}

static void send_IPI_mask(const cpumask_t *cpumask, int irq)
{
	int i;
	u16 tmp;

	for (i = 0; i < NR_CPUS; i++) {
		if (cpumask_test_cpu(i, cpumask)) {
			
			tmp = CROSS_GxICR(irq, i);
			CROSS_GxICR(irq, i) =
				tmp | GxICR_REQUEST | GxICR_DETECT;
			tmp = CROSS_GxICR(irq, i); 
		}
	}
}

void send_IPI_self(int irq)
{
	send_IPI_mask(cpumask_of(smp_processor_id()), irq);
}

void send_IPI_allbutself(int irq)
{
	cpumask_t cpumask;

	cpumask_copy(&cpumask, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &cpumask);
	send_IPI_mask(&cpumask, irq);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	BUG();
	
}

void arch_send_call_function_single_ipi(int cpu)
{
	send_IPI_mask(cpumask_of(cpu), CALL_FUNC_SINGLE_IPI);
}

void smp_send_reschedule(int cpu)
{
	send_IPI_mask(cpumask_of(cpu), RESCHEDULE_IPI);
}

int smp_nmi_call_function(smp_call_func_t func, void *info, int wait)
{
	struct nmi_call_data_struct data;
	unsigned long flags;
	unsigned int cnt;
	int cpus, ret = 0;

	cpus = num_online_cpus() - 1;
	if (cpus < 1)
		return 0;

	data.func = func;
	data.info = info;
	cpumask_copy(&data.started, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &data.started);
	data.wait = wait;
	if (wait)
		data.finished = data.started;

	spin_lock_irqsave(&smp_nmi_call_lock, flags);
	nmi_call_data = &data;
	smp_mb();

	
	send_IPI_allbutself(CALL_FUNCTION_NMI_IPI);

	
	if (CALL_FUNCTION_NMI_IPI_TIMEOUT > 0) {
		for (cnt = 0;
		     cnt < CALL_FUNCTION_NMI_IPI_TIMEOUT &&
			     !cpumask_empty(&data.started);
		     cnt++)
			mdelay(1);

		if (wait && cnt < CALL_FUNCTION_NMI_IPI_TIMEOUT) {
			for (cnt = 0;
			     cnt < CALL_FUNCTION_NMI_IPI_TIMEOUT &&
				     !cpumask_empty(&data.finished);
			     cnt++)
				mdelay(1);
		}

		if (cnt >= CALL_FUNCTION_NMI_IPI_TIMEOUT)
			ret = -ETIMEDOUT;

	} else {
		while (!cpumask_empty(&data.started))
			barrier();
		if (wait)
			while (!cpumask_empty(&data.finished))
				barrier();
	}

	spin_unlock_irqrestore(&smp_nmi_call_lock, flags);
	return ret;
}

void smp_jump_to_debugger(void)
{
	if (num_online_cpus() > 1)
		
		send_IPI_allbutself(DEBUGGER_NMI_IPI);
}

void stop_this_cpu(void *unused)
{
	static volatile int stopflag;
	unsigned long flags;

#ifdef CONFIG_GDBSTUB
	atomic_set(&procindebug[smp_processor_id()], 0);
#endif	

	flags = arch_local_cli_save();
	set_cpu_online(smp_processor_id(), false);

	while (!stopflag)
		cpu_relax();

	set_cpu_online(smp_processor_id(), true);
	arch_local_irq_restore(flags);
}

void smp_send_stop(void)
{
	smp_nmi_call_function(stop_this_cpu, NULL, 0);
}

static irqreturn_t smp_reschedule_interrupt(int irq, void *dev_id)
{
	scheduler_ipi();
	return IRQ_HANDLED;
}

static irqreturn_t smp_call_function_interrupt(int irq, void *dev_id)
{
	
	generic_smp_call_function_single_interrupt();
	return IRQ_HANDLED;
}

void smp_nmi_call_function_interrupt(void)
{
	smp_call_func_t func = nmi_call_data->func;
	void *info = nmi_call_data->info;
	int wait = nmi_call_data->wait;

	smp_mb();
	cpumask_clear_cpu(smp_processor_id(), &nmi_call_data->started);
	(*func)(info);

	if (wait) {
		smp_mb();
		cpumask_clear_cpu(smp_processor_id(),
				  &nmi_call_data->finished);
	}
}

#if !defined(CONFIG_GENERIC_CLOCKEVENTS) || \
    defined(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST)
static irqreturn_t smp_ipi_timer_interrupt(int irq, void *dev_id)
{
	return local_timer_interrupt();
}
#endif

void __init smp_init_cpus(void)
{
	int i;
	for (i = 0; i < NR_CPUS; i++) {
		set_cpu_possible(i, true);
		set_cpu_present(i, true);
	}
}

static void __init smp_cpu_init(void)
{
	unsigned long flags;
	int cpu_id = smp_processor_id();
	u16 tmp16;

	if (test_and_set_bit(cpu_id, &cpu_initialized)) {
		printk(KERN_WARNING "CPU#%d already initialized!\n", cpu_id);
		for (;;)
			local_irq_enable();
	}
	printk(KERN_INFO "Initializing CPU#%d\n", cpu_id);

	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	BUG_ON(current->mm);

	enter_lazy_tlb(&init_mm, current);

	
	clear_using_fpu(current);

	GxICR(CALL_FUNC_SINGLE_IPI) = CALL_FUNCTION_GxICR_LV | GxICR_DETECT;
	mn10300_ipi_enable(CALL_FUNC_SINGLE_IPI);

	GxICR(LOCAL_TIMER_IPI) = LOCAL_TIMER_GxICR_LV | GxICR_DETECT;
	mn10300_ipi_enable(LOCAL_TIMER_IPI);

	GxICR(RESCHEDULE_IPI) = RESCHEDULE_GxICR_LV | GxICR_DETECT;
	mn10300_ipi_enable(RESCHEDULE_IPI);

#ifdef CONFIG_MN10300_CACHE_ENABLED
	GxICR(FLUSH_CACHE_IPI) = FLUSH_CACHE_GxICR_LV | GxICR_DETECT;
	mn10300_ipi_enable(FLUSH_CACHE_IPI);
#endif

	mn10300_ipi_shutdown(SMP_BOOT_IRQ);

	
	flags = arch_local_cli_save();
	GxICR(CALL_FUNCTION_NMI_IPI) = GxICR_NMI | GxICR_ENABLE | GxICR_DETECT;
	tmp16 = GxICR(CALL_FUNCTION_NMI_IPI);
	arch_local_irq_restore(flags);
}

void smp_prepare_cpu_init(void)
{
	int loop;

	
	IVAR0 = EXCEP_IRQ_LEVEL0;
	IVAR1 = EXCEP_IRQ_LEVEL1;
	IVAR2 = EXCEP_IRQ_LEVEL2;
	IVAR3 = EXCEP_IRQ_LEVEL3;
	IVAR4 = EXCEP_IRQ_LEVEL4;
	IVAR5 = EXCEP_IRQ_LEVEL5;
	IVAR6 = EXCEP_IRQ_LEVEL6;

	
	for (loop = 0; loop < GxICR_NUM_IRQS; loop++)
		GxICR(loop) = GxICR_LEVEL_6 | GxICR_DETECT;

#ifdef CONFIG_KERNEL_DEBUGGER
	
	do {
		unsigned long flags;
		u16 tmp16;

		flags = arch_local_cli_save();
		GxICR(DEBUGGER_NMI_IPI) = GxICR_NMI | GxICR_ENABLE | GxICR_DETECT;
		tmp16 = GxICR(DEBUGGER_NMI_IPI);
		arch_local_irq_restore(flags);
	} while (0);
#endif
}

int __init start_secondary(void *unused)
{
	smp_cpu_init();
	smp_callin();
	while (!cpumask_test_cpu(smp_processor_id(), &smp_commenced_mask))
		cpu_relax();

	local_flush_tlb();
	preempt_disable();
	smp_online();

#ifdef CONFIG_GENERIC_CLOCKEVENTS
	init_clockevents();
#endif
	cpu_idle();
	return 0;
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int phy_id;

	
	smp_store_cpu_info(0);
	smp_tune_scheduling();

	init_ipi();

	
	if (max_cpus == 0) {
		printk(KERN_INFO "SMP mode deactivated.\n");
		goto smp_done;
	}

	
	for (phy_id = 0; phy_id < NR_CPUS; phy_id++) {
		
		if (max_cpus <= cpucount + 1)
			continue;
		if (phy_id != 0)
			do_boot_cpu(phy_id);
		set_cpu_possible(phy_id, true);
		smp_show_cpu_info(phy_id);
	}

smp_done:
	Dprintk("Boot done.\n");
}

static void __init smp_store_cpu_info(int cpu)
{
	struct mn10300_cpuinfo *ci = &cpu_data[cpu];

	*ci = boot_cpu_data;
	ci->loops_per_jiffy = loops_per_jiffy;
	ci->type = CPUREV;
}

static void __init smp_tune_scheduling(void)
{
}

static int __init do_boot_cpu(int phy_id)
{
	struct task_struct *idle;
	unsigned long send_status, callin_status;
	int timeout, cpu_id;

	send_status = GxICR_REQUEST;
	callin_status = 0;
	timeout = 0;
	cpu_id = phy_id;

	cpucount++;

	
	idle = fork_idle(cpu_id);
	if (IS_ERR(idle))
		panic("Failed fork for CPU#%d.", cpu_id);

	idle->thread.pc = (unsigned long)start_secondary;

	printk(KERN_NOTICE "Booting CPU#%d\n", cpu_id);
	start_stack[cpu_id - 1] = idle->thread.sp;

	task_thread_info(idle)->cpu = cpu_id;

	
	send_IPI_mask(cpumask_of(phy_id), SMP_BOOT_IRQ);

	Dprintk("Waiting for send to finish...\n");

	
	do {
		udelay(1000);
		send_status =
			CROSS_GxICR(SMP_BOOT_IRQ, phy_id) & GxICR_REQUEST;
	} while (send_status == GxICR_REQUEST && timeout++ < 100);

	Dprintk("Waiting for cpu_callin_map.\n");

	if (send_status == 0) {
		
		cpumask_set_cpu(cpu_id, &cpu_callout_map);

		
		timeout = 0;
		do {
			udelay(1000);
			callin_status = cpumask_test_cpu(cpu_id,
							 &cpu_callin_map);
		} while (callin_status == 0 && timeout++ < 5000);

		if (callin_status == 0)
			Dprintk("Not responding.\n");
	} else {
		printk(KERN_WARNING "IPI not delivered.\n");
	}

	if (send_status == GxICR_REQUEST || callin_status == 0) {
		cpumask_clear_cpu(cpu_id, &cpu_callout_map);
		cpumask_clear_cpu(cpu_id, &cpu_callin_map);
		cpumask_clear_cpu(cpu_id, &cpu_initialized);
		cpucount--;
		return 1;
	}
	return 0;
}

static void __init smp_show_cpu_info(int cpu)
{
	struct mn10300_cpuinfo *ci = &cpu_data[cpu];

	printk(KERN_INFO
	       "CPU#%d : ioclk speed: %lu.%02luMHz : bogomips : %lu.%02lu\n",
	       cpu,
	       MN10300_IOCLK / 1000000,
	       (MN10300_IOCLK / 10000) % 100,
	       ci->loops_per_jiffy / (500000 / HZ),
	       (ci->loops_per_jiffy / (5000 / HZ)) % 100);
}

static void __init smp_callin(void)
{
	unsigned long timeout;
	int cpu;

	cpu = smp_processor_id();
	timeout = jiffies + (2 * HZ);

	if (cpumask_test_cpu(cpu, &cpu_callin_map)) {
		printk(KERN_ERR "CPU#%d already present.\n", cpu);
		BUG();
	}
	Dprintk("CPU#%d waiting for CALLOUT\n", cpu);

	
	while (time_before(jiffies, timeout)) {
		if (cpumask_test_cpu(cpu, &cpu_callout_map))
			break;
		cpu_relax();
	}

	if (!time_before(jiffies, timeout)) {
		printk(KERN_ERR
		       "BUG: CPU#%d started up but did not get a callout!\n",
		       cpu);
		BUG();
	}

#ifdef CONFIG_CALIBRATE_DELAY
	calibrate_delay();		
#endif

	
	smp_store_cpu_info(cpu);

	
	cpumask_set_cpu(cpu, &cpu_callin_map);
}

static void __init smp_online(void)
{
	int cpu;

	cpu = smp_processor_id();

	notify_cpu_starting(cpu);

	ipi_call_lock();
	set_cpu_online(cpu, true);
	ipi_call_unlock();

	local_irq_enable();
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

void __devinit smp_prepare_boot_cpu(void)
{
	cpumask_set_cpu(0, &cpu_callout_map);
	cpumask_set_cpu(0, &cpu_callin_map);
	current_thread_info()->cpu = 0;
}

void initialize_secondary(void)
{
	asm volatile (
		"mov	%0,sp	\n"
		"jmp	(%1)	\n"
		:
		: "a"(current->thread.sp), "a"(current->thread.pc));
}

int __devinit __cpu_up(unsigned int cpu)
{
	int timeout;

#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() == 1)
		disable_hlt();
	if (sleep_mode[cpu])
		run_wakeup_cpu(cpu);
#endif 

	cpumask_set_cpu(cpu, &smp_commenced_mask);

	
	for (timeout = 0 ; timeout < 5000 ; timeout++) {
		if (cpu_online(cpu))
			break;
		udelay(1000);
	}

	BUG_ON(!cpu_online(cpu));
	return 0;
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

#ifdef CONFIG_HOTPLUG_CPU

static DEFINE_PER_CPU(struct cpu, cpu_devices);

static int __init topology_init(void)
{
	int cpu, ret;

	for_each_cpu(cpu) {
		ret = register_cpu(&per_cpu(cpu_devices, cpu), cpu, NULL);
		if (ret)
			printk(KERN_WARNING
			       "topology_init: register_cpu %d failed (%d)\n",
			       cpu, ret);
	}
	return 0;
}

subsys_initcall(topology_init);

int __cpu_disable(void)
{
	int cpu = smp_processor_id();
	if (cpu == 0)
		return -EBUSY;

	migrate_irqs();
	cpumask_clear_cpu(cpu, &mm_cpumask(current->active_mm));
	return 0;
}

void __cpu_die(unsigned int cpu)
{
	run_sleep_cpu(cpu);

	if (num_online_cpus() == 1)
		enable_hlt();
}

#ifdef CONFIG_MN10300_CACHE_ENABLED
static inline void hotplug_cpu_disable_cache(void)
{
	int tmp;
	asm volatile(
		"	movhu	(%1),%0	\n"
		"	and	%2,%0	\n"
		"	movhu	%0,(%1)	\n"
		"1:	movhu	(%1),%0	\n"
		"	btst	%3,%0	\n"
		"	bne	1b	\n"
		: "=&r"(tmp)
		: "a"(&CHCTR),
		  "i"(~(CHCTR_ICEN | CHCTR_DCEN)),
		  "i"(CHCTR_ICBUSY | CHCTR_DCBUSY)
		: "memory", "cc");
}

static inline void hotplug_cpu_enable_cache(void)
{
	int tmp;
	asm volatile(
		"movhu	(%1),%0	\n"
		"or	%2,%0	\n"
		"movhu	%0,(%1)	\n"
		: "=&r"(tmp)
		: "a"(&CHCTR),
		  "i"(CHCTR_ICEN | CHCTR_DCEN)
		: "memory", "cc");
}

static inline void hotplug_cpu_invalidate_cache(void)
{
	int tmp;
	asm volatile (
		"movhu	(%1),%0	\n"
		"or	%2,%0	\n"
		"movhu	%0,(%1)	\n"
		: "=&r"(tmp)
		: "a"(&CHCTR),
		  "i"(CHCTR_ICINV | CHCTR_DCINV)
		: "cc");
}

#else 
#define hotplug_cpu_disable_cache()	do {} while (0)
#define hotplug_cpu_enable_cache()	do {} while (0)
#define hotplug_cpu_invalidate_cache()	do {} while (0)
#endif 

static int hotplug_cpu_nmi_call_function(cpumask_t cpumask,
					 smp_call_func_t func, void *info,
					 int wait)
{
	static struct nmi_call_data_struct nmi_call_func_mask_data
		__cacheline_aligned;
	unsigned long start, end;

	start = (unsigned long)&nmi_call_func_mask_data;
	end = start + sizeof(struct nmi_call_data_struct);

	nmi_call_func_mask_data.func = func;
	nmi_call_func_mask_data.info = info;
	nmi_call_func_mask_data.started = cpumask;
	nmi_call_func_mask_data.wait = wait;
	if (wait)
		nmi_call_func_mask_data.finished = cpumask;

	spin_lock(&smp_nmi_call_lock);
	nmi_call_data = &nmi_call_func_mask_data;
	mn10300_local_dcache_flush_range(start, end);
	smp_wmb();

	send_IPI_mask(cpumask, CALL_FUNCTION_NMI_IPI);

	do {
		mn10300_local_dcache_inv_range(start, end);
		barrier();
	} while (!cpumask_empty(&nmi_call_func_mask_data.started));

	if (wait) {
		do {
			mn10300_local_dcache_inv_range(start, end);
			barrier();
		} while (!cpumask_empty(&nmi_call_func_mask_data.finished));
	}

	spin_unlock(&smp_nmi_call_lock);
	return 0;
}

static void restart_wakeup_cpu(void)
{
	unsigned int cpu = smp_processor_id();

	cpumask_set_cpu(cpu, &cpu_callin_map);
	local_flush_tlb();
	set_cpu_online(cpu, true);
	smp_wmb();
}

static void prepare_sleep_cpu(void *unused)
{
	sleep_mode[smp_processor_id()] = 1;
	smp_mb();
	mn10300_local_dcache_flush_inv();
	hotplug_cpu_disable_cache();
	hotplug_cpu_invalidate_cache();
}

static void sleep_cpu(void *unused)
{
	unsigned int cpu_id = smp_processor_id();
	do {
		smp_mb();
		__sleep_cpu();
	} while (sleep_mode[cpu_id]);
	restart_wakeup_cpu();
}

static void run_sleep_cpu(unsigned int cpu)
{
	unsigned long flags;
	cpumask_t cpumask;

	cpumask_copy(&cpumask, &cpumask_of(cpu));
	flags = arch_local_cli_save();
	hotplug_cpu_nmi_call_function(cpumask, prepare_sleep_cpu, NULL, 1);
	hotplug_cpu_nmi_call_function(cpumask, sleep_cpu, NULL, 0);
	udelay(1);		
	arch_local_irq_restore(flags);
}

static void wakeup_cpu(void)
{
	hotplug_cpu_invalidate_cache();
	hotplug_cpu_enable_cache();
	smp_mb();
	sleep_mode[smp_processor_id()] = 0;
}

static void run_wakeup_cpu(unsigned int cpu)
{
	unsigned long flags;

	flags = arch_local_cli_save();
#if NR_CPUS == 2
	mn10300_local_dcache_flush_inv();
#else
#error not support NR_CPUS > 2, when CONFIG_HOTPLUG_CPU=y.
#endif
	hotplug_cpu_nmi_call_function(cpumask_of(cpu), wakeup_cpu, NULL, 1);
	arch_local_irq_restore(flags);
}

#endif 
