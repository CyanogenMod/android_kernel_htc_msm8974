/*
 * OMAP4 SMP source file. It contains platform specific fucntions
 * needed for the linux smp kernel.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Platform file needed for the OMAP4 SMP. This file is based on arm
 * realview smp platform.
 * * Copyright (c) 2002 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/smp_scu.h>

#include <mach/hardware.h>
#include <mach/omap-secure.h>

#include "iomap.h"
#include "common.h"
#include "clockdomain.h"

static void __iomem *scu_base;

static DEFINE_SPINLOCK(boot_lock);

void __iomem *omap4_get_scu_base(void)
{
	return scu_base;
}

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	if (cpu_is_omap443x() && (omap_type() != OMAP2_DEVICE_TYPE_GP))
		omap_secure_dispatcher(OMAP4_PPA_CPU_ACTRL_SMP_INDEX,
							4, 0, 0, 0, 0, 0);

	gic_secondary_init(0);

	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	static struct clockdomain *cpu1_clkdm;
	static bool booted;
	spin_lock(&boot_lock);

	omap_modify_auxcoreboot0(0x200, 0xfffffdff);
	flush_cache_all();
	smp_wmb();

	if (!cpu1_clkdm)
		cpu1_clkdm = clkdm_lookup("mpu1_clkdm");

	if (booted) {
		clkdm_wakeup(cpu1_clkdm);
		clkdm_allow_idle(cpu1_clkdm);
	} else {
		dsb_sev();
		booted = true;
	}

	gic_raise_softirq(cpumask_of(cpu), 1);

	spin_unlock(&boot_lock);

	return 0;
}

static void __init wakeup_secondary(void)
{
	omap_auxcoreboot_addr(virt_to_phys(omap_secondary_startup));
	smp_wmb();

	dsb_sev();
	mb();
}

void __init smp_init_cpus(void)
{
	unsigned int i, ncores;

	scu_base =  OMAP2_L4_IO_ADDRESS(OMAP44XX_SCU_BASE);
	BUG_ON(!scu_base);

	ncores = scu_get_core_count(scu_base);

	
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{

	scu_enable(scu_base);
	wakeup_secondary();
}
