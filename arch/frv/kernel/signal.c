/* signal.c: FRV specific bits of signal handling
 *
 * Copyright (C) 2003-5 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from arch/m68k/kernel/signal.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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
#include <linux/personality.h>
#include <linux/freezer.h>
#include <linux/tracehook.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

struct fdpic_func_descriptor {
	unsigned long	text;
	unsigned long	GOT;
};

asmlinkage int sys_sigsuspend(int history0, int history1, old_sigset_t mask)
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

asmlinkage int sys_sigaction(int sig,
			     const struct old_sigaction __user *act,
			     struct old_sigaction __user *oact)
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

asmlinkage
int sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss)
{
	return do_sigaltstack(uss, uoss, __frame->sp);
}



struct sigframe
{
	__sigrestore_t pretcode;
	int sig;
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS-1];
	uint32_t retcode[2];
};

struct rt_sigframe
{
	__sigrestore_t pretcode;
	int sig;
	struct siginfo __user *pinfo;
	void __user *puc;
	struct siginfo info;
	struct ucontext uc;
	uint32_t retcode[2];
};

static int restore_sigcontext(struct sigcontext __user *sc, int *_gr8)
{
	struct user_context *user = current->thread.user;
	unsigned long tbr, psr;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	tbr = user->i.tbr;
	psr = user->i.psr;
	if (copy_from_user(user, &sc->sc_context, sizeof(sc->sc_context)))
		goto badframe;
	user->i.tbr = tbr;
	user->i.psr = psr;

	restore_user_regs(user);

	user->i.syscallno = -1;		

	*_gr8 = user->i.gr[8];
	return 0;

 badframe:
	return 1;
}

asmlinkage int sys_sigreturn(void)
{
	struct sigframe __user *frame = (struct sigframe __user *) __frame->sp;
	sigset_t set;
	int gr8;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__get_user(set.sig[0], &frame->sc.sc_oldmask))
		goto badframe;

	if (_NSIG_WORDS > 1 &&
	    __copy_from_user(&set.sig[1], &frame->extramask, sizeof(frame->extramask)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(&frame->sc, &gr8))
		goto badframe;
	return gr8;

 badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int sys_rt_sigreturn(void)
{
	struct rt_sigframe __user *frame = (struct rt_sigframe __user *) __frame->sp;
	sigset_t set;
	int gr8;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(&frame->uc.uc_mcontext, &gr8))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, __frame->sp) == -EFAULT)
		goto badframe;

	return gr8;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

static int setup_sigcontext(struct sigcontext __user *sc, unsigned long mask)
{
	save_user_regs(current->thread.user);

	if (copy_to_user(&sc->sc_context, current->thread.user, sizeof(sc->sc_context)) != 0)
		goto badframe;

	
	if (__put_user(mask, &sc->sc_oldmask) < 0)
		goto badframe;

	return 0;

 badframe:
	return 1;
}

static inline void __user *get_sigframe(struct k_sigaction *ka,
					size_t frame_size)
{
	unsigned long sp;

	
	sp = __frame->sp;

	
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (! sas_ss_flags(sp))
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	return (void __user *) ((sp - frame_size) & ~7UL);

} 

static int setup_frame(int sig, struct k_sigaction *ka, sigset_t *set)
{
	struct sigframe __user *frame;
	int rsig;

	set_fs(USER_DS);

	frame = get_sigframe(ka, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	rsig = sig;
	if (sig < 32 &&
	    __current_thread_info->exec_domain &&
	    __current_thread_info->exec_domain->signal_invmap)
		rsig = __current_thread_info->exec_domain->signal_invmap[sig];

	if (__put_user(rsig, &frame->sig) < 0)
		goto give_sigsegv;

	if (setup_sigcontext(&frame->sc, set->sig[0]))
		goto give_sigsegv;

	if (_NSIG_WORDS > 1) {
		if (__copy_to_user(frame->extramask, &set->sig[1],
				   sizeof(frame->extramask)))
			goto give_sigsegv;
	}

	if (ka->sa.sa_flags & SA_RESTORER) {
		if (__put_user(ka->sa.sa_restorer, &frame->pretcode) < 0)
			goto give_sigsegv;
	}
	else {
		if (__put_user((__sigrestore_t)frame->retcode, &frame->pretcode) ||
		    __put_user(0x8efc0000|__NR_sigreturn, &frame->retcode[0]) ||
		    __put_user(0xc0700000, &frame->retcode[1]))
			goto give_sigsegv;

		flush_icache_range((unsigned long) frame->retcode,
				   (unsigned long) (frame->retcode + 2));
	}

	
	if (current->personality & FDPIC_FUNCPTRS) {
		struct fdpic_func_descriptor __user *funcptr =
			(struct fdpic_func_descriptor __user *) ka->sa.sa_handler;
		struct fdpic_func_descriptor desc;
		if (copy_from_user(&desc, funcptr, sizeof(desc)))
			goto give_sigsegv;
		__frame->pc = desc.text;
		__frame->gr15 = desc.GOT;
	} else {
		__frame->pc   = (unsigned long) ka->sa.sa_handler;
		__frame->gr15 = 0;
	}

	__frame->sp   = (unsigned long) frame;
	__frame->lr   = (unsigned long) &frame->retcode;
	__frame->gr8  = sig;

	
	if (test_thread_flag(TIF_SINGLESTEP))
		ptrace_notify(SIGTRAP);

#if DEBUG_SIG
	printk("SIG deliver %d (%s:%d): sp=%p pc=%lx ra=%p\n",
	       sig, current->comm, current->pid, frame, __frame->pc,
	       frame->pretcode);
#endif

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;

} 

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			  sigset_t *set)
{
	struct rt_sigframe __user *frame;
	int rsig;

	set_fs(USER_DS);

	frame = get_sigframe(ka, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	rsig = sig;
	if (sig < 32 &&
	    __current_thread_info->exec_domain &&
	    __current_thread_info->exec_domain->signal_invmap)
		rsig = __current_thread_info->exec_domain->signal_invmap[sig];

	if (__put_user(rsig,		&frame->sig) ||
	    __put_user(&frame->info,	&frame->pinfo) ||
	    __put_user(&frame->uc,	&frame->puc))
		goto give_sigsegv;

	if (copy_siginfo_to_user(&frame->info, info))
		goto give_sigsegv;

	
	if (__put_user(0, &frame->uc.uc_flags) ||
	    __put_user(NULL, &frame->uc.uc_link) ||
	    __put_user((void __user *)current->sas_ss_sp, &frame->uc.uc_stack.ss_sp) ||
	    __put_user(sas_ss_flags(__frame->sp), &frame->uc.uc_stack.ss_flags) ||
	    __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size))
		goto give_sigsegv;

	if (setup_sigcontext(&frame->uc.uc_mcontext, set->sig[0]))
		goto give_sigsegv;

	if (__copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set)))
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
		if (__put_user(ka->sa.sa_restorer, &frame->pretcode))
			goto give_sigsegv;
	}
	else {
		if (__put_user((__sigrestore_t)frame->retcode, &frame->pretcode) ||
		    __put_user(0x8efc0000|__NR_rt_sigreturn, &frame->retcode[0]) ||
		    __put_user(0xc0700000, &frame->retcode[1]))
			goto give_sigsegv;

		flush_icache_range((unsigned long) frame->retcode,
				   (unsigned long) (frame->retcode + 2));
	}

	
	if (current->personality & FDPIC_FUNCPTRS) {
		struct fdpic_func_descriptor __user *funcptr =
			(struct fdpic_func_descriptor __user *) ka->sa.sa_handler;
		struct fdpic_func_descriptor desc;
		if (copy_from_user(&desc, funcptr, sizeof(desc)))
			goto give_sigsegv;
		__frame->pc = desc.text;
		__frame->gr15 = desc.GOT;
	} else {
		__frame->pc   = (unsigned long) ka->sa.sa_handler;
		__frame->gr15 = 0;
	}

	__frame->sp  = (unsigned long) frame;
	__frame->lr  = (unsigned long) &frame->retcode;
	__frame->gr8 = sig;
	__frame->gr9 = (unsigned long) &frame->info;

	
	if (test_thread_flag(TIF_SINGLESTEP))
		ptrace_notify(SIGTRAP);

#if DEBUG_SIG
	printk("SIG deliver %d (%s:%d): sp=%p pc=%lx ra=%p\n",
	       sig, current->comm, current->pid, frame, __frame->pc,
	       frame->pretcode);
#endif

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;

} 

static int handle_signal(unsigned long sig, siginfo_t *info,
			 struct k_sigaction *ka, sigset_t *oldset)
{
	int ret;

	
	if (__frame->syscallno != -1) {
		
		switch (__frame->gr8) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			__frame->gr8 = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				__frame->gr8 = -EINTR;
				break;
			}

			
		case -ERESTARTNOINTR:
			__frame->gr8 = __frame->orig_gr8;
			__frame->pc -= 4;
		}
		__frame->syscallno = -1;
	}

	
	if (ka->sa.sa_flags & SA_SIGINFO)
		ret = setup_rt_frame(sig, ka, info, oldset);
	else
		ret = setup_frame(sig, ka, oldset);

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

static void do_signal(void)
{
	struct k_sigaction ka;
	siginfo_t info;
	sigset_t *oldset;
	int signr;

	if (!user_mode(__frame))
		return;

	if (try_to_freeze())
		goto no_signal;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, __frame, NULL);
	if (signr > 0) {
		if (handle_signal(signr, &info, &ka, oldset) == 0) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);

			tracehook_signal_handler(signr, &info, &ka, __frame,
						 test_thread_flag(TIF_SINGLESTEP));
		}

		return;
	}

no_signal:
	
	if (__frame->syscallno != -1) {
		
		switch (__frame->gr8) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			__frame->gr8 = __frame->orig_gr8;
			__frame->pc -= 4;
			break;

		case -ERESTART_RESTARTBLOCK:
			__frame->gr7 = __NR_restart_syscall;
			__frame->pc -= 4;
			break;
		}
		__frame->syscallno = -1;
	}

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}

} 

asmlinkage void do_notify_resume(__u32 thread_info_flags)
{
	
	if (thread_info_flags & _TIF_SINGLESTEP)
		clear_thread_flag(TIF_SINGLESTEP);

	
	if (thread_info_flags & (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK))
		do_signal();

	
	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(__frame);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}

} 
