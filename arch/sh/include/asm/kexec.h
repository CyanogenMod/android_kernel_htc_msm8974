#ifndef __ASM_SH_KEXEC_H
#define __ASM_SH_KEXEC_H

#include <asm/ptrace.h>
#include <asm/string.h>


#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)
#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)
#define KEXEC_CONTROL_MEMORY_LIMIT TASK_SIZE

#define KEXEC_CONTROL_PAGE_SIZE	4096

#define KEXEC_ARCH KEXEC_ARCH_SH

#ifdef CONFIG_KEXEC
void reserve_crashkernel(void);

static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs)
		memcpy(newregs, oldregs, sizeof(*newregs));
	else {
		__asm__ __volatile__ ("mov r0, %0" : "=r" (newregs->regs[0]));
		__asm__ __volatile__ ("mov r1, %0" : "=r" (newregs->regs[1]));
		__asm__ __volatile__ ("mov r2, %0" : "=r" (newregs->regs[2]));
		__asm__ __volatile__ ("mov r3, %0" : "=r" (newregs->regs[3]));
		__asm__ __volatile__ ("mov r4, %0" : "=r" (newregs->regs[4]));
		__asm__ __volatile__ ("mov r5, %0" : "=r" (newregs->regs[5]));
		__asm__ __volatile__ ("mov r6, %0" : "=r" (newregs->regs[6]));
		__asm__ __volatile__ ("mov r7, %0" : "=r" (newregs->regs[7]));
		__asm__ __volatile__ ("mov r8, %0" : "=r" (newregs->regs[8]));
		__asm__ __volatile__ ("mov r9, %0" : "=r" (newregs->regs[9]));
		__asm__ __volatile__ ("mov r10, %0" : "=r" (newregs->regs[10]));
		__asm__ __volatile__ ("mov r11, %0" : "=r" (newregs->regs[11]));
		__asm__ __volatile__ ("mov r12, %0" : "=r" (newregs->regs[12]));
		__asm__ __volatile__ ("mov r13, %0" : "=r" (newregs->regs[13]));
		__asm__ __volatile__ ("mov r14, %0" : "=r" (newregs->regs[14]));
		__asm__ __volatile__ ("mov r15, %0" : "=r" (newregs->regs[15]));

		__asm__ __volatile__ ("sts pr, %0" : "=r" (newregs->pr));
		__asm__ __volatile__ ("sts macl, %0" : "=r" (newregs->macl));
		__asm__ __volatile__ ("sts mach, %0" : "=r" (newregs->mach));

		__asm__ __volatile__ ("stc gbr, %0" : "=r" (newregs->gbr));
		__asm__ __volatile__ ("stc sr, %0" : "=r" (newregs->sr));

		newregs->pc = (unsigned long)current_text_addr();
	}
}
#else
static inline void reserve_crashkernel(void) { }
#endif 

#endif 
