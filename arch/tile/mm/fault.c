/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * From i386 code copyright (C) 1995  Linus Torvalds
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
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>		
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/hugetlb.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#include <asm/pgalloc.h>
#include <asm/sections.h>
#include <asm/traps.h>
#include <asm/syscalls.h>

#include <arch/interrupts.h>

static noinline void force_sig_info_fault(const char *type, int si_signo,
					  int si_code, unsigned long address,
					  int fault_num,
					  struct task_struct *tsk,
					  struct pt_regs *regs)
{
	siginfo_t info;

	if (unlikely(tsk->pid < 2)) {
		panic("Signal %d (code %d) at %#lx sent to %s!",
		      si_signo, si_code & 0xffff, address,
		      is_idle_task(tsk) ? "the idle task" : "init");
	}

	info.si_signo = si_signo;
	info.si_errno = 0;
	info.si_code = si_code;
	info.si_addr = (void __user *)address;
	info.si_trapno = fault_num;
	trace_unhandled_signal(type, regs, address, si_signo);
	force_sig_info(si_signo, &info, tsk);
}

#ifndef __tilegx__
SYSCALL_DEFINE2(cmpxchg_badaddr, unsigned long, address,
		struct pt_regs *, regs)
{
	if (address >= PAGE_OFFSET)
		force_sig_info_fault("atomic segfault", SIGSEGV, SEGV_MAPERR,
				     address, INT_DTLB_MISS, current, regs);
	else
		force_sig_info_fault("atomic alignment fault", SIGBUS,
				     BUS_ADRALN, address,
				     INT_UNALIGN_DATA, current, regs);

	regs->pc -= 8;

	regs->flags |= PT_FLAGS_CALLER_SAVES;

	return 0;
}
#endif

static inline pmd_t *vmalloc_sync_one(pgd_t *pgd, unsigned long address)
{
	unsigned index = pgd_index(address);
	pgd_t *pgd_k;
	pud_t *pud, *pud_k;
	pmd_t *pmd, *pmd_k;

	pgd += index;
	pgd_k = init_mm.pgd + index;

	if (!pgd_present(*pgd_k))
		return NULL;

	pud = pud_offset(pgd, address);
	pud_k = pud_offset(pgd_k, address);
	if (!pud_present(*pud_k))
		return NULL;

	pmd = pmd_offset(pud, address);
	pmd_k = pmd_offset(pud_k, address);
	if (!pmd_present(*pmd_k))
		return NULL;
	if (!pmd_present(*pmd)) {
		set_pmd(pmd, *pmd_k);
		arch_flush_lazy_mmu_mode();
	} else
		BUG_ON(pmd_ptfn(*pmd) != pmd_ptfn(*pmd_k));
	return pmd_k;
}

static inline int vmalloc_fault(pgd_t *pgd, unsigned long address)
{
	pmd_t *pmd_k;
	pte_t *pte_k;

	
	if (!(address >= VMALLOC_START && address < VMALLOC_END))
		return -1;

	pmd_k = vmalloc_sync_one(pgd, address);
	if (!pmd_k)
		return -1;
	if (pmd_huge(*pmd_k))
		return 0;   
	pte_k = pte_offset_kernel(pmd_k, address);
	if (!pte_present(*pte_k))
		return -1;
	return 0;
}

static void wait_for_migration(pte_t *pte)
{
	if (pte_migrating(*pte)) {
		int retries = 0;
		int bound = get_clock_rate();
		while (pte_migrating(*pte)) {
			barrier();
			if (++retries > bound)
				panic("Hit migrating PTE (%#llx) and"
				      " page PFN %#lx still migrating",
				      pte->val, pte_pfn(*pte));
		}
	}
}

static pgd_t *get_current_pgd(void)
{
	HV_Context ctx = hv_inquire_context();
	unsigned long pgd_pfn = ctx.page_table >> PAGE_SHIFT;
	struct page *pgd_page = pfn_to_page(pgd_pfn);
	BUG_ON(PageHighMem(pgd_page));   
	return (pgd_t *) __va(ctx.page_table);
}

