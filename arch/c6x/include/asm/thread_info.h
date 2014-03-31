/*
 *  Port on Texas Instruments TMS320C6x architecture
 *
 *  Copyright (C) 2004, 2009, 2010, 2011 Texas Instruments Incorporated
 *  Author: Aurelien Jacquiot (aurelien.jacquiot@jaluna.com)
 *
 *  Updated for 2.6.3x: Mark Salter <msalter@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef _ASM_C6X_THREAD_INFO_H
#define _ASM_C6X_THREAD_INFO_H

#ifdef __KERNEL__

#include <asm/page.h>

#ifdef CONFIG_4KSTACKS
#define THREAD_SIZE		4096
#define THREAD_SHIFT		12
#define THREAD_ORDER		0
#else
#define THREAD_SIZE		8192
#define THREAD_SHIFT		13
#define THREAD_ORDER		1
#endif

#define THREAD_START_SP		(THREAD_SIZE - 8)

#ifndef __ASSEMBLY__

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	int			cpu;		
	int			preempt_count;	
	mm_segment_t		addr_limit;	
	struct restart_block	restart_block;
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.addr_limit	= KERNEL_DS,		\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

static inline __attribute__((const))
struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	asm volatile (" clr   .s2 B15,0,%1,%0\n"
		      : "=b" (ti)
		      : "Iu5" (THREAD_SHIFT - 1));
	return ti;
}

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#ifdef CONFIG_DEBUG_STACK_USAGE
#define THREAD_FLAGS (GFP_KERNEL | __GFP_NOTRACK | __GFP_ZERO)
#else
#define THREAD_FLAGS (GFP_KERNEL | __GFP_NOTRACK)
#endif

#define alloc_thread_info_node(tsk, node)	\
	((struct thread_info *)__get_free_pages(THREAD_FLAGS, THREAD_ORDER))

#define free_thread_info(ti)	free_pages((unsigned long) (ti), THREAD_ORDER)
#define get_thread_info(ti)	get_task_struct((ti)->task)
#define put_thread_info(ti)	put_task_struct((ti)->task)
#endif 

#define	PREEMPT_ACTIVE	0x10000000

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_RESTORE_SIGMASK	4	

#define TIF_POLLING_NRFLAG	16	
#define TIF_MEMDIE		17	

#define TIF_WORK_MASK		0x00007FFE 
#define TIF_ALLWORK_MASK	0x00007FFF 

#endif 

#endif 
