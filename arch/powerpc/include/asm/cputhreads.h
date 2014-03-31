#ifndef _ASM_POWERPC_CPUTHREADS_H
#define _ASM_POWERPC_CPUTHREADS_H

#include <linux/cpumask.h>


#ifdef CONFIG_SMP
extern int threads_per_core;
extern int threads_shift;
extern cpumask_t threads_core_mask;
#else
#define threads_per_core	1
#define threads_shift		0
#define threads_core_mask	(CPU_MASK_CPU0)
#endif

static inline cpumask_t cpu_thread_mask_to_cores(const struct cpumask *threads)
{
	cpumask_t	tmp, res;
	int		i;

	cpumask_clear(&res);
	for (i = 0; i < NR_CPUS; i += threads_per_core) {
		cpumask_shift_left(&tmp, &threads_core_mask, i);
		if (cpumask_intersects(threads, &tmp))
			cpumask_set_cpu(i, &res);
	}
	return res;
}

static inline int cpu_nr_cores(void)
{
	return NR_CPUS >> threads_shift;
}

static inline cpumask_t cpu_online_cores_map(void)
{
	return cpu_thread_mask_to_cores(cpu_online_mask);
}

#ifdef CONFIG_SMP
int cpu_core_index_of_thread(int cpu);
int cpu_first_thread_of_core(int core);
#else
static inline int cpu_core_index_of_thread(int cpu) { return cpu; }
static inline int cpu_first_thread_of_core(int core) { return core; }
#endif

static inline int cpu_thread_in_core(int cpu)
{
	return cpu & (threads_per_core - 1);
}

static inline int cpu_first_thread_sibling(int cpu)
{
	return cpu & ~(threads_per_core - 1);
}

static inline int cpu_last_thread_sibling(int cpu)
{
	return cpu | (threads_per_core - 1);
}



#endif 

