/*
 *  linux/arch/frv/mm/fault.c
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * - Written by David Howells (dhowells@redhat.com)
 * - Derived from arch/m68knommu/mm/fault.c
 *   - Copyright (C) 1998  D. Jeff Dionne <jeff@lineo.ca>,
 *   - Copyright (C) 2000  Lineo, Inc.  (www.lineo.com)
 *
 *  Based on:
 *
 *  linux/arch/m68k/mm/fault.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 */

#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/hardirq.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/gdb-stub.h>

asmlinkage void do_page_fault(int datammu, unsigned long esr0, unsigned long ear0)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	unsigned long _pme, lrai, lrad, fixup;
	siginfo_t info;
	pgd_t *pge;
	pud_t *pue;
	pte_t *pte;
	int write;
	int fault;

#if 0
	const char *atxc[16] = {
		[0x0] = "mmu-miss", [0x8] = "multi-dat", [0x9] = "multi-sat",
		[0xa] = "tlb-miss", [0xc] = "privilege", [0xd] = "write-prot",
	};

	printk("do_page_fault(%d,%lx [%s],%lx)\n",
	       datammu, esr0, atxc[esr0 >> 20 & 0xf], ear0);
#endif

	mm = current->mm;

	if (!user_mode(__frame) && (esr0 & ESR0_ATXC) == ESR0_ATXC_AMRTLB_MISS) {
		if (ear0 >= VMALLOC_START && ear0 < VMALLOC_END)
			goto kernel_pte_fault;
		if (ear0 >= PKMAP_BASE && ear0 < PKMAP_END)
			goto kernel_pte_fault;
	}

	info.si_code = SEGV_MAPERR;

	if (in_atomic() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, ear0);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= ear0)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (user_mode(__frame)) {
		if ((ear0 & PAGE_MASK) + 2 * PAGE_SIZE < __frame->sp) {
#if 0
			printk("[%d] ### Access below stack @%lx (sp=%lx)\n",
			       current->pid, ear0, __frame->sp);
			show_registers(__frame);
			printk("[%d] ### Code: [%08lx] %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       current->pid,
			       __frame->pc,
			       ((u8*)__frame->pc)[0],
			       ((u8*)__frame->pc)[1],
			       ((u8*)__frame->pc)[2],
			       ((u8*)__frame->pc)[3],
			       ((u8*)__frame->pc)[4],
			       ((u8*)__frame->pc)[5],
			       ((u8*)__frame->pc)[6],
			       ((u8*)__frame->pc)[7]
			       );
#endif
			goto bad_area;
		}
	}

	if (expand_stack(vma, ear0))
		goto bad_area;

 good_area:
	info.si_code = SEGV_ACCERR;
	write = 0;
	switch (esr0 & ESR0_ATXC) {
	default:
		
	case ESR0_ATXC_WP_EXCEP:
#ifdef TEST_VERIFY_AREA
		if (!(user_mode(__frame)))
			printk("WP fault at %08lx\n", __frame->pc);
#endif
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
		write = 1;
		break;

		 
	case ESR0_ATXC_PRIV_EXCEP:
		goto bad_area;

	case ESR0_ATXC_AMRTLB_MISS:
		if (!(vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC)))
			goto bad_area;
		break;
	}

	fault = handle_mm_fault(mm, vma, ear0, write ? FAULT_FLAG_WRITE : 0);
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

	
	if (user_mode(__frame)) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void *) ear0;
		force_sig_info(SIGSEGV, &info, current);
		return;
	}

 no_context:
	
	if ((fixup = search_exception_table(__frame->pc)) != 0) {
		__frame->pc = fixup;
		return;
	}


	bust_spinlocks(1);

	if (ear0 < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(" at virtual addr %08lx\n", ear0);
	printk("  PC  : %08lx\n", __frame->pc);
	printk("  EXC : esr0=%08lx ear0=%08lx\n", esr0, ear0);

	asm("lrai %1,%0,#1,#0,#0" : "=&r"(lrai) : "r"(ear0));
	asm("lrad %1,%0,#1,#0,#0" : "=&r"(lrad) : "r"(ear0));

	printk(KERN_ALERT "  LRAI: %08lx\n", lrai);
	printk(KERN_ALERT "  LRAD: %08lx\n", lrad);

	__break_hijack_kernel_event();

	pge = pgd_offset(current->mm, ear0);
	pue = pud_offset(pge, ear0);
	_pme = pue->pue[0].ste[0];

	printk(KERN_ALERT "  PGE : %8p { PME %08lx }\n", pge, _pme);

	if (_pme & xAMPRx_V) {
		unsigned long dampr, damlr, val;

		asm volatile("movsg dampr2,%0 ! movgs %2,dampr2 ! movsg damlr2,%1"
			     : "=&r"(dampr), "=r"(damlr)
			     : "r" (_pme | xAMPRx_L|xAMPRx_SS_16Kb|xAMPRx_S|xAMPRx_C|xAMPRx_V)
			     );

		pte = (pte_t *) damlr + __pte_index(ear0);
		val = pte_val(*pte);

		asm volatile("movgs %0,dampr2" :: "r" (dampr));

		printk(KERN_ALERT "  PTE : %8p { %08lx }\n", pte, val);
	}

	die_if_kernel("Oops\n");
	do_exit(SIGKILL);

 out_of_memory:
	up_read(&mm->mmap_sem);
	if (!user_mode(__frame))
		goto no_context;
	pagefault_out_of_memory();
	return;

 do_sigbus:
	up_read(&mm->mmap_sem);

	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *) ear0;
	force_sig_info(SIGBUS, &info, current);

	
	if (!user_mode(__frame))
		goto no_context;
	return;

 kernel_pte_fault:
	{
		int index = pgd_index(ear0);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd = (pgd_t *) __get_TTBR();
		pgd = (pgd_t *)__va(pgd) + index;
		pgd_k = ((pgd_t *)(init_mm.pgd)) + index;

		if (!pgd_present(*pgd_k))
			goto no_context;
		

		pud_k = pud_offset(pgd_k, ear0);
		if (!pud_present(*pud_k))
			goto no_context;

		pmd_k = pmd_offset(pud_k, ear0);
		if (!pmd_present(*pmd_k))
			goto no_context;

		pud = pud_offset(pgd, ear0);
		pmd = pmd_offset(pud, ear0);
		set_pmd(pmd, *pmd_k);

		pte_k = pte_offset_kernel(pmd_k, ear0);
		if (!pte_present(*pte_k))
			goto no_context;
		return;
	}
} 
