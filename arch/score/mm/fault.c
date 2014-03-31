/*
 * arch/score/mm/fault.c
 *
 * Score Processor version.
 *
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Lennox Wu <lennox.wu@sunplusct.com>
 *  Chen Liqin <liqin.chen@sunplusct.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long write,
				unsigned long address)
{
	struct vm_area_struct *vma = NULL;
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;
	const int field = sizeof(unsigned long) * 2;
	siginfo_t info;
	int fault;

	info.si_code = SEGV_MAPERR;

	if (unlikely(address >= VMALLOC_START && address <= VMALLOC_END))
		goto vmalloc_fault;
#ifdef MODULE_START
	if (unlikely(address >= MODULE_START && address < MODULE_END))
		goto vmalloc_fault;
#endif

	if (in_atomic() || !mm)
		goto bad_area_nosemaphore;

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
	info.si_code = SEGV_ACCERR;

	if (write) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC)))
			goto bad_area;
	}

survive:
	fault = handle_mm_fault(mm, vma, address, write);
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
		tsk->thread.cp0_badvaddr = address;
		tsk->thread.error_code = write;
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void __user *) address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:
	
	if (fixup_exception(regs)) {
		current->thread.cp0_baduaddr = address;
		return;
	}

	bust_spinlocks(1);

	printk(KERN_ALERT "CPU %d Unable to handle kernel paging request at "
			"virtual address %0*lx, epc == %0*lx, ra == %0*lx\n",
			0, field, address, field, regs->cp0_epc,
			field, regs->regs[3]);
	die("Oops", regs);

out_of_memory:
	up_read(&mm->mmap_sem);
	if (is_global_init(tsk)) {
		yield();
		down_read(&mm->mmap_sem);
		goto survive;
	}
	printk("VM: killing process %s\n", tsk->comm);
	if (user_mode(regs))
		do_group_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);
	
	if (!user_mode(regs))
		goto no_context;
	else
	tsk->thread.cp0_badvaddr = address;
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void __user *) address;
	force_sig_info(SIGBUS, &info, tsk);
	return;
vmalloc_fault:
	{
		int offset = __pgd_offset(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd = (pgd_t *) pgd_current + offset;
		pgd_k = init_mm.pgd + offset;

		if (!pgd_present(*pgd_k))
			goto no_context;
		set_pgd(pgd, *pgd_k);

		pud = pud_offset(pgd, address);
		pud_k = pud_offset(pgd_k, address);
		if (!pud_present(*pud_k))
			goto no_context;

		pmd = pmd_offset(pud, address);
		pmd_k = pmd_offset(pud_k, address);
		if (!pmd_present(*pmd_k))
			goto no_context;
		set_pmd(pmd, *pmd_k);

		pte_k = pte_offset_kernel(pmd_k, address);
		if (!pte_present(*pte_k))
			goto no_context;
		return;
	}
}
