/*
 * arch/sh/kernel/cpu/sh5/probe.c
 *
 * CPU Subtype Probing for SH-5.
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003 - 2007  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/string.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/tlb.h>

void __cpuinit cpu_probe(void)
{
	unsigned long long cir;

	cir = peek_real_address_q(0x0d000008);
	if ((cir & 0xffff) == 0x5103)
		boot_cpu_data.type = CPU_SH5_103;
	else if (((cir >> 32) & 0xffff) == 0x51e2)
		
		boot_cpu_data.type = CPU_SH5_101;

	boot_cpu_data.family = CPU_FAMILY_SH5;

	boot_cpu_data.icache.ways		= 4;
	boot_cpu_data.icache.sets		= 256;
	boot_cpu_data.icache.linesz		= L1_CACHE_BYTES;
	boot_cpu_data.icache.way_incr		= (1 << 13);
	boot_cpu_data.icache.entry_shift	= 5;
	boot_cpu_data.icache.way_size		= boot_cpu_data.icache.sets *
						  boot_cpu_data.icache.linesz;
	boot_cpu_data.icache.entry_mask		= 0x1fe0;
	boot_cpu_data.icache.flags		= 0;

	boot_cpu_data.dcache		= boot_cpu_data.icache;

#if defined(CONFIG_CACHE_WRITETHROUGH)
	set_bit(SH_CACHE_MODE_WT, &(boot_cpu_data.dcache.flags));
#elif defined(CONFIG_CACHE_WRITEBACK)
	set_bit(SH_CACHE_MODE_WB, &(boot_cpu_data.dcache.flags));
#endif

	
	sh64_tlb_init();
}
