/*
 * Copyright 2007-2009 Analog Devices Inc.
 *               Graff Yang <graf.yang@analog.com>
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/smp.h>
#include <asm/blackfin.h>
#include <asm/cacheflush.h>
#include <mach/pll.h>

int hotplug_coreb;

void platform_cpu_die(void)
{
	unsigned long iwr;

	hotplug_coreb = 1;

	blackfin_invalidate_entire_dcache();

	
	bfin_write_TCNTL(0);

	
	bfin_write_SICB_SYSCR(bfin_read_SICB_SYSCR() | (1 << (10 + 1)));
	SSYNC();

	
	bfin_iwr_set_sup0(&iwr, &iwr, &iwr);
	SSYNC();

	coreb_die();
}
