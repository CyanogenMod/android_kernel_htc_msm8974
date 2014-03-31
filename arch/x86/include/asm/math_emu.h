#ifndef _ASM_X86_MATH_EMU_H
#define _ASM_X86_MATH_EMU_H

#include <asm/ptrace.h>
#include <asm/vm86.h>

struct math_emu_info {
	long ___orig_eip;
	union {
		struct pt_regs *regs;
		struct kernel_vm86_regs *vm86;
	};
};
#endif 
