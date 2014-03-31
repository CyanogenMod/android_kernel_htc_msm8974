/*
 * OpenRISC fault.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/uaccess.h>
#include <asm/siginfo.h>
#include <asm/signal.h>

#define NUM_TLB_ENTRIES 64
#define TLB_OFFSET(add) (((add) >> PAGE_SHIFT) & (NUM_TLB_ENTRIES-1))

unsigned long pte_misses;	
unsigned long pte_errors;	

volatile pgd_t *current_pgd;

extern void die(char *, struct pt_regs *, long);


asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long address,
			      unsigned long vector, int write_acc)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	siginfo_t info;
	int fault;

	tsk = current;


	if (address >= VMALLOC_START &&
	    (vector != 0x300 && vector != 0x400) &&
	    !user_mode(regs))
		goto vmalloc_fault;

	
	if (user_mode(regs)) {
		
		local_irq_enable();
	} else {
		if (regs->sr && (SPR_SR_IEE | SPR_SR_TEE))
			local_irq_enable();
	}

	mm = tsk->mm;
	info.si_code = SEGV_MAPERR;


	if (in_interrupt() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);

	if (!vma)
		goto bad_area;

	if (vma->vm_start <= address)
		goto good_area;

	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (user_mode(regs)) {
		if (address + PAGE_SIZE < regs->sp)
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;


good_area:
	info.si_code = SEGV_ACCERR;

	

	if (write_acc) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
	}

	
	if ((vector == 0x400) && !(vma->vm_page_prot.pgprot & _PAGE_EXEC))
		goto bad_area;


	fault = handle_mm_fault(mm, vma, address, write_acc);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	
	if (fault & VM_FAULT_MAJOR)
		tsk->maj_flt++;
	else
		tsk->min_flt++;

	up_read(&mm->mmap_sem);
	return;


bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:

	

	if (user_mode(regs)) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:


	{
		const struct exception_table_entry *entry;

		__asm__ __volatile__("l.nop 42");

		if ((entry = search_exception_tables(regs->pc)) != NULL) {
			
			regs->pc = entry->fixup;
			return;
		}
	}


	if ((unsigned long)(address) < PAGE_SIZE)
		printk(KERN_ALERT
		       "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel access");
	printk(" at virtual address 0x%08lx\n", address);

	die("Oops", regs, write_acc);

	do_exit(SIGKILL);


out_of_memory:
	__asm__ __volatile__("l.nop 42");
	__asm__ __volatile__("l.nop 1");

	up_read(&mm->mmap_sem);
	printk("VM: killing process %s\n", tsk->comm);
	if (user_mode(regs))
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *)address;
	force_sig_info(SIGBUS, &info, tsk);

	
	if (!user_mode(regs))
		goto no_context;
	return;

vmalloc_fault:
	{

		int offset = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd = (pgd_t *)current_pgd + offset;
		pgd_k = init_mm.pgd + offset;


		pud = pud_offset(pgd, address);
		pud_k = pud_offset(pgd_k, address);
		if (!pud_present(*pud_k))
			goto no_context;

		pmd = pmd_offset(pud, address);
		pmd_k = pmd_offset(pud_k, address);

		if (!pmd_present(*pmd_k))
			goto bad_area_nosemaphore;

		set_pmd(pmd, *pmd_k);


		pte_k = pte_offset_kernel(pmd_k, address);
		if (!pte_present(*pte_k))
			goto no_context;

		return;
	}
}
