#ifndef _ASM_X86_SIGFRAME_H
#define _ASM_X86_SIGFRAME_H

#include <asm/sigcontext.h>
#include <asm/siginfo.h>
#include <asm/ucontext.h>

#ifdef CONFIG_X86_32
#define sigframe_ia32		sigframe
#define rt_sigframe_ia32	rt_sigframe
#define sigcontext_ia32		sigcontext
#define _fpstate_ia32		_fpstate
#define ucontext_ia32		ucontext
#else 

#ifdef CONFIG_IA32_EMULATION
#include <asm/ia32.h>
#endif 

#endif 

#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
struct sigframe_ia32 {
	u32 pretcode;
	int sig;
	struct sigcontext_ia32 sc;
	struct _fpstate_ia32 fpstate_unused;
#ifdef CONFIG_IA32_EMULATION
	unsigned int extramask[_COMPAT_NSIG_WORDS-1];
#else 
	unsigned long extramask[_NSIG_WORDS-1];
#endif 
	char retcode[8];
	
};

struct rt_sigframe_ia32 {
	u32 pretcode;
	int sig;
	u32 pinfo;
	u32 puc;
#ifdef CONFIG_IA32_EMULATION
	compat_siginfo_t info;
#else 
	struct siginfo info;
#endif 
	struct ucontext_ia32 uc;
	char retcode[8];
	
};
#endif 

#ifdef CONFIG_X86_64

struct rt_sigframe {
	char __user *pretcode;
	struct ucontext uc;
	struct siginfo info;
	
};

#ifdef CONFIG_X86_X32_ABI

struct rt_sigframe_x32 {
	u64 pretcode;
	struct ucontext_x32 uc;
	compat_siginfo_t info;
	
};

#endif 

#endif 

#endif 
