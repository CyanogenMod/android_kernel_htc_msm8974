#ifndef _PPC64_PPC32_H
#define _PPC64_PPC32_H

#include <linux/compat.h>
#include <asm/siginfo.h>
#include <asm/signal.h>

/*
 * Data types and macros for providing 32b PowerPC support.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


typedef struct compat_siginfo {
	int si_signo;
	int si_errno;
	int si_code;

	union {
		int _pad[SI_PAD_SIZE32];

		
		struct {
			compat_pid_t _pid;		
			compat_uid_t _uid;		
		} _kill;

		
		struct {
			compat_timer_t _tid;			
			int _overrun;			
			compat_sigval_t _sigval;		
			int _sys_private;		
		} _timer;

		
		struct {
			compat_pid_t _pid;		
			compat_uid_t _uid;		
			compat_sigval_t _sigval;
		} _rt;

		
		struct {
			compat_pid_t _pid;		
			compat_uid_t _uid;		
			int _status;			
			compat_clock_t _utime;
			compat_clock_t _stime;
		} _sigchld;

		
		struct {
			unsigned int _addr; 
		} _sigfault;

		
		struct {
			int _band;	
			int _fd;
		} _sigpoll;
	} _sifields;
} compat_siginfo_t;

#define __old_sigaction32	old_sigaction32

struct __old_sigaction32 {
	compat_uptr_t		sa_handler;
	compat_old_sigset_t  	sa_mask;
	unsigned int    	sa_flags;
	compat_uptr_t		sa_restorer;     
};



struct sigaction32 {
       compat_uptr_t  sa_handler;	
       unsigned int sa_flags;
       compat_uptr_t sa_restorer;	
       compat_sigset_t sa_mask;		
};

typedef struct sigaltstack_32 {
	unsigned int ss_sp;
	int ss_flags;
	compat_size_t ss_size;
} stack_32_t;

struct pt_regs32 {
	unsigned int gpr[32];
	unsigned int nip;
	unsigned int msr;
	unsigned int orig_gpr3;		
	unsigned int ctr;
	unsigned int link;
	unsigned int xer;
	unsigned int ccr;
	unsigned int mq;		
	unsigned int trap;		
	unsigned int dar;		
	unsigned int dsisr;
	unsigned int result;		
};

struct sigcontext32 {
	unsigned int	_unused[4];
	int		signal;
	compat_uptr_t	handler;
	unsigned int	oldmask;
	compat_uptr_t	regs;  
};

struct mcontext32 {
	elf_gregset_t32		mc_gregs;
	elf_fpregset_t		mc_fregs;
	unsigned int		mc_pad[2];
	elf_vrregset_t32	mc_vregs __attribute__((__aligned__(16)));
	elf_vsrreghalf_t32      mc_vsregs __attribute__((__aligned__(16)));
};

struct ucontext32 { 
	unsigned int	  	uc_flags;
	unsigned int 	  	uc_link;
	stack_32_t	 	uc_stack;
	int		 	uc_pad[7];
	compat_uptr_t		uc_regs;	
	compat_sigset_t	 	uc_sigmask;	
	
	int		 	uc_maskext[30];
	int		 	uc_pad2[3];
	struct mcontext32	uc_mcontext;
};

#endif  
