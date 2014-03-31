/*
 *  linux/arch/h8300/mm/fault.c
 *
 *  Copyright (C) 1998  D. Jeff Dionne <jeff@lineo.ca>,
 *  Copyright (C) 2000  Lineo, Inc.  (www.lineo.com) 
 *
 *  Based on:
 *
 *  linux/arch/m68knommu/mm/fault.c
 *  linux/arch/m68k/mm/fault.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 */

#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>

#include <asm/pgtable.h>

asmlinkage int do_page_fault(struct pt_regs *regs, unsigned long address,
			      unsigned long error_code)
{
#ifdef DEBUG
	printk ("regs->sr=%#x, regs->pc=%#lx, address=%#lx, %ld\n",
		regs->sr, regs->pc, address, error_code);
#endif

	if ((unsigned long) address < PAGE_SIZE) {
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	} else
		printk(KERN_ALERT "Unable to handle kernel access");
	printk(" at virtual address %08lx\n",address);
	if (!user_mode(regs))
		die("Oops", regs, error_code);
	do_exit(SIGKILL);

	return 1;
}

