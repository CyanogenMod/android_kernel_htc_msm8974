/*
 * include/asm-h8300/processor.h
 *
 * Copyright (C) 2002 Yoshinori Sato
 *
 * Based on: linux/asm-m68nommu/processor.h
 *
 * Copyright (C) 1995 Hamish Macdonald
 */

#ifndef __ASM_H8300_PROCESSOR_H
#define __ASM_H8300_PROCESSOR_H

#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#include <linux/compiler.h>
#include <asm/segment.h>
#include <asm/fpu.h>
#include <asm/ptrace.h>
#include <asm/current.h>

static inline unsigned long rdusp(void) {
	extern unsigned int	sw_usp;
	return(sw_usp);
}

static inline void wrusp(unsigned long usp) {
	extern unsigned int	sw_usp;
	sw_usp = usp;
}

#define TASK_SIZE	(0xFFFFFFFFUL)

#ifdef __KERNEL__
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
#endif

#define TASK_UNMAPPED_BASE	0

struct thread_struct {
	unsigned long  ksp;		
	unsigned long  usp;		
	unsigned long  ccr;		
	unsigned long  esp0;            
	struct {
		unsigned short *addr;
		unsigned short inst;
	} breakinfo;
};

#define INIT_THREAD  {						\
	.ksp  = sizeof(init_stack) + (unsigned long)init_stack, \
	.usp  = 0,						\
	.ccr  = PS_S,						\
	.esp0 = 0,						\
	.breakinfo = {						\
		.addr = (unsigned short *)-1,			\
		.inst = 0					\
	}							\
}

#if defined(__H8300H__)
#define start_thread(_regs, _pc, _usp)			        \
do {							        \
  	(_regs)->pc = (_pc);				        \
	(_regs)->ccr = 0x00;	           \
	(_regs)->er5 = current->mm->start_data;	  \
	wrusp((unsigned long)(_usp) - sizeof(unsigned long)*3);	\
} while(0)
#endif
#if defined(__H8300S__)
#define start_thread(_regs, _pc, _usp)			        \
do {							        \
	(_regs)->pc = (_pc);				        \
	(_regs)->ccr = 0x00;	         \
	(_regs)->exr = 0x78;         \
	(_regs)->er5 = current->mm->start_data;	  \
	 \
	wrusp(((unsigned long)(_usp)) - 14);                    \
} while(0)
#endif

struct task_struct;

static inline void release_thread(struct task_struct *dead_task)
{
}

extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define prepare_to_copy(tsk)	do { } while (0)

static inline void exit_thread(void)
{
}

unsigned long thread_saved_pc(struct task_struct *tsk);
unsigned long get_wchan(struct task_struct *p);

#define	KSTK_EIP(tsk)	\
    ({			\
	unsigned long eip = 0;	 \
	if ((tsk)->thread.esp0 > PAGE_SIZE && \
	    MAP_NR((tsk)->thread.esp0) < max_mapnr) \
	      eip = ((struct pt_regs *) (tsk)->thread.esp0)->pc; \
	eip; })
#define	KSTK_ESP(tsk)	((tsk) == current ? rdusp() : (tsk)->thread.usp)

#define cpu_relax()    barrier()

#define HARD_RESET_NOW() ({		\
        local_irq_disable();		\
        asm("jmp @@0");			\
})

#endif
