#include <linux/export.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>

notrace unsigned int debug_smp_processor_id(void)
{
	unsigned long preempt_count = preempt_count();
	int this_cpu = raw_smp_processor_id();

	if (likely(preempt_count))
		goto out;

	if (irqs_disabled())
		goto out;

	if (cpumask_equal(tsk_cpus_allowed(current), cpumask_of(this_cpu)))
		goto out;

	if (system_state != SYSTEM_RUNNING)
		goto out;

	preempt_disable_notrace();

	if (!printk_ratelimit())
		goto out_enable;

	printk(KERN_ERR "BUG: using smp_processor_id() in preemptible [%08x] "
			"code: %s/%d\n",
			preempt_count() - 1, current->comm, current->pid);
	print_symbol("caller is %s\n", (long)__builtin_return_address(0));
	dump_stack();

out_enable:
	preempt_enable_no_resched_notrace();
out:
	return this_cpu;
}

EXPORT_SYMBOL(debug_smp_processor_id);

