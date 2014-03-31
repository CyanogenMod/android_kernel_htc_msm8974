/*
 *  arch/cris/mm/fault.c
 *
 *  Copyright (C) 2000-2010  Axis Communications AB
 */

#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <arch/system.h>

extern int find_fixup_code(struct pt_regs *);
extern void die_if_kernel(const char *, struct pt_regs *, long);
extern void show_registers(struct pt_regs *regs);

#undef DEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

#define DPG(x)


DEFINE_PER_CPU(pgd_t *, current_pgd);
unsigned long cris_signal_return_page;


asmlinkage void
do_page_fault(unsigned long address, struct pt_regs *regs,
	      int protection, int writeaccess)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	siginfo_t info;
	int fault;

	D(printk(KERN_DEBUG
		 "Page fault for %lX on %X at %lX, prot %d write %d\n",
		 address, smp_processor_id(), instruction_pointer(regs),
		 protection, writeaccess));

	tsk = current;


	if (address >= VMALLOC_START &&
	    !protection &&
	    !user_mode(regs))
		goto vmalloc_fault;

	if (cris_signal_return_page &&
	    address == cris_signal_return_page &&
	    !protection && user_mode(regs))
		goto vmalloc_fault;

	
	local_irq_enable();

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
	if (user_mode(regs)) {
		if (address + PAGE_SIZE < rdusp())
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;


 good_area:
	info.si_code = SEGV_ACCERR;

	

	if (writeaccess == 2){
		if (!(vma->vm_flags & VM_EXEC))
			goto bad_area;
	} else if (writeaccess == 1) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
	}


	fault = handle_mm_fault(mm, vma, address, (writeaccess & 1) ? FAULT_FLAG_WRITE : 0);
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
	DPG(show_registers(regs));

	

	if (user_mode(regs)) {
		printk(KERN_NOTICE "%s (pid %d) segfaults for page "
			"address %08lx at pc %08lx\n",
			tsk->comm, tsk->pid,
			address, instruction_pointer(regs));

		
		DPG(if (0))
			show_registers(regs);

#ifdef CONFIG_NO_SEGFAULT_TERMINATION
		DECLARE_WAIT_QUEUE_HEAD(wq);
		wait_event_interruptible(wq, 0 == 1);
#else
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, tsk);
#endif
		return;
	}

 no_context:


	if (find_fixup_code(regs))
		return;


	if (!oops_in_progress) {
		oops_in_progress = 1;
		if ((unsigned long) (address) < PAGE_SIZE)
			printk(KERN_ALERT "Unable to handle kernel NULL "
				"pointer dereference");
		else
			printk(KERN_ALERT "Unable to handle kernel access"
				" at virtual address %08lx\n", address);

		die_if_kernel("Oops", regs, (writeaccess << 1) | protection);
		oops_in_progress = 0;
	}

	do_exit(SIGKILL);


 out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
	return;

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

		pgd = (pgd_t *)per_cpu(current_pgd, smp_processor_id()) + offset;
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

int
find_fixup_code(struct pt_regs *regs)
{
	const struct exception_table_entry *fixup;
	
	unsigned long ip = (instruction_pointer(regs) & ~0x1);

	fixup = search_exception_tables(ip);
	if (fixup != 0) {
		
		instruction_pointer(regs) = fixup->fixup;
		arch_fixup(regs);
		return 1;
	}

	return 0;
}
