#include <linux/irq.h>
#include <linux/init.h>

#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/smtc.h>
#include <asm/smtc_ipi.h>



static void msmtc_send_ipi_single(int cpu, unsigned int action)
{
	
	smtc_send_ipi(cpu, LINUX_SMP_IPI, action);
}

static void msmtc_send_ipi_mask(const struct cpumask *mask, unsigned int action)
{
	unsigned int i;

	for_each_cpu(i, mask)
		msmtc_send_ipi_single(i, action);
}

static void __cpuinit msmtc_init_secondary(void)
{
	int myvpe;

	
	myvpe = read_c0_tcbind() & TCBIND_CURVPE;
	if (myvpe != 0) {
		
		clear_c0_status(ST0_IM);
		set_c0_status((0x100 << cp0_compare_irq)
				| (0x100 << MIPS_CPU_IPI_IRQ));
		if (cp0_perfcount_irq >= 0)
			set_c0_status(0x100 << cp0_perfcount_irq);
	}

	smtc_init_secondary();
}

static void __cpuinit msmtc_boot_secondary(int cpu, struct task_struct *idle)
{
	smtc_boot_secondary(cpu, idle);
}

static void __cpuinit msmtc_smp_finish(void)
{
	smtc_smp_finish();
}


static void msmtc_cpus_done(void)
{
}


static void __init msmtc_smp_setup(void)
{
	smp_num_siblings = smtc_build_cpu_map(0);
}

static void __init msmtc_prepare_cpus(unsigned int max_cpus)
{
	smtc_prepare_cpus(max_cpus);
}

struct plat_smp_ops msmtc_smp_ops = {
	.send_ipi_single	= msmtc_send_ipi_single,
	.send_ipi_mask		= msmtc_send_ipi_mask,
	.init_secondary		= msmtc_init_secondary,
	.smp_finish		= msmtc_smp_finish,
	.cpus_done		= msmtc_cpus_done,
	.boot_secondary		= msmtc_boot_secondary,
	.smp_setup		= msmtc_smp_setup,
	.prepare_cpus		= msmtc_prepare_cpus,
};

#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF


int plat_set_irq_affinity(struct irq_data *d, const struct cpumask *affinity,
			  bool force)
{
	cpumask_t tmask;
	int cpu = 0;
	void smtc_set_irq_affinity(unsigned int irq, cpumask_t aff);


	cpumask_copy(&tmask, affinity);
	for_each_cpu(cpu, affinity) {
		if ((cpu_data[cpu].vpe_id != 0) || !cpu_online(cpu))
			cpu_clear(cpu, tmask);
	}
	cpumask_copy(d->affinity, &tmask);

	if (cpus_empty(tmask))
		printk(KERN_WARNING
		       "IRQ affinity leaves no legal CPU for IRQ %d\n", d->irq);

	
	smtc_set_irq_affinity(d->irq, tmask);

	return IRQ_SET_MASK_OK_NOCOPY;
}
#endif 