static int handle_migrating_pte(pgd_t *pgd, int fault_num,
				unsigned long address, unsigned long pc,
				int is_kernel_mode, int write)
{
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t pteval;

	if (pgd_addr_invalid(address))
		return 0;

	pgd += pgd_index(address);
	pud = pud_offset(pgd, address);
	if (!pud || !pud_present(*pud))
		return 0;
	pmd = pmd_offset(pud, address);
	if (!pmd || !pmd_present(*pmd))
		return 0;
	pte = pmd_huge_page(*pmd) ? ((pte_t *)pmd) :
		pte_offset_kernel(pmd, address);
	pteval = *pte;
	if (pte_migrating(pteval)) {
		if (in_nmi() && search_exception_tables(pc))
			return 0;
		wait_for_migration(pte);
		return 1;
	}

	if (!is_kernel_mode || !pte_present(pteval))
		return 0;
	if (fault_num == INT_ITLB_MISS) {
		if (pte_exec(pteval))
			return 1;
	} else if (write) {
		if (pte_write(pteval))
			return 1;
	} else {
		if (pte_read(pteval))
			return 1;
	}

	return 0;
}

static int handle_page_fault(struct pt_regs *regs,
			     int fault_num,
			     int is_page_fault,
			     unsigned long address,
			     int write)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long stack_offset;
	int fault;
	int si_code;
	int is_kernel_mode;
	pgd_t *pgd;

	
	if (!is_page_fault)
		write = 1;

	is_kernel_mode = (EX1_PL(regs->ex1) != USER_PL);

	tsk = validate_current();

	stack_offset = stack_pointer & (THREAD_SIZE-1);
	if (stack_offset < THREAD_SIZE / 8) {
		pr_alert("Potential stack overrun: sp %#lx\n",
		       stack_pointer);
		show_regs(regs);
		pr_alert("Killing current process %d/%s\n",
		       tsk->pid, tsk->comm);
		do_group_exit(SIGKILL);
	}

	pgd = get_current_pgd();
	if (handle_migrating_pte(pgd, fault_num, address, regs->pc,
				 is_kernel_mode, write))
		return 1;

	si_code = SEGV_MAPERR;

	if (unlikely(address >= TASK_SIZE &&
		     !is_arch_mappable_range(address, 0))) {
		if (is_kernel_mode && is_page_fault &&
		    vmalloc_fault(pgd, address) >= 0)
			return 1;
		mm = NULL;  
		vma = NULL;
		goto bad_area_nosemaphore;
	}

	if (!(regs->flags & PT_FLAGS_DISABLE_IRQ))
		local_irq_enable();

	mm = tsk->mm;

	if (in_atomic() || !mm) {
		vma = NULL;  
		goto bad_area_nosemaphore;
	}

	if (!down_read_trylock(&mm->mmap_sem)) {
		if (is_kernel_mode &&
		    !search_exception_tables(regs->pc)) {
			vma = NULL;  
			goto bad_area_nosemaphore;
		}
		down_read(&mm->mmap_sem);
	}

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (regs->sp < PAGE_OFFSET) {
		if (address < regs->sp)
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;

good_area:
	si_code = SEGV_ACCERR;
	if (fault_num == INT_ITLB_MISS) {
		if (!(vma->vm_flags & VM_EXEC))
			goto bad_area;
	} else if (write) {
#ifdef TEST_VERIFY_AREA
		if (!is_page_fault && regs->cs == KERNEL_CS)
			pr_err("WP fault at "REGFMT"\n", regs->eip);
#endif
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		if (!is_page_fault || !(vma->vm_flags & VM_READ))
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

#if CHIP_HAS_TILE_DMA() || CHIP_HAS_SN_PROC()
	switch (fault_num) {
#if CHIP_HAS_TILE_DMA()
	case INT_DMATLB_MISS:
	case INT_DMATLB_MISS_DWNCL:
	case INT_DMATLB_ACCESS:
	case INT_DMATLB_ACCESS_DWNCL:
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__REQUEST_MASK);
		break;
#endif
#if CHIP_HAS_SN_PROC()
	case INT_SNITLB_MISS:
	case INT_SNITLB_MISS_DWNCL:
		__insn_mtspr(SPR_SNCTL,
			     __insn_mfspr(SPR_SNCTL) &
			     ~SPR_SNCTL__FRZPROC_MASK);
		break;
#endif
	}
#endif

	up_read(&mm->mmap_sem);
	return 1;

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	
	if (!is_kernel_mode) {
		local_irq_enable();

		force_sig_info_fault("segfault", SIGSEGV, si_code, address,
				     fault_num, tsk, regs);
		return 0;
	}

no_context:
	
	if (fixup_exception(regs))
		return 0;


	bust_spinlocks(1);

	
#ifdef SUPPORT_LOOKUP_ADDRESS
	if (fault_num == INT_ITLB_MISS) {
		pte_t *pte = lookup_address(address);

		if (pte && pte_present(*pte) && !pte_exec_kernel(*pte))
			pr_crit("kernel tried to execute"
			       " non-executable page - exploit attempt?"
			       " (uid: %d)\n", current->uid);
	}
#endif
	if (address < PAGE_SIZE)
		pr_alert("Unable to handle kernel NULL pointer dereference\n");
	else
		pr_alert("Unable to handle kernel paging request\n");
	pr_alert(" at virtual address "REGFMT", pc "REGFMT"\n",
		 address, regs->pc);

	show_regs(regs);

	if (unlikely(tsk->pid < 2)) {
		panic("Kernel page fault running %s!",
		      is_idle_task(tsk) ? "the idle task" : "init");
	}

#ifdef SUPPORT_DIE
	die("Oops", regs);
#endif
	bust_spinlocks(1);

	do_group_exit(SIGKILL);

out_of_memory:
	up_read(&mm->mmap_sem);
	if (is_global_init(tsk)) {
		yield();
		down_read(&mm->mmap_sem);
		goto survive;
	}
	pr_alert("VM: killing process %s\n", tsk->comm);
	if (!is_kernel_mode)
		do_group_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	
	if (is_kernel_mode)
		goto no_context;

	force_sig_info_fault("bus error", SIGBUS, BUS_ADRERR, address,
			     fault_num, tsk, regs);
	return 0;
}

