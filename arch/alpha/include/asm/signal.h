#ifndef _ASMAXP_SIGNAL_H
#define _ASMAXP_SIGNAL_H

#include <linux/types.h>

struct siginfo;

#ifdef __KERNEL__

#define _NSIG		64
#define _NSIG_BPW	64
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

typedef unsigned long old_sigset_t;		

typedef struct {
	unsigned long sig[_NSIG_WORDS];
} sigset_t;

#else

#define NSIG		32
typedef unsigned long sigset_t;

#endif 


#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGEMT		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGBUS		10
#define SIGSEGV		11
#define SIGSYS		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGURG		16
#define SIGSTOP		17
#define SIGTSTP		18
#define SIGCONT		19
#define SIGCHLD		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGIO		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGINFO		29
#define SIGUSR1		30
#define SIGUSR2		31

#define SIGPOLL	SIGIO
#define SIGPWR	SIGINFO
#define SIGIOT	SIGABRT

#define SIGRTMIN	32
#define SIGRTMAX	_NSIG


#define SA_ONSTACK	0x00000001
#define SA_RESTART	0x00000002
#define SA_NOCLDSTOP	0x00000004
#define SA_NODEFER	0x00000008
#define SA_RESETHAND	0x00000010
#define SA_NOCLDWAIT	0x00000020
#define SA_SIGINFO	0x00000040

#define SA_ONESHOT	SA_RESETHAND
#define SA_NOMASK	SA_NODEFER

#define SS_ONSTACK	1
#define SS_DISABLE	2

#define MINSIGSTKSZ	4096
#define SIGSTKSZ	16384

#define SIG_BLOCK          1	
#define SIG_UNBLOCK        2	
#define SIG_SETMASK        3	

#include <asm-generic/signal-defs.h>

#ifdef __KERNEL__
struct osf_sigaction {
	__sighandler_t	sa_handler;
	old_sigset_t	sa_mask;
	int		sa_flags;
};

struct sigaction {
	__sighandler_t	sa_handler;
	unsigned long	sa_flags;
	sigset_t	sa_mask;	
};

struct k_sigaction {
	struct sigaction sa;
	__sigrestore_t ka_restorer;
};
#else

struct sigaction {
	union {
	  __sighandler_t	_sa_handler;
	  void (*_sa_sigaction)(int, struct siginfo *, void *);
	} _u;
	sigset_t	sa_mask;
	int		sa_flags;
};

#define sa_handler	_u._sa_handler
#define sa_sigaction	_u._sa_sigaction

#endif 

typedef struct sigaltstack {
	void __user *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;


struct sigstack {
	void __user *ss_sp;
	int ss_onstack;
};

#ifdef __KERNEL__
#include <asm/sigcontext.h>

#define ptrace_signal_deliver(regs, cookie) do { } while (0)

#endif

#endif
