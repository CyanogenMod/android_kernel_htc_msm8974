#ifndef _ASM_IA64_SIGNAL_H
#define _ASM_IA64_SIGNAL_H


#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

#define SIGRTMIN	32
#define SIGRTMAX	_NSIG

#define SA_NOCLDSTOP	0x00000001
#define SA_NOCLDWAIT	0x00000002
#define SA_SIGINFO	0x00000004
#define SA_ONSTACK	0x08000000
#define SA_RESTART	0x10000000
#define SA_NODEFER	0x40000000
#define SA_RESETHAND	0x80000000

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#define SA_RESTORER	0x04000000

#define SS_ONSTACK	1
#define SS_DISABLE	2

#if 1
# define MINSIGSTKSZ	131027	
#else
# define MINSIGSTKSZ	131072	
#endif
#define SIGSTKSZ	262144	

#ifdef __KERNEL__

#define _NSIG		64
#define _NSIG_BPW	64
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#endif 

#include <asm-generic/signal-defs.h>

# ifndef __ASSEMBLY__

#  include <linux/types.h>

struct siginfo;

typedef struct sigaltstack {
	void __user *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

#ifdef __KERNEL__


typedef unsigned long old_sigset_t;

typedef struct {
	unsigned long sig[_NSIG_WORDS];
} sigset_t;

struct sigaction {
	__sighandler_t sa_handler;
	unsigned long sa_flags;
	sigset_t sa_mask;		
};

struct k_sigaction {
	struct sigaction sa;
};

#  include <asm/sigcontext.h>

#define ptrace_signal_deliver(regs, cookie) do { } while (0)

#endif 

# endif 
#endif 
