/*
 *  linux/arch/arm/mach-tegra/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 *  Copyright (C) 2009 Palm
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/powergate.h>

#include "fuse.h"
#include "flowctrl.h"
#include "reset.h"

extern void tegra_secondary_startup(void);

static void __iomem *scu_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE);

#define EVP_CPU_RESET_VECTOR \
	(IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE) + 0x100)
#define CLK_RST_CONTROLLER_CLK_CPU_CMPLX \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x4c)
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x340)
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x344)
#define CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x34c)

#define CPU_CLOCK(cpu)	(0x1<<(8+cpu))
#define CPU_RESET(cpu)	(0x1111ul<<(cpu))

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	gic_secondary_init(0);

}

static int tegra20_power_up_cpu(unsigned int cpu)
{
	u32 reg;

	
	reg = readl(CLK_RST_CONTROLLER_CLK_CPU_CMPLX);
	writel(reg & ~CPU_CLOCK(cpu), CLK_RST_CONTROLLER_CLK_CPU_CMPLX);
	barrier();
	reg = readl(CLK_RST_CONTROLLER_CLK_CPU_CMPLX);

	
	flowctrl_write_cpu_csr(cpu, 0);

	return 0;
}

static int tegra30_power_up_cpu(unsigned int cpu)
{
	u32 reg;
	int ret, pwrgateid;
	unsigned long timeout;

	pwrgateid = tegra_cpu_powergate_id(cpu);
	if (pwrgateid < 0)
		return pwrgateid;

	
	if (!tegra_powergate_is_powered(pwrgateid)) {
		ret = tegra_powergate_power_on(pwrgateid);
		if (ret)
			return ret;

		
		timeout = jiffies + 10*HZ;
		while (tegra_powergate_is_powered(pwrgateid)) {
			if (time_after(jiffies, timeout))
				return -ETIMEDOUT;
			udelay(10);
		}
	}

	
	writel(CPU_CLOCK(cpu), CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR);
	reg = readl(CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR);
	udelay(10);

	
	ret = tegra_powergate_remove_clamping(pwrgateid);
	udelay(10);

	
	flowctrl_write_cpu_csr(cpu, 0);

	return 0;
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	int status;

	writel(CPU_RESET(cpu), CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET);
	dmb();

	flowctrl_write_cpu_halt(cpu, 0);

	switch (tegra_chip_id) {
	case TEGRA20:
		status = tegra20_power_up_cpu(cpu);
		break;
	case TEGRA30:
		status = tegra30_power_up_cpu(cpu);
		break;
	default:
		status = -EINVAL;
		break;
	}

	if (status)
		goto done;

	
	writel(CPU_RESET(cpu), CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);
	wmb();
done:
	return status;
}

void __init smp_init_cpus(void)
{
	unsigned int i, ncores = scu_get_core_count(scu_base);

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
	tegra_cpu_reset_handler_init();
	scu_enable(scu_base);
}
