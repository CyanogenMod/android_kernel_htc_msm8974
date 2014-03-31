/*
 * Copyright 2004-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#include <asm/page.h>
#include <asm/entry.h>
#include <asm/l1layout.h>
#include <linux/compiler.h>

#ifdef __KERNEL__

#define ALIGN_PAGE_MASK		0xffffe000

#define THREAD_SIZE_ORDER	1
#define THREAD_SIZE		8192	
#define STACK_WARN		(THREAD_SIZE/8)

#ifndef __ASSEMBLY__

typedef unsigned long mm_segment_t;


struct thread_info {
	struct task_struct *task;	
	struct exec_domain *exec_domain;	
	unsigned long flags;	
	int cpu;		
	int preempt_count;	
	mm_segment_t addr_limit;	
	struct restart_block restart_block;
#ifndef CONFIG_SMP
	struct l1_scratch_task_info l1_task_info;
#endif
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}
#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

__attribute_const__
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	__asm__("%0 = sp;" : "=da"(ti));
	return (struct thread_info *)((long)ti & ~((long)THREAD_SIZE-1));
}

#endif				

#define TI_TASK		0
#define TI_EXECDOMAIN	4
#define TI_FLAGS	8
#define TI_CPU		12
#define TI_PREEMPT	16

#define	PREEMPT_ACTIVE	0x4000000

#define TIF_SYSCALL_TRACE	0	
#define TIF_SIGPENDING		1	
#define TIF_NEED_RESCHED	2	
#define TIF_POLLING_NRFLAG	3	
#define TIF_MEMDIE		4	
#define TIF_RESTORE_SIGMASK	5	
#define TIF_FREEZE		6	
#define TIF_IRQ_SYNC		7	
#define TIF_NOTIFY_RESUME	8	
#define TIF_SINGLESTEP		9

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_FREEZE		(1<<TIF_FREEZE)
#define _TIF_IRQ_SYNC		(1<<TIF_IRQ_SYNC)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)

#define _TIF_WORK_MASK		0x0000FFFE	

#endif				

#endif				
