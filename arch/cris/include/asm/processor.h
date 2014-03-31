/*
 * include/asm-cris/processor.h
 *
 * Copyright (C) 2000, 2001 Axis Communications AB
 *
 * Authors:         Bjorn Wesen        Initial version
 *
 */

#ifndef __ASM_CRIS_PROCESSOR_H
#define __ASM_CRIS_PROCESSOR_H

#include <asm/page.h>
#include <asm/ptrace.h>
#include <arch/processor.h>
#include <arch/system.h>

struct task_struct;

#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP

#define TASK_UNMAPPED_BASE      (PAGE_ALIGN(TASK_SIZE / 3))


#define THREAD_SIZE       PAGE_SIZE
#define KERNEL_STACK_SIZE PAGE_SIZE


#define user_regs(thread_info) (((struct pt_regs *)((unsigned long)(thread_info) + THREAD_SIZE)) - 1)


#define task_pt_regs(task) user_regs(task_thread_info(task))
#define current_regs() task_pt_regs(current)

static inline void prepare_to_copy(struct task_struct *tsk)
{
}

extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

unsigned long get_wchan(struct task_struct *p);

#define KSTK_ESP(tsk)   ((tsk) == current ? rdusp() : (tsk)->thread.usp)

extern unsigned long thread_saved_pc(struct task_struct *tsk);

static inline void release_thread(struct task_struct *dead_task)
{
        
}

#define init_stack      (init_thread_union.stack)

#define cpu_relax()     barrier()

#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

void default_idle(void);

#endif 
