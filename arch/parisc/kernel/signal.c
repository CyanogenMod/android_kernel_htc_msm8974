/*
 *  linux/arch/parisc/kernel/signal.c: Architecture-specific signal
 *  handling support.
 *
 *  Copyright (C) 2000 David Huggins-Daines <dhd@debian.org>
 *  Copyright (C) 2000 Linuxcare, Inc.
 *
 *  Based on the ia64, i386, and alpha versions.
 *
 *  Like the IA-64, we are a recent enough port (we are *starting*
 *  with glibc2.2) that we do not need to support the old non-realtime
 *  Linux signals.  Therefore we don't.  HP/UX signals will go in
 *  arch/parisc/hpux/signal.c when we figure out how to do them.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/tracehook.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/compat.h>
#include <linux/elf.h>
#include <asm/ucontext.h>
#include <asm/rt_sigframe.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/cacheflush.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_COMPAT
#include "signal32.h"
#endif

#define DEBUG_SIG 0 
#define DEBUG_SIG_LEVEL 2

#if DEBUG_SIG
#define DBG(LEVEL, ...) \
        ((DEBUG_SIG_LEVEL >= LEVEL) \
	? printk(__VA_ARGS__) : (void) 0)
#else
#define DBG(LEVEL, ...)
#endif
	

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

#define A(__x)	((unsigned long)(__x))

#ifdef CONFIG_64BIT
#include "sys32.h"
#endif


#define INSN_LDI_R25_0	 0x34190000 
#define INSN_LDI_R25_1	 0x34190002 
#define INSN_LDI_R20	 0x3414015a 
#define INSN_BLE_SR2_R0  0xe4008200 
#define INSN_NOP	 0x08000240 
#define INSN_DIE_HORRIBLY 0x68000ccc 

static long
restore_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs)
{
	long err = 0;

	err |= __copy_from_user(regs->gr, sc->sc_gr, sizeof(regs->gr));
	err |= __copy_from_user(regs->fr, sc->sc_fr, sizeof(regs->fr));
	err |= __copy_from_user(regs->iaoq, sc->sc_iaoq, sizeof(regs->iaoq));
	err |= __copy_from_user(regs->iasq, sc->sc_iasq, sizeof(regs->iasq));
	err |= __get_user(regs->sar, &sc->sc_sar);
	DBG(2,"restore_sigcontext: iaoq is 0x%#lx / 0x%#lx\n", 
			regs->iaoq[0],regs->iaoq[1]);
	DBG(2,"restore_sigcontext: r28 is %ld\n", regs->gr[28]);
	return err;
}

void
sys_rt_sigreturn(struct pt_regs *regs, int in_syscall)
{
	struct rt_sigframe __user *frame;
	sigset_t set;
	unsigned long usp = (regs->gr[30] & ~(0x01UL));
	unsigned long sigframe_size = PARISC_RT_SIGFRAME_SIZE;
#ifdef CONFIG_64BIT
	compat_sigset_t compat_set;
	struct compat_rt_sigframe __user * compat_frame;
	
	if (is_compat_task())
		sigframe_size = PARISC_RT_SIGFRAME_SIZE32;
#endif


	
	frame = (struct rt_sigframe __user *)
		(usp - sigframe_size);
	DBG(2,"sys_rt_sigreturn: frame is %p\n", frame);

#ifdef CONFIG_64BIT
	compat_frame = (struct compat_rt_sigframe __user *)frame;
	
	if (is_compat_task()) {
		DBG(2,"sys_rt_sigreturn: ELF32 process.\n");
		if (__copy_from_user(&compat_set, &compat_frame->uc.uc_sigmask, sizeof(compat_set)))
			goto give_sigsegv;
		sigset_32to64(&set,&compat_set);
	} else
#endif
	{
		if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
			goto give_sigsegv;
	}
		
	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	
#ifdef CONFIG_64BIT
	if (is_compat_task()) {
		DBG(1,"sys_rt_sigreturn: compat_frame->uc.uc_mcontext 0x%p\n",
				&compat_frame->uc.uc_mcontext);
		if (restore_sigcontext32(&compat_frame->uc.uc_mcontext, 
					&compat_frame->regs, regs))
			goto give_sigsegv;
		DBG(1,"sys_rt_sigreturn: usp %#08lx stack 0x%p\n", 
				usp, &compat_frame->uc.uc_stack);
		if (do_sigaltstack32(&compat_frame->uc.uc_stack, NULL, usp) == -EFAULT)
			goto give_sigsegv;
	} else
#endif
	{
		DBG(1,"sys_rt_sigreturn: frame->uc.uc_mcontext 0x%p\n",
				&frame->uc.uc_mcontext);
		if (restore_sigcontext(&frame->uc.uc_mcontext, regs))
			goto give_sigsegv;
		DBG(1,"sys_rt_sigreturn: usp %#08lx stack 0x%p\n", 
				usp, &frame->uc.uc_stack);
		if (do_sigaltstack(&frame->uc.uc_stack, NULL, usp) == -EFAULT)
			goto give_sigsegv;
	}
		


	if (in_syscall)
		regs->gr[31] = regs->iaoq[0];
#if DEBUG_SIG
	DBG(1,"sys_rt_sigreturn: returning to %#lx, DUMPING REGS:\n", regs->iaoq[0]);
	show_regs(regs);
#endif
	return;

give_sigsegv:
	DBG(1,"sys_rt_sigreturn: Sending SIGSEGV\n");
	force_sig(SIGSEGV, current);
	return;
}


static inline void __user *
get_sigframe(struct k_sigaction *ka, unsigned long sp, size_t frame_size)
{

	DBG(1,"get_sigframe: ka = %#lx, sp = %#lx, frame_size = %#lx\n",
			(unsigned long)ka, sp, frame_size);
	
	if ((ka->sa.sa_flags & SA_ONSTACK) != 0 && ! sas_ss_flags(sp))
		sp = current->sas_ss_sp; 

	DBG(1,"get_sigframe: Returning sp = %#lx\n", (unsigned long)sp);
	return (void __user *) sp; 
}

static long
setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs, int in_syscall)
		 
{
	unsigned long flags = 0;
	long err = 0;

	if (on_sig_stack((unsigned long) sc))
		flags |= PARISC_SC_FLAG_ONSTACK;
	if (in_syscall) {
		flags |= PARISC_SC_FLAG_IN_SYSCALL;
		
		err |= __put_user(regs->gr[31], &sc->sc_iaoq[0]);
		err |= __put_user(regs->gr[31]+4, &sc->sc_iaoq[1]);
		err |= __put_user(regs->sr[3], &sc->sc_iasq[0]);
		err |= __put_user(regs->sr[3], &sc->sc_iasq[1]);
		DBG(1,"setup_sigcontext: iaoq %#lx / %#lx (in syscall)\n",
			regs->gr[31], regs->gr[31]+4);
	} else {
		err |= __copy_to_user(sc->sc_iaoq, regs->iaoq, sizeof(regs->iaoq));
		err |= __copy_to_user(sc->sc_iasq, regs->iasq, sizeof(regs->iasq));
		DBG(1,"setup_sigcontext: iaoq %#lx / %#lx (not in syscall)\n", 
			regs->iaoq[0], regs->iaoq[1]);
	}

	err |= __put_user(flags, &sc->sc_flags);
	err |= __copy_to_user(sc->sc_gr, regs->gr, sizeof(regs->gr));
	err |= __copy_to_user(sc->sc_fr, regs->fr, sizeof(regs->fr));
	err |= __put_user(regs->sar, &sc->sc_sar);
	DBG(1,"setup_sigcontext: r28 is %ld\n", regs->gr[28]);

	return err;
}

static long
setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
	       sigset_t *set, struct pt_regs *regs, int in_syscall)
{
	struct rt_sigframe __user *frame;
	unsigned long rp, usp;
	unsigned long haddr, sigframe_size;
	int err = 0;
#ifdef CONFIG_64BIT
	compat_int_t compat_val;
	struct compat_rt_sigframe __user * compat_frame;
	compat_sigset_t compat_set;
#endif
	
	usp = (regs->gr[30] & ~(0x01UL));
	
	frame = get_sigframe(ka, usp, sizeof(*frame));

	DBG(1,"SETUP_RT_FRAME: START\n");
	DBG(1,"setup_rt_frame: frame %p info %p\n", frame, info);

	
#ifdef CONFIG_64BIT

	compat_frame = (struct compat_rt_sigframe __user *)frame;
	
	if (is_compat_task()) {
		DBG(1,"setup_rt_frame: frame->info = 0x%p\n", &compat_frame->info);
		err |= copy_siginfo_to_user32(&compat_frame->info, info);
		DBG(1,"SETUP_RT_FRAME: 1\n");
		compat_val = (compat_int_t)current->sas_ss_sp;
		err |= __put_user(compat_val, &compat_frame->uc.uc_stack.ss_sp);
		DBG(1,"SETUP_RT_FRAME: 2\n");
		compat_val = (compat_int_t)current->sas_ss_size;
		err |= __put_user(compat_val, &compat_frame->uc.uc_stack.ss_size);
		DBG(1,"SETUP_RT_FRAME: 3\n");
		compat_val = sas_ss_flags(regs->gr[30]);		
		err |= __put_user(compat_val, &compat_frame->uc.uc_stack.ss_flags);		
		DBG(1,"setup_rt_frame: frame->uc = 0x%p\n", &compat_frame->uc);
		DBG(1,"setup_rt_frame: frame->uc.uc_mcontext = 0x%p\n", &compat_frame->uc.uc_mcontext);
		err |= setup_sigcontext32(&compat_frame->uc.uc_mcontext, 
					&compat_frame->regs, regs, in_syscall);
		sigset_64to32(&compat_set,set);
		err |= __copy_to_user(&compat_frame->uc.uc_sigmask, &compat_set, sizeof(compat_set));
	} else
#endif
	{	
		DBG(1,"setup_rt_frame: frame->info = 0x%p\n", &frame->info);
		err |= copy_siginfo_to_user(&frame->info, info);
		err |= __put_user(current->sas_ss_sp, &frame->uc.uc_stack.ss_sp);
		err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
		err |= __put_user(sas_ss_flags(regs->gr[30]),
				  &frame->uc.uc_stack.ss_flags);
		DBG(1,"setup_rt_frame: frame->uc = 0x%p\n", &frame->uc);
		DBG(1,"setup_rt_frame: frame->uc.uc_mcontext = 0x%p\n", &frame->uc.uc_mcontext);
		err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, in_syscall);
		
		err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	}
	
	if (err)
		goto give_sigsegv;

	err |= __put_user(in_syscall ? INSN_LDI_R25_1 : INSN_LDI_R25_0,
			&frame->tramp[SIGRESTARTBLOCK_TRAMP+0]);
	err |= __put_user(INSN_LDI_R20, 
			&frame->tramp[SIGRESTARTBLOCK_TRAMP+1]);
	err |= __put_user(INSN_BLE_SR2_R0, 
			&frame->tramp[SIGRESTARTBLOCK_TRAMP+2]);
	err |= __put_user(INSN_NOP, &frame->tramp[SIGRESTARTBLOCK_TRAMP+3]);

#if DEBUG_SIG
	
	{
		int sid;
		asm ("mfsp %%sr3,%0" : "=r" (sid));
		DBG(1,"setup_rt_frame: Flushing 64 bytes at space %#x offset %p\n",
		       sid, frame->tramp);
	}
#endif

	flush_user_dcache_range((unsigned long) &frame->tramp[0],
			   (unsigned long) &frame->tramp[TRAMP_SIZE]);
	flush_user_icache_range((unsigned long) &frame->tramp[0],
			   (unsigned long) &frame->tramp[TRAMP_SIZE]);

	rp = (unsigned long) &frame->tramp[SIGRESTARTBLOCK_TRAMP];

	if (err)
		goto give_sigsegv;

	haddr = A(ka->sa.sa_handler);
	
#ifdef CONFIG_64BIT
	if (is_compat_task()) {
#endif
		if (haddr & PA_PLABEL_FDESC) {
			Elf32_Fdesc fdesc;
			Elf32_Fdesc __user *ufdesc = (Elf32_Fdesc __user *)A(haddr & ~3);

			err = __copy_from_user(&fdesc, ufdesc, sizeof(fdesc));

			if (err)
				goto give_sigsegv;

			haddr = fdesc.addr;
			regs->gr[19] = fdesc.gp;
		}
#ifdef CONFIG_64BIT
	} else {
		Elf64_Fdesc fdesc;
		Elf64_Fdesc __user *ufdesc = (Elf64_Fdesc __user *)A(haddr & ~3);
		
		err = __copy_from_user(&fdesc, ufdesc, sizeof(fdesc));
		
		if (err)
			goto give_sigsegv;
		
		haddr = fdesc.addr;
		regs->gr[19] = fdesc.gp;
		DBG(1,"setup_rt_frame: 64 bit signal, exe=%#lx, r19=%#lx, in_syscall=%d\n",
		     haddr, regs->gr[19], in_syscall);
	}
#endif

	sigframe_size = PARISC_RT_SIGFRAME_SIZE;
#ifdef CONFIG_64BIT
	if (is_compat_task())
		sigframe_size = PARISC_RT_SIGFRAME_SIZE32;
#endif
	if (in_syscall) {
		regs->gr[31] = haddr;
#ifdef CONFIG_64BIT
		if (!test_thread_flag(TIF_32BIT))
			sigframe_size |= 1;
#endif
	} else {
		unsigned long psw = USER_PSW;
#ifdef CONFIG_64BIT
		if (!test_thread_flag(TIF_32BIT))
			psw |= PSW_W;
#endif

		if (pa_psw(current)->r) {
			pa_psw(current)->r = 0;
			psw |= PSW_R;
			mtctl(-1, 0);
		}

		regs->gr[0] = psw;
		regs->iaoq[0] = haddr | 3;
		regs->iaoq[1] = regs->iaoq[0] + 4;
	}

	regs->gr[2]  = rp;                
	regs->gr[26] = sig;               
	
#ifdef CONFIG_64BIT
	if (is_compat_task()) {
		regs->gr[25] = A(&compat_frame->info); 
		regs->gr[24] = A(&compat_frame->uc);   
	} else
#endif
	{		
		regs->gr[25] = A(&frame->info); 
		regs->gr[24] = A(&frame->uc);   
	}
	
	DBG(1,"setup_rt_frame: making sigreturn frame: %#lx + %#lx = %#lx\n",
	       regs->gr[30], sigframe_size,
	       regs->gr[30] + sigframe_size);
	
	regs->gr[30] = (A(frame) + sigframe_size);


	DBG(1,"setup_rt_frame: sig deliver (%s,%d) frame=0x%p sp=%#lx iaoq=%#lx/%#lx rp=%#lx\n",
	       current->comm, current->pid, frame, regs->gr[30],
	       regs->iaoq[0], regs->iaoq[1], rp);

	return 1;

give_sigsegv:
	DBG(1,"setup_rt_frame: sending SIGSEGV\n");
	force_sigsegv(sig, current);
	return 0;
}

	

static long
handle_signal(unsigned long sig, siginfo_t *info, struct k_sigaction *ka,
		sigset_t *oldset, struct pt_regs *regs, int in_syscall)
{
	DBG(1,"handle_signal: sig=%ld, ka=%p, info=%p, oldset=%p, regs=%p\n",
	       sig, ka, info, oldset, regs);
	
	
	if (!setup_rt_frame(sig, ka, info, oldset, regs, in_syscall))
		return 0;

	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked,sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	tracehook_signal_handler(sig, info, ka, regs, 
		test_thread_flag(TIF_SINGLESTEP) ||
		test_thread_flag(TIF_BLOCKSTEP));

	return 1;
}

static inline void
syscall_restart(struct pt_regs *regs, struct k_sigaction *ka)
{
	
	switch (regs->gr[28]) {
	case -ERESTART_RESTARTBLOCK:
		current_thread_info()->restart_block.fn =
			do_no_restart_syscall;
	case -ERESTARTNOHAND:
		DBG(1,"ERESTARTNOHAND: returning -EINTR\n");
		regs->gr[28] = -EINTR;
		break;

	case -ERESTARTSYS:
		if (!(ka->sa.sa_flags & SA_RESTART)) {
			DBG(1,"ERESTARTSYS: putting -EINTR\n");
			regs->gr[28] = -EINTR;
			break;
		}
		
	case -ERESTARTNOINTR:
		regs->gr[31] -= 8; 
		
		regs->gr[28] = regs->orig_r28;
		break;
	}
}

static inline void
insert_restart_trampoline(struct pt_regs *regs)
{
	switch(regs->gr[28]) {
	case -ERESTART_RESTARTBLOCK: {
		
		unsigned int *usp = (unsigned int *)regs->gr[30];

#ifdef CONFIG_64BIT
		put_user(regs->gr[31] >> 32, &usp[0]);
		put_user(regs->gr[31] & 0xffffffff, &usp[1]);
		put_user(0x0fc010df, &usp[2]);
#else
		put_user(regs->gr[31], &usp[0]);
		put_user(0x0fc0109f, &usp[2]);
#endif
		put_user(0xe0008200, &usp[3]);
		put_user(0x34140000, &usp[4]);

		flush_user_dcache_range(regs->gr[30], regs->gr[30] + 4);
		flush_user_icache_range(regs->gr[30], regs->gr[30] + 4);

		regs->gr[31] = regs->gr[30] + 8;
		
		regs->gr[28] = regs->orig_r28;

		return;
	}
	case -ERESTARTNOHAND:
	case -ERESTARTSYS:
	case -ERESTARTNOINTR: {
		regs->gr[31] -= 8;
		
		regs->gr[28] = regs->orig_r28;

		return;
	}
	default:
		break;
	}
}

asmlinkage void
do_signal(struct pt_regs *regs, long in_syscall)
{
	siginfo_t info;
	struct k_sigaction ka;
	int signr;
	sigset_t *oldset;

	DBG(1,"\ndo_signal: oldset=0x%p, regs=0x%p, sr7 %#lx, in_syscall=%d\n",
	       oldset, regs, regs->sr[7], in_syscall);


	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	DBG(1,"do_signal: oldset %08lx / %08lx\n", 
		oldset->sig[0], oldset->sig[1]);


	
	while (1) {
	  
		signr = get_signal_to_deliver(&info, &ka, regs, NULL);
		DBG(3,"do_signal: signr = %d, regs->gr[28] = %ld\n", signr, regs->gr[28]); 
	
		if (signr <= 0)
		  break;
		
		
		if (in_syscall)
			syscall_restart(regs, &ka);

		if (handle_signal(signr, &info, &ka, oldset,
				  regs, in_syscall)) {
			DBG(1,KERN_DEBUG "do_signal: Exit (success), regs->gr[28] = %ld\n",
				regs->gr[28]);
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
			return;
		}
	}
	

	
	if (in_syscall)
		insert_restart_trampoline(regs);
	
	DBG(1,"do_signal: Exit (not delivered), regs->gr[28] = %ld\n", 
		regs->gr[28]);

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}

	return;
}

void do_notify_resume(struct pt_regs *regs, long in_syscall)
{
	if (test_thread_flag(TIF_SIGPENDING) ||
	    test_thread_flag(TIF_RESTORE_SIGMASK))
		do_signal(regs, in_syscall);

	if (test_thread_flag(TIF_NOTIFY_RESUME)) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}
