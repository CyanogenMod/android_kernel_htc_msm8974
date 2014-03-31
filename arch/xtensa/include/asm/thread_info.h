/*
 * include/asm-xtensa/thread_info.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_THREAD_INFO_H
#define _XTENSA_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
# include <asm/processor.h>
#endif


#ifndef __ASSEMBLY__

#if XTENSA_HAVE_COPROCESSORS

typedef struct xtregs_coprocessor {
	xtregs_cp0_t cp0;
	xtregs_cp1_t cp1;
	xtregs_cp2_t cp2;
	xtregs_cp3_t cp3;
	xtregs_cp4_t cp4;
	xtregs_cp5_t cp5;
	xtregs_cp6_t cp6;
	xtregs_cp7_t cp7;
} xtregs_coprocessor_t;

#endif

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	unsigned long		status;		
	__u32			cpu;		
	__s32			preempt_count;	

	mm_segment_t		addr_limit;	
	struct restart_block    restart_block;

	unsigned long		cpenable;

	
#if XTENSA_HAVE_COPROCESSORS
	xtregs_coprocessor_t	xtregs_cp;
#endif
	xtregs_user_t		xtregs_user;
};

#else 

#define TI_TASK		 0x00000000
#define TI_EXEC_DOMAIN	 0x00000004
#define TI_FLAGS	 0x00000008
#define TI_STATUS	 0x0000000C
#define TI_CPU		 0x00000010
#define TI_PRE_COUNT	 0x00000014
#define TI_ADDR_LIMIT	 0x00000018
#define TI_RESTART_BLOCK 0x000001C

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

static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	 __asm__("extui %0,a1,0,13\n\t"
	         "xor %0, a1, %0" : "=&r" (ti) : );
	return ti;
}

#else 

#define GET_THREAD_INFO(reg,sp) \
	extui reg, sp, 0, 13; \
	xor   reg, sp, reg
#endif


#define TIF_SYSCALL_TRACE	0	
#define TIF_SIGPENDING		1	
#define TIF_NEED_RESCHED	2	
#define TIF_SINGLESTEP		3	
#define TIF_IRET		4	
#define TIF_MEMDIE		5	
#define TIF_RESTORE_SIGMASK	6	
#define TIF_POLLING_NRFLAG	16	

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_IRET		(1<<TIF_IRET)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)

#define _TIF_WORK_MASK		0x0000FFFE	
#define _TIF_ALLWORK_MASK	0x0000FFFF	

#define TS_USEDFPU		0x0001	

#define THREAD_SIZE 8192	
#define THREAD_SIZE_ORDER 1

#endif	
#endif	
