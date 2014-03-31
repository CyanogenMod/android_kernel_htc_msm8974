/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 2000 by Ralf Baechle
 */
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/perf_event.h>

#include <asm/branch.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/ptrace.h>
#include <asm/highmem.h>		
#include <linux/kdebug.h>

asmlinkage void __kprobes do_page_fault(struct pt_regs *regs, unsigned long write,
			      unsigned long address)
{
	struct vm_area_struct * vma = NULL;
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;
	const int field = sizeof(unsigned long) * 2;
	siginfo_t info;
	int fault;
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE |
						 (write ? FAULT_FLAG_WRITE : 0);

#if 0
	printk("Cpu%d[%s:%d:%0*lx:%ld:%0*lx]\n", raw_smp_processor_id(),
	       current->comm, current->pid, field, address, write,
	       field, regs->cp0_epc);
#endif

#ifdef CONFIG_KPROBES
	if (notify_die(DIE_PAGE_FAULT, "page fault", regs, -1,
		       (regs->cp0_cause >> 2) & 0x1f, SIGSEGV) == NOTIFY_STOP)
		return;
#endif

	info.si_code = SEGV_MAPERR;

#ifdef CONFIG_64BIT
# define VMALLOC_FAULT_TARGET no_context
#else
# define VMALLOC_FAULT_TARGET vmalloc_fault
#endif

	if (unlikely(address >= VMALLOC_START && address <= VMALLOC_END))
		goto VMALLOC_FAULT_TARGET;
#ifdef MODULE_START
	if (unlikely(address >= MODULE_START && address < MODULE_END))
		goto VMALLOC_FAULT_TARGET;
#endif

	if (in_atomic() || !mm)
		goto bad_area_nosemaphore;

retry:
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
		if (kernel_uses_smartmips_rixi) {
			if (address == regs->cp0_epc && !(vma->vm_flags & VM_EXEC)) {
#if 0
				pr_notice("Cpu%d[%s:%d:%0*lx:%ld:%0*lx] XI violation\n",
					  raw_smp_processor_id(),
					  current->comm, current->pid,
					  field, address, write,
					  field, regs->cp0_epc);
#endif
				goto bad_area;
			}
			if (!(vma->vm_flags & VM_READ)) {
#if 0
				pr_notice("Cpu%d[%s:%d:%0*lx:%ld:%0*lx] RI violation\n",
					  raw_smp_processor_id(),
					  current->comm, current->pid,
					  field, address, write,
					  field, regs->cp0_epc);
#endif
				goto bad_area;
			}
		} else {
			if (!(vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC)))
				goto bad_area;
		}
	}

	fault = handle_mm_fault(mm, vma, address, flags);

	if ((fault & VM_FAULT_RETRY) && fatal_signal_pending(current))
		return;

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, address);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	if (flags & FAULT_FLAG_ALLOW_RETRY) {
		if (fault & VM_FAULT_MAJOR) {
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1,
						  regs, address);
			tsk->maj_flt++;
		} else {
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1,
						  regs, address);
			tsk->min_flt++;
		}
		if (fault & VM_FAULT_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;


			goto retry;
		}
	}

	up_read(&mm->mmap_sem);
	return;

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	
	if (user_mode(regs)) {
		tsk->thread.cp0_badvaddr = address;
		tsk->thread.error_code = write;
#if 0
		printk("do_page_fault() #2: sending SIGSEGV to %s for "
		       "invalid %s\n%0*lx (epc == %0*lx, ra == %0*lx)\n",
		       tsk->comm,
		       write ? "write access to" : "read access from",
		       field, address,
		       field, (unsigned long) regs->cp0_epc,
		       field, (unsigned long) regs->regs[31]);
#endif
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
	       raw_smp_processor_id(), field, address, field, regs->cp0_epc,
	       field,  regs->regs[31]);
	die("Oops", regs);

out_of_memory:
	up_read(&mm->mmap_sem);
	pagefault_out_of_memory();
	return;

do_sigbus:
	up_read(&mm->mmap_sem);

	
	if (!user_mode(regs))
		goto no_context;
	else
#if 0
		printk("do_page_fault() #3: sending SIGBUS to %s for "
		       "invalid %s\n%0*lx (epc == %0*lx, ra == %0*lx)\n",
		       tsk->comm,
		       write ? "write access to" : "read access from",
		       field, address,
		       field, (unsigned long) regs->cp0_epc,
		       field, (unsigned long) regs->regs[31]);
#endif
	tsk->thread.cp0_badvaddr = address;
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void __user *) address;
	force_sig_info(SIGBUS, &info, tsk);

	return;
#ifndef CONFIG_64BIT
vmalloc_fault:
	{
		int offset = __pgd_offset(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd = (pgd_t *) pgd_current[raw_smp_processor_id()] + offset;
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
#endif
}
