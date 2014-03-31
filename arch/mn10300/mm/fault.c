/* MN10300 MMU Fault handler
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Modified by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/vt_kern.h>		

#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/hardirq.h>
#include <asm/cpu-regs.h>
#include <asm/debugger.h>
#include <asm/gdb-stub.h>

void bust_spinlocks(int yes)
{
	if (yes) {
		oops_in_progress = 1;
	} else {
		int loglevel_save = console_loglevel;
#ifdef CONFIG_VT
		unblank_screen();
#endif
		oops_in_progress = 0;
		console_loglevel = 15;	
		printk(" ");
		console_loglevel = loglevel_save;
	}
}

void do_BUG(const char *file, int line)
{
	bust_spinlocks(1);
	printk(KERN_EMERG "------------[ cut here ]------------\n");
	printk(KERN_EMERG "kernel BUG at %s:%d!\n", file, line);
}

#if 0
static void print_pagetable_entries(pgd_t *pgdir, unsigned long address)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgdir + __pgd_offset(address);
	printk(KERN_DEBUG "pgd entry %p: %016Lx\n",
	       pgd, (long long) pgd_val(*pgd));

	if (!pgd_present(*pgd)) {
		printk(KERN_DEBUG "... pgd not present!\n");
		return;
	}
	pmd = pmd_offset(pgd, address);
	printk(KERN_DEBUG "pmd entry %p: %016Lx\n",
	       pmd, (long long)pmd_val(*pmd));

	if (!pmd_present(*pmd)) {
		printk(KERN_DEBUG "... pmd not present!\n");
		return;
	}
	pte = pte_offset(pmd, address);
	printk(KERN_DEBUG "pte entry %p: %016Lx\n",
	       pte, (long long) pte_val(*pte));

	if (!pte_present(*pte))
		printk(KERN_DEBUG "... pte not present!\n");
}
#endif

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long fault_code,
			      unsigned long address)
{
	struct vm_area_struct *vma;
	struct task_struct *tsk;
	struct mm_struct *mm;
	unsigned long page;
	siginfo_t info;
	int write, fault;

#ifdef CONFIG_GDBSTUB
	
	if (gdbstub_busy) {
		gdbstub_exception(regs, TBR & TBR_INT_CODE);
		return;
	}
#endif

#if 0
	printk(KERN_DEBUG "--- do_page_fault(%p,%s:%04lx,%08lx)\n",
	       regs,
	       fault_code & 0x10000 ? "ins" : "data",
	       fault_code & 0xffff, address);
#endif

	tsk = current;

	if (address >= VMALLOC_START && address < VMALLOC_END &&
	    (fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_SR &&
	    (fault_code & MMUFCR_xFC_PGINVAL) == MMUFCR_xFC_PGINVAL
	    )
		goto vmalloc_fault;

	mm = tsk->mm;
	info.si_code = SEGV_MAPERR;

	if (in_atomic() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if ((fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_USR) {
		if ((address & PAGE_MASK) + 2 * PAGE_SIZE < regs->sp) {
#if 0
			printk(KERN_WARNING
			       "[%d] ### Access below stack @%lx (sp=%lx)\n",
			       current->pid, address, regs->sp);
			printk(KERN_WARNING
			       "vma [%08x - %08x]\n",
			       vma->vm_start, vma->vm_end);
			show_registers(regs);
			printk(KERN_WARNING
			       "[%d] ### Code: [%08lx]"
			       " %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       current->pid,
			       regs->pc,
			       ((u8 *) regs->pc)[0],
			       ((u8 *) regs->pc)[1],
			       ((u8 *) regs->pc)[2],
			       ((u8 *) regs->pc)[3],
			       ((u8 *) regs->pc)[4],
			       ((u8 *) regs->pc)[5],
			       ((u8 *) regs->pc)[6],
			       ((u8 *) regs->pc)[7]
			       );
#endif
			goto bad_area;
		}
	}

	if (expand_stack(vma, address))
		goto bad_area;

good_area:
	info.si_code = SEGV_ACCERR;
	write = 0;
	switch (fault_code & (MMUFCR_xFC_PGINVAL|MMUFCR_xFC_TYPE)) {
	default:	
	case MMUFCR_xFC_TYPE_WRITE:
#ifdef TEST_VERIFY_AREA
		if ((fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_SR)
			printk(KERN_DEBUG "WP fault at %08lx\n", regs->pc);
#endif
		
	case MMUFCR_xFC_PGINVAL | MMUFCR_xFC_TYPE_WRITE:
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
		write++;
		break;

		
	case MMUFCR_xFC_TYPE_READ:
		goto bad_area;

		
	case MMUFCR_xFC_PGINVAL | MMUFCR_xFC_TYPE_READ:
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
		break;
	}

	fault = handle_mm_fault(mm, vma, address, write ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		current->maj_flt++;
	else
		current->min_flt++;

	up_read(&mm->mmap_sem);
	return;

bad_area:
	up_read(&mm->mmap_sem);

	
	if ((fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_USR) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:
	
	if (fixup_exception(regs))
		return;


	bust_spinlocks(1);

	if (address < PAGE_SIZE)
		printk(KERN_ALERT
		       "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT
		       "Unable to handle kernel paging request");
	printk(" at virtual address %08lx\n", address);
	printk(" printing pc:\n");
	printk(KERN_ALERT "%08lx\n", regs->pc);

	debugger_intercept(fault_code & 0x00010000 ? EXCEP_IAERROR : EXCEP_DAERROR,
			   SIGSEGV, SEGV_ACCERR, regs);

	page = PTBR;
	page = ((unsigned long *) __va(page))[address >> 22];
	printk(KERN_ALERT "*pde = %08lx\n", page);
	if (page & 1) {
		page &= PAGE_MASK;
		address &= 0x003ff000;
		page = ((unsigned long *) __va(page))[address >> PAGE_SHIFT];
		printk(KERN_ALERT "*pte = %08lx\n", page);
	}

	die("Oops", regs, fault_code);
	do_exit(SIGKILL);

out_of_memory:
	up_read(&mm->mmap_sem);
	printk(KERN_ALERT "VM: killing process %s\n", tsk->comm);
	if ((fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_USR)
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *)address;
	force_sig_info(SIGBUS, &info, tsk);

	
	if ((fault_code & MMUFCR_xFC_ACCESS) == MMUFCR_xFC_ACCESS_SR)
		goto no_context;
	return;

vmalloc_fault:
	{
		int index = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd_k = init_mm.pgd + index;

		if (!pgd_present(*pgd_k))
			goto no_context;

		pud_k = pud_offset(pgd_k, address);
		if (!pud_present(*pud_k))
			goto no_context;

		pmd_k = pmd_offset(pud_k, address);
		if (!pmd_present(*pmd_k))
			goto no_context;

		pgd = (pgd_t *) PTBR + index;
		pud = pud_offset(pgd, address);
		pmd = pmd_offset(pud, address);
		set_pmd(pmd, *pmd_k);

		pte_k = pte_offset_kernel(pmd_k, address);
		if (!pte_present(*pte_k))
			goto no_context;
		return;
	}
}
