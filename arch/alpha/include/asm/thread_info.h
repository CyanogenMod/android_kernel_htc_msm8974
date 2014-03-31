#ifndef _ALPHA_THREAD_INFO_H
#define _ALPHA_THREAD_INFO_H

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/types.h>
#include <asm/hwrpb.h>
#endif

#ifndef __ASSEMBLY__
struct thread_info {
	struct pcb_struct	pcb;		

	struct task_struct	*task;		
	unsigned int		flags;		
	unsigned int		ieee_state;	

	struct exec_domain	*exec_domain;	
	mm_segment_t		addr_limit;	
	unsigned		cpu;		
	int			preempt_count; 

	int bpt_nsaved;
	unsigned long bpt_addr[2];		
	unsigned int bpt_insn[2];

	struct restart_block	restart_block;
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.addr_limit	= KERNEL_DS,		\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.restart_block = {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

register struct thread_info *__current_thread_info __asm__("$8");
#define current_thread_info()  __current_thread_info

#endif 

#define THREAD_SIZE_ORDER 1
#define THREAD_SIZE (2*PAGE_SIZE)

#define PREEMPT_ACTIVE		0x40000000

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_POLLING_NRFLAG	8	
#define TIF_DIE_IF_KERNEL	9	
#define TIF_UAC_NOPRINT		10	
#define TIF_UAC_NOFIX		11	
#define TIF_UAC_SIGBUS		12	
#define TIF_MEMDIE		13	
#define TIF_RESTORE_SIGMASK	14	

#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)

#define _TIF_WORK_MASK		(_TIF_SIGPENDING | _TIF_NEED_RESCHED | \
				 _TIF_NOTIFY_RESUME)

#define _TIF_ALLWORK_MASK	(_TIF_WORK_MASK		\
				 | _TIF_SYSCALL_TRACE)

#define ALPHA_UAC_SHIFT		TIF_UAC_NOPRINT
#define ALPHA_UAC_MASK		(1 << TIF_UAC_NOPRINT | 1 << TIF_UAC_NOFIX | \
				 1 << TIF_UAC_SIGBUS)

#define SET_UNALIGN_CTL(task,value)	({				     \
	task_thread_info(task)->flags = ((task_thread_info(task)->flags &    \
		~ALPHA_UAC_MASK)					     \
		| (((value) << ALPHA_UAC_SHIFT)       & (1<<TIF_UAC_NOPRINT))\
		| (((value) << (ALPHA_UAC_SHIFT + 1)) & (1<<TIF_UAC_SIGBUS)) \
		| (((value) << (ALPHA_UAC_SHIFT - 1)) & (1<<TIF_UAC_NOFIX)));\
	0; })

#define GET_UNALIGN_CTL(task,value)	({				\
	put_user((task_thread_info(task)->flags & (1 << TIF_UAC_NOPRINT))\
		  >> ALPHA_UAC_SHIFT					\
		 | (task_thread_info(task)->flags & (1 << TIF_UAC_SIGBUS))\
		 >> (ALPHA_UAC_SHIFT + 1)				\
		 | (task_thread_info(task)->flags & (1 << TIF_UAC_NOFIX))\
		 >> (ALPHA_UAC_SHIFT - 1),				\
		 (int __user *)(value));				\
	})

#endif 
#endif 
