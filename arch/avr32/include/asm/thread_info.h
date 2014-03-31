/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_THREAD_INFO_H
#define __ASM_AVR32_THREAD_INFO_H

#include <asm/page.h>

#define THREAD_SIZE_ORDER	1
#define THREAD_SIZE		(PAGE_SIZE << THREAD_SIZE_ORDER)

#ifndef __ASSEMBLY__
#include <asm/types.h>

struct task_struct;
struct exec_domain;

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	__u32			cpu;
	__s32			preempt_count;	
	__u32			rar_saved;	
	__u32			rsr_saved;	
	struct restart_block	restart_block;
	__u8			supervisor_stack[0];
};

#define INIT_THREAD_INFO(tsk)						\
{									\
	.task		= &tsk,						\
	.exec_domain	= &default_exec_domain,				\
	.flags		= 0,						\
	.cpu		= 0,						\
	.preempt_count	= INIT_PREEMPT_COUNT,				\
	.restart_block	= {						\
		.fn	= do_no_restart_syscall				\
	}								\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

static inline struct thread_info *current_thread_info(void)
{
	unsigned long addr = ~(THREAD_SIZE - 1);

	asm("and %0, sp" : "=r"(addr) : "0"(addr));
	return (struct thread_info *)addr;
}

#define get_thread_info(ti) get_task_struct((ti)->task)
#define put_thread_info(ti) put_task_struct((ti)->task)

#endif 

#define PREEMPT_ACTIVE		0x40000000

#define TIF_SYSCALL_TRACE       0       
#define TIF_SIGPENDING          1       
#define TIF_NEED_RESCHED        2       
#define TIF_POLLING_NRFLAG      3       
#define TIF_BREAKPOINT		4	
#define TIF_SINGLE_STEP		5	
#define TIF_MEMDIE		6	
#define TIF_RESTORE_SIGMASK	7	
#define TIF_CPU_GOING_TO_SLEEP	8	
#define TIF_NOTIFY_RESUME	9	
#define TIF_DEBUG		30	
#define TIF_USERSPACE		31      

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_SINGLE_STEP	(1 << TIF_SINGLE_STEP)
#define _TIF_MEMDIE		(1 << TIF_MEMDIE)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#define _TIF_CPU_GOING_TO_SLEEP (1 << TIF_CPU_GOING_TO_SLEEP)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)


#define _TIF_WORK_MASK				\
	((1 << TIF_SIGPENDING)			\
	 | _TIF_NOTIFY_RESUME			\
	 | (1 << TIF_NEED_RESCHED)		\
	 | (1 << TIF_POLLING_NRFLAG)		\
	 | (1 << TIF_BREAKPOINT)		\
	 | (1 << TIF_RESTORE_SIGMASK))

#define _TIF_ALLWORK_MASK	(_TIF_WORK_MASK | (1 << TIF_SYSCALL_TRACE) | \
				 _TIF_NOTIFY_RESUME)
#define _TIF_DBGWORK_MASK	(_TIF_WORK_MASK & ~(1 << TIF_BREAKPOINT))

#endif 
