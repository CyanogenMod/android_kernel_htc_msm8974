/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 */

#ifndef _ASM_X86_STACKTRACE_H
#define _ASM_X86_STACKTRACE_H

#include <linux/uaccess.h>
#include <linux/ptrace.h>

extern int kstack_depth_to_print;

struct thread_info;
struct stacktrace_ops;

typedef unsigned long (*walk_stack_t)(struct thread_info *tinfo,
				      unsigned long *stack,
				      unsigned long bp,
				      const struct stacktrace_ops *ops,
				      void *data,
				      unsigned long *end,
				      int *graph);

extern unsigned long
print_context_stack(struct thread_info *tinfo,
		    unsigned long *stack, unsigned long bp,
		    const struct stacktrace_ops *ops, void *data,
		    unsigned long *end, int *graph);

extern unsigned long
print_context_stack_bp(struct thread_info *tinfo,
		       unsigned long *stack, unsigned long bp,
		       const struct stacktrace_ops *ops, void *data,
		       unsigned long *end, int *graph);


struct stacktrace_ops {
	void (*address)(void *data, unsigned long address, int reliable);
	
	int (*stack)(void *data, char *name);
	walk_stack_t	walk_stack;
};

void dump_trace(struct task_struct *tsk, struct pt_regs *regs,
		unsigned long *stack, unsigned long bp,
		const struct stacktrace_ops *ops, void *data);

#ifdef CONFIG_X86_32
#define STACKSLOTS_PER_LINE 8
#define get_bp(bp) asm("movl %%ebp, %0" : "=r" (bp) :)
#else
#define STACKSLOTS_PER_LINE 4
#define get_bp(bp) asm("movq %%rbp, %0" : "=r" (bp) :)
#endif

#ifdef CONFIG_FRAME_POINTER
static inline unsigned long
stack_frame(struct task_struct *task, struct pt_regs *regs)
{
	unsigned long bp;

	if (regs)
		return regs->bp;

	if (task == current) {
		
		get_bp(bp);
		return bp;
	}

	
	return *(unsigned long *)task->thread.sp;
}
#else
static inline unsigned long
stack_frame(struct task_struct *task, struct pt_regs *regs)
{
	return 0;
}
#endif

extern void
show_trace_log_lvl(struct task_struct *task, struct pt_regs *regs,
		   unsigned long *stack, unsigned long bp, char *log_lvl);

extern void
show_stack_log_lvl(struct task_struct *task, struct pt_regs *regs,
		   unsigned long *sp, unsigned long bp, char *log_lvl);

extern unsigned int code_bytes;

struct stack_frame {
	struct stack_frame *next_frame;
	unsigned long return_address;
};

struct stack_frame_ia32 {
    u32 next_frame;
    u32 return_address;
};

static inline unsigned long caller_frame_pointer(void)
{
	struct stack_frame *frame;

	get_bp(frame);

#ifdef CONFIG_FRAME_POINTER
	frame = frame->next_frame;
#endif

	return (unsigned long)frame;
}

#endif 
