#ifndef _ASM_M68K_THREAD_INFO_H
#define _ASM_M68K_THREAD_INFO_H

#include <asm/types.h>
#include <asm/page.h>
#include <asm/segment.h>

#if PAGE_SHIFT < 13
#ifdef CONFIG_4KSTACKS
#define THREAD_SIZE	4096
#else
#define THREAD_SIZE	8192
#endif
#else
#define THREAD_SIZE	PAGE_SIZE
#endif
#define THREAD_SIZE_ORDER	((THREAD_SIZE / PAGE_SIZE) - 1)

#ifndef __ASSEMBLY__

struct thread_info {
	struct task_struct	*task;		
	unsigned long		flags;
	struct exec_domain	*exec_domain;	
	mm_segment_t		addr_limit;	
	int			preempt_count;	
	__u32			cpu;		
	unsigned long		tp_value;	
	struct restart_block    restart_block;
};
#endif 

#define PREEMPT_ACTIVE		0x4000000

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

#define init_stack		(init_thread_union.stack)

#ifndef __ASSEMBLY__
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	__asm__(
		"move.l %%sp, %0 \n\t"
		"and.l  %1, %0"
		: "=&d"(ti)
		: "di" (~(THREAD_SIZE-1))
		);
	return ti;
}
#endif

#define init_thread_info	(init_thread_union.thread_info)

#define TIF_SIGPENDING		6	
#define TIF_NEED_RESCHED	7	
#define TIF_DELAYED_TRACE	14	
#define TIF_SYSCALL_TRACE	15	
#define TIF_MEMDIE		16	
#define TIF_RESTORE_SIGMASK	18	

#endif	
