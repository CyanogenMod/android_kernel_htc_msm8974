
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/cpumask.h>

#include <asm/r4k-timer.h>
#include <linux/atomic.h>
#include <asm/barrier.h>
#include <asm/mipsregs.h>

static atomic_t __cpuinitdata count_start_flag = ATOMIC_INIT(0);
static atomic_t __cpuinitdata count_count_start = ATOMIC_INIT(0);
static atomic_t __cpuinitdata count_count_stop = ATOMIC_INIT(0);
static atomic_t __cpuinitdata count_reference = ATOMIC_INIT(0);

#define COUNTON	100
#define NR_LOOPS 5

void __cpuinit synchronise_count_master(void)
{
	int i;
	unsigned long flags;
	unsigned int initcount;
	int nslaves;

#ifdef CONFIG_MIPS_MT_SMTC
	return;
#endif

	printk(KERN_INFO "Synchronize counters across %u CPUs: ",
	       num_online_cpus());

	local_irq_save(flags);

	atomic_set(&count_reference, read_c0_count());
	atomic_set(&count_start_flag, 1);
	smp_wmb();

	
	initcount = read_c0_count();


	nslaves = num_online_cpus()-1;
	for (i = 0; i < NR_LOOPS; i++) {
		
		while (atomic_read(&count_count_start) != nslaves)
			mb();
		atomic_set(&count_count_stop, 0);
		smp_wmb();

		
		atomic_inc(&count_count_start);

		if (i == NR_LOOPS-1)
			write_c0_count(initcount);

		while (atomic_read(&count_count_stop) != nslaves)
			mb();
		atomic_set(&count_count_start, 0);
		smp_wmb();
		atomic_inc(&count_count_stop);
	}
	
	write_c0_compare(read_c0_count() + COUNTON);

	local_irq_restore(flags);

	printk("done.\n");
}

void __cpuinit synchronise_count_slave(void)
{
	int i;
	unsigned long flags;
	unsigned int initcount;
	int ncpus;

#ifdef CONFIG_MIPS_MT_SMTC
	return;
#endif

	local_irq_save(flags);


	while (!atomic_read(&count_start_flag))
		mb();

	
	initcount = atomic_read(&count_reference);

	ncpus = num_online_cpus();
	for (i = 0; i < NR_LOOPS; i++) {
		atomic_inc(&count_count_start);
		while (atomic_read(&count_count_start) != ncpus)
			mb();

		if (i == NR_LOOPS-1)
			write_c0_count(initcount);

		atomic_inc(&count_count_stop);
		while (atomic_read(&count_count_stop) != ncpus)
			mb();
	}
	
	write_c0_compare(read_c0_count() + COUNTON);

	local_irq_restore(flags);
}
#undef NR_LOOPS
