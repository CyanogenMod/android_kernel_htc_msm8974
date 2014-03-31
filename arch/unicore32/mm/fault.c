/*
 * linux/arch/unicore32/mm/fault.c
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>
#include <linux/page-flags.h>
#include <linux/sched.h>
#include <linux/io.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#define FSR_LNX_PF		(1 << 31)

static inline int fsr_fs(unsigned int fsr)
{
	
	return (fsr & 31) + ((fsr & (3 << 5)) >> 5);
}

void show_pte(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;

	if (!mm)
		mm = &init_mm;

	printk(KERN_ALERT "pgd = %p\n", mm->pgd);
	pgd = pgd_offset(mm, addr);
	printk(KERN_ALERT "[%08lx] *pgd=%08lx", addr, pgd_val(*pgd));

	do {
		pmd_t *pmd;
		pte_t *pte;

		if (pgd_none(*pgd))
			break;

		if (pgd_bad(*pgd)) {
			printk("(bad)");
			break;
		}

		pmd = pmd_offset((pud_t *) pgd, addr);
		if (PTRS_PER_PMD != 1)
			printk(", *pmd=%08lx", pmd_val(*pmd));

		if (pmd_none(*pmd))
			break;

		if (pmd_bad(*pmd)) {
			printk("(bad)");
			break;
		}

		
		if (PageHighMem(pfn_to_page(pmd_val(*pmd) >> PAGE_SHIFT)))
			break;

		pte = pte_offset_map(pmd, addr);
		printk(", *pte=%08lx", pte_val(*pte));
		pte_unmap(pte);
	} while (0);

	printk("\n");
}

static void __do_kernel_fault(struct mm_struct *mm, unsigned long addr,
		unsigned int fsr, struct pt_regs *regs)
{
	if (fixup_exception(regs))
		return;

	bust_spinlocks(1);
	printk(KERN_ALERT
	       "Unable to handle kernel %s at virtual address %08lx\n",
	       (addr < PAGE_SIZE) ? "NULL pointer dereference" :
	       "paging request", addr);

	show_pte(mm, addr);
	die("Oops", regs, fsr);
	bust_spinlocks(0);
	do_exit(SIGKILL);
}

static void __do_user_fault(struct task_struct *tsk, unsigned long addr,
		unsigned int fsr, unsigned int sig, int code,
		struct pt_regs *regs)
{
	struct siginfo si;

	tsk->thread.address = addr;
	tsk->thread.error_code = fsr;
	tsk->thread.trap_no = 14;
	si.si_signo = sig;
	si.si_errno = 0;
	si.si_code = code;
	si.si_addr = (void __user *)addr;
	force_sig_info(sig, &si, tsk);
}

void do_bad_area(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->active_mm;

	if (user_mode(regs))
		__do_user_fault(tsk, addr, fsr, SIGSEGV, SEGV_MAPERR, regs);
	else
		__do_kernel_fault(mm, addr, fsr, regs);
}

#define VM_FAULT_BADMAP		0x010000
#define VM_FAULT_BADACCESS	0x020000

static inline bool access_error(unsigned int fsr, struct vm_area_struct *vma)
{
	unsigned int mask = VM_READ | VM_WRITE | VM_EXEC;

	if (!(fsr ^ 0x12))	
		mask = VM_WRITE;
	if (fsr & FSR_LNX_PF)
		mask = VM_EXEC;

	return vma->vm_flags & mask ? false : true;
}

static int __do_pf(struct mm_struct *mm, unsigned long addr, unsigned int fsr,
		struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	int fault;

	vma = find_vma(mm, addr);
	fault = VM_FAULT_BADMAP;
	if (unlikely(!vma))
		goto out;
	if (unlikely(vma->vm_start > addr))
		goto check_stack;

good_area:
	if (access_error(fsr, vma)) {
		fault = VM_FAULT_BADACCESS;
		goto out;
	}

	fault = handle_mm_fault(mm, vma, addr & PAGE_MASK,
			    (!(fsr ^ 0x12)) ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR))
		return fault;
	if (fault & VM_FAULT_MAJOR)
		tsk->maj_flt++;
	else
		tsk->min_flt++;
	return fault;

check_stack:
	if (vma->vm_flags & VM_GROWSDOWN && !expand_stack(vma, addr))
		goto good_area;
out:
	return fault;
}

static int do_pf(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	int fault, sig, code;

	tsk = current;
	mm = tsk->mm;

	if (in_atomic() || !mm)
		goto no_context;

	if (!down_read_trylock(&mm->mmap_sem)) {
		if (!user_mode(regs)
		    && !search_exception_tables(regs->UCreg_pc))
			goto no_context;
		down_read(&mm->mmap_sem);
	} else {
		might_sleep();
#ifdef CONFIG_DEBUG_VM
		if (!user_mode(regs) &&
		    !search_exception_tables(regs->UCreg_pc))
			goto no_context;
#endif
	}

	fault = __do_pf(mm, addr, fsr, tsk);
	up_read(&mm->mmap_sem);

	if (likely(!(fault &
	       (VM_FAULT_ERROR | VM_FAULT_BADMAP | VM_FAULT_BADACCESS))))
		return 0;

	if (fault & VM_FAULT_OOM) {
		pagefault_out_of_memory();
		return 0;
	}

	if (!user_mode(regs))
		goto no_context;

	if (fault & VM_FAULT_SIGBUS) {
		sig = SIGBUS;
		code = BUS_ADRERR;
	} else {
		sig = SIGSEGV;
		code = fault == VM_FAULT_BADACCESS ? SEGV_ACCERR : SEGV_MAPERR;
	}

	__do_user_fault(tsk, addr, fsr, sig, code, regs);
	return 0;

no_context:
	__do_kernel_fault(mm, addr, fsr, regs);
	return 0;
}

static int do_ifault(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	unsigned int index;
	pgd_t *pgd, *pgd_k;
	pmd_t *pmd, *pmd_k;

	if (addr < TASK_SIZE)
		return do_pf(addr, fsr, regs);

	if (user_mode(regs))
		goto bad_area;

	index = pgd_index(addr);

	pgd = cpu_get_pgd() + index;
	pgd_k = init_mm.pgd + index;

	if (pgd_none(*pgd_k))
		goto bad_area;

	pmd_k = pmd_offset((pud_t *) pgd_k, addr);
	pmd = pmd_offset((pud_t *) pgd, addr);

	if (pmd_none(*pmd_k))
		goto bad_area;

	set_pmd(pmd, *pmd_k);
	flush_pmd_entry(pmd);
	return 0;

bad_area:
	do_bad_area(addr, fsr, regs);
	return 0;
}

static int do_bad(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	return 1;
}

static int do_good(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	unsigned int res1, res2;

	printk("dabt exception but no error!\n");

	__asm__ __volatile__(
			"mff %0,f0\n"
			"mff %1,f1\n"
			: "=r"(res1), "=r"(res2)
			:
			: "memory");

	printk(KERN_EMERG "r0 :%08x  r1 :%08x\n", res1, res2);
	panic("shut up\n");
	return 0;
}

static struct fsr_info {
	int (*fn) (unsigned long addr, unsigned int fsr, struct pt_regs *regs);
	int sig;
	int code;
	const char *name;
} fsr_info[] = {
	{ do_good,	SIGBUS,  0,		"no error"		},
	{ do_bad,	SIGBUS,  BUS_ADRALN,	"alignment exception"	},
	{ do_bad,	SIGBUS,  BUS_OBJERR,	"external exception"	},
	{ do_bad,	SIGBUS,  0,		"burst operation"	},
	{ do_bad,	SIGBUS,  0,		"unknown 00100"		},
	{ do_ifault,	SIGSEGV, SEGV_MAPERR,	"2nd level pt non-exist"},
	{ do_bad,	SIGBUS,  0,		"2nd lvl large pt non-exist" },
	{ do_bad,	SIGBUS,  0,		"invalid pte"		},
	{ do_pf,	SIGSEGV, SEGV_MAPERR,	"page miss"		},
	{ do_bad,	SIGBUS,  0,		"middle page miss"	},
	{ do_bad,	SIGBUS,	 0,		"large page miss"	},
	{ do_pf,	SIGSEGV, SEGV_MAPERR,	"super page (section) miss" },
	{ do_bad,	SIGBUS,  0,		"unknown 01100"		},
	{ do_bad,	SIGBUS,  0,		"unknown 01101"		},
	{ do_bad,	SIGBUS,  0,		"unknown 01110"		},
	{ do_bad,	SIGBUS,  0,		"unknown 01111"		},
	{ do_bad,	SIGBUS,  0,		"addr: up 3G or IO"	},
	{ do_pf,	SIGSEGV, SEGV_ACCERR,	"read unreadable addr"	},
	{ do_pf,	SIGSEGV, SEGV_ACCERR,	"write unwriteable addr"},
	{ do_pf,	SIGSEGV, SEGV_ACCERR,	"exec unexecutable addr"},
	{ do_bad,	SIGBUS,  0,		"unknown 10100"		},
	{ do_bad,	SIGBUS,  0,		"unknown 10101"		},
	{ do_bad,	SIGBUS,  0,		"unknown 10110"		},
	{ do_bad,	SIGBUS,  0,		"unknown 10111"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11000"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11001"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11010"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11011"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11100"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11101"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11110"		},
	{ do_bad,	SIGBUS,  0,		"unknown 11111"		}
};

void __init hook_fault_code(int nr,
		int (*fn) (unsigned long, unsigned int, struct pt_regs *),
		int sig, int code, const char *name)
{
	if (nr < 0 || nr >= ARRAY_SIZE(fsr_info))
		BUG();

	fsr_info[nr].fn   = fn;
	fsr_info[nr].sig  = sig;
	fsr_info[nr].code = code;
	fsr_info[nr].name = name;
}

asmlinkage void do_DataAbort(unsigned long addr, unsigned int fsr,
			struct pt_regs *regs)
{
	const struct fsr_info *inf = fsr_info + fsr_fs(fsr);
	struct siginfo info;

	if (!inf->fn(addr, fsr & ~FSR_LNX_PF, regs))
		return;

	printk(KERN_ALERT "Unhandled fault: %s (0x%03x) at 0x%08lx\n",
	       inf->name, fsr, addr);

	info.si_signo = inf->sig;
	info.si_errno = 0;
	info.si_code = inf->code;
	info.si_addr = (void __user *)addr;
	uc32_notify_die("", regs, &info, fsr, 0);
}

asmlinkage void do_PrefetchAbort(unsigned long addr,
			unsigned int ifsr, struct pt_regs *regs)
{
	const struct fsr_info *inf = fsr_info + fsr_fs(ifsr);
	struct siginfo info;

	if (!inf->fn(addr, ifsr | FSR_LNX_PF, regs))
		return;

	printk(KERN_ALERT "Unhandled prefetch abort: %s (0x%03x) at 0x%08lx\n",
	       inf->name, ifsr, addr);

	info.si_signo = inf->sig;
	info.si_errno = 0;
	info.si_code = inf->code;
	info.si_addr = (void __user *)addr;
	uc32_notify_die("", regs, &info, ifsr, 0);
}
