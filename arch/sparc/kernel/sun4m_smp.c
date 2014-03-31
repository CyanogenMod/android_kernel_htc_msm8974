/*
 *  sun4m SMP support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/delay.h>
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/switch_to.h>
#include <asm/tlbflush.h>

#include "irq.h"
#include "kernel.h"

#define IRQ_IPI_SINGLE		12
#define IRQ_IPI_MASK		13
#define IRQ_IPI_RESCHED		14
#define IRQ_CROSS_CALL		15

static inline unsigned long
swap_ulong(volatile unsigned long *ptr, unsigned long val)
{
	__asm__ __volatile__("swap [%1], %0\n\t" :
			     "=&r" (val), "=&r" (ptr) :
			     "0" (val), "1" (ptr));
	return val;
}

static void smp4m_ipi_init(void);
static void smp_setup_percpu_timer(void);

void __cpuinit smp4m_callin(void)
{
	int cpuid = hard_smp_processor_id();

	local_flush_cache_all();
	local_flush_tlb_all();

	notify_cpu_starting(cpuid);

	
	smp_setup_percpu_timer();

	calibrate_delay();
	smp_store_cpu_info(cpuid);

	local_flush_cache_all();
	local_flush_tlb_all();

	
	swap_ulong(&cpu_callin_map[cpuid], 1);

	
	local_flush_cache_all();
	local_flush_tlb_all();

	
	__asm__ __volatile__("ld [%0], %%g6\n\t"
			     : : "r" (&current_set[cpuid])
			     : "memory" );

	
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	while (!cpumask_test_cpu(cpuid, &smp_commenced_mask))
		mb();

	local_irq_enable();

	set_cpu_online(cpuid, true);
}

void __init smp4m_boot_cpus(void)
{
	smp4m_ipi_init();
	smp_setup_percpu_timer();
	local_flush_cache_all();
}

int __cpuinit smp4m_boot_one_cpu(int i)
{
	unsigned long *entry = &sun4m_cpu_startup;
	struct task_struct *p;
	int timeout;
	int cpu_node;

	cpu_find_by_mid(i, &cpu_node);

	
	p = fork_idle(i);
	current_set[i] = task_thread_info(p);
	
	entry += ((i - 1) * 3);

	smp_penguin_ctable.which_io = 0;
	smp_penguin_ctable.phys_addr = (unsigned int) srmmu_ctx_table_phys;
	smp_penguin_ctable.reg_size = 0;

	
	printk(KERN_INFO "Starting CPU %d at %p\n", i, entry);
	local_flush_cache_all();
	prom_startcpu(cpu_node, &smp_penguin_ctable, 0, (char *)entry);

	
	for (timeout = 0; timeout < 10000; timeout++) {
		if (cpu_callin_map[i])
			break;
		udelay(200);
	}

	if (!(cpu_callin_map[i])) {
		printk(KERN_ERR "Processor %d is stuck.\n", i);
		return -ENODEV;
	}

	local_flush_cache_all();
	return 0;
}

void __init smp4m_smp_done(void)
{
	int i, first;
	int *prev;

	
	first = 0;
	prev = &first;
	for_each_online_cpu(i) {
		*prev = i;
		prev = &cpu_data(i).next;
	}
	*prev = first;
	local_flush_cache_all();

	
}


static void __init smp4m_ipi_init(void)
{
}

static void smp4m_ipi_resched(int cpu)
{
	set_cpu_int(cpu, IRQ_IPI_RESCHED);
}

static void smp4m_ipi_single(int cpu)
{
	set_cpu_int(cpu, IRQ_IPI_SINGLE);
}

static void smp4m_ipi_mask_one(int cpu)
{
	set_cpu_int(cpu, IRQ_IPI_MASK);
}

static struct smp_funcall {
	smpfunc_t func;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
	unsigned long arg4;
	unsigned long arg5;
	unsigned long processors_in[SUN4M_NCPUS];  
	unsigned long processors_out[SUN4M_NCPUS]; 
} ccall_info;

static DEFINE_SPINLOCK(cross_call_lock);

static void smp4m_cross_call(smpfunc_t func, cpumask_t mask, unsigned long arg1,
			     unsigned long arg2, unsigned long arg3,
			     unsigned long arg4)
{
		register int ncpus = SUN4M_NCPUS;
		unsigned long flags;

		spin_lock_irqsave(&cross_call_lock, flags);

		
		ccall_info.func = func;
		ccall_info.arg1 = arg1;
		ccall_info.arg2 = arg2;
		ccall_info.arg3 = arg3;
		ccall_info.arg4 = arg4;
		ccall_info.arg5 = 0;

		
		{
			register int i;

			cpumask_clear_cpu(smp_processor_id(), &mask);
			cpumask_and(&mask, cpu_online_mask, &mask);
			for (i = 0; i < ncpus; i++) {
				if (cpumask_test_cpu(i, &mask)) {
					ccall_info.processors_in[i] = 0;
					ccall_info.processors_out[i] = 0;
					set_cpu_int(i, IRQ_CROSS_CALL);
				} else {
					ccall_info.processors_in[i] = 1;
					ccall_info.processors_out[i] = 1;
				}
			}
		}

		{
			register int i;

			i = 0;
			do {
				if (!cpumask_test_cpu(i, &mask))
					continue;
				while (!ccall_info.processors_in[i])
					barrier();
			} while (++i < ncpus);

			i = 0;
			do {
				if (!cpumask_test_cpu(i, &mask))
					continue;
				while (!ccall_info.processors_out[i])
					barrier();
			} while (++i < ncpus);
		}
		spin_unlock_irqrestore(&cross_call_lock, flags);
}

void smp4m_cross_call_irq(void)
{
	int i = smp_processor_id();

	ccall_info.processors_in[i] = 1;
	ccall_info.func(ccall_info.arg1, ccall_info.arg2, ccall_info.arg3,
			ccall_info.arg4, ccall_info.arg5);
	ccall_info.processors_out[i] = 1;
}

void smp4m_percpu_timer_interrupt(struct pt_regs *regs)
{
	struct pt_regs *old_regs;
	int cpu = smp_processor_id();

	old_regs = set_irq_regs(regs);

	sun4m_clear_profile_irq(cpu);

	profile_tick(CPU_PROFILING);

	if (!--prof_counter(cpu)) {
		int user = user_mode(regs);

		irq_enter();
		update_process_times(user);
		irq_exit();

		prof_counter(cpu) = prof_multiplier(cpu);
	}
	set_irq_regs(old_regs);
}

static void __cpuinit smp_setup_percpu_timer(void)
{
	int cpu = smp_processor_id();

	prof_counter(cpu) = prof_multiplier(cpu) = 1;
	load_profile_irq(cpu, lvl14_resolution);

	if (cpu == boot_cpu_id)
		sun4m_unmask_profile_irq();
}

static void __init smp4m_blackbox_id(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;

	addr[0] = 0x81580000 | rd;		
	addr[1] = 0x8130200c | rd | rs1;	
	addr[2] = 0x80082003 | rd | rs1;	
}

static void __init smp4m_blackbox_current(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;

	addr[0] = 0x81580000 | rd;		
	addr[2] = 0x8130200a | rd | rs1;	
	addr[4] = 0x8008200c | rd | rs1;	
}

void __init sun4m_init_smp(void)
{
	BTFIXUPSET_BLACKBOX(hard_smp_processor_id, smp4m_blackbox_id);
	BTFIXUPSET_BLACKBOX(load_current, smp4m_blackbox_current);
	BTFIXUPSET_CALL(smp_cross_call, smp4m_cross_call, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(__hard_smp_processor_id, __smp4m_processor_id, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_ipi_resched, smp4m_ipi_resched, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_ipi_single, smp4m_ipi_single, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_ipi_mask_one, smp4m_ipi_mask_one, BTFIXUPCALL_NORM);
}
