/*
 * linux/arch/arm/mach-pxa/mfp.c
 *
 * PXA3xx Multi-Function Pin Support
 *
 * Copyright (C) 2007 Marvell Internation Ltd.
 *
 * 2007-08-21: eric miao <eric.miao@marvell.com>
 *             initial version
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <mach/hardware.h>
#include <mach/mfp-pxa3xx.h>
#include <mach/pxa3xx-regs.h>

#ifdef CONFIG_PM
static int pxa3xx_mfp_suspend(void)
{
	mfp_config_lpm();
	return 0;
}

static void pxa3xx_mfp_resume(void)
{
	mfp_config_run();

	ASCR &= ~(ASCR_RDH | ASCR_D1S | ASCR_D2S | ASCR_D3S);
}
#else
#define pxa3xx_mfp_suspend	NULL
#define pxa3xx_mfp_resume	NULL
#endif

struct syscore_ops pxa3xx_mfp_syscore_ops = {
	.suspend	= pxa3xx_mfp_suspend,
	.resume		= pxa3xx_mfp_resume,
};
