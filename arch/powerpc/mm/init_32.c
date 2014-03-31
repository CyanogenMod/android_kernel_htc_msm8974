/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  PPC44x/36-bit changes by Matt Porter (mporter@mvista.com)
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/initrd.h>
#include <linux/pagemap.h>
#include <linux/memblock.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/hugetlb.h>

#include <asm/pgalloc.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/smp.h>
#include <asm/machdep.h>
#include <asm/btext.h>
#include <asm/tlb.h>
#include <asm/sections.h>
#include <asm/hugetlb.h>

#include "mmu_decl.h"

#if defined(CONFIG_KERNEL_START_BOOL) || defined(CONFIG_LOWMEM_SIZE_BOOL)
#if (CONFIG_LOWMEM_SIZE > (0xF0000000 - PAGE_OFFSET))
#error "You must adjust CONFIG_LOWMEM_SIZE or CONFIG_START_KERNEL"
#endif
#endif
#define MAX_LOW_MEM	CONFIG_LOWMEM_SIZE

phys_addr_t total_memory;
phys_addr_t total_lowmem;

phys_addr_t memstart_addr = (phys_addr_t)~0ull;
EXPORT_SYMBOL(memstart_addr);
phys_addr_t kernstart_addr;
EXPORT_SYMBOL(kernstart_addr);

#ifdef CONFIG_RELOCATABLE_PPC32
long long virt_phys_offset;
EXPORT_SYMBOL(virt_phys_offset);
#endif

phys_addr_t lowmem_end_addr;

int boot_mapsize;
#ifdef CONFIG_PPC_PMAC
unsigned long agp_special_page;
EXPORT_SYMBOL(agp_special_page);
#endif

void MMU_init(void);

extern struct task_struct *current_set[NR_CPUS];

int __map_without_bats;
int __map_without_ltlbs;

int __allow_ioremap_reserved;

unsigned long __max_low_memory = MAX_LOW_MEM;

void MMU_setup(void)
{
	
	if (strstr(cmd_line, "nobats")) {
		__map_without_bats = 1;
	}

	if (strstr(cmd_line, "noltlbs")) {
		__map_without_ltlbs = 1;
	}
#ifdef CONFIG_DEBUG_PAGEALLOC
	__map_without_bats = 1;
	__map_without_ltlbs = 1;
#endif
}

void __init MMU_init(void)
{
	if (ppc_md.progress)
		ppc_md.progress("MMU:enter", 0x111);

	
	MMU_setup();

	reserve_hugetlb_gpages();

	if (memblock.memory.cnt > 1) {
#ifndef CONFIG_WII
		memblock_enforce_memory_limit(memblock.memory.regions[0].size);
		printk(KERN_WARNING "Only using first contiguous memory region");
#else
		wii_memory_fixups();
#endif
	}

	total_lowmem = total_memory = memblock_end_of_DRAM() - memstart_addr;
	lowmem_end_addr = memstart_addr + total_lowmem;

#ifdef CONFIG_FSL_BOOKE
	adjust_total_lowmem();
#endif 

	if (total_lowmem > __max_low_memory) {
		total_lowmem = __max_low_memory;
		lowmem_end_addr = memstart_addr + total_lowmem;
#ifndef CONFIG_HIGHMEM
		total_memory = total_lowmem;
		memblock_enforce_memory_limit(total_lowmem);
#endif 
	}

	
	if (ppc_md.progress)
		ppc_md.progress("MMU:hw init", 0x300);
	MMU_init_hw();

	
	if (ppc_md.progress)
		ppc_md.progress("MMU:mapin", 0x301);
	mapin_ram();

	
	ioremap_bot = IOREMAP_TOP;

	
	if (ppc_md.progress)
		ppc_md.progress("MMU:setio", 0x302);

	if (ppc_md.progress)
		ppc_md.progress("MMU:exit", 0x211);

	
#ifdef CONFIG_BOOTX_TEXT
	btext_unmap();
#endif

	
	memblock_set_current_limit(lowmem_end_addr);
}

void __init *early_get_page(void)
{
	if (init_bootmem_done)
		return alloc_bootmem_pages(PAGE_SIZE);
	else
		return __va(memblock_alloc(PAGE_SIZE, PAGE_SIZE));
}

#ifdef CONFIG_8xx 
void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				phys_addr_t first_memblock_size)
{
	BUG_ON(first_memblock_base != 0);

	
	memblock_set_current_limit(min_t(u64, first_memblock_size, 0x00800000));
}
#endif 
