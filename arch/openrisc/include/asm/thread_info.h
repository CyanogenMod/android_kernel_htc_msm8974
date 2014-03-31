/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/processor.h>
#endif



#define THREAD_SIZE_ORDER 0
#define THREAD_SIZE       (PAGE_SIZE << THREAD_SIZE_ORDER)

#ifndef __ASSEMBLY__

typedef unsigned long mm_segment_t;

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	__u32			cpu;		
	__s32			preempt_count; 

	mm_segment_t		addr_limit; 
	struct restart_block    restart_block;
	__u8			supervisor_stack[0];

	
	unsigned long           ksp;
};
#endif

#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)				\
{							\
	.task		= &tsk,				\
	.exec_domain	= &default_exec_domain,		\
	.flags		= 0,				\
	.cpu		= 0,				\
	.preempt_count	= 1,				\
	.addr_limit	= KERNEL_DS,			\
	.restart_block  = {				\
			  .fn = do_no_restart_syscall,	\
	},						\
	.ksp            = 0,                            \
}

#define init_thread_info	(init_thread_union.thread_info)

register struct thread_info *current_thread_info_reg asm("r10");
#define current_thread_info()   (current_thread_info_reg)

#define get_thread_info(ti) get_task_struct((ti)->task)
#define put_thread_info(ti) put_task_struct((ti)->task)

#endif 

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_SINGLESTEP		4	
#define TIF_SYSCALL_TRACEPOINT  8       
#define TIF_RESTORE_SIGMASK     9
#define TIF_POLLING_NRFLAG	16	
#define TIF_MEMDIE              17

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_RESTORE_SIGMASK     (1<<TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)


#define _TIF_WORK_MASK (0xff & ~(_TIF_SYSCALL_TRACE|_TIF_SINGLESTEP))

#endif 

#endif 
