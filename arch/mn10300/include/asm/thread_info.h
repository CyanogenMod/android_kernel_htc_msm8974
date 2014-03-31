/* MN10300 Low-level thread information
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#include <asm/page.h>

#define PREEMPT_ACTIVE		0x10000000

#ifdef CONFIG_4KSTACKS
#define THREAD_SIZE		(4096)
#else
#define THREAD_SIZE		(8192)
#endif

#define STACK_WARN		(THREAD_SIZE / 8)

#ifndef __ASSEMBLY__
typedef struct {
	unsigned long	seg;
} mm_segment_t;

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	struct pt_regs		*frame;		
	unsigned long		flags;		
	__u32			cpu;		
	__s32			preempt_count;	

	mm_segment_t		addr_limit;	
	struct restart_block    restart_block;

	__u8			supervisor_stack[0];
};

#define thread_info_to_uregs(ti)					\
	((struct pt_regs *)						\
	 ((unsigned long)ti + THREAD_SIZE - sizeof(struct pt_regs)))

#else 

#ifndef __ASM_OFFSETS_H__
#include <asm/asm-offsets.h>
#endif

#endif

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
#define init_uregs							\
	((struct pt_regs *)						\
	 ((unsigned long) init_stack + THREAD_SIZE - sizeof(struct pt_regs)))

extern struct thread_info *__current_ti;

static inline __attribute__((const))
struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	asm("mov sp,%0\n"
	    "and %1,%0\n"
	    : "=d" (ti)
	    : "i" (~(THREAD_SIZE - 1))
	    : "cc");
	return ti;
}

static inline __attribute__((const))
struct pt_regs *current_frame(void)
{
	return current_thread_info()->frame;
}

static inline unsigned long current_stack_pointer(void)
{
	unsigned long sp;
	asm("mov sp,%0; ":"=r" (sp));
	return sp;
}

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#ifdef CONFIG_DEBUG_STACK_USAGE
#define alloc_thread_info_node(tsk, node)			\
		kzalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#else
#define alloc_thread_info_node(tsk, node)			\
		kmalloc_node(THREAD_SIZE, GFP_KERNEL, node)
#endif

#ifndef CONFIG_KGDB
#define free_thread_info(ti)	kfree((ti))
#else
extern void free_thread_info(struct thread_info *);
#endif
#define get_thread_info(ti)	get_task_struct((ti)->task)
#define put_thread_info(ti)	put_task_struct((ti)->task)

#else 

#ifndef __VMLINUX_LDS__
.macro GET_THREAD_INFO reg
	mov	sp,\reg
	and	-THREAD_SIZE,\reg
.endm
#endif
#endif

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_SINGLESTEP		4	
#define TIF_RESTORE_SIGMASK	5	
#define TIF_POLLING_NRFLAG	16	
#define TIF_MEMDIE		17	

#define _TIF_SYSCALL_TRACE	+(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	+(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		+(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	+(1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		+(1 << TIF_SINGLESTEP)
#define _TIF_RESTORE_SIGMASK	+(1 << TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG	+(1 << TIF_POLLING_NRFLAG)

#define _TIF_WORK_MASK		0x0000FFFE	
#define _TIF_ALLWORK_MASK	0x0000FFFF	

#endif 

#endif 
