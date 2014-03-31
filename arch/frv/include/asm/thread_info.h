/* thread_info.h: description
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * Derived from include/asm-i386/thread_info.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#endif

#define THREAD_SIZE		8192

#define __HAVE_ARCH_TASK_STRUCT_ALLOCATOR

#ifndef __ASSEMBLY__

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	unsigned long		status;		
	__u32			cpu;		
	int			preempt_count;	

	mm_segment_t		addr_limit;	
	struct restart_block    restart_block;

	__u8			supervisor_stack[0];
};

#else 

#include <asm/asm-offsets.h>

#endif

#define PREEMPT_ACTIVE		0x10000000

#ifndef __ASSEMBLY__

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.addr_limit	= KERNEL_DS,		\
	.restart_block = {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

register struct thread_info *__current_thread_info asm("gr15");

#define current_thread_info() ({ __current_thread_info; })

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#ifdef CONFIG_DEBUG_STACK_USAGE
#define alloc_thread_info_node(tsk, node)			\
		kzalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#else
#define alloc_thread_info_node(tsk, node)			\
		kmalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#endif

#define free_thread_info(info)	kfree(info)

#endif 

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_SINGLESTEP		4	
#define TIF_RESTORE_SIGMASK	5	
#define TIF_POLLING_NRFLAG	16	
#define TIF_MEMDIE		17	

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)

#define _TIF_WORK_MASK		0x0000FFFE	
#define _TIF_ALLWORK_MASK	0x0000FFFF	

#define TS_USEDFPM		0x0001	

#endif 

#endif 
