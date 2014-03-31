/*
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
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

#ifndef _ASM_TILE_THREAD_INFO_H
#define _ASM_TILE_THREAD_INFO_H

#include <asm/processor.h>
#include <asm/page.h>
#ifndef __ASSEMBLY__

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	unsigned long		status;		
	__u32			homecache_cpu;	
	__u32			cpu;		
	int			preempt_count;	

	mm_segment_t		addr_limit;	
	struct restart_block	restart_block;
	struct single_step_state *step_state;	
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
	.step_state	= NULL,			\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

#endif 

#if PAGE_SIZE < 8192
#define THREAD_SIZE_ORDER (13 - PAGE_SHIFT)
#else
#define THREAD_SIZE_ORDER (0)
#endif
#define THREAD_SIZE_PAGES (1 << THREAD_SIZE_ORDER)

#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define LOG2_THREAD_SIZE (PAGE_SHIFT + THREAD_SIZE_ORDER)

#define STACK_WARN             (THREAD_SIZE/8)

#ifndef __ASSEMBLY__

register unsigned long stack_pointer __asm__("sp");

#define current_thread_info() \
  ((struct thread_info *)(stack_pointer & -THREAD_SIZE))

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR
extern struct thread_info *alloc_thread_info_node(struct task_struct *task, int node);
extern void free_thread_info(struct thread_info *info);

extern void smp_nap(void);

extern void _cpu_idle(void);

extern void cpu_idle_on_new_stack(struct thread_info *old_ti,
				  unsigned long new_sp,
				  unsigned long new_ss10);

#else 

#ifdef __tilegx__
#define EXTRACT_THREAD_INFO(reg) mm reg, zero, LOG2_THREAD_SIZE, 63
#else
#define GET_THREAD_INFO(reg) mm reg, sp, zero, LOG2_THREAD_SIZE, 31
#endif

#endif 

#define PREEMPT_ACTIVE		0x10000000

#define TIF_SIGPENDING		0	
#define TIF_NEED_RESCHED	1	
#define TIF_SINGLESTEP		2	
#define TIF_ASYNC_TLB		3	
#define TIF_SYSCALL_TRACE	4	
#define TIF_SYSCALL_AUDIT	5	
#define TIF_SECCOMP		6	
#define TIF_MEMDIE		7	
#define TIF_NOTIFY_RESUME	8	

#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_ASYNC_TLB		(1<<TIF_ASYNC_TLB)
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_MEMDIE		(1<<TIF_MEMDIE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)

#define _TIF_ALLWORK_MASK \
  (_TIF_SIGPENDING|_TIF_NEED_RESCHED|_TIF_SINGLESTEP|\
   _TIF_ASYNC_TLB|_TIF_NOTIFY_RESUME)

#ifdef __tilegx__
#define TS_COMPAT		0x0001	
#endif
#define TS_POLLING		0x0004	
#define TS_RESTORE_SIGMASK	0x0008	

#define tsk_is_polling(t) (task_thread_info(t)->status & TS_POLLING)

#ifndef __ASSEMBLY__
#define HAVE_SET_RESTORE_SIGMASK	1
static inline void set_restore_sigmask(void)
{
	struct thread_info *ti = current_thread_info();
	ti->status |= TS_RESTORE_SIGMASK;
	set_bit(TIF_SIGPENDING, &ti->flags);
}
#endif	

#endif 