#ifndef __tilegx__

#define ics_panic(fmt, ...) do { \
	__insn_mtspr(SPR_INTERRUPT_CRITICAL_SECTION, 0); \
	panic(fmt, __VA_ARGS__); \
} while (0)

struct intvec_state do_page_fault_ics(struct pt_regs *regs, int fault_num,
				      unsigned long address,
				      unsigned long info)
{
	unsigned long pc = info & ~1;
	int write = info & 1;
	pgd_t *pgd = get_current_pgd();

	
	struct intvec_state state = {
		do_page_fault, fault_num, address, write, 1
	};

	
	if ((pc & 0x7) != 0 || pc < PAGE_OFFSET ||
	    (fault_num != INT_DTLB_MISS &&
	     fault_num != INT_DTLB_ACCESS)) {
		unsigned long old_pc = regs->pc;
		regs->pc = pc;
		ics_panic("Bad ICS page fault args:"
			  " old PC %#lx, fault %d/%d at %#lx\n",
			  old_pc, fault_num, write, address);
	}

	
	if (fault_num != INT_DTLB_ACCESS && vmalloc_fault(pgd, address) >= 0)
		return state;

	if (pc >= (unsigned long) sys_cmpxchg &&
	    pc < (unsigned long) __sys_cmpxchg_end) {
#ifdef CONFIG_SMP
		
		if (pc >= (unsigned long)__sys_cmpxchg_grab_lock) {
			int *lock_ptr = (int *)(regs->regs[ATOMIC_LOCK_REG]);
			__atomic_fault_unlock(lock_ptr);
		}
#endif
		regs->sp = regs->regs[27];
	}

	else if (pc >= (unsigned long) __start_atomic_asm_code &&
		   pc < (unsigned long) __end_atomic_asm_code) {
		const struct exception_table_entry *fixup;
#ifdef CONFIG_SMP
		
		int *lock_ptr = (int *)(regs->regs[ATOMIC_LOCK_REG]);
		__atomic_fault_unlock(lock_ptr);
#endif
		fixup = search_exception_tables(pc);
		if (!fixup)
			ics_panic("ICS atomic fault not in table:"
				  " PC %#lx, fault %d", pc, fault_num);
		regs->pc = fixup->fixup;
		regs->ex1 = PL_ICS_EX1(KERNEL_PL, 0);
	}

	if (fault_num == INT_DTLB_ACCESS)
		write = 1;
	if (handle_migrating_pte(pgd, fault_num, address, pc, 1, write))
		return state;

	
	state.retval = 0;
	return state;
}

#endif 

void do_page_fault(struct pt_regs *regs, int fault_num,
		   unsigned long address, unsigned long write)
{
	int is_page_fault;

	
	BUG_ON(write & ~1);

#if CHIP_HAS_TILE_DMA()
	if (fault_num == INT_DMATLB_MISS ||
	    fault_num == INT_DMATLB_ACCESS ||
	    fault_num == INT_DMATLB_MISS_DWNCL ||
	    fault_num == INT_DMATLB_ACCESS_DWNCL) {
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__SUSPEND_MASK);
		while (__insn_mfspr(SPR_DMA_USER_STATUS) &
		       SPR_DMA_STATUS__BUSY_MASK)
			;
	}
