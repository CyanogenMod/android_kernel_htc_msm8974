/*
 *  arch/s390/kernel/signal.c
 *
 *    Copyright (C) IBM Corp. 1999,2006
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *
 *    Based on Intel version
 * 
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-11-28  Modified for POSIX.1b signals by Richard Henderson
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/tty.h>
#include <linux/personality.h>
#include <linux/binfmts.h>
#include <linux/tracehook.h>
#include <linux/syscalls.h>
#include <linux/compat.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <asm/lowcore.h>
#include <asm/switch_to.h>
#include "entry.h"

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))


typedef struct 
{
	__u8 callee_used_stack[__SIGNAL_FRAMESIZE];
	struct sigcontext sc;
	_sigregs sregs;
	int signo;
	__u8 retcode[S390_SYSCALL_SIZE];
} sigframe;

typedef struct 
{
	__u8 callee_used_stack[__SIGNAL_FRAMESIZE];
	__u8 retcode[S390_SYSCALL_SIZE];
	struct siginfo info;
	struct ucontext uc;
} rt_sigframe;

SYSCALL_DEFINE3(sigsuspend, int, history0, int, history1, old_sigset_t, mask)
{
	sigset_t blocked;

	current->saved_sigmask = current->blocked;
	mask &= _BLOCKABLE;
	siginitset(&blocked, mask);
	set_current_blocked(&blocked);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
	set_restore_sigmask();
	return -ERESTARTNOHAND;
}

SYSCALL_DEFINE3(sigaction, int, sig, const struct old_sigaction __user *, act,
		struct old_sigaction __user *, oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer) ||
		    __get_user(new_ka.sa.sa_flags, &act->sa_flags) ||
		    __get_user(mask, &act->sa_mask))
			return -EFAULT;
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer) ||
		    __put_user(old_ka.sa.sa_flags, &oact->sa_flags) ||
		    __put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask))
			return -EFAULT;
	}

	return ret;
}

SYSCALL_DEFINE2(sigaltstack, const stack_t __user *, uss,
		stack_t __user *, uoss)
{
	struct pt_regs *regs = task_pt_regs(current);
	return do_sigaltstack(uss, uoss, regs->gprs[15]);
}

static int save_sigregs(struct pt_regs *regs, _sigregs __user *sregs)
{
	_sigregs user_sregs;

	save_access_regs(current->thread.acrs);

	user_sregs.regs.psw.mask = psw_user_bits |
		(regs->psw.mask & PSW_MASK_USER);
	user_sregs.regs.psw.addr = regs->psw.addr;
	memcpy(&user_sregs.regs.gprs, &regs->gprs, sizeof(sregs->regs.gprs));
	memcpy(&user_sregs.regs.acrs, current->thread.acrs,
	       sizeof(sregs->regs.acrs));
	save_fp_regs(&current->thread.fp_regs);
	memcpy(&user_sregs.fpregs, &current->thread.fp_regs,
	       sizeof(s390_fp_regs));
	return __copy_to_user(sregs, &user_sregs, sizeof(_sigregs));
}

static int restore_sigregs(struct pt_regs *regs, _sigregs __user *sregs)
{
	int err;
	_sigregs user_sregs;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	err = __copy_from_user(&user_sregs, sregs, sizeof(_sigregs));
	if (err)
		return err;
	
	regs->psw.mask = (regs->psw.mask & ~PSW_MASK_USER) |
		(user_sregs.regs.psw.mask & PSW_MASK_USER);
	
	if (regs->psw.mask & PSW_MASK_EA)
		regs->psw.mask |= PSW_MASK_BA;
	regs->psw.addr = user_sregs.regs.psw.addr;
	memcpy(&regs->gprs, &user_sregs.regs.gprs, sizeof(sregs->regs.gprs));
	memcpy(&current->thread.acrs, &user_sregs.regs.acrs,
	       sizeof(sregs->regs.acrs));
	restore_access_regs(current->thread.acrs);

	memcpy(&current->thread.fp_regs, &user_sregs.fpregs,
	       sizeof(s390_fp_regs));
	current->thread.fp_regs.fpc &= FPC_VALID_MASK;

	restore_fp_regs(&current->thread.fp_regs);
	clear_thread_flag(TIF_SYSCALL);	
	return 0;
}

SYSCALL_DEFINE0(sigreturn)
{
	struct pt_regs *regs = task_pt_regs(current);
	sigframe __user *frame = (sigframe __user *)regs->gprs[15];
	sigset_t set;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set.sig, &frame->sc.oldmask, _SIGMASK_COPY_SIZE))
		goto badframe;
	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);
	if (restore_sigregs(regs, &frame->sregs))
		goto badframe;
	return regs->gprs[2];
badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

SYSCALL_DEFINE0(rt_sigreturn)
{
	struct pt_regs *regs = task_pt_regs(current);
	rt_sigframe __user *frame = (rt_sigframe __user *)regs->gprs[15];
	sigset_t set;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set.sig, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;
	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);
	if (restore_sigregs(regs, &frame->uc.uc_mcontext))
		goto badframe;
	if (do_sigaltstack(&frame->uc.uc_stack, NULL,
			   regs->gprs[15]) == -EFAULT)
		goto badframe;
	return regs->gprs[2];
badframe:
	force_sig(SIGSEGV, current);
	return 0;
}



static inline void __user *
get_sigframe(struct k_sigaction *ka, struct pt_regs * regs, size_t frame_size)
{
	unsigned long sp;

	
	sp = regs->gprs[15];

	
	if (on_sig_stack(sp) && !on_sig_stack((sp - frame_size) & -8UL))
		return (void __user *) -1UL;

	
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (! sas_ss_flags(sp))
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	
	else if (!user_mode(regs) &&
		 !(ka->sa.sa_flags & SA_RESTORER) &&
		 ka->sa.sa_restorer) {
		sp = (unsigned long) ka->sa.sa_restorer;
	}

	return (void __user *)((sp - frame_size) & -8ul);
}

static inline int map_signal(int sig)
{
	if (current_thread_info()->exec_domain
	    && current_thread_info()->exec_domain->signal_invmap
	    && sig < 32)
		return current_thread_info()->exec_domain->signal_invmap[sig];
	else
		return sig;
}

static int setup_frame(int sig, struct k_sigaction *ka,
		       sigset_t *set, struct pt_regs * regs)
{
	sigframe __user *frame;

	frame = get_sigframe(ka, regs, sizeof(sigframe));
	if (!access_ok(VERIFY_WRITE, frame, sizeof(sigframe)))
		goto give_sigsegv;

	if (frame == (void __user *) -1UL)
		goto give_sigsegv;

	if (__copy_to_user(&frame->sc.oldmask, &set->sig, _SIGMASK_COPY_SIZE))
		goto give_sigsegv;

	if (save_sigregs(regs, &frame->sregs))
		goto give_sigsegv;
	if (__put_user(&frame->sregs, &frame->sc.sregs))
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
                regs->gprs[14] = (unsigned long)
			ka->sa.sa_restorer | PSW_ADDR_AMODE;
	} else {
                regs->gprs[14] = (unsigned long)
			frame->retcode | PSW_ADDR_AMODE;
		if (__put_user(S390_SYSCALL_OPCODE | __NR_sigreturn,
	                       (u16 __user *)(frame->retcode)))
			goto give_sigsegv;
	}

	
	if (__put_user(regs->gprs[15], (addr_t __user *) frame))
		goto give_sigsegv;

	
	regs->gprs[15] = (unsigned long) frame;
	regs->psw.mask |= PSW_MASK_EA | PSW_MASK_BA;	
	regs->psw.addr = (unsigned long) ka->sa.sa_handler | PSW_ADDR_AMODE;

	regs->gprs[2] = map_signal(sig);
	regs->gprs[3] = (unsigned long) &frame->sc;

	if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL ||
	    sig == SIGTRAP || sig == SIGFPE) {
		
		regs->gprs[4] = regs->int_code & 127;
		regs->gprs[5] = regs->int_parm_long;
		regs->gprs[6] = task_thread_info(current)->last_break;
	}

	
	if (__put_user(regs->gprs[2], (int __user *) &frame->signo))
		goto give_sigsegv;
	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			   sigset_t *set, struct pt_regs * regs)
{
	int err = 0;
	rt_sigframe __user *frame;

	frame = get_sigframe(ka, regs, sizeof(rt_sigframe));
	if (!access_ok(VERIFY_WRITE, frame, sizeof(rt_sigframe)))
		goto give_sigsegv;

	if (frame == (void __user *) -1UL)
		goto give_sigsegv;

	if (copy_siginfo_to_user(&frame->info, info))
		goto give_sigsegv;

	
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __put_user((void __user *)current->sas_ss_sp, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->gprs[15]),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= save_sigregs(regs, &frame->uc.uc_mcontext);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
                regs->gprs[14] = (unsigned long)
			ka->sa.sa_restorer | PSW_ADDR_AMODE;
	} else {
                regs->gprs[14] = (unsigned long)
			frame->retcode | PSW_ADDR_AMODE;
		if (__put_user(S390_SYSCALL_OPCODE | __NR_rt_sigreturn,
			       (u16 __user *)(frame->retcode)))
			goto give_sigsegv;
	}

	
	if (__put_user(regs->gprs[15], (addr_t __user *) frame))
		goto give_sigsegv;

	
	regs->gprs[15] = (unsigned long) frame;
	regs->psw.mask |= PSW_MASK_EA | PSW_MASK_BA;	
	regs->psw.addr = (unsigned long) ka->sa.sa_handler | PSW_ADDR_AMODE;

	regs->gprs[2] = map_signal(sig);
	regs->gprs[3] = (unsigned long) &frame->info;
	regs->gprs[4] = (unsigned long) &frame->uc;
	regs->gprs[5] = task_thread_info(current)->last_break;
	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static int handle_signal(unsigned long sig, struct k_sigaction *ka,
			 siginfo_t *info, sigset_t *oldset,
			 struct pt_regs *regs)
{
	int ret;

	
	if (ka->sa.sa_flags & SA_SIGINFO)
		ret = setup_rt_frame(sig, ka, info, oldset, regs);
	else
		ret = setup_frame(sig, ka, oldset, regs);
	if (ret)
		return ret;
	block_sigmask(ka, sig);
	return 0;
}

void do_signal(struct pt_regs *regs)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;
	sigset_t *oldset;

	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	current_thread_info()->system_call =
		test_thread_flag(TIF_SYSCALL) ? regs->int_code : 0;
	signr = get_signal_to_deliver(&info, &ka, regs, NULL);

	if (signr > 0) {
		
		if (current_thread_info()->system_call) {
			regs->int_code = current_thread_info()->system_call;
			
			switch (regs->gprs[2]) {
			case -ERESTART_RESTARTBLOCK:
			case -ERESTARTNOHAND:
				regs->gprs[2] = -EINTR;
				break;
			case -ERESTARTSYS:
				if (!(ka.sa.sa_flags & SA_RESTART)) {
					regs->gprs[2] = -EINTR;
					break;
				}
			
			case -ERESTARTNOINTR:
				regs->gprs[2] = regs->orig_gpr2;
				regs->psw.addr =
					__rewind_psw(regs->psw,
						     regs->int_code >> 16);
				break;
			}
		}
		
		clear_thread_flag(TIF_SYSCALL);

		if ((is_compat_task() ?
		     handle_signal32(signr, &ka, &info, oldset, regs) :
		     handle_signal(signr, &ka, &info, oldset, regs)) == 0) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);

			tracehook_signal_handler(signr, &info, &ka, regs,
					 test_thread_flag(TIF_SINGLE_STEP));
		}
		return;
	}

	
	clear_thread_flag(TIF_SYSCALL);
	if (current_thread_info()->system_call) {
		regs->int_code = current_thread_info()->system_call;
		switch (regs->gprs[2]) {
		case -ERESTART_RESTARTBLOCK:
			
			regs->int_code = __NR_restart_syscall;
		
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			
			regs->gprs[2] = regs->orig_gpr2;
			set_thread_flag(TIF_SYSCALL);
			break;
		}
	}

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

void do_notify_resume(struct pt_regs *regs)
{
	clear_thread_flag(TIF_NOTIFY_RESUME);
	tracehook_notify_resume(regs);
	if (current->replacement_session_keyring)
		key_replace_session_keyring();
}
