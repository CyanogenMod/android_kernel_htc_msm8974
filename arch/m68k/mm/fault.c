/*
 *  linux/arch/m68k/mm/fault.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 */

#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/setup.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>

extern void die_if_kernel(char *, struct pt_regs *, long);

int send_fault_sig(struct pt_regs *regs)
{
	siginfo_t siginfo = { 0, 0, 0, };

	siginfo.si_signo = current->thread.signo;
	siginfo.si_code = current->thread.code;
	siginfo.si_addr = (void *)current->thread.faddr;
#ifdef DEBUG
	printk("send_fault_sig: %p,%d,%d\n", siginfo.si_addr, siginfo.si_signo, siginfo.si_code);
#endif

	if (user_mode(regs)) {
		force_sig_info(siginfo.si_signo,
			       &siginfo, current);
	} else {
		if (handle_kernel_fault(regs))
			return -1;

		
		
		

		if ((unsigned long)siginfo.si_addr < PAGE_SIZE)
			printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
		else
			printk(KERN_ALERT "Unable to handle kernel access");
		printk(" at virtual address %p\n", siginfo.si_addr);
		die_if_kernel("Oops", regs, 0 );
		do_exit(SIGKILL);
	}

	return 1;
}

int do_page_fault(struct pt_regs *regs, unsigned long address,
			      unsigned long error_code)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct * vma;
	int write, fault;

#ifdef DEBUG
	printk ("do page fault:\nregs->sr=%#x, regs->pc=%#lx, address=%#lx, %ld, %p\n",
		regs->sr, regs->pc, address, error_code,
		current->mm->pgd);
#endif

	if (in_atomic() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (!vma)
		goto map_err;
	if (vma->vm_flags & VM_IO)
		goto acc_err;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto map_err;
	if (user_mode(regs)) {
		if (address + 256 < rdusp())
			goto map_err;
	}
	if (expand_stack(vma, address))
		goto map_err;

good_area:
#ifdef DEBUG
	printk("do_page_fault: good_area\n");
#endif
	write = 0;
	switch (error_code & 3) {
		default:	
			
		case 2:		
			if (!(vma->vm_flags & VM_WRITE))
				goto acc_err;
			write++;
			break;
		case 1:		
			goto acc_err;
		case 0:		
			if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
				goto acc_err;
	}


	fault = handle_mm_fault(mm, vma, address, write ? FAULT_FLAG_WRITE : 0);
#ifdef DEBUG
	printk("handle_mm_fault returns %d\n",fault);
#endif
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto bus_err;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		current->maj_flt++;
	else
		current->min_flt++;

	up_read(&mm->mmap_sem);
	return 0;

out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
	return 0;

no_context:
	current->thread.signo = SIGBUS;
	current->thread.faddr = address;
	return send_fault_sig(regs);

bus_err:
	current->thread.signo = SIGBUS;
	current->thread.code = BUS_ADRERR;
	current->thread.faddr = address;
	goto send_sig;

map_err:
	current->thread.signo = SIGSEGV;
	current->thread.code = SEGV_MAPERR;
	current->thread.faddr = address;
	goto send_sig;

acc_err:
	current->thread.signo = SIGSEGV;
	current->thread.code = SEGV_ACCERR;
	current->thread.faddr = address;

send_sig:
	up_read(&mm->mmap_sem);
	return send_fault_sig(regs);
}
