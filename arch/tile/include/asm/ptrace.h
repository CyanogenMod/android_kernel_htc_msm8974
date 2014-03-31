/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_PTRACE_H
#define _ASM_TILE_PTRACE_H

#include <arch/chip.h>
#include <arch/abi.h>

#if CHIP_WORD_SIZE() == 32
#define PTREGS_OFFSET_REG(n)    ((n)*4)
#else
#define PTREGS_OFFSET_REG(n)    ((n)*8)
#endif
#define PTREGS_OFFSET_BASE      0
#define PTREGS_OFFSET_TP        PTREGS_OFFSET_REG(53)
#define PTREGS_OFFSET_SP        PTREGS_OFFSET_REG(54)
#define PTREGS_OFFSET_LR        PTREGS_OFFSET_REG(55)
#define PTREGS_NR_GPRS          56
#define PTREGS_OFFSET_PC        PTREGS_OFFSET_REG(56)
#define PTREGS_OFFSET_EX1       PTREGS_OFFSET_REG(57)
#define PTREGS_OFFSET_FAULTNUM  PTREGS_OFFSET_REG(58)
#define PTREGS_OFFSET_ORIG_R0   PTREGS_OFFSET_REG(59)
#define PTREGS_OFFSET_FLAGS     PTREGS_OFFSET_REG(60)
#if CHIP_HAS_CMPEXCH()
#define PTREGS_OFFSET_CMPEXCH   PTREGS_OFFSET_REG(61)
#endif
#define PTREGS_SIZE             PTREGS_OFFSET_REG(64)

#ifndef __ASSEMBLY__

#ifdef __KERNEL__
typedef unsigned long pt_reg_t;
#else
typedef uint_reg_t pt_reg_t;
#endif

struct pt_regs {
	
	
	pt_reg_t regs[53];
	pt_reg_t tp;		
	pt_reg_t sp;		
	pt_reg_t lr;		

	
	pt_reg_t pc;		
	pt_reg_t ex1;		
	pt_reg_t faultnum;	
	pt_reg_t orig_r0;	
	pt_reg_t flags;		
#if !CHIP_HAS_CMPEXCH()
	pt_reg_t pad[3];
#else
	pt_reg_t cmpexch;	
	pt_reg_t pad[2];
#endif
};

#endif 

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13
#define PTRACE_GETFPREGS	14
#define PTRACE_SETFPREGS	15

#define PTRACE_O_TRACEMIGRATE	0x00010000
#define PTRACE_EVENT_MIGRATE	16
#ifdef __KERNEL__
#define PTRACE_O_MASK_TILE	(PTRACE_O_TRACEMIGRATE)
#define PT_TRACE_MIGRATE	0x00080000
#define PT_TRACE_MASK_TILE	(PT_TRACE_MIGRATE)
#endif

#ifdef __KERNEL__

#define PT_FLAGS_DISABLE_IRQ    1  
#define PT_FLAGS_CALLER_SAVES   2  
#define PT_FLAGS_RESTORE_REGS   4  

#ifndef __ASSEMBLY__

#define instruction_pointer(regs) ((regs)->pc)
#define profile_pc(regs) instruction_pointer(regs)

#define user_mode(regs) (EX1_PL((regs)->ex1) == USER_PL)

struct pt_regs *get_pt_regs(struct pt_regs *);

extern void do_syscall_trace(void);

#define arch_has_single_step()	(1)

struct single_step_state {
	
	void __user *buffer;

	union {
		int flags;
		struct {
			unsigned long is_enabled:1, update:1, update_reg:6;
		};
	};

	unsigned long orig_pc;		
	unsigned long next_pc;		
	unsigned long branch_next_pc;	
	unsigned long update_value;	
};

extern void single_step_once(struct pt_regs *regs);

extern void single_step_execve(void);

struct task_struct;

extern void send_sigtrap(struct task_struct *tsk, struct pt_regs *regs,
			 int error_code);

#ifdef __tilegx__
#define __ARCH_WANT_COMPAT_SYS_PTRACE
#endif

#endif 

#define SINGLESTEP_STATE_MASK_IS_ENABLED      0x1
#define SINGLESTEP_STATE_MASK_UPDATE          0x2
#define SINGLESTEP_STATE_TARGET_LB              2
#define SINGLESTEP_STATE_TARGET_UB              7

#endif 

#endif 
