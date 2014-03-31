/*
 * arch/xtensa/mm/fault.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 *
 * Chris Zankel <chris@zankel.net>
 * Joe Taylor	<joe@tensilica.com, joetylr@yahoo.com>
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>
#include <asm/hardirq.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>

unsigned long asid_cache = ASID_USER_FIRST;
void bad_page_fault(struct pt_regs*, unsigned long, int);

#undef DEBUG_PAGE_FAULT


void do_page_fault(struct pt_regs *regs)
{
	struct vm_area_struct * vma;
	struct mm_struct *mm = current->mm;
	unsigned int exccause = regs->exccause;
	unsigned int address = regs->excvaddr;
	siginfo_t info;

	int is_write, is_exec;
	int fault;

	info.si_code = SEGV_MAPERR;

	if (address >= TASK_SIZE && !user_mode(regs))
		goto vmalloc_fault;

	if (in_atomic() || !mm) {
		bad_page_fault(regs, address, SIGSEGV);
		return;
	}

	is_write = (exccause == EXCCAUSE_STORE_CACHE_ATTRIBUTE) ? 1 : 0;
	is_exec =  (exccause == EXCCAUSE_ITLB_PRIVILEGE ||
		    exccause == EXCCAUSE_ITLB_MISS ||
		    exccause == EXCCAUSE_FETCH_CACHE_ATTRIBUTE) ? 1 : 0;

#ifdef DEBUG_PAGE_FAULT
	printk("[%s:%d:%08x:%d:%08x:%s%s]\n", current->comm, current->pid,
	       address, exccause, regs->pc, is_write? "w":"", is_exec? "x":"");
#endif

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

	if (is_write) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else if (is_exec) {
		if (!(vma->vm_flags & VM_EXEC))
			goto bad_area;
	} else	
		if (!(vma->vm_flags & (VM_READ | VM_WRITE)))
			goto bad_area;

	fault = handle_mm_fault(mm, vma, address, is_write ? FAULT_FLAG_WRITE : 0);
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
	if (user_mode(regs)) {
		current->thread.bad_vaddr = address;
		current->thread.error_code = is_write;
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void *) address;
		force_sig_info(SIGSEGV, &info, current);
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

	current->thread.bad_vaddr = address;
	info.si_code = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *) address;
	force_sig_info(SIGBUS, &info, current);

	
	if (!user_mode(regs))
		bad_page_fault(regs, address, SIGBUS);

vmalloc_fault:
	{
		struct mm_struct *act_mm = current->active_mm;
		int index = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		if (act_mm == NULL)
			goto bad_page_fault;

		pgd = act_mm->pgd + index;
		pgd_k = init_mm.pgd + index;

		if (!pgd_present(*pgd_k))
			goto bad_page_fault;

		pgd_val(*pgd) = pgd_val(*pgd_k);

		pmd = pmd_offset(pgd, address);
		pmd_k = pmd_offset(pgd_k, address);
		if (!pmd_present(*pmd) || !pmd_present(*pmd_k))
			goto bad_page_fault;

		pmd_val(*pmd) = pmd_val(*pmd_k);
		pte_k = pte_offset_kernel(pmd_k, address);

		if (!pte_present(*pte_k))
			goto bad_page_fault;
		return;
	}
bad_page_fault:
	bad_page_fault(regs, address, SIGKILL);
	return;
}


void
bad_page_fault(struct pt_regs *regs, unsigned long address, int sig)
{
	extern void die(const char*, struct pt_regs*, long);
	const struct exception_table_entry *entry;

	
	if ((entry = search_exception_tables(regs->pc)) != NULL) {
#ifdef DEBUG_PAGE_FAULT
		printk(KERN_DEBUG "%s: Exception at pc=%#010lx (%lx)\n",
				current->comm, regs->pc, entry->fixup);
#endif
		current->thread.bad_uaddr = address;
		regs->pc = entry->fixup;
		return;
	}

	printk(KERN_ALERT "Unable to handle kernel paging request at virtual "
	       "address %08lx\n pc = %08lx, ra = %08lx\n",
	       address, regs->pc, regs->areg[0]);
	die("Oops", regs, sig);
	do_exit(sig);
}

