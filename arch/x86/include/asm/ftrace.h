#ifndef _ASM_X86_FTRACE_H
#define _ASM_X86_FTRACE_H

#ifdef __ASSEMBLY__

	.macro MCOUNT_SAVE_FRAME
	
	subq $0x38, %rsp
	movq %rax, (%rsp)
	movq %rcx, 8(%rsp)
	movq %rdx, 16(%rsp)
	movq %rsi, 24(%rsp)
	movq %rdi, 32(%rsp)
	movq %r8, 40(%rsp)
	movq %r9, 48(%rsp)
	.endm

	.macro MCOUNT_RESTORE_FRAME
	movq 48(%rsp), %r9
	movq 40(%rsp), %r8
	movq 32(%rsp), %rdi
	movq 24(%rsp), %rsi
	movq 16(%rsp), %rdx
	movq 8(%rsp), %rcx
	movq (%rsp), %rax
	addq $0x38, %rsp
	.endm

#endif

#ifdef CONFIG_FUNCTION_TRACER
#define MCOUNT_ADDR		((long)(mcount))
#define MCOUNT_INSN_SIZE	5 

#ifndef __ASSEMBLY__
extern void mcount(void);

static inline unsigned long ftrace_call_adjust(unsigned long addr)
{
	return addr;
}

#ifdef CONFIG_DYNAMIC_FTRACE

struct dyn_arch_ftrace {
	
};

#endif 
#endif 
#endif 

#endif 
