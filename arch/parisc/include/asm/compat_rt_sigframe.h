#include<linux/compat.h>
#include<linux/compat_siginfo.h>
#include<asm/compat_ucontext.h>

#ifndef _ASM_PARISC_COMPAT_RT_SIGFRAME_H
#define _ASM_PARISC_COMPAT_RT_SIGFRAME_H

struct compat_regfile {
	compat_int_t rf_gr[32];
	compat_int_t rf_iasq[2];
	compat_int_t rf_iaoq[2];
	compat_int_t rf_sar;
};

#define COMPAT_SIGRETURN_TRAMP 4
#define COMPAT_SIGRESTARTBLOCK_TRAMP 5 
#define COMPAT_TRAMP_SIZE (COMPAT_SIGRETURN_TRAMP + COMPAT_SIGRESTARTBLOCK_TRAMP)

struct compat_rt_sigframe {
	compat_uint_t tramp[COMPAT_TRAMP_SIZE];
	compat_siginfo_t info;
	struct compat_ucontext uc;
	
	struct compat_regfile regs; 
};

#define SIGFRAME32		64
#define FUNCTIONCALLFRAME32	48
#define PARISC_RT_SIGFRAME_SIZE32					\
	(((sizeof(struct compat_rt_sigframe) + FUNCTIONCALLFRAME32) + SIGFRAME32) & -SIGFRAME32)

#endif
