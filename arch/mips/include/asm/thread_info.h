/* thread_info.h: MIPS low-level thread information
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__


#ifndef __ASSEMBLY__

#include <asm/processor.h>

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	unsigned long		tp_value;	
	__u32			cpu;		
	int			preempt_count;	

	mm_segment_t		addr_limit;	
	struct restart_block	restart_block;
	struct pt_regs		*regs;
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= _TIF_FIXADE,		\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.addr_limit	= KERNEL_DS,		\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

register struct thread_info *__current_thread_info __asm__("$28");
#define current_thread_info()  __current_thread_info

#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_32BIT)
#define THREAD_SIZE_ORDER (1)
#endif
#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_64BIT)
#define THREAD_SIZE_ORDER (2)
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define THREAD_SIZE_ORDER (1)
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_32KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define THREAD_SIZE_ORDER (0)
#endif

#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_MASK (THREAD_SIZE - 1UL)

#define STACK_WARN	(THREAD_SIZE / 8)

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#ifdef CONFIG_DEBUG_STACK_USAGE
#define alloc_thread_info_node(tsk, node) \
		kzalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#else
#define alloc_thread_info_node(tsk, node) \
		kmalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#endif

#define free_thread_info(info) kfree(info)

#endif 

#define PREEMPT_ACTIVE		0x10000000

#define TIF_SIGPENDING		1	
#define TIF_NEED_RESCHED	2	
#define TIF_SYSCALL_AUDIT	3	
#define TIF_SECCOMP		4	
#define TIF_NOTIFY_RESUME	5	
#define TIF_RESTORE_SIGMASK	9	
#define TIF_USEDFPU		16	
#define TIF_POLLING_NRFLAG	17	
#define TIF_MEMDIE		18	
#define TIF_FIXADE		20	
#define TIF_LOGADE		21	
#define TIF_32BIT_REGS		22	
#define TIF_32BIT_ADDR		23	
#define TIF_FPUBOUND		24	
#define TIF_LOAD_WATCH		25	
#define TIF_SYSCALL_TRACE	31	

#ifdef CONFIG_MIPS32_O32
#define TIF_32BIT TIF_32BIT_REGS
#elif defined(CONFIG_MIPS32_N32)
#define TIF_32BIT _TIF_32BIT_ADDR
#endif 

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_USEDFPU		(1<<TIF_USEDFPU)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_FIXADE		(1<<TIF_FIXADE)
#define _TIF_LOGADE		(1<<TIF_LOGADE)
#define _TIF_32BIT_REGS		(1<<TIF_32BIT_REGS)
#define _TIF_32BIT_ADDR		(1<<TIF_32BIT_ADDR)
#define _TIF_FPUBOUND		(1<<TIF_FPUBOUND)
#define _TIF_LOAD_WATCH		(1<<TIF_LOAD_WATCH)

#define _TIF_WORK_SYSCALL_EXIT	(_TIF_SYSCALL_TRACE | _TIF_SYSCALL_AUDIT)

#define _TIF_WORK_MASK		(0x0000ffef &				\
					~(_TIF_SECCOMP | _TIF_SYSCALL_AUDIT))
#define _TIF_ALLWORK_MASK	(0x8000ffff & ~_TIF_SECCOMP)

#endif 

#endif 
