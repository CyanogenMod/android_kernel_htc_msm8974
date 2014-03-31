/*
 *	Intel SMP support routines.
 *
 *	(c) 1995 Alan Cox, Building #3 <alan@lxorguk.ukuu.org.uk>
 *	(c) 1998-99, 2000, 2009 Ingo Molnar <mingo@redhat.com>
 *      (c) 2002,2003 Andi Kleen, SuSE Labs.
 *
 *	i386 and x86_64 integration by Glauber Costa <gcosta@redhat.com>
 *
 *	This code is released under the GNU General Public License version 2 or
 *	later.
 */

#include <linux/init.h>

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <linux/kernel_stat.h>
#include <linux/mc146818rtc.h>
#include <linux/cache.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/gfp.h>

#include <asm/mtrr.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include <asm/proto.h>
#include <asm/apic.h>
#include <asm/nmi.h>

static void native_smp_send_reschedule(int cpu)
{
	if (unlikely(cpu_is_offline(cpu))) {
		WARN_ON(1);
		return;
	}
	apic->send_IPI_mask(cpumask_of(cpu), RESCHEDULE_VECTOR);
}

void native_send_call_func_single_ipi(int cpu)
{
	apic->send_IPI_mask(cpumask_of(cpu), CALL_FUNCTION_SINGLE_VECTOR);
}

void native_send_call_func_ipi(const struct cpumask *mask)
{
	cpumask_var_t allbutself;

	if (!alloc_cpumask_var(&allbutself, GFP_ATOMIC)) {
		apic->send_IPI_mask(mask, CALL_FUNCTION_VECTOR);
		return;
	}

	cpumask_copy(allbutself, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), allbutself);

	if (cpumask_equal(mask, allbutself) &&
	    cpumask_equal(cpu_online_mask, cpu_callout_mask))
		apic->send_IPI_allbutself(CALL_FUNCTION_VECTOR);
	else
		apic->send_IPI_mask(mask, CALL_FUNCTION_VECTOR);

	free_cpumask_var(allbutself);
}

static atomic_t stopping_cpu = ATOMIC_INIT(-1);

static int smp_stop_nmi_callback(unsigned int val, struct pt_regs *regs)
{
	
	if (raw_smp_processor_id() == atomic_read(&stopping_cpu))
		return NMI_HANDLED;

	stop_this_cpu(NULL);

	return NMI_HANDLED;
}

static void native_nmi_stop_other_cpus(int wait)
{
	unsigned long flags;
	unsigned long timeout;

	if (reboot_force)
		return;

	if (num_online_cpus() > 1) {
		
		if (atomic_cmpxchg(&stopping_cpu, -1, safe_smp_processor_id()) != -1)
			return;

		if (register_nmi_handler(NMI_LOCAL, smp_stop_nmi_callback,
					 NMI_FLAG_FIRST, "smp_stop"))
			
			return;

		
		wmb();

		apic->send_IPI_allbutself(NMI_VECTOR);

		timeout = USEC_PER_SEC;
		while (num_online_cpus() > 1 && (wait || timeout--))
			udelay(1);
	}

	local_irq_save(flags);
	disable_local_APIC();
	local_irq_restore(flags);
}


asmlinkage void smp_reboot_interrupt(void)
{
	ack_APIC_irq();
	irq_enter();
	stop_this_cpu(NULL);
	irq_exit();
}

static void native_irq_stop_other_cpus(int wait)
{
	unsigned long flags;
	unsigned long timeout;

	if (reboot_force)
		return;

	if (num_online_cpus() > 1) {
		apic->send_IPI_allbutself(REBOOT_VECTOR);

		timeout = USEC_PER_SEC;
		while (num_online_cpus() > 1 && (wait || timeout--))
			udelay(1);
	}

	local_irq_save(flags);
	disable_local_APIC();
	local_irq_restore(flags);
}

static void native_smp_disable_nmi_ipi(void)
{
	smp_ops.stop_other_cpus = native_irq_stop_other_cpus;
}

void smp_reschedule_interrupt(struct pt_regs *regs)
{
	ack_APIC_irq();
	inc_irq_stat(irq_resched_count);
	scheduler_ipi();
}

void smp_call_function_interrupt(struct pt_regs *regs)
{
	ack_APIC_irq();
	irq_enter();
	generic_smp_call_function_interrupt();
	inc_irq_stat(irq_call_count);
	irq_exit();
}

void smp_call_function_single_interrupt(struct pt_regs *regs)
{
	ack_APIC_irq();
	irq_enter();
	generic_smp_call_function_single_interrupt();
	inc_irq_stat(irq_call_count);
	irq_exit();
}

static int __init nonmi_ipi_setup(char *str)
{
        native_smp_disable_nmi_ipi();
        return 1;
}

__setup("nonmi_ipi", nonmi_ipi_setup);

struct smp_ops smp_ops = {
	.smp_prepare_boot_cpu	= native_smp_prepare_boot_cpu,
	.smp_prepare_cpus	= native_smp_prepare_cpus,
	.smp_cpus_done		= native_smp_cpus_done,

	.stop_other_cpus	= native_nmi_stop_other_cpus,
	.smp_send_reschedule	= native_smp_send_reschedule,

	.cpu_up			= native_cpu_up,
	.cpu_die		= native_cpu_die,
	.cpu_disable		= native_cpu_disable,
	.play_dead		= native_play_dead,

	.send_call_func_ipi	= native_send_call_func_ipi,
	.send_call_func_single_ipi = native_send_call_func_single_ipi,
};
EXPORT_SYMBOL_GPL(smp_ops);
