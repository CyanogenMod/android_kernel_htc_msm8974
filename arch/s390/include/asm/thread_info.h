/*
 *  include/asm-s390/thread_info.h
 *
 *  S390 version
 *    Copyright (C) IBM Corp. 2002,2006
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __s390x__
#define THREAD_ORDER 1
#define ASYNC_ORDER  1
#else 
#ifndef __SMALL_STACK
#define THREAD_ORDER 2
#define ASYNC_ORDER  2
#else
#define THREAD_ORDER 1
#define ASYNC_ORDER  1
#endif
#endif 

#define THREAD_SIZE (PAGE_SIZE << THREAD_ORDER)
#define ASYNC_SIZE  (PAGE_SIZE << ASYNC_ORDER)

#ifndef __ASSEMBLY__
#include <asm/lowcore.h>
#include <asm/page.h>
#include <asm/processor.h>

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	unsigned int		cpu;		
	int			preempt_count;	
	struct restart_block	restart_block;
	unsigned int		system_call;
	__u64			user_timer;
	__u64			system_timer;
	unsigned long		last_break;	
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

static inline struct thread_info *current_thread_info(void)
{
	return (struct thread_info *) S390_lowcore.thread_info;
}

#define THREAD_SIZE_ORDER THREAD_ORDER

#endif

#define TIF_SYSCALL		0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_PER_TRAP		6	
#define TIF_MCCK_PENDING	7	
#define TIF_SYSCALL_TRACE	8	
#define TIF_SYSCALL_AUDIT	9	
#define TIF_SECCOMP		10	
#define TIF_SYSCALL_TRACEPOINT	11	
#define TIF_SIE			12	
#define TIF_POLLING_NRFLAG	16	
#define TIF_31BIT		17	
#define TIF_MEMDIE		18	
#define TIF_RESTORE_SIGMASK	19	
#define TIF_SINGLE_STEP		20	

#define _TIF_SYSCALL		(1<<TIF_SYSCALL)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_PER_TRAP		(1<<TIF_PER_TRAP)
#define _TIF_MCCK_PENDING	(1<<TIF_MCCK_PENDING)
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_SYSCALL_TRACEPOINT	(1<<TIF_SYSCALL_TRACEPOINT)
#define _TIF_SIE		(1<<TIF_SIE)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_31BIT		(1<<TIF_31BIT)
#define _TIF_SINGLE_STEP	(1<<TIF_SINGLE_STEP)

#ifdef CONFIG_64BIT
#define is_32bit_task()		(test_thread_flag(TIF_31BIT))
#else
#define is_32bit_task()		(1)
#endif

#endif 

#define PREEMPT_ACTIVE		0x4000000

#endif 
