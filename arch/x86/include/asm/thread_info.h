/* thread_info.h: low-level thread information
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_X86_THREAD_INFO_H
#define _ASM_X86_THREAD_INFO_H

#include <linux/compiler.h>
#include <asm/page.h>
#include <asm/types.h>

#ifndef __ASSEMBLY__
struct task_struct;
struct exec_domain;
#include <asm/processor.h>
#include <asm/ftrace.h>
#include <linux/atomic.h>

struct thread_info {
	struct task_struct	*task;		
	struct exec_domain	*exec_domain;	
	__u32			flags;		
	__u32			status;		
	__u32			cpu;		
	int			preempt_count;	
	mm_segment_t		addr_limit;
	struct restart_block    restart_block;
	void __user		*sysenter_return;
#ifdef CONFIG_X86_32
	unsigned long           previous_esp;   
	__u8			supervisor_stack[0];
#endif
	unsigned int		sig_on_uaccess_error:1;
	unsigned int		uaccess_err:1;	
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

#else 

#include <asm/asm-offsets.h>

#endif

#define TIF_SYSCALL_TRACE	0	
#define TIF_NOTIFY_RESUME	1	
#define TIF_SIGPENDING		2	
#define TIF_NEED_RESCHED	3	
#define TIF_SINGLESTEP		4	
#define TIF_IRET		5	
#define TIF_SYSCALL_EMU		6	
#define TIF_SYSCALL_AUDIT	7	
#define TIF_SECCOMP		8	
#define TIF_MCE_NOTIFY		10	
#define TIF_USER_RETURN_NOTIFY	11	
#define TIF_NOTSC		16	
#define TIF_IA32		17	
#define TIF_FORK		18	
#define TIF_MEMDIE		20	
#define TIF_DEBUG		21	
#define TIF_IO_BITMAP		22	
#define TIF_FORCED_TF		24	
#define TIF_BLOCKSTEP		25	
#define TIF_LAZY_MMU_UPDATES	27	
#define TIF_SYSCALL_TRACEPOINT	28	
#define TIF_ADDR32		29	
#define TIF_X32			30	

#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_IRET		(1 << TIF_IRET)
#define _TIF_SYSCALL_EMU	(1 << TIF_SYSCALL_EMU)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_MCE_NOTIFY		(1 << TIF_MCE_NOTIFY)
#define _TIF_USER_RETURN_NOTIFY	(1 << TIF_USER_RETURN_NOTIFY)
#define _TIF_NOTSC		(1 << TIF_NOTSC)
#define _TIF_IA32		(1 << TIF_IA32)
#define _TIF_FORK		(1 << TIF_FORK)
#define _TIF_DEBUG		(1 << TIF_DEBUG)
#define _TIF_IO_BITMAP		(1 << TIF_IO_BITMAP)
#define _TIF_FORCED_TF		(1 << TIF_FORCED_TF)
#define _TIF_BLOCKSTEP		(1 << TIF_BLOCKSTEP)
#define _TIF_LAZY_MMU_UPDATES	(1 << TIF_LAZY_MMU_UPDATES)
#define _TIF_SYSCALL_TRACEPOINT	(1 << TIF_SYSCALL_TRACEPOINT)
#define _TIF_ADDR32		(1 << TIF_ADDR32)
#define _TIF_X32		(1 << TIF_X32)

#define _TIF_WORK_SYSCALL_ENTRY	\
	(_TIF_SYSCALL_TRACE | _TIF_SYSCALL_EMU | _TIF_SYSCALL_AUDIT |	\
	 _TIF_SECCOMP | _TIF_SINGLESTEP | _TIF_SYSCALL_TRACEPOINT)

#define _TIF_WORK_SYSCALL_EXIT	\
	(_TIF_SYSCALL_TRACE | _TIF_SYSCALL_AUDIT | _TIF_SINGLESTEP |	\
	 _TIF_SYSCALL_TRACEPOINT)

#define _TIF_WORK_MASK							\
	(0x0000FFFF &							\
	 ~(_TIF_SYSCALL_TRACE|_TIF_SYSCALL_AUDIT|			\
	   _TIF_SINGLESTEP|_TIF_SECCOMP|_TIF_SYSCALL_EMU))

#define _TIF_ALLWORK_MASK						\
	((0x0000FFFF & ~_TIF_SECCOMP) | _TIF_SYSCALL_TRACEPOINT)

#define _TIF_DO_NOTIFY_MASK						\
	(_TIF_SIGPENDING | _TIF_MCE_NOTIFY | _TIF_NOTIFY_RESUME |	\
	 _TIF_USER_RETURN_NOTIFY)

#define _TIF_WORK_CTXSW							\
	(_TIF_IO_BITMAP|_TIF_NOTSC|_TIF_BLOCKSTEP)

#define _TIF_WORK_CTXSW_PREV (_TIF_WORK_CTXSW|_TIF_USER_RETURN_NOTIFY)
#define _TIF_WORK_CTXSW_NEXT (_TIF_WORK_CTXSW|_TIF_DEBUG)

#define PREEMPT_ACTIVE		0x10000000

#ifdef CONFIG_DEBUG_STACK_USAGE
#define THREAD_FLAGS (GFP_KERNEL | __GFP_NOTRACK | __GFP_ZERO)
#else
#define THREAD_FLAGS (GFP_KERNEL | __GFP_NOTRACK)
#endif

#define __HAVE_ARCH_THREAD_INFO_ALLOCATOR

#define alloc_thread_info_node(tsk, node)				\
({									\
	struct page *page = alloc_pages_node(node, THREAD_FLAGS,	\
					     THREAD_ORDER);		\
	struct thread_info *ret = page ? page_address(page) : NULL;	\
									\
	ret;								\
})

#ifdef CONFIG_X86_32

#define STACK_WARN	(THREAD_SIZE/8)
#ifndef __ASSEMBLY__


register unsigned long current_stack_pointer asm("esp") __used;

static inline struct thread_info *current_thread_info(void)
{
	return (struct thread_info *)
		(current_stack_pointer & ~(THREAD_SIZE - 1));
}

#else 

#define GET_THREAD_INFO(reg)	 \
	movl $-THREAD_SIZE, reg; \
	andl %esp, reg

#define GET_THREAD_INFO_WITH_ESP(reg) \
	andl $-THREAD_SIZE, reg

#endif

#else 

#include <asm/percpu.h>
#define KERNEL_STACK_OFFSET (5*8)

#ifndef __ASSEMBLY__
DECLARE_PER_CPU(unsigned long, kernel_stack);

static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	ti = (void *)(percpu_read_stable(kernel_stack) +
		      KERNEL_STACK_OFFSET - THREAD_SIZE);
	return ti;
}

#else 

#define GET_THREAD_INFO(reg) \
	movq PER_CPU_VAR(kernel_stack),reg ; \
	subq $(THREAD_SIZE-KERNEL_STACK_OFFSET),reg

#define THREAD_INFO(reg, off) KERNEL_STACK_OFFSET+(off)-THREAD_SIZE(reg)

#endif

#endif 

#define TS_COMPAT		0x0002	
#define TS_POLLING		0x0004	
#define TS_RESTORE_SIGMASK	0x0008	

#define tsk_is_polling(t) (task_thread_info(t)->status & TS_POLLING)

#ifndef __ASSEMBLY__
#define HAVE_SET_RESTORE_SIGMASK	1
static inline void set_restore_sigmask(void)
{
	struct thread_info *ti = current_thread_info();
	ti->status |= TS_RESTORE_SIGMASK;
	set_bit(TIF_SIGPENDING, (unsigned long *)&ti->flags);
}

static inline bool is_ia32_task(void)
{
#ifdef CONFIG_X86_32
	return true;
#endif
#ifdef CONFIG_IA32_EMULATION
	if (current_thread_info()->status & TS_COMPAT)
		return true;
#endif
	return false;
}
#endif	

#ifndef __ASSEMBLY__
extern void arch_task_cache_init(void);
extern void free_thread_info(struct thread_info *ti);
extern int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);
#define arch_task_cache_init arch_task_cache_init
#endif
#endif 
