/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by Ralf Baechle
 * Copyright 1999 SuSE GmbH (Philipp Rumpf, prumpf@tux.org)
 * Copyright 1999 Hewlett Packard Co.
 *
 */

#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/traps.h>

#define PRINT_USER_FAULTS 
			 


#define bit22set(x)		(x & 0x00000200)
#define bits23_25set(x)		(x & 0x000001c0)
#define isGraphicsFlushRead(x)	((x & 0xfc003fdf) == 0x04001a80)
				

#define BITSSET		0x1c0	


DEFINE_PER_CPU(struct exception_data, exception_data);

static unsigned long
parisc_acctyp(unsigned long code, unsigned int inst)
{
	if (code == 6 || code == 16)
	    return VM_EXEC;

	switch (inst & 0xf0000000) {
	case 0x40000000: 
	case 0x50000000: 
		return VM_READ;

	case 0x60000000: 
	case 0x70000000: 
		return VM_WRITE;

	case 0x20000000: 
	case 0x30000000: 
		if (bit22set(inst))
			return VM_WRITE;

	case 0x0: 
		if (bit22set(inst)) {
			if (isGraphicsFlushRead(inst))
				return VM_READ;
			return VM_WRITE;
		} else {
			if (bits23_25set(inst) == BITSSET)
				return VM_WRITE;
		}
		return VM_READ; 
	}
	return VM_READ; 
}

#undef bit22set
#undef bits23_25set
#undef isGraphicsFlushRead
#undef BITSSET


#if 0
			while (tree != vm_avl_empty) {
				if (tree->vm_start > addr) {
					tree = tree->vm_avl_left;
				} else {
					prev = tree;
					if (prev->vm_next == NULL)
						break;
					if (prev->vm_next->vm_start > addr)
						break;
					tree = tree->vm_avl_right;
				}
			}
#endif

int fixup_exception(struct pt_regs *regs)
{
	const struct exception_table_entry *fix;

	fix = search_exception_tables(regs->iaoq[0]);
	if (fix) {
		struct exception_data *d;
		d = &__get_cpu_var(exception_data);
		d->fault_ip = regs->iaoq[0];
		d->fault_space = regs->isr;
		d->fault_addr = regs->ior;

		regs->iaoq[0] = ((fix->fixup) & ~3);
		regs->iaoq[1] = regs->iaoq[0] + 4;
		regs->gr[0] &= ~PSW_B; 

		return 1;
	}

	return 0;
}

void do_page_fault(struct pt_regs *regs, unsigned long code,
			      unsigned long address)
{
	struct vm_area_struct *vma, *prev_vma;
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;
	unsigned long acc_type;
	int fault;

	if (in_atomic() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);
	vma = find_vma_prev(mm, address, &prev_vma);
	if (!vma || address < vma->vm_start)
		goto check_expansion;

good_area:

	acc_type = parisc_acctyp(code,regs->iir);

	if ((vma->vm_flags & acc_type) != acc_type)
		goto bad_area;


	fault = handle_mm_fault(mm, vma, address, (acc_type & VM_WRITE) ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto bad_area;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		current->maj_flt++;
	else
		current->min_flt++;
	up_read(&mm->mmap_sem);
	return;

check_expansion:
	vma = prev_vma;
	if (vma && (expand_stack(vma, address) == 0))
		goto good_area;

bad_area:
	up_read(&mm->mmap_sem);

	if (user_mode(regs)) {
		struct siginfo si;

#ifdef PRINT_USER_FAULTS
		printk(KERN_DEBUG "\n");
		printk(KERN_DEBUG "do_page_fault() pid=%d command='%s' type=%lu address=0x%08lx\n",
		    task_pid_nr(tsk), tsk->comm, code, address);
		if (vma) {
			printk(KERN_DEBUG "vm_start = 0x%08lx, vm_end = 0x%08lx\n",
					vma->vm_start, vma->vm_end);
		}
		show_regs(regs);
#endif
		
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		si.si_code = SEGV_MAPERR;
		si.si_addr = (void __user *) address;
		force_sig_info(SIGSEGV, &si, current);
		return;
	}

no_context:

	if (!user_mode(regs) && fixup_exception(regs)) {
		return;
	}

	parisc_terminate("Bad Address (null pointer deref?)", regs, code, address);

  out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(regs))
		goto no_context;
	pagefault_out_of_memory();
}
