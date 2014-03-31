/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/io.h>
#include <linux/of.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/hardware.h>
#include <mach/id.h>

static void __iomem *l2x0_base;

static int __init ux500_l2x0_unlock(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		writel_relaxed(0x0, l2x0_base + L2X0_LOCKDOWN_WAY_D_BASE +
			       i * L2X0_LOCKDOWN_STRIDE);
		writel_relaxed(0x0, l2x0_base + L2X0_LOCKDOWN_WAY_I_BASE +
			       i * L2X0_LOCKDOWN_STRIDE);
	}
	return 0;
}

static int __init ux500_l2x0_init(void)
{
	if (cpu_is_u5500())
		l2x0_base = __io_address(U5500_L2CC_BASE);
	else if (cpu_is_u8500())
		l2x0_base = __io_address(U8500_L2CC_BASE);
	else
		ux500_unknown_soc();

	
	ux500_l2x0_unlock();

	
	if (of_have_populated_dt())
		l2x0_of_init(0x3e060000, 0xc0000fff);
	else
		l2x0_init(l2x0_base, 0x3e060000, 0xc0000fff);

	outer_cache.disable = NULL;

	return 0;
}

early_initcall(ux500_l2x0_init);
