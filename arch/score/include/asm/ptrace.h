#ifndef _ASM_SCORE_PTRACE_H
#define _ASM_SCORE_PTRACE_H

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13

#define PC		32
#define CONDITION	33
#define ECR		34
#define EMA		35
#define CEH		36
#define CEL		37
#define COUNTER		38
#define LDCR		39
#define STCR		40
#define PSR		41

#define SINGLESTEP16_INSN	0x7006
#define SINGLESTEP32_INSN	0x840C8000
#define BREAKPOINT16_INSN	0x7002		
#define BREAKPOINT32_INSN	0x84048000	

#define INSN32_MASK	0x80008000

#define J32	0x88008000	
#define J32M	0xFC008000	

#define B32	0x90008000	
#define B32M	0xFC008000
#define BL32	0x90008001	
#define BL32M	B32
#define BR32	0x80008008	
#define BR32M	0xFFE0807E
#define BRL32	0x80008009	
#define BRL32M	BR32M

#define B32_SET	(J32 | B32 | BL32 | BR32 | BRL32)

#define J16	0x3000		
#define J16M	0xF000
#define B16	0x4000		
#define B16M	0xF000
#define BR16	0x0004		
#define BR16M	0xF00F
#define B16_SET (J16 | B16 | BR16)


struct pt_regs {
	unsigned long pad0[6];	
	unsigned long orig_r4;
	unsigned long orig_r7;
	long is_syscall;

	unsigned long regs[32];

	unsigned long cel;
	unsigned long ceh;

	unsigned long sr0;	
	unsigned long sr1;	
	unsigned long sr2;	

	unsigned long cp0_epc;
	unsigned long cp0_ema;
	unsigned long cp0_psr;
	unsigned long cp0_ecr;
	unsigned long cp0_condition;
};

#ifdef __KERNEL__

struct task_struct;

#define user_mode(regs) 	((regs->cp0_psr & 8) == 8)

#define instruction_pointer(regs)	((unsigned long)(regs)->cp0_epc)
#define profile_pc(regs)		instruction_pointer(regs)

extern void do_syscall_trace(struct pt_regs *regs, int entryexit);
extern int read_tsk_long(struct task_struct *, unsigned long, unsigned long *);
extern int read_tsk_short(struct task_struct *, unsigned long,
			 unsigned short *);

#define arch_has_single_step()	(1)

#endif 

#endif 
