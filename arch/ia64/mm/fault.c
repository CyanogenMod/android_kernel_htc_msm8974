/*
 * MMU fault handling support.
 *
 * Copyright (C) 1998-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/kprobes.h>
#include <linux/kdebug.h>
#include <linux/prefetch.h>

#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

extern int die(char *, struct pt_regs *, long);

#ifdef CONFIG_KPROBES
static inline int notify_page_fault(struct pt_regs *regs, int trap)
{
	int ret = 0;

	if (!user_mode(regs)) {
		
		preempt_disable();
		if (kprobe_running() && kprobe_fault_handler(regs, trap))
			ret = 1;
		preempt_enable();
	}

	return ret;
}
#else
static inline int notify_page_fault(struct pt_regs *regs, int trap)
{
	return 0;
}
#endif

static int
mapped_kernel_page_is_present (unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;

	pgd = pgd_offset_k(address);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pud = pud_offset(pgd, address);
	if (pud_none(*pud) || pud_bad(*pud))
		return 0;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_kernel(pmd, address);
	if (!ptep)
		return 0;

	pte = *ptep;
	return pte_present(pte);
}

void __kprobes
ia64_do_page_fault (unsigned long address, unsigned long isr, struct pt_regs *regs)
{
	int signal = SIGSEGV, code = SEGV_MAPERR;
	struct vm_area_struct *vma, *prev_vma;
	struct mm_struct *mm = current->mm;
	struct siginfo si;
	unsigned long mask;
	int fault;

	
	prefetchw(&mm->mmap_sem);

	if (in_atomic() || !mm)
		goto no_context;

#ifdef CONFIG_VIRTUAL_MEM_MAP

	if ((REGION_NUMBER(address) == 5) && !user_mode(regs))
		goto bad_area_no_up;
#endif

	if (notify_page_fault(regs, TRAP_BRKPT))
		return;

	down_read(&mm->mmap_sem);

	vma = find_vma_prev(mm, address, &prev_vma);
	if (!vma && !prev_vma )
		goto bad_area;

        if (( !vma && prev_vma ) || (address < vma->vm_start) )
		goto check_expansion;

  good_area:
	code = SEGV_ACCERR;

	

#	define VM_READ_BIT	0
#	define VM_WRITE_BIT	1
#	define VM_EXEC_BIT	2

#	if (((1 << VM_READ_BIT) != VM_READ || (1 << VM_WRITE_BIT) != VM_WRITE) \
	    || (1 << VM_EXEC_BIT) != VM_EXEC)
#		error File is out of sync with <linux/mm.h>.  Please update.
#	endif

	if (((isr >> IA64_ISR_R_BIT) & 1UL) && (!(vma->vm_flags & (VM_READ | VM_WRITE))))
		goto bad_area;

	mask = (  (((isr >> IA64_ISR_X_BIT) & 1UL) << VM_EXEC_BIT)
		| (((isr >> IA64_ISR_W_BIT) & 1UL) << VM_WRITE_BIT));

	if ((vma->vm_flags & mask) != mask)
		goto bad_area;

	fault = handle_mm_fault(mm, vma, address, (mask & VM_WRITE) ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM) {
			goto out_of_memory;
		} else if (fault & VM_FAULT_SIGBUS) {
			signal = SIGBUS;
			goto bad_area;
		}
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		current->maj_flt++;
	else
		current->min_flt++;
	up_read(&mm->mmap_sem);
	return;

  check_expansion:
	if (!(prev_vma && (prev_vma->vm_flags & VM_GROWSUP) && (address == prev_vma->vm_end))) {
		if (!vma)
			goto bad_area;
		if (!(vma->vm_flags & VM_GROWSDOWN))
			goto bad_area;
		if (REGION_NUMBER(address) != REGION_NUMBER(vma->vm_start)
		    || REGION_OFFSET(address) >= RGN_MAP_LIMIT)
			goto bad_area;
		if (expand_stack(vma, address))
			goto bad_area;
	} else {
		vma = prev_vma;
		if (REGION_NUMBER(address) != REGION_NUMBER(vma->vm_start)
		    || REGION_OFFSET(address) >= RGN_MAP_LIMIT)
			goto bad_area;
		if (address > vma->vm_end + PAGE_SIZE - sizeof(long))
			goto bad_area;
		if (expand_upwards(vma, address))
			goto bad_area;
	}
	goto good_area;

  bad_area:
	up_read(&mm->mmap_sem);
#ifdef CONFIG_VIRTUAL_MEM_MAP
  bad_area_no_up:
#endif
	if ((isr & IA64_ISR_SP)
	    || ((isr & IA64_ISR_NA) && (isr & IA64_ISR_CODE_MASK) == IA64_ISR_CODE_LFETCH))
	{
		ia64_psr(regs)->ed = 1;
		return;
	}
	if (user_mode(regs)) {
		si.si_signo = signal;
		si.si_errno = 0;
		si.si_code = code;
		si.si_addr = (void __user *) address;
		si.si_isr = isr;
		si.si_flags = __ISR_VALID;
		force_sig_info(signal, &si, current);
		return;
	}

  no_context:
	if ((isr & IA64_ISR_SP)
	    || ((isr & IA64_ISR_NA) && (isr & IA64_ISR_CODE_MASK) == IA64_ISR_CODE_LFETCH))
	{
		ia64_psr(regs)->ed = 1;
		return;
	}

	if (REGION_NUMBER(address) == 5 && mapped_kernel_page_is_present(address))
		return;

	if (ia64_done_with_exception(regs))
		return;

	bust_spinlocks(1);

	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference (address %016lx)\n", address);
	else
		printk(KERN_ALERT "Unable to handle kernel paging request at "
		       "virtual address %016lx\n", address);
	if (die("Oops", regs, isr))
		regs = NULL;
	bust_spinlocks(0);
	if (regs)
		do_exit(SIGKILL);
	return;

  out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
}
