/*
 * SMP support for R-Mobile / SH-Mobile
 *
 * Copyright (C) 2010  Magnus Damm
 *
 * Based on realview, Copyright (C) 2002 ARM Ltd, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <mach/common.h>
#include <asm/cacheflush.h>

static cpumask_t dead_cpus;

int platform_cpu_kill(unsigned int cpu)
{
	int k;

	for (k = 0; k < 1000; k++) {
		if (cpumask_test_cpu(cpu, &dead_cpus))
			return shmobile_platform_cpu_kill(cpu);

		mdelay(1);
	}

	return 0;
}

void platform_cpu_die(unsigned int cpu)
{
	
	flush_cache_all();
	dsb();

	
	cpumask_set_cpu(cpu, &dead_cpus);

	while (1) {
		asm(".word	0xe320f003\n"
		    :
		    :
		    : "memory", "cc");
	}
}

int platform_cpu_disable(unsigned int cpu)
{
	cpumask_clear_cpu(cpu, &dead_cpus);
	return cpu == 0 ? -EPERM : 0;
}
