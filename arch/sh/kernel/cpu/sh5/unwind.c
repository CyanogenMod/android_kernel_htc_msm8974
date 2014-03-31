/*
 * arch/sh/kernel/cpu/sh5/unwind.c
 *
 * Copyright (C) 2004  Paul Mundt
 * Copyright (C) 2004  Richard Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/io.h>

static u8 regcache[63];

static int lookup_prev_stack_frame(unsigned long fp, unsigned long pc,
		      unsigned long *pprev_fp, unsigned long *pprev_pc,
		      struct pt_regs *regs)
{
	const char *sym;
	char namebuf[128];
	unsigned long offset;
	unsigned long prologue = 0;
	unsigned long fp_displacement = 0;
	unsigned long fp_prev = 0;
	unsigned long offset_r14 = 0, offset_r18 = 0;
	int i, found_prologue_end = 0;

	sym = kallsyms_lookup(pc, NULL, &offset, NULL, namebuf);
	if (!sym)
		return -EINVAL;

	prologue = pc - offset;
	if (!prologue)
		return -EINVAL;

	if ((fp < (unsigned long) phys_to_virt(__MEMORY_START)) ||
	    (fp >= (unsigned long)(phys_to_virt(__MEMORY_START)) + 128*1024*1024) ||
	    ((fp & 7) != 0)) {
		return -EINVAL;
	}

	for (i = 0; i < 100; i++, prologue += sizeof(unsigned long)) {
		unsigned long op;
		u8 major, minor;
		u8 src, dest, disp;

		op = *(unsigned long *)prologue;

		major = (op >> 26) & 0x3f;
		src   = (op >> 20) & 0x3f;
		minor = (op >> 16) & 0xf;
		disp  = (op >> 10) & 0x3f;
		dest  = (op >>  4) & 0x3f;


		switch (major) {
		case (0x00 >> 2):
			switch (minor) {
			case 0x8: 
			case 0x9: 
				
				if (src == 15 && disp == 63 && dest == 14)
					found_prologue_end = 1;

				break;
			case 0xa: 
			case 0xb: 
				if (src != 15 || dest != 15)
					continue;

				fp_displacement -= regcache[disp];
				fp_prev = fp - fp_displacement;
				break;
			}
			break;
		case (0xa8 >> 2): 
			if (src != 15)
				continue;

			switch (dest) {
			case 14:
				if (offset_r14 || fp_displacement == 0)
					continue;

				offset_r14 = (u64)(((((s64)op >> 10) & 0x3ff) << 54) >> 54);
				offset_r14 *= sizeof(unsigned long);
				offset_r14 += fp_displacement;
				break;
			case 18:
				if (offset_r18 || fp_displacement == 0)
					continue;

				offset_r18 = (u64)(((((s64)op >> 10) & 0x3ff) << 54) >> 54);
				offset_r18 *= sizeof(unsigned long);
				offset_r18 += fp_displacement;
				break;
			}

			break;
		case (0xcc >> 2): 
			if (dest >= 63) {
				printk(KERN_NOTICE "%s: Invalid dest reg %d "
				       "specified in movi handler. Failed "
				       "opcode was 0x%lx: ", __func__,
				       dest, op);

				continue;
			}

			
			regcache[dest] =
				((((s64)(u64)op >> 10) & 0xffff) << 54) >> 54;
			break;
		case (0xd0 >> 2): 
		case (0xd4 >> 2): 
			
			if (src != 15 || dest != 15)
				continue;

			
			fp_displacement +=
				(u64)(((((s64)op >> 10) & 0x3ff) << 54) >> 54);
			fp_prev = fp - fp_displacement;
			break;
		}

		if (found_prologue_end && offset_r14 && (offset_r18 || *pprev_pc) && fp_prev)
			break;
	}

	if (offset_r14 == 0 || fp_prev == 0) {
		if (!offset_r14)
			pr_debug("Unable to find r14 offset\n");
		if (!fp_prev)
			pr_debug("Unable to find previous fp\n");

		return -EINVAL;
	}

	
	if (!*pprev_pc && (offset_r18 == 0))
		return -EINVAL;

	*pprev_fp = *(unsigned long *)(fp_prev + offset_r14);

	if (offset_r18)
		*pprev_pc = *(unsigned long *)(fp_prev + offset_r18);

	*pprev_pc &= ~1;

	return 0;
}

static struct pt_regs here_regs;

extern const char syscall_ret;
extern const char ret_from_syscall;
extern const char ret_from_exception;
extern const char ret_from_irq;

static void sh64_unwind_inner(struct pt_regs *regs);

static void unwind_nested (unsigned long pc, unsigned long fp)
{
	if ((fp >= __MEMORY_START) &&
	    ((fp & 7) == 0)) {
		sh64_unwind_inner((struct pt_regs *) fp);
	}
}

static void sh64_unwind_inner(struct pt_regs *regs)
{
	unsigned long pc, fp;
	int ofs = 0;
	int first_pass;

	pc = regs->pc & ~1;
	fp = regs->regs[14];

	first_pass = 1;
	for (;;) {
		int cond;
		unsigned long next_fp, next_pc;

		if (pc == ((unsigned long) &syscall_ret & ~1)) {
			printk("SYSCALL\n");
			unwind_nested(pc,fp);
			return;
		}

		if (pc == ((unsigned long) &ret_from_syscall & ~1)) {
			printk("SYSCALL (PREEMPTED)\n");
			unwind_nested(pc,fp);
			return;
		}

		if (pc == ((unsigned long) &ret_from_exception & ~1)) {
			printk("EXCEPTION\n");
			unwind_nested(pc,fp);
			return;
		}

		if (pc == ((unsigned long) &ret_from_irq & ~1)) {
			printk("IRQ\n");
			unwind_nested(pc,fp);
			return;
		}

		cond = ((pc >= __MEMORY_START) && (fp >= __MEMORY_START) &&
			((pc & 3) == 0) && ((fp & 7) == 0));

		pc -= ofs;

		printk("[<%08lx>] ", pc);
		print_symbol("%s\n", pc);

		if (first_pass) {
			next_pc = regs->regs[18];
		} else {
			next_pc = 0;
		}

		if (lookup_prev_stack_frame(fp, pc, &next_fp, &next_pc, regs) == 0) {
			ofs = sizeof(unsigned long);
			pc = next_pc & ~1;
			fp = next_fp;
		} else {
			printk("Unable to lookup previous stack frame\n");
			break;
		}
		first_pass = 0;
	}

	printk("\n");

}

void sh64_unwind(struct pt_regs *regs)
{
	if (!regs) {
		regs = &here_regs;

		__asm__ __volatile__ ("ori r14, 0, %0" : "=r" (regs->regs[14]));
		__asm__ __volatile__ ("ori r15, 0, %0" : "=r" (regs->regs[15]));
		__asm__ __volatile__ ("ori r18, 0, %0" : "=r" (regs->regs[18]));

		__asm__ __volatile__ ("gettr tr0, %0" : "=r" (regs->tregs[0]));
		__asm__ __volatile__ ("gettr tr1, %0" : "=r" (regs->tregs[1]));
		__asm__ __volatile__ ("gettr tr2, %0" : "=r" (regs->tregs[2]));
		__asm__ __volatile__ ("gettr tr3, %0" : "=r" (regs->tregs[3]));
		__asm__ __volatile__ ("gettr tr4, %0" : "=r" (regs->tregs[4]));
		__asm__ __volatile__ ("gettr tr5, %0" : "=r" (regs->tregs[5]));
		__asm__ __volatile__ ("gettr tr6, %0" : "=r" (regs->tregs[6]));
		__asm__ __volatile__ ("gettr tr7, %0" : "=r" (regs->tregs[7]));

		__asm__ __volatile__ (
			"pta 0f, tr0\n\t"
			"blink tr0, %0\n\t"
			"0: nop"
			: "=r" (regs->pc)
		);
	}

	printk("\nCall Trace:\n");
	sh64_unwind_inner(regs);
}

