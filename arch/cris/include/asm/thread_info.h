/* thread_info.h: CRIS low-level thread information
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 * 
 * CRIS port by Axis Communications
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/processor.h>
#include <arch/thread_info.h>
#include <asm/segment.h>
#endif


#ifndef __ASSEMBLY__
struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	__u32			cpu;		
	int			preempt_count;	
	__u32			tls;		

	mm_segment_t		addr_limit;	
	struct restart_block    restart_block;
	__u8			supervisor_stack[0];
};

#endif

#define PREEMPT_ACTIVE		0x10000000

#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)				\
{							\
	.task		= &tsk,				\
	.exec_domain	= &default_exec_domain,		\
	.flags		= 0,				\
	.cpu		= 0,				\
	.preempt_count	= INIT_PREEMPT_COUNT,		\
	.addr_limit	= KERNEL_DS,			\
	.restart_block = {				\
		       .fn = do_no_restart_syscall,	\
	},						\
}

#define init_thread_info	(init_thread_union.thread_info)

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR
#define alloc_thread_info_node(tsk, node)	\
	((struct thread_info *) __get_free_pages(GFP_KERNEL, 1))
#define free_thread_info(ti) free_pages((unsigned long) (ti), 1)

#endif 

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_RESTORE_SIGMASK	9	
#define TIF_POLLING_NRFLAG	16	
#define TIF_MEMDIE		17	

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)

#define _TIF_WORK_MASK		0x0000FFFE	
#define _TIF_ALLWORK_MASK	0x0000FFFF	

#endif 

#endif 
