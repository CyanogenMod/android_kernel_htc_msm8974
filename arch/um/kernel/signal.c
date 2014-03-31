/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/siginfo.h>
#include <asm/signal.h>
#include <asm/unistd.h>
#include "frame_kern.h"
#include "kern_util.h"

EXPORT_SYMBOL(block_signals);
EXPORT_SYMBOL(unblock_signals);

#define _S(nr) (1<<((nr)-1))

#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

static int handle_signal(struct pt_regs *regs, unsigned long signr,
			 struct k_sigaction *ka, siginfo_t *info,
			 sigset_t *oldset)
{
	unsigned long sp;
	int err;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	
	if (PT_REGS_SYSCALL_NR(regs) >= 0) {
		
		switch (PT_REGS_SYSCALL_RET(regs)) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			PT_REGS_SYSCALL_RET(regs) = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				PT_REGS_SYSCALL_RET(regs) = -EINTR;
				break;
			}
		
		case -ERESTARTNOINTR:
			PT_REGS_RESTART_SYSCALL(regs);
			PT_REGS_ORIG_SYSCALL(regs) = PT_REGS_SYSCALL_NR(regs);
			break;
		}
	}

	sp = PT_REGS_SP(regs);
	if ((ka->sa.sa_flags & SA_ONSTACK) && (sas_ss_flags(sp) == 0))
		sp = current->sas_ss_sp + current->sas_ss_size;

#ifdef CONFIG_ARCH_HAS_SC_SIGNALS
	if (!(ka->sa.sa_flags & SA_SIGINFO))
		err = setup_signal_stack_sc(sp, signr, ka, regs, oldset);
	else
#endif
		err = setup_signal_stack_si(sp, signr, ka, regs, info, oldset);

	if (err)
		force_sigsegv(signr, current);
	else
		block_sigmask(ka, signr);

	return err;
}

static int kern_do_signal(struct pt_regs *regs)
{
	struct k_sigaction ka_copy;
	siginfo_t info;
	sigset_t *oldset;
	int sig, handled_sig = 0;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	while ((sig = get_signal_to_deliver(&info, &ka_copy, regs, NULL)) > 0) {
		handled_sig = 1;
		
		if (!handle_signal(regs, sig, &ka_copy, &info, oldset)) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
			break;
		}
	}

	
	if (!handled_sig && (PT_REGS_SYSCALL_NR(regs) >= 0)) {
		
		switch (PT_REGS_SYSCALL_RET(regs)) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			PT_REGS_ORIG_SYSCALL(regs) = PT_REGS_SYSCALL_NR(regs);
			PT_REGS_RESTART_SYSCALL(regs);
			break;
		case -ERESTART_RESTARTBLOCK:
			PT_REGS_ORIG_SYSCALL(regs) = __NR_restart_syscall;
			PT_REGS_RESTART_SYSCALL(regs);
			break;
		}
	}

	if (current->ptrace & PT_DTRACE)
		current->thread.singlestep_syscall =
			is_syscall(PT_REGS_IP(&current->thread.regs));

	if (!handled_sig && test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
	return handled_sig;
}

int do_signal(void)
{
	return kern_do_signal(&current->thread.regs);
}

long sys_sigsuspend(int history0, int history1, old_sigset_t mask)
{
	sigset_t blocked;

	mask &= _BLOCKABLE;
	siginitset(&blocked, mask);
	set_current_blocked(&blocked);

	current->state = TASK_INTERRUPTIBLE;
	schedule();
	set_thread_flag(TIF_RESTORE_SIGMASK);
	return -ERESTARTNOHAND;
}

long sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss)
{
	return do_sigaltstack(uss, uoss, PT_REGS_SP(&current->thread.regs));
}
