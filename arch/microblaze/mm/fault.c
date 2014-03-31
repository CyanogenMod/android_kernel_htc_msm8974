/*
 *  arch/microblaze/mm/fault.c
 *
 *    Copyright (C) 2007 Xilinx, Inc.  All rights reserved.
 *
 *  Derived from "arch/ppc/mm/fault.c"
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/i386/mm/fault.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Modified by Cort Dougan and Paul Mackerras.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 */

#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <linux/uaccess.h>
#include <asm/exceptions.h>

static unsigned long pte_misses;	
static unsigned long pte_errors;	

static int store_updates_sp(struct pt_regs *regs)
{
	unsigned int inst;

	if (get_user(inst, (unsigned int __user *)regs->pc))
		return 0;
	
	if (((inst >> 21) & 0x1f) != 1)
		return 0;
	
	if ((inst & 0xd0000000) == 0xd0000000)
		return 1;
	return 0;
}


void bad_page_fault(struct pt_regs *regs, unsigned long address, int sig)
{
	const struct exception_table_entry *fixup;
	
	fixup = search_exception_tables(regs->pc);
	if (fixup) {
		regs->pc = fixup->fixup;
		return;
	}

	
	die("kernel access of bad area", regs, sig);
}

void do_page_fault(struct pt_regs *regs, unsigned long address,
		   unsigned long error_code)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	siginfo_t info;
	int code = SEGV_MAPERR;
	int is_write = error_code & ESR_S;
	int fault;

	regs->ear = address;
	regs->esr = error_code;

	
	if (unlikely(kernel_mode(regs) && (address >= TASK_SIZE))) {
		printk(KERN_WARNING "kernel task_size exceed");
		_exception(SIGSEGV, regs, code, address);
	}

	
	if ((error_code & 0x13) == 0x13 || (error_code & 0x11) == 0x11)
		is_write = 0;

	if (unlikely(in_atomic() || !mm)) {
		if (kernel_mode(regs))
			goto bad_area_nosemaphore;

		printk(KERN_EMERG "Page fault in user mode with "
		       "in_atomic(), mm = %p\n", mm);
		printk(KERN_EMERG "r15 = %lx  MSR = %lx\n",
		       regs->r15, regs->msr);
		die("Weird page fault", regs, SIGSEGV);
	}

	if (unlikely(!down_read_trylock(&mm->mmap_sem))) {
		if (kernel_mode(regs) && !search_exception_tables(regs->pc))
			goto bad_area_nosemaphore;

		down_read(&mm->mmap_sem);
	}

	vma = find_vma(mm, address);
	if (unlikely(!vma))
		goto bad_area;

	if (vma->vm_start <= address)
		goto good_area;

	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN)))
		goto bad_area;

	if (unlikely(!is_write))
		goto bad_area;

	if (unlikely(address + 0x100000 < vma->vm_end)) {

		
		struct pt_regs *uregs = current->thread.regs;
		if (uregs == NULL)
			goto bad_area;

		if (address + 2048 < uregs->r1
			&& (kernel_mode(regs) || !store_updates_sp(regs)))
				goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;

good_area:
	code = SEGV_ACCERR;

	
	if (unlikely(is_write)) {
		if (unlikely(!(vma->vm_flags & VM_WRITE)))
			goto bad_area;
	
	} else {
		
		if (unlikely(error_code & 0x08000000))
			goto bad_area;
		if (unlikely(!(vma->vm_flags & (VM_READ | VM_EXEC))))
			goto bad_area;
	}

	fault = handle_mm_fault(mm, vma, address, is_write ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	if (unlikely(fault & VM_FAULT_MAJOR))
		current->maj_flt++;
	else
		current->min_flt++;
	up_read(&mm->mmap_sem);
	pte_misses++;
	return;

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	pte_errors++;

	
	if (user_mode(regs)) {
		_exception(SIGSEGV, regs, code, address);
		return;
	}

	bad_page_fault(regs, address, SIGSEGV);
	return;

out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		bad_page_fault(regs, address, SIGKILL);
	else
		pagefault_out_of_memory();
	return;

do_sigbus:
	up_read(&mm->mmap_sem);
	if (user_mode(regs)) {
		info.si_signo = SIGBUS;
		info.si_errno = 0;
		info.si_code = BUS_ADRERR;
		info.si_addr = (void __user *)address;
		force_sig_info(SIGBUS, &info, current);
		return;
	}
	bad_page_fault(regs, address, SIGBUS);
}
