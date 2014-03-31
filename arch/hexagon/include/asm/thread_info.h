/*
 * Thread support for the Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/registers.h>
#include <asm/page.h>
#endif

#define THREAD_SHIFT		12
#define THREAD_SIZE		(1<<THREAD_SHIFT)

#if THREAD_SHIFT >= PAGE_SHIFT
#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)
#else  
#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR
extern struct thread_info *alloc_thread_info_node(struct task_struct *tsk, int node);
extern void free_thread_info(struct thread_info *ti);
#endif


#ifndef __ASSEMBLY__

typedef struct {
	unsigned long seg;
} mm_segment_t;


struct thread_info {
	struct task_struct	*task;		
	struct exec_domain      *exec_domain;   
	unsigned long		flags;          
	__u32                   cpu;            
	int                     preempt_count;  
	mm_segment_t            addr_limit;     
	struct restart_block    restart_block;
	
	struct pt_regs		*regs;
	unsigned long		sp;
};

#else 

#include <asm/asm-offsets.h>

#endif  


#define PREEMPT_ACTIVE		0x10000000

#ifndef __ASSEMBLY__

#define INIT_THREAD_INFO(tsk)                   \
{                                               \
	.task           = &tsk,                 \
	.exec_domain    = &default_exec_domain, \
	.flags          = 0,                    \
	.cpu            = 0,                    \
	.preempt_count  = 1,                    \
	.addr_limit     = KERNEL_DS,            \
	.restart_block = {                      \
		.fn = do_no_restart_syscall,    \
	},                                      \
	.sp = 0,				\
	.regs = NULL,			\
}

#define init_thread_info        (init_thread_union.thread_info)
#define init_stack              (init_thread_union.stack)

#define	qqstr(s) qstr(s)
#define qstr(s) #s
#define QUOTED_THREADINFO_REG qqstr(THREADINFO_REG)

register struct thread_info *__current_thread_info asm(QUOTED_THREADINFO_REG);
#define current_thread_info()  __current_thread_info

#endif 


#define TIF_SYSCALL_TRACE       0       
#define TIF_NOTIFY_RESUME       1       
#define TIF_SIGPENDING          2       
#define TIF_NEED_RESCHED        3       
#define TIF_SINGLESTEP          4       
#define TIF_IRET                5       
#define TIF_RESTORE_SIGMASK     6       
#define TIF_POLLING_NRFLAG      16
#define TIF_MEMDIE              17      

#define _TIF_SYSCALL_TRACE      (1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME      (1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING         (1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED       (1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP         (1 << TIF_SINGLESTEP)
#define _TIF_IRET               (1 << TIF_IRET)
#define _TIF_RESTORE_SIGMASK    (1 << TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG     (1 << TIF_POLLING_NRFLAG)

#define _TIF_WORK_MASK          (0x0000FFFF & ~_TIF_SYSCALL_TRACE)

#define _TIF_ALLWORK_MASK       0x0000FFFF

#endif 

#endif
