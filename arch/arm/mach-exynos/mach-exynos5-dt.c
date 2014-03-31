/*
 * SAMSUNG EXYNOS5250 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/serial_core.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/map.h>

#include <plat/cpu.h>
#include <plat/regs-serial.h>

#include "common.h"

static const struct of_dev_auxdata exynos5250_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART0,
				"exynos4210-uart.0", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART1,
				"exynos4210-uart.1", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART2,
				"exynos4210-uart.2", NULL),
	OF_DEV_AUXDATA("samsung,exynos4210-uart", EXYNOS5_PA_UART3,
				"exynos4210-uart.3", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA0, "dma-pl330.0", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_PDMA1, "dma-pl330.1", NULL),
	OF_DEV_AUXDATA("arm,pl330", EXYNOS5_PA_MDMA1, "dma-pl330.2", NULL),
	{},
};

static void __init exynos5250_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
	s3c24xx_init_clocks(24000000);
}

static void __init exynos5250_dt_machine_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
				exynos5250_auxdata_lookup, NULL);
}

static char const *exynos5250_dt_compat[] __initdata = {
	"samsung,exynos5250",
	NULL
};

DT_MACHINE_START(EXYNOS5_DT, "SAMSUNG EXYNOS5 (Flattened Device Tree)")
	
	.init_irq	= exynos5_init_irq,
	.map_io		= exynos5250_dt_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= exynos5250_dt_machine_init,
	.timer		= &exynos4_timer,
	.dt_compat	= exynos5250_dt_compat,
	.restart        = exynos5_restart,
MACHINE_END
