#ifndef __X86_KERNEL_KPROBES_COMMON_H
#define __X86_KERNEL_KPROBES_COMMON_H


#ifdef CONFIG_X86_64
#define SAVE_REGS_STRING			\
			\
	"	subq $24, %rsp\n"		\
	"	pushq %rdi\n"			\
	"	pushq %rsi\n"			\
	"	pushq %rdx\n"			\
	"	pushq %rcx\n"			\
	"	pushq %rax\n"			\
	"	pushq %r8\n"			\
	"	pushq %r9\n"			\
	"	pushq %r10\n"			\
	"	pushq %r11\n"			\
	"	pushq %rbx\n"			\
	"	pushq %rbp\n"			\
	"	pushq %r12\n"			\
	"	pushq %r13\n"			\
	"	pushq %r14\n"			\
	"	pushq %r15\n"
#define RESTORE_REGS_STRING			\
	"	popq %r15\n"			\
	"	popq %r14\n"			\
	"	popq %r13\n"			\
	"	popq %r12\n"			\
	"	popq %rbp\n"			\
	"	popq %rbx\n"			\
	"	popq %r11\n"			\
	"	popq %r10\n"			\
	"	popq %r9\n"			\
	"	popq %r8\n"			\
	"	popq %rax\n"			\
	"	popq %rcx\n"			\
	"	popq %rdx\n"			\
	"	popq %rsi\n"			\
	"	popq %rdi\n"			\
			\
	"	addq $24, %rsp\n"
#else
#define SAVE_REGS_STRING			\
		\
	"	subl $16, %esp\n"		\
	"	pushl %fs\n"			\
	"	pushl %es\n"			\
	"	pushl %ds\n"			\
	"	pushl %eax\n"			\
	"	pushl %ebp\n"			\
	"	pushl %edi\n"			\
	"	pushl %esi\n"			\
	"	pushl %edx\n"			\
	"	pushl %ecx\n"			\
	"	pushl %ebx\n"
#define RESTORE_REGS_STRING			\
	"	popl %ebx\n"			\
	"	popl %ecx\n"			\
	"	popl %edx\n"			\
	"	popl %esi\n"			\
	"	popl %edi\n"			\
	"	popl %ebp\n"			\
	"	popl %eax\n"			\
	\
	"	addl $24, %esp\n"
#endif

extern int can_boost(kprobe_opcode_t *instruction);
extern unsigned long recover_probed_instruction(kprobe_opcode_t *buf,
					 unsigned long addr);
extern int __copy_instruction(u8 *dest, u8 *src);

extern void synthesize_reljump(void *from, void *to);
extern void synthesize_relcall(void *from, void *to);

#ifdef	CONFIG_OPTPROBES
extern int arch_init_optprobes(void);
extern int setup_detour_execution(struct kprobe *p, struct pt_regs *regs, int reenter);
extern unsigned long __recover_optprobed_insn(kprobe_opcode_t *buf, unsigned long addr);
#else	
static inline int arch_init_optprobes(void)
{
	return 0;
}
static inline int setup_detour_execution(struct kprobe *p, struct pt_regs *regs, int reenter)
{
	return 0;
}
static inline unsigned long __recover_optprobed_insn(kprobe_opcode_t *buf, unsigned long addr)
{
	return addr;
}
#endif
#endif
