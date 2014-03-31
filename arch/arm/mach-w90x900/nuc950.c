/*
 * linux/arch/arm/mach-w90x900/nuc950.c
 *
 * Based on linux/arch/arm/plat-s3c24xx/s3c244x.c by Ben Dooks
 *
 * Copyright (c) 2008 Nuvoton technology corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * NUC950 cpu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <mach/hardware.h>

#include "cpu.h"


static struct platform_device *nuc950_dev[] __initdata = {
	&nuc900_device_kpi,
	&nuc900_device_fmi,
	&nuc900_device_lcd,
};


static struct map_desc nuc950evb_iodesc[] __initdata = {
};


void __init nuc950_map_io(void)
{
	nuc900_map_io(nuc950evb_iodesc, ARRAY_SIZE(nuc950evb_iodesc));
}


void __init nuc950_init_clocks(void)
{
	nuc900_init_clocks();
}


void __init nuc950_board_init(void)
{
	nuc900_board_init(nuc950_dev, ARRAY_SIZE(nuc950_dev));
}
