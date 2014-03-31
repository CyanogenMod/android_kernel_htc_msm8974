#ifndef _ASM_PARISC_THREAD_INFO_H
#define _ASM_PARISC_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/special_insns.h>

struct thread_info {
	struct task_struct *task;	
	struct exec_domain *exec_domain;
	unsigned long flags;		
	mm_segment_t addr_limit;	
	__u32 cpu;			
	int preempt_count;		
	struct restart_block restart_block;
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.cpu		= 0,			\
	.addr_limit	= KERNEL_DS,		\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
  	.restart_block	= {			\
		.fn = do_no_restart_syscall	\
	}					\
}

#define init_thread_info        (init_thread_union.thread_info)
#define init_stack              (init_thread_union.stack)

#define current_thread_info()	((struct thread_info *)mfctl(30))

#endif 


#define THREAD_SIZE_ORDER            2
#define THREAD_SIZE             (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_SHIFT            (PAGE_SHIFT + THREAD_SIZE_ORDER)

#define PREEMPT_ACTIVE_BIT	28
#define PREEMPT_ACTIVE		(1 << PREEMPT_ACTIVE_BIT)

#define TIF_SYSCALL_TRACE	0	
#define TIF_SIGPENDING		1	
#define TIF_NEED_RESCHED	2	
#define TIF_POLLING_NRFLAG	3	
#define TIF_32BIT               4       
#define TIF_MEMDIE		5	
#define TIF_RESTORE_SIGMASK	6	
#define TIF_NOTIFY_RESUME	8	
#define TIF_SINGLESTEP		9	
#define TIF_BLOCKSTEP		10	

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_32BIT		(1 << TIF_32BIT)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_BLOCKSTEP		(1 << TIF_BLOCKSTEP)

#define _TIF_USER_WORK_MASK     (_TIF_SIGPENDING | _TIF_NOTIFY_RESUME | \
                                 _TIF_NEED_RESCHED | _TIF_RESTORE_SIGMASK)

#endif 

#endif 
