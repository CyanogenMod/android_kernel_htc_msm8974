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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/bootmem.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>

static DEFINE_PER_CPU(int, cpu_state) = { 0 };

unsigned long start_cpu_function_addr;

void __init smp_prepare_boot_cpu(void)
{
	int cpu = smp_processor_id();
	set_cpu_online(cpu, 1);
	set_cpu_present(cpu, 1);
	__get_cpu_var(cpu_state) = CPU_ONLINE;

	init_messaging();
}

static void start_secondary(void);

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	long rc;
	int cpu, cpu_count;
	int boot_cpu = smp_processor_id();

	current_thread_info()->cpu = boot_cpu;

	rc = sched_setaffinity(current->pid, cpumask_of(boot_cpu));
	if (rc != 0)
		pr_err("Couldn't set init affinity to boot cpu (%ld)\n", rc);

	
	print_disabled_cpus();

	start_cpu_function_addr = (unsigned long) &online_secondary;

	
	cpu_count = 1;
	for (cpu = 0; cpu < NR_CPUS; ++cpu)	{
		struct task_struct *idle;

		if (cpu == boot_cpu)
			continue;

		if (!cpu_possible(cpu)) {
			per_cpu(boot_sp, cpu) = 0;
			per_cpu(boot_pc, cpu) = (unsigned long) smp_nap;
			continue;
		}

		
		idle = fork_idle(cpu);
		if (IS_ERR(idle))
			panic("failed fork for CPU %d", cpu);
		idle->thread.pc = (unsigned long) start_secondary;

		
		per_cpu(boot_sp, cpu) = task_ksp0(idle);
		per_cpu(boot_pc, cpu) = idle->thread.pc;

		++cpu_count;
	}
	BUG_ON(cpu_count > (max_cpus ? max_cpus : 1));

	
	init_cpu_present(cpu_possible_mask);
	if (cpumask_weight(cpu_present_mask) > 1) {
		mb();  
		hv_start_all_tiles();
	}
}

static __initdata struct cpumask init_affinity;

static __init int reset_init_affinity(void)
{
	long rc = sched_setaffinity(current->pid, &init_affinity);
	if (rc != 0)
		pr_warning("couldn't reset init affinity (%ld)\n",
		       rc);
	return 0;
}
late_initcall(reset_init_affinity);

static struct cpumask cpu_started __cpuinitdata;

static void __cpuinit start_secondary(void)
{
	int cpuid = smp_processor_id();

	
	set_my_cpu_offset(__per_cpu_offset[cpuid]);

	preempt_disable();

	

	
	__get_cpu_var(current_asid) = min_asid;

	
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	if (current->mm)
		BUG();
	enter_lazy_tlb(&init_mm, current);

	
	init_messaging();
	local_irq_enable();

	
	
	if (cpumask_test_and_set_cpu(cpuid, &cpu_started)) {
		pr_warning("CPU#%d already started!\n", cpuid);
		for (;;)
			local_irq_enable();
	}

	smp_nap();
}

void __cpuinit online_secondary(void)
{
	local_flush_tlb();

	BUG_ON(in_interrupt());

	
	wmb();

	notify_cpu_starting(smp_processor_id());

	ipi_call_lock();
	set_cpu_online(smp_processor_id(), 1);
	ipi_call_unlock();
	__get_cpu_var(cpu_state) = CPU_ONLINE;

	
	setup_cpu(0);

	
	setup_tile_timer();

	preempt_enable();

	cpu_idle();
}

int __cpuinit __cpu_up(unsigned int cpu)
{
	
	static int timeout;
	for (; !cpumask_test_cpu(cpu, &cpu_started); timeout++) {
		if (timeout >= 50000) {
			pr_info("skipping unresponsive cpu%d\n", cpu);
			local_irq_enable();
			return -EIO;
		}
		udelay(100);
	}

	local_irq_enable();
	per_cpu(cpu_state, cpu) = CPU_UP_PREPARE;

	
	send_IPI_single(cpu, MSG_TAG_START_CPU);
	while (!cpumask_test_cpu(cpu, cpu_online_mask))
		cpu_relax();
	return 0;
}

static void panic_start_cpu(void)
{
	panic("Received a MSG_START_CPU IPI after boot finished.");
}

void __init smp_cpus_done(unsigned int max_cpus)
{
	int cpu, next, rc;

	
	start_cpu_function_addr = (unsigned long) &panic_start_cpu;

	cpumask_copy(&init_affinity, cpu_online_mask);

	for (cpu = cpumask_first(&init_affinity);
	     (next = cpumask_next(cpu, &init_affinity)) < nr_cpu_ids;
	     cpu = next)
		;
	rc = sched_setaffinity(current->pid, cpumask_of(cpu));
	if (rc != 0)
		pr_err("Couldn't set init affinity to cpu %d (%d)\n", cpu, rc);
}
