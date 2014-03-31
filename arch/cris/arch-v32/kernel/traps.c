/*
 * Copyright (C) 2003-2006, Axis Communications AB.
 */

#include <linux/ptrace.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <hwregs/supp_reg.h>
#include <hwregs/intr_vect_defs.h>
#include <asm/irq.h>

void show_registers(struct pt_regs *regs)
{
	unsigned long usp = rdusp();
	unsigned long d_mmu_cause;
	unsigned long i_mmu_cause;

	printk("CPU: %d\n", smp_processor_id());

	printk("ERP: %08lx SRP: %08lx  CCS: %08lx USP: %08lx MOF: %08lx\n",
	       regs->erp, regs->srp, regs->ccs, usp, regs->mof);

	printk(" r0: %08lx  r1: %08lx   r2: %08lx  r3: %08lx\n",
	       regs->r0, regs->r1, regs->r2, regs->r3);

	printk(" r4: %08lx  r5: %08lx   r6: %08lx  r7: %08lx\n",
	       regs->r4, regs->r5, regs->r6, regs->r7);

	printk(" r8: %08lx  r9: %08lx  r10: %08lx r11: %08lx\n",
	       regs->r8, regs->r9, regs->r10, regs->r11);

	printk("r12: %08lx r13: %08lx oR10: %08lx acr: %08lx\n",
	       regs->r12, regs->r13, regs->orig_r10, regs->acr);

	printk(" sp: %08lx\n", (unsigned long)regs);

	SUPP_BANK_SEL(BANK_IM);
	SUPP_REG_RD(RW_MM_CAUSE, i_mmu_cause);

	SUPP_BANK_SEL(BANK_DM);
	SUPP_REG_RD(RW_MM_CAUSE, d_mmu_cause);

	printk("       Data MMU Cause: %08lx\n", d_mmu_cause);
	printk("Instruction MMU Cause: %08lx\n", i_mmu_cause);

	printk("Process %s (pid: %d, stackpage=%08lx)\n",
	       current->comm, current->pid, (unsigned long)current);

	if (!user_mode(regs)) {
		int i;

		show_stack(NULL, (unsigned long *)usp);

		if (usp != 0)
			show_stack(NULL, NULL);

		printk("\nCode: ");

		if (regs->erp < PAGE_OFFSET)
			goto bad_value;

		for (i = -12; i < 12; i++) {
			unsigned char c;

			if (__get_user(c, &((unsigned char *)regs->erp)[i])) {
bad_value:
				printk(" Bad IP value.");
				break;
			}

			if (i == 0)
				printk("(%02x) ", c);
			else
				printk("%02x ", c);
		}
		printk("\n");
	}
}

void arch_enable_nmi(void)
{
	unsigned long flags;

	local_save_flags(flags);
	flags |= (1 << 30); 
	local_irq_restore(flags);
}

extern void (*nmi_handler)(struct pt_regs *);
void handle_nmi(struct pt_regs *regs)
{
#ifdef CONFIG_ETRAXFS
	reg_intr_vect_r_nmi r;
#endif

	if (nmi_handler)
		nmi_handler(regs);

#ifdef CONFIG_ETRAXFS
	
	do {
		r = REG_RD(intr_vect, regi_irq, r_nmi);
	} while (r.ext == regk_intr_vect_on);
#endif
}


#ifdef CONFIG_BUG
extern void die_if_kernel(const char *str, struct pt_regs *regs, long err);

struct pt_regs BUG_regs;

void do_BUG(char *file, unsigned int line)
{
	printk("kernel BUG at %s:%d!\n", file, line);
	die_if_kernel("Oops", &BUG_regs, 0);
}
EXPORT_SYMBOL(do_BUG);

void fixup_BUG(struct pt_regs *regs)
{
	BUG_regs = *regs;

#ifdef CONFIG_DEBUG_BUGVERBOSE
	{
		const struct exception_table_entry *fixup;

		fixup = search_exception_tables(instruction_pointer(regs) - 2);
		if (fixup) {
			
			instruction_pointer(regs) = fixup->fixup;
			arch_fixup(regs);
		}
	}
#else
	
	do_BUG("unknown", 0);
#endif
}

__asm__  ( ".text\n\t"
	   ".global breakh_BUG\n\t"
	   "breakh_BUG:\n\t"
	   SAVE_ALL
	   KGDB_FIXUP
	   "move.d $sp, $r10\n\t"
	   "jsr fixup_BUG\n\t"
	   "nop\n\t"
	   "jump ret_from_intr\n\t"
	   "nop\n\t");


#ifdef CONFIG_DEBUG_BUGVERBOSE
void
handle_BUG(struct pt_regs *regs)
{
}
#endif
#endif
