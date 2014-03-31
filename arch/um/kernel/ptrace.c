/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include "linux/audit.h"
#include "linux/ptrace.h"
#include "linux/sched.h"
#include "asm/uaccess.h"
#include "skas_ptrace.h"



void user_enable_single_step(struct task_struct *child)
{
	child->ptrace |= PT_DTRACE;
	child->thread.singlestep_syscall = 0;

#ifdef SUBARCH_SET_SINGLESTEPPING
	SUBARCH_SET_SINGLESTEPPING(child, 1);
#endif
}

void user_disable_single_step(struct task_struct *child)
{
	child->ptrace &= ~PT_DTRACE;
	child->thread.singlestep_syscall = 0;

#ifdef SUBARCH_SET_SINGLESTEPPING
	SUBARCH_SET_SINGLESTEPPING(child, 0);
#endif
}

void ptrace_disable(struct task_struct *child)
{
	user_disable_single_step(child);
}

extern int peek_user(struct task_struct * child, long addr, long data);
extern int poke_user(struct task_struct * child, long addr, long data);

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int i, ret;
	unsigned long __user *p = (void __user *)data;
	void __user *vp = p;

	switch (request) {
	
	case PTRACE_PEEKUSR:
		ret = peek_user(child, addr, data);
		break;

	
	case PTRACE_POKEUSR:
		ret = poke_user(child, addr, data);
		break;

	case PTRACE_SYSEMU:
	case PTRACE_SYSEMU_SINGLESTEP:
		ret = -EIO;
		break;

#ifdef PTRACE_GETREGS
	case PTRACE_GETREGS: { 
		if (!access_ok(VERIFY_WRITE, p, MAX_REG_OFFSET)) {
			ret = -EIO;
			break;
		}
		for ( i = 0; i < MAX_REG_OFFSET; i += sizeof(long) ) {
			__put_user(getreg(child, i), p);
			p++;
		}
		ret = 0;
		break;
	}
#endif
#ifdef PTRACE_SETREGS
	case PTRACE_SETREGS: { 
		unsigned long tmp = 0;
		if (!access_ok(VERIFY_READ, p, MAX_REG_OFFSET)) {
			ret = -EIO;
			break;
		}
		for ( i = 0; i < MAX_REG_OFFSET; i += sizeof(long) ) {
			__get_user(tmp, p);
			putreg(child, i, tmp);
			p++;
		}
		ret = 0;
		break;
	}
#endif
	case PTRACE_GET_THREAD_AREA:
		ret = ptrace_get_thread_area(child, addr, vp);
		break;

	case PTRACE_SET_THREAD_AREA:
		ret = ptrace_set_thread_area(child, addr, vp);
		break;

	case PTRACE_FAULTINFO: {
		ret = copy_to_user(p, &child->thread.arch.faultinfo,
				   sizeof(struct ptrace_faultinfo)) ?
			-EIO : 0;
		break;
	}

#ifdef PTRACE_LDT
	case PTRACE_LDT: {
		struct ptrace_ldt ldt;

		if (copy_from_user(&ldt, p, sizeof(ldt))) {
			ret = -EIO;
			break;
		}

		ret = -EIO;
		break;
	}
#endif
	default:
		ret = ptrace_request(child, request, addr, data);
		if (ret == -EIO)
			ret = subarch_ptrace(child, request, addr, data);
		break;
	}

	return ret;
}

static void send_sigtrap(struct task_struct *tsk, struct uml_pt_regs *regs,
		  int error_code)
{
	struct siginfo info;

	memset(&info, 0, sizeof(info));
	info.si_signo = SIGTRAP;
	info.si_code = TRAP_BRKPT;

	
	info.si_addr = UPT_IS_USER(regs) ? (void __user *) UPT_IP(regs) : NULL;

	
	force_sig_info(SIGTRAP, &info, tsk);
}

void syscall_trace(struct uml_pt_regs *regs, int entryexit)
{
	int is_singlestep = (current->ptrace & PT_DTRACE) && entryexit;
	int tracesysgood;

	if (!entryexit)
		audit_syscall_entry(HOST_AUDIT_ARCH,
				    UPT_SYSCALL_NR(regs),
				    UPT_SYSCALL_ARG1(regs),
				    UPT_SYSCALL_ARG2(regs),
				    UPT_SYSCALL_ARG3(regs),
				    UPT_SYSCALL_ARG4(regs));
	else
		audit_syscall_exit(regs);

	
	if (is_singlestep)
		send_sigtrap(current, regs, 0);

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;

	if (!(current->ptrace & PT_PTRACED))
		return;

	tracesysgood = (current->ptrace & PT_TRACESYSGOOD);
	ptrace_notify(SIGTRAP | (tracesysgood ? 0x80 : 0));

	if (entryexit) 
		set_thread_flag(TIF_SIGPENDING);

	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
