/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 95, 96, 97, 98, 99, 2000 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_PTRACE_H
#define _ASM_PTRACE_H

#define FPR_BASE	32
#define PC		64
#define CAUSE		65
#define BADVADDR	66
#define MMHI		67
#define MMLO		68
#define FPC_CSR		69
#define FPC_EIR		70
#define DSP_BASE	71		
#define DSP_CONTROL	77
#define ACX		78

struct pt_regs {
#ifdef CONFIG_32BIT
	
	unsigned long pad0[6];
#endif

	
	unsigned long regs[32];

	
	unsigned long cp0_status;
	unsigned long hi;
	unsigned long lo;
#ifdef CONFIG_CPU_HAS_SMARTMIPS
	unsigned long acx;
#endif
	unsigned long cp0_badvaddr;
	unsigned long cp0_cause;
	unsigned long cp0_epc;
#ifdef CONFIG_MIPS_MT_SMTC
	unsigned long cp0_tcstatus;
#endif 
#ifdef CONFIG_CPU_CAVIUM_OCTEON
	unsigned long long mpl[3];        
	unsigned long long mtp[3];        
#endif
} __attribute__ ((aligned (8)));

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13
#define PTRACE_GETFPREGS		14
#define PTRACE_SETFPREGS		15

#define PTRACE_OLDSETOPTIONS	21

#define PTRACE_GET_THREAD_AREA	25
#define PTRACE_SET_THREAD_AREA	26

#define PTRACE_PEEKTEXT_3264	0xc0
#define PTRACE_PEEKDATA_3264	0xc1
#define PTRACE_POKETEXT_3264	0xc2
#define PTRACE_POKEDATA_3264	0xc3
#define PTRACE_GET_THREAD_AREA_3264	0xc4

enum pt_watch_style {
	pt_watch_style_mips32,
	pt_watch_style_mips64
};
struct mips32_watch_regs {
	unsigned int watchlo[8];
	
	unsigned short watchhi[8];
	unsigned short watch_masks[8];
	
	unsigned int num_valid;
} __attribute__((aligned(8)));

struct mips64_watch_regs {
	unsigned long long watchlo[8];
	unsigned short watchhi[8];
	unsigned short watch_masks[8];
	unsigned int num_valid;
} __attribute__((aligned(8)));

struct pt_watch_regs {
	enum pt_watch_style style;
	union {
		struct mips32_watch_regs mips32;
		struct mips64_watch_regs mips64;
	};
};

#define PTRACE_GET_WATCH_REGS	0xd0
#define PTRACE_SET_WATCH_REGS	0xd1

#ifdef __KERNEL__

#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <asm/isadep.h>

struct task_struct;

extern int ptrace_getregs(struct task_struct *child, __s64 __user *data);
extern int ptrace_setregs(struct task_struct *child, __s64 __user *data);

extern int ptrace_getfpregs(struct task_struct *child, __u32 __user *data);
extern int ptrace_setfpregs(struct task_struct *child, __u32 __user *data);

extern int ptrace_get_watch_regs(struct task_struct *child,
	struct pt_watch_regs __user *addr);
extern int ptrace_set_watch_regs(struct task_struct *child,
	struct pt_watch_regs __user *addr);

#define user_mode(regs) (((regs)->cp0_status & KU_MASK) == KU_USER)

static inline int is_syscall_success(struct pt_regs *regs)
{
	return !regs->regs[7];
}

static inline long regs_return_value(struct pt_regs *regs)
{
	if (is_syscall_success(regs))
		return regs->regs[2];
	else
		return -regs->regs[2];
}

#define instruction_pointer(regs) ((regs)->cp0_epc)
#define profile_pc(regs) instruction_pointer(regs)

extern asmlinkage void syscall_trace_enter(struct pt_regs *regs);
extern asmlinkage void syscall_trace_leave(struct pt_regs *regs);

extern void die(const char *, struct pt_regs *) __noreturn;

static inline void die_if_kernel(const char *str, struct pt_regs *regs)
{
	if (unlikely(!user_mode(regs)))
		die(str, regs);
}

#endif

#endif 
