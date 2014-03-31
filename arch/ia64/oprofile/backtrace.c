/**
 * @file backtrace.c
 *
 * @remark Copyright 2004 Silicon Graphics Inc.  All Rights Reserved.
 * @remark Read the file COPYING
 *
 * @author Greg Banks <gnb@melbourne.sgi.com>
 * @author Keith Owens <kaos@melbourne.sgi.com>
 * Based on work done for the ia64 port of the SGI kernprof patch, which is
 *    Copyright (c) 2003-2004 Silicon Graphics Inc.  All Rights Reserved.
 */

#include <linux/oprofile.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/ptrace.h>

typedef struct
{
	unsigned int depth;
	struct pt_regs *regs;
	struct unw_frame_info frame;
	unsigned long *prev_pfs_loc;	
} ia64_backtrace_t;

static __inline__ int in_ivt_code(unsigned long pc)
{
	extern char ia64_ivt[];
	return (pc >= (u_long)ia64_ivt && pc < (u_long)ia64_ivt+32768);
}

static __inline__ int next_frame(ia64_backtrace_t *bt)
{
	if (in_ivt_code(bt->frame.ip))
		return 0;

	if (bt->prev_pfs_loc && bt->regs && bt->frame.pfs_loc == bt->prev_pfs_loc)
		bt->frame.pfs_loc = &bt->regs->ar_pfs;
	bt->prev_pfs_loc = NULL;

	return unw_unwind(&bt->frame) == 0;
}


static void do_ia64_backtrace(struct unw_frame_info *info, void *vdata)
{
	ia64_backtrace_t *bt = vdata;
	struct switch_stack *sw;
	int count = 0;
	u_long pc, sp;

	sw = (struct switch_stack *)(info+1);
	
	sw = (struct switch_stack *)(((unsigned long)sw + 15) & ~15);

	unw_init_frame_info(&bt->frame, current, sw);

	
	do {
		unw_get_sp(&bt->frame, &sp);
		if (sp >= (u_long)bt->regs)
			break;
		if (!next_frame(bt))
			return;
	} while (count++ < 200);

	
	while (bt->depth-- && next_frame(bt)) {
		unw_get_ip(&bt->frame, &pc);
		oprofile_add_trace(pc);
		if (unw_is_intr_frame(&bt->frame)) {
			
			break;
		}
	}
}

void
ia64_backtrace(struct pt_regs * const regs, unsigned int depth)
{
	ia64_backtrace_t bt;
	unsigned long flags;

	if (user_mode(regs))
		return;

	bt.depth = depth;
	bt.regs = regs;
	bt.prev_pfs_loc = NULL;
	local_irq_save(flags);
	unw_init_running(do_ia64_backtrace, &bt);
	local_irq_restore(flags);
}
