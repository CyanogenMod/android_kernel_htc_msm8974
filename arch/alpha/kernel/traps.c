/*
 * arch/alpha/kernel/traps.c
 *
 * (C) Copyright 1994 Linus Torvalds
 */


#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/ratelimit.h>

#include <asm/gentrap.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <asm/sysinfo.h>
#include <asm/hwrpb.h>
#include <asm/mmu_context.h>
#include <asm/special_insns.h>

#include "proto.h"


static int opDEC_fix;

static void __cpuinit
opDEC_check(void)
{
	__asm__ __volatile__ (
	
	"	br	$16, 1f\n"
	"	ldq	$16, 8($sp)\n"
	"	addq	$16, 4, $16\n"
	"	stq	$16, 8($sp)\n"
	"	call_pal %[rti]\n"
	
	"1:	lda	$17, 3\n"
	"	call_pal %[wrent]\n"
	"	lda %[fix], 0\n"
	"	cvttq/svm $f31,$f31\n"
	"	lda %[fix], 4"
	: [fix] "=r" (opDEC_fix)
	: [rti] "n" (PAL_rti), [wrent] "n" (PAL_wrent)
	: "$0", "$1", "$16", "$17", "$22", "$23", "$24", "$25");

	if (opDEC_fix)
		printk("opDEC fixup enabled.\n");
}

void
dik_show_regs(struct pt_regs *regs, unsigned long *r9_15)
{
	printk("pc = [<%016lx>]  ra = [<%016lx>]  ps = %04lx    %s\n",
	       regs->pc, regs->r26, regs->ps, print_tainted());
	print_symbol("pc is at %s\n", regs->pc);
	print_symbol("ra is at %s\n", regs->r26 );
	printk("v0 = %016lx  t0 = %016lx  t1 = %016lx\n",
	       regs->r0, regs->r1, regs->r2);
	printk("t2 = %016lx  t3 = %016lx  t4 = %016lx\n",
 	       regs->r3, regs->r4, regs->r5);
	printk("t5 = %016lx  t6 = %016lx  t7 = %016lx\n",
	       regs->r6, regs->r7, regs->r8);

	if (r9_15) {
		printk("s0 = %016lx  s1 = %016lx  s2 = %016lx\n",
		       r9_15[9], r9_15[10], r9_15[11]);
		printk("s3 = %016lx  s4 = %016lx  s5 = %016lx\n",
		       r9_15[12], r9_15[13], r9_15[14]);
		printk("s6 = %016lx\n", r9_15[15]);
	}

	printk("a0 = %016lx  a1 = %016lx  a2 = %016lx\n",
	       regs->r16, regs->r17, regs->r18);
	printk("a3 = %016lx  a4 = %016lx  a5 = %016lx\n",
 	       regs->r19, regs->r20, regs->r21);
 	printk("t8 = %016lx  t9 = %016lx  t10= %016lx\n",
	       regs->r22, regs->r23, regs->r24);
	printk("t11= %016lx  pv = %016lx  at = %016lx\n",
	       regs->r25, regs->r27, regs->r28);
	printk("gp = %016lx  sp = %p\n", regs->gp, regs+1);
#if 0
__halt();
#endif
}

#if 0
static char * ireg_name[] = {"v0", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			   "t7", "s0", "s1", "s2", "s3", "s4", "s5", "s6",
			   "a0", "a1", "a2", "a3", "a4", "a5", "t8", "t9",
			   "t10", "t11", "ra", "pv", "at", "gp", "sp", "zero"};
#endif

static void
dik_show_code(unsigned int *pc)
{
	long i;

	printk("Code:");
	for (i = -6; i < 2; i++) {
		unsigned int insn;
		if (__get_user(insn, (unsigned int __user *)pc + i))
			break;
		printk("%c%08x%c", i ? ' ' : '<', insn, i ? ' ' : '>');
	}
	printk("\n");
}

static void
dik_show_trace(unsigned long *sp)
{
	long i = 0;
	printk("Trace:\n");
	while (0x1ff8 & (unsigned long) sp) {
		extern char _stext[], _etext[];
		unsigned long tmp = *sp;
		sp++;
		if (tmp < (unsigned long) &_stext)
			continue;
		if (tmp >= (unsigned long) &_etext)
			continue;
		printk("[<%lx>]", tmp);
		print_symbol(" %s", tmp);
		printk("\n");
		if (i > 40) {
			printk(" ...");
			break;
		}
	}
	printk("\n");
}

