/*
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_THREAD_INFO_H
#define _ASM_MICROBLAZE_THREAD_INFO_H

#ifdef __KERNEL__

#define THREAD_SHIFT		13
#define THREAD_SIZE		(1 << THREAD_SHIFT)
#define THREAD_SIZE_ORDER	1

#ifndef __ASSEMBLY__
# include <linux/types.h>
# include <asm/processor.h>


struct cpu_context {
	__u32	r1; 
	__u32	r2;
	
	__u32	r13;
	__u32	r14;
	__u32	r15;
	__u32	r16;
	__u32	r17;
	__u32	r18;
	
	__u32	r19;
	__u32	r20;
	__u32	r21;
	__u32	r22;
	__u32	r23;
	__u32	r24;
	__u32	r25;
	__u32	r26;
	__u32	r27;
	__u32	r28;
	__u32	r29;
	__u32	r30;
	
	
	__u32	msr;
	__u32	ear;
	__u32	esr;
	__u32	fsr;
};

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_info {
	struct task_struct	*task; 
	struct exec_domain	*exec_domain; 
	unsigned long		flags; 
	unsigned long		status; 
	__u32			cpu; 
	__s32			preempt_count; 
	mm_segment_t		addr_limit; 
	struct restart_block	restart_block;

	struct cpu_context	cpu_context;
};

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
	register unsigned long sp asm("r1");

	return (struct thread_info *)(sp & ~(THREAD_SIZE-1));
}

#endif 

#define PREEMPT_ACTIVE		0x10000000

#define TIF_SYSCALL_TRACE	0 
#define TIF_NOTIFY_RESUME	1 
#define TIF_SIGPENDING		2 
#define TIF_NEED_RESCHED	3 
#define TIF_SINGLESTEP		4
#define TIF_IRET		5 
#define TIF_MEMDIE		6	
#define TIF_SYSCALL_AUDIT	9       
#define TIF_SECCOMP		10      

#define TIF_POLLING_NRFLAG	16

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_IRET		(1 << TIF_IRET)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)

#define _TIF_WORK_SYSCALL_MASK  (_TIF_SYSCALL_TRACE | _TIF_SINGLESTEP | \
				 _TIF_SYSCALL_AUDIT | _TIF_SECCOMP)

#define _TIF_WORK_MASK		0x0000FFFE

#define _TIF_ALLWORK_MASK	0x0000FFFF

#define TS_USEDFPU		0x0001
#define TS_RESTORE_SIGMASK	0x0002

#ifndef __ASSEMBLY__
#define HAVE_SET_RESTORE_SIGMASK 1
static inline void set_restore_sigmask(void)
{
	struct thread_info *ti = current_thread_info();
	ti->status |= TS_RESTORE_SIGMASK;
	set_bit(TIF_SIGPENDING, (unsigned long *)&ti->flags);
}
#endif

#endif 
#endif 