#endif

	
	switch (fault_num) {
	case INT_ITLB_MISS:
	case INT_DTLB_MISS:
#if CHIP_HAS_TILE_DMA()
	case INT_DMATLB_MISS:
	case INT_DMATLB_MISS_DWNCL:
#endif
#if CHIP_HAS_SN_PROC()
	case INT_SNITLB_MISS:
	case INT_SNITLB_MISS_DWNCL:
#endif
		is_page_fault = 1;
		break;

	case INT_DTLB_ACCESS:
#if CHIP_HAS_TILE_DMA()
	case INT_DMATLB_ACCESS:
	case INT_DMATLB_ACCESS_DWNCL:
#endif
		is_page_fault = 0;
		break;

	default:
		panic("Bad fault number %d in do_page_fault", fault_num);
	}

#if CHIP_HAS_TILE_DMA() || CHIP_HAS_SN_PROC()
	if (EX1_PL(regs->ex1) != USER_PL) {
		struct async_tlb *async;
		switch (fault_num) {
#if CHIP_HAS_TILE_DMA()
		case INT_DMATLB_MISS:
		case INT_DMATLB_ACCESS:
		case INT_DMATLB_MISS_DWNCL:
		case INT_DMATLB_ACCESS_DWNCL:
			async = &current->thread.dma_async_tlb;
			break;
#endif
#if CHIP_HAS_SN_PROC()
		case INT_SNITLB_MISS:
		case INT_SNITLB_MISS_DWNCL:
			async = &current->thread.sn_async_tlb;
			break;
#endif
		default:
			async = NULL;
		}
		if (async) {

			local_irq_enable();

			set_thread_flag(TIF_ASYNC_TLB);
			if (async->fault_num != 0) {
				panic("Second async fault %d;"
				      " old fault was %d (%#lx/%ld)",
				      fault_num, async->fault_num,
				      address, write);
			}
			BUG_ON(fault_num == 0);
			async->fault_num = fault_num;
			async->is_fault = is_page_fault;
			async->is_write = write;
			async->address = address;
			return;
		}
	}
#endif

	handle_page_fault(regs, fault_num, is_page_fault, address, write);
}


#if CHIP_HAS_TILE_DMA() || CHIP_HAS_SN_PROC()
static void handle_async_page_fault(struct pt_regs *regs,
				    struct async_tlb *async)
{
	if (async->fault_num) {
		int fault_num = async->fault_num;
		async->fault_num = 0;
		handle_page_fault(regs, fault_num, async->is_fault,
				  async->address, async->is_write);
	}
}

void do_async_page_fault(struct pt_regs *regs)
{
	clear_thread_flag(TIF_ASYNC_TLB);

#if CHIP_HAS_TILE_DMA()
	handle_async_page_fault(regs, &current->thread.dma_async_tlb);
#endif
#if CHIP_HAS_SN_PROC()
	handle_async_page_fault(regs, &current->thread.sn_async_tlb);
#endif
}
#endif 


void vmalloc_sync_all(void)
{
#ifdef __tilegx__
	
	BUG_ON(pgd_index(VMALLOC_END) != pgd_index(VMALLOC_START));
#else
	static DECLARE_BITMAP(insync, PTRS_PER_PGD);
	static unsigned long start = PAGE_OFFSET;
	unsigned long address;

	BUILD_BUG_ON(PAGE_OFFSET & ~PGDIR_MASK);
	for (address = start; address >= PAGE_OFFSET; address += PGDIR_SIZE) {
		if (!test_bit(pgd_index(address), insync)) {
			unsigned long flags;
			struct list_head *pos;

			spin_lock_irqsave(&pgd_lock, flags);
			list_for_each(pos, &pgd_list)
				if (!vmalloc_sync_one(list_to_pgd(pos),
								address)) {
					
					BUG_ON(pos != pgd_list.next);
					break;
				}
			spin_unlock_irqrestore(&pgd_lock, flags);
			if (pos != pgd_list.next)
				set_bit(pgd_index(address), insync);
		}
		if (address == start && test_bit(pgd_index(address), insync))
			start = address + PGDIR_SIZE;
	}
#endif
}