static int kstack_depth_to_print = 24;

void show_stack(struct task_struct *task, unsigned long *sp)
{
	unsigned long *stack;
	int i;

	if(sp==NULL)
		sp=(unsigned long*)&sp;

	stack = sp;
	for(i=0; i < kstack_depth_to_print; i++) {
		if (((long) stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i && ((i % 4) == 0))
			printk("\n       ");
		printk("%016lx ", *stack++);
	}
	printk("\n");
	dik_show_trace(sp);
}

void dump_stack(void)
{
	show_stack(NULL, NULL);
}

EXPORT_SYMBOL(dump_stack);

void
die_if_kernel(char * str, struct pt_regs *regs, long err, unsigned long *r9_15)
{
	if (regs->ps & 8)
		return;
#ifdef CONFIG_SMP
	printk("CPU %d ", hard_smp_processor_id());
#endif
	printk("%s(%d): %s %ld\n", current->comm, task_pid_nr(current), str, err);
	dik_show_regs(regs, r9_15);
	add_taint(TAINT_DIE);
	dik_show_trace((unsigned long *)(regs+1));
	dik_show_code((unsigned int *)regs->pc);

	if (test_and_set_thread_flag (TIF_DIE_IF_KERNEL)) {
		printk("die_if_kernel recursion detected.\n");
		local_irq_enable();
		while (1);
	}
	do_exit(SIGSEGV);
}

#ifndef CONFIG_MATHEMU
static long dummy_emul(void) { return 0; }
long (*alpha_fp_emul_imprecise)(struct pt_regs *regs, unsigned long writemask)
  = (void *)dummy_emul;
long (*alpha_fp_emul) (unsigned long pc)
  = (void *)dummy_emul;
#else
long alpha_fp_emul_imprecise(struct pt_regs *regs, unsigned long writemask);
long alpha_fp_emul (unsigned long pc);
#endif

asmlinkage void
do_entArith(unsigned long summary, unsigned long write_mask,
	    struct pt_regs *regs)
{
	long si_code = FPE_FLTINV;
	siginfo_t info;

	if (summary & 1) {
		if (!amask(AMASK_PRECISE_TRAP))
			si_code = alpha_fp_emul(regs->pc - 4);
		else
			si_code = alpha_fp_emul_imprecise(regs, write_mask);
		if (si_code == 0)
			return;
	}
	die_if_kernel("Arithmetic fault", regs, 0, NULL);

	info.si_signo = SIGFPE;
	info.si_errno = 0;
	info.si_code = si_code;
	info.si_addr = (void __user *) regs->pc;
	send_sig_info(SIGFPE, &info, current);
}

asmlinkage void
do_entIF(unsigned long type, struct pt_regs *regs)
{
	siginfo_t info;
	int signo, code;

	if ((regs->ps & ~IPL_MAX) == 0) {
		if (type == 1) {
			const unsigned int *data
			  = (const unsigned int *) regs->pc;
			printk("Kernel bug at %s:%d\n",
			       (const char *)(data[1] | (long)data[2] << 32), 
			       data[0]);
		}
		die_if_kernel((type == 1 ? "Kernel Bug" : "Instruction fault"),
			      regs, type, NULL);
	}

	switch (type) {
	      case 0: 
		info.si_signo = SIGTRAP;
		info.si_errno = 0;
		info.si_code = TRAP_BRKPT;
		info.si_trapno = 0;
		info.si_addr = (void __user *) regs->pc;

		if (ptrace_cancel_bpt(current)) {
			regs->pc -= 4;	
		}

		send_sig_info(SIGTRAP, &info, current);
		return;

	      case 1: 
		info.si_signo = SIGTRAP;
		info.si_errno = 0;
		info.si_code = __SI_FAULT;
		info.si_addr = (void __user *) regs->pc;
		info.si_trapno = 0;
		send_sig_info(SIGTRAP, &info, current);
		return;
		
	      case 2: 
		info.si_addr = (void __user *) regs->pc;
		info.si_trapno = regs->r16;
		switch ((long) regs->r16) {
		case GEN_INTOVF:
			signo = SIGFPE;
			code = FPE_INTOVF;
			break;
		case GEN_INTDIV:
			signo = SIGFPE;
			code = FPE_INTDIV;
			break;
		case GEN_FLTOVF:
			signo = SIGFPE;
			code = FPE_FLTOVF;
			break;
		case GEN_FLTDIV:
			signo = SIGFPE;
			code = FPE_FLTDIV;
			break;
		case GEN_FLTUND:
			signo = SIGFPE;
			code = FPE_FLTUND;
			break;
		case GEN_FLTINV:
			signo = SIGFPE;
			code = FPE_FLTINV;
			break;
		case GEN_FLTINE:
			signo = SIGFPE;
			code = FPE_FLTRES;
			break;
		case GEN_ROPRAND:
			signo = SIGFPE;
			code = __SI_FAULT;
			break;

		case GEN_DECOVF:
		case GEN_DECDIV:
		case GEN_DECINV:
		case GEN_ASSERTERR:
		case GEN_NULPTRERR:
		case GEN_STKOVF:
		case GEN_STRLENERR:
		case GEN_SUBSTRERR:
		case GEN_RANGERR:
		case GEN_SUBRNG:
		case GEN_SUBRNG1:
		case GEN_SUBRNG2:
		case GEN_SUBRNG3:
		case GEN_SUBRNG4:
		case GEN_SUBRNG5:
		case GEN_SUBRNG6:
		case GEN_SUBRNG7:
		default:
			signo = SIGTRAP;
			code = __SI_FAULT;
			break;
		}

		info.si_signo = signo;
		info.si_errno = 0;
		info.si_code = code;
		info.si_addr = (void __user *) regs->pc;
		send_sig_info(signo, &info, current);
		return;

	      case 4: 
		if (implver() == IMPLVER_EV4) {
			long si_code;

			regs->pc += opDEC_fix; 
			
			si_code = alpha_fp_emul(regs->pc - 4);
			if (si_code == 0)
				return;
			if (si_code > 0) {
				info.si_signo = SIGFPE;
				info.si_errno = 0;
				info.si_code = si_code;
				info.si_addr = (void __user *) regs->pc;
				send_sig_info(SIGFPE, &info, current);
				return;
			}
		}
		break;

	      case 3: 
		current_thread_info()->pcb.flags |= 1;
		__reload_thread(&current_thread_info()->pcb);
		return;

	      case 5: 
	      default: 
		      ;
	}

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code = ILL_ILLOPC;
	info.si_addr = (void __user *) regs->pc;
	send_sig_info(SIGILL, &info, current);
}


asmlinkage void
do_entDbg(struct pt_regs *regs)
{
	siginfo_t info;

	die_if_kernel("Instruction fault", regs, 0, NULL);

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code = ILL_ILLOPC;
	info.si_addr = (void __user *) regs->pc;
	force_sig_info(SIGILL, &info, current);
}


struct allregs {
	unsigned long regs[32];
	unsigned long ps, pc, gp, a0, a1, a2;
};

struct unaligned_stat {
	unsigned long count, va, pc;
} unaligned[2];


#define una_reg(r)  (_regs[(r) >= 16 && (r) <= 18 ? (r)+19 : (r)])


asmlinkage void
do_entUna(void * va, unsigned long opcode, unsigned long reg,
	  struct allregs *regs)
{
	long error, tmp1, tmp2, tmp3, tmp4;
	unsigned long pc = regs->pc - 4;
	unsigned long *_regs = regs->regs;
	const struct exception_table_entry *fixup;

	unaligned[0].count++;
	unaligned[0].va = (unsigned long) va;
	unaligned[0].pc = pc;


	switch (opcode) {
	case 0x0c: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,1(%3)\n"
		"	extwl %1,%3,%1\n"
		"	extwh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = tmp1|tmp2;
		return;

	case 0x28: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = (int)(tmp1|tmp2);
		return;

	case 0x29: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = tmp1|tmp2;
		return;

	case 0x0d: 
		__asm__ __volatile__(
		"1:	ldq_u %2,1(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inswh %6,%5,%4\n"
		"	inswl %6,%5,%3\n"
		"	mskwh %2,%5,%2\n"
		"	mskwl %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,1(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;

	case 0x2c: 
		__asm__ __volatile__(
		"1:	ldq_u %2,3(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inslh %6,%5,%4\n"
		"	insll %6,%5,%3\n"
		"	msklh %2,%5,%2\n"
		"	mskll %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,3(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;

	case 0x2d: 
		__asm__ __volatile__(
		"1:	ldq_u %2,7(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	insqh %6,%5,%4\n"
		"	insql %6,%5,%3\n"
		"	mskqh %2,%5,%2\n"
		"	mskql %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,7(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n\t"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;
	}

	printk("Bad unaligned kernel access at %016lx: %p %lx %lu\n",
		pc, va, opcode, reg);
	do_exit(SIGSEGV);

got_exception:
	if ((fixup = search_exception_tables(pc)) != 0) {
		unsigned long newpc;
		newpc = fixup_exception(una_reg, fixup, pc);

		printk("Forwarding unaligned exception at %lx (%lx)\n",
		       pc, newpc);

		regs->pc = newpc;
		return;
	}


	printk("%s(%d): unhandled unaligned exception\n",
	       current->comm, task_pid_nr(current));

	printk("pc = [<%016lx>]  ra = [<%016lx>]  ps = %04lx\n",
	       pc, una_reg(26), regs->ps);
	printk("r0 = %016lx  r1 = %016lx  r2 = %016lx\n",
	       una_reg(0), una_reg(1), una_reg(2));
	printk("r3 = %016lx  r4 = %016lx  r5 = %016lx\n",
 	       una_reg(3), una_reg(4), una_reg(5));
	printk("r6 = %016lx  r7 = %016lx  r8 = %016lx\n",
	       una_reg(6), una_reg(7), una_reg(8));
	printk("r9 = %016lx  r10= %016lx  r11= %016lx\n",
	       una_reg(9), una_reg(10), una_reg(11));
	printk("r12= %016lx  r13= %016lx  r14= %016lx\n",
	       una_reg(12), una_reg(13), una_reg(14));
	printk("r15= %016lx\n", una_reg(15));
	printk("r16= %016lx  r17= %016lx  r18= %016lx\n",
	       una_reg(16), una_reg(17), una_reg(18));
	printk("r19= %016lx  r20= %016lx  r21= %016lx\n",
 	       una_reg(19), una_reg(20), una_reg(21));
 	printk("r22= %016lx  r23= %016lx  r24= %016lx\n",
	       una_reg(22), una_reg(23), una_reg(24));
	printk("r25= %016lx  r27= %016lx  r28= %016lx\n",
	       una_reg(25), una_reg(27), una_reg(28));
	printk("gp = %016lx  sp = %p\n", regs->gp, regs+1);

	dik_show_code((unsigned int *)pc);
	dik_show_trace((unsigned long *)(regs+1));

	if (test_and_set_thread_flag (TIF_DIE_IF_KERNEL)) {
		printk("die_if_kernel recursion detected.\n");
		local_irq_enable();
		while (1);
	}
	do_exit(SIGSEGV);
}

static inline unsigned long
s_mem_to_reg (unsigned long s_mem)
{
	unsigned long frac    = (s_mem >>  0) & 0x7fffff;
	unsigned long sign    = (s_mem >> 31) & 0x1;
	unsigned long exp_msb = (s_mem >> 30) & 0x1;
	unsigned long exp_low = (s_mem >> 23) & 0x7f;
	unsigned long exp;

	exp = (exp_msb << 10) | exp_low;	
	if (exp_msb) {
		if (exp_low == 0x7f) {
			exp = 0x7ff;
		}
	} else {
		if (exp_low == 0x00) {
			exp = 0x000;
		} else {
			exp |= (0x7 << 7);
		}
	}
	return (sign << 63) | (exp << 52) | (frac << 29);
}

static inline unsigned long
s_reg_to_mem (unsigned long s_reg)
{
	return ((s_reg >> 62) << 30) | ((s_reg << 5) >> 34);
}


#define OP_INT_MASK	( 1L << 0x28 | 1L << 0x2c   	\
			| 1L << 0x29 | 1L << 0x2d   	\
			| 1L << 0x0c | 1L << 0x0d   	\
			| 1L << 0x0a | 1L << 0x0e ) 

#define OP_WRITE_MASK	( 1L << 0x26 | 1L << 0x27   	\
			| 1L << 0x2c | 1L << 0x2d   	\
			| 1L << 0x0d | 1L << 0x0e ) 

#define R(x)	((size_t) &((struct pt_regs *)0)->x)

static int unauser_reg_offsets[32] = {
	R(r0), R(r1), R(r2), R(r3), R(r4), R(r5), R(r6), R(r7), R(r8),
	
	-56, -48, -40, -32, -24, -16, -8,
	R(r16), R(r17), R(r18),
	R(r19), R(r20), R(r21), R(r22), R(r23), R(r24), R(r25), R(r26),
	R(r27), R(r28), R(gp),
	0, 0
};

#undef R

asmlinkage void
do_entUnaUser(void __user * va, unsigned long opcode,
	      unsigned long reg, struct pt_regs *regs)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 5);

	unsigned long tmp1, tmp2, tmp3, tmp4;
	unsigned long fake_reg, *reg_addr = &fake_reg;
	siginfo_t info;
	long error;


	if (!test_thread_flag (TIF_UAC_NOPRINT)) {
		if (__ratelimit(&ratelimit)) {
			printk("%s(%d): unaligned trap at %016lx: %p %lx %ld\n",
			       current->comm, task_pid_nr(current),
			       regs->pc - 4, va, opcode, reg);
		}
	}
	if (test_thread_flag (TIF_UAC_SIGBUS))
		goto give_sigbus;
	
	if (test_thread_flag (TIF_UAC_NOFIX))
		return;

	if (!__access_ok((unsigned long)va, 0, USER_DS))
		goto give_sigsegv;

	++unaligned[1].count;
	unaligned[1].va = (unsigned long)va;
	unaligned[1].pc = regs->pc - 4;

	if ((1L << opcode) & OP_INT_MASK) {
		
		if (reg < 30) {
			reg_addr = (unsigned long *)
			  ((char *)regs + unauser_reg_offsets[reg]);
		} else if (reg == 30) {
			
			fake_reg = rdusp();
		} else {
			
			fake_reg = 0;
		}
	}


	switch (opcode) {
	case 0x0c: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,1(%3)\n"
		"	extwl %1,%3,%1\n"
		"	extwh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = tmp1|tmp2;
		break;

	case 0x22: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		alpha_write_fp_reg(reg, s_mem_to_reg((int)(tmp1|tmp2)));
		return;

	case 0x23: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		alpha_write_fp_reg(reg, tmp1|tmp2);
		return;

	case 0x28: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = (int)(tmp1|tmp2);
		break;

	case 0x29: 
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %1,3b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = tmp1|tmp2;
		break;

	case 0x0d: 
		__asm__ __volatile__(
		"1:	ldq_u %2,1(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inswh %6,%5,%4\n"
		"	inswl %6,%5,%3\n"
		"	mskwh %2,%5,%2\n"
		"	mskwl %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,1(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	case 0x26: 
		fake_reg = s_reg_to_mem(alpha_read_fp_reg(reg));
		

	case 0x2c: 
		__asm__ __volatile__(
		"1:	ldq_u %2,3(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inslh %6,%5,%4\n"
		"	insll %6,%5,%3\n"
		"	msklh %2,%5,%2\n"
		"	mskll %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,3(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	case 0x27: 
		fake_reg = alpha_read_fp_reg(reg);
		

	case 0x2d: 
		__asm__ __volatile__(
		"1:	ldq_u %2,7(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	insqh %6,%5,%4\n"
		"	insql %6,%5,%3\n"
		"	mskqh %2,%5,%2\n"
		"	mskql %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,7(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n\t"
		"	.long 1b - .\n"
		"	lda %2,5b-1b(%0)\n"
		"	.long 2b - .\n"
		"	lda %1,5b-2b(%0)\n"
		"	.long 3b - .\n"
		"	lda $31,5b-3b(%0)\n"
		"	.long 4b - .\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	default:
		
		goto give_sigbus;
	}

	
	if (reg == 30)
		wrusp(fake_reg);
	return;

give_sigsegv:
	regs->pc -= 4;  
	info.si_signo = SIGSEGV;
	info.si_errno = 0;

	if (!__access_ok((unsigned long)va, 0, USER_DS))
		info.si_code = SEGV_ACCERR;
	else {
		struct mm_struct *mm = current->mm;
		down_read(&mm->mmap_sem);
		if (find_vma(mm, (unsigned long)va))
			info.si_code = SEGV_ACCERR;
		else
			info.si_code = SEGV_MAPERR;
		up_read(&mm->mmap_sem);
	}
	info.si_addr = va;
	send_sig_info(SIGSEGV, &info, current);
	return;

give_sigbus:
	regs->pc -= 4;
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRALN;
	info.si_addr = va;
	send_sig_info(SIGBUS, &info, current);
	return;
}

void __cpuinit
trap_init(void)
{
	
	register unsigned long gptr __asm__("$29");
	wrkgp(gptr);

	if (implver() == IMPLVER_EV4)
		opDEC_check();

	wrent(entArith, 1);
	wrent(entMM, 2);
	wrent(entIF, 3);
	wrent(entUna, 4);
	wrent(entSys, 5);
	wrent(entDbg, 6);
}
