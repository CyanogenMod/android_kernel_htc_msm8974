/*
 * This file contains the routines for initializing the MMU
 * on the 4xx series of chips.
 *  -- paulus
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
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

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/memblock.h>

#include <asm/pgalloc.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/uaccess.h>
#include <asm/smp.h>
#include <asm/bootx.h>
#include <asm/machdep.h>
#include <asm/setup.h>

#include "mmu_decl.h"

extern int __map_without_ltlbs;
void __init MMU_init_hw(void)
{

        mtspr(SPRN_ZPR, 0x10000000);

	flush_instruction_cache();


        mtspr(SPRN_DCWR, 0x00000000);	


        mtspr(SPRN_DCCR, 0xFFFF0000);	
        mtspr(SPRN_ICCR, 0xFFFF0000);	
}

#define LARGE_PAGE_SIZE_16M	(1<<24)
#define LARGE_PAGE_SIZE_4M	(1<<22)

unsigned long __init mmu_mapin_ram(unsigned long top)
{
	unsigned long v, s, mapped;
	phys_addr_t p;

	v = KERNELBASE;
	p = 0;
	s = total_lowmem;

	if (__map_without_ltlbs)
		return 0;

	while (s >= LARGE_PAGE_SIZE_16M) {
		pmd_t *pmdp;
		unsigned long val = p | _PMD_SIZE_16M | _PAGE_EXEC | _PAGE_HWWRITE;

		pmdp = pmd_offset(pud_offset(pgd_offset_k(v), v), v);
		pmd_val(*pmdp++) = val;
		pmd_val(*pmdp++) = val;
		pmd_val(*pmdp++) = val;
		pmd_val(*pmdp++) = val;

		v += LARGE_PAGE_SIZE_16M;
		p += LARGE_PAGE_SIZE_16M;
		s -= LARGE_PAGE_SIZE_16M;
	}

	while (s >= LARGE_PAGE_SIZE_4M) {
		pmd_t *pmdp;
		unsigned long val = p | _PMD_SIZE_4M | _PAGE_EXEC | _PAGE_HWWRITE;

		pmdp = pmd_offset(pud_offset(pgd_offset_k(v), v), v);
		pmd_val(*pmdp) = val;

		v += LARGE_PAGE_SIZE_4M;
		p += LARGE_PAGE_SIZE_4M;
		s -= LARGE_PAGE_SIZE_4M;
	}

	mapped = total_lowmem - s;

	memblock_set_current_limit(mapped);

	return mapped;
}

void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				phys_addr_t first_memblock_size)
{
	BUG_ON(first_memblock_base != 0);

	
	memblock_set_current_limit(min_t(u64, first_memblock_size, 0x00800000));
}
