/*
 *  linux/arch/cris/kernel/signal.c
 *
 *  Based on arch/i386/kernel/signal.c by
 *     Copyright (C) 1991, 1992  Linus Torvalds
 *     1997-11-28  Modified for POSIX.1b signals by Richard Henderson *
 *
 *  Ideas also taken from arch/arm.
 *
 *  Copyright (C) 2000-2007 Axis Communications AB
 *
 *  Authors:  Bjorn Wesen (bjornw@axis.com)
 *
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

#include <asm/processor.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <arch/system.h>

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))


#define RESTART_CRIS_SYS(regs) regs->r10 = regs->orig_r10; regs->irp -= 2;

void do_signal(int canrestart, struct pt_regs *regs);

int sys_sigsuspend(old_sigset_t mask, long r11, long r12, long r13, long mof,
	long srp, struct pt_regs *regs)
{
	mask &= _BLOCKABLE;
	spin_lock_irq(&current->sighand->siglock);
	current->saved_sigmask = current->blocked;
	siginitset(&current->blocked, mask);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	set_thread_flag(TIF_RESTORE_SIGMASK);
	return -ERESTARTNOHAND;
}

int sys_sigaction(int sig, const struct old_sigaction __user *act,
	struct old_sigaction *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}

int sys_sigaltstack(const stack_t *uss, stack_t __user *uoss)
{
	return do_sigaltstack(uss, uoss, rdusp());
}



struct sigframe {
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS-1];
	unsigned char retcode[8];  
};

struct rt_sigframe {
	struct siginfo *pinfo;
	void *puc;
	struct siginfo info;
	struct ucontext uc;
	unsigned char retcode[8];  
};


static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc)
{
	unsigned int err = 0;
	unsigned long old_usp;

        
	current_thread_info()->restart_block.fn = do_no_restart_syscall;


	if (__copy_from_user(regs, sc, sizeof(struct pt_regs)))
                goto badframe;

	

	regs->dccr |= 1 << 8;


	err |= __get_user(old_usp, &sc->usp);

	wrusp(old_usp);


	return err;

badframe:
	return 1;
}


asmlinkage int sys_sigreturn(long r10, long r11, long r12, long r13, long mof,
                             long srp, struct pt_regs *regs)
{
	struct sigframe __user *frame = (struct sigframe *)rdusp();
	sigset_t set;

        if (((long)frame) & 3)
                goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__get_user(set.sig[0], &frame->sc.oldmask)
	    || (_NSIG_WORDS > 1
		&& __copy_from_user(&set.sig[1], frame->extramask,
				    sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->sc))
		goto badframe;

	

	return regs->r10;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}


asmlinkage int sys_rt_sigreturn(long r10, long r11, long r12, long r13,
                                long mof, long srp, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame = (struct rt_sigframe *)rdusp();
	sigset_t set;

        if (((long)frame) & 3)
                goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, rdusp()) == -EFAULT)
		goto badframe;

	return regs->r10;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}


static int setup_sigcontext(struct sigcontext __user *sc,
	struct pt_regs *regs, unsigned long mask)
{
	int err = 0;
	unsigned long usp = rdusp();

	

	err |= __copy_to_user(sc, regs, sizeof(struct pt_regs));

        regs->frametype = CRIS_FRAME_NORMAL;

	

	err |= __put_user(mask, &sc->oldmask);

	err |= __put_user(usp, &sc->usp);

	return err;
}


static inline void __user *
get_sigframe(struct k_sigaction *ka, struct pt_regs *regs, size_t frame_size)
{
	unsigned long sp = rdusp();

	
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (! on_sig_stack(sp))
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	

	sp &= ~3;

	return (void __user*)(sp - frame_size);
}


static int setup_frame(int sig, struct k_sigaction *ka,
		       sigset_t *set, struct pt_regs *regs)
{
	struct sigframe __user *frame;
	unsigned long return_ip;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);
	if (err)
		goto give_sigsegv;

	if (_NSIG_WORDS > 1) {
		err |= __copy_to_user(frame->extramask, &set->sig[1],
				      sizeof(frame->extramask));
	}
	if (err)
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
		return_ip = (unsigned long)ka->sa.sa_restorer;
	} else {
		
		return_ip = (unsigned long)&frame->retcode;
		
		err |= __put_user(0x9c5f,         (short __user*)(frame->retcode+0));
		err |= __put_user(__NR_sigreturn, (short __user*)(frame->retcode+2));
		err |= __put_user(0xe93d,         (short __user*)(frame->retcode+4));
	}

	if (err)
		goto give_sigsegv;

	

	regs->irp = (unsigned long) ka->sa.sa_handler;  
	regs->srp = return_ip;                          
	regs->r10 = sig;                                

	

	wrusp((unsigned long)frame);

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
	sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	unsigned long return_ip;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);
	if (err)
		goto give_sigsegv;

	
        err |= __clear_user(&frame->uc, offsetof(struct ucontext, uc_mcontext));

	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);

	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
		return_ip = (unsigned long)ka->sa.sa_restorer;
	} else {
		
		return_ip = (unsigned long)&frame->retcode;
		
		err |= __put_user(0x9c5f, (short __user *)(frame->retcode+0));
		err |= __put_user(__NR_rt_sigreturn,
			(short __user *)(frame->retcode+2));
		err |= __put_user(0xe93d, (short __user *)(frame->retcode+4));
	}

	if (err)
		goto give_sigsegv;

	

	

	
	regs->irp = (unsigned long) ka->sa.sa_handler;
	
	regs->srp = return_ip;
	
	regs->r10 = sig;
	
	regs->r11 = (unsigned long)&frame->info;
	
	regs->r12 = 0;

	
	wrusp((unsigned long)frame);

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}


static inline int handle_signal(int canrestart, unsigned long sig,
	siginfo_t *info, struct k_sigaction *ka,
	sigset_t *oldset, struct pt_regs *regs)
{
	int ret;

	
	if (canrestart) {
		
		switch (regs->r10) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->r10 = -EINTR;
			break;
		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->r10 = -EINTR;
				break;
			}
		
		case -ERESTARTNOINTR:
			RESTART_CRIS_SYS(regs);
		}
	}

	
	if (ka->sa.sa_flags & SA_SIGINFO)
		ret = setup_rt_frame(sig, ka, info, oldset, regs);
	else
		ret = setup_frame(sig, ka, oldset, regs);

	if (ret == 0) {
		spin_lock_irq(&current->sighand->siglock);
		sigorsets(&current->blocked, &current->blocked,
			&ka->sa.sa_mask);
		if (!(ka->sa.sa_flags & SA_NODEFER))
			sigaddset(&current->blocked, sig);
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}
	return ret;
}


void do_signal(int canrestart, struct pt_regs *regs)
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

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		
		if (handle_signal(canrestart, signr, &info, &ka,
				oldset, regs)) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
		}
		return;
	}

	
	if (canrestart) {
		
		if (regs->r10 == -ERESTARTNOHAND ||
		    regs->r10 == -ERESTARTSYS ||
		    regs->r10 == -ERESTARTNOINTR) {
			RESTART_CRIS_SYS(regs);
		}
		if (regs->r10 == -ERESTART_RESTARTBLOCK) {
			regs->r9 = __NR_restart_syscall;
			regs->irp -= 2;
		}
	}

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}
