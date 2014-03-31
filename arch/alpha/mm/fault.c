/*
 *  linux/arch/alpha/mm/fault.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/io.h>

#define __EXTERN_INLINE inline
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#undef  __EXTERN_INLINE

#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/uaccess.h>

extern void die_if_kernel(char *,struct pt_regs *,long, unsigned long *);



#ifndef CONFIG_SMP
unsigned long last_asn = ASN_FIRST_VERSION;
#endif

void
__load_new_mm_context(struct mm_struct *next_mm)
{
	unsigned long mmc;
	struct pcb_struct *pcb;

	mmc = __get_new_mm_context(next_mm, smp_processor_id());
	next_mm->context[smp_processor_id()] = mmc;

	pcb = &current_thread_info()->pcb;
	pcb->asn = mmc & HARDWARE_ASN_MASK;
	pcb->ptbr = ((unsigned long) next_mm->pgd - IDENT_ADDR) >> PAGE_SHIFT;

	__reload_thread(pcb);
}



#define dpf_reg(r)							\
	(((unsigned long *)regs)[(r) <= 8 ? (r) : (r) <= 15 ? (r)-16 :	\
				 (r) <= 18 ? (r)+8 : (r)-10])

asmlinkage void
do_page_fault(unsigned long address, unsigned long mmcsr,
	      long cause, struct pt_regs *regs)
{
	struct vm_area_struct * vma;
	struct mm_struct *mm = current->mm;
	const struct exception_table_entry *fixup;
	int fault, si_code = SEGV_MAPERR;
	siginfo_t info;

	if (cause == 0) {
		unsigned int insn;
		__get_user(insn, (unsigned int __user *)regs->pc);
		if ((insn >> 21 & 0x1f) == 0x1f &&
		    
		    (1ul << (insn >> 26) & 0x30f00001400ul)) {
			regs->pc += 4;
			return;
		}
	}

	if (!mm || in_atomic())
		goto no_context;

#ifdef CONFIG_ALPHA_LARGE_VMALLOC
	if (address >= TASK_SIZE)
		goto vmalloc_fault;
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
	si_code = SEGV_ACCERR;
	if (cause < 0) {
		if (!(vma->vm_flags & VM_EXEC))
			goto bad_area;
	} else if (!cause) {
		
		if (!(vma->vm_flags & (VM_READ | VM_WRITE)))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	}

	fault = handle_mm_fault(mm, vma, address, cause > 0 ? FAULT_FLAG_WRITE : 0);
	up_read(&mm->mmap_sem);
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
	return;

 bad_area:
	up_read(&mm->mmap_sem);

	if (user_mode(regs))
		goto do_sigsegv;

 no_context:
	
	if ((fixup = search_exception_tables(regs->pc)) != 0) {
		unsigned long newpc;
		newpc = fixup_exception(dpf_reg, fixup, regs->pc);
		regs->pc = newpc;
		return;
	}

	printk(KERN_ALERT "Unable to handle kernel paging request at "
	       "virtual address %016lx\n", address);
	die_if_kernel("Oops", regs, cause, (unsigned long*)regs - 16);
	do_exit(SIGKILL);

 out_of_memory:
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
	return;

 do_sigbus:
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void __user *) address;
	force_sig_info(SIGBUS, &info, current);
	if (!user_mode(regs))
		goto no_context;
	return;

 do_sigsegv:
	info.si_signo = SIGSEGV;
	info.si_errno = 0;
	info.si_code = si_code;
	info.si_addr = (void __user *) address;
	force_sig_info(SIGSEGV, &info, current);
	return;

#ifdef CONFIG_ALPHA_LARGE_VMALLOC
 vmalloc_fault:
	if (user_mode(regs))
		goto do_sigsegv;
	else {
		long index = pgd_index(address);
		pgd_t *pgd, *pgd_k;

		pgd = current->active_mm->pgd + index;
		pgd_k = swapper_pg_dir + index;
		if (!pgd_present(*pgd) && pgd_present(*pgd_k)) {
			pgd_val(*pgd) = pgd_val(*pgd_k);
			return;
		}
		goto no_context;
	}
#endif
}
