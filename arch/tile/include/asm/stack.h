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

#ifndef _ASM_TILE_STACK_H
#define _ASM_TILE_STACK_H

#include <linux/types.h>
#include <linux/sched.h>
#include <asm/backtrace.h>
#include <asm/page.h>
#include <hv/hypervisor.h>

struct KBacktraceIterator {
	BacktraceIterator it;
	struct task_struct *task;     
	int end;		      
	int new_context;              
	int profile;                  
	int verbose;		      
	int is_current;               
};


extern void KBacktraceIterator_init(struct KBacktraceIterator *kbt,
				    struct task_struct *, struct pt_regs *);

extern void KBacktraceIterator_init_current(struct KBacktraceIterator *kbt);

extern void _KBacktraceIterator_init_current(struct KBacktraceIterator *kbt,
				ulong pc, ulong lr, ulong sp, ulong r52);

extern int KBacktraceIterator_end(struct KBacktraceIterator *kbt);

extern void KBacktraceIterator_next(struct KBacktraceIterator *kbt);

extern void tile_show_stack(struct KBacktraceIterator *, int headers);

extern void dump_stack_regs(struct pt_regs *);

extern void _dump_stack(int dummy, ulong pc, ulong lr, ulong sp, ulong r52);

#endif 
