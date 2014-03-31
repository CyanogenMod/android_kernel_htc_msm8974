/*
 * reset.c  -- common ColdFire SoC reset support
 *
 * (C) Copyright 2012, Greg Ungerer <gerg@uclinux.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>


#ifdef MCFSIM_SYPCR
static void mcf_cpu_reset(void)
{
	local_irq_disable();
	
	__raw_writeb(0xc0, MCF_MBAR + MCFSIM_SYPCR);
	for (;;)
		;
}
#endif

#ifdef MCF_RCR
static void mcf_cpu_reset(void)
{
	local_irq_disable();
	__raw_writeb(MCF_RCR_SWRESET, MCF_RCR);
}
#endif

static int __init mcf_setup_reset(void)
{
	mach_reset = mcf_cpu_reset;
	return 0;
}

arch_initcall(mcf_setup_reset);
