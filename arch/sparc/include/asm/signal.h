#ifndef __SPARC_SIGNAL_H
#define __SPARC_SIGNAL_H

#include <asm/sigcontext.h>
#include <linux/compiler.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <linux/personality.h>
#include <linux/types.h>
#endif
#endif

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define    SUBSIG_STACK       0
#define    SUBSIG_ILLINST     2
#define    SUBSIG_PRIVINST    3
#define    SUBSIG_BADTRAP(t)  (0x80 + (t))

#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6

#define SIGEMT           7
#define    SUBSIG_TAG    10

#define SIGFPE		 8
#define    SUBSIG_FPDISABLED     0x400
#define    SUBSIG_FPERROR        0x404
#define    SUBSIG_FPINTOVFL      0x001
#define    SUBSIG_FPSTSIG        0x002
#define    SUBSIG_IDIVZERO       0x014
#define    SUBSIG_FPINEXACT      0x0c4
#define    SUBSIG_FPDIVZERO      0x0c8
#define    SUBSIG_FPUNFLOW       0x0cc
#define    SUBSIG_FPOPERROR      0x0d0
#define    SUBSIG_FPOVFLOW       0x0d4

#define SIGKILL		 9
#define SIGBUS          10
#define    SUBSIG_BUSTIMEOUT    1
#define    SUBSIG_ALIGNMENT     2
#define    SUBSIG_MISCERROR     5

#define SIGSEGV		11
#define    SUBSIG_NOMAPPING     3
#define    SUBSIG_PROTECTION    4
#define    SUBSIG_SEGERROR      5

#define SIGSYS		12

#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGURG          16

#define SIGSTOP		17
#define SIGTSTP		18
#define SIGCONT		19
#define SIGCHLD		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGIO		23
#define SIGPOLL		SIGIO   
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGLOST		29
#define SIGPWR		SIGLOST
#define SIGUSR1		30
#define SIGUSR2		31


#define __OLD_NSIG	32
#define __NEW_NSIG      64
#ifdef __arch64__
#define _NSIG_BPW       64
#else
#define _NSIG_BPW       32
#endif
#define _NSIG_WORDS     (__NEW_NSIG / _NSIG_BPW)

#define SIGRTMIN       32
#define SIGRTMAX       __NEW_NSIG

#if defined(__KERNEL__) || defined(__WANT_POSIX1B_SIGNALS__)
#define _NSIG			__NEW_NSIG
#define __new_sigset_t		sigset_t
#define __new_sigaction		sigaction
#define __new_sigaction32	sigaction32
#define __old_sigset_t		old_sigset_t
#define __old_sigaction		old_sigaction
#define __old_sigaction32	old_sigaction32
#else
#define _NSIG			__OLD_NSIG
#define NSIG			_NSIG
#define __old_sigset_t		sigset_t
#define __old_sigaction		sigaction
#define __old_sigaction32	sigaction32
#endif

#ifndef __ASSEMBLY__

typedef unsigned long __old_sigset_t;            

typedef struct {
       unsigned long sig[_NSIG_WORDS];
} __new_sigset_t;

struct sigstack {
	
	char *the_stack;
	int   cur_status;
};

#define _SV_SSTACK    1u    
#define _SV_INTR      2u    
#define _SV_RESET     4u    
#define _SV_IGNCHILD  8u    

#define SA_NOCLDSTOP	_SV_IGNCHILD
#define SA_STACK	_SV_SSTACK
#define SA_ONSTACK	_SV_SSTACK
#define SA_RESTART	_SV_INTR
#define SA_ONESHOT	_SV_RESET
#define SA_NODEFER	0x20u
#define SA_NOCLDWAIT    0x100u
#define SA_SIGINFO      0x200u

#define SA_NOMASK	SA_NODEFER

#define SIG_BLOCK          0x01	
#define SIG_UNBLOCK        0x02	
#define SIG_SETMASK        0x04	

#define SS_ONSTACK	1
#define SS_DISABLE	2

#define MINSIGSTKSZ	4096
#define SIGSTKSZ	16384

#ifdef __KERNEL__
#define SA_STATIC_ALLOC         0x8000
#endif

#include <asm-generic/signal-defs.h>

struct __new_sigaction {
	__sighandler_t		sa_handler;
	unsigned long		sa_flags;
	__sigrestore_t		sa_restorer;  
	__new_sigset_t		sa_mask;
};

struct __old_sigaction {
	__sighandler_t		sa_handler;
	__old_sigset_t		sa_mask;
	unsigned long		sa_flags;
	void			(*sa_restorer)(void);  
};

typedef struct sigaltstack {
	void			__user *ss_sp;
	int			ss_flags;
	size_t			ss_size;
} stack_t;

#ifdef __KERNEL__

struct k_sigaction {
	struct			__new_sigaction sa;
	void			__user *ka_restorer;
};

#define ptrace_signal_deliver(regs, cookie) do { } while (0)

#endif 

#endif 

#endif 
