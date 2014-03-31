#ifndef _ASM_X86_VDSO_H
#define _ASM_X86_VDSO_H

#if defined CONFIG_X86_32 || defined CONFIG_COMPAT
extern const char VDSO32_PRELINK[];

#define VDSO32_SYMBOL(base, name)					\
({									\
	extern const char VDSO32_##name[];				\
	(void *)(VDSO32_##name - VDSO32_PRELINK + (unsigned long)(base)); \
})
#endif

extern void __user __kernel_sigreturn;
extern void __user __kernel_rt_sigreturn;

extern const char vdso32_int80_start, vdso32_int80_end;
extern const char vdso32_syscall_start, vdso32_syscall_end;
extern const char vdso32_sysenter_start, vdso32_sysenter_end;

#endif 
