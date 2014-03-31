/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_TRAPS_H
#define _ASM_TILE_TRAPS_H

#include <arch/chip.h>

void do_page_fault(struct pt_regs *, int fault_num,
		   unsigned long address, unsigned long write);
#if CHIP_HAS_TILE_DMA() || CHIP_HAS_SN_PROC()
void do_async_page_fault(struct pt_regs *);
#endif

#ifndef __tilegx__
struct intvec_state {
	void *handler;
	unsigned long vecnum;
	unsigned long fault_num;
	unsigned long info;
	unsigned long retval;
};
struct intvec_state do_page_fault_ics(struct pt_regs *regs, int fault_num,
				      unsigned long address,
				      unsigned long info);
#endif

void do_trap(struct pt_regs *, int fault_num, unsigned long reason);
void kernel_double_fault(int dummy, ulong pc, ulong lr, ulong sp, ulong r52);

void do_timer_interrupt(struct pt_regs *, int fault_num);

void hv_message_intr(struct pt_regs *, int intnum);

void tile_dev_intr(struct pt_regs *, int intnum);

#ifdef CONFIG_HARDWALL
void do_hardwall_trap(struct pt_regs *, int fault_num);
#endif

void do_breakpoint(struct pt_regs *, int fault_num);


#ifdef __tilegx__
void gx_singlestep_handle(struct pt_regs *, int fault_num);

void fill_ra_stack(void);
#endif

#endif 
