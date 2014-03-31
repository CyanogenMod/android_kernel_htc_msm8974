/*
 * This file contains the routines for handling the MMU.
 *
 *    Copyright (C) 2007 Xilinx, Inc.  All rights reserved.
 *
 *  Derived from arch/ppc/mm/4xx_mmu.c:
 *  -- paulus
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
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

#include <linux/mm.h>
#include <linux/init.h>

#include <asm/tlbflush.h>
#include <asm/mmu_context.h>

mm_context_t next_mmu_context;
unsigned long context_map[LAST_CONTEXT / BITS_PER_LONG + 1];
atomic_t nr_free_contexts;
struct mm_struct *context_mm[LAST_CONTEXT+1];

void __init mmu_context_init(void)
{
	context_map[0] = (1 << FIRST_CONTEXT) - 1;
	next_mmu_context = FIRST_CONTEXT;
	atomic_set(&nr_free_contexts, LAST_CONTEXT - FIRST_CONTEXT + 1);
}

void steal_context(void)
{
	struct mm_struct *mm;

	
	
	if (next_mmu_context < FIRST_CONTEXT)
		next_mmu_context = FIRST_CONTEXT;
	mm = context_mm[next_mmu_context];
	flush_tlb_mm(mm);
	destroy_context(mm);
}
