/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/i386/mm/fault.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Modified by Cort Dougan and Paul Mackerras.
 *
 *  Modified for PPC64 by Dave Engebretsen (engebret@ibm.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
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
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kdebug.h>
#include <linux/perf_event.h>
#include <linux/magic.h>
#include <linux/ratelimit.h>

#include <asm/firmware.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/siginfo.h>
#include <asm/debug.h>
#include <mm/mmu_decl.h>

#include "icswx.h"

#ifdef CONFIG_KPROBES
static inline int notify_page_fault(struct pt_regs *regs)
{
	int ret = 0;

	
	if (!user_mode(regs)) {
		preempt_disable();
		if (kprobe_running() && kprobe_fault_handler(regs, 11))
			ret = 1;
		preempt_enable();
	}

	return ret;
}
#else
static inline int notify_page_fault(struct pt_regs *regs)
{
	return 0;
}
#endif

static int store_updates_sp(struct pt_regs *regs)
{
	unsigned int inst;

	if (get_user(inst, (unsigned int __user *)regs->nip))
		return 0;
	
	if (((inst >> 16) & 0x1f) != 1)
		return 0;
	
	switch (inst >> 26) {
	case 37:	
	case 39:	
	case 45:	
	case 53:	
	case 55:	
		return 1;
	case 62:	
		return (inst & 3) == 1;
	case 31:
		
		switch ((inst >> 1) & 0x3ff) {
		case 181:	
		case 183:	
		case 247:	
		case 439:	
		case 695:	
		case 759:	
			return 1;
		}
	}
	return 0;
}

#define MM_FAULT_RETURN		0
#define MM_FAULT_CONTINUE	-1
#define MM_FAULT_ERR(sig)	(sig)

static int out_of_memory(struct pt_regs *regs)
{
	up_read(&current->mm->mmap_sem);
	if (!user_mode(regs))
		return MM_FAULT_ERR(SIGKILL);
	pagefault_out_of_memory();
	return MM_FAULT_RETURN;
}

static int do_sigbus(struct pt_regs *regs, unsigned long address)
{
	siginfo_t info;

	up_read(&current->mm->mmap_sem);

	if (user_mode(regs)) {
		info.si_signo = SIGBUS;
		info.si_errno = 0;
		info.si_code = BUS_ADRERR;
		info.si_addr = (void __user *)address;
		force_sig_info(SIGBUS, &info, current);
		return MM_FAULT_RETURN;
	}
	return MM_FAULT_ERR(SIGBUS);
}

static int mm_fault_error(struct pt_regs *regs, unsigned long addr, int fault)
{
	if (fatal_signal_pending(current)) {
		if (!(fault & VM_FAULT_RETRY))
			up_read(&current->mm->mmap_sem);
		
		if (user_mode(regs))
			return MM_FAULT_RETURN;
		return MM_FAULT_ERR(SIGKILL);
	}

	
	if (!(fault & VM_FAULT_ERROR))
		return MM_FAULT_CONTINUE;

	
	if (fault & VM_FAULT_OOM)
		return out_of_memory(regs);

	if (fault & VM_FAULT_SIGBUS)
		return do_sigbus(regs, addr);

	
	BUG();
	return MM_FAULT_CONTINUE;
}

int __kprobes do_page_fault(struct pt_regs *regs, unsigned long address,
			    unsigned long error_code)
{
	struct vm_area_struct * vma;
	struct mm_struct *mm = current->mm;
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE;
	int code = SEGV_MAPERR;
	int is_write = 0;
	int trap = TRAP(regs);
 	int is_exec = trap == 0x400;
	int fault;

#if !(defined(CONFIG_4xx) || defined(CONFIG_BOOKE))
	if (trap == 0x400)
		error_code &= 0x48200000;
	else
		is_write = error_code & DSISR_ISSTORE;
#else
	is_write = error_code & ESR_DST;
#endif 

	if (is_write)
		flags |= FAULT_FLAG_WRITE;

#ifdef CONFIG_PPC_ICSWX
	if (error_code & ICSWX_DSI_UCT) {
		int rc = acop_handle_fault(regs, address, error_code);
		if (rc)
			return rc;
	}
#endif 

	if (notify_page_fault(regs))
		return 0;

	if (unlikely(debugger_fault_handler(regs)))
		return 0;

	
	if (!user_mode(regs) && (address >= TASK_SIZE))
		return SIGSEGV;

#if !(defined(CONFIG_4xx) || defined(CONFIG_BOOKE) || \
			     defined(CONFIG_PPC_BOOK3S_64))
  	if (error_code & DSISR_DABRMATCH) {
		
		do_dabr(regs, address, error_code);
		return 0;
	}
#endif

	
	if (!arch_irq_disabled_regs(regs))
		local_irq_enable();

	if (in_atomic() || mm == NULL) {
		if (!user_mode(regs))
			return SIGSEGV;
		printk(KERN_EMERG "Page fault in user mode with "
		       "in_atomic() = %d mm = %p\n", in_atomic(), mm);
		printk(KERN_EMERG "NIP = %lx  MSR = %lx\n",
		       regs->nip, regs->msr);
		die("Weird page fault", regs, SIGSEGV);
	}

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, address);

	if (!down_read_trylock(&mm->mmap_sem)) {
		if (!user_mode(regs) && !search_exception_tables(regs->nip))
			goto bad_area_nosemaphore;

retry:
		down_read(&mm->mmap_sem);
	} else {
		might_sleep();
	}

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (address + 0x100000 < vma->vm_end) {
		
		struct pt_regs *uregs = current->thread.regs;
		if (uregs == NULL)
			goto bad_area;

		if (address + 2048 < uregs->gpr[1]
		    && (!user_mode(regs) || !store_updates_sp(regs)))
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;

good_area:
	code = SEGV_ACCERR;
#if defined(CONFIG_6xx)
	if (error_code & 0x95700000)
		goto bad_area;
#endif 
#if defined(CONFIG_8xx)
	if (error_code & 0x40000000) 
		_tlbil_va(address, 0, 0, 0);

	if (error_code & 0x10000000)
                
		goto bad_area;
#endif 

	if (is_exec) {
#ifdef CONFIG_PPC_STD_MMU
		if (error_code & DSISR_PROTFAULT)
			goto bad_area;
#endif 

		if (!(vma->vm_flags & VM_EXEC) &&
		    (cpu_has_feature(CPU_FTR_NOEXECUTE) ||
		     !(vma->vm_flags & (VM_READ | VM_WRITE))))
			goto bad_area;
	
	} else if (is_write) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	
	} else {
		
		if (error_code & 0x08000000)
			goto bad_area;
		if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
			goto bad_area;
	}

	fault = handle_mm_fault(mm, vma, address, flags);
	if (unlikely(fault & (VM_FAULT_RETRY|VM_FAULT_ERROR))) {
		int rc = mm_fault_error(regs, address, fault);
		if (rc >= MM_FAULT_RETURN)
			return rc;
	}

	if (flags & FAULT_FLAG_ALLOW_RETRY) {
		if (fault & VM_FAULT_MAJOR) {
			current->maj_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1,
				      regs, address);
#ifdef CONFIG_PPC_SMLPAR
			if (firmware_has_feature(FW_FEATURE_CMO)) {
				preempt_disable();
				get_lppaca()->page_ins += (1 << PAGE_FACTOR);
				preempt_enable();
			}
#endif 
		} else {
			current->min_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1,
				      regs, address);
		}
		if (fault & VM_FAULT_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
			goto retry;
		}
	}

	up_read(&mm->mmap_sem);
	return 0;

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	
	if (user_mode(regs)) {
		_exception(SIGSEGV, regs, code, address);
		return 0;
	}

	if (is_exec && (error_code & DSISR_PROTFAULT))
		printk_ratelimited(KERN_CRIT "kernel tried to execute NX-protected"
				   " page (%lx) - exploit attempt? (uid: %d)\n",
				   address, current_uid());

	return SIGSEGV;

}

void bad_page_fault(struct pt_regs *regs, unsigned long address, int sig)
{
	const struct exception_table_entry *entry;
	unsigned long *stackend;

	
	if ((entry = search_exception_tables(regs->nip)) != NULL) {
		regs->nip = entry->fixup;
		return;
	}

	

	switch (regs->trap) {
	case 0x300:
	case 0x380:
		printk(KERN_ALERT "Unable to handle kernel paging request for "
			"data at address 0x%08lx\n", regs->dar);
		break;
	case 0x400:
	case 0x480:
		printk(KERN_ALERT "Unable to handle kernel paging request for "
			"instruction fetch\n");
		break;
	default:
		printk(KERN_ALERT "Unable to handle kernel paging request for "
			"unknown fault\n");
		break;
	}
	printk(KERN_ALERT "Faulting instruction address: 0x%08lx\n",
		regs->nip);

	stackend = end_of_stack(current);
	if (current != &init_task && *stackend != STACK_END_MAGIC)
		printk(KERN_ALERT "Thread overran stack, or stack corrupted\n");

	die("Kernel access of bad area", regs, sig);
}
