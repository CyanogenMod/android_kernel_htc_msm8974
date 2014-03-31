/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997 Ralf Baechle
 */
#include <linux/irqflags.h>
#include <linux/kernel.h>

#include <asm/cacheflush.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>

void wrppmc_machine_restart(char *command)
{
	local_irq_disable();
	set_c0_status(ST0_BEV | ST0_ERL);
	change_c0_config(CONF_CM_CMASK, CONF_CM_UNCACHED);
	flush_cache_all();
	write_c0_wired(0);
	__asm__ __volatile__("jr\t%0"::"r"(0xbfc00000));
}

void wrppmc_machine_halt(void)
{
	local_irq_disable();

	printk(KERN_NOTICE "You can safely turn off the power\n");
	while (1) {
		if (cpu_wait)
			cpu_wait();
	}
}
