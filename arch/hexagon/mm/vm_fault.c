/*
 * Memory fault handling for Hexagon
 *
 * Copyright (c) 2010-2011 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include <asm/pgtable.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/module.h>
#include <linux/hardirq.h>

#define FLT_IFETCH     -1
#define FLT_LOAD        0
#define FLT_STORE       1


void do_page_fault(unsigned long address, long cause, struct pt_regs *regs)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	siginfo_t info;
	int si_code = SEGV_MAPERR;
	int fault;
	const struct exception_table_entry *fixup;

	if (unlikely(in_interrupt() || !mm))
		goto no_context;

	local_irq_enable();

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;

	if (vma->vm_start <= address)
		goto good_area;

	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (expand_stack(vma, address))
		goto bad_area;

good_area:
	
	si_code = SEGV_ACCERR;

	switch (cause) {
	case FLT_IFETCH:
		if (!(vma->vm_flags & VM_EXEC))
			goto bad_area;
		break;
	case FLT_LOAD:
		if (!(vma->vm_flags & VM_READ))
			goto bad_area;
		break;
	case FLT_STORE:
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
		break;
	}

	fault = handle_mm_fault(mm, vma, address, (cause > 0));

	
	if (likely(!(fault & VM_FAULT_ERROR))) {
		if (fault & VM_FAULT_MAJOR)
			current->maj_flt++;
		else
			current->min_flt++;

		up_read(&mm->mmap_sem);
		return;
	}

	up_read(&mm->mmap_sem);

	
	if (!user_mode(regs))
		goto no_context;

	if (fault & VM_FAULT_OOM) {
		pagefault_out_of_memory();
		return;
	}

	if (fault & VM_FAULT_SIGBUS) {
		info.si_signo = SIGBUS;
		info.si_code = BUS_ADRERR;
	}
	
	else {
		info.si_signo = SIGSEGV;
		info.si_code = SEGV_ACCERR;
	}
	info.si_errno = 0;
	info.si_addr = (void __user *)address;
	force_sig_info(info.si_code, &info, current);
	return;

bad_area:
	up_read(&mm->mmap_sem);

	if (user_mode(regs)) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		info.si_code = si_code;
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, current);
		return;
	}
	

no_context:
	fixup = search_exception_tables(pt_elr(regs));
	if (fixup) {
		pt_set_elr(regs, fixup->fixup);
		return;
	}

	
	bust_spinlocks(1);
	printk(KERN_EMERG "Unable to handle kernel paging request at "
		"virtual address 0x%08lx, regs %p\n", address, regs);
	die("Bad Kernel VA", regs, SIGKILL);
}


void read_protection_fault(struct pt_regs *regs)
{
	unsigned long badvadr = pt_badva(regs);

	do_page_fault(badvadr, FLT_LOAD, regs);
}

void write_protection_fault(struct pt_regs *regs)
{
	unsigned long badvadr = pt_badva(regs);

	do_page_fault(badvadr, FLT_STORE, regs);
}

void execute_protection_fault(struct pt_regs *regs)
{
	unsigned long badvadr = pt_badva(regs);

	do_page_fault(badvadr, FLT_IFETCH, regs);
}
