
/*
 *	linux/arch/m68knommu/platform/5206/config.c
 *
 *	Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 * 	Copyright (C) 2000-2001, Lineo Inc. (www.lineo.com) 
 */


#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>


void __init config_BSP(char *commandp, int size)
{
#if defined(CONFIG_NETtel)
	
	memcpy(commandp, (char *) 0xf0004000, size);
	commandp[size-1] = 0;
#endif 

	mach_sched_init = hw_timer_init;

	
	mcf_mapirq2imr(25, MCFINTC_EINT1);
	mcf_mapirq2imr(28, MCFINTC_EINT4);
	mcf_mapirq2imr(31, MCFINTC_EINT7);
}

