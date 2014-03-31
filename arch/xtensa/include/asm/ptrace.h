/*
 * include/asm-xtensa/ptrace.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PTRACE_H
#define _XTENSA_PTRACE_H


#define KERNEL_STACK_SIZE (2 * PAGE_SIZE)


#define EXC_TABLE_KSTK		0x004	
#define EXC_TABLE_DOUBLE_SAVE	0x008	
#define EXC_TABLE_FIXUP		0x00c	
#define EXC_TABLE_PARAM		0x010	
#define EXC_TABLE_SYSCALL_SAVE	0x014	
#define EXC_TABLE_FAST_USER	0x100	
#define EXC_TABLE_FAST_KERNEL	0x200	
#define EXC_TABLE_DEFAULT	0x300	
#define EXC_TABLE_SIZE		0x400


#define REG_A_BASE	0x0000
#define REG_AR_BASE	0x0100
#define REG_PC		0x0020
#define REG_PS		0x02e6
#define REG_WB		0x0248
#define REG_WS		0x0249
#define REG_LBEG	0x0200
#define REG_LEND	0x0201
#define REG_LCOUNT	0x0202
#define REG_SAR		0x0203

#define SYSCALL_NR	0x00ff


#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13
#define PTRACE_GETXTREGS	18
#define PTRACE_SETXTREGS	19

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

#include <asm/coprocessor.h>

struct pt_regs {
	unsigned long pc;		
	unsigned long ps;		
	unsigned long depc;		
	unsigned long exccause;		
	unsigned long excvaddr;		
	unsigned long debugcause;	
	unsigned long wmask;		
	unsigned long lbeg;		
	unsigned long lend;		
	unsigned long lcount;		
	unsigned long sar;		
	unsigned long windowbase;	
	unsigned long windowstart;	
	unsigned long syscall;		
	unsigned long icountlevel;	
	int reserved[1];		

	
	xtregs_opt_t xtregs_opt;

	
	int align[0] __attribute__ ((aligned(16)));

	unsigned long areg[16];		
};

#include <variant/core.h>

# define arch_has_single_step()	(1)
# define task_pt_regs(tsk) ((struct pt_regs*) \
  (task_stack_page(tsk) + KERNEL_STACK_SIZE - (XCHAL_NUM_AREGS-16)*4) - 1)
# define user_mode(regs) (((regs)->ps & 0x00000020)!=0)
# define instruction_pointer(regs) ((regs)->pc)

# ifndef CONFIG_SMP
#  define profile_pc(regs) instruction_pointer(regs)
# endif

#else	

# include <asm/asm-offsets.h>
#define PT_REGS_OFFSET	  (KERNEL_STACK_SIZE - PT_USER_SIZE)

#endif	

#endif  

#endif	
