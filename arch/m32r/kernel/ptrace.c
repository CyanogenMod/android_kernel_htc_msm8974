/*
 * linux/arch/m32r/kernel/ptrace.c
 *
 * Copyright (C) 2002  Hirokazu Takata, Takeo Takahashi
 * Copyright (C) 2004  Hirokazu Takata, Kei Sakamoto
 *
 * Original x86 implementation:
 *	By Ross Biro 1/23/92
 *	edited by Linus Torvalds
 *
 * Some code taken from sh version:
 *   Copyright (C) 1999, 2000  Kaz Kojima & Niibe Yutaka
 * Some code taken from arm version:
 *   Copyright (C) 2000 Russell King
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/string.h>
#include <linux/signal.h>

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/mmu_context.h>

static inline unsigned long int
get_stack_long(struct task_struct *task, int offset)
{
	unsigned long *stack;

	stack = (unsigned long *)task_pt_regs(task);

	return stack[offset];
}

static inline int
put_stack_long(struct task_struct *task, int offset, unsigned long data)
{
	unsigned long *stack;

	stack = (unsigned long *)task_pt_regs(task);
	stack[offset] = data;

	return 0;
}

static int reg_offset[] = {
	PT_R0, PT_R1, PT_R2, PT_R3, PT_R4, PT_R5, PT_R6, PT_R7,
	PT_R8, PT_R9, PT_R10, PT_R11, PT_R12, PT_FP, PT_LR, PT_SPU,
};

static int ptrace_read_user(struct task_struct *tsk, unsigned long off,
			    unsigned long __user *data)
{
	unsigned long tmp;
#ifndef NO_FPU
	struct user * dummy = NULL;
#endif

	if ((off & 3) || off > sizeof(struct user) - 3)
		return -EIO;

	off >>= 2;
	switch (off) {
	case PT_EVB:
		__asm__ __volatile__ (
			"mvfc	%0, cr5 \n\t"
	 		: "=r" (tmp)
		);
		break;
	case PT_CBR: {
			unsigned long psw;
			psw = get_stack_long(tsk, PT_PSW);
			tmp = ((psw >> 8) & 1);
		}
		break;
	case PT_PSW: {
			unsigned long psw, bbpsw;
			psw = get_stack_long(tsk, PT_PSW);
			bbpsw = get_stack_long(tsk, PT_BBPSW);
			tmp = ((psw >> 8) & 0xff) | ((bbpsw & 0xff) << 8);
		}
		break;
	case PT_PC:
		tmp = get_stack_long(tsk, PT_BPC);
		break;
	case PT_BPC:
		off = PT_BBPC;
		
	default:
		if (off < (sizeof(struct pt_regs) >> 2))
			tmp = get_stack_long(tsk, off);
#ifndef NO_FPU
		else if (off >= (long)(&dummy->fpu >> 2) &&
			 off < (long)(&dummy->u_fpvalid >> 2)) {
			if (!tsk_used_math(tsk)) {
				if (off == (long)(&dummy->fpu.fpscr >> 2))
					tmp = FPSCR_INIT;
				else
					tmp = 0;
			} else
				tmp = ((long *)(&tsk->thread.fpu >> 2))
					[off - (long)&dummy->fpu];
		} else if (off == (long)(&dummy->u_fpvalid >> 2))
			tmp = !!tsk_used_math(tsk);
#endif 
		else
			tmp = 0;
	}

	return put_user(tmp, data);
}

static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
			     unsigned long data)
{
	int ret = -EIO;
#ifndef NO_FPU
	struct user * dummy = NULL;
#endif

	if ((off & 3) || off > sizeof(struct user) - 3)
		return -EIO;

	off >>= 2;
	switch (off) {
	case PT_EVB:
	case PT_BPC:
	case PT_SPI:
		
		ret = 0;
		break;
	case PT_PSW:
	case PT_CBR: {
			
			unsigned long psw;
			psw = get_stack_long(tsk, PT_PSW);
			psw = (psw & ~0x100) | ((data & 1) << 8);
			ret = put_stack_long(tsk, PT_PSW, psw);
		}
		break;
	case PT_PC:
		off = PT_BPC;
		data &= ~1;
		
	default:
		if (off < (sizeof(struct pt_regs) >> 2))
			ret = put_stack_long(tsk, off, data);
#ifndef NO_FPU
		else if (off >= (long)(&dummy->fpu >> 2) &&
			 off < (long)(&dummy->u_fpvalid >> 2)) {
			set_stopped_child_used_math(tsk);
			((long *)&tsk->thread.fpu)
				[off - (long)&dummy->fpu] = data;
			ret = 0;
		} else if (off == (long)(&dummy->u_fpvalid >> 2)) {
			conditional_stopped_child_used_math(data, tsk);
			ret = 0;
		}
#endif 
		break;
	}

	return ret;
}

static int ptrace_getregs(struct task_struct *tsk, void __user *uregs)
{
	struct pt_regs *regs = task_pt_regs(tsk);

	return copy_to_user(uregs, regs, sizeof(struct pt_regs)) ? -EFAULT : 0;
}

static int ptrace_setregs(struct task_struct *tsk, void __user *uregs)
{
	struct pt_regs newregs;
	int ret;

	ret = -EFAULT;
	if (copy_from_user(&newregs, uregs, sizeof(struct pt_regs)) == 0) {
		struct pt_regs *regs = task_pt_regs(tsk);
		*regs = newregs;
		ret = 0;
	}

	return ret;
}


static inline int
check_condition_bit(struct task_struct *child)
{
	return (int)((get_stack_long(child, PT_PSW) >> 8) & 1);
}

static int
check_condition_src(unsigned long op, unsigned long regno1,
		    unsigned long regno2, struct task_struct *child)
{
	unsigned long reg1, reg2;

	reg2 = get_stack_long(child, reg_offset[regno2]);

	switch (op) {
	case 0x0: 
		reg1 = get_stack_long(child, reg_offset[regno1]);
		return reg1 == reg2;
	case 0x1: 
		reg1 = get_stack_long(child, reg_offset[regno1]);
		return reg1 != reg2;
	case 0x8: 
		return reg2 == 0;
	case 0x9: 
		return reg2 != 0;
	case 0xa: 
		return (int)reg2 < 0;
	case 0xb: 
		return (int)reg2 >= 0;
	case 0xc: 
		return (int)reg2 <= 0;
	case 0xd: 
		return (int)reg2 > 0;
	default:
		
		return 0;
	}
}

static void
compute_next_pc_for_16bit_insn(unsigned long insn, unsigned long pc,
			       unsigned long *next_pc,
			       struct task_struct *child)
{
	unsigned long op, op2, op3;
	unsigned long disp;
	unsigned long regno;
	int parallel = 0;

	if (insn & 0x00008000)
		parallel = 1;
	if (pc & 3)
		insn &= 0x7fff;	
	else
		insn >>= 16;	

	op = (insn >> 12) & 0xf;
	op2 = (insn >> 8) & 0xf;
	op3 = (insn >> 4) & 0xf;

	if (op == 0x7) {
		switch (op2) {
		case 0xd: 
		case 0x9: 
			if (!check_condition_bit(child)) {
				disp = (long)(insn << 24) >> 22;
				*next_pc = (pc & ~0x3) + disp;
				return;
			}
			break;
		case 0x8: 
		case 0xc: 
			if (check_condition_bit(child)) {
				disp = (long)(insn << 24) >> 22;
				*next_pc = (pc & ~0x3) + disp;
				return;
			}
			break;
		case 0xe: 
		case 0xf: 
			disp = (long)(insn << 24) >> 22;
			*next_pc = (pc & ~0x3) + disp;
			return;
			break;
		}
	} else if (op == 0x1) {
		switch (op2) {
		case 0x0:
			if (op3 == 0xf) { 
#if 1
				
#else
 				
				unsigned long evb;
				unsigned long trapno;
				trapno = insn & 0xf;
				__asm__ __volatile__ (
					"mvfc %0, cr5\n"
		 			:"=r"(evb)
		 			:
				);
				*next_pc = evb + (trapno << 2);
				return;
#endif
			} else if (op3 == 0xd) { 
				*next_pc = get_stack_long(child, PT_BPC);
				return;
			}
			break;
		case 0xc: 
			if (op3 == 0xc && check_condition_bit(child)) {
				regno = insn & 0xf;
				*next_pc = get_stack_long(child,
							  reg_offset[regno]);
				return;
			}
			break;
		case 0xd: 
			if (op3 == 0xc && !check_condition_bit(child)) {
				regno = insn & 0xf;
				*next_pc = get_stack_long(child,
							  reg_offset[regno]);
				return;
			}
			break;
		case 0xe: 
		case 0xf: 
			if (op3 == 0xc) { 
				regno = insn & 0xf;
				*next_pc = get_stack_long(child,
							  reg_offset[regno]);
				return;
			}
			break;
		}
	}
	if (parallel)
		*next_pc = pc + 4;
	else
		*next_pc = pc + 2;
}

static void
compute_next_pc_for_32bit_insn(unsigned long insn, unsigned long pc,
			       unsigned long *next_pc,
			       struct task_struct *child)
{
	unsigned long op;
	unsigned long op2;
	unsigned long disp;
	unsigned long regno1, regno2;

	op = (insn >> 28) & 0xf;
	if (op == 0xf) { 	
		op2 = (insn >> 24) & 0xf;
		switch (op2) {
		case 0xd:	
		case 0x9:	
			if (!check_condition_bit(child)) {
				disp = (long)(insn << 8) >> 6;
				*next_pc = (pc & ~0x3) + disp;
				return;
			}
			break;
		case 0x8:	
		case 0xc:	
			if (check_condition_bit(child)) {
				disp = (long)(insn << 8) >> 6;
				*next_pc = (pc & ~0x3) + disp;
				return;
			}
			break;
		case 0xe:	
		case 0xf:	
			disp = (long)(insn << 8) >> 6;
			*next_pc = (pc & ~0x3) + disp;
			return;
		}
	} else if (op == 0xb) { 
		op2 = (insn >> 20) & 0xf;
		switch (op2) {
		case 0x0: 
		case 0x1: 
		case 0x8: 
		case 0x9: 
		case 0xa: 
		case 0xb: 
		case 0xc: 
		case 0xd: 
			regno1 = ((insn >> 24) & 0xf);
			regno2 = ((insn >> 16) & 0xf);
			if (check_condition_src(op2, regno1, regno2, child)) {
				disp = (long)(insn << 16) >> 14;
				*next_pc = (pc & ~0x3) + disp;
				return;
			}
			break;
		}
	}
	*next_pc = pc + 4;
}

static inline void
compute_next_pc(unsigned long insn, unsigned long pc,
		unsigned long *next_pc, struct task_struct *child)
{
	if (insn & 0x80000000)
		compute_next_pc_for_32bit_insn(insn, pc, next_pc, child);
	else
		compute_next_pc_for_16bit_insn(insn, pc, next_pc, child);
}

static int
register_debug_trap(struct task_struct *child, unsigned long next_pc,
	unsigned long next_insn, unsigned long *code)
{
	struct debug_trap *p = &child->thread.debug_trap;
	unsigned long addr = next_pc & ~3;

	if (p->nr_trap == MAX_TRAPS) {
		printk("kernel BUG at %s %d: p->nr_trap = %d\n",
					__FILE__, __LINE__, p->nr_trap);
		return -1;
	}
	p->addr[p->nr_trap] = addr;
	p->insn[p->nr_trap] = next_insn;
	p->nr_trap++;
	if (next_pc & 3) {
		*code = (next_insn & 0xffff0000) | 0x10f1;
		
	} else {
		if ((next_insn & 0x80000000) || (next_insn & 0x8000)) {
			*code = 0x10f17000;
			
		} else {
			*code = (next_insn & 0xffff) | 0x10f10000;
			
		}
	}
	return 0;
}

static int
unregister_debug_trap(struct task_struct *child, unsigned long addr,
		      unsigned long *code)
{
	struct debug_trap *p = &child->thread.debug_trap;
        int i;

	
	for (i = 0; i < p->nr_trap; i++) {
		if (p->addr[i] == addr)
			break;
	}
	if (i >= p->nr_trap) {
		return 0;
	}

	
	*code = p->insn[i];

	
	while (i < p->nr_trap - 1) {
		p->insn[i] = p->insn[i + 1];
		p->addr[i] = p->addr[i + 1];
		i++;
	}
	p->nr_trap--;
	return 1;
}

static void
unregister_all_debug_traps(struct task_struct *child)
{
	struct debug_trap *p = &child->thread.debug_trap;
	int i;

	for (i = 0; i < p->nr_trap; i++)
		access_process_vm(child, p->addr[i], &p->insn[i], sizeof(p->insn[i]), 1);
	p->nr_trap = 0;
}

static inline void
invalidate_cache(void)
{
#if defined(CONFIG_CHIP_M32700) || defined(CONFIG_CHIP_OPSP)

	_flush_cache_copyback_all();

#else	

	
	__asm__ __volatile__ (
                "ldi    r0, #-1					\n\t"
                "ldi    r1, #0					\n\t"
                "stb    r1, @r0		; cache off		\n\t"
                ";						\n\t"
                "ldi    r0, #-2					\n\t"
                "ldi    r1, #1					\n\t"
                "stb    r1, @r0		; cache invalidate	\n\t"
                ".fillinsn					\n"
                "0:						\n\t"
                "ldb    r1, @r0		; invalidate check	\n\t"
                "bnez   r1, 0b					\n\t"
                ";						\n\t"
                "ldi    r0, #-1					\n\t"
                "ldi    r1, #1					\n\t"
                "stb    r1, @r0		; cache on		\n\t"
		: : : "r0", "r1", "memory"
	);
#endif	
}

static int
embed_debug_trap(struct task_struct *child, unsigned long next_pc)
{
	unsigned long next_insn, code;
	unsigned long addr = next_pc & ~3;

	if (access_process_vm(child, addr, &next_insn, sizeof(next_insn), 0)
	    != sizeof(next_insn)) {
		return -1; 
	}

	
	if (register_debug_trap(child, next_pc, next_insn, &code)) {
		return -1; 
	}
	if (access_process_vm(child, addr, &code, sizeof(code), 1)
	    != sizeof(code)) {
		return -1; 
	}
	return 0; 
}

void
withdraw_debug_trap(struct pt_regs *regs)
{
	unsigned long addr;
	unsigned long code;

 	addr = (regs->bpc - 2) & ~3;
	regs->bpc -= 2;
	if (unregister_debug_trap(current, addr, &code)) {
	    access_process_vm(current, addr, &code, sizeof(code), 1);
	    invalidate_cache();
	}
}

void
init_debug_traps(struct task_struct *child)
{
	struct debug_trap *p = &child->thread.debug_trap;
	int i;
	p->nr_trap = 0;
	for (i = 0; i < MAX_TRAPS; i++) {
		p->addr[i] = 0;
		p->insn[i] = 0;
	}
}

void user_enable_single_step(struct task_struct *child)
{
	unsigned long next_pc;
	unsigned long pc, insn;

	clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);

	
	pc = get_stack_long(child, PT_BPC);

	if (access_process_vm(child, pc&~3, &insn, sizeof(insn), 0)
	    != sizeof(insn))
		return -EIO;

	compute_next_pc(insn, pc, &next_pc, child);
	if (next_pc & 0x80000000)
		return -EIO;

	if (embed_debug_trap(child, next_pc))
		return -EIO;

	invalidate_cache();
	return 0;
}

void user_disable_single_step(struct task_struct *child)
{
	unregister_all_debug_traps(child);
	invalidate_cache();
}

void ptrace_disable(struct task_struct *child)
{
	
}

long
arch_ptrace(struct task_struct *child, long request,
	    unsigned long addr, unsigned long data)
{
	int ret;
	unsigned long __user *datap = (unsigned long __user *) data;

	switch (request) {
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		ret = generic_ptrace_peekdata(child, addr, data);
		break;

	case PTRACE_PEEKUSR:
		ret = ptrace_read_user(child, addr, datap);
		break;

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		ret = generic_ptrace_pokedata(child, addr, data);
		if (ret == 0 && request == PTRACE_POKETEXT)
			invalidate_cache();
		break;

	case PTRACE_POKEUSR:
		ret = ptrace_write_user(child, addr, data);
		break;

	case PTRACE_GETREGS:
		ret = ptrace_getregs(child, datap);
		break;

	case PTRACE_SETREGS:
		ret = ptrace_setregs(child, datap);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

void do_syscall_trace(void)
{
	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;
	if (!(current->ptrace & PT_PTRACED))
		return;
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));

	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
