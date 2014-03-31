/*
 * SMP support for Hexagon
 *
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>

#include <asm/time.h>    
#include <asm/hexagon_vm.h>

#define BASE_IPI_IRQ 26


struct ipi_data {
	unsigned long bits;
};

static DEFINE_PER_CPU(struct ipi_data, ipi_data);

static inline void __handle_ipi(unsigned long *ops, struct ipi_data *ipi,
				int cpu)
{
	unsigned long msg = 0;
	do {
		msg = find_next_bit(ops, BITS_PER_LONG, msg+1);

		switch (msg) {

		case IPI_TIMER:
			ipi_timer();
			break;

		case IPI_CALL_FUNC:
			generic_smp_call_function_interrupt();
			break;

		case IPI_CALL_FUNC_SINGLE:
			generic_smp_call_function_single_interrupt();
			break;

		case IPI_CPU_STOP:
			__vmstop();
			break;

		case IPI_RESCHEDULE:
			scheduler_ipi();
			break;
		}
	} while (msg < BITS_PER_LONG);
}

void smp_vm_unmask_irq(void *info)
{
	__vmintop_locen((long) info);
}



irqreturn_t handle_ipi(int irq, void *desc)
{
	int cpu = smp_processor_id();
	struct ipi_data *ipi = &per_cpu(ipi_data, cpu);
	unsigned long ops;

	while ((ops = xchg(&ipi->bits, 0)) != 0)
		__handle_ipi(&ops, ipi, cpu);
	return IRQ_HANDLED;
}

void send_ipi(const struct cpumask *cpumask, enum ipi_message_type msg)
{
	unsigned long flags;
	unsigned long cpu;
	unsigned long retval;

	local_irq_save(flags);

	for_each_cpu(cpu, cpumask) {
		struct ipi_data *ipi = &per_cpu(ipi_data, cpu);

		set_bit(msg, &ipi->bits);
		
		retval = __vmintop_post(BASE_IPI_IRQ+cpu);

		if (retval != 0) {
			printk(KERN_ERR "interrupt %ld not configured?\n",
				BASE_IPI_IRQ+cpu);
		}
	}

	local_irq_restore(flags);
}

static struct irqaction ipi_intdesc = {
	.handler = handle_ipi,
	.flags = IRQF_TRIGGER_RISING,
	.name = "ipi_handler"
};

void __init smp_prepare_boot_cpu(void)
{
}


void __cpuinit start_secondary(void)
{
	unsigned int cpu;
	unsigned long thread_ptr;

	
	__asm__ __volatile__(
		"%0 = SP;\n"
		: "=r" (thread_ptr)
	);

	thread_ptr = thread_ptr & ~(THREAD_SIZE-1);

	__asm__ __volatile__(
		QUOTED_THREADINFO_REG " = %0;\n"
		:
		: "r" (thread_ptr)
	);

	
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	cpu = smp_processor_id();

	setup_irq(BASE_IPI_IRQ + cpu, &ipi_intdesc);

	
	setup_percpu_clockdev();

	printk(KERN_INFO "%s cpu %d\n", __func__, current_thread_info()->cpu);

	notify_cpu_starting(cpu);

	ipi_call_lock();
	set_cpu_online(cpu, true);
	ipi_call_unlock();

	local_irq_enable();

	cpu_idle();
}



int __cpuinit __cpu_up(unsigned int cpu)
{
	struct task_struct *idle;
	struct thread_info *thread;
	void *stack_start;

	
	idle = fork_idle(cpu);
	if (IS_ERR(idle))
		panic(KERN_ERR "fork_idle failed\n");

	thread = (struct thread_info *)idle->stack;
	thread->cpu = cpu;

	
	stack_start =  ((void *) thread) + THREAD_SIZE;
	__vmstart(start_secondary, stack_start);

	while (!cpu_online(cpu))
		barrier();

	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int i;


	
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	
	if (max_cpus > 1)
		setup_irq(BASE_IPI_IRQ, &ipi_intdesc);
}

void smp_send_reschedule(int cpu)
{
	send_ipi(cpumask_of(cpu), IPI_RESCHEDULE);
}

void smp_send_stop(void)
{
	struct cpumask targets;
	cpumask_copy(&targets, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &targets);
	send_ipi(&targets, IPI_CPU_STOP);
}

void arch_send_call_function_single_ipi(int cpu)
{
	send_ipi(cpumask_of(cpu), IPI_CALL_FUNC_SINGLE);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	send_ipi(mask, IPI_CALL_FUNC);
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

void smp_start_cpus(void)
{
	int i;

	for (i = 0; i < NR_CPUS; i++)
		set_cpu_possible(i, true);
}
