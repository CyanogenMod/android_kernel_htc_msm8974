/*
 * include/asm-xtensa/processor.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PROCESSOR_H
#define _XTENSA_PROCESSOR_H

#include <variant/core.h>
#include <platform/hardware.h>

#include <linux/compiler.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/regs.h>


#if (XCHAL_HAVE_WINDOWED != 1)
# error Linux requires the Xtensa Windowed Registers Option.
#endif

#define ARCH_SLAB_MINALIGN	XCHAL_DATA_WIDTH


#ifdef CONFIG_MMU
#define TASK_SIZE	__XTENSA_UL_CONST(0x40000000)
#else
#define TASK_SIZE	(PLATFORM_DEFAULT_MEM_START + PLATFORM_DEFAULT_MEM_SIZE)
#endif

#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP


#define EXCCAUSE_MAPPED_DEBUG	63


#define VALID_DOUBLE_EXCEPTION_ADDRESS	64

#define LOCKLEVEL 1

#define WSBITS  (XCHAL_NUM_AREGS / 4)      
#define WBBITS  (XCHAL_NUM_AREGS_LOG2 - 2) 

#ifndef __ASSEMBLY__

#define MAKE_RA_FOR_CALL(ra,ws)   (((ra) & 0x3fffffff) | (ws) << 30)

#define MAKE_PC_FROM_RA(ra,sp)    (((ra) & 0x3fffffff) | ((sp) & 0xc0000000))

typedef struct {
    unsigned long seg;
} mm_segment_t;

struct thread_struct {

	
	unsigned long ra; 
	unsigned long sp; 

	mm_segment_t current_ds;    

	

	unsigned long bad_vaddr; 
	unsigned long bad_uaddr; 
	unsigned long error_code;

	unsigned long ibreak[XCHAL_NUM_IBREAK];
	unsigned long dbreaka[XCHAL_NUM_DBREAK];
	unsigned long dbreakc[XCHAL_NUM_DBREAK];

	
	int align[0] __attribute__ ((aligned(16)));
};


#define current_text_addr()  ({ __label__ _l; _l: &&_l;})


#define TASK_UNMAPPED_BASE	(TASK_SIZE / 2)

#define INIT_THREAD  \
{									\
	ra:		0, 						\
	sp:		sizeof(init_stack) + (long) &init_stack,	\
	current_ds:	{0},						\
							\
	bad_vaddr:	0,						\
	bad_uaddr:	0,						\
	error_code:	0,						\
}


#define USER_PS_VALUE ((1 << PS_WOE_BIT) |				\
                       (1 << PS_CALLINC_SHIFT) |			\
                       (USER_RING << PS_RING_SHIFT) |			\
                       (1 << PS_UM_BIT) |				\
                       (1 << PS_EXCM_BIT))

#define start_thread(regs, new_pc, new_sp) \
	regs->pc = new_pc; \
	regs->ps = USER_PS_VALUE; \
	regs->areg[1] = new_sp; \
	regs->areg[0] = 0; \
	regs->wmask = 1; \
	regs->depc = 0; \
	regs->windowbase = 0; \
	regs->windowstart = 1;

struct task_struct;
struct mm_struct;

#define release_thread(thread) do { } while(0)

extern void prepare_to_copy(struct task_struct*);

extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define copy_segments(p, mm)	do { } while(0)
#define release_segments(mm)	do { } while(0)
#define forget_segments()	do { } while (0)

#define thread_saved_pc(tsk)	(task_pt_regs(tsk)->pc)

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)		(task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)		(task_pt_regs(tsk)->areg[1])

#define cpu_relax()  barrier()


#define WSR(v,sr) __asm__ __volatile__ ("wsr %0,"__stringify(sr) :: "a"(v));
#define RSR(v,sr) __asm__ __volatile__ ("rsr %0,"__stringify(sr) : "=a"(v));

#define set_sr(x,sr) ({unsigned int v=(unsigned int)x; WSR(v,sr);})
#define get_sr(sr) ({unsigned int v; RSR(v,sr); v; })

#endif	
#endif	
