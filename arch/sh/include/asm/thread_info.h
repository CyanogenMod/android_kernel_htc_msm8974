#ifndef __ASM_SH_THREAD_INFO_H
#define __ASM_SH_THREAD_INFO_H

/* SuperH version
 * Copyright (C) 2002  Niibe Yutaka
 *
 * The copyright of original i386 version is:
 *
 *  Copyright (C) 2002  David Howells (dhowells@redhat.com)
 *  - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */
#ifdef __KERNEL__
#include <asm/page.h>

#ifndef __ASSEMBLY__
#include <asm/processor.h>

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	unsigned long		flags;		
	__u32			status;		
	__u32			cpu;
	int			preempt_count; 
	mm_segment_t		addr_limit;	
	struct restart_block	restart_block;
	unsigned long		previous_sp;	
	__u8			supervisor_stack[0];
};

#endif

#define PREEMPT_ACTIVE		0x10000000

#if defined(CONFIG_4KSTACKS)
#define THREAD_SHIFT	12
#else
#define THREAD_SHIFT	13
#endif

#define THREAD_SIZE	(1 << THREAD_SHIFT)
#define STACK_WARN	(THREAD_SIZE >> 3)

#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.status		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.addr_limit	= KERNEL_DS,		\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

register unsigned long current_stack_pointer asm("r15") __used;

static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
#if defined(CONFIG_SUPERH64)
	__asm__ __volatile__ ("getcon	cr17, %0" : "=r" (ti));
#elif defined(CONFIG_CPU_HAS_SR_RB)
	__asm__ __volatile__ ("stc	r7_bank, %0" : "=r" (ti));
#else
	unsigned long __dummy;

	__asm__ __volatile__ (
		"mov	r15, %0\n\t"
		"and	%1, %0\n\t"
		: "=&r" (ti), "=r" (__dummy)
		: "1" (~(THREAD_SIZE - 1))
		: "memory");
#endif

	return ti;
}

#if THREAD_SHIFT >= PAGE_SHIFT

#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)

#endif

extern struct thread_info *alloc_thread_info_node(struct task_struct *tsk, int node);
extern void free_thread_info(struct thread_info *ti);
extern void arch_task_cache_init(void);
#define arch_task_cache_init arch_task_cache_init
extern int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);
extern void init_thread_xstate(void);

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#endif 

#define TIF_SYSCALL_TRACE	0	
#define TIF_SIGPENDING		1	
#define TIF_NEED_RESCHED	2	
#define TIF_SINGLESTEP		4	
#define TIF_SYSCALL_AUDIT	5	
#define TIF_SECCOMP		6	
#define TIF_NOTIFY_RESUME	7	
#define TIF_SYSCALL_TRACEPOINT	8	
#define TIF_POLLING_NRFLAG	17	
#define TIF_MEMDIE		18	

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SYSCALL_TRACEPOINT	(1 << TIF_SYSCALL_TRACEPOINT)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)


#define _TIF_WORK_SYSCALL_MASK	(_TIF_SYSCALL_TRACE | _TIF_SINGLESTEP | \
				 _TIF_SYSCALL_AUDIT | _TIF_SECCOMP    | \
				 _TIF_SYSCALL_TRACEPOINT)

#define _TIF_ALLWORK_MASK	(_TIF_SYSCALL_TRACE | _TIF_SIGPENDING      | \
				 _TIF_NEED_RESCHED  | _TIF_SYSCALL_AUDIT   | \
				 _TIF_SINGLESTEP    | _TIF_NOTIFY_RESUME   | \
				 _TIF_SYSCALL_TRACEPOINT)

#define _TIF_WORK_MASK		(_TIF_ALLWORK_MASK & ~(_TIF_SYSCALL_TRACE | \
				 _TIF_SYSCALL_AUDIT | _TIF_SINGLESTEP))

#define TS_RESTORE_SIGMASK	0x0001	
#define TS_USEDFPU		0x0002	

#ifndef __ASSEMBLY__
#define HAVE_SET_RESTORE_SIGMASK	1
static inline void set_restore_sigmask(void)
{
	struct thread_info *ti = current_thread_info();
	ti->status |= TS_RESTORE_SIGMASK;
	set_bit(TIF_SIGPENDING, (unsigned long *)&ti->flags);
}
#endif	

#endif 

#endif 
